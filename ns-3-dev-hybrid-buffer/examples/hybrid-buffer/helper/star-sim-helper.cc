#include "star-sim-helper.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

namespace ns3
{

namespace hb
{

NS_LOG_COMPONENT_DEFINE("StarSimHelper");

StarSimHelper::StarSimHelper(std::string simName, Time start, Time stop)
    : SimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
    m_mmu = nullptr;
    m_offChipBuffer = nullptr;
    m_routerFifoQdiscFactory.SetTypeId("ns3::FifoQueueDisc");
    // Set The FIFO leaf Max Size to be 100,000 packet to avoid unnecessary
    // scheduler drops.
    m_routerFifoQdiscFactory.Set("MaxSize", QueueSizeValue(QueueSize("100000000p")));

    m_routerRedQdisFatory.SetTypeId("ns3::RedQueueDisc");
    m_routerRedQdisFatory.Set("MaxSize", QueueSizeValue(QueueSize("100000000p")));
    // m_routerRedQdisFatory.Set("MaxSize", QueueSizeValue(QueueSize("100000000p")));  // 最大 1000 包
    // m_routerRedQdisFatory.Set("UseEcn", BooleanValue(true));      // 启用 ECN
    // m_routerRedQdisFatory.Set("MinTh", DoubleValue(480));         // ECN 标记阈值（包数）
    // m_routerRedQdisFatory.Set("MaxTh", DoubleValue(480));         // 固定阈值（DCTCP 风格）
    // m_routerRedQdisFatory.Set("QW", DoubleValue(1.0));           // 队列权重

    m_routerRedQdisFatory.SetTypeId("ns3::RedQueueDisc");
    m_routerRedQdisFatory.Set("UseEcn", BooleanValue(true));
    m_routerRedQdisFatory.Set("UseHardDrop", BooleanValue(false));
    m_routerRedQdisFatory.Set("MeanPktSize", UintegerValue(1500));
    m_routerRedQdisFatory.Set("MaxSize", QueueSizeValue(QueueSize("100000000p")));
    m_routerRedQdisFatory.Set("QW", DoubleValue(1));
    m_routerRedQdisFatory.Set("MinTh", DoubleValue(480));
    m_routerRedQdisFatory.Set("MaxTh", DoubleValue(480));
    m_routerRedQdisFatory.Set("LinkBandwidth", StringValue("100Gbps"));
    m_routerRedQdisFatory.Set("LinkDelay", StringValue("1us"));
    
    // Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    // Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    // Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(1500));
    // Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("100000000p")));
    // // DCTCP tracks instantaneous queue length only; so set QW = 1
    // Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(1));
    // Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(480));
    // Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(480));
    // Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue("100Gbps"));
    // Config::SetDefault("ns3::RedQueueDisc::LinkDelay", StringValue("10us"));
    


    m_enableHbmThroughputTracing = false;
    m_enableWCacheThroughputTracing = false;
    m_enableSramThroughputTracing = false;
    m_enableBufferUsageTracing = false;
    m_enableBmResultTracing = false;
    m_enablePortThroughputTracing = false;
    m_enableQueueThroughputTracing = false;
    m_enableSqThroughputTracing = false;
    m_enableQueueWCacheTracing = false;
    m_enableQueueSramTracing = false;
    m_enableQueueHbmTracing = false;
    m_hbmThroughputMeasureWindow = MicroSeconds(1);
    m_hbmReadBytes = 0;
    m_hbmWriteBytes = 0;

