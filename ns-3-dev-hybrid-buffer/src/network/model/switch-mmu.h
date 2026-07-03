/*
 * Copyright (c) 2022 Xi'an Jiaotong University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Shunlei Yang <yxlzqmysl0405@stu.xjtu.edu.cn>
 */

#ifndef SWITCH_MMU_H
#define SWITCH_MMU_H

#include "off-chip-buffer.h"

#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/traced-value.h"

#include <cmath>
#include <iomanip>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace ns3
{
class Packet;
class OffChipBuffer;

/**
 *
 * \ingroup network
 *
 * \brief A switch memory management model.
 *
 * Simple schematic of typical hybrid buffer switch mmu.
 *
 *          -----------------------------------------------------
 *          |                                  ______    ______  |
 *          |                               __|Wcache|__|      | |
 *          |   MMU    ------------------- /  |______|  |  D   | |
 *          |         |                  |/             |  R   | |
 *          |   ----> |Buffer Management |              |  A   | |
 *          |  packet |                  |\    ______   |  M   | |
 *          |          ------------------- \__| SRAM |  |(HBM) | |
 *          |                                 |______|  |______| |
 *          -----------------------------------------------------
 *
 * After packet get into MMU model of switch, there are a few
 * should be done before put it into Switch Buffer (SRAM or DRAM).
 *
 * First, we need to get all the traffic characteristics we need.
 *
 * Second, the Buffer Management will use all traffic characteris
 * -tics we get before, to determine whether the packet can be
 * accepted and where the packet should accepted(SRAM or DRAM).
 *
 * Third, load the packet into our Buffer. If the destination is
 * the DRAM, packet will be loaded into wcache first and then into
 * DRAM.
 *
 * This class holds together:
 *  - a memory model, which may have different type of memory
 *      together, like DRAM (HBM), SRAM, Cache.
 *  - a list of different set of Buffer Management algorithm,
 *    like DT, ABM, FB... to check if the Packet is legal to
 *    Ingress/Egress.
 *  - a list of counters which stand for the status of Memory.
 */
class SwitchMmu : public Object
{
  public:
    /**
     * This Constant will be used in every packet input place:
     * (BM input, Switch Input and OffChipBuffer input)
     *
     * This is a bug that was discovered very early but no elegant
     * solution was found:
     * When the packet is stored in the buffer(whether onchip or offchip),
     * The packet processing flow of ns3 will will add the corresponding
     * header to it(IP and MAC Header), which will cause the packet length
     * of the input packet to be inconsistent with the packet length of the
     * output packet. (The output packet is 22 bytes larger than the input
     * packet) It will caused unexpected counter value error and leading to
     * the error of Buffer Management Algorithm error.
     *
     * Follow-up we are looking for an elegant way to solve this problem. And
     * in order to temporarily avoid this problem and carry out testing, we
     * just added this packet length offset in hardcoded form to ensure correct
     * operation.
     */
    static const uint32_t IPV4_INPUT_PKTSIZE_CORRECTION = 22;

    /// \brief Structure that keeps the MMU statistics
    struct Stats
    {
        uint64_t nTotalStoredPackets; //!< The Total Packets Num stored into SwitchMmu (including
                                      //!< those stored to off-chip buffer and those have left)
        uint64_t nTotalBmDropPackets; //!< The number of dropped packets due to buffer management
        uint64_t nTotalBmDropPacketsSize;
        uint64_t nTotalOnChipBufferStoredPackets; //!< The total number of packets stored into
                                                  //!< OnChip Buffer (including those have left)
        uint64_t perusStoredPackets;              //!< The number of packets stored in per us
        /// constructor
        Stats();
    };

    /**
     * \brief Switch Buffer Model Type.
     *
     * In order to adapt to different switch buffer architecture in a
     * set of code frameworks. We just develop a configurable Buffer
     * type Frame. It is the type of BufferModel.
     */
    typedef enum
    {
        ONCHIP,    //< Switch with only onchip buffer
        ONOFFCHIP, //< Switch with both onchip buffer and offchip buffer
                   //< (i.e., SRAM, DRAM and Write-in Cache)
    } BufferModel;

    /**
     * \brief Buffer Management algorithm.
     *
     * In order to adapt to different cache management algorithms in a
     * set of code frameworks. We just develop a configurable BM Frame.
     * It is the type
     */
    typedef enum
    {
        HW,  //!< HuaWei Buffer Management Algorithm
        YSL, //!< My own Create Management Algorithm: Shunlei Yang
        TDT, // Three DT.
        BASELINE,
        YRF, // My own Create Management Algorithm: Renfu Yao
        DEEPHIR
    } BmAlgorithm;

    /**
     * \brief Buffer Management Algorithm Result.
     *
     * For the Buffer Algorithm result in Traditional Switch Arch(SRAM only),
     * there are only two possibilities: Drop or Accept. But As for the Buffer
     * Algorithm Result in Hybrid Buffer Switch Arch, it should be more complex:
     * In addition to considering whether to accept, we also need to consider
     * the specific location of the accept(OnChipBuffer or OffChipBuffer).
     */
    typedef enum
    {
        TO_OFFCHIPBUFFER, //!< The Packet should be accepted to Off-Chip-Buffer.
        TO_ONCHIPBUFFER,  //!< The Packet should be accepted to On-Chip-Buffer.
        DROP              //!< The Packet should be Dropped.
    } BmResult;

    /**
     * \brief Queue Congestion State
     *
     * Considering that the Buffer Management admission and Qdisc scheduler order
     * scheduling are both based on the queue level as the basic unit, If the congestion
     * distinction is based on the port, it will be too coarse-grained to specifically affect
     * the existing cache management and scheduler framework. The control unit is too fine-grained
     * to achieve precise control, if the basic unit is the flow level.
     *
     * Therefore, the queue level is both suitable and necessary as the basic unit of congestion
     * differentiation.
     */
    typedef enum
    {
        NOT_CONGESTION, //!< The Queue is not in Congestion State
        CONGESTION,     //!< The Queue is in Congestion State
        BURST           //!< The Queue is in BURST STATE
    } QueueStatus;

    /**
     * \brief Types of port rate
     *
     * We should distinguish different port rates, because ports with different rates
     * have different parameter settings.
     */
    typedef enum
    {
        Gbps100 = 0, //!< 100Gbps port
        Gbps400 = 1, //!< 400Gbps port
        Gbps800 = 2  //!< 800Gbps port
    } PortType;

    /**
     * \brief Get the Wcache Qlen.
     * \return the Wcache Qlen.
     */
    void ReadWcacheComplete(Ptr<const Packet> packet);

    /**
     * \brief Get the Wcache Qlen.
     * \return the Wcache Qlen.
     */
    void WriteWcacheComplete(Ptr<const Packet> packet);

    /**
     * \brief Get the Dram Qlen.
     * \return the Dram Qlen.
     */
    void ReadDramComplete(Ptr<const Packet> packet);

    /**
     * \brief Get the Dram Qlen.
     * \return the Dram Qlen.
     */
    void WriteDramComplete(Ptr<const Packet> packet);

    /**
     * \brief Get the TypeId
     *
     * Set 'static' to avoid other model to get it easily.
     *
     * \return The TypeId for this class.
     */
    static TypeId GetTypeId();

    /**
     * \brief Get the type ID for the instance
     * \return the instance TypeId
     */
    TypeId GetInstanceTypeId() const
        override; // 该函数被声明为const override，表明它是一个虚函数，并且在派生类中可以进行重写

    /**
     * \brief Construct a SwitchMmu.
     *
     * This is the constructor for the SwitchMmu. It takes
     * as a SharedMemory Management model to the SwitchNode,
     * help to manage all the Buffer, whether it is onChip
     * or offChip.
     */
    SwitchMmu();

    /**
     * \brief Destory a SwitchMmu.
     *
     * This is the destructor for the SwitchMmu.
     */
    ~SwitchMmu() override;

    /**
     * \brief Retrieve all the collected statistics.
     * \return the collected statistics.
     */
    const Stats& GetStats();

    /**
     * \brief Set the node the Swith MMU is associated with
     * \param node the node
     */

    void SetNode(Ptr<Node> node);

    /**
     * \brief Set the switch mem type.
     *
     * \param type The setting Switch Mem Type.
     */
    void SetMemType(BufferModel type);

    /**
     * \brief Get the Switch Mem type.
     *
     * \return Return the setting Switch Type.
     */
    BufferModel GetMemType() const;

    /**
     * \brief Set the number of switch port
     *
     * \param nPorts The number of switch ports
     */
    void SetNPorts(uint32_t nPorts);

    /**
     * \brief Get the setting port num.
     *
     * \return Return the port num.
     */
    uint32_t GetNPorts() const;

    /**
     * \brief Set the number of queues per port
     *
     * \param nQueues the number of queues
     */
    void SetNQueues(uint32_t nQueues);

    /**
     * \brief Get the setting Queue num per port.
     *
     * \return Return the setting Queue Num per port.
     */
    uint32_t GetNQueues() const;

    /**
     * \brief Set the number of priorities
     *
     * \param nPriorities the number of priorities
     */
    void SetNPriorities(uint32_t nPriorities);

    /**
     * \brief Get the Switch Priority Num.
     *
     * \return Return the Priority Number of Switch Flow Id.
     */
    uint32_t GetNPriorities() const;

    /**
     * \brief Set the OnChipBuffer Management Algorithm.
     *
     * \param type the SwitchMMU OnChipBuffer Buffer Management Algorithm.
     */
    void SetBmAlgorithm(BmAlgorithm type);

    /**
     * \brief Get the OnChipBuffer Management Algorithm.
     *
     * \return Return the setting SwitchMMU OnChipBuffer Buffer Management
     * Algorithm.
     */
    BmAlgorithm GetBmAlgorithm() const;

    /**
     * \brief Set the onchip buffer size.
     *
     */
    void SetOnChipBufferSize(uint64_t size);

    /**
     * \brief Get the onchip buffer size.
     *
     * \return Return the onchip buffer size.
     */
    uint64_t GetOnChipBufferSize() const;

    /**
     * \brief Set the reorder buffer size.
     *
     */
    void SetReorderBufferSize(uint64_t size);

    /**
     * \brief Get the reorder buffer size.
     *
     * \return Return the reorder buffer size.
     */
    uint64_t GetReorderBufferSize() const;

    /**
     * \brief Get the remaining reorder buffer size.
     *
     * \return Return remaining reorder buffer size.
     */
    uint64_t GetReorderBufferRemain() const;

    /**
     * \brief Update the remaining reorder buffer size.
     *
     * \param pktSize the size of packet
     * \param inc true if increase a pktSize, else decrease
     */
    void UpdateReorderBufferRemain(uint32_t pktSize, bool inc);

    /**
     * \brief Get the remaining onchip buffer size.
     *
     * \return Return remaining onchip buffer size.
     */
    uint64_t GetOnChipBufferRemain() const;

    /**
     * \brief Set the status of port level hierarating management.
     *
     * \param status the status to enable or disable the port level hierarating management.
     */
    void SetOnChipPdpStatus(bool status);

    /**
     * \brief Set queue level alpha.
     *
     * \param flowType flow type.
     * \param qIndex queue index to be set.
     * \param alpha the setting alpha.
     */
    void SetQueueLevelAlpha(uint32_t flowType, uint32_t qIndex, uint32_t priority, uint32_t alpha);

    /**
     * \brief Set priority level alpha.
     *
     * \param prior priority index to be set.
     * \param alpha the setting alpha.
     */
    void SetPriorityLevelAlpha(uint32_t prior, uint32_t alpha);

    /**
     * \brief Set port level alpha.
     *
     * \param port port index to be set.
     * \param alpha the setting alpha.
     */
    void SetPortLevelAlpha(uint32_t port, uint32_t alpha);

    /**
     * \brief Set rate type of ports.
     *
     * \param port port index to be set.
     * \param type type of port rate.
     */
    void SetPortRateType(uint32_t port, PortType type);

    /**
     * \brief Get rate type of ports.
     *
     * \param port port index.
     * \returns the type of port rate.
     */
    PortType GetPortRateType(uint32_t port);

    /**
     * \brief Get the congestion status of given queue.
     *
     * \param port the port index.
     * \param qIndex the queue index.
     * \param pri the queue priority.
     * \returns the congestion status.
     */
    QueueStatus GetQueueStatus(uint32_t port, uint32_t qIndex, uint32_t pri);

    /**
     * Set the Update Measure Window
     *
     * \param updateWindow
     */
    void SetUpdateMeasureWindow(Time updateWindow);

    /**
     * Get the Update Meausre Window
     *
     * \returns the Update Measure Window
     */
    Time GetUpdateMeasureWindow() const;

    /**
     * \brief Set the full threshold of wcache.
     *
     * \param prior the priority index.
     * \param th the threshold to be set.
     */
    void SetWcacheFullTh(uint32_t prior, uint64_t th);

    /**
     * \brief Set the congestion threshold of wcache.
     *
     * \param prior the priority index.
     * \param th the threshold to be set.
     */
    void SetWcacheCgTh(uint32_t prior, uint64_t th);

    /**
     * \brief Set the congestion threshold of queue length.
     *
     * \param pri the priority.
     * \param th the congestion threshold to be set.
     */
    void SetCgMin(uint32_t pri, uint64_t th);

    /**
     * \brief Set the uncongestion threshold of queue length.
     *
     * \param pri the priority.
     * \param th the uncongestion threshold to be set.
     */
    void SetCgMax(uint32_t pri, uint64_t th);

    /**
     * \brief Get and update the congestion status of given queue.
     *
     * \param port the port index.
     * \param qIndex the queue index.
     * \param pri the priority.
     * \returns true if this queue is congested.
     */
    QueueStatus GetHWCgStatus(uint32_t port, uint32_t qIndex, uint32_t pri);

    /**
     * \brief Get and update the congestion status of given queue.
     *
     * \param port the port index.
     * \param qIndex the queue index.
     * \param pri the priority.
     * \returns true if this queue is congested.
     */
    QueueStatus GetTimerQueueStatus(uint32_t port, uint32_t qIndex, uint32_t pri);

    /**
     * \brief Set the long congestion queue length.
     *
     * \param pri the priority.
     * \param qlen the long congestion queue length to be set.
     */
    void SetLongCgQlen(uint32_t pri, uint64_t qlen);

    /**
     * \brief Get the ptr of off-chip buffer.
     *
     * \returns the ptr of off-chip buffer.
     */
    Ptr<OffChipBuffer> GetOffChipBuffer() const;

    /**
     * \brief Attach the off-chip buffer to the MMU
     *
     * \param offChipBuffer the pointer to the off-chip buffer
     */
    void AttachOffChipBuffer(Ptr<OffChipBuffer> offChipBuffer);

    /**
     * \brief Get Total OffChip Packets.
     *
     * \return Return total OffChip Packets.
     */
    uint64_t GetTotalOffChipPkts() const;

    /**
     * \brief Get the Off-Chip Buffer HBM Packets.
     *
     * \return Return HBM Packet counters.
     */
    uint64_t GetTotalHbmPkts() const;

    /**
     * \brief Get the number of pkts currently in the buffer (including both onchip buffer and
     * offchip buffer).
     *
     * \return Return number of pkts currently in the buffer.
     */
    uint64_t GetNPackets() const;

    /**
     * \brief Show all the fundamental info of this switch mmu.
     */
    void Show();

    /**
     * \brief Show all the counters in Mmu. (Use to Debug)
     *
     * Note:
     * 1. If there are some new counters in the Mmu Model, and need to show/log
     * during debug time. We need to add.
     * 2. All the debug info using 'NS_LOG_DEBUG'. So if you wanna use it, lower
     * the LOG_LEVEL to DEBUG_LEVEL.
     * 3. It just use to print out in std now. If needed, add log to file path further.
     */
    void ShowCounters();

    /**
     * \brief Check if ingress bytes is greater than the ingress threshold.
     *
     * \param packet: the Ptr to the Ingress Packet.
     * \returns Returns the packet buffer location.
     */
    BmResult CheckIngressAdmission(Ptr<Packet> packet);

    /**
     * \brief Store a packet into the buffer(Ingress).
     *
     * \param packet: the Ptr to the Ingress Packet
     */
    void Store(Ptr<Packet> packet, BmResult location);

    /**
     * \brief Fetch a packet from buffer.
     * As the Queue Management and Packet Scheduler has decided to send
     * the Packet. Fetch the Packet(Egress).
     * from the buffer.
     *
     * \param packet: the Ptr to Ingress Packet.
     * \returns whether it is successfully to be fetched from buffer.
     */
    bool Fetch(Ptr<Packet> packet);

    /**
     * \brief Modify some counters and status after BufferEgress Complete.
     * After the packet are fully removed from Buffer.Set some queue, priority
     * and port-level counters.
     *
     * Note: The time to call this function should be after the packet
     * is confirmed to be 'completely' fetched. Or there may be some
     * unexpected counter and Buffer error.
     *
     * \param packet: the Ptr to the Packet.
     */
    void FetchComplete(Ptr<Packet> packet);

    /**
     * \brief A device handler
     *
     * This callback should be used when mmu fetch a packet from off-chip buffer to
     * explicitly invoke the AttemptTransmission of Reorder model.
     *
     * \param device a pointer to the net device which should transmit the packet.
     */ //定义了一个名为DeviceHandler的回调函数类型
    typedef Callback<bool>
        DeviceHandler; // 当内存管理单元（MMU）从片外缓冲区获取数据包时，会显式调用重新排序模块的AttemptTransmission函数

    /**
     * \param handler the handler to register
     * \param device the device attached to this handler. If the
     *        value is zero, the handler is attached to all
     *        devices on this node.
     */
    void RegisterDeviceHandler(DeviceHandler handler, Ptr<NetDevice> device);

    /**
     * \param handler the handler to unregister
     *
     * After this call returns, the input handler will never
     * be invoked anymore.
     */
    void UnregisterDeviceHandler(DeviceHandler handler);

    /**
     * \brief Handle the request from off-chip buffer.
     *
     * This method will use the matched handler to handle the request from
     * off-chip buffer.
     *
     * \param device a pointer to the net device which should transmit the packet.
     * \returns true if the matched handler is found.
     */
    bool HandleRequest(Ptr<NetDevice> dev);

    /**
     * \brief Get buffer used by the queue
     *
     * \param port the port index
     * \param priority the priority
     * \param qIndex the queue index
     * \returns the buffer used by the queue
     */
    uint64_t GetQueueUsedBuffer(uint32_t port, uint32_t priority, uint32_t qIndex) const;

    /**
     * \brief Get the maximum buffer used by the queue
     *
     * \param port the port index
     * \param priority the priority
     * \param qIndex the queue index
     * \returns the maximum buffer used by the queue
     */
    uint64_t GetQueueMaxUsedBuffer(uint32_t port, uint32_t priority, uint32_t qIndex) const;

    /**
     * \brief Get the total received bytes by the queue
     *
     * \param port the port index
     * \param priority the priority
     * \param qIndex the queue index
     * \returns the total received bytes by the queue
     */
    uint64_t GetQueueTotalReceived(uint32_t port, uint32_t priority, uint32_t qIndex) const;

  protected:
    void DoDispose() override;
    void DoInitialize() override;
    void NotifyNewAggregate() override;

    /**
     * \brief The Mmu Memory Type.
     *
     * To help Distinguish different mem type switch.
     */
    BufferModel m_memType;

  private:
    /**
     * As for the SwitchMMU, Buffer Management is an important part.
     * And to help further developing BM algorithm and comparing bet-
     * ween different BM algorithm, we need a good and clear frame work.
     *
     * The Following few functions is about to Set the Frame work of
     * Buffer Management Algorithm.
     *
     * The basic flow of buffer admission control:
     * Basic Check---->Find the buffer location---->BufferAlgorithmCheck
     *(legal or not)      (OnChip or OffChip)     (DT, ABM...Algorithm check)
     *
     * The calling chain as a preliminary idea:
     * CheckIngressAdmission--->IsPacketAcceptable
     *                          |   ↑
     *                          |   | BMResult
     *                          ↓   |
     *                      FindBufferLocation------->CheckBmAdmission
     *                          ↑                         |
     *                          ---------------------------
     *                            Return Accept or not
     *
     * The Detailed info of this structure is in:
     * https://comet-cross-085.notion.site/How-did-we-design-and-construct-the-switch-mmu-46a48f1c3c954489b70d045af4e4da22
     */

    /**
     * \brief Huawei Buffer Management Algorithm.
     *
     * \param packet The Pointer to Input Packet.
     * \return Return BMResult.
     */
    BmResult CheckHWBmAlgorithm(Ptr<Packet> packet);

    /**
     * \brief 3DT Buffer Management Algorithm.
     *
     * \param packet The Pointer to Input Packet.
     * \return Return BMResult.
     */
    BmResult Check3DTBmAlgorithm(Ptr<Packet> packet);

    /**
     * To distinguish some traffic to be Congestion Traffic.
     */
    void SelectLRUCongestion();

    /**
     * Lru Timer Update
     */
    void UpdateLruTimer();

    /**
     * To reset the queue status when the queue len is 0.
     */
    void UpdateQueueStatus(uint32_t port, uint32_t priority, uint32_t qIndex, QueueStatus status);

    /**
     * \brief YRF Buffer Management Algorithm.
     */
    BmResult CheckYRFBmAlgorithm(Ptr<Packet> packet);

    // void SaveSRAMCost(Ptr<Packet> packet);

    void YRF_BMS(double r, double etc);

    /**
     * \brief Baseline Buffer Management Algorithm.
     */
    BmResult CheckBaselineBmAlgorithm(Ptr<Packet> packet);

    /**
     * \brief DeepHir Buffer Management Algorithm.
     */
    BmResult CheckDeepHirBmAlgorithm(Ptr<Packet> packet);

    /**
     * \brief Shunlei Yang Buffer Management Algorithm.
     *
     * \param packet The Pointer to Input Packet.
     * \return Return BMResult.
     */
    BmResult CheckYSLBmAlgorithm(Ptr<Packet> packet);

    /**
     * \brief Dynamic DT for SRAM
     *
     * \param queueId
     * \return Threshold
     */
    uint64_t GetDynamicAlphaSramThreshold(uint32_t qIndex);

    /**
     * \brief Check if The Packet can be accpted.
     *
     * It is an unified API for Packet Buffer Admission Control. This
     * is compatible with many different buffer management algorithms
     * and is configurable for both on-chip and off-chip Buffer.
     *
     * \param packet The Ptr to packet that wanna to get Buffer Admission.
     * \param location The Packet Buffer Location.
     * \return Return true if the BM algorithm considers this packet acceptable.
     */
    bool CheckBmAdmission(Ptr<Packet> packet, BmResult location);

    /**
     * \brief Check if the Packet Can be Accepted by the Mmu.
     *
     * Note: The final API should be argued carefully again to determine
     * How does the BM module finally give the admission and storage
     * of the data packet.
     *
     * \param packet: the Ptr to the Packet.
     * \return Return BMResult Result.
     */
    BmResult FindBufferLocation(Ptr<Packet> packet);

    /**
     * \brief Device handler entry.
     * This structure is used to explicitly invoke the all reorder net device
     * in a switch node.
     */
    struct DeviceHandlerEntry
    {
        DeviceHandler handler; //!< the device handler
        Ptr<NetDevice> device; //!< the NetDevice
    };

    /**
     * Typedef for device handlers container.
     */
    typedef std::vector<struct DeviceHandlerEntry> DeviceHandlerList;

    static const uint64_t DEFAULT_ONCHIPBUFFER_SIZE = 4L << 20; // 4MB

    static const uint64_t DEFAULT_REORDERBUFFER_SIZE = 500L << 10; // 500KB

    /// The node this TrafficControlLayer object is aggregated to
    Ptr<Node> m_node;

    /**
     * \brief The number of switch ports
     */
    uint32_t m_nPorts;

    /**
     * \brief Queue num per port
     */
    uint32_t m_nQueuesPerPort;

    /**
     * \brief Switch Flow Priority num.
     */
    uint32_t m_nPriorities;

    /**
     * \brief Buffer Management algorithm
     */
    BmAlgorithm m_bmAlgorithm;

    // Buffer itself
    /**
     * \brief Onchip buffer size
     *
     * Onchip Buffer stands for the onchip buffer which near switch Chip.
     * The delay and the bandwidth has no effect on overall performance.
     * So it is just a counter to represent its Memory Size.
     */
    uint64_t m_onChipBufferSize;

    /**
     * \brief Reorder buffer size
     */
    uint64_t m_reorderBufferSize;

    /**
     * OffChip Buffer.
     */
    Ptr<OffChipBuffer> m_offChipBuffer;

    /**
     * \brief Remain size of onchip buffer
     */
    uint64_t m_onChipBufferRemain;

    /**
     * \brief Remain size of reorder buffer
     */
    uint64_t m_reorderBufferRemain;

    // Qlens Counter
    // TODO:
    // Check if we should log the counters here, or just use
    // QueueDisc Reference to get Queue Length info.
    std::vector<std::vector<uint64_t>> m_qlens;

    // Active Queue num per port.
    std::vector<uint64_t> m_activeQueNum;
    uint64_t m_activeQueNumSwitch = 0; //整个交换机上的所有活跃队列
    std::vector<uint64_t> m_activePortNum;

    /**
     * alpha set
     */
    std::vector<std::vector<uint64_t>> m_alpha;

    /**
     * TODO: !!!!!
     * We may need some priority related counters. But at this moment,
     * I have no idea what we need.
     */
    double Simlulator_time_stop;
    std::vector<uint64_t> m_priOnChipUsed;
    std::vector<std::vector<uint64_t>> m_priDpUsed;
    std::vector<std::vector<std::vector<uint64_t>>> m_qUsed; //!< The buffer used by each queue
    std::vector<std::vector<std::vector<uint64_t>>> m_pUsed;
    std::vector<std::vector<std::vector<uint64_t>>>
        m_qMaxUsed; //!< The maximum buffer used by each queue
    std::vector<std::vector<std::vector<uint64_t>>>
        m_qTotalRcvd; //!< The total received bytes by each queue

    /// Sram_Qlen
    std::vector<std::vector<std::vector<uint64_t>>> m_sramQlen;

    void CountDramBandwidth();

    // Some Global Counters
    TracedValue<uint64_t> m_nPackets; //!< The number of pkts currently in the Buffer
                                      //!< (in both onchip and offchip)
    Stats m_stats;                    //!< The collected statistics

    DeviceHandlerList m_handlers; //!< Device handlers in the switch mmu

    std::string baseFilePath;
    std::string nextFilePath;

    // 当前算法名字
    std::string now_algorithm_name;

    // 定义包指针记录结构
    std::map<uint64_t, std::pair<int, size_t>> Ctag;

    /**
     * params used by BmAlgHw
     */

    // Priority
    enum Pri
    {
        hp = 0, //!< HP priority
        lp = 1  //!< LP priority
    };

    // Flow type
    enum Flow
    {
        up = 0,  //!< uplink
        down = 1 //!< downlink
    };

    std::vector<PortType> m_portRates; //!< Out rates of ports, default 100Gbps

    // queue level alpha
    std::vector<std::vector<std::vector<std::vector<double>>>> m_alphaOfQueue;
    std::vector<double> m_alphaOfPriority; //<! priority level alpha
    std::vector<double> m_alphaOfPort;     //!< port level alpha
    bool m_enableOnChipPdp;                //!< enable port level hierarating management of SRAM
    std::vector<std::vector<std::vector<double>>> Packet_Num_Cycle;  // 当前周期 数据包数目
    std::vector<std::vector<std::vector<double>>> Packet_Size_Cycle; // 当前周期 数据量 +1000B
    std::vector<std::vector<std::vector<double>>>
        ReadSram_Rate_Cycle; // Csr,指优先级队列从SRAM读取的速率，以下同
    std::vector<std::vector<std::vector<double>>> WriteDram_Rate_Cycle;      // cdw
    std::vector<std::vector<std::vector<double>>> ReadDram_Rate_Cycle;       // cdr
    std::vector<std::vector<std::vector<double>>> WriteDram_Rate_Cycle_last; // cdw
    double WriteDram_Size_Total = 0;
    double ReadDram_Size_Total = 0;
    double WriteDram_Rate_Total = 0;
    double ReadDram_Rate_Total = 0;
    // double Dram_Bandwidth_Remain;
    double Dram_Bandwidth_Timer = 1000;
    int print_flag = 1;
    uint64_t min_T = 100;

    std::vector<std::vector<std::vector<double>>> ReadSram_Size_Cycle;  // Csr*etc
    std::vector<std::vector<std::vector<double>>> UsedSram_Size_Cycle;  // Csr*etc
    std::vector<std::vector<std::vector<double>>> WriteDram_Size_Cycle; // cdw*etc
    std::vector<std::vector<std::vector<double>>> ReadDram_Size_Cycle;  // cdr*etc
    std::vector<std::vector<std::vector<Time>>> Timer_Mill;             //

    std::vector<std::vector<std::vector<double>>> Packet_Size_Cycle_Max;
    std::vector<std::vector<std::vector<double>>> EWMA_R;        // lamba
    std::vector<std::vector<std::vector<bool>>> YRF_Flag_result; // =1：存片内； =0 存片外；
    std::vector<std::vector<std::vector<bool>>> YRF_Flag_result2; // =1：存片内； =0 存片外；
    std::vector<std::vector<std::vector<int>>>
        Predict_Flag_First; // 判断第一个周期 1 第一个周期； 2 第一个周期结束； 0非第一个周期
    std::vector<std::vector<std::vector<double>>> eta;           //
    std::vector<std::vector<std::vector<double>>> cs_out_array;  //
    std::vector<std::vector<std::vector<double>>> delta_Q_array; //

    double eta_base = 1e-5;
    std::vector<std::vector<std::vector<double>>> utility;
    std::vector<std::vector<std::vector<double>>> decision;
    std::vector<std::vector<std::vector<double>>> Sr_last;         // SRAM_Size_Remain  _last cycle
    std::vector<std::vector<std::vector<double>>> Dr_last;         // DRAM_BandWidth_Remain  _lat
    std::vector<std::vector<std::vector<double>>> Pqs_last;        // periodicl buffer strategy Qi_S
    std::vector<std::vector<std::vector<int>>> T_seq;              // 标记当前是第几个周期
    std::vector<std::vector<std::vector<int>>> drop_flag;          // 标记当前是第几个周期
    std::vector<std::vector<std::vector<int>>> drop_DRAM_last;     // 标记当前是第几个周期
    std::vector<std::vector<std::vector<int>>> drop_DRAM_lastlast; // 标记当前是第几个周期
    std::vector<std::vector<std::vector<int>>> drop_real_per_period; // 标记当前是第几个周期

    uint64_t n1usStoredPackets; // a

    double DT_alpha = 3;
    uint64_t target_T;
    std::vector<std::vector<std::vector<double>>> AI; // =1：存片内； =0 存片外；
    std::vector<std::vector<std::vector<double>>> MD;

    // 记录各个包进入和离开发送队列的时间点，以计算其延迟,20240408
    std::queue<Time> m_enqueTime;
    uint m_dequePktCnt;
    // 数据包的平均延迟(单位：ms)
    double m_avgDelay;
    double Total_delay;

    bool DeepHir_Flag;

    bool YRF_Flag_First; //
    // bool Predict_Flag_First;
    // bool YRF_Flag_result;
    double W, W1, W2, W3;
    double EWMA_W;
    double eta_MD;
    double Sq, Dq, Wq;
    double S_th, W_th, D_th;
    double Cq, Cqs, Cqd, Cdw;

    double ETC;
    double U_sram, U_dram;

    double ETC_S, ETC_D;
    double Cost_min_S, Cost_min_D;

    double ETC_S0, ETC_S1, ETC_S2;
    double Cost_S0, Cost_S1, Cost_S2;

    double ETC_D0, ETC_D1, ETC_D2;
    double Cost_D0, Cost_D1, Cost_D2;

    double LossPacketNum_Last;
    double LossPacketNumTotalSizeLast;

    std::vector<uint64_t> m_wcacheFullTh;            //!< full threshold of wcache
    std::vector<uint64_t> m_wcacheCgTh;              //!< congestion threshold of wcache
    std::vector<std::vector<uint64_t>> m_longCgQlen; //!< queue length of long congestion
    std::vector<std::vector<uint64_t>> m_cgMax;      //!< congested line to check the queue length
    std::vector<std::vector<uint64_t>> m_cgMin;      //!< uncongested line to check the queue length
    // congestion status of each queue
    std::vector<std::vector<std::vector<QueueStatus>>> m_cgStatus;

    // LRU related
    Time m_updateLruTimeWindow;
    Time m_LastCycleTimeLength; //!< The Log Window
    Time m_CycleTimeLength;     //!< The time length of cycle
    // Time m_Cost_ETC;   //最后计算结果
    std::vector<std::vector<std::vector<Time>>> simulation_start;
    std::vector<std::vector<std::vector<Time>>> m_Cost_ETC; // 最后计算结果
    Time simulation_end;
    Time Timer_Mill_Loss;
    std::vector<std::vector<uint64_t>> m_cgTimer; //!< Log the Cycle

    /// Ysl
    std::vector<std::vector<std::vector<double>>> m_dramAlphaOfQueue;
    std::vector<std::vector<std::vector<double>>> m_wcacheAlphaOfQueue;
    std::vector<uint32_t> m_dramAlphaOfPriority;
    std::vector<uint32_t> m_wcacheAlphaOfPriority;

    /**
     * params used by baseline BmAlg
     */
    std::vector<uint64_t> m_wredTh; //<! wred threshold for each queue
    uint64_t if_change_threshold = 1;
    uint64_t flow_rate = 100;
    double Deeohir_threshold = 0.0;
    uint64_t if_test8 = 0;
    uint64_t if_test9 = 0;

    /// @brief Qlen of WCache and Dram.
    std::vector<std::vector<std::vector<uint32_t>>> m_wcacheQlen;
    std::vector<std::vector<std::vector<uint64_t>>> m_dramQlen;
    /// @brief Pri Wcache Used.
    std::vector<uint64_t> m_priWcacheUsed;
    std::vector<uint64_t> m_priDramUsed;

    /// Traced Callback: Store a packet into memory
    TracedCallback<Ptr<const Packet>> m_traceStore;
    /// Traced Callback: Fetch a packet into memory
    TracedCallback<Ptr<const Packet>> m_traceFetch;
    /// Traced Callback: Check admission for a packet
    TracedCallback<Ptr<const Packet>, uint32_t> m_traceCheckAdmission;
    /// Traced Callback: SRAM Read Complete.
    TracedCallback<Ptr<const Packet>> m_traceSramReadComplete;
    /// Traced Callback: SRAM Write Complete.
    TracedCallback<Ptr<const Packet>> m_traceSramWriteComplete;
};
} // namespace ns3

#endif /* SWITCH_MMU_H */
