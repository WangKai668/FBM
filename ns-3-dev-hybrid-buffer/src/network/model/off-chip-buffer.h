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

#ifndef OFF_CHIP_BUFFER_H
#define OFF_CHIP_BUFFER_H

#include "switch-mmu.h"

#include "ns3/callback.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <unordered_map>

namespace ns3
{

class Packet;
class SwitchMmu;

/**
 *
 * \ingroup point-to-point
 *
 * \brief An off chip memory model.
 *
 * - About OffChipBuffer Model Architecture:
 * Simple schematic of typical hybrid off chip buffer model.
 *
 *      -----------------------------------
 *      | Off-Chip-Buffer Model           |
 *      |              -----------------  |
 *      |             |                 | |
 *      |  ------     | off-chip buffer | |
 *      | |wcache|____|     (DRAM)      | |
 *      | |______|    |                 | |
 *      |             |_________________| |
 *      -----------------------------------
 *
 * When it comes to write the data to the Off-Chip-Buffer.
 * The data flow simple schematic should be as below:
 *
 *      -----------------------------------
 *      | Off-Chip-Buffer Model           |
 *      |              -----------------  |
 *      |             |                 | |
 * packe|t ------     | off-chip buffer | |
 * -----|>|wcache|--->|     (DRAM)      | |
 *      | |______|    |                 | |
 *      |             |_________________| |
 *      -----------------------------------
 *
 * The data goes into wcache (SRAM), before it goes into DRAM(HBM).
 *
 * When it comes to read the data from the Off-Chip-Buffer.
 * The data flow simple schematic should be as below:
 *
 *      -----------------------------------
 *      | Off-Chip-Buffer Model           |
 *      |  ----                           |
 *      | |Map |_________________         |
 * -----|-|    |Check if in DRAM?|        |
 *      | |____|                 ↓        |
 *      | in|wcache?   -----------------  |
 *      |   ↓         |                 | |
 *      |  ------     | off-chip buffer | |
 * <----|-|wcache|    |     (DRAM)      | |
 *      | |______|    |                 | |
 * <----|-------------|_________________| |
 *      -----------------------------------
 *
 * If there is an Ask for fetching the data from Off-Chip-Buffer
 * Model. The model first check the packet map to find out where
 * the packet is(Wcache or DRAM). And then extract the data from
 * the data place, the reading overhead should be different.
 *
 * - About DRAM/HBM Working Simulation Model:
 * Compared with the network speed and the processing speed of the
 * switch chip, the W/R speed of HBM is very slow. To prevent the
 * slowest part of the system from slowing down the system speed.
 * Asynchronous communication should be used between the switch and
 * HBM. And To greatly improve the processing power and enhance
 * the flexibility of W/R ability. The HBM W/R share a same engine
 * and channel, and if there both request in Write and Read come in
 * the same time, the engine can only choose one to satisfy, and the
 * other one may need to be rescheduled to be finished.
 *
 * The detailed Working Model of OffChipBuffer in:
 * https://comet-cross-085.notion.site/How-did-we-design-OffChipBuffer-9149cd91d2af48b78569be1eaed2c2d4
 */
class OffChipBuffer : public Object
{
  public:
    /// \brief Structure that keeps the MMU statistics
    struct Stats
    {
        uint64_t nTotalDramStoredPackets;   //!< The Total Packets Num stored into off-chip
                                            //!< buffer (including those have left)
        uint64_t nTotalWcacheStoredPackets; //!< The total number of packets stored into wcache
                                            //!< (including those have left)
        /// constructor
        Stats();
    };

    /**
     * \brief The DRAM/Channel State.
     *
     * The Switch need to initiate a signal ring the DRAM when the DRAM is not
     * working(IDLE), but the Switch wanna to store or fetch a packet to drive
     * DRAM go to work. This 'State' help to Distinguish DRAM working status so
     * that Switch can know when they need to initiate a signal, when they do
     * not need.
     */
    typedef enum
    {
        INITIALIZING,
        IDLE,
        BUSY
    } State;

    /**
     * \brief The bus arbitration result
     *
     * The DRAM W/R share a same engine and Channel, so the it may need a
     * Arbitration Mechanism for DRAM to determine what to satisfy next
     * (Write or Read). It is the enum result type for Arbitration Result.
     */
    typedef enum
    {
        PKTWRITE,
        PKTREAD,
        NONE
    } ArbResult;

    /**
     * \brief The Selected bus arbitration algorithm.
     *
     * To make different bus arbitration algorithms run under the same code
     * framework and reduce the adaptation overhead, we have developed a set
     * of configurable arbitra-frame. It is the enum Arbitration Algorithm type.
     */
    typedef enum
    {
        INTERLEAVEDWRR,
        CLASSICALWRR
    } ArbAlgorithm;