    Initialize();
}

StarSimHelper::~StarSimHelper()
{
    NS_LOG_FUNCTION(this);
}

void
StarSimHelper::ConfigTopology(uint32_t numSpokes, DataRate linkCapacity, Time linkDelay)
{
    NS_LOG_FUNCTION(this << numSpokes << linkCapacity << linkDelay);
    ConfigTopology(numSpokes, 1, linkCapacity, linkDelay, linkCapacity, linkDelay);
}

void
StarSimHelper::ConfigTopology(uint32_t numSpokes,
                              uint32_t numReceivers,
                              DataRate recvLinkCapacity,
                              Time recvLinkDelay,
                              DataRate sendLinkCapacity,
                              Time sendLinkDelay)
{
    NS_LOG_FUNCTION(this << numSpokes << numReceivers << recvLinkCapacity << recvLinkDelay
                         << sendLinkCapacity << sendLinkDelay);
    NS_ASSERT(numReceivers > 0);
    NS_ASSERT(numSpokes > numReceivers);
    m_nReceivers = numReceivers;
    m_nSpokes = numSpokes;
    m_recvLinkCapacity = recvLinkCapacity;
    m_recvLinkDelay = recvLinkDelay;
    m_sendLinkCapacity = sendLinkCapacity;
    m_sendLinkDelay = sendLinkDelay;
}

void
StarSimHelper::ConfigTraffic(uint64_t flowSize)
{
    NS_LOG_FUNCTION(this << flowSize);
    m_flowSize = flowSize;
}

void
StarSimHelper::CreateTopology()
{
    NS_LOG_FUNCTION(this);
    m_hub = CreateObject<Node>();
    m_spokes.Create(m_nSpokes);

    PointToPointReorderHelper sendP2pHelper;
    sendP2pHelper.SetDeviceAttribute("DataRate", DataRateValue(m_sendLinkCapacity));
    sendP2pHelper.SetChannelAttribute("Delay", TimeValue(m_sendLinkDelay));
    sendP2pHelper.SetQueueA("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("2p"))); //QueueSizeValue(m_reorderQueueSize));   //QueueSize("2p")
    sendP2pHelper.SetQueueB("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("2p")));

    PointToPointReorderHelper recvP2pHelper;
    recvP2pHelper.SetDeviceAttribute("DataRate", DataRateValue(m_recvLinkCapacity));
    recvP2pHelper.SetChannelAttribute("Delay", TimeValue(m_recvLinkDelay));
    recvP2pHelper.SetQueueA("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("2p"))); //QueueSizeValue(m_reorderQueueSize));
    recvP2pHelper.SetQueueB("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("2p")));

    NS_LOG_LOGIC("Install links");
    for (uint32_t i = 0; i < m_nReceivers; ++i)
    {
        NetDeviceContainer nd = recvP2pHelper.Install(m_hub, m_spokes.Get(i), false);
        m_hubDevices.Add(nd.Get(0));
        m_spokeDevices.Add(nd.Get(1));
    }

    for (uint32_t i = m_nReceivers; i < m_nSpokes; ++i)
    {
        NetDeviceContainer nd = sendP2pHelper.Install(m_hub, m_spokes.Get(i), false);
        m_hubDevices.Add(nd.Get(0));
        m_spokeDevices.Add(nd.Get(1));
    }

    NS_LOG_LOGIC("Install Internet stack");
    InternetStackHelper stackHelper;
    stackHelper.Install(m_hub);
    stackHelper.Install(m_spokes);

    NS_LOG_LOGIC("Assign addresses");
    Ipv4AddressHelper addrHelper("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < m_spokes.GetN(); ++i)
    {
        m_hubInterfaces.Add(addrHelper.Assign(m_hubDevices.Get(i)));
        m_spokeInterfaces.Add(addrHelper.Assign(m_spokeDevices.Get(i)));
        addrHelper.NewNetwork();
    }
}

void
StarSimHelper::SetupMmu()
{
    NS_LOG_FUNCTION(this);
    m_mmu = m_mmuFactory.Create<SwitchMmu>();
    m_hub->AggregateObject(m_mmu->GetObject<Object>());
    for (uint32_t i = 0; i < m_hubDevices.GetN(); i++)
    {
        Ptr<PointToPointReorderNetDevice> netdev =
            m_hubDevices.Get(i)->GetObject<PointToPointReorderNetDevice>();
        netdev->SetMmu(m_mmu);
        netdev->NotifyLinkUp();
        Ptr<NetDeviceQueueInterface> ndqi = netdev->GetObject<NetDeviceQueueInterface>();
        ndqi->SetMmu(m_mmu);
    }

    uint32_t nPorts = m_mmu->GetNPorts();
    uint32_t nPriority = m_mmu->GetNPriorities();
    uint32_t nQueue = m_mmu->GetNQueues();
    m_portSendBytes.resize(nPorts, 0);
    m_queueSendBytes.resize(nPorts);

    /// Queue WCache Initialize
    m_qWCacheReadBytes.resize(nPorts);
    m_qWCacheWriteBytes.resize(nPorts);
    m_qWCacheLength.resize(nPorts);

    /// Queue Sram Initialize
    m_qSramWriteBytes.resize(nPorts);
    m_qSramReadBytes.resize(nPorts);
    m_qSramLength.resize(nPorts);

    /// Queue Hbm Initialize
    m_qHbmWriteBytes.resize(nPorts);
    m_qHbmReadBytes.resize(nPorts);
    m_qHbmLength.resize(nPorts);
    for (uint32_t i = 0; i < nPorts; i++)
    {
        m_queueSendBytes[i].resize(nPriority);
        m_qWCacheReadBytes[i].resize(nPriority);
        m_qWCacheWriteBytes[i].resize(nPriority);
        m_qWCacheLength[i].resize(nPriority);
        /// Queue Sram Initialize
        m_qSramWriteBytes[i].resize(nPriority);
        m_qSramReadBytes[i].resize(nPriority);
        m_qSramLength[i].resize(nPriority);

        /// Queue Hbm Initialize
        m_qHbmWriteBytes[i].resize(nPriority);
        m_qHbmReadBytes[i].resize(nPriority);
        m_qHbmLength[i].resize(nPriority);
        for (uint32_t j = 0; j < nPriority; j++)
        {
            m_queueSendBytes[i][j].resize(nQueue, 0);
            m_qWCacheReadBytes[i][j].resize(nQueue, 0);
            m_qWCacheWriteBytes[i][j].resize(nQueue, 0);
            m_qWCacheLength[i][j].resize(nQueue, 0);

            /// Queue Sram Initialize
            m_qSramWriteBytes[i][j].resize(nQueue, 0);
            m_qSramReadBytes[i][j].resize(nQueue, 0);
            m_qSramLength[i][j].resize(nQueue, 0);

            /// Queue Hbm Initialize
            m_qHbmWriteBytes[i][j].resize(nQueue, 0);
            m_qHbmReadBytes[i][j].resize(nQueue, 0);
            m_qHbmLength[i][j].resize(nQueue, 0);
        }
    }

    // Sq Throughput, we donot log the Sq num.
    m_sqSendBytes.resize(nPorts);
    for (uint32_t i = 0; i < nPorts; i++)
    {
        m_sqSendBytes[i].resize(20, 0);
    }
}

void
StarSimHelper::SetupOffChipBuffer()
{
    NS_LOG_FUNCTION(this);
    m_offChipBuffer = m_offChipBufferFactory.Create<OffChipBuffer>();
    m_mmu->AttachOffChipBuffer(m_offChipBuffer);
}

void
StarSimHelper::SetupHostQueueDisc()
{
    NS_LOG_FUNCTION(this);
    TrafficControlHelper tcHelper;
    // Never drop packets inside the host queue
    tcHelper.SetRootQueueDisc("ns3::FifoQueueDisc",
                              "MaxSize",
                              QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, 160L << 20))); //modify wk  default  3 * GetMaxCwnd())));

    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        NS_LOG_LOGIC("Install traffic control on host " << i);

        // Delete the existing root qdisc
        // The root queue qidsc was installed when assigning IP addresses
        Ptr<Node> node = m_spokes.Get(i);
        Ptr<NetDevice> dev = node->GetDevice(0);
        Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
        tc->DeleteRootQueueDiscOnDevice(dev);

        tcHelper.Install(dev);
        // tcHelper.Uninstall(dev);
        // tcHelper.Uninstall(node);
    }
}

void
StarSimHelper::SetupRouterQueueDisc()
{
    NS_LOG_FUNCTION(this);
    // 删除现有的根队列规则
    
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        Ptr<NetDevice> dev = m_hubDevices.Get(i);
        tc->DeleteRootQueueDiscOnDevice(dev);
    }

    // 1. 设置根队列为 PrioQueueDisc（优先级队列）
    TrafficControlHelper tcHelper;
    tcHelper.SetRootQueueDisc("ns3::PrioQueueDisc");
    QueueDiscContainer rootQdiscs = tcHelper.Install(m_hubDevices);

    // 2. 为每个输出端口安装队列规则
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        Ptr<PrioQueueDisc> rootQdisc = rootQdiscs.Get(i)->GetObject<PrioQueueDisc>();
        rootQdisc->SetLayerId(QueueDisc::priority);

        // --- 高优先级分支（HP）---
        {
            Ptr<PrioQueueDisc> hpQdisc = CreateObject<PrioQueueDisc>();
            hpQdisc->SetLayerId(QueueDisc::queue);
            Ptr<QueueDiscClass> hpCls = CreateObject<QueueDiscClass>();
            hpCls->SetQueueDisc(hpQdisc);
            rootQdisc->AddQueueDiscClass(hpCls);

            // 子队列：3 个 RedQueueDisc（替换原有的 FifoQueueDisc）
            for (uint32_t cs = 0; cs < 3; cs++)
            {
                // 创建TCP
                // 创建 RedQueueDisc 并配置 ECN
                Ptr<RedQueueDisc> redQdisc = m_routerRedQdisFatory.Create<RedQueueDisc>();
                if (!redQdisc) {
                    std::cout<<"Failed to create RedQueueDisc!"<< std::endl;
                }
                // std::cout<<"RedQueueDisc created: " << redQdisc->GetInstanceTypeId()<< std::endl;

                Ptr<QueueDiscClass> leafCls = CreateObject<QueueDiscClass>();
                leafCls->SetQueueDisc(redQdisc);
                hpQdisc->AddQueueDiscClass(leafCls);

                //创建UDP
                // Ptr<FifoQueueDisc> leafQdisc = m_routerFifoQdiscFactory.Create<FifoQueueDisc>();
                //     Ptr<QueueDiscClass> leafCls = CreateObject<QueueDiscClass>();
                //     leafCls->SetQueueDisc(leafQdisc);
                //     hpQdisc->AddQueueDiscClass(leafCls);

            }
        }

        // --- 低优先级分支（LP）---
        {
            Ptr<DrrQueueDisc> lpQdisc = CreateObject<DrrQueueDisc>();
            lpQdisc->SetLayerId(QueueDisc::queue);
            Ptr<QueueDiscClass> lpCls = CreateObject<QueueDiscClass>();
            lpCls->SetQueueDisc(lpQdisc);
            rootQdisc->AddQueueDiscClass(lpCls);

            // 子队列：5 个 RedQueueDisc（替换原有的 FifoQueueDisc）
            uint32_t quantums[5] = {2, 2, 1, 1, 1};
            for (uint32_t cs = 0; cs < 5; cs++)
            {
                //创建TCP
                Ptr<RedQueueDisc> redQdisc = m_routerRedQdisFatory.Create<RedQueueDisc>();

                Ptr<DrrFlow> leafCls = CreateObject<DrrFlow>();
                leafCls->SetQuantum(quantums[cs]);
                leafCls->SetQueueDisc(redQdisc);
                lpQdisc->AddQueueDiscClass(leafCls);
                //创建UDP
                // Ptr<FifoQueueDisc> fifoQdisc = m_routerFifoQdiscFactory.Create<FifoQueueDisc>();
                // Ptr<DrrFlow> leafCls = CreateObject<DrrFlow>();
                // leafCls->SetQuantum(quantums[cs]);
                // leafCls->SetQueueDisc(fifoQdisc);
                // lpQdisc->AddQueueDiscClass(leafCls);
                
            }
        }
    }
}

void
StarSimHelper::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: All flows goto hp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        if(m_socketType == "udp"){
            rootFilter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                    Ipv4Address::GetAny(),
                                    Ipv4Address::GetAny(),
                                    Ipv4Mask::GetZero(),
                                    Ipv4Mask::GetZero(),
                                    0,
                                    0xffff,
                                    0,
                                    0xffff,
                                    0);
        }else{
            rootFilter->AddClassifyRule(TcpL4Protocol::PROT_NUMBER,
                                    Ipv4Address::GetAny(),
                                    Ipv4Address::GetAny(),
                                    Ipv4Mask::GetZero(),
                                    Ipv4Mask::GetZero(),
                                    0,
                                    0xffff,
                                    0,
                                    0xffff,
                                    0);
        }
        
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 classify rule: all flows go to queue 0
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            if(m_socketType == "udp"){
                l2Filter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                      Ipv4Address::GetAny(),
                                      Ipv4Address::GetAny(),
                                      Ipv4Mask::GetZero(),
                                      Ipv4Mask::GetZero(),
                                      0,
                                      0xffff,
                                      0,
                                      0xffff,
                                      0);
            }else{
                l2Filter->AddClassifyRule(TcpL4Protocol::PROT_NUMBER,
                                      Ipv4Address::GetAny(),
                                      Ipv4Address::GetAny(),
                                      Ipv4Mask::GetZero(),
                                      Ipv4Mask::GetZero(),
                                      0,
                                      0xffff,
                                      0,
                                      0xffff,
                                      0);
            }
            l2Qdisc->AddPacketFilter(l2Filter);
        }
    }
}

