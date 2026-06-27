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

#include "ns3/drop-tail-queue.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-reorder-net-device.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include <string>

using namespace ns3;

/**
 * \brief Test class for PointToPointReorder model
 *
 * It tries to send one packet from one NetDevice to another, over a
 * PointToPointChannel.
 */
class PointToPointReorderTest : public TestCase
{
  public:
    /**
     * \brief Create the test
     */
    PointToPointReorderTest();

    /**
     * \brief Run the test
     */
    virtual void DoRun(void);

  private:
    Ptr<const Packet> m_recvdPacket; //!< received packet

    /**
     * \brief Send one packet to the device specified
     *
     * \param device NetDevice to send to.
     * \param buffer Payload content of the packet.
     * \param size Size of the payload.
     * \param res The expected result of send method.
     */
    void SendOnePacket(Ptr<PointToPointReorderNetDevice> device, Ptr<Packet> pkt, bool res);

    /**
     * \brief Callback function which sets the recvdPacket parameter
     *
     * \param dev The receiving device.
     * \param pkt The received packet.
     * \param mode The protocol mode used.
     * \param sender The sender address.
     *
     * \return A boolean indicating packet handled properly.
     */
    bool RxPacket(Ptr<NetDevice> dev, Ptr<const Packet> pkt, uint16_t mode, const Address& sender);

    /**
     * \brief Check the received packet
     *
     * \param txBuffer The buffer which store initial payload of this packet.
     * \param bufferSize The size of the payload.
     * \param pkt The initial packet to match the received packet.
     */
    void CheckRxPacket(uint8_t* txBuffer, size_t bufferSize);

    /**
     * \brief Notify a packet ready to be sent
     *
     * \param pkt The ptr of the ready packet.
     */
    void NotifyPacketReady(Ptr<PointToPointReorderNetDevice> dev, Ptr<Packet> pkt);
};

PointToPointReorderTest::PointToPointReorderTest()
    : TestCase("PointToPointReorder")
{
}

void
PointToPointReorderTest::SendOnePacket(Ptr<PointToPointReorderNetDevice> device,
                                       Ptr<Packet> ptr,
                                       bool res)
{
    NS_TEST_EXPECT_MSG_EQ(device->Send(ptr, device->GetBroadcast(), 0x800), res, "Send one packet");
}

bool
PointToPointReorderTest::RxPacket(Ptr<NetDevice> dev,
                                  Ptr<const Packet> pkt,
                                  uint16_t mode,
                                  const Address& sender)
{
    m_recvdPacket = pkt;
    return true;
}

void
PointToPointReorderTest::CheckRxPacket(uint8_t* txBuffer, size_t bufferSize)
{
    NS_TEST_EXPECT_MSG_EQ(m_recvdPacket->GetSize(),
                          bufferSize,
                          "The size of received packet should be matched");

    uint8_t
        rxBuffer[1500]; // As large as the P2P MTU size, assuming that the user didn't change it.

    m_recvdPacket->CopyData(rxBuffer, bufferSize);
    // NS_TEST_EXPECT_MSG_EQ (memcmp (rxBuffer, txBuffer, bufferSize), 0, "The content of the
    // payload should be matched");
}

void
PointToPointReorderTest::NotifyPacketReady(Ptr<PointToPointReorderNetDevice> dev, Ptr<Packet> pkt)
{
    NS_TEST_EXPECT_MSG_EQ(dev->AttemptTransmission(), true, "Notify a packet ready to be sent");
}

