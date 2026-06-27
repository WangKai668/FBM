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

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/drr-queue-disc.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/five-tuple-packet-filter.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/node-container.h"
#include "ns3/packet-filter.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-module.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/udp-header.h"
#include "ns3/udp-l4-protocol.h"

using namespace ns3;

const uint8_t UDP_PROT_NUMBER = 17; //!< UDP Protocol number

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Queue Disc Test Item
 */
class FiveTupleQueueDiscTestItem : public QueueDiscItem
{
  public:
    /**
     * Constructor
     *
     * \param p the packet
     * \param addr the address
     */
    FiveTupleQueueDiscTestItem(Ptr<Packet> p, const Address& addr);
    virtual ~FiveTupleQueueDiscTestItem();
    virtual void AddHeader(void);
    virtual bool Mark(void);

  private:
    FiveTupleQueueDiscTestItem();
    /**
     * \brief Copy constructor
     * Disable default implementation to avoid misuse
     */
    FiveTupleQueueDiscTestItem(const FiveTupleQueueDiscTestItem&);
    /**
     * \brief Assignment operator
     * \return this object
     * Disable default implementation to avoid misuse
     */
    FiveTupleQueueDiscTestItem& operator=(const FiveTupleQueueDiscTestItem&);
};

FiveTupleQueueDiscTestItem::FiveTupleQueueDiscTestItem(Ptr<Packet> p, const Address& addr)
    : QueueDiscItem(p, addr, 0)
{
}

FiveTupleQueueDiscTestItem::~FiveTupleQueueDiscTestItem()
{
}

void
FiveTupleQueueDiscTestItem::AddHeader(void)
{
}

bool
FiveTupleQueueDiscTestItem::Mark(void)
{
    return false;
}

/**
 * This class tests packet filter based on five tuple.
 */
class FiveTuplePacketFilterTestCase : public TestCase
{
  public:
    FiveTuplePacketFilterTestCase();
    virtual ~FiveTuplePacketFilterTestCase();
    virtual void DoRun(void);

  private:
    /**
     * Check enqueued packets
     * \param queue the queue disc on which CheckEnqueued needs to be done
     * \param pktNum the number of enqueued packets to be compared with
     */
    void CheckEnqueued(Ptr<QueueDisc> queue, uint32_t pktNum);
};

FiveTuplePacketFilterTestCase::FiveTuplePacketFilterTestCase()
    : TestCase("Test five tuple packet filter")
{
}

FiveTuplePacketFilterTestCase::~FiveTuplePacketFilterTestCase()
{
}

