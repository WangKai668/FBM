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

#ifndef POINT_TO_POINT_REORDER_NET_DEVICE_H
#define POINT_TO_POINT_REORDER_NET_DEVICE_H

#include "ns3/point-to-point-net-device.h"
#include "ns3/ptr.h"
#include "ns3/switch-mmu.h"
#include "ns3/traffic-control-layer.h"

#include <cstring>
#include <unordered_map>

namespace ns3
{

class PointToPointChannel;
class ErrorModel;

/**
 * \ingroup point-to-point
 * \class PointToPointReorderNetDevice
 * \brief A Device for a Point to Point Network Link with reorder function.
 *
 * This PointToPointReorderNetDevice extends the PointToPointNetDevice to
 * provide reorder function for hybrid buffer system. It adds a flag for
 * each packet in the queue to indicate whether the packet has been read
 * from the buffer.
 */
class PointToPointReorderNetDevice : public PointToPointNetDevice
{
  public:
    /**
     * \brief Get the TypeId
     *
     * \return The TypeId for this class
     */
    static TypeId GetTypeId();

    /**
     * Construct a PointToPointReorderNetDevice
     *
     * This is the constructor for the PointToPointReorderNetDevice. It takes as a
     * parameter a pointer to the Node to which this device is connected,
     * as well as an optional DataRate object.
     */
    PointToPointReorderNetDevice();

    /**
     * Destroy a PointToPointReorderNetDevice
     *
     * This is the destructor for the PointToPointReorderNetDevice.
     */
    ~PointToPointReorderNetDevice() override;

    /**
     * Called from higher layer to send packet into Network Device
     * to the specified destination Address.
     *
     * Before dequeue a packet from inner queue, this method check whether
     * the packet has been read from the buffer. If true, transmit this
     * packet, otherwise, give up the transimission even there are read
     * packets behind it.
     *
     * \param packet packet sent from above down to Network Device
     * \param dest mac address of the destination (already resolved)
     * \param protocolNumber identifies the type of payload contained in
     *        this packet. Used to call the right L3Protocol when the packet
     *        is received.
     * \return whether the Send operation succeeded
     */
    bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber) override;

    /**
     * Set the ptr of the traffic control layer.
     *
     * This method is used in configuration.
     * \param tc Ptr of the traffic control layer
     */
    void SetTc(Ptr<TrafficControlLayer> tc);

    /**
     * Get the ptr of the traffic control layer.
     *
     * This method is set to access the mmu in traffic control layer.
     *
     * \return Ptr of the traffic control layer
     */
    Ptr<TrafficControlLayer> GetTc();

    /**
     * Attempt to transmit a packet
     *
     * This method should be called explicitly when a packet is become
     * ready to send.
     *
     * \return whether the resend operation succeeded
     */
    bool AttemptTransmission();

    /**
     * \brief Make the link up and running
     *
     * It calls also the linkChange callback. To explicitly trigger the transmission,
     * we also add the AttemptTransmission into DeviceHandler of mmu in this method.
     */
    void NotifyLinkUp() override;

    /**
     * Attach the device to a channel.
     *
     * This method is overwrited to call the overwrited NotifyLinkUp.
     *
     * \param ch Ptr to the channel to which this object is being attached.
     * \return true if the operation was successful (always true actually)
     */
    bool Attach(Ptr<PointToPointChannel> ch) override;

    void SetNode(Ptr<Node> node) override;

    void SetMmu(Ptr<SwitchMmu> mmu);

    Ptr<SwitchMmu> GetMmu();

    void TransmitComplete() override;

    bool TransmitStart(Ptr<Packet> p) override;

    void LinkDown();

    void Receive(Ptr<Packet> packet) override;

  private:
    /**
     * \brief Assign operator
     *
     * The method is private, so it is DISABLED.
     *
     * \param o Other NetDevice
     * \return New instance of the NetDevice
     */
    PointToPointReorderNetDevice& operator=(const PointToPointReorderNetDevice& o);

    /**
     * \brief Copy constructor
     *
     * The method is private, so it is DISABLED.

     * \param o Other NetDevice
     */
    PointToPointReorderNetDevice(const PointToPointReorderNetDevice& o);

    /**
     * \brief Dispose of the object
     */
    void DoDispose() override;

  private:
    Ptr<TrafficControlLayer> m_tc; //!< Ptr of the traffic control layer to use switch mmu

    /**
     * SwitchMmu attached to this device
     */
    Ptr<SwitchMmu> m_mmu;

    bool m_enableMulticast;  //!< Enable multicast for each ingress port
    uint32_t m_copyNums;     //!< Number of copies
    bool flag_print = 0;
};

} // namespace ns3

#endif /* REORDER_POINT_TO_POINT_NET_DEVICE_H */