    /**
     * Get the TypeId
     *
     * \return The TypeId for this class
     */
    static TypeId GetTypeId();

    /**
     * \brief Contruct a OffChipBuffer.
     */
    OffChipBuffer();

    /**
     * \brief Destroy a offChipBuffer.
     */
    ~OffChipBuffer() override;

    /**
     * \brief Retrieve all the collected statistics.
     * \return the collected statistics.
     */
    const Stats& GetStats();

    /**
     * Store a packet from Switch MMU.
     *
     * \param packet:the Ptr to Packet about the Ingress Packet.
     * \return true if the Switch Store operation is successful.
     */
    bool Write(Ptr<Packet> packet);

    /**
     * Fetch a packet from Model.
     *
     * \param packet:the Ptr to Packet about the Ingress Packet.
     * \return true if the Switch Fetch operation is successful.
     */
    bool Read(Ptr<Packet> packet);

    /**
     * Set the SwitchMMU that owns this OffChipBuffer.
     *
     * \param mmu the mmu owns this OffChipBuffer.
     */
    bool SetMmu(Ptr<SwitchMmu> mmu);

    /**
     * Get the SwicthMMU Reference that owns this OffChipBuffer.
     *
     * \return Ptr to SwitchMMU.
     */
    Ptr<SwitchMmu> GetMmu() const;

    /**
     * Set the Write-in Cache Size.
     *
     * \param size The setting size of Write-in Cache. Write-in Cache
     * won't be larger than 1GB, so uint32_t will be enough.
     * \return true if the setting size is legal for Write-in Cache.
     */
    bool SetWcacheSize(uint32_t size);

    /**
     * Get the Write-in Cache Size in this OffChipBuffer.
     *
     * \return size of the Write-in Cache.
     */
    uint32_t GetWcacheSize() const;

    /**
     * Set the DRAM size in this OffChipBuffer.
     *
     * \param size The setting size of offchip buffer.
     * Offchip buffer may be very large even larger than 8GB,
     * so using uint64_t instead of uint32_t.
     *
     * \return true if the setting size is legal.
     */
    void SetDramSize(uint64_t size);

    /**
     * Get the DRAM Size in this OffChipBuffer.
     *
     * \return the setting size.
     */
    uint64_t GetDramSize() const;

    uint64_t GetDramBandwidth() const;

    /**
     * Set the Channel Delay between HBM and Switch. If the Switch
     * Wanna push a W/R Request in HBM Req-Queue and ring the HBM
     * to complete the corresponding operation, it may takes Channel
     * Delay Time to Send the Request.
     *
     * \param t the Channel Delay between Switch and HBM.
     */
    void SetChannelDelay(Time t);

    /**
     * Get the Channel Delay between HBM and Switch.
     *
     * \return the Channel Delay between Switch and HBM.
     */
    Time GetChannelDelay() const;

    /**
     * Set the HBM W/R Arbitration Algorithm.
     *
     * \param algo the setting Arbitration Algorithm that should be used
     * in HBM W/R Arbitration.
     */
    void SetArbAlgorithm(ArbAlgorithm algo);

    /**
     * Get the HBM W/R Arbitration Algorithm.
     *
     * \return the Arbitration Algorithm using in the HBM.
     */
    ArbAlgorithm GetArbAlgorithm() const;

    /**
     * \brief Get the used bytes of wcache.
     *
     * \return the used bytes of wcache.
     */
    uint64_t GetWcacheUsed() const;

    /**
     * \brief Get the used bytes of Dram.
     *
     * \return the used bytes of Dram.
     */
    uint64_t GetDramUsed() const;

    /**
     * \brief Get the remain bytes of DRAM.
     *
     * \return the remain bytes of DRAM.
     */
    uint64_t GetDramRemain() const;

    /**
     * \brief Show all the fundamental info about this Off-Chip-Buffer.
     */
    void Show();

    /**
     * \brief Show all the counters in this Off-Chip-Buffer.
     */
    void ShowCounters();

  private:
    /**
     * As long as there is currently an external W/R request for a
     * packet, the external will initiate a signal to ring the HBM
     * to complete the corresponding operation. But if there are many
     * Request need to be satisfied, the W/R Arbitration mechanism will
     * help HBM to decide what to do next turn. Because HBM working mode
     * is automatically-continue turn by turn when the W/R requests are
     * piled up, similar to the Cycle of the logic circuit, so the functions
     * followed are all named with prefix 'Cycle'. Every 'CycleStart' to
     * 'CycleComplete' stands for a W/R request has been satisfied.
     *
     * All in all, in order to better simulate the HBM working model, we
     * will mix read and write, and use "Cycle" instead of read and write
     * separately. In conclusion, the W/R operation of HBM are separated
     * from the W/R request of DRAM. You may apply for a read
     * request to DRAM, but it can be satisfied after long time.
     */

