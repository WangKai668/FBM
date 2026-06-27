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
 * Author: Danfeng Shan <dfshan@xjtu.edu.cn>
 */

#ifndef FULLMESH_APPLICATION_H
#define FULLMESH_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/queue-item.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Address;
class Socket;

/**
 * \ingroup applications
 * \defgroup fullmesh FullmeshApplication
 *
 * This traffic generator TODO: description
 */

/**
 * \ingroup fullmesh
 *
 * \brief TODO
 *
 * TODO: description
 */
class FullmeshApplication : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    FullmeshApplication();

    ~FullmeshApplication() override;

    /**
     * \brief Set the upper bound for the total number of bytes to send.
     *
     * Once this bound is reached, no more application bytes are sent. If the
     * application is stopped during the simulation and restarted, the
     * total number of bytes sent is not reset; however, the maxBytes
     * bound is still effective and the application will continue sending
     * up to maxBytes. The value zero for maxBytes means that
     * there is no upper bound; i.e. data is sent until the application
     * or simulation is stopped.
     *
     * \param maxBytes the upper bound of bytes to send
     */
    void SetMaxBytes(uint64_t maxBytes);

    /**
     * \brief Get the i'th socket this application is attached to.
     * \return pointer to i'th associated socket
     */
    Ptr<Socket> GetSocket(uint32_t i) const;
    /**
     * \brief Add a remote node
     *
     * \params Remote address
     * \params Weight of sending packets
     * For example, if the weight is 1:2:3, socket 1, socket 2, socket 3
     * will successively send 1 packet, 2 packets, and send 3 packets,
     * respectively.
     */
    void AddRemote(const Address& addr, uint32_t weight = 1);
    /**
     * Weight of sending packets
     * For example, if the weight is 1:2:3, socket 1, socket 2, socket 3
     * will successively send 1 packet, 2 packets, and send 3 packets,
     * respectively.
     */
    // void SetSendWeight(std::vector<uint32_t> weights);

  protected:
    void DoDispose() override;

  private:
    // inherited from Application base class.
    void StartApplication() override; // Called at time specified by Start
    void StopApplication() override;  // Called at time specified by Stop

    void InvokeSendData(Ptr<const QueueDiscItem> item);
    void SetupSmallQueue(Ptr<Socket> socket);

    /**
     * \brief Check whether the queue length exceeds the limit
     * \param socket
     * \return True if the device queue / qdisc queue is full
     */
    bool IsQueueSmall(Ptr<Socket> socket);

    /**
     * \brief Send data until the L4 transmission buffer is full.
     * \param from From address
     * \param to To address
     */
    virtual void SendData();

    std::vector<Ptr<Socket>> m_sockets; //!< Associated socket
    std::vector<Address> m_peers;       //!< Peer addresses
    uint32_t m_sendSize;                //!< Size of data to send each time
    uint32_t m_limitOutputBytes; //!< Limit of queued bytes inside network stack (mimic Linux TSQ)
    std::vector<uint32_t> m_nSends;    //!< Number of packets to send each time
    std::uint64_t m_maxBytes;          //!< Limit total number of bytes sent
    std::vector<uint64_t> m_sentBytes; //!< Total bytes sent so far
    TypeId m_tid;                      //!< The type of protocol to use.
    Ptr<Packet> m_unsentPacket;        //!< Variable to cache unsent packet
    uint32_t m_nConnected;             //!< Number of connected sockets
    uint32_t m_round;                  //!< Round number
    uint32_t m_sendSockId;             //!< Socket id to send the packet
    bool m_isBlocked;                  //!< Whether this application is blocked by small queue

    /// Traced Callback: sent packets
    TracedCallback<Ptr<const Packet>> m_txTrace;

  private:
    /**
     * \brief Connection Succeeded (called by Socket through a callback)
     * \param socket the connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Connection Failed (called by Socket through a callback)
     * \param socket the connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);
    /**
     * \brief Send more data as soon as some has been transmitted.
     *
     * Used in socket's SetSendCallback - params are forced by it.
     *
     * \param socket socket to use
     * \param unused actually unused
     */
    virtual void DataSend(Ptr<Socket> socket, uint32_t unused);
};

} // namespace ns3

#endif /* BULK_SEND_APPLICATION_H */
