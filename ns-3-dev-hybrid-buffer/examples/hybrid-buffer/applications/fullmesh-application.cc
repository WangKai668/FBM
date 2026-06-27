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
 * Author: Danfeng Shan <dfshan@xjtu.edu.cn>
 */

#include "fullmesh-application.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/uinteger.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FullmeshApplication");

NS_OBJECT_ENSURE_REGISTERED(FullmeshApplication);

TypeId
FullmeshApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FullmeshApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<FullmeshApplication>()
            .AddAttribute("SendSize",
                          "The amount of data to send each time.",
                          UintegerValue(512),
                          MakeUintegerAccessor(&FullmeshApplication::m_sendSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. "
                          "Once these bytes are sent, "
                          "no data  is sent again. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&FullmeshApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use.",
                          TypeIdValue(TcpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&FullmeshApplication::m_tid),
                          MakeTypeIdChecker())
            .AddAttribute("LimitOutputBytes",
                          "Limit of queued bytes in the network stack",
                          UintegerValue(500 << 10),
                          MakeUintegerAccessor(&FullmeshApplication::m_limitOutputBytes),
                          MakeUintegerChecker<uint32_t>(1500))
            .AddTraceSource("Tx",
                            "A new packet is sent",
                            MakeTraceSourceAccessor(&FullmeshApplication::m_txTrace),
                            "ns3::Packet::TracedCallback");
    return tid;
}

FullmeshApplication::FullmeshApplication()
    : m_sentBytes(0),
      m_unsentPacket(nullptr),
      m_nConnected(0),
      m_round(0),
      m_sendSockId(0),
      m_isBlocked(false)
{
    m_sockets.clear();
    m_peers.clear();
    NS_LOG_FUNCTION(this);
}

FullmeshApplication::~FullmeshApplication()
{
    NS_LOG_FUNCTION(this);
}

void
FullmeshApplication::SetMaxBytes(uint64_t maxBytes)
{
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
FullmeshApplication::GetSocket(uint32_t i) const
{
    NS_LOG_FUNCTION(this);
    return m_sockets[i];
}

void
FullmeshApplication::AddRemote(const Address& addr, uint32_t weight)
{
    NS_LOG_FUNCTION(this << addr);
    m_peers.push_back(addr);
    m_sockets.emplace_back(nullptr);
    m_nSends.push_back(weight);
    m_sentBytes.push_back(0);
}

void
FullmeshApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_sockets.clear();
    m_peers.clear();
    m_unsentPacket = nullptr;
    // chain up
    Application::DoDispose();
}

// Application Methods
void
FullmeshApplication::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);

    for (uint32_t i = 0; i < m_sockets.size(); i++)
    {
        m_sentBytes[i] = 0;
        if (!m_sockets[i])
        {
            m_sockets[i] = Socket::CreateSocket(GetNode(), m_tid);

            int ret = -1;
            if (Inet6SocketAddress::IsMatchingType(m_peers[i]))
            {
                ret = m_sockets[i]->Bind6();
            }
            else if (InetSocketAddress::IsMatchingType(m_peers[i]))
            {
                ret = m_sockets[i]->Bind();
            }
            if (ret == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }

            m_sockets[i]->SetConnectCallback(
                MakeCallback(&FullmeshApplication::ConnectionSucceeded, this),
                MakeCallback(&FullmeshApplication::ConnectionFailed, this));
            m_sockets[i]->Connect(m_peers[i]);
            m_sockets[i]->SetSendCallback(MakeCallback(&FullmeshApplication::DataSend, this));
            m_sockets[i]->ShutdownRecv();
            SetupSmallQueue(m_sockets[i]);
        }
    }

    // Create the socket if not already
    if (m_nConnected >= m_sockets.size())
    {
        SendData();
    }
}

void
FullmeshApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    for (auto& socket : m_sockets)
    {
        if (socket)
        {
            socket->Close();
            m_nConnected--;
        }
        else
        {
            NS_LOG_WARN("FullmeshApplication found null socket to close in StopApplication");
        }
    }
}

void
FullmeshApplication::InvokeSendData(Ptr<const QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this);

    if (m_isBlocked)
    {
        m_isBlocked = false;
        SendData();
    }
}

void
FullmeshApplication::SetupSmallQueue(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Ptr<Node> node = socket->GetNode();
    Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
    if (node->GetNDevices() > 2)
    {
        // one sending dev and one receiving dev
        NS_LOG_ERROR("Multiple devices are not supported!");
    }
    NS_ASSERT(node->GetNDevices() == 2);
    Ptr<NetDevice> dev = node->GetDevice(0);
    Ptr<QueueDisc> qdisc = tc->GetRootQueueDiscOnDevice(dev);
    if (qdisc)
    {
        qdisc->TraceConnectWithoutContext("Dequeue",
                                          MakeCallback(&FullmeshApplication::InvokeSendData, this));
    }
    else
    {
        NS_LOG_ERROR("Cannot find queue disc");
        NS_ASSERT(false);
    }
}

