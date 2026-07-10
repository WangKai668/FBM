#ifndef HYBRID_BUFFER_SIMULATION_HELPER_H
#define HYBRID_BUFFER_SIMULATION_HELPER_H

#include "ns3/applications-module.h"
#include "ns3/attribute-construction-list.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <iostream>

namespace ns3
{

namespace hb
{

class SimHelper : public Object
{
  public:
    SimHelper(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));

    virtual ~SimHelper();

    void Run();
    // Setup all
    void Setup();
    // Post actions after simulation is done
    virtual void Finish();

    // MTU size in bytes
    static const uint32_t MTU_BYTES = 1480; //9600;//1480; //default 9600;
    // Length of ppp header
    static const uint32_t PPPHDR_BYTES = 2;
    // Length of ip header
    static const uint32_t IPHDR_BYTES = 20;
    // Length of tcp header, including 12 bytes of options
    static const uint32_t TCPHDR_BYTES = 20 + 12;
    // Length of udp header
    static const uint32_t UDPHDR_BYTES = 8;
    // length of ip payload
    static const uint32_t IPPAYLOAD_BYTES = MTU_BYTES - IPHDR_BYTES - PPPHDR_BYTES;
    // Length of tcp payload
    static const uint32_t TCPPAYLOAD_BYTES = IPPAYLOAD_BYTES - TCPHDR_BYTES;
    // Length of udp payload
    static const uint32_t UDPPAYLOAD_BYTES = IPPAYLOAD_BYTES - UDPHDR_BYTES;

    // Convert applicate-layer traffic rate to physical-layer traffic rate
    static DataRate ConvertPhyRateToAppRate(DataRate phyRate, std::string socketType);

    DataRate ConvertPhyRateToAppRate(DataRate phyRate);

    // Flow info
    struct FlowInfo
    {
        uint32_t srcId; //!< source id
        uint32_t dstId; //!< destination id
        Time startTime; //!< flow start time
        Time stopTime;  //!< flow stop time
        DataRate rate;  //!< sending rate
        uint64_t size;  //!< flow size in bytes
        std::string onTime;  //!< udp onoff on time
        std::string offTime; //!< udp onoff off time
        uint32_t pktSize;    //!< size of packet
        void Print(std::ostream& os) const;
    };

    // Get the payload size of a packet
    // Payload size = MTU - header length
    uint32_t GetPayloadSize()
    {
        if (m_socketType == "tcp")
        {
            return TCPPAYLOAD_BYTES;
        }
        else if (m_socketType == "udp")
        {
            return UDPPAYLOAD_BYTES;
        }
        else
        {
            return MTU_BYTES - IPHDR_BYTES;
        }
    }

    virtual void Initialize();

    virtual void ConfigTopology() = 0;
    // NOTE: transport configuration should be run AFTER topology configuration
    virtual void ConfigTransport(std::string socketType = "tcp",//"udp", modify be wk  default udp
                                 std::string ccType = "ns3::TcpDctcp"); //TcpDctcp"); //TcpCubic");  default TcpNewReno");
    // virtual void ConfigTransport(std::string socketType = "udp",//"udp", modify be wk  default udp
    //                             std::string ccType = "ns3::TcpNewReno"); //TcpDctcp"); //TcpCubic");  default TcpNewReno");

    virtual void ConfigRoute()
    {
        Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
    }

    virtual void ConfigTraffic() = 0;
    virtual void CreateTopology() = 0;
    virtual void SetupMmu() = 0;
    virtual void SetupOffChipBuffer() = 0;
    virtual void SetupHostQueueDisc() = 0;
    virtual void SetupRouterQueueDisc() = 0;
    virtual void SetupRouterPacketFilter() = 0;
    virtual void PopulateRoutingTables();
    virtual void CreateTraffic() = 0;

    virtual void SetupTracing();

    virtual void TraceSocket() = 0;
    virtual void TraceMmu() = 0;
    virtual void TraceOffChipBuffer() = 0;

    virtual void TraceFlows();

    virtual Time GetRtt() = 0;
    virtual uint64_t GetBdp() = 0;

    virtual uint32_t GetMaxCwnd()
    {
        return 10 * GetBdp(); 
    }

