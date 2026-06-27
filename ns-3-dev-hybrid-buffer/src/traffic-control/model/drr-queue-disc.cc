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

#include "drr-queue-disc.h"

#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DrrQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(DrrFlow);

TypeId
DrrFlow::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DrrFlow")
                            .SetParent<QueueDiscClass>()
                            .SetGroupName("TrafficControl")
                            .AddConstructor<DrrFlow>();
    return tid;
}

DrrFlow::DrrFlow()
    : m_deficit(0),
      m_quantum(1024)
{
    NS_LOG_FUNCTION(this);
}

DrrFlow::~DrrFlow()
{
    NS_LOG_FUNCTION(this);
}

void
DrrFlow::SetDeficit(int32_t deficit)
{
    NS_LOG_FUNCTION(this << deficit);
    m_deficit = deficit;
}

int32_t
DrrFlow::GetDeficit() const
{
    NS_LOG_FUNCTION(this);
    return m_deficit;
}

void
DrrFlow::IncreaseDeficit()
{
    m_deficit += m_quantum * 500;
}

void
DrrFlow::DecreaseDeficit(int32_t deficit)
{
    NS_LOG_FUNCTION(this << deficit);
    m_deficit -= deficit;
}

void
DrrFlow::SetQuantum(uint32_t quantum)
{
    NS_LOG_FUNCTION(this << quantum);
    m_quantum = quantum;
}

uint32_t
DrrFlow::GetQuantum() const
{
    return m_quantum;
}

NS_OBJECT_ENSURE_REGISTERED(DrrQueueDisc);

TypeId
DrrQueueDisc::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DrrQueueDisc")
                            .SetParent<QueueDisc>()
                            .SetGroupName("TrafficControl")
                            .AddConstructor<DrrQueueDisc>();
    return tid;
}

DrrQueueDisc::DrrQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS),
      m_last(0)
{
    NS_LOG_FUNCTION(this);
}

DrrQueueDisc::~DrrQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

bool
DrrQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    uint32_t flowId = GetNQueueDiscClasses();

    int32_t ret = Classify(item);

    if (ret != PacketFilter::PF_NO_MATCH)
    {
        NS_LOG_DEBUG("Packet filters returned " << ret);

        if (ret >= 0 && static_cast<uint32_t>(ret) < GetNQueueDiscClasses())
        {
            flowId = static_cast<uint32_t>(ret);
        }
    }
    else
    {
        NS_LOG_ERROR("No filter has been able to classify this packet, drop it.");
        DropBeforeEnqueue(item, UNCLASSIFIED_DROP);
        return false;
    }
    bool retval = GetQueueDiscClass(flowId)->GetQueueDisc()->Enqueue(item);

    // If Queue::Enqueue fails, QueueDisc::Drop is called by the child queue disc
    NS_LOG_LOGIC("Number packets class "
                 << flowId << ": " << GetQueueDiscClass(flowId)->GetQueueDisc()->GetNPackets());

    return retval;
}

Ptr<QueueDiscItem>
DrrQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<DrrFlow> flow;
    Ptr<QueueDiscItem> item = 0;

    if (GetNPackets() > 0 && GetNQueueDiscClasses() > 0)
    {
        uint32_t curIndex = (m_last + 1) % GetNQueueDiscClasses();
        // for (uint32_t i = 1; i <= GetNQueueDiscClasses(); i++)
        while (!item)
        {
            // uint32_t curIndex = (m_last + i) % GetNQueueDiscClasses();
            // use the last dequeued class to begin from next class in the next round
            flow = StaticCast<DrrFlow>(GetQueueDiscClass(curIndex));
            if (flow->GetQueueDisc()->GetNPackets() > 0)
            {
                flow->IncreaseDeficit();    // Only increase the deficit when there are packets
                                            // in a queue
                if (flow->GetDeficit() > 0)
                {
                    item = flow->GetQueueDisc()->Dequeue();
                    // when deficit is enough but no item dequeued, round to next index
                    if (item)
                    {
                        flow->DecreaseDeficit(item->GetSize());
                        NS_LOG_DEBUG("Dequeued packet " << item->GetPacket());
                        m_last = curIndex;
                        return item;
                    }
                    else
                    {
                        flow->SetDeficit(0);
                        return item;
                    }
		        }
            }
            else
            {
                flow->SetDeficit(0);
            }
            curIndex = (curIndex + 1) % GetNQueueDiscClasses();
        }
    }

    return nullptr;
}

bool
DrrQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNQueueDiscClasses() == 0)
    {
        NS_LOG_ERROR("DrrQueueDisc must have classes");
        return false;
    }

    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("DrrQueueDisc cannot have internal queues");
        return false;
    }

    if (GetNPacketFilters() == 0)
    {
        NS_LOG_ERROR("DrrQueueDisc must have at least one packet filter");
        return false;
    }
    return true;
}

void
DrrQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
}
} // namespace ns3