void
StarSimHelper::SetupDefaultTraffic()
{
    NS_LOG_FUNCTION(this);
    // The first m_nReceivers nodes are receivers and the others are senders
    for (uint32_t sendid = m_nReceivers; sendid < m_nSpokes; sendid++)
    {
        uint32_t recvid = (sendid - m_nReceivers) % m_nReceivers;
        AddFlow(sendid, recvid, m_startTime, m_stopTime, 0, m_flowSize);
    }
}

void
StarSimHelper::CreateTraffic()
{
    NS_LOG_FUNCTION(this);
    uint16_t baseListenPort = 1000;
    if (m_flows.size() == 0) // Set up default flow when no flow is added
    {
        SetupDefaultTraffic();
    }

    for (auto& flow : m_flows)
    {
        std::cout<<"debugwk flow "<<flow.srcId<<" "<<flow.dstId<<" "<<flow.size<<" "<<flow.pktSize<<" "<<flow.rate<<" "<<flow.startTime<<" "<<flow.stopTime <<" "<<m_socketType<<std::endl;

        NS_LOG_LOGIC(flow);
        uint16_t listenPort = baseListenPort + m_spokes.Get(flow.dstId)->GetNApplications();
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), listenPort));
        ApplicationContainer sinkApp;

        AddressValue recvAddress(
            InetSocketAddress(m_spokeInterfaces.GetAddress(flow.dstId), listenPort));
        ApplicationContainer sourceApp;
        if (m_socketType == "tcp")
        {
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            sinkApp = sinkHelper.Install(m_spokes.Get(flow.dstId));
            uint32_t payload = flow.pktSize ? flow.pktSize : TCPPAYLOAD_BYTES;
            
            BulkSendHelper tcpSendHelper("ns3::TcpSocketFactory", Address());
            tcpSendHelper.SetAttribute("Remote", recvAddress);
            tcpSendHelper.SetAttribute("SendSize", UintegerValue(payload));
            tcpSendHelper.SetAttribute("MaxBytes", UintegerValue(flow.size));
            sourceApp = tcpSendHelper.Install(m_spokes.Get(flow.srcId));
        }
        else
        {
            PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
            sinkApp = sinkHelper.Install(m_spokes.Get(flow.dstId));
            int32_t payload = flow.pktSize ? flow.pktSize : UDPPAYLOAD_BYTES;

            OnOffHelper udpSendHelper("ns3::UdpSocketFactory", Address());
            udpSendHelper.SetAttribute("Remote", recvAddress);
            udpSendHelper.SetAttribute("MaxBytes", UintegerValue(flow.size));
            udpSendHelper.SetConstantRate(ConvertPhyRateToAppRate(flow.rate), payload);
            udpSendHelper.SetAttribute("OnTime", StringValue(flow.onTime));
            udpSendHelper.SetAttribute("OffTime", StringValue(flow.offTime));
            sourceApp = udpSendHelper.Install(m_spokes.Get(flow.srcId));
        }

        sinkApp.Start(flow.startTime);
        sinkApp.Stop(flow.stopTime);
        sourceApp.Start(flow.startTime);
        sourceApp.Stop(flow.stopTime);
        m_sinkApps.push_back(sinkApp);
        m_sourceApps.push_back(sourceApp);
    }
}