    void SetMmuAttribute(std::string name, const AttributeValue& value)
    {
        m_mmuFactory.Set(name, value);
    }

    void SetOffChipBufferAttribute(std::string name, const AttributeValue& value)
    {
        m_offChipBufferFactory.Set(name, value);
    }

    void AddFlow(FlowInfo flow)
    {
        m_flows.push_back(flow);
    }

    virtual void AddFlow(uint32_t srcId,
                         uint32_t dstId,
                         Time start,
                         Time stop,
                         DataRate rate = 0,
                         uint64_t flowSize = 0,
                         std::string onTime = "ns3::ConstantRandomVariable[Constant=1000]",
                         std::string offTime = "ns3::ConstantRandomVariable[Constant=0]",
                         uint32_t pktSize = 0)
    {
        FlowInfo flow = {.srcId = srcId,
                         .dstId = dstId,
                         .startTime = start,
                         .stopTime = stop,
                         .rate = rate,
                         .size = flowSize,
                         .onTime = onTime,
                         .offTime = offTime,
                         .pktSize = pktSize};
        AddFlow(flow);
    }

  protected:
    Time m_startTime;
    Time m_stopTime;
    std::string m_socketType;
    std::string m_ccType;
    std::string m_simName;
    FlowMonitorHelper m_flowMonitorHelper;

    QueueSize m_reorderQueueSize;
    ObjectFactory m_mmuFactory;
    ObjectFactory m_offChipBufferFactory;

    std::vector<FlowInfo> m_flows;
};

inline std::ostream&
operator<<(std::ostream& os, const SimHelper::FlowInfo& flow)
{
    flow.Print(os);
    return os;
}

} // namespace hb

inline void
PrintProgress(Time interval)
{
    std::cout << Simulator::Now().GetSeconds() << "s" << std::endl;
    Simulator::Schedule(interval, &PrintProgress, interval);
}

typedef enum
{
    PacketRead = 0,
    PacketReadWCache = 1,
    PacketWrite = 2
} RwType;

inline void
TraceOffChipBufferRwComplete(uint64_t *pTotalBytes,
                             std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                             RwType type, Ptr<const Packet> packet)
{
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t opsize = packet->GetDramStoredSize();

    (*pTotalBytes) += opsize;
    (*qTotalBytes)[port][priority][qIndex] += opsize;
}

inline void
TraceWCacheRwComplete(uint64_t *pTotalBytes,
                      std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                      RwType type, Ptr<const Packet> packet)
{
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t dramSize = packet->GetDramStoredSize();
    uint32_t psize = packet->GetSize();
    uint32_t wcacheSize = psize - dramSize;

    if (type == PacketWrite)
    {
        (*pTotalBytes) += psize + 22;
        (*qTotalBytes)[port][priority][qIndex] += psize + 22;
    }
    else if (type == PacketRead)
    {
        (*pTotalBytes) += wcacheSize;
        (*qTotalBytes)[port][priority][qIndex] += wcacheSize;
    }
}

inline void
TraceSramRwComplete(uint64_t *pTotalBytes,
                    std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                    RwType type, Ptr<const Packet> packet)
{
    // For Sram it read and write in a unit of Packet.
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t psize = packet->GetSize();

    if (type == PacketRead) {
        *pTotalBytes += psize + 22;
        (*qTotalBytes)[port][priority][qIndex] += psize + 22;
    }
    else if (type == PacketWrite) {
        *pTotalBytes += psize;
        (*qTotalBytes)[port][priority][qIndex] += psize;
    }
}

inline void
TraceBufferUsage(Ptr<OutputStreamWrapper> stream,
                 Ptr<SwitchMmu> mmu,
                 RwType type,
                 Ptr<const Packet> packet)
{
    Time now = Simulator::Now();
    Ptr<OffChipBuffer> offChipBuffer = mmu->GetOffChipBuffer();
    uint64_t onChipUsed = mmu->GetOnChipBufferSize() - mmu->GetOnChipBufferRemain();
    uint64_t wcacheUsed = 0;
    uint64_t dramUsed = 0;
    if (offChipBuffer)
    {
        wcacheUsed = offChipBuffer->GetWcacheUsed();
        dramUsed = offChipBuffer->GetDramSize() - offChipBuffer->GetDramRemain();
    }
    *stream->GetStream() << now.GetSeconds() << "," << onChipUsed << "," << wcacheUsed << ","
                         << dramUsed << std::endl;
}

