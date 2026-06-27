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

#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/htbf-queue-disc.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/uinteger.h"

using namespace ns3;

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Hierachical Tbf Queue Disc Test Item
 */
class HtbfQueueDiscTestItem : public QueueDiscItem
{
  public:
    /**
     * Constructor
     *
     * \param p the packet
     * \param addr the address
     */
    HtbfQueueDiscTestItem(Ptr<Packet> p, const Address& addr);
    virtual ~HtbfQueueDiscTestItem();
    virtual void AddHeader(void);
    virtual bool Mark(void);

  private:
    HtbfQueueDiscTestItem();
    /**
     * \brief Copy constructor
     * Disable default implementation to avoid misuse
     */
    HtbfQueueDiscTestItem(const HtbfQueueDiscTestItem&);
    /**
     * \brief Assignment operator
     * \return this object
     * Disable default implementation to avoid misuse
     */
    HtbfQueueDiscTestItem& operator=(const HtbfQueueDiscTestItem&);
};

HtbfQueueDiscTestItem::HtbfQueueDiscTestItem(Ptr<Packet> p, const Address& addr)
    : QueueDiscItem(p, addr, 0)
{
}

HtbfQueueDiscTestItem::~HtbfQueueDiscTestItem()
{
}

void
HtbfQueueDiscTestItem::AddHeader(void)
{
}