void
StarSimHelper::TraceSocket()
{
    NS_LOG_FUNCTION(this);
    if (m_enablePortThroughputTracing)
    {
        for (uint32_t pid = 0; pid < m_hubDevices.GetN(); pid++)
        {
            AsciiTraceHelper ascii;
            std::stringstream traceFname;
            traceFname << "port-throughput-" << m_simName << "-p" << pid << ".csv";
            Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
            *stream->GetStream() << "start,end,sendRate" << std::endl;

            Ptr<NetDevice> dev = m_hubDevices.Get(pid);
            Ptr<PointToPointReorderNetDevice> reorder =
                StaticCast<PointToPointReorderNetDevice>(dev);
            reorder->TraceConnectWithoutContext(
                "ReorderPhyTxEnd",
                MakeBoundCallback(&TracePortTxComplete, &m_portSendBytes));

            Simulator::Schedule(m_startTime + m_portThroughputMeasureWindow,
                                &TracePortThroughput,
                                stream,
                                &m_portSendBytes[pid],
                                m_portThroughputMeasureWindow);
        }
    }

    if (m_enableQueueThroughputTracing)
    {
        for (uint32_t port = 0; port < m_hubDevices.GetN(); port++)
        {
            Ptr<NetDevice> dev = m_hubDevices.Get(port);
            Ptr<PointToPointReorderNetDevice> reorder =
                StaticCast<PointToPointReorderNetDevice>(dev);
            reorder->TraceConnectWithoutContext(
                "ReorderPhyTxEnd",
                MakeBoundCallback(&TraceQueueTxComplete, &m_queueSendBytes));

            for (uint32_t pri = 0; pri < m_mmu->GetNPriorities(); pri++)
            {
                for (uint32_t qId = 0; qId < m_mmu->GetNQueues(); qId++)
                {
                    AsciiTraceHelper ascii;
                    std::stringstream traceFname;
                    traceFname << "queue-throughput-" << m_simName << "-p" << port << "-pri" << pri
                               << "-q" << qId << ".csv";
                    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
                    *stream->GetStream() << "start,end,sendRate" << std::endl;

                    Simulator::Schedule(m_startTime + m_queueThroughputMeasureWindow,
                                        &TraceQueueThroughput,
                                        stream,
                                        &m_queueSendBytes[port][pri][qId],
                                        m_queueThroughputMeasureWindow);
                }
            }
        }
    }

    if (m_enableSqThroughputTracing)
    {
        for (uint32_t port = 0; port < m_hubDevices.GetN(); port++)
        {
            Ptr<NetDevice> dev = m_hubDevices.Get(port);
            Ptr<PointToPointReorderNetDevice> reorder =
                StaticCast<PointToPointReorderNetDevice>(dev);
            reorder->TraceConnectWithoutContext(
                "ReorderPhyTxEnd",
                MakeBoundCallback(&TraceSqTxComplete, &m_sqSendBytes));

            for (uint32_t sq = 0; sq < 20; sq++)
            {
                AsciiTraceHelper ascii;
                std::stringstream traceFname;
                traceFname << "sq-throughput-" << m_simName << "-p" << port << "-sq" << sq << ".csv";    
                Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
                *stream->GetStream() << "start,end,sendRate" << std::endl;

                Simulator::Schedule(m_startTime + m_sqThroughputMeasureWindow,
                                    &TraceSqThroughput,
                                    stream,
                                    &m_sqSendBytes[port][sq],
                                    m_sqThroughputMeasureWindow);
            }
        }
    }
}

