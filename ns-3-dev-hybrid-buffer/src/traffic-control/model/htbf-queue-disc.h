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

#ifndef HTBF_QUEUE_DISC_H
#define HTBF_QUEUE_DISC_H

#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/random-variable-stream.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/traced-value.h"

namespace ns3
{

/**
 * \ingroup traffic-control
 *
 * \brief A hierachical TBF packet queue disc
 *
 * Hierachical TBF packet queue disc is a classful queue disc which extends
 * classless TBF queue disc to support hierachical TBF scheduling.
 */
class HtbfQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief HtbfQueueDisc Constructor
     *
     * Create a hierachical TBF queue disc
     */
    HtbfQueueDisc();

    /**
     * \brief Destructor
     *
     * Destructor
     */
    ~HtbfQueueDisc() override;

    /**
     * \brief Set the size of the first bucket in bytes.
     *
     * \param burst The size of first bucket in bytes.
     */
    void SetBurst(uint32_t burst);

    /**
     * \brief Get the size of the first bucket in bytes.
     *
     * \returns The size of the first bucket in bytes.
     */
    uint32_t GetBurst() const;

    /**
     * \brief Set the size of the second bucket in bytes.
     *
     * \param mtu The size of second bucket in bytes.
     */
    void SetMtu(uint32_t mtu);

    /**
     * \brief Get the size of the second bucket in bytes.
     *
     * \returns The size of the second bucket in bytes.
     */
    uint32_t GetMtu() const;

    /**
     * \brief Set the rate of the tokens entering the first bucket.
     *
     * \param rate The rate of first bucket tokens.
     */
    void SetRate(DataRate rate);

    /**
     * \brief Get the rate of the tokens entering the first bucket.
     *
     * \returns The rate of first bucket tokens.
     */
    DataRate GetRate() const;

    /**
     * \brief Set the rate of the tokens entering the second bucket.
     *
     * \param peakRate The rate of second bucket tokens.
     */
    void SetPeakRate(DataRate peakRate);

    /**
     * \brief Get the rate of the tokens entering the second bucket.
     *
     * \returns The rate of second bucket tokens.
     */
    DataRate GetPeakRate() const;

    /**
     * \brief Get the current number of tokens inside the first bucket in bytes.
     *
     * \returns The number of first bucket tokens in bytes.
     */
    uint32_t GetFirstBucketTokens() const;

    /**
     * \brief Get the current number of tokens inside the second bucket in bytes.
     *
     * \returns The number of second bucket tokens in bytes.
     */
    uint32_t GetSecondBucketTokens() const;

    /**
     * \brief Set the root qdisc.
     *
     * If this qdisc is not root qdisc, root qdisc should be recorded to
     * wake the Run method when token is enough. If this qdisc is that
     * root qdisc, set to itself.
     *
     * \param qd Ptr of the root qdisc.
     * \returns Whether this action is successful.
     */
    bool SetRootQdisc(Ptr<QueueDisc> qd);

    // Reasons for dropping packets
    static constexpr const char* UNCLASSIFIED_DROP =
        "Unclassified drop"; //!< No packet filter able to classify packet

  protected:
    /**
     * \brief Dispose of the object
     */
    void DoDispose() override;

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    bool CheckConfig() override;
    void InitializeParams() override;

    /* parameters for the hierachical TBF Queue Disc */
    uint32_t m_burst;      //!< Size of first bucket in bytes
    uint32_t m_mtu;        //!< Size of second bucket in bytes
    DataRate m_rate;       //!< Rate at which tokens enter the first bucket
    DataRate m_peakRate;   //!< Rate at which tokens enter the second bucket
    Ptr<QueueDisc> m_root; //!< Ptr of root queue disc

    /* variables stored by hierachical TBF Queue Disc */
    TracedValue<uint32_t> m_btokens; //!< Current number of tokens in first bucket
    TracedValue<uint32_t> m_ptokens; //!< Current number of tokens in second bucket
    Time m_timeCheckPoint;           //!< Time check-point
    EventId m_id; //!< EventId of the scheduled queue waking event when enough tokens are available
    uint32_t m_last; //!< last dequeued class id
};

} // namespace ns3

#endif /* HTBF_QUEUE_DISC_H */
