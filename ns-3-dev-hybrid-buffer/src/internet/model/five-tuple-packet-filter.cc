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

#include "ns3/five-tuple-packet-filter.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/queue-item.h"
#include "ns3/tcp-header.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/udp-header.h"
#include "ns3/udp-l4-protocol.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FiveTuplePacketFilter");

NS_OBJECT_ENSURE_REGISTERED(FiveTuplePacketFilter);

TypeId
FiveTuplePacketFilter::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FiveTuplePacketFilter")
                            .SetParent<PacketFilter>()
                            .SetGroupName("TrafficControl")
                            .AddConstructor<FiveTuplePacketFilter>();
    return tid;
}

FiveTuplePacketFilter::FiveTuplePacketFilter()
{
    NS_LOG_FUNCTION(this);
}

FiveTuplePacketFilter::~FiveTuplePacketFilter()
{
    NS_LOG_FUNCTION(this);
}

bool
FiveTuplePacketFilter::CheckProtocol(Ptr<QueueDiscItem> item) const
{
    return true;
}

int32_t
FiveTuplePacketFilter::DoClassify(Ptr<QueueDiscItem> item) const
{
    // extract the five tuple
    Ptr<Packet> copy = item->GetPacket()->Copy();
    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem>(item);
    Ipv4Header iph = ipv4Item->GetHeader();
    uint8_t prot = iph.GetProtocol();

    // concrol packet to default class
    if (prot == 0x01)
    {
        return 0;
    }
    Ipv4Address srcAddr = iph.GetSource();
    Ipv4Address dstAddr = iph.GetDestination();
    uint16_t fragOffset = iph.GetFragmentOffset();

    // remove the ipv4 header to parse L4 header correctly
    Ipv4Header temp;
    copy->RemoveHeader(temp);

    TcpHeader tcpHdr;
    UdpHeader udpHdr;
    uint16_t srcPort = 0;
    uint16_t dstPort = 0;

    if (prot == TcpL4Protocol::PROT_NUMBER && fragOffset == 0) // TCP
    {
        copy->RemoveHeader(tcpHdr);
        srcPort = tcpHdr.GetSourcePort();
        dstPort = tcpHdr.GetDestinationPort();
    }
    else if (prot == UdpL4Protocol::PROT_NUMBER && fragOffset == 0) // UDP
    {
        copy->RemoveHeader(udpHdr);
        srcPort = udpHdr.GetSourcePort();
        dstPort = udpHdr.GetDestinationPort();
    }
    if (prot != TcpL4Protocol::PROT_NUMBER && prot != UdpL4Protocol::PROT_NUMBER)
    {
        NS_LOG_WARN("Unknown transport protocol");
    }

    for (uint32_t i = 0; i < m_rules.size(); i++)
    {
        if (IsProtocolMatch(i, prot) && IsSrcAddrMatch(i, srcAddr) && IsDstAddrMatch(i, dstAddr) &&
            IsSrcPortMatch(i, srcPort) && IsDstPortMatch(i, dstPort))
        {
            return GetClass(i);
        }
    }
    NS_LOG_ERROR("No matched rule");
    return PacketFilter::PF_NO_MATCH;
}

void
FiveTuplePacketFilter::AddClassifyRule(uint8_t proto,
                                       Ipv4Address srcAddress,
                                       Ipv4Address dstAddress,
                                       Ipv4Mask srcMask,
                                       Ipv4Mask dstMask,
                                       uint16_t srcPortLow,
                                       uint16_t srcPortHigh,
                                       uint16_t dstPortLow,
                                       uint16_t dstPortHigh,
                                       int32_t cls)
{
    struct Ipv4Addr srcAddr;
    struct Ipv4Addr dstAddr;
    srcAddr.address = srcAddress;
    srcAddr.mask = srcMask;
    dstAddr.address = dstAddress;
    dstAddr.mask = dstMask;

    struct PortRange srcPortRange;
    struct PortRange dstPortRange;
    srcPortRange.portLow = srcPortLow;
    srcPortRange.portHigh = srcPortHigh;
    dstPortRange.portLow = dstPortLow;
    dstPortRange.portHigh = dstPortHigh;

    struct ClassifyRule rule;
    rule.protocol = proto;
    rule.srcAddr = srcAddr;
    rule.dstAddr = dstAddr;
    rule.srcPortRange = srcPortRange;
    rule.dstPortRange = dstPortRange;
    rule.cls = cls;

    m_rules.push_back(rule);

    // Set two-way rules for TCP flow
    if (proto == TcpL4Protocol::PROT_NUMBER)
    {
        rule.srcAddr = dstAddr;
        rule.dstAddr = srcAddr;
        rule.srcPortRange = dstPortRange;
        rule.dstPortRange = srcPortRange;
        m_rules.push_back(rule);
    }
    
}

bool
FiveTuplePacketFilter::IsSrcAddrMatch(uint32_t index, Ipv4Address srcAddress) const
{
    NS_LOG_INFO("src addr check match: pkt=" << srcAddress
                                             << " rule=" << m_rules[index].srcAddr.address << "/"
                                             << m_rules[index].srcAddr.mask);
    if (srcAddress.CombineMask(m_rules[index].srcAddr.mask) == m_rules[index].srcAddr.address)
    {
        return true;
    }
    NS_LOG_INFO("NOT OK!");
    return false;
}

bool
FiveTuplePacketFilter::IsDstAddrMatch(uint32_t index, Ipv4Address dstAddress) const
{
    NS_LOG_INFO("dst addr check match: pkt=" << dstAddress
                                             << " rule=" << m_rules[index].dstAddr.address << "/"
                                             << m_rules[index].dstAddr.mask);
    if (dstAddress.CombineMask(m_rules[index].dstAddr.mask) == m_rules[index].dstAddr.address)
    {
        return true;
    }
    NS_LOG_INFO("NOT OK!");
    return false;
}

bool
FiveTuplePacketFilter::IsSrcPortMatch(uint32_t index, uint16_t srcPort) const
{
    NS_LOG_INFO("src port check match: pkt=" << srcPort << " rule= ["
                                             << m_rules[index].srcPortRange.portLow << " TO "
                                             << m_rules[index].srcPortRange.portHigh << "]");
    if (srcPort >= m_rules[index].srcPortRange.portLow &&
        srcPort <= m_rules[index].srcPortRange.portHigh)
    {
        return true;
    }
    NS_LOG_INFO("NOT OK!");
    return false;
}

bool
FiveTuplePacketFilter::IsDstPortMatch(uint32_t index, uint16_t dstPort) const
{
    NS_LOG_INFO("dst port check match: pkt=" << dstPort << " rule= ["
                                             << m_rules[index].dstPortRange.portLow << " TO "
                                             << m_rules[index].dstPortRange.portHigh << "]");
    if (dstPort >= m_rules[index].dstPortRange.portLow &&
        dstPort <= m_rules[index].dstPortRange.portHigh)
    {
        return true;
    }
    NS_LOG_INFO("NOT OK!");
    return false;
}

bool
FiveTuplePacketFilter::IsProtocolMatch(uint32_t index, uint8_t proto) const
{
    NS_LOG_INFO("proto check match: pkt=" << (uint16_t)proto
                                          << " rule=" << (uint16_t)(m_rules[index].protocol));
    if (proto == m_rules[index].protocol)
    {
        return true;
    }
    NS_LOG_INFO("NOT OK!");
    return false;
}

int32_t
FiveTuplePacketFilter::GetClass(uint32_t index) const
{
    return m_rules[index].cls;
}
} // namespace ns3