inline void
TraceWCacheBufferUsage(Ptr<OutputStreamWrapper> stream,
                       std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                       RwType type, Ptr<const Packet> packet)
{
    Time now = Simulator::Now();
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t dramSize = packet->GetDramStoredSize();
    uint32_t psize = packet->GetSize();
    uint32_t wcacheSize = psize - dramSize;

    if (type == PacketWrite)
    {
        (*qTotalBytes)[port][priority][qIndex] += psize + 22;
    }
    else if (type == PacketRead)
    {
        (*qTotalBytes)[port][priority][qIndex] -= wcacheSize;
    }
    else if (type == PacketReadWCache)
    {
        (*qTotalBytes)[port][priority][qIndex] -= dramSize;
    }

    *stream->GetStream() << now.GetSeconds() << ","
                         << port << ","
                         << priority << ","
                         << qIndex << ","
                         << (*qTotalBytes)[port][priority][qIndex]
                         << std::endl;
}

inline void
TraceSramBufferUsage(Ptr<OutputStreamWrapper> stream,
                     std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                     RwType type, Ptr<const Packet> packet)
{
    Time now = Simulator::Now();
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t psize = packet->GetSize();

    if (type == PacketWrite)
    {
        (*qTotalBytes)[port][priority][qIndex] += psize + 22;
    }
    else if (type == PacketRead)
    {
        (*qTotalBytes)[port][priority][qIndex] -= psize;
    }

    *stream->GetStream() << now.GetSeconds() << ","
                         << port << ","
                         << priority << ","
                         << qIndex << ","
                         << (*qTotalBytes)[port][priority][qIndex]
                         << std::endl;
}

inline void
TraceHbmBufferUsage(Ptr<OutputStreamWrapper> stream,
                    std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                    RwType type, Ptr<const Packet> packet)
{
    Time now = Simulator::Now();
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t opsize = packet->GetDramStoredSize();

    if (type == PacketWrite)
    {
        (*qTotalBytes)[port][priority][qIndex] += opsize;
    }
    else if (type == PacketRead)
    {
        (*qTotalBytes)[port][priority][qIndex] -= opsize;
    }

    *stream->GetStream() << now.GetSeconds() << ","
                         << port << ","
                         << priority << ","
                         << qIndex << ","
                         << (*qTotalBytes)[port][priority][qIndex]
                         << std::endl;
}

// Note that an event function has 6 arguments at most
inline void
TraceThroughput(Ptr<OutputStreamWrapper> stream,
                   uint64_t* pReadBytes,
                   uint64_t* pWriteBytes,
                   uint64_t prevReadBytes,
                   uint64_t prevWriteBytes,
                   Time measureWindow)
{
    if (*pReadBytes > prevReadBytes || *pWriteBytes > prevWriteBytes)
    {
        Time now = Simulator::Now();
        uint64_t readRate = (*pReadBytes - prevReadBytes) * 8L / measureWindow.GetSeconds();
        uint64_t writeRate = (*pWriteBytes - prevWriteBytes) * 8L / measureWindow.GetSeconds();
        *stream->GetStream() << (now - measureWindow).GetSeconds() << "," << now.GetSeconds() << ","
                             << readRate << "," << writeRate << std::endl;
    }
    Simulator::Schedule(measureWindow,
                        &TraceThroughput,
                        stream,
                        pReadBytes,
                        pWriteBytes,
                        *pReadBytes,
                        *pWriteBytes,
                        measureWindow);
}