    /**
     * Schedule the HBM working Cycle.
     */
    void ScheduleCycle(void);

    /**
     * Schedule the HBM working Cycle Start.
     */
    void ScheduleCycleStart(ArbResult result);

    /**
     * Schedule the HBM working Cycle Start.
     */
    void ScheduleCycleAtomic(Ptr<Packet> packet, ArbResult result, uint64_t leftOpCnt);

    /**
     * Schedule the HBM working Cycle Done
     *
     * \param packet the Ptr to packet that this HBM Cycle work for.
     * \param result the Arbitration Result of this HBM Working Cycle.
     */
    void ScheduleCycleComplete(Ptr<Packet> packet, ArbResult result);

    /// Arbitration Algorithm
    /**
     * Do HBM W/R Arbitration.
     *
     * \return The Arbitration Algorithm Result.
     */
    ArbResult ArbitrateWR();

    /**
     * Read Write-Request Queue Arbitration Algorithm: WRR.
     * It is interleaved WRR algorithm.
     *
     * \return Returns the Arbitration Result.
     */
    ArbResult DoReadWriteInterleavedWrr();

    /**
     * Read Write-Request Queue Arbitration Algorithm: WRR.
     * It is Classical WRR algorithm.
     *
     * \return Returns the Arbitration Result.
     */
    ArbResult DoReadWriteClassicalWrr();

    /**
     * Start Sending a Write Request to the HBM Buffer.
     *
     * \param packet the Ptr to packet that need to write.
     * \return Returns True if the Write Request sending successfully.
     */
    bool SendWriteRequestStart(Ptr<Packet> packet);

    /**
     * Stop Sending a Write Request.
     *
     * \param packet the Ptr to packet that need to write.
     */
    void SendWriteRequestComplete(Ptr<Packet> packet);

    /**
     * \brief Force cancellation of write operations for packets that are being
     * written to HBM.
     *
     * \param packet The Ptr to Packet that need to cancel Write operation.
     */
    void CancelWrite(Ptr<Packet> packet);

    /**
     * Start Sendding a Read Request to the HBM Buffer.
     *
     * \param packet the Ptr to packet that need to read.
     * \return Returns True if the Read Request sending successfully.
     */
    bool SendReadRequestStart(Ptr<Packet> packet);

    /**
     * Stop Sending a Read Request.
     *
     * \param packet the Ptr to packet that need to write.
     */
    void SendReadRequestComplete(Ptr<Packet> packet);

    // static const uint64_t DEFAULT_WCACHE_SIZE = 400L << 10;       // 400KB
    // static const uint64_t DEFAULT_OFFCHIPBUFER_SIZE = 10L << 30;  // 10GB
    // static const uint64_t DEFAULT_HBM_BITWIDTH = 128;             // 1024bit = 128byte
    // static const uint64_t DEFAULT_HBM_BURST_LENGTH = 4;           // 4
    // static const uint64_t DEFAULT_HBM_BUS_FREQUENCY = 2000000000; // 2000MHZ
    // static const uint64_t DEFAULT_HBM_ATOMIC_LENGTH = 2; // Atomic Op = 2 HBM Burst Mode op.

    // yaorenfu
    static const uint64_t DEFAULT_WCACHE_SIZE = 50L << 10;      // 50KB
    static const uint64_t DEFAULT_OFFCHIPBUFER_SIZE = 2L << 30; // 2GB
    static const uint64_t DEFAULT_HBM_BITWIDTH = 128;           // 1024bit = 128byte
    static const uint64_t DEFAULT_HBM_BURST_LENGTH = 4;         // 4
    static const uint64_t DEFAULT_HBM_BUS_FREQUENCY =
        1000000000; // 1000MHZ   1000MHZ * 1024bit= 1024*1000 M b /s = 1000 Gbps
    static const uint64_t DEFAULT_HBM_ATOMIC_LENGTH = 2; // Atomic Op = 2 HBM Burst Mode op.

    /**
     * Ptr to SwitchMmu Owning this OffChipBuffer.
     */
    Ptr<SwitchMmu> m_mmu;

    /**
     * Ptr to node which the OffChipBuffer attach to.
     */
    Ptr<Node> m_node;

    /**
     * The working HBM W/R Arbitration Algo.
     */
    ArbAlgorithm m_arbAlgorithm;

    // WRR Arbitration member here.
    uint32_t m_wrrWWeight; //!< The setting weight of Packet Write.
    uint32_t m_wrrRWeight; //!< The setting weight of Packet Read.

