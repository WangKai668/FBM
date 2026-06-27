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
 * Authors: Shunlei Yang <yxlzqmysl0405@stu.xjtu.edu.cn>
 */

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/switch-mmu-helper.h"
#include "ns3/switch-mmu.h"
#include "ns3/test.h"

#include <stdio.h>

using namespace ns3;

/**
 * \ingroup network-test
 * \ingroup tests
 *
 * \brief SwitchMmu Test Case
 */

class SwitchMmuTestCase : public TestCase
{
  public:
    SwitchMmuTestCase();
    virtual void DoRun(void);
    void R(Ptr<Packet> p, Ptr<SwitchMmu> mmu);
};

SwitchMmuTestCase::SwitchMmuTestCase()
    : TestCase("Simple Test for Switch Mmu")
{
}

void
SwitchMmuTestCase::R(Ptr<Packet> p, Ptr<SwitchMmu> mmu)
{
    mmu->Fetch(p);
}

void
SwitchMmuTestCase::DoRun(void)
{
    LogComponentEnable("SwitchMmu", LOG_LEVEL_DEBUG);
    LogComponentEnable("OffChipBuffer", LOG_LEVEL_DEBUG);
    uint32_t pktSize = 1500;
    Ptr<Packet> write1 = Create<Packet>(pktSize);
    Ptr<Packet> write2 = Create<Packet>(pktSize);
    Ptr<Packet> write3 = Create<Packet>(pktSize);
    Ptr<Packet> write4 = Create<Packet>(pktSize);
    Ptr<Packet> write5 = Create<Packet>(pktSize);

    write1->SetMmuUsedPort(0);
    write1->SetMmuUsedPriority(1);
    write1->SetMmuUsedQueueId(2);

    write2->SetMmuUsedPort(1);
    write2->SetMmuUsedPriority(2);
    write2->SetMmuUsedQueueId(1);

    write3->SetMmuUsedPort(1);
    write3->SetMmuUsedPriority(1);
    write3->SetMmuUsedQueueId(0);

    write4->SetMmuUsedPort(1);
    write4->SetMmuUsedPriority(2);
    write4->SetMmuUsedQueueId(0);

    write5->SetMmuUsedPort(2);
    write5->SetMmuUsedPriority(2);
    write5->SetMmuUsedQueueId(0);

    Ptr<SwitchMmu> mmu = CreateObject<SwitchMmu>();

    SwitchMmu::BmResult result = mmu->CheckIngressAdmission(write1);
    mmu->Store(write1, result);

    result = mmu->CheckIngressAdmission(write2);
    mmu->Store(write2, result);

    result = mmu->CheckIngressAdmission(write3);
    mmu->Store(write3, result);

    result = mmu->CheckIngressAdmission(write5);
    mmu->Store(write5, result);

    result = mmu->CheckIngressAdmission(write4);
    mmu->Store(write4, result);

    // Condition 1: Write and immediately Read, so that the Req has not been
    // the Req Vec. And need to clear it not in the Request Queue.
    mmu->Fetch(write1);

    // Condition 2: The Packet in HBM and get
    Simulator::Schedule(Seconds(0.000000007), &SwitchMmuTestCase::R, this, write2, mmu);

    // Condition 3: The Packet is doing the writing
    Simulator::Schedule(Seconds(0.000000008), &SwitchMmuTestCase::R, this, write3, mmu);

    // Condition 4: The Write-in Req is in the Vec so just need clear Req Queue.
    Simulator::Schedule(Seconds(0.000000008), &SwitchMmuTestCase::R, this, write4, mmu);

    Simulator::Run();
    Simulator::Destroy();
}

/**
 * \ingroup network-test
 * \ingroup tests
 *
 * \brief SwitchMmu Test Suite
 */
class SwitchMmuTestSuite : public TestSuite
{
  public:
    SwitchMmuTestSuite()
        : TestSuite("switch-mmu", UNIT)
    {
        AddTestCase(new SwitchMmuTestCase(), TestCase::QUICK);
    }
};

static SwitchMmuTestSuite g_switchMmuTestSuite; ///< the test suite
