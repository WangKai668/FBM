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

#include "htbf-queue-disc.h"

#include "ns3/attribute.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("HtbfQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(HtbfQueueDisc);

TypeId
HtbfQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::HtbfQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<HtbfQueueDisc>()
            .AddAttribute("Burst",
                          "Size of the first bucket in bytes",
                          UintegerValue(125000),
                          MakeUintegerAccessor(&HtbfQueueDisc::SetBurst),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Mtu",
                          "Size of the second bucket in bytes. If null, it is initialized"
                          " to the MTU of the receiving NetDevice (if any)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&HtbfQueueDisc::SetMtu),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Rate",
                          "Rate at which tokens enter the first bucket in bps or Bps.",
                          DataRateValue(DataRate("125KB/s")),
                          MakeDataRateAccessor(&HtbfQueueDisc::SetRate),
                          MakeDataRateChecker())
            .AddAttribute("PeakRate",
                          "Rate at which tokens enter the second bucket in bps or Bps."
                          "If null, there is no second bucket",
                          DataRateValue(DataRate("0KB/s")),
                          MakeDataRateAccessor(&HtbfQueueDisc::SetPeakRate),
                          MakeDataRateChecker())
            .AddAttribute("RootQdisc",
                          "Ptr of root queue disc. If null, this is the root qdisc.",
                          PointerValue(),
                          MakePointerAccessor(&HtbfQueueDisc::m_root),
                          MakePointerChecker<QueueDisc>())
            .AddTraceSource("TokensInFirstBucket",
                            "Number of First Bucket Tokens in bytes",
                            MakeTraceSourceAccessor(&HtbfQueueDisc::m_btokens),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("TokensInSecondBucket",
                            "Number of Second Bucket Tokens in bytes",
                            MakeTraceSourceAccessor(&HtbfQueueDisc::m_ptokens),
                            "ns3::TracedValueCallback::Uint32");

    return tid;
}

HtbfQueueDisc::HtbfQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS),
      m_root(this)
{
    NS_LOG_FUNCTION(this);
}

HtbfQueueDisc::~HtbfQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
HtbfQueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    QueueDisc::DoDispose();
}

void
HtbfQueueDisc::SetBurst(uint32_t burst)
{
    NS_LOG_FUNCTION(this << burst);
    m_burst = burst;
}

uint32_t
HtbfQueueDisc::GetBurst() const
{
    NS_LOG_FUNCTION(this);
    return m_burst;
}

void
HtbfQueueDisc::SetMtu(uint32_t mtu)
{
    NS_LOG_FUNCTION(this << mtu);
    m_mtu = mtu;
}

uint32_t
HtbfQueueDisc::GetMtu() const
{
    NS_LOG_FUNCTION(this);
    return m_mtu;
}

void
HtbfQueueDisc::SetRate(DataRate rate)
{
    NS_LOG_FUNCTION(this << rate);
    m_rate = rate;
}

DataRate
HtbfQueueDisc::GetRate() const
{
    NS_LOG_FUNCTION(this);
    return m_rate;
}

void
HtbfQueueDisc::SetPeakRate(DataRate peakRate)
{
    NS_LOG_FUNCTION(this << peakRate);
    m_peakRate = peakRate;
}

DataRate
HtbfQueueDisc::GetPeakRate() const
{
    NS_LOG_FUNCTION(this);
    return m_peakRate;
}

uint32_t
HtbfQueueDisc::GetFirstBucketTokens() const
{
    NS_LOG_FUNCTION(this);
    return m_btokens;
}

uint32_t
HtbfQueueDisc::GetSecondBucketTokens() const
{
    NS_LOG_FUNCTION(this);
    return m_ptokens;
}

bool
HtbfQueueDisc::SetRootQdisc(Ptr<QueueDisc> qd)
{
    NS_LOG_FUNCTION(this);
    m_root = qd;
    return true;
}

bool
HtbfQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    uint32_t classId = GetNQueueDiscClasses();
    int32_t ret = Classify(item);
    if (ret != PacketFilter::PF_NO_MATCH)
    {
        NS_LOG_DEBUG("Packet filters returned " << ret);

        if (ret >= 0 && static_cast<uint32_t>(ret) < GetNQueueDiscClasses())
        {
            classId = static_cast<uint32_t>(ret);
        }
    }
    else
    {
        NS_LOG_ERROR("No filter has been able to classify this packet, drop it.");
        DropBeforeEnqueue(item, UNCLASSIFIED_DROP);

        return false;
    }

    NS_ASSERT_MSG(classId < GetNQueueDiscClasses(), "Selected class out of range");
    bool retval = GetQueueDiscClass(classId)->GetQueueDisc()->Enqueue(item);

    return retval;
}

