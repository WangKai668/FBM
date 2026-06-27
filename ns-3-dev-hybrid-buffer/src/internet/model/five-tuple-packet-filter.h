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

#ifndef FIVE_TUPLE_PACKET_FILTER
#define FIVE_TUPLE_PACKET_FILTER

#include "ns3/ipv4-address.h"
#include "ns3/packet-filter.h"
#include "ns3/queue-disc.h"

namespace ns3
{

/**
 * \ingroup traffic-control
 *
 * Packet filter to classify packets based on five tuples.
 *
 */
class FiveTuplePacketFilter : public PacketFilter
{
  public:
    /**
     * \brief Get the type ID.
     *
     * \returns the object TypeId.
     */
    static TypeId GetTypeId();
    /**
     * \brief FiveTuplePacketFilter constructor.
     *
     * Creates a FiveTuplePacketFilter by default.
     */
    FiveTuplePacketFilter();
    /**
     * \brief Add a classify rule.
     *
     * \param proto protocol number in this rule.
     * \param srcAddress source address in this rule.
     * \param dstAddress destination address in this rule.
     * \param srcMask source net mask in this rule.
     * \param dstMask destination net mask in this rule.
     * \param srcPortLow source port low boundry in this rule.
     * \param srcPortHigh source port high boundry in this rule.
     * \param dstPortLow destination port low boundry in this rule.
     * \param dstPortHigh destination port high boundry in this rule.
     * \param cls class in this rule.
     */
    void AddClassifyRule(uint8_t proto,
                         Ipv4Address srcAddress,
                         Ipv4Address dstAddress,
                         Ipv4Mask srcMask,
                         Ipv4Mask dstMask,
                         uint16_t srcPortLow,
                         uint16_t srcPortHigh,
                         uint16_t dstPortLow,
                         uint16_t dstPortHigh,
                         int32_t cls);

    ~FiveTuplePacketFilter() override;

  private:
    /**
     * \brief Checks if the filter is able to classify a kind of items.
     *
     * \param item an example item to check.
     * \returns true if this filter is able to classify packets.
     */
    bool CheckProtocol(Ptr<QueueDiscItem> item) const override;
    /**
     * \brief Classify a packet.
     *
     * \param item the packet to classify.
     * \returns -1 if the item does not match the filter conditions, or the configured
     * return value otherwise.
     */
    int32_t DoClassify(Ptr<QueueDiscItem> item) const override;
    /**
     * \brief Check whether the source address matches the rule.
     *
     * \param index index of the rule to check.
     * \param srcAddress source IP address to check.
     * \returns true if a match.
     */
    bool IsSrcAddrMatch(uint32_t index, Ipv4Address srcAddress) const;
    /**
     * \brief Check whether the destination address matches the rule.
     *
     * \param index index of the rule to check.
     * \param dstAddress destination IP address to check.
     * \returns true if a match.
     */
    bool IsDstAddrMatch(uint32_t index, Ipv4Address dstAddress) const;
    /**
     * \brief Check whether the source port matches the rule.
     *
     * \param index index of the rule to check.
     * \param srcPort source port to check.
     * \returns true if a match.
     */
    bool IsSrcPortMatch(uint32_t index, uint16_t srcPort) const;
    /**
     * \brief Check whether the destination port matches the rule.
     *
     * \param index index of the rule to check.
     * \param dstPort destination port to check.
     * \returns true if a match.
     */
    bool IsDstPortMatch(uint32_t index, uint16_t dstPort) const;
    /**
     * \brief Check whether protocol matches the rule.
     *
     * \param index index of the rule to check.
     * \param proto protocol number to check.
     * \returns true if a match.
     */
    bool IsProtocolMatch(uint32_t index, uint8_t proto) const;
    /**
     * \brief Get class from index of rule.
     *
     * \param index the index of rule.
     * \returns class in this rule.
     */
    int32_t GetClass(uint32_t index) const;

    // PortRange structure
    struct PortRange
    {
        uint16_t portLow;  //!< port low
        uint16_t portHigh; //!< port high
    };

    // Ipv4Addr structure
    struct Ipv4Addr
    {
        Ipv4Address address; //!< IP address
        Ipv4Mask mask;       //!< net mask
    };

    // ClassifyRule structure
    struct ClassifyRule
    {
        uint8_t protocol;              //!< protocol
        struct Ipv4Addr srcAddr;       //!< source address
        struct Ipv4Addr dstAddr;       //!< destination address
        struct PortRange srcPortRange; //!< source port range
        struct PortRange dstPortRange; //!< destination port range
        int32_t cls;                   //!< class
    };

    std::vector<struct ClassifyRule> m_rules; //!< classify rules
};

} // namespace ns3

#endif /* FIVE_TUPLE_PACKET_FILTER_H */
