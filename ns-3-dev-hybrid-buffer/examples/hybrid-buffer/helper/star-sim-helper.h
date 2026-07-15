#ifndef HYBRID_BUFFER_STAR_SIMULATION_HELPER_H
#define HYBRID_BUFFER_STAR_SIMULATION_HELPER_H

#include "sim-helper.h"
#include <tuple>
#include <optional>
#include <string>
#include <algorithm>

namespace ns3
{

namespace hb
{

class StarSimHelper : public SimHelper
{
  public:
    StarSimHelper(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));
    ~StarSimHelper() override;
    // TCP / UDP 全局配置
    enum class TransportProtocol
    {
        TCP,
        UDP
    };
    void SetTransportProtocol(const std::string& protocol);
    bool IsTcpTransport() const
    {
        return m_transportProtocol == TransportProtocol::TCP;
    }
    bool IsUdpTransport() const
    {
        return m_transportProtocol == TransportProtocol::UDP;
    }
    // 重写父类的 ConfigTransport
    void ConfigTransport(
        std::string socketType = "tcp",
        std::string ccType = "ns3::TcpDctcp") override;
    // --sj 添加控制
    void ConfigTopology() override
    {
        ConfigTopology(3, DataRate("40Gbps"), MicroSeconds(100));
    }

    virtual void ConfigTopology(uint32_t numSpokes, DataRate linkCapacity, Time linkDelay);

    virtual void ConfigTopology(uint32_t numSpokes,
                                uint32_t numReceivers,
                                DataRate recvLinkCapacity,
                                Time recvLinkDelay,
                                DataRate sendLinkCapacity,
                                Time sendLinkDelay);

    void ConfigTraffic() override
    {
        ConfigTraffic(10 << 20);
    }

    virtual void ConfigTraffic(uint64_t flowSize);
    void CreateTopology() override;
    void SetupMmu() override;
    void SetupOffChipBuffer() override;
    void SetupHostQueueDisc() override;
    void SetupRouterQueueDisc() override;
    void SetupRouterPacketFilter() override;
    void CreateTraffic() override;

    void TraceSocket() override;
    void TraceMmu() override;
    void TraceOffChipBuffer() override;

    Time GetRtt() override
    {
        return 2 * (m_recvLinkDelay + m_sendLinkDelay);
    }

    uint64_t GetBdp() override
    {
        DataRate bottleLinkCapacity = std::min(m_recvLinkCapacity, m_sendLinkCapacity);
        return static_cast<uint64_t>(bottleLinkCapacity * GetRtt() / 8);
    }

    void EnableHbmThroughputTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableHbmThroughputTracing = true;
        m_hbmThroughputMeasureWindow = measureWindow;
    }