void
StarSimHelper::TraceMmu()
{
    NS_LOG_FUNCTION(this);
    if (m_enableBufferUsageTracing)
    {
        AsciiTraceHelper ascii;
        std::stringstream traceFname;
        traceFname << "buffer-usage-" << m_simName << ".csv";
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
        *stream->GetStream() << "time,sram,wcache,dram" << std::endl;

        m_mmu->TraceConnectWithoutContext(
            "Store",
            MakeBoundCallback(&TraceBufferUsage, stream, m_mmu, PacketWrite));
        m_mmu->TraceConnectWithoutContext(
            "Fetch",
            MakeBoundCallback(&TraceBufferUsage, stream, m_mmu, PacketRead));

        Ptr<OffChipBuffer> offChipBuffer = m_mmu->GetOffChipBuffer();
        offChipBuffer->TraceConnectWithoutContext(
            "DramWriteComplete",
            MakeBoundCallback(&TraceBufferUsage, stream, m_mmu, PacketWrite));
    }

    if (m_enableBmResultTracing)
    {
        AsciiTraceHelper ascii;
        std::stringstream traceFname;
        traceFname << "bm-result-" << m_simName << ".csv";
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
        *stream->GetStream() << "time,port,sq,priority,qIndex,result,qTotalRcvd,qMaxUsed" << std::endl;

        m_mmu->TraceConnectWithoutContext("CheckAdmission",
                                          MakeBoundCallback(&TraceMmuDecision, stream, m_mmu));
    }
}

