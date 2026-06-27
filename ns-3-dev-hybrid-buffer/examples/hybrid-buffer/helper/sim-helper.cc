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

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(ccType));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(TCPPAYLOAD_BYTES));
    // Disable Delayed ACK
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0)); //default 0
    // Disable Nagle's algorithm
    Config::SetDefault("ns3::TcpSocket::TcpNoDelay", BooleanValue(true));


    // TCP RTO timeout when opening connection
    Config::SetDefault("ns3::TcpSocket::ConnTimeout", TimeValue(MilliSeconds(5))); //default 5
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(5))); //default 5
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1000)); //wk  default 10
    // Clock Granularity used in RTO calculations
    Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MicroSeconds(1)));
    Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", StringValue("400Gbps"));
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



static Time prevTime = Seconds (0);
static uint32_t prev = 0;

static void
TraceThroughput (Ptr<FlowMonitor> monitor)
{
  	FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
 	std::map<FlowId, FlowMonitor::FlowStats>::const_iterator itr = stats.begin();  //检测第一条流
 	//下边是最后一条流最后一条流
 	// std::map<FlowId, FlowMonitor::FlowStats>::const_iterator itr = stats.end(); itr--;

   	Time curTime = Now ();
    std::cout <<" debugwk-TraceThroughput: "<<  Simulator::Now().GetMilliSeconds() << " " << 8 * (itr->second.txBytes - prev) / (1000 * 1000 * (curTime.GetSeconds () - prevTime.GetSeconds ())) <<""<< std::endl;
    prevTime = curTime;
   	prev = itr->second.txBytes;
   	Simulator::Schedule (Seconds (80*1e-6), &TraceThroughput, monitor);
}


void
SimHelper::TraceFlows()
{
    NS_LOG_FUNCTION(this);
    Ptr<FlowMonitor> monitor = m_flowMonitorHelper.InstallAll();
    // Simulator::Schedule (Seconds (0 + 0.0001), &TraceThroughput, monitor);
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
    // PeriodicFlowStats(Seconds(100.0*1e-6), "flow-stats.csv"); // Print stats every 1 second to CSV file
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

// Add this new function to your SimHelper class
void
SimHelper::PeriodicFlowStats(Time interval, const std::string& filename)
{
    NS_LOG_FUNCTION(this << interval << filename);
    Ptr<FlowMonitor> monitor = m_flowMonitorHelper.GetMonitor();
    
    // Open the file in append mode (creates if doesn't exist)
    std::ofstream outFile;
    outFile.open(filename, std::ios_base::app);
    if (!outFile.is_open()) {
        NS_LOG_ERROR("Could not open file " << filename << " for writing flow stats");
        return;
    }
    outFile << "Simulation Time,Flow ID,Source IP,Source Port,Destination IP,Destination Port,Tx Packets,Rx Packets,Lost Packets,Throughput (kbps),Mean Delay (s)" << std::endl;
    outFile.close();
    
    // Create a recurring event to print flow stats
    Simulator::Schedule(interval, &SimHelper::PrintFlowStatsToFile, this, monitor, interval, filename);
}

void
SimHelper::PrintFlowStatsToFile(Ptr<FlowMonitor> monitor, Time interval, const std::string& filename)
{
    NS_LOG_FUNCTION(this << monitor << interval << filename);
    
    // Get flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(m_flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    // Open file in append mode
    std::ofstream outFile;
    outFile.open(filename, std::ios_base::app);
    if (!outFile.is_open()) {
        NS_LOG_ERROR("Could not open file " << filename << " for writing flow stats");
        return;
    }
    
    // Write statistics for each flow
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        
        double throughput = 0.0;
        double meanDelay = 0.0;
        
        if (it->second.rxPackets > 0) {
            throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket - it->second.timeFirstTxPacket).GetSeconds() / 1000;
            meanDelay = it->second.delaySum.GetSeconds() / it->second.rxPackets;
        }
        
        outFile << Simulator::Now().GetSeconds() << ","
                << it->first << ","
                << t.sourceAddress << ","
                << t.sourcePort << ","
                << t.destinationAddress << ","
                << t.destinationPort << ","
                << it->second.txPackets << ","
                << it->second.rxPackets << ","
                << it->second.lostPackets << ","
                << throughput << ","
                << meanDelay << std::endl;
    }
    
    outFile.close();
    
    // Schedule next print
    Simulator::Schedule(interval, &SimHelper::PrintFlowStatsToFile, this, monitor, interval, filename);
}

} // namespace hb

} // namespace ns3
