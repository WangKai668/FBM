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

#ifndef DRR_QUEUE_DISC_H
#define DRR_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3
{

/**
 * \ingroup traffic-control
 *
 * \brief A flow queue used by the DRR queue disc
 */

class DrrFlow : public QueueDiscClass
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * \brief DRRFlow constructor
     */
    DrrFlow();

    ~DrrFlow() override;

    /**
     * \brief Set the deficit for this flow
     * \param deficit the deficit for this flow
     */
    void SetDeficit(int32_t deficit);
    /**
     * \brief Get the deficit for this flow
     * \return the deficit for this flow
     */
    int32_t GetDeficit() const;
    /**
     * \brief Increase the deficit for this flow by a quantum
     */
    void IncreaseDeficit();

    /**
     * \brief Decrease the deficit for this flow
     * \param deficit the amount by which the deficit is to be decreased
     */
    void DecreaseDeficit(int32_t deficit);

    /**
     * \brief Set the quantum value.
     *
     * \param quantum The number of bytes each queue gets to dequeue on each round of the scheduling
     * algorithm
     */
    void SetQuantum(uint32_t quantum);

    /**
     * \brief Get the quantum value.
     *
     * \returns The number of bytes each queue gets to dequeue on each round of the scheduling
     * algorithm
     */
    uint32_t GetQuantum() const;

  private:
    int32_t m_deficit; //!< the deficit for this flow
    uint32_t m_quantum; //!< deficit assgned to this flow at each round
};

/**
 * \ingroup traffic-control
 *
 * \brief A DRR packet queue disc
 *
 * The DRR queue disc is a simple classful queueing discipline that can
 * contains an arbitrary number of classes of differing dificit weight.
 * The available bandwidth is distributed among classes proportionaly
 * to the weights.
 */
class DrrQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * \brief DrrQueueDisc constructor
     */
    DrrQueueDisc();

    ~DrrQueueDisc() override;

    // Reasons for dropping packets
    static constexpr const char* UNCLASSIFIED_DROP =
        "Unclassified drop"; //!< No packet filter able to classify packet

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    bool CheckConfig() override;
    void InitializeParams() override;

    uint32_t m_last; //!< last dequeued class id
};

} // namespace ns3

#endif /* DRR_QUEUE_DISC_H */