void
StarSimHelper::TraceOffChipBuffer()
{
    NS_LOG_FUNCTION(this);
    if (m_enableSramThroughputTracing) {
        AsciiTraceHelper ascii;
        m_sramReadBytes = 0;
        m_sramWriteBytes = 0;
        std::stringstream traceFname;
        traceFname << "sram-throughput-" << m_simName << ".csv";
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
        *stream->GetStream() << "start,end,read_rate,write_rate" << std::endl;

        m_mmu->TraceConnectWithoutContext(
            "SramReadComplete",
            MakeBoundCallback(&TraceSramRwComplete, &m_sramReadBytes, &m_qSramReadBytes, PacketRead));
        m_mmu->TraceConnectWithoutContext(
            "SramWriteComplete",
            MakeBoundCallback(&TraceSramRwComplete, &m_sramWriteBytes, &m_qSramWriteBytes, PacketWrite));
        Simulator::Schedule(m_startTime + m_sramThroughputMeasureWindow,
                            &TraceThroughput,
                            stream,
                            &m_sramReadBytes,
                            &m_sramWriteBytes,
                            0,
                            0,
                            m_sramThroughputMeasureWindow);

        if (m_enableQueueSramTracing)
        {
            // Usage
            AsciiTraceHelper ascii_usage;
            std::stringstream traceFname_usage;
            traceFname_usage << "queue-sram-usage-" << m_simName << ".csv";
            Ptr<OutputStreamWrapper> stream_usage = ascii_usage.CreateFileStream(traceFname_usage.str());
            *stream_usage->GetStream() << "time,port,pri,queue,sram" << std::endl;
            for (uint32_t port = 0; port < m_hubDevices.GetN(); port++)
            {
                // Queue Sram Throughput and Usage Log.
                // Throughput
                AsciiTraceHelper ascii_th_read;
                std::stringstream traceFnameth_read;
                traceFnameth_read << "queue-sram-read-throughput-" << m_simName << "-p" << port << ".csv";
                Ptr<OutputStreamWrapper> streamth_read = ascii_th_read.CreateFileStream(traceFnameth_read.str());
                *streamth_read->GetStream() << "start,end,pri,queue,read_rate" << std::endl;

                // Queue Sram Throughput and Usage Log.
                // Throughput
                AsciiTraceHelper ascii_th_write;
                std::stringstream traceFnameth_write;
                traceFnameth_write << "queue-sram-write-throughput-" << m_simName << "-p" << port << ".csv";
                Ptr<OutputStreamWrapper> streamth_write = ascii_th_write.CreateFileStream(traceFnameth_write.str());
                *streamth_write->GetStream() << "start,end,pri,queue,send_rate" << std::endl;
                for (uint32_t pri = 0; pri < m_mmu->GetNPriorities(); pri++)
                {
                    for (uint32_t qId = 0; qId < m_mmu->GetNQueues(); qId++)
                    {
                        Simulator::Schedule(m_startTime + m_queueSramMeasureWindow,
                                            &TraceQueueBufferThroughput,
                                            streamth_read,
                                            &m_qSramReadBytes[port][pri][qId],
                                            0,
                                            pri,
                                            qId,
                                            m_queueSramMeasureWindow);

                        Simulator::Schedule(m_startTime + m_queueSramMeasureWindow,
                                            &TraceQueueBufferThroughput,
                                            streamth_write,
                                            &m_qSramWriteBytes[port][pri][qId],
                                            0,
                                            pri,
                                            qId,
                                            m_queueSramMeasureWindow);
                    }
                }
            }
            m_mmu->TraceConnectWithoutContext(
                "SramReadComplete",
                MakeBoundCallback(&TraceSramBufferUsage, stream_usage, &m_qSramLength, PacketRead));
            m_mmu->TraceConnectWithoutContext(
                "SramWriteComplete",
                MakeBoundCallback(&TraceSramBufferUsage, stream_usage, &m_qSramLength, PacketWrite));
        }
    }

    // Note: for Wcache The Write throughput is the Bandwidth 
    // from Outside writing into WCache. But for Read Bandwidth,
    // it only to log the Port Wcache read Bandwidth, to compare
    // the read bandwidth.
    if (m_enableWCacheThroughputTracing)
    {
        AsciiTraceHelper ascii;
        m_wcacheReadBytes = 0;
        m_wcacheWriteBytes = 0;
        std::stringstream traceFname;
        traceFname << "wcache-throughput-" << m_simName << ".csv";
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
        *stream->GetStream() << "start,end,read_rate,send_rate" << std::endl;

        m_offChipBuffer->TraceConnectWithoutContext(
            "WCacheReadComplete",
            MakeBoundCallback(&TraceWCacheRwComplete, &m_wcacheReadBytes, &m_qWCacheReadBytes, PacketRead));
        m_offChipBuffer->TraceConnectWithoutContext(
            "WCacheWriteComplete",
            MakeBoundCallback(&TraceWCacheRwComplete, &m_wcacheWriteBytes, &m_qWCacheWriteBytes, PacketWrite));
        Simulator::Schedule(m_startTime + m_wcacheThroughputMeasureWindow,
                            &TraceThroughput,
                            stream,
                            &m_wcacheReadBytes,
                            &m_wcacheWriteBytes,
                            0,
                            0,
                            m_wcacheThroughputMeasureWindow);

        // Queue WCache Throughput and Usage Log.
        if (m_enableQueueWCacheTracing)
        {
            // Usage
            AsciiTraceHelper ascii_usage;
            std::stringstream traceFname_usage;
            traceFname_usage << "queue-wcache-usage-" << m_simName << ".csv";
            Ptr<OutputStreamWrapper> stream_usage = ascii_usage.CreateFileStream(traceFname_usage.str());
            *stream_usage->GetStream() << "time,port,pri,queue,wcache" << std::endl;
            for (uint32_t port = 0; port < m_hubDevices.GetN(); port++)
            {
                // Queue Sram Throughput and Usage Log.
                // Throughput
                AsciiTraceHelper ascii_th_read;
                std::stringstream traceFnameth_read;
                traceFnameth_read << "queue-wcache-read-throughput-" << m_simName << "-p" << port << ".csv";
                Ptr<OutputStreamWrapper> streamth_read = ascii_th_read.CreateFileStream(traceFnameth_read.str());
                *streamth_read->GetStream() << "start,end,pri,queue,read_rate" << std::endl;

                // Queue Sram Throughput and Usage Log.
                // Throughput
                AsciiTraceHelper ascii_th_write;
                std::stringstream traceFnameth_write;
                traceFnameth_write << "queue-wcache-write-throughput-" << m_simName << "-p" << port << ".csv";
                Ptr<OutputStreamWrapper> streamth_write = ascii_th_write.CreateFileStream(traceFnameth_write.str());
                *streamth_write->GetStream() << "start,end,pri,queue,send_rate" << std::endl;
                for (uint32_t pri = 0; pri < m_mmu->GetNPriorities(); pri++)
                {
                    for (uint32_t qId = 0; qId < m_mmu->GetNQueues(); qId++)
                    {
                        Simulator::Schedule(m_startTime + m_queueWCacheMeasureWindow,
                                            &TraceQueueBufferThroughput,
                                            streamth_read,
                                            &m_qWCacheReadBytes[port][pri][qId],
                                            0,
                                            pri,
                                            qId,
                                            m_queueWCacheMeasureWindow);

                        Simulator::Schedule(m_startTime + m_queueWCacheMeasureWindow,
                                            &TraceQueueBufferThroughput,
                                            streamth_write,
                                            &m_qWCacheWriteBytes[port][pri][qId],
                                            0,
                                            pri,
                                            qId,
                                            m_queueWCacheMeasureWindow);
                    }
                }
            }
            m_offChipBuffer->TraceConnectWithoutContext(
                "WCacheReadComplete",
                MakeBoundCallback(&TraceWCacheBufferUsage, stream_usage, &m_qWCacheLength, PacketRead));
            m_offChipBuffer->TraceConnectWithoutContext(
                "WCacheWriteComplete",
                MakeBoundCallback(&TraceWCacheBufferUsage, stream_usage, &m_qWCacheLength, PacketWrite));
            m_offChipBuffer->TraceConnectWithoutContext(
                "DramWriteComplete",
                MakeBoundCallback(&TraceWCacheBufferUsage, stream_usage, &m_qWCacheLength, PacketReadWCache));
        }
    }

    if (m_enableHbmThroughputTracing)
    {
        AsciiTraceHelper ascii;
        m_hbmReadBytes = 0;
        m_hbmWriteBytes = 0;
        std::stringstream traceFname;
        traceFname << "hbm-throughput-" << m_simName << ".csv";
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(traceFname.str());
        *stream->GetStream() << "start,end,read_rate,write_rate" << std::endl;

        m_offChipBuffer->TraceConnectWithoutContext(
            "DramReadComplete",
            MakeBoundCallback(&TraceOffChipBufferRwComplete, &m_hbmReadBytes, &m_qHbmReadBytes, PacketRead));
        m_offChipBuffer->TraceConnectWithoutContext(
            "DramWriteComplete",
            MakeBoundCallback(&TraceOffChipBufferRwComplete, &m_hbmWriteBytes, &m_qHbmWriteBytes, PacketWrite));
        Simulator::Schedule(m_startTime + m_hbmThroughputMeasureWindow,
                            &TraceThroughput,
                            stream,
                            &m_hbmReadBytes,
                            &m_hbmWriteBytes,
                            0,
                            0,
                            m_hbmThroughputMeasureWindow);

        // Queue Sram Throughput and Usage Log.
        if (m_enableQueueHbmTracing)
        {
            // Usage
            AsciiTraceHelper ascii_usage;
            std::stringstream traceFname_usage;
            traceFname_usage << "queue-hbm-usage-" << m_simName << ".csv";

            Ptr<OutputStreamWrapper> stream_usage = ascii_usage.CreateFileStream(traceFname_usage.str());
            *stream_usage->GetStream() << "time,port,pri,queue,hbm" << std::endl;
            for (uint32_t port = 0; port < m_hubDevices.GetN(); port++)
            {
                // Throughput
                AsciiTraceHelper ascii_th_read;
                std::stringstream traceFnameth_read;
                traceFnameth_read << "queue-hbm-read-throughput-" << m_simName << "-p" << port << ".csv";
                Ptr<OutputStreamWrapper> streamth_read = ascii_th_read.CreateFileStream(traceFnameth_read.str());
                *streamth_read->GetStream() << "start,end,pri,queue,read_rate" << std::endl;

                // Throughput
                AsciiTraceHelper ascii_th_write;
                std::stringstream traceFnameth_write;
                traceFnameth_write << "queue-hbm-write-throughput-" << m_simName << "-p" << port << ".csv";
                Ptr<OutputStreamWrapper> streamth_write = ascii_th_write.CreateFileStream(traceFnameth_write.str());
                *streamth_write->GetStream() << "start,end,pri,queue,send_rate" << std::endl;
                for (uint32_t pri = 0; pri < m_mmu->GetNPriorities(); pri++)
                {
                    for (uint32_t qId = 0; qId < m_mmu->GetNQueues(); qId++)
                    {
                        Simulator::Schedule(m_startTime + m_queueHbmMeasureWindow,
                                            &TraceQueueBufferThroughput,
                                            streamth_read,
                                            &m_qHbmReadBytes[port][pri][qId],
                                            0,
                                            pri,
                                            qId,
                                            m_queueHbmMeasureWindow);

                        Simulator::Schedule(m_startTime + m_queueHbmMeasureWindow,
                                            &TraceQueueBufferThroughput,
                                            streamth_write,
                                            &m_qHbmWriteBytes[port][pri][qId],
                                            0,
                                            pri,
                                            qId,
                                            m_queueHbmMeasureWindow);
                    }
                }
            }
            m_offChipBuffer->TraceConnectWithoutContext(
                "DramReadComplete",
                MakeBoundCallback(&TraceHbmBufferUsage, stream_usage, &m_qHbmLength, PacketRead));
            m_offChipBuffer->TraceConnectWithoutContext(
                "DramWriteComplete",
                MakeBoundCallback(&TraceHbmBufferUsage, stream_usage, &m_qHbmLength, PacketWrite));
        }
    }
}

} // namespace hb

} // namespace ns3
