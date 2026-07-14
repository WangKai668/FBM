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

#include "point-to-point-reorder-net-device.h"

#include "point-to-point-channel.h"
#include "ppp-header.h"

#include "ns3/error-model.h"
#include "ns3/llc-snap-header.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/pointer.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PointToPointReorderNetDevice");

NS_OBJECT_ENSURE_REGISTERED(PointToPointReorderNetDevice);

TypeId
PointToPointReorderNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PointToPointReorderNetDevice")
            .SetParent<PointToPointNetDevice>()
            .SetGroupName("PointToPoint")
            .AddConstructor<PointToPointReorderNetDevice>()
            .AddAttribute("EnableMulticast",
                "Enable multicast for each ingress port",
                BooleanValue(false),
                MakeBooleanAccessor(&PointToPointReorderNetDevice::m_enableMulticast),
                MakeBooleanChecker())
            .AddAttribute("CopyNums",
                "Copy numbers of multicast",
                UintegerValue(100),
                MakeUintegerAccessor(&PointToPointReorderNetDevice::m_copyNums),
                MakeUintegerChecker<uint32_t>())          
            .AddTraceSource("ReorderPhyTxEnd",
                            "Trace source indicating a packet has been "
                            "completely transmitted over the channel",
                            MakeTraceSourceAccessor(&PointToPointReorderNetDevice::m_phyTxEndTrace),
                            "ns3::Packet::TracedCallback");
    return tid;
}

PointToPointReorderNetDevice::PointToPointReorderNetDevice()
{
    NS_LOG_FUNCTION(this);
}

PointToPointReorderNetDevice::~PointToPointReorderNetDevice()
{
    NS_LOG_FUNCTION(this);
}

void
PointToPointReorderNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    PointToPointNetDevice::DoDispose();
}

void
PointToPointReorderNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this);
    m_node = node;
    m_mmu = m_node->GetObject<SwitchMmu>();
    // NS_ASSERT_MSG(m_mmu, "Cannot find mmu aggregated to this node");
}

void
PointToPointReorderNetDevice::SetMmu(Ptr<SwitchMmu> mmu)
{
    NS_LOG_FUNCTION(this);
    m_mmu = mmu;
    Ptr<Node> node = m_mmu->GetObject<Node>();
    NS_ASSERT_MSG(m_node == nullptr || node == m_node,
                  "MMU is not attached to the node of the device");
}

Ptr<SwitchMmu>
PointToPointReorderNetDevice::GetMmu()
{
    NS_LOG_FUNCTION(this);
    return m_mmu;
}

bool
PointToPointReorderNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet << dest << protocolNumber);
    NS_LOG_LOGIC("p=" << packet << ", dest=" << &dest);
    NS_LOG_LOGIC("UID is " << packet->GetUid());

    // If IsLinkUp() is false it means there is no channel to send any packet
    // over so we just hit the drop trace on the packet and return an error.
    if (IsLinkUp() == false)
    {
        m_macTxDropTrace(packet);
        return false;
    }

    // Stick a point to point protocol header on the packet in preparation for
    // shoving it out the door.
    AddHeader(packet, protocolNumber);

    m_macTxTrace(packet);

    // We should enqueue and dequeue the packet to hit the tracing hooks.
    if (m_queue->Enqueue(packet))
    {
        // Fetch the packet from mmu
        if (m_mmu)
        {
            //--sj TCP输出添加
            if(flag_print == 1){
            std::cout << "P2P_REORDER_QUEUE"
              << ",time_s=" << Simulator::Now().GetSeconds()
              << ",port=" << packet->GetMmuUsedPort()
              << ",bytes=" << m_queue->GetNBytes()
              << ",packets=" << m_queue->GetNPackets()
              << std::endl;
            }
            NS_LOG_LOGIC("Fetch the packet from mmu");
            if (m_mmu->Fetch(packet))
            {
                return AttemptTransmission();
            }
        }
        return AttemptTransmission();
    }
    NS_LOG_LOGIC("Send failed");
    return false;
}

bool
PointToPointReorderNetDevice::AttemptTransmission()
{
    NS_LOG_FUNCTION(this);
    // If the channel is ready for transition we send the packet right now
    if (m_txMachineState == READY)
    {
        Ptr<Packet> packet = 0;
        // Check whether the packet is ready before dequeue
        if (m_mmu)
        {
            packet = ConstCast<Packet>(m_queue->Peek());
            // Check zero pointer condition first
            if (!packet || packet->GetMmuFetchStatus() == false)
            {
                NS_LOG_LOGIC("Packet has not fetched from MMU");
                return false;
            }
        }

        packet = m_queue->Dequeue();
        // --sj TCP添加
<<<<<<< HEAD
        if(flag_print == 1){
            if (m_mmu && packet)
            {
                std::cout << "P2P_REORDER_QUEUE"
                        << ",time_s=" << Simulator::Now().GetSeconds()
                        << ",port=" << packet->GetMmuUsedPort()
                        << ",bytes=" << m_queue->GetNBytes()
                        << ",packets=" << m_queue->GetNPackets()
                        << std::endl;
            }            
        }

=======
        // if (m_mmu && packet)
        // {
        //     std::cout << "P2P_REORDER_QUEUE"
        //             << ",time_s=" << Simulator::Now().GetSeconds()
        //             << ",port=" << packet->GetMmuUsedPort()
        //             << ",bytes=" << m_queue->GetNBytes()
        //             << ",packets=" << m_queue->GetNPackets()
        //             << std::endl;
        // }
>>>>>>> upstream/main
        m_snifferTrace(packet);
        m_promiscSnifferTrace(packet);
        NS_LOG_LOGIC("Send the packet " << packet << "(UID=" << packet->GetUid() << ")");
        bool ret = TransmitStart(packet);
        return ret;
    }
    NS_LOG_LOGIC("Channel not ready");
    return false;
}

