#include "sim-helper.h"
#include "ns3/flow-monitor-helper.h"

namespace ns3
{

namespace hb
{

NS_LOG_COMPONENT_DEFINE("SimHelper");

void
SimHelper::FlowInfo::Print(std::ostream& os) const
{
    os << srcId << "->" << dstId << ", from " << startTime.GetSeconds() << "s to "
       << stopTime.GetSeconds() << "s, rate=" << rate.GetBitRate() / 1000.0 / 1000.0 / 1000.0
       << "Gbps, flowsize=" << size / 1024.0 / 1024.0 << "MB";
}

SimHelper::SimHelper(std::string simName, Time start, Time stop)
{
    NS_LOG_FUNCTION(this << simName);

    m_mmuFactory.SetTypeId("ns3::SwitchMmu");
    m_offChipBufferFactory.SetTypeId("ns3::OffChipBuffer");

    m_startTime = start;
    m_stopTime = stop;
    m_simName = simName;
    m_reorderQueueSize = QueueSize("500KB");
}

SimHelper::~SimHelper()
{
    NS_LOG_FUNCTION(this);
}

DataRate
SimHelper::ConvertPhyRateToAppRate(DataRate phyRate, std::string socketType)
{
    NS_LOG_FUNCTION(phyRate << socketType);
    double payload = 0;
    double mtu = MTU_BYTES;
    if (socketType == "tcp")
    {
        payload = TCPPAYLOAD_BYTES;
    }
    else if (socketType == "udp")
    {
        payload = UDPPAYLOAD_BYTES;
    }
    else
    {
        payload = IPPAYLOAD_BYTES;
    }
    DataRate appRate = phyRate * (payload / mtu);
    return appRate;
}

DataRate
SimHelper::ConvertPhyRateToAppRate(DataRate phyRate)
{
    NS_LOG_FUNCTION(this << phyRate);
    return ConvertPhyRateToAppRate(phyRate, m_socketType);
}

void
SimHelper::Initialize()
{
    NS_LOG_FUNCTION(this);
    ConfigTopology();
    ConfigTransport();
    ConfigRoute();
    ConfigTraffic();
}

void
SimHelper::ConfigTransport(std::string socketType, std::string ccType)
{
    NS_LOG_FUNCTION(this << socketType << ccType);
    if (socketType == "tcp" || socketType == "udp")
    {
        m_socketType = socketType;
    }
    else
    {
        std::cerr << "unsupport socket type (supported: tcp, udp): " << socketType << std::endl;
    }

    m_ccType = ccType;
    std::cout << "[TCP-CONFIG] CongestionControl=" << ccType << std::endl;  //TCP使用  --sj
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(ccType));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(TCPPAYLOAD_BYTES));
    // Disable Delayed ACK
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0)); //default 0
    // Disable Nagle's algorithm
    Config::SetDefault("ns3::TcpSocket::TcpNoDelay", BooleanValue(true));


    // TCP RTO timeout when opening connection
    Config::SetDefault("ns3::TcpSocket::ConnTimeout", TimeValue(MilliSeconds(5))); //default 5
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(5))); //default 5
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(7)); //wk  default 10   100Gbps*32us = 400KB   这里的单位是包数，不是字节数  1000KB=667pkts
    Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(7)); //wk  default MaxUint32     这里的单位是包数，不是字节数
    // Clock Granularity used in RTO calculations
    Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MicroSeconds(1)));
    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", StringValue("100Gbps")); // default 400Gbps
    // Config::SetDefault("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(100)));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(160L << 20)); // wk  default GetMaxCwnd()
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(160L << 20));
}

// void
// SimHelper::SetOffChipBufferAttribute(std::string name, const AttributeValue& value)
// {
//     NS_LOG_FUNCTION(this << name << &value);
//     if (name == "")
//     {
//         return;
//     }
//
//     TypeId tid = TypeId::LookupByName("ns3::OffChipBuffer");
//     struct TypeId::AttributeInformation info;
//     if (!tid.LookupAttributeByName(name, &info))
//     {
//         NS_FATAL_ERROR("Invalid attribute set (" << name << ") on " << tid.GetName());
//         return;
//     }
//     Ptr<AttributeValue> v = info.checker->CreateValidValue(value);
//     if (!v)
//     {
//         NS_FATAL_ERROR("Invalid value for attribute set (" << name << ") on " << tid.GetName());
//         return;
//     }
//     m_offChipAttributes.Add(name, info.checker, value.Copy());
// }

void
SimHelper::PopulateRoutingTables()
{
    NS_LOG_FUNCTION(this);
    // setup ip routing tables to get total ip-level connectivity.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void
SimHelper::SetupTracing()
{
    NS_LOG_FUNCTION(this);
    Config::SetDefault("ns3::PcapFileWrapper::NanosecMode", BooleanValue(true));
    TraceSocket();
    TraceMmu();
    TraceOffChipBuffer();
    TraceFlows();
}

void
SimHelper::TraceFlows()
{
    NS_LOG_FUNCTION(this);
    m_flowMonitorHelper.InstallAll();
}

void
SimHelper::Setup()
{
    NS_LOG_FUNCTION(this);
    CreateTopology();
    SetupMmu();
    SetupOffChipBuffer();
    SetupHostQueueDisc();
    SetupRouterQueueDisc();
    SetupRouterPacketFilter();
    PopulateRoutingTables();
    CreateTraffic();
    SetupTracing();
}

void
SimHelper::Run()
{
    NS_LOG_FUNCTION(this);
    Setup();
    Simulator::Stop(m_stopTime);
    std::cout << "Run Simulation." << std::endl;
    Simulator::Run();
    std::cout << "Simulation Done" << std::endl;
    Simulator::Destroy();
    Finish();
}

void
SimHelper::Finish()
{
    NS_LOG_FUNCTION(this);
    std::stringstream flowMonitorFname;
    flowMonitorFname << "flow-monitor-" << m_simName << ".xml";
    m_flowMonitorHelper.GetMonitor()->SerializeToXmlFile(flowMonitorFname.str(), false, false);
}

} // namespace hb

} // namespace ns3