Ptr<QueueDiscItem>
HtbfQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<QueueDiscClass> cls;
    Ptr<QueueDiscItem> item;

    // round robin to decide which class to dequeue
    for (uint32_t i = 0; i < GetNQueueDiscClasses(); i++)
    {
        // use the last dequeued class to begin from next class in the next round
        uint32_t cur_index = (m_last + 1 + i) % GetNQueueDiscClasses();
        cls = GetQueueDiscClass(cur_index);

        Ptr<const QueueDiscItem> itemPeek = cls->GetQueueDisc()->Peek();
        if (itemPeek)
        {
            uint32_t pktSize = itemPeek->GetSize();
            NS_LOG_LOGIC("Next packet size " << pktSize);

            int64_t btoks = 0;
            int64_t ptoks = 0;
            Time now = Simulator::Now();

            double delta = (now - m_timeCheckPoint).GetSeconds();
            NS_LOG_LOGIC("Time Difference delta " << delta);

            if (m_peakRate > DataRate("0bps"))
            {
                ptoks = m_ptokens + round(delta * (m_peakRate.GetBitRate() / 8));
                if (ptoks > m_mtu)
                {
                    ptoks = m_mtu;
                }
                NS_LOG_LOGIC("Number of ptokens we can consume " << ptoks);
                NS_LOG_LOGIC("Required to dequeue next packet " << pktSize);
                ptoks -= pktSize;
            }

            btoks = m_btokens + round(delta * (m_rate.GetBitRate() / 8));

            if (btoks > m_burst)
            {
                btoks = m_burst;
            }

            NS_LOG_LOGIC("Number of btokens we can consume " << btoks);
            NS_LOG_LOGIC("Required to dequeue next packet " << pktSize);
            btoks -= pktSize;

            if ((btoks | ptoks) >= 0) // else packet blocked
            {
                Ptr<QueueDiscItem> item = cls->GetQueueDisc()->Dequeue();
                if (!item)
                {
                    NS_LOG_DEBUG("That's odd! Expecting the peeked packet, we got no packet.");
                    return item;
                }

                m_timeCheckPoint = now;
                m_btokens = btoks;
                m_ptokens = ptoks;

                NS_LOG_LOGIC(m_btokens << " btokens and " << m_ptokens
                                       << " ptokens after packet dequeue");

                // Only if the packet is dequeued, the next class will get the turn
                // from round robin. This is important to block the following classes
                // when this class has set a timer and the timer is not expired.
                m_last = cur_index;
                return item;
            }
            else
            {
                // the watchdog timer setup.
                // A packet gets blocked if the above if() condition is not satisfied:
                // either or both btoks and ptoks are negative.  In that case, we have
                // to schedule the waking of root qdisc when enough tokens are available.
                if (m_id.IsExpired() == true)
                {
                    NS_ASSERT_MSG(m_rate.GetBitRate() > 0, "Rate must be positive");
                    Time requiredDelayTime;
                    if (m_peakRate.GetBitRate() == 0)
                    {
                        NS_ASSERT_MSG(btoks < 0, "Logic error; btoks must be < 0 here");
                        requiredDelayTime = m_rate.CalculateBytesTxTime(-btoks);
                    }
                    else
                    {
                        if (btoks < 0 && ptoks >= 0)
                        {
                            requiredDelayTime = m_rate.CalculateBytesTxTime(-btoks);
                        }
                        else if (btoks >= 0 && ptoks < 0)
                        {
                            requiredDelayTime = m_peakRate.CalculateBytesTxTime(-ptoks);
                        }
                        else
                        {
                            requiredDelayTime = std::max(m_rate.CalculateBytesTxTime(-btoks),
                                                         m_peakRate.CalculateBytesTxTime(-ptoks));
                        }
                    }
                    NS_ASSERT_MSG(requiredDelayTime.GetSeconds() >= 0, "Negative time");
                    if (m_root)
                    {
                        m_id = Simulator::Schedule(requiredDelayTime, &QueueDisc::Run, m_root);
                        NS_LOG_LOGIC("Waking Event Scheduled in " << requiredDelayTime.As(Time::S));
                    }
                }
                return 0;
            }
        }
    }
    return 0;
}

bool
HtbfQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("HtbfQueueDisc cannot have internal queues");
        return false;
    }

    if (GetNPacketFilters() == 0)
    {
        NS_LOG_ERROR("HtbfQueueDisc must have packet filters");
        return false;
    }

    if (GetNQueueDiscClasses() == 0)
    {
        NS_LOG_ERROR("HtbfQueueDisc must set at least 1 child queue disc");
        return false;
    }

    // This type of variable initialization would normally be done in
    // InitializeParams (), but we want to use the value to subsequently
    // check configuration of peak rate, so we move it forward here.
    if (m_mtu == 0)
    {
        Ptr<NetDeviceQueueInterface> ndqi = GetNetDeviceQueueInterface();
        Ptr<NetDevice> dev;
        // if the NetDeviceQueueInterface object is aggregated to a
        // NetDevice, get the MTU of such NetDevice
        if (ndqi && (dev = ndqi->GetObject<NetDevice>()))
        {
            m_mtu = dev->GetMtu();
        }
    }

    if (m_mtu == 0 && m_peakRate > DataRate("0bps"))
    {
        NS_LOG_ERROR(
            "A non-null peak rate has been set, but the mtu is null. No packet will be dequeued");
        return false;
    }

    if (m_burst <= m_mtu)
    {
        NS_LOG_WARN("The size of the first bucket ("
                    << m_burst << ") should be "
                    << "greater than the size of the second bucket (" << m_mtu << ").");
    }

    if (m_peakRate > DataRate("0bps") && m_peakRate <= m_rate)
    {
        NS_LOG_WARN("The rate for the second bucket ("
                    << m_peakRate << ") should be "
                    << "greater than the rate for the first bucket (" << m_rate << ").");
    }
    // The recorder m_last should initialize to last class of this
    // queue disc to round begin from first class.
    if (m_last != (GetNQueueDiscClasses() - 1))
    {
        m_last = GetNQueueDiscClasses() - 1;
    }

    return true;
}

void
HtbfQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
    // Token Buckets are full at the beginning.
    m_btokens = m_burst;
    m_ptokens = m_mtu;
    // Initialising other variables to 0.
    m_timeCheckPoint = Seconds(0);
    m_id = EventId();
}

} // namespace ns3