// Note that an event function has 6 arguments at most
inline void
TraceQueueBufferThroughput(Ptr<OutputStreamWrapper> stream,
                           uint64_t* pBytes,
                           uint64_t prevBytes,
                           uint32_t pri,
                           uint32_t queue,
                           Time measureWindow)
{
    if (*pBytes > prevBytes)
    {
        Time now = Simulator::Now();
        uint64_t Rate = (*pBytes - prevBytes) * 8L / measureWindow.GetSeconds();
        *stream->GetStream() << (now - measureWindow).GetSeconds() << "," << now.GetSeconds() << ","
                             << pri << "," << queue << ","
                             << Rate << std::endl;
    }
    Simulator::Schedule(measureWindow,
                        &TraceQueueBufferThroughput,
                        stream,
                        pBytes,
                        *pBytes,
                        pri,
                        queue,
                        measureWindow);
}

inline void
TraceMmuDecision(Ptr<OutputStreamWrapper> stream,
                 Ptr<SwitchMmu> mmu,
                 Ptr<const Packet> packet,
                 uint32_t result)
{
    Time now = Simulator::Now();
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t sqId = packet->GetMmuUsedSqId();
    uint64_t qTotalRcvd = mmu->GetQueueTotalReceived(port, priority, qIndex);
    uint64_t qMaxUsed = mmu->GetQueueMaxUsedBuffer(port, priority, qIndex);
    *stream->GetStream() << now.GetSeconds() << "," << port << "," << sqId << "," << priority << "," << qIndex
                         << "," << result << "," << qTotalRcvd << "," << qMaxUsed << std::endl;
}

inline void
TracePortTxComplete(std::vector<uint64_t>* pTotalBytes, Ptr<const Packet> packet)
{
    uint32_t psize = packet->GetSize();
    uint32_t port = packet->GetMmuUsedPort();
    (*pTotalBytes)[port] += psize;
}

inline void
TraceQueueTxComplete(std::vector<std::vector<std::vector<uint64_t>>>* qTotalBytes,
                     Ptr<const Packet> packet)
{
    uint32_t psize = packet->GetSize();
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    (*qTotalBytes)[port][priority][qIndex] += psize;
}

inline void
TraceSqTxComplete(std::vector<std::vector<uint64_t>>* qTotalBytes,
                  Ptr<const Packet> packet)
{
    uint32_t psize = packet->GetSize();
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t sq = packet->GetMmuUsedSqId();
    (*qTotalBytes)[port][sq] += psize;
}

inline void
TracePortThroughput(Ptr<OutputStreamWrapper> stream, uint64_t* pTotalBytes, Time measureWindow)
{
    if (*pTotalBytes > 0)
    {
        Time now = Simulator::Now();
        uint64_t sendRate = *pTotalBytes * 8L / measureWindow.GetSeconds();
        *stream->GetStream() << (now - measureWindow).GetSeconds() << "," << now.GetSeconds() << ","
                             << sendRate << std::endl;
    }
    Simulator::Schedule(measureWindow, &TracePortThroughput, stream, pTotalBytes, measureWindow);
    *pTotalBytes = 0;
}

inline void
TraceSqThroughput(Ptr<OutputStreamWrapper> stream, uint64_t* pTotalBytes, Time measureWindow)
{
    if (*pTotalBytes > 0)
    {
        Time now = Simulator::Now();
        uint64_t sendRate = *pTotalBytes * 8L / measureWindow.GetSeconds();
        *stream->GetStream() << (now - measureWindow).GetSeconds() << "," << now.GetSeconds() << ","
                             << sendRate << std::endl;
    }
    Simulator::Schedule(measureWindow, &TraceSqThroughput, stream, pTotalBytes, measureWindow);
    *pTotalBytes = 0;
}

inline void
TraceQueueThroughput(Ptr<OutputStreamWrapper> stream, uint64_t* qTotalBytes, Time measureWindow)
{
    if (*qTotalBytes > 0)
    {
        Time now = Simulator::Now();
        uint64_t sendRate = *qTotalBytes * 8L / measureWindow.GetSeconds();
        *stream->GetStream() << (now - measureWindow).GetSeconds() << "," << now.GetSeconds() << ","
                             << sendRate << std::endl;
    }
    Simulator::Schedule(measureWindow, &TraceQueueThroughput, stream, qTotalBytes, measureWindow);
    *qTotalBytes = 0;
}


inline void
TraceHostRx(uint64_t* totalBytes, Ptr<const Packet> packet)
{
    *totalBytes += packet->GetSize();
}
} // namespace ns3

#endif
