/*
 * Copyright (c) 2023 Xi'an Jiaotong University
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
 * Authors: Danfeng Shan <dfshan@xjtu.edu.cn>
 */

#include "helper/fullmesh-star-sim-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/switch-mmu-helper.h"

/**
 * Default Network Topology
 *
 *        10.1.1.0
 * n0 -----switch 0------- n1
 *      point-to-point
 *
 * (node means node without switchmmu,
 *  while switch means node with switchmmu.)
 */

namespace ns3
{

namespace hb
{
NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

class FullmeshStarSimHelperTc301 : public FullmeshStarSimHelper
{
  public:
    FullmeshStarSimHelperTc301(std::string simName, Time start = Seconds(1), Time stop = Seconds(1.2));
    ~FullmeshStarSimHelperTc301() override;

    void SetupRouterPacketFilter() override;
};

FullmeshStarSimHelperTc301::FullmeshStarSimHelperTc301(std::string simName, Time start, Time stop)
    : FullmeshStarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

FullmeshStarSimHelperTc301::~FullmeshStarSimHelperTc301()
{
    NS_LOG_FUNCTION(this);
}

void
FullmeshStarSimHelperTc301::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls(12, 0);
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            for (uint32_t did = 0; did < m_nReceivers; did++)
            {
                rootFilter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                            m_spokeInterfaces.GetAddress(sid),
                                            m_spokeInterfaces.GetAddress(did),
                                            Ipv4Mask::GetOnes(),
                                            Ipv4Mask::GetOnes(),
                                            0,
                                            0xffff,
                                            0,
                                            0xffff,
                                            priCls[sid - m_nReceivers]);
            }
        }
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 (hp)
            std::vector<uint8_t> qCls(12, 0);
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
            {
                for (uint32_t did = 0; did < m_nReceivers; did++)
                {
                    l2Filter->AddClassifyRule(UdpL4Protocol::PROT_NUMBER,
                                            m_spokeInterfaces.GetAddress(sid),
                                            m_spokeInterfaces.GetAddress(did),
                                            Ipv4Mask::GetOnes(),
                                            Ipv4Mask::GetOnes(),
                                            0,
                                            0xffff,
                                            0,
                                            0xffff,
                                            qCls[sid - m_nReceivers]);
                }
            }
            l2Qdisc->AddPacketFilter(l2Filter);
        }
    }
}

} // namespace hb
} // namespace ns3

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FullmeshExample");

void
TracePhyTx(Ptr<PcapFileWrapper> file, Ptr<const Packet> packet)
{
    file->Write(Simulator::Now(), packet);
}

void
StartTracePhyTx(Ptr<PcapFileWrapper> file, Ptr<hb::FullmeshStarSimHelper> simHelper)
{
    Ptr<PointToPointNetDevice> dev =
        DynamicCast<PointToPointNetDevice>(simHelper->GetSpokeDevice(0));
    dev->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&TracePhyTx, file));
}


int
main(int argc, char* argv[])
{
    LogComponentEnable("FullmeshExample", LOG_LEVEL_ALL);
    LogComponentEnable("SimHelper", LOG_LEVEL_ALL);
    LogComponentEnable("StarSimHelper", LOG_LEVEL_ALL);
    LogComponentEnable("FullmeshStarSimHelper", LOG_LEVEL_ALL);
    LogComponentEnable("FullmeshApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("SwitchMmu", LOG_LEVEL_ALL);
    // LogComponentEnable("OffChipBuffer", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointReorderNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointReorderHelper", LOG_LEVEL_ALL);
    // LogComponentEnable("PrioQueueDisc", ns3::LOG_LEVEL_ALL);
    // LogComponentEnable("DrrQueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("FifoQueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("QueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("FiveTuplePacketFilter", LOG_LEVEL_ALL);
    // LogComponentEnable("TrafficControlLayer", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    uint32_t numSpokes = 16;
    uint32_t numReceivers = 8;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("100Gbps");
    Time sendLinkDelay = MicroSeconds(1);

    hb::FullmeshStarSimHelperTc301 simHelper("example");

    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);

    simHelper.EnableHbmThroughputTracing();
    simHelper.EnableBufferUsageTracing();
    simHelper.EnableBmResultTracing();
    simHelper.EnablePortThroughputTracing();
    simHelper.EnableQueueThroughputTracing();
    simHelper.EnableWCacheThroughputTracing();
    simHelper.EnableSramThroughputTracing();
    simHelper.EnableQueueWCacheTracing();
    simHelper.EnableQueueSramTracing();
    simHelper.EnableQueueHbmTracing();

    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> file =
        pcapHelper.CreateFile("sender-0.pcap", std::ios::out, PcapHelper::DLT_PPP);

    // First some background flow.
    uint32_t recvId = 1;
    uint32_t sendId = recvId + numReceivers;
    simHelper.AddFlow(sendId, recvId, Seconds(1), Seconds(1.1), DataRate("100Gbps"));
    Simulator::Schedule(NanoSeconds(1), &StartTracePhyTx, file, &simHelper);

    // Second some fullmesh flow.
    std::vector<std::vector<int>> fullmeshSocketIndex = {{0, 2, 3}, {5, 6, 7}};
    std::vector<std::vector<int>> fullmeshWeight = {{2, 1, 3}, {2, 2, 2}};
    simHelper.SetUpFullMeshTraffic(&fullmeshSocketIndex, &fullmeshWeight);

    // last config about the pktsize and sendsize.
    simHelper.SetPktSize(9600);

    simHelper.Run();

    return 0;
}