void
PointToPointReorderNetDevice::SetTc(Ptr<TrafficControlLayer> tc)
{
    NS_LOG_FUNCTION(this << tc);
    m_tc = tc;
}

Ptr<TrafficControlLayer>
PointToPointReorderNetDevice::GetTc()
{
    NS_LOG_FUNCTION(this);
    return m_tc;
}

void
PointToPointReorderNetDevice::NotifyLinkUp()
{
    NS_LOG_FUNCTION(this);
    m_linkUp = true;
    m_linkChangeCallbacks();

    // Register the device handler
    if (m_mmu)
    {
        m_mmu->RegisterDeviceHandler(
            MakeCallback(&PointToPointReorderNetDevice::AttemptTransmission, this),
            this);
    }
}

void
PointToPointReorderNetDevice::LinkDown()
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Get the P2PReorderNetdeviceDown");

    m_linkUp = false;
}

bool
PointToPointReorderNetDevice::Attach(Ptr<PointToPointChannel> ch)
{
    NS_LOG_FUNCTION(this << &ch);

    m_channel = ch;

    m_channel->Attach(this);

    //
    // This device is up whenever it is attached to a channel.  A better plan
    // would be to have the link come up when both devices are attached, but this
    // is not done for now.
    //
    NotifyLinkUp();
    return true;
}

void
PointToPointReorderNetDevice::TransmitComplete()
{
    NS_LOG_FUNCTION(this);

    //
    // This function is called to when we're all done transmitting a packet.
    // We try and pull another packet off of the transmit queue.  If the queue
    // is empty, we are done, otherwise we need to start transmitting the
    // next packet.
    //
    NS_ASSERT_MSG(m_txMachineState == BUSY, "Must be BUSY if transmitting");
    m_txMachineState = READY;

    NS_ASSERT_MSG(m_currentPkt, "PointToPointNetDevice::TransmitComplete(): m_currentPkt zero");

    m_phyTxEndTrace(m_currentPkt);
    m_currentPkt = nullptr;

    if (!m_queue->Peek())
    {
        NS_LOG_LOGIC("No pending packets in device queue after tx complete");
        return;
    }
    AttemptTransmission();
}

bool
PointToPointReorderNetDevice::TransmitStart(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);
    NS_LOG_LOGIC("UID is " << p->GetUid() << ")");

    //
    // This function is called to start the process of transmitting a packet.
    // We need to tell the channel that we've started wiggling the wire and
    // schedule an event that will be executed when the transmission is complete.
    //
    NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
    m_txMachineState = BUSY;
    m_currentPkt = p;
    m_phyTxBeginTrace(m_currentPkt);

    Time txTime = m_bps.CalculateBytesTxTime(p->GetSize());
    Time txCompleteTime = txTime + m_tInterframeGap;

    NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.As(Time::S));
    Simulator::Schedule(txCompleteTime, &PointToPointReorderNetDevice::TransmitComplete, this);

    bool result = m_channel->TransmitStart(p, this, txTime);
    if (result == false)
    {
        m_phyTxDropTrace(p);
    }
    return result;
}

void
PointToPointReorderNetDevice::Receive(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    uint16_t protocol = 0;

    if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
    {
        //
        // If we have an error model and it indicates that it is time to lose a
        // corrupted packet, don't forward this packet up, let it go.
        //
        m_phyRxDropTrace(packet);
    }
    else
    {
        //
        // Hit the trace hooks.  All of these hooks are in the same place in this
        // device because it is so simple, but this is not usually the case in
        // more complicated devices.
        //
        m_snifferTrace(packet);
        m_promiscSnifferTrace(packet);
        m_phyRxEndTrace(packet);

        //
        // Trace sinks will expect complete packets, not packets without some of the
        // headers.
        //
        Ptr<Packet> originalPacket = packet->Copy();

        //
        // Strip off the point-to-point protocol header and forward this packet
        // up the protocol stack.  Since this is a simple point-to-point link,
        // there is no difference in what the promisc callback sees and what the
        // normal receive callback sees.
        //
        ProcessHeader(packet, protocol);

        if (m_enableMulticast)
        {
            for (uint32_t i = 0; i < m_copyNums; i++)
            {
                Ptr<Packet> copy = packet->Copy();
                if (!m_promiscCallback.IsNull())
                {
                    m_macPromiscRxTrace(originalPacket);
                    m_promiscCallback(this,
                                    copy,
                                    protocol,
                                    GetRemote(),
                                    GetAddress(),
                                    NetDevice::PACKET_HOST);
                }
                m_macRxTrace(originalPacket);
                m_rxCallback(this, packet, protocol, GetRemote());
            }
        }
        else
        {
            if (!m_promiscCallback.IsNull())
            {
                m_macPromiscRxTrace(originalPacket);
                m_promiscCallback(this,
                                packet,
                                protocol,
                                GetRemote(),
                                GetAddress(),
                                NetDevice::PACKET_HOST);
            }
            m_macRxTrace(originalPacket);
            m_rxCallback(this, packet, protocol, GetRemote());
        }
    }
}


} // namespace ns3