void
PointToPointReorderTest::DoRun(void)
{
    Ptr<Node> a = CreateObject<Node>();
    Ptr<Node> b = CreateObject<Node>();
    Ptr<PointToPointReorderNetDevice> devA = CreateObject<PointToPointReorderNetDevice>();
    Ptr<PointToPointReorderNetDevice> devB = CreateObject<PointToPointReorderNetDevice>();
    Ptr<PointToPointChannel> channel = CreateObject<PointToPointChannel>();

    devA->Attach(channel);
    devA->SetAddress(Mac48Address::Allocate());
    devA->SetQueue(CreateObject<DropTailQueue<Packet>>());
    devB->Attach(channel);
    devB->SetAddress(Mac48Address::Allocate());
    devB->SetQueue(CreateObject<DropTailQueue<Packet>>());

    a->AddDevice(devA);
    b->AddDevice(devB);

    devB->SetReceiveCallback(MakeCallback(&PointToPointReorderTest::RxPacket, this));

    // Generate two packets with different payload transmit at 1.0s
    uint8_t txBuffer1[] = "P1\"Can you tell me where my country lies?\" \\ said the unifaun to his "
                          "true love's eyes. \\ \"It lies with me!\" cried the Queen of Maybe \\ - "
                          "for her merchandise, he traded in his prize.";
    size_t txBufferSize1 = sizeof(txBuffer1);
    uint8_t txBuffer2[] = "P2\"Can you tell me where my country lies?\" \\ said the unifaun to his "
                          "true love's eyes. \\ \"It lies with me!";
    size_t txBufferSize2 = sizeof(txBuffer2);

    /*
     * Test 1: two packet are both ready to be sent
     */
    Ptr<Packet> p1 = Create<Packet>(txBuffer1, txBufferSize1);
    Ptr<Packet> p2 = Create<Packet>(txBuffer2, txBufferSize2);

    Simulator::Schedule(Seconds(1.0), &Packet::SetMmuFetchStatus, p1, true);
    Simulator::Schedule(Seconds(1.0), &Packet::SetMmuFetchStatus, p2, true);

    Simulator::Schedule(Seconds(1.1),
                        &PointToPointReorderTest::SendOnePacket,
                        this,
                        devA,
                        p1,
                        true);
    Simulator::Schedule(Seconds(1.2),
                        &PointToPointReorderTest::CheckRxPacket,
                        this,
                        txBuffer1,
                        txBufferSize1);
    Simulator::Schedule(Seconds(1.3),
                        &PointToPointReorderTest::SendOnePacket,
                        this,
                        devA,
                        p2,
                        true);
    Simulator::Schedule(Seconds(1.4),
                        &PointToPointReorderTest::CheckRxPacket,
                        this,
                        txBuffer2,
                        txBufferSize2);

    /*
     * Test 2: the first packet is sent and the second packet is blocked
     */
    Ptr<Packet> p3 = Create<Packet>(txBuffer1, txBufferSize1);
    Ptr<Packet> p4 = Create<Packet>(txBuffer2, txBufferSize2);
    Simulator::Schedule(Seconds(2.0), &Packet::SetMmuFetchStatus, p3, true);

    Simulator::Schedule(Seconds(2.1),
                        &PointToPointReorderTest::SendOnePacket,
                        this,
                        devA,
                        p3,
                        true);
    Simulator::Schedule(Seconds(2.2),
                        &PointToPointReorderTest::CheckRxPacket,
                        this,
                        txBuffer1,
                        txBufferSize1);
    Simulator::Schedule(Seconds(2.3),
                        &PointToPointReorderTest::SendOnePacket,
                        this,
                        devA,
                        p4,
                        false);

    Simulator::Schedule(Seconds(2.4), &Packet::SetMmuFetchStatus, p4, true);
    Simulator::Schedule(Seconds(2.5), &PointToPointReorderTest::NotifyPacketReady, this, devA, p4);
    Simulator::Schedule(Seconds(2.6),
                        &PointToPointReorderTest::CheckRxPacket,
                        this,
                        txBuffer2,
                        txBufferSize2);

    /*
     * Test 3: the second packet is also blocked because the first
     * packet is not ready
     */
    Ptr<Packet> p5 = Create<Packet>(txBuffer1, txBufferSize1);
    Ptr<Packet> p6 = Create<Packet>(txBuffer2, txBufferSize2);
    Simulator::Schedule(Seconds(3.0), &Packet::SetMmuFetchStatus, p6, true);

    Simulator::Schedule(Seconds(3.1),
                        &PointToPointReorderTest::SendOnePacket,
                        this,
                        devA,
                        p5,
                        false);
    Simulator::Schedule(Seconds(3.2),
                        &PointToPointReorderTest::SendOnePacket,
                        this,
                        devA,
                        p6,
                        false);

    Simulator::Schedule(Seconds(3.3), &Packet::SetMmuFetchStatus, p5, true);
    Simulator::Schedule(Seconds(3.4), &PointToPointReorderTest::NotifyPacketReady, this, devA, p5);

    // when first packet is ready, these two packets will be sent out in order
    Simulator::Schedule(Seconds(3.5),
                        &PointToPointReorderTest::CheckRxPacket,
                        this,
                        txBuffer2,
                        txBufferSize2);

    Simulator::Run();
    Simulator::Destroy();
}

/**
 * \brief TestSuite for PointToPointReorderNetDevice
 */
class PointToPointReorderTestSuite : public TestSuite
{
  public:
    /**
     * \brief Constructor
     */
    PointToPointReorderTestSuite();
};

PointToPointReorderTestSuite::PointToPointReorderTestSuite()
    : TestSuite("reorder-point-to-point", UNIT)
{
    AddTestCase(new PointToPointReorderTest, TestCase::QUICK);
}

static PointToPointReorderTestSuite g_reorderPointToPointTestSuite; //!< The testsuite
