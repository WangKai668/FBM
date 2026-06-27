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

#include "ns3/drr-queue-disc.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/log.h"
#include "ns3/packet-filter.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief DRR Queue Disc Test Item
 */

class DrrQueueDiscTestItem : public QueueDiscItem
{
  public:
    /**
     * Constructor
     *
     * \param p the packet
     * \param addr the address
     * \param priority the packet priority
     */
    DrrQueueDiscTestItem(Ptr<Packet> p, const Address& addr, uint8_t priority);
    virtual ~DrrQueueDiscTestItem();
    virtual void AddHeader(void);
    virtual bool Mark(void);
};

DrrQueueDiscTestItem::DrrQueueDiscTestItem(Ptr<Packet> p, const Address& addr, uint8_t priority)
    : QueueDiscItem(p, addr, 0)
{
    SocketPriorityTag priorityTag;
    priorityTag.SetPriority(priority);
    p->ReplacePacketTag(priorityTag);
}

DrrQueueDiscTestItem::~DrrQueueDiscTestItem()
{
}

void
DrrQueueDiscTestItem::AddHeader(void)
{
}

bool
DrrQueueDiscTestItem::Mark(void)
{
    return false;
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Drr Queue Disc Test Packet Filter
 */

class DrrQueueDiscTestFilter : public PacketFilter
{
  public:
    /**
     * Constructor
     *
     * \param cls whether this filter is able to classify a DrrQueueDiscTestItem
     */
    DrrQueueDiscTestFilter(bool cls);
    virtual ~DrrQueueDiscTestFilter();
    /**
     * \brief Set the value returned by DoClassify
     *
     * \param ret the value that DoClassify returns
     */
    void SetReturnValue(int32_t ret);

  private:
    virtual bool CheckProtocol(Ptr<QueueDiscItem> item) const;
    virtual int32_t DoClassify(Ptr<QueueDiscItem> item) const;

    bool m_cls;    //!< whether this filter is able to classify a DrrQueueDiscTestItem
    int32_t m_ret; //!< the value that DoClassify returns if m_cls is true
};

DrrQueueDiscTestFilter::DrrQueueDiscTestFilter(bool cls)
    : m_cls(cls),
      m_ret(0)
{
}

DrrQueueDiscTestFilter::~DrrQueueDiscTestFilter()
{
}

void
DrrQueueDiscTestFilter::SetReturnValue(int32_t ret)
{
    m_ret = ret;
}

bool
DrrQueueDiscTestFilter::CheckProtocol(Ptr<QueueDiscItem> item) const
{
    return m_cls;
}

int32_t
DrrQueueDiscTestFilter::DoClassify(Ptr<QueueDiscItem> item) const
{
    return m_ret;
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief Drr Queue Disc Test Case
 */

class DrrQueueDiscTestCase : public TestCase
{
  public:
    DrrQueueDiscTestCase();
    virtual void DoRun(void);
};

DrrQueueDiscTestCase::DrrQueueDiscTestCase()
    : TestCase("Sanity check on the DRR queue disc implementation")
{
}

void
DrrQueueDiscTestCase::DoRun(void)
{
    Ptr<DrrQueueDisc> qdisc;
    Ptr<QueueDiscItem> item;
    Address dest;

    /*
     * Test 1: set quantum
     */

    qdisc = CreateObject<DrrQueueDisc>();

    // add 4 child fifo queue discs
    for (uint8_t i = 0; i < 4; i++)
    {
        Ptr<FifoQueueDisc> child = CreateObject<FifoQueueDisc>();
        child->Initialize();
        Ptr<DrrFlow> c = CreateObject<DrrFlow>();
        c->SetQuantum(500 + i * 500);
        c->SetQueueDisc(child);
        qdisc->AddQueueDiscClass(c);
    }
    Ptr<DrrQueueDiscTestFilter> df1 = CreateObject<DrrQueueDiscTestFilter>(true);
    qdisc->AddPacketFilter(df1);
    qdisc->Initialize();

    NS_TEST_EXPECT_MSG_EQ(qdisc->GetNQueueDiscClasses(),
                          (size_t)4,
                          "Verify that the queue disc has 4 child queue discs");

    for (uint8_t i = 0; i < 4; i++)
    {
        NS_TEST_EXPECT_MSG_EQ(StaticCast<DrrFlow>(qdisc->GetQueueDiscClass(i))->GetQuantum(),
                              (uint32_t)(500 + i * 500),
                              "Verify that the quantum in class " << i << "has been correctly set");
    }

    /*
     * Test 2: classify packets based on the value returned by the installed packet filter
     */

    // create packets to class 0 to 3
    for (uint16_t i = 0; i < 4; i++)
    {
        df1->SetReturnValue(i);
        NS_TEST_EXPECT_MSG_EQ(qdisc->GetQueueDiscClass(i)->GetQueueDisc()->GetNPackets(),
                              (uint32_t)0,
                              "There should be no packets in the child queue disc " << i);
        for (uint16_t j = 0; j < 4; j++)
        {
            item = Create<DrrQueueDiscTestItem>(Create<Packet>(2000), dest, i);
            qdisc->Enqueue(item);
        }

        NS_TEST_EXPECT_MSG_EQ(qdisc->GetQueueDiscClass(i)->GetQueueDisc()->GetNPackets(),
                              (uint32_t)4,
                              "There should be 4 packet in the child queue disc " << i);
    }

    /*
     * Test 3: dequeue packets follow the DRR scheduler
     */

    for (uint16_t i = 0; i < 10; i++)
    {
        item = qdisc->Dequeue();
    }
    for (uint16_t i = 0; i < 4; i++)
    {
        NS_TEST_EXPECT_MSG_EQ(qdisc->GetQueueDiscClass(i)->GetQueueDisc()->GetNPackets(),
                              (uint32_t)(3 - i),
                              "Class " << i << " dequeued correctly");
    }

    Simulator::Destroy();
}

/**
 * \ingroup traffic-control-test
 * \ingroup tests
 *
 * \brief DRR Queue Disc Test Suite
 */
static class DrrQueueDiscTestSuite : public TestSuite
{
  public:
    DrrQueueDiscTestSuite()
        : TestSuite("drr-queue-disc", UNIT)
    {
        AddTestCase(new DrrQueueDiscTestCase(), TestCase::QUICK);
    }
} g_drrQueueTestSuite; ///< the test suite