bool
HtbfQueueDiscTestItem::Mark(void)
{
    return false;
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Htbf Queue Disc Test Packet Filter
 */

class HtbfQueueDiscTestFilter : public PacketFilter
{
  public:
    /**
     * Constructor
     *
     * \param cls whether this filter is able to classify a HtbfQueueDiscTestItem
     */
    HtbfQueueDiscTestFilter(bool cls);
    virtual ~HtbfQueueDiscTestFilter();
    /**
     * \brief Set the value returned by DoClassify
     *
     * \param ret the value that DoClassify returns
     */
    void SetReturnValue(int32_t ret);

  private:
    virtual bool CheckProtocol(Ptr<QueueDiscItem> item) const;
    virtual int32_t DoClassify(Ptr<QueueDiscItem> item) const;

    bool m_cls;    //!< whether this filter is able to classify a HtbfQueueDiscTestItem
    int32_t m_ret; //!< the value that DoClassify returns if m_cls is true
};

HtbfQueueDiscTestFilter::HtbfQueueDiscTestFilter(bool cls)
    : m_cls(cls),
      m_ret(0)
{
}

HtbfQueueDiscTestFilter::~HtbfQueueDiscTestFilter()
{
}

void
HtbfQueueDiscTestFilter::SetReturnValue(int32_t ret)
{
    m_ret = ret;
}

bool
HtbfQueueDiscTestFilter::CheckProtocol(Ptr<QueueDiscItem> item) const
{
    return m_cls;
}

int32_t
HtbfQueueDiscTestFilter::DoClassify(Ptr<QueueDiscItem> item) const
{
    return m_ret;
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Htbf Queue Disc Test Case
 */
class HtbfQueueDiscTestCase : public TestCase
{
  public:
    HtbfQueueDiscTestCase();
    virtual void DoRun(void);

  private:
    /**
     * Set Attribute function
     * \param queue the queue disc into which set attribute needs to be done
     * \param name the name of attribute to be set
     * \param value the value of attribute to be set
     * \param flag the boolean value against which the return value to be compared with
     */
    void SetAttribute(Ptr<HtbfQueueDisc> queue,
                      std::string name,
                      const AttributeValue& value,
                      bool flag);
    /**
     * Enqueue function
     * \param queue the queue disc into which enqueue needs to be done
     * \param dest the destination address
     * \param size the size of the packet in bytes to be enqueued
     */
    void Enqueue(Ptr<HtbfQueueDisc> queue, Address dest, uint32_t size);
    /**
     * DequeueAndCheck function to check if a packet is blocked or not after dequeuing and verify
     * against expected result \param queue the queue disc on which DequeueAndCheck needs to be done
     * \param flag the boolean value against which the return value of dequeue () has to be compared
     * with \param printStatement the string to be printed in the NS_TEST_EXPECT_MSG_EQ
     */
    void DequeueAndCheck(Ptr<HtbfQueueDisc> queue, bool flag, std::string printStatement);
    /**
     * PacketsCheck function to check if the number of packets is expected
     * \param queue the queue disc on which PacketsCheck neeeds to be done
     * \param num the number of packets in the queue disc to be compared with
     * \param printStatement the string to be printed in the NS_TEST_EXPECT_MSG_EQ
     */
    void PacketsCheck(Ptr<HtbfQueueDisc> queue, uint32_t num, std::string printStatement);
    /**
     * SetRootQdisc function
     * \param queue the queue disc on which SetRootQdisc function needs to be done
     * \param root the root queue disc to be set
     * \param flag the boolean value against which the return value has to be compared with
     */
    void SetRootQdisc(Ptr<HtbfQueueDisc> queue, Ptr<QueueDisc> root, bool flag);
    /**
     * Run hierachical TBF test function
     * \param mode the mode
     */
    void RunHtbfTest(QueueSizeUnit mode);
};

HtbfQueueDiscTestCase::HtbfQueueDiscTestCase()
    : TestCase("Sanity check on the hierachical TBF queue implementation")
{
}

void
HtbfQueueDiscTestCase::RunHtbfTest(QueueSizeUnit mode)
{
    uint32_t pktSize = 1000;
    // 1 for packets; pktSize for bytes
    uint32_t modeSize = 1;
    uint32_t qSize = 10;
    uint32_t burst[3] = {5000, 4000, 2000};
    uint32_t mtu = 0;
    DataRate rate[3] = {DataRate("10KB/s"), DataRate("10KB/s"), DataRate("10KB/s")};
    DataRate peakRate = DataRate("0KB/s");

    Ptr<HtbfQueueDisc> qdisc = CreateObject<HtbfQueueDisc>();

    /*
     * test 1: Construct and test 3-layer htbf qdisc
     */

    // verify that we can actually set the attribute at root qdisc
    NS_TEST_EXPECT_MSG_EQ(qdisc->SetAttributeFailSafe("Burst", UintegerValue(burst[0])),
                          true,
                          "Verify that we can actually set the attribute Burst");
    NS_TEST_EXPECT_MSG_EQ(qdisc->SetAttributeFailSafe("Mtu", UintegerValue(mtu)),
                          true,
                          "Verify that we can actually set the attribute Mtu");
    NS_TEST_EXPECT_MSG_EQ(qdisc->SetAttributeFailSafe("Rate", DataRateValue(rate[0])),
                          true,
                          "Verify that we can actually set the attribute Rate");
    NS_TEST_EXPECT_MSG_EQ(qdisc->SetAttributeFailSafe("PeakRate", DataRateValue(peakRate)),
                          true,
                          "Verify that we can actually set the attribute PeakRate");
    NS_TEST_EXPECT_MSG_EQ(qdisc->SetRootQdisc(0),
                          true,
                          "Verify that we can actually set the attribute RootQdisc");

    // add root filter
    Ptr<HtbfQueueDiscTestFilter> rootf = CreateObject<HtbfQueueDiscTestFilter>(true);
    qdisc->AddPacketFilter(rootf);

    for (uint8_t i = 0; i < 2; i++)
    {
        Ptr<HtbfQueueDisc> l2qdisc = CreateObject<HtbfQueueDisc>();
        // verify that we can actually set the attribute at l2 qdisc
        NS_TEST_EXPECT_MSG_EQ(l2qdisc->SetAttributeFailSafe("Burst", UintegerValue(burst[i + 1])),
                              true,
                              "Verify that we can actually set the attribute Burst");
        NS_TEST_EXPECT_MSG_EQ(l2qdisc->SetAttributeFailSafe("Mtu", UintegerValue(mtu)),
                              true,
                              "Verify that we can actually set the attribute Mtu");
        NS_TEST_EXPECT_MSG_EQ(l2qdisc->SetAttributeFailSafe("Rate", DataRateValue(rate[i + 1])),
                              true,
                              "Verify that we can actually set the attribute Rate");
        NS_TEST_EXPECT_MSG_EQ(l2qdisc->SetAttributeFailSafe("PeakRate", DataRateValue(peakRate)),
                              true,
                              "Verify that we can actually set the attribute PeakRate");
        NS_TEST_EXPECT_MSG_EQ(l2qdisc->SetRootQdisc(0),
                              true,
                              "Verify that we can actually set the attribute RootQdisc");

        // add l2 filter
        Ptr<HtbfQueueDiscTestFilter> l2f = CreateObject<HtbfQueueDiscTestFilter>(true);
        l2qdisc->AddPacketFilter(l2f);

        Ptr<QueueDiscClass> l2c = CreateObject<QueueDiscClass>();
        l2c->SetQueueDisc(l2qdisc);
        // l3 configuration
        for (uint8_t j = 0; j < 1; j++)
        {
            Ptr<FifoQueueDisc> l3qdisc = CreateObject<FifoQueueDisc>();
            l3qdisc->Initialize();
            Ptr<QueueDiscClass> l3c = CreateObject<QueueDiscClass>();
            l3c->SetQueueDisc(l3qdisc);
            l2qdisc->AddQueueDiscClass(l3c);
        }
        l2qdisc->Initialize();
        qdisc->AddQueueDiscClass(l2c);
    }
    qdisc->Initialize();

    if (mode == QueueSizeUnit::BYTES)
    {
        modeSize = pktSize;
        qSize = qSize * modeSize;
    }

    Ptr<HtbfQueueDisc> l2qdisc1 =
        StaticCast<HtbfQueueDisc>(qdisc->GetQueueDiscClass(0)->GetQueueDisc());
    Ptr<HtbfQueueDisc> l2qdisc2 =
        StaticCast<HtbfQueueDisc>(qdisc->GetQueueDiscClass(1)->GetQueueDisc());
    Ptr<HtbfQueueDiscTestFilter> l2f1 =
        StaticCast<HtbfQueueDiscTestFilter>(l2qdisc1->GetPacketFilter(0));
    Ptr<HtbfQueueDiscTestFilter> l2f2 =
        StaticCast<HtbfQueueDiscTestFilter>(l2qdisc2->GetPacketFilter(0));

    /*
     * test 2 : This test checks the shaping control of 2-layer htbf qdisc. The traffic
     * should shaped by the two child qdisc and also under the control of root qdisc.
     */

    Address dest;
    // enqueue 4 packets to l2 right child qdisc
    rootf->SetReturnValue(0);
    l2f1->SetReturnValue(0);
    for (uint32_t i = 1; i <= 4; i++)
    {
        HtbfQueueDiscTestCase::Enqueue(qdisc, dest, pktSize);
    }
    // enqueue 4 packets to l2 right child qdisc
    rootf->SetReturnValue(1);
    l2f2->SetReturnValue(0);
    for (uint32_t i = 1; i <= 4; i++)
    {
        HtbfQueueDiscTestCase::Enqueue(qdisc, dest, pktSize);
    }

    for (uint32_t i = 1; i <= 8; i++)
    {
        if (i <= 5)
        {
            Ptr<QueueDiscItem> item = qdisc->Dequeue();
            NS_TEST_EXPECT_MSG_EQ((item != nullptr),
                                  true,
                                  "First 6 packets should dequeue successfully");
        }
        else
        {
            Ptr<QueueDiscItem> item = qdisc->Dequeue();
            NS_TEST_EXPECT_MSG_EQ((item != nullptr),
                                  false,
                                  "Last 10 packets should dequeue failed");
        }
    }
    NS_TEST_EXPECT_MSG_EQ(l2qdisc1->GetNPackets(),
                          1,
                          "There should be 4 packet in the left child queue disc");
    NS_TEST_EXPECT_MSG_EQ(l2qdisc2->GetNPackets(),
                          2,
                          "There should be 6 packet in the right child queue disc");

    // remove the 3 left packets after 0.2s
    Simulator::Schedule(Time(Seconds(0.2)),
                        &HtbfQueueDiscTestCase::DequeueAndCheck,
                        this,
                        l2qdisc1,
                        true,
                        "remove 1 packet from left qdisc in L2");
    Simulator::Schedule(Time(Seconds(0.2)),
                        &HtbfQueueDiscTestCase::DequeueAndCheck,
                        this,
                        l2qdisc2,
                        true,
                        "remove 1 packet from right qdisc in L2");
    Simulator::Schedule(Time(Seconds(0.2)),
                        &HtbfQueueDiscTestCase::DequeueAndCheck,
                        this,
                        l2qdisc2,
                        true,
                        "remove 1 packet from right qdisc in L2");
    Simulator::Schedule(Time(Seconds(0.2)),
                        &HtbfQueueDiscTestCase::PacketsCheck,
                        this,
                        l2qdisc1,
                        0,
                        "There should be no packets in the left child queue disc");
    Simulator::Schedule(Time(Seconds(0.2)),
                        &HtbfQueueDiscTestCase::PacketsCheck,
                        this,
                        l2qdisc2,
                        0,
                        "There should be no packets in the right child queue disc");

    /*
     * test 3 : When DataRate == FirstBucketTokenRate; packets should pass smoothly.
     */

    mtu = 1000;
    peakRate = DataRate("100KB/s");
    double interval = 2; //  enough time interval to wait first buckets become full

    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "Mtu",
                        UintegerValue(mtu),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "PeakRate",
                        DataRateValue(peakRate),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "Mtu",
                        UintegerValue(mtu),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "PeakRate",
                        DataRateValue(peakRate),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc2,
                        "Mtu",
                        UintegerValue(mtu),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc2,
                        "PeakRate",
                        DataRateValue(peakRate),
                        true);

    Simulator::Schedule(Time(Seconds(interval)), &QueueDisc::Initialize, qdisc);

    double delay = 0.09;
    for (uint32_t i = 1; i <= 10; i++)
    {
        Simulator::Schedule(Time(Seconds((i + 1) * delay + interval)),
                            &HtbfQueueDiscTestCase::Enqueue,
                            this,
                            qdisc,
                            dest,
                            pktSize);
    }
    delay = 0.1;
    for (uint32_t i = 1; i <= 10; i++)
    {
        Simulator::Schedule(Time(Seconds((i + 1) * delay + interval)),
                            &HtbfQueueDiscTestCase::DequeueAndCheck,
                            this,
                            qdisc,
                            true,
                            "No packet should be blocked");
    }
    Simulator::Stop(Seconds(interval + 1));
    Simulator::Run();

    /*
     * test 4 : When DataRate >>> FirstBucketTokenRate; some packets should get blocked and waking
     * of queue should get scheduled. 10 packets are enqueued and then dequeued. Since the token
     * rate is less than the data rate, the last packet i.e the 10th packet gets blocked and waking
     * of queue is scheduled after a time when enough tokens will be available. At that time the
     * 10th packet passes through.
     */

    // consruct the topo
    Config::SetDefault("ns3::QueueDisc::Quota", UintegerValue(1));
    NodeContainer nodesA;
    nodesA.Create(2);
    Ptr<SimpleNetDevice> txDevA = CreateObject<SimpleNetDevice>();
    nodesA.Get(0)->AddDevice(txDevA);
    Ptr<SimpleNetDevice> rxDevA = CreateObject<SimpleNetDevice>();
    nodesA.Get(1)->AddDevice(rxDevA);
    Ptr<SimpleChannel> channelA = CreateObject<SimpleChannel>();
    txDevA->SetChannel(channelA);
    rxDevA->SetChannel(channelA);
    txDevA->SetNode(nodesA.Get(0));
    rxDevA->SetNode(nodesA.Get(1));

    dest = txDevA->GetAddress();

    Ptr<TrafficControlLayer> tcA = CreateObject<TrafficControlLayer>();
    nodesA.Get(0)->AggregateObject(tcA);
    tcA->SetRootQueueDiscOnDevice(txDevA, qdisc);
    tcA->Initialize();

    interval += 2;
    burst[0] = 5000;
    burst[1] = 5000;
    rate[0] = DataRate("5KB/s");
    rate[1] = DataRate("5KB/s");

    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "Burst",
                        UintegerValue(burst[0]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "Rate",
                        DataRateValue(rate[0]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetRootQdisc,
                        this,
                        qdisc,
                        qdisc,
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "Burst",
                        UintegerValue(burst[1]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "Rate",
                        DataRateValue(rate[1]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetRootQdisc,
                        this,
                        l2qdisc1,
                        qdisc,
                        true);
    Simulator::Schedule(Time(Seconds(interval)), &QueueDisc::Initialize, qdisc);

    delay = 0.09;
    for (uint32_t i = 1; i <= 10; i++)
    {
        Simulator::Schedule(Time(Seconds((i + 1) * delay + interval)),
                            &HtbfQueueDiscTestCase::Enqueue,
                            this,
                            qdisc,
                            dest,
                            1000);
    }
    delay = 0.1;
    for (uint32_t i = 1; i <= 10; i++)
    {
        if (i == 10)
        {
            Simulator::Schedule(Time(Seconds((i + 1) * delay + interval)),
                                &HtbfQueueDiscTestCase::DequeueAndCheck,
                                this,
                                qdisc,
                                false,
                                "10th packet should be blocked");
        }
        else
        {
            Simulator::Schedule(Time(Seconds((i + 1) * delay + interval)),
                                &HtbfQueueDiscTestCase::DequeueAndCheck,
                                this,
                                qdisc,
                                true,
                                "This packet should not be blocked");
        }
    }
    // When the tokens are not enough, a packet should be blocked until the tokens increase to
    // enough. In this test, tokens increase to enough after 1.2s from initialization.
    Simulator::Schedule(Time(Seconds(12 * delay - 0.0001 + interval)),
                        &HtbfQueueDiscTestCase::PacketsCheck,
                        this,
                        qdisc,
                        1,
                        "Blocked packet should be send out");
    Simulator::Schedule(Time(Seconds(12 * delay + 0.0001 + interval)),
                        &HtbfQueueDiscTestCase::PacketsCheck,
                        this,
                        qdisc,
                        0,
                        "Blocked packet should be send out");
    Simulator::Stop(Seconds(interval + 1.3));
    Simulator::Run();

    /*
     * test 5 : This test checks the peakRate control of packet dequeue, when DataRate <
     * FirstBucketTokenRate. 10 packets each of size 1000 bytes are enqueued followed by their
     * dequeue. The data rate (25 KB/s) is not sufficiently higher than the btokens rate (15 KB/s),
     * so that in the startup phase the first bucket is not empty. Hence when adequate tokens are
     * present in the second (peak) bucket, the packets get transmitted, otherwise they are blocked.
     * So basically the transmission of packets falls under the regulation of the second bucket
     * since first bucket will always have excess tokens. TBF does not let all the packets go
     * smoothly without any control just because there are excess tokens in the first bucket.
     */

    interval += 2;
    burst[0] = 15000;
    burst[1] = 15000;
    rate[0] = DataRate("15KB/s");
    rate[1] = DataRate("15KB/s");
    peakRate = DataRate("20KB/s");

    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "Burst",
                        UintegerValue(burst[0]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "Rate",
                        DataRateValue(rate[0]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        qdisc,
                        "PeakRate",
                        DataRateValue(peakRate),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "Burst",
                        UintegerValue(burst[1]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "Rate",
                        DataRateValue(rate[1]),
                        true);
    Simulator::Schedule(Time(Seconds(interval)),
                        &HtbfQueueDiscTestCase::SetAttribute,
                        this,
                        l2qdisc1,
                        "PeakRate",
                        DataRateValue(peakRate),
                        true);
    Simulator::Schedule(Time(Seconds(interval)), &QueueDisc::Initialize, qdisc);

    delay = 0.04;
    interval += 2;
    for (uint32_t i = 1; i <= 10; i++)
    {
        Simulator::Schedule(Time(Seconds((i + 1) * delay + interval)),
                            &HtbfQueueDiscTestCase::Enqueue,
                            this,
                            qdisc,
                            dest,
                            pktSize);
    }

    // The pattern being checked is a pattern of dequeue followed by blocked.  The delay between
    // enqueues is not sufficient to allow ptokens to refill befor the next dequeue.  The first
    // enqueue is at 1.08s in the future, and the attempted dequeue is at 1.10s in the future.  The
    // first dequeue will always succeed.  The second enqueue is 1.12s and attempted dequeue is
    // at 1.14s in the future, but the last dequeue was 0.04s prior; only 800 tokens can be refilled
    // in 0.04s at a peak rate of 20Kbps.  The actual dequeue occurs at 0.01s further into the
    // future when ptokens refills to 1000. To repeat the pattern, odd-numbered dequeue events
    // should be spaced at intervals of at least 100ms, and the even-numbered dequeue events (that
    // block) should be 0.04s (delay) following the last odd-numbered dequeue event.
    double nextDelay =
        (2 * delay) + 0.02 + interval; // 20ms after first enqueue to attempt the first dequeue;
    for (uint32_t i = 1; i <= 2; i++)
    {
        if (i % 2 == 1)
        {
            Simulator::Schedule(Seconds(nextDelay),
                                &HtbfQueueDiscTestCase::DequeueAndCheck,
                                this,
                                qdisc,
                                true,
                                "1st packet should not be blocked");
            nextDelay += 0.04;
        }
        else
        {
            Simulator::Schedule(Seconds(nextDelay),
                                &HtbfQueueDiscTestCase::DequeueAndCheck,
                                this,
                                qdisc,
                                false,
                                "This packet should be blocked");
            nextDelay += 0.06; // Need 0.04 + 0.06 seconds to allow the next packet to be dequeued
                               // without block
        }
    }
    Simulator::Stop(Seconds(0.55 + interval));
    Simulator::Run();
}

void
HtbfQueueDiscTestCase::Enqueue(Ptr<HtbfQueueDisc> queue, Address dest, uint32_t size)
{
    queue->Enqueue(Create<HtbfQueueDiscTestItem>(Create<Packet>(size), dest));
}

void
HtbfQueueDiscTestCase::DequeueAndCheck(Ptr<HtbfQueueDisc> queue,
                                       bool flag,
                                       std::string printStatement)
{
    Ptr<QueueDiscItem> item = queue->Dequeue();
    NS_TEST_EXPECT_MSG_EQ((item != nullptr), flag, printStatement);
}

void
HtbfQueueDiscTestCase::PacketsCheck(Ptr<HtbfQueueDisc> queue,
                                    uint32_t num,
                                    std::string printStatement)
{
    NS_TEST_EXPECT_MSG_EQ(queue->GetNPackets(), num, printStatement);
}

void
HtbfQueueDiscTestCase::SetAttribute(Ptr<HtbfQueueDisc> queue,
                                    std::string name,
                                    const AttributeValue& value,
                                    bool flag)
{
    NS_TEST_EXPECT_MSG_EQ(queue->SetAttributeFailSafe(name, value),
                          flag,
                          "Verify that we can actually set the attribute " + name);
}

void
HtbfQueueDiscTestCase::SetRootQdisc(Ptr<HtbfQueueDisc> queue, Ptr<QueueDisc> root, bool flag)
{
    NS_TEST_EXPECT_MSG_EQ(queue->SetRootQdisc(root),
                          flag,
                          "Verify that we can actually set the attribute RootQdisc");
}

void
HtbfQueueDiscTestCase::DoRun(void)
{
    RunHtbfTest(QueueSizeUnit::PACKETS);
    RunHtbfTest(QueueSizeUnit::BYTES);
    Simulator::Destroy();
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Tbf Queue Disc Test Suite
 */
static class HtbfQueueDiscTestSuite : public TestSuite
{
  public:
    HtbfQueueDiscTestSuite()
        : TestSuite("htbf-queue-disc", UNIT)
    {
        AddTestCase(new HtbfQueueDiscTestCase(), TestCase::QUICK);
    }
} g_htbfQueueTestSuite; ///< the test suite