// Private helpers
bool
FullmeshApplication::IsQueueSmall(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Ptr<Node> node = socket->GetNode();
    Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
    Ptr<NetDevice> dev = node->GetDevice(0);
    Ptr<QueueDisc> qdisc = tc->GetRootQueueDiscOnDevice(dev);
    if (qdisc->GetNBytes() <= m_limitOutputBytes &&
        qdisc->GetNPackets() <= m_limitOutputBytes / m_sendSize)
    {
        return true;
    }
    return false;
}

void
FullmeshApplication::SendData()
{
    NS_LOG_FUNCTION(this);

    uint32_t nFinished = 0;
    int prevSockId = m_sendSockId;
    if (m_isBlocked)
    {
        return;
    }
    while (m_maxBytes == 0 || nFinished < m_sockets.size())
    {
        if (int(m_sendSockId) == prevSockId)
        {
            nFinished = 0;
        }
        if (m_maxBytes > 0 && m_sentBytes[m_sendSockId] >= m_maxBytes)
        {
            nFinished++;
            m_sendSockId++;
            if (m_sendSockId >= m_sockets.size())
            {
                m_sendSockId = 0;
                m_round++;
            }
            continue;
        }
        // Time to send more

        if (!IsQueueSmall(m_sockets[m_sendSockId]))
        {
            // block this application
            m_isBlocked = true;
            break;
        }

        // uint64_t to allow the comparison later.
        // the result is in a uint32_t range anyway, because
        // m_sendSize is uint32_t.
        uint64_t toSend = m_sendSize;
        // Make sure we don't send too many
        if (m_maxBytes > 0)
        {
            toSend = std::min(toSend, m_maxBytes - m_sentBytes[m_sendSockId]);
        }

        NS_LOG_LOGIC("sending packet at " << Simulator::Now());

        Ptr<Packet> packet;
        if (m_unsentPacket)
        {
            packet = m_unsentPacket;
            toSend = packet->GetSize();
        }
        else
        {
            packet = Create<Packet>(toSend);
        }

        int actual = m_sockets[m_sendSockId]->Send(packet);
        if ((unsigned)actual == toSend)
        {
            m_sentBytes[m_sendSockId] += actual;
            m_txTrace(packet);
            m_unsentPacket = nullptr;
        }
        else if (actual == -1)
        {
            // We exit this loop when actual < toSend as the send side
            // buffer is full. The "DataSent" callback will pop when
            // some buffer space has freed up.
            NS_LOG_DEBUG("Unable to send packet; caching for later attempt");
            m_unsentPacket = packet;
            break;
        }
        else if (actual > 0 && (unsigned)actual < toSend)
        {
            // A Linux socket (non-blocking, such as in DCE) may return
            // a quantity less than the packet size.  Split the packet
            // into two, trace the sent packet, save the unsent packet
            NS_LOG_DEBUG("Packet size: " << packet->GetSize() << "; sent: " << actual
                                         << "; fragment saved: " << toSend - (unsigned)actual);
            Ptr<Packet> sent = packet->CreateFragment(0, actual);
            Ptr<Packet> unsent = packet->CreateFragment(actual, (toSend - (unsigned)actual));
            m_sentBytes[m_sendSockId] += actual;
            m_txTrace(sent);
            m_unsentPacket = unsent;
            break;
        }
        else
        {
            NS_FATAL_ERROR("Unexpected return value from m_socket->Send ()");
        }

        if (m_sentBytes[m_sendSockId] >= m_round * m_nSends[m_sendSockId] * m_sendSize)
        {
            m_sendSockId++;
            if (m_sendSockId >= m_sockets.size())
            {
                m_sendSockId = 0;
                m_round++;
            }
        }
    }
    // Close all sockets
    if (nFinished >= m_sockets.size())
    {
        for (uint32_t i = 0; i < m_sockets.size(); i++)
        {
            m_sockets[i]->Close();
            m_nConnected--;
        }
    }
}

void
FullmeshApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("FullmeshApplication Connection succeeded");
    m_nConnected++;
    if (m_nConnected >= m_sockets.size())
    {
        SendData();
    }
}

void
FullmeshApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("FullmeshApplication, Connection Failed");
}

void
FullmeshApplication::DataSend(Ptr<Socket> socket, uint32_t)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_nConnected >= m_sockets.size())
    { // Only send new data if the connection has completed
        SendData();
    }
}

} // Namespace ns3