    // Interleaved WRR member here.
    uint32_t m_wrrQueIndex; //!< The QueIndex of begin round.
                            //!< Just 2 Queue here(Write and Read Queue).
                            //!< index 0 stand for Write and 1 stand for Read.

    // Interleaved Wrr Attribute.
    uint32_t m_interleavedWrrRoundCnt; //!< The round counter.

    // Classical Wrr Attribute.
    uint32_t m_classicalWrrPacketCnt;

    /**
     * The interReq gap time and Bank Conflict Probility between two request
     *
     * Note: Cx is the abbreviation of Conflict
     */
    Time m_tInterBankCxReadReqGap; //!< The interReq gap between two Bank Conflict Read requests.
    Time m_tInterWRReqGap;         //!< The interReq gap between Write and Read request.
    Time m_tInterRWReqGap;         //!< The interReq gap between Read and Write request.
    double m_probOfBankCxReadReqs; //!< The Probability of Read Bank Conflict

    // Use to Log how many times the conflict happened between two Req.
    uint64_t m_nWrite2ReadCx;   //!< The Write-Read Conflict Num.
    uint64_t m_nRead2WriteCx;   //!< The Read-Write Conflict Num.
    uint64_t m_ntwoBankCxRead;  //!< The Bank Conflict Num Between two Read Request.
    uint64_t m_ntwoBankCxWrite; //!< The Bank Conflict Num Between two Write Request.

    /**
     * The Wcache Size
     */
    uint32_t m_wcacheSize;

    /**
     * The DRAM Size
     */
    uint64_t m_dramSize;

    /**
     * Read/Write DDR Channel Delay
     */
    Time m_channelDelay;

    /**
     * Dram BitWidth.
     */
    uint64_t m_dramBitWidth;

    /**
     * Dram BurstLength.
     */
    uint64_t m_dramBurstLength;

    /**
     * Dram Bus Frequency.
     */
    uint64_t m_dramBusFrequency;

    /**
     * Dram Atomic Op Length.
     */
    uint64_t m_dramAtomicOpLength;

    /**
     * The working state of HBM engine.
     */
    State m_state;

    // TODO:
    // There may need some more counters to get more statistics
    // to help us to finish our BM algorithm in the future.
    // Maybe in the priority level or Queue length level.

    // some counters to log the status of buffer
    /**
     * Used Write-in Cache size.
     */
    uint32_t m_wcacheUsed;

    /**
     * The Total Packet num in Wcache.
     */
    uint32_t m_nWcachePackets;

    /**
     * Used HBM Buffer size.
     */
    uint64_t m_dramUsed;

    /**
     * The Total Packet num in Buffer now.
     */
    uint64_t m_nDramPackets;

    /**
     * \brief The Request Vector Used to store W/R Request.
     *
     * The bandwidth of HBM is much lower than the limit demand
     * that can be generated by switch traffic. So there is need to
     * set a Queue to separately log the W/R request.
     */
    typedef std::vector<Ptr<Packet>> RequestVec;
    RequestVec m_writeRequestVec;
    RequestVec m_readRequestVec;

    /**
     * Some registers that record the running status at runtime.
     */
    ArbResult m_lastArbResult;        //!< The last Cycle Arbitration Result.
    Time m_lastCycleEndTime;          //!< The last 'Cycle' End Time.
    Ptr<Packet> m_writingPacket;      //!< The Ptr to Packet which is writing in HBM at this time.
    EventId m_interWriteReqWaitEvent; //!< Event id for HBM Write Req Wait event.
    bool m_writingStopFlag;           //!< The Flag indicates the writing event should stop.

    /// The maximum of Read-Req-Queue Length, to avoid long time waiting.
    uint32_t m_thresholdOfReadRequest;

    Stats m_stats; //!< The collected statistics

    /** OffChipBuffer read start trace source */
    TracedCallback<Ptr<const Packet>> m_traceDramReadStart;

    /**
     * OffChipBuffer write start trace source
     */
    TracedCallback<Ptr<const Packet>> m_traceDramWriteStart;

    /**
     * OffChipBuffer Read Complete trace source
     */
    TracedCallback<Ptr<const Packet>> m_traceDramReadComplete;

    /**
     * OffChipBuffer Write Complete trace source
     */
    TracedCallback<Ptr<const Packet>> m_traceDramWriteComplete;

    /**
     * OffChipBuffer WCache Read Complete trace source.
     */
    TracedCallback<Ptr<const Packet>> m_traceWcacheReadComplete;

    /**
     * OffChipBuffer WCache Write Complete trace source.
     */
    TracedCallback<Ptr<const Packet>> m_traceWcacheWriteComplete;

    /**
     *
     */
    uint64_t m_cancelWriteTime;
};
} // namespace ns3

#endif /* OFF_CHIP_BUFFER_H */
