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
 * Authors: Yuqi Liu <liuyuqi526@126.com>
 */

//
// Network topology
//
//  n1
//     \ 10 Mb/s, 1ms
//      \          10Mb/s, 1ms
//       n0 -------------------------n3
//      /
//     / 10 Mb/s, 1ms
//   n2
//
// - all net devices are reorder-point-to-point net devices
// - all links are point-to-point links with indicated one-way BW/delay
// - UDP flows from n1 to n3, and from n2 to n3
// - DropTail queues with backpressure from NetDeviceQueueInterface
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridBufferExample");

const uint8_t UDP_PROT_NUMBER = 17; //!< UDP Protocol number

void
ShowTotalPktsLog(std::ofstream& out, Ptr<SwitchMmu> mmu)
{
    SwitchMmu::Stats mmuStats = mmu->GetStats();
    OffChipBuffer::Stats offchipStats = mmu->GetOffChipBuffer()->GetStats();
    out << "OnChipPackets: " << mmuStats.nTotalOnChipBufferStoredPackets << std::endl;
    out << "DropPackets: " << mmuStats.nTotalBmDropPackets << std::endl;
    out << "TotalWcachePackets: " << offchipStats.nTotalWcacheStoredPackets << std::endl;
    out << "TotalDramPackets: " << offchipStats.nTotalDramStoredPackets << std::endl;
    out << "At the End, current Mmu Pkts: " << mmu->GetNPackets() << std::endl;
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Maybe redirect to your own PATH.
    std::ofstream out("./hybrid-buffer-data/log_out");

    NodeContainer nodes;
    nodes.Create(4);

    // Topology
    SwitchMmuHelper mmuHelper;
    Ptr<SwitchMmu> mmu = mmuHelper.Install(nodes.Get(0));
    // todo: setmmutype
    Ptr<OffChipBuffer> dram = mmu->GetOffChipBuffer();
    // dram->SetDataRate (DataRate ("100Mbps"));

    NetDeviceContainer devs;
    NetDeviceContainer nodeDevs;
    NetDeviceContainer switchDevs;
    for (uint32_t i = 1; i < 4; i++)
    {
        Ptr<PointToPointReorderNetDevice> dev1 = CreateObject<PointToPointReorderNetDevice>();
        dev1->SetAttribute("DataRate", StringValue("100Gbps"));
        Ptr<PointToPointReorderNetDevice> dev2 = CreateObject<PointToPointReorderNetDevice>();
        dev2->SetAttribute("DataRate", StringValue("100Gbps"));
        Ptr<PointToPointChannel> channel = CreateObject<PointToPointChannel>();
        channel->SetAttribute("Delay", StringValue("1ms"));

        // AddDevice before Attach to set the ptr of Node
        nodes.Get(i)->AddDevice(dev1);
        nodes.Get(0)->AddDevice(dev2);

        dev1->SetAddress(Mac48Address::Allocate());
        dev2->SetAddress(Mac48Address::Allocate());

        // Set queue to net device
        dev1->SetQueue(CreateObject<DropTailQueue<Packet>>());
        dev2->SetQueue(CreateObject<DropTailQueue<Packet>>());
        Ptr<Queue<Packet>> queue1 = dev1->GetQueue();
        Ptr<Queue<Packet>> queue2 = dev2->GetQueue();

        // Reorder buffer size: 500KB
        queue1->SetAttribute("MaxSize", StringValue("500KiB"));
        queue2->SetAttribute("MaxSize", StringValue("500KiB"));

        // Aggregate NetDeviceQueueInterface object
        Ptr<NetDeviceQueueInterface> ndqi2 = CreateObject<NetDeviceQueueInterface>();
        ndqi2->GetTxQueue(0)->ConnectQueueTraces(queue2);
        dev2->AggregateObject(ndqi2);

        dev1->Attach(channel);
        dev2->Attach(channel);

        nodeDevs.Add(dev1);
        switchDevs.Add(dev2);
    }
    devs.Add(nodeDevs);
    devs.Add(switchDevs);
    NetDeviceContainer d1d0 = NetDeviceContainer(nodeDevs.Get(0), switchDevs.Get(0));
    NetDeviceContainer d2d0 = NetDeviceContainer(nodeDevs.Get(1), switchDevs.Get(1));
    NetDeviceContainer d3d0 = NetDeviceContainer(nodeDevs.Get(2), switchDevs.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    TrafficControlHelper tc;
    tc.SetRootQueueDisc("ns3::PrioQueueDisc");
    QueueDiscContainer roots = tc.Install(switchDevs);

    // Qdisc configuration
    for (uint32_t i = 0; i < 3; i++) // root qdisc
    {
        Ptr<PrioQueueDisc> priQdisc = StaticCast<PrioQueueDisc>(roots.Get(i));
        priQdisc->SetLayerId(QueueDisc::priority);
        Ptr<FiveTuplePacketFilter> priFilter = CreateObject<FiveTuplePacketFilter>();
        // Set classify rules to classify priority
        priFilter->AddClassifyRule(UDP_PROT_NUMBER,
                                   "10.1.1.1",
                                   "10.1.3.1",
                                   "255.255.255.255",
                                   "255.255.255.255",
                                   0,
                                   65535,
                                   0,
                                   65535,
                                   1);
        priFilter->AddClassifyRule(UDP_PROT_NUMBER,
                                   "10.1.2.1",
                                   "10.1.3.1",
                                   "255.255.255.255",
                                   "255.255.255.255",
                                   0,
                                   65535,
                                   0,
                                   65535,
                                   1);
        priQdisc->AddPacketFilter(priFilter);

        for (uint32_t j = 0; j < 2; j++) // 3 priority
        {
            if (j == 0) // HP
            {
                Ptr<PrioQueueDisc> qQdisc = CreateObject<PrioQueueDisc>();
                qQdisc->SetLayerId(QueueDisc::queue);
                Ptr<FiveTuplePacketFilter> qFilter = CreateObject<FiveTuplePacketFilter>();
                // Set classify rules to classify destination port
                qFilter->AddClassifyRule(UDP_PROT_NUMBER,
                                         "10.1.1.1",
                                         "10.1.3.1",
                                         "255.255.255.255",
                                         "255.255.255.255",
                                         0,
                                         65535,
                                         0,
                                         65535,
                                         1);
                qFilter->AddClassifyRule(UDP_PROT_NUMBER,
                                         "10.1.2.1",
                                         "10.1.3.1",
                                         "255.255.255.255",
                                         "255.255.255.255",
                                         0,
                                         65535,
                                         0,
                                         65535,
                                         1);
                qQdisc->AddPacketFilter(qFilter);
                Ptr<QueueDiscClass> priCls = CreateObject<QueueDiscClass>();
                priCls->SetQueueDisc(qQdisc);
                priQdisc->AddQueueDiscClass(priCls);

                for (uint32_t m = 0; m < 3; m++) // HP queues: CS0-CS2
                {
                    Ptr<FifoQueueDisc> leafQdisc = CreateObject<FifoQueueDisc>();
                    leafQdisc->Initialize();
                    // Set The FIFO leaf Max Size to be 100,000 packet to avoid unnecessary
                    // scheduler drops.
                    leafQdisc->SetAttribute("MaxSize", QueueSizeValue(QueueSize("100000000p")));
                    Ptr<QueueDiscClass> qCls = CreateObject<QueueDiscClass>();
                    qCls->SetQueueDisc(leafQdisc);
                    qQdisc->AddQueueDiscClass(qCls);
                }
                qQdisc->Initialize();
            }
            else if (j == 1) // LP
            {
                // DRR
                Ptr<DrrQueueDisc> qQdisc = CreateObject<DrrQueueDisc>();
                qQdisc->SetLayerId(QueueDisc::queue);
                Ptr<FiveTuplePacketFilter> qFilter = CreateObject<FiveTuplePacketFilter>();
                // Set classify rules to classify destination port
                qFilter->AddClassifyRule(UDP_PROT_NUMBER,
                                         "10.1.1.1",
                                         "10.1.3.1",
                                         "255.255.255.255",
                                         "255.255.255.255",
                                         0,
                                         65535,
                                         0,
                                         65535,
                                         1);
                qFilter->AddClassifyRule(UDP_PROT_NUMBER,
                                         "10.1.2.1",
                                         "10.1.3.1",
                                         "255.255.255.255",
                                         "255.255.255.255",
                                         0,
                                         65535,
                                         0,
                                         65535,
                                         2);
                qQdisc->AddPacketFilter(qFilter);
                Ptr<QueueDiscClass> priCls = CreateObject<QueueDiscClass>();
                priCls->SetQueueDisc(qQdisc);
                priQdisc->AddQueueDiscClass(priCls);
                uint32_t quantums[5] = {2, 2, 1, 1, 1};
                for (uint32_t m = 0; m < 5; m++) // LP queues: CS3-CS7
                {
                    Ptr<FifoQueueDisc> leafQdisc = CreateObject<FifoQueueDisc>();
                    leafQdisc->Initialize();
                    // Set The FIFO leaf Max Size to be 100,000 packet to avoid unnecessary
                    // scheduler drops.
                    leafQdisc->SetAttribute("MaxSize", QueueSizeValue(QueueSize("100000000p")));
                    Ptr<DrrFlow> qCls = CreateObject<DrrFlow>();
                    qCls->SetQuantum(quantums[m] * 1024);
                    qCls->SetQueueDisc(leafQdisc);
                    qQdisc->AddQueueDiscClass(qCls);
                }
                qQdisc->Initialize();
            }
        }
        priQdisc->Initialize();
    }

    // IP address configuration
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i0 = address.Assign(d1d0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i0 = address.Assign(d2d0);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i0 = address.Assign(d3d0);

    // IP routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // n1 -> n3
    uint16_t port = 2;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(i3i0.GetAddress(0), port)));
    onoff.SetConstantRate(DataRate("60Gbps"));
    ApplicationContainer apps = onoff.Install(nodes.Get(1));
    apps.Start(Seconds(1.1));
    apps.Stop(Seconds(1.5));

    // n2 -> n3
    onoff.SetConstantRate(DataRate("60Gbps"));
    apps = onoff.Install(nodes.Get(2));
    apps.Start(Seconds(1.1));
    apps.Stop(Seconds(1.5));

    // Create a packet sink to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(nodes.Get(3));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(2.0));

    Simulator::Run();
    Simulator::Destroy();
    ShowTotalPktsLog(out, mmu);
}