    void EnableWCacheThroughputTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableWCacheThroughputTracing = true;
        m_wcacheThroughputMeasureWindow = measureWindow;
    }

    void EnableSramThroughputTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableSramThroughputTracing = true;
        m_sramThroughputMeasureWindow = measureWindow;
    }

    void EnableQueueWCacheTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableQueueWCacheTracing = true;
        m_queueWCacheMeasureWindow = measureWindow;
    }

    void EnableQueueSramTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableQueueSramTracing = true;
        m_queueSramMeasureWindow = measureWindow;
    }

    void EnableQueueHbmTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableQueueHbmTracing = true;
        m_queueHbmMeasureWindow = measureWindow;
    }

    void EnableBufferUsageTracing()
    {
        m_enableBufferUsageTracing = true;
    }

    void EnableBmResultTracing()
    {
        m_enableBmResultTracing = true;
    }

    void EnablePortThroughputTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enablePortThroughputTracing = true;
        m_portThroughputMeasureWindow = measureWindow;
    }

    void EnableQueueThroughputTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableQueueThroughputTracing = true;
        m_queueThroughputMeasureWindow = measureWindow;
    }

    void EnableSqThroughputTracing(Time measureWindow = MicroSeconds(1))
    {
        m_enableSqThroughputTracing = true;
        m_sqThroughputMeasureWindow = measureWindow;
    }

    Ptr<Node> GetSpokeNode(uint32_t i)
    {
        return m_spokes.Get(i);
    }

    Ptr<NetDevice> GetSpokeDevice(uint32_t i)
    {
        return m_spokeDevices.Get(i);
    }

    void AddFlow(uint32_t srcId,
                 uint32_t dstId,
                 Time start,
                 Time stop,
                 DataRate rate = 0,
                 uint64_t flowSize = 0,
                 std::string onTime = "ns3::ConstantRandomVariable[Constant=1000]",
                 std::string offTime = "ns3::ConstantRandomVariable[Constant=0]",
                 uint32_t pktSize = 0) override
    {
        if (rate == 0)
        {
            rate = m_sendLinkCapacity;
        }
        FlowInfo flow = {.srcId = srcId,
                         .dstId = dstId,
                         .startTime = start,
                         .stopTime = stop,
                         .rate = rate,
                         .size = flowSize,
                         .onTime = onTime,
                         .offTime = offTime,
                         .pktSize = pktSize};
        SimHelper::AddFlow(flow);
    }

  private:
    void SetupDefaultTraffic();

  protected:

    // 全局传输协议，默认使用 TCP
    TransportProtocol m_transportProtocol = TransportProtocol::UDP;
    // topology parameters
    uint32_t m_nSpokes;          //!< Number of spoke nodes
    uint32_t m_nReceivers;       //!< Number of receivers (the first m_nReceivers spoke nodes are
                                 //!< receivers and others are senders)
    DataRate m_recvLinkCapacity; //!< Link capacity between the hub node and each receiver node
    Time m_recvLinkDelay;        //!< Link delay between the hub node and each receiver node
    DataRate m_sendLinkCapacity; //!< Link capacity between the hub node and each receiver node
    Time m_sendLinkDelay;        //!< Link delay between the hub node and each receiver node
    Ptr<Node> m_hub;             //!< Hub node
    Ptr<SwitchMmu> m_mmu;
    Ptr<OffChipBuffer> m_offChipBuffer;
    NetDeviceContainer m_hubDevices;          //!< Hub node NetDevices
    NodeContainer m_spokes;                   //!< Spoke nodes
    NetDeviceContainer m_spokeDevices;        //!< Spoke nodes NetDevices
    Ipv4InterfaceContainer m_hubInterfaces;   //!< IPv4 hub interfaces
    Ipv4InterfaceContainer m_spokeInterfaces; //!< IPv4 spoke nodes interfaces
    // Traffic
    uint64_t m_flowSize;
    std::vector<ApplicationContainer> m_sourceApps;
    std::vector<ApplicationContainer> m_sinkApps;
    ObjectFactory m_routerFifoQdiscFactory;
    ObjectFactory m_routerRedQdisFatory;
    // Tracing
    // Throughput
    bool m_enableHbmThroughputTracing;
    bool m_enableWCacheThroughputTracing;
    bool m_enableSramThroughputTracing;
    uint64_t m_hbmReadBytes;
    uint64_t m_hbmWriteBytes;
    uint64_t m_wcacheReadBytes;
    uint64_t m_wcacheWriteBytes;
    uint64_t m_sramReadBytes;
    uint64_t m_sramWriteBytes;
    Time m_hbmThroughputMeasureWindow;
    Time m_wcacheThroughputMeasureWindow;
    Time m_sramThroughputMeasureWindow;
    bool m_enableBufferUsageTracing;
    bool m_enableBmResultTracing;
    bool m_enablePortThroughputTracing;
    bool m_enableQueueThroughputTracing;
    bool m_enableSqThroughputTracing;
    // Queue level Buffer Usage and Throughput
    bool m_enableQueueWCacheTracing;
    bool m_enableQueueSramTracing;
    bool m_enableQueueHbmTracing;
    Time m_portThroughputMeasureWindow;
    Time m_queueThroughputMeasureWindow;
    Time m_sqThroughputMeasureWindow;
    Time m_queueWCacheMeasureWindow;
    Time m_queueSramMeasureWindow;
    Time m_queueHbmMeasureWindow;




    std::vector<uint64_t> m_portSendBytes;
    std::vector<uint64_t> m_hostRxBytes;  //主机侧的吞吐
    std::vector<uint64_t> m_hostTxBytes;  //主机侧的吞吐

    std::vector<std::vector<uint64_t>> m_sqSendBytes;
    std::vector<std::vector<std::vector<uint64_t>>> m_queueSendBytes;

    /// @brief WCache Throughput Per Queue.
    std::vector<std::vector<std::vector<uint64_t>>> m_qWCacheWriteBytes;
    std::vector<std::vector<std::vector<uint64_t>>> m_qWCacheReadBytes;
    /// @brief WCache Usage Per Queue.
    std::vector<std::vector<std::vector<uint64_t>>> m_qWCacheLength;

    /// @brief Sram Throughput Per Queue.
    std::vector<std::vector<std::vector<uint64_t>>> m_qSramWriteBytes;
    std::vector<std::vector<std::vector<uint64_t>>> m_qSramReadBytes;
    /// @brief Sram Usage Per Queue.
    std::vector<std::vector<std::vector<uint64_t>>> m_qSramLength;

    /// @brief Sram Throughput Per Queue.
    std::vector<std::vector<std::vector<uint64_t>>> m_qHbmWriteBytes;
    std::vector<std::vector<std::vector<uint64_t>>> m_qHbmReadBytes;
    /// @brief Sram Usage Per Queue.
    std::vector<std::vector<std::vector<uint64_t>>> m_qHbmLength;
};

} // namespace hb

} // namespace ns3

#endif