void
FiveTuplePacketFilterTestCase::DoRun(void)
{
    /*
     * test 1 : Test the GetEnqueueClasses function.
     */

    // construct the 3 layer qdisc
    Ptr<DrrQueueDisc> root = CreateObject<DrrQueueDisc>();
    Ptr<FiveTuplePacketFilter> rootf = CreateObject<FiveTuplePacketFilter>();
    root->SetLayerId(QueueDisc::priority);
    root->AddPacketFilter(rootf);
    for (uint8_t i = 0; i < 2; i++)
    {
        Ptr<DrrQueueDisc> l2qdisc = CreateObject<DrrQueueDisc>();
        Ptr<FiveTuplePacketFilter> l2f = CreateObject<FiveTuplePacketFilter>();
        l2qdisc->SetLayerId(QueueDisc::queue);
        l2qdisc->AddPacketFilter(l2f);
        Ptr<DrrFlow> l2c = CreateObject<DrrFlow>();
        l2c->SetQuantum(1500);
        l2c->SetQueueDisc(l2qdisc);

        for (uint8_t j = 0; j < 2; j++)
        {
            Ptr<FifoQueueDisc> leaf = CreateObject<FifoQueueDisc>();
            leaf->Initialize();
            Ptr<QueueDiscClass> leafc = CreateObject<QueueDiscClass>();
            leafc->SetQueueDisc(leaf);
            l2qdisc->AddQueueDiscClass(leafc);
        }
        l2qdisc->Initialize();
        root->AddQueueDiscClass(l2c);
    }
    root->Initialize();

    // add classify rules
    rootf->AddClassifyRule(UDP_PROT_NUMBER,
                           "10.1.1.0",
                           "10.1.1.0",
                           "255.255.255.0",
                           "255.255.255.0",
                           0,
                           65535,
                           5678,
                           5678,
                           1);

    Ptr<FiveTuplePacketFilter> l2f1 = StaticCast<FiveTuplePacketFilter>(
        root->GetQueueDiscClass(0)->GetQueueDisc()->GetPacketFilter(0));
    Ptr<FiveTuplePacketFilter> l2f2 = StaticCast<FiveTuplePacketFilter>(
        root->GetQueueDiscClass(1)->GetQueueDisc()->GetPacketFilter(0));
    l2f1->AddClassifyRule(UDP_PROT_NUMBER,
                          "10.1.1.0",
                          "10.1.1.0",
                          "255.255.255.0",
                          "255.255.255.0",
                          0,
                          65535,
                          5678,
                          5678,
                          0);
    l2f2->AddClassifyRule(UDP_PROT_NUMBER,
                          "10.1.1.0",
                          "10.1.1.0",
                          "255.255.255.0",
                          "255.255.255.0",
                          0,
                          65535,
                          5678,
                          5678,
                          0);

    // create a packet with l3 and l4 header
    Ptr<Packet> udpPacket = Create<Packet>(1000);
    UdpHeader udpHeader;
    udpHeader.SetSourcePort(1234);
    udpHeader.SetDestinationPort(5678);
    udpPacket->AddHeader(udpHeader);

    Ipv4Header ipHeader;
    ipHeader.SetSource("10.1.1.1");
    ipHeader.SetDestination("10.1.1.2");
    ipHeader.SetProtocol(UDP_PROT_NUMBER);
    ipHeader.SetTos(1);
    ipHeader.SetPayloadSize(8); // Full UDP header
    udpPacket->AddHeader(ipHeader);

    Address dst;
    std::vector<std::tuple<QueueDisc::LayerId, int32_t>> path;
    path = root->GetEnqueueClasses(
        Create<Ipv4QueueDiscItem>(udpPacket, dst, UDP_PROT_NUMBER, ipHeader));
    NS_TEST_EXPECT_MSG_EQ(std::get<0>(path[0]),
                          QueueDisc::priority,
                          "Verify the layer id of first layer");
    NS_TEST_EXPECT_MSG_EQ(std::get<1>(path[0]), 1, "Verify the enqueue class of first layer");
    NS_TEST_EXPECT_MSG_EQ(std::get<0>(path[1]),
                          QueueDisc::queue,
                          "Verify the layer id of second layer");
    NS_TEST_EXPECT_MSG_EQ(std::get<1>(path[1]), 0, "Verify the enqueue class of second layer");

    /*
     * test 2 : Test whether the classify rule can work normally.
     */

    // construct the topo
    NodeContainer nodes;
    nodes.Create(2);
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    // config the queue disc
    TrafficControlHelper tc;
    tc.SetRootQueueDisc("ns3::DrrQueueDisc");
    QueueDiscContainer qdiscs = tc.Install(devices.Get(0));
    Ptr<DrrQueueDisc> qdisc = StaticCast<DrrQueueDisc>(qdiscs.Get(0));
    for (uint8_t i = 0; i < 2; i++)
    {
        Ptr<FifoQueueDisc> child = CreateObject<FifoQueueDisc>();
        child->Initialize();
        Ptr<DrrFlow> cls = CreateObject<DrrFlow>();
        cls->SetQuantum(1500);
        cls->SetQueueDisc(child);
        qdisc->AddQueueDiscClass(cls);
    }
    Ptr<FiveTuplePacketFilter> filter = CreateObject<FiveTuplePacketFilter>();
    filter->AddClassifyRule(UDP_PROT_NUMBER, // set classify rule for the first application
                            "10.1.1.0",
                            "10.1.1.0",
                            "255.255.255.0",
                            "255.255.255.0",
                            0,
                            65535,
                            1,
                            1,
                            0);
    filter->AddClassifyRule(UDP_PROT_NUMBER, // set classify rule for the second application
                            "10.1.1.0",
                            "10.1.1.0",
                            "255.255.255.0",
                            "255.255.255.0",
                            0,
                            65535,
                            2,
                            2,
                            1);
    qdisc->AddPacketFilter(filter);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // install two application with different five tuple
    UdpEchoServerHelper echoServer1(1);
    UdpEchoServerHelper echoServer2(2);

    ApplicationContainer serverApps = echoServer1.Install(nodes.Get(1));
    serverApps = echoServer2.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), 1);
    UdpEchoClientHelper echoClient2(interfaces.GetAddress(1), 2);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient1.Install(nodes.Get(0));
    clientApps = echoClient2.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_TEST_EXPECT_MSG_EQ(qdisc->GetNQueueDiscClasses(),
                          2,
                          "Verify that the queue disc has 2 child queue discs");

    // There should be 5 packets enqueued to the first class and the other 5 packets
    // enqueued to the second class.
    Simulator::Schedule(Time(Seconds(9.0)),
                        &FiveTuplePacketFilterTestCase::CheckEnqueued,
                        this,
                        qdisc->GetQueueDiscClass(0)->GetQueueDisc(),
                        5);
    Simulator::Schedule(Time(Seconds(9.0)),
                        &FiveTuplePacketFilterTestCase::CheckEnqueued,
                        this,
                        qdisc->GetQueueDiscClass(1)->GetQueueDisc(),
                        5);

    Simulator::Run();
    Simulator::Destroy();
}

void
FiveTuplePacketFilterTestCase::CheckEnqueued(Ptr<QueueDisc> queue, uint32_t pktNum)
{
    NS_TEST_EXPECT_MSG_EQ(queue->GetStats().nTotalEnqueuedPackets,
                          pktNum,
                          "Verify qdisc has the correct packets");
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Five Tuple Packet Filter Test Suite
 */
static class FiveTuplePacketFilterTestSuite : public TestSuite
{
  public:
    FiveTuplePacketFilterTestSuite()
        : TestSuite("five-tuple-packet-filter", UNIT)
    {
        AddTestCase(new FiveTuplePacketFilterTestCase(), TestCase::QUICK);
    }
} g_fiveTuplePacketFilterTestSuite; ///< the test suite
