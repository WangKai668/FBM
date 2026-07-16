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

/*
 * Network topology
 *
 *         n1            n6
 *            \        /
 *  240Gbps,   \      /   100Gbps,
 *  1ms     .   \    /  . 1ms
 *          .     n0    .
 *          .   /    \  .
 *  240Gbps,   /      \
 *  1ms       /        \
 *         n5            n10
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 * - Traffic: n1-n5 send 1-to-1 traffic to n6-n10. Traffic from n2-n5 is 120Gbps
 *            burst flow to HP queue, n1 starts 120Gbps burst flow to LP queue
 *            when former flow reaches steady state.
 *            SP scheduling between HP and LP queue.
 */
#include "../helper/star-sim-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

namespace ns3
{

namespace hb
{
NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

class StarSimHelperTc201 : public StarSimHelper
{
  public:
    StarSimHelperTc201(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));
    ~StarSimHelperTc201() override;

    void SetupRouterPacketFilter() override;
};

StarSimHelperTc201::StarSimHelperTc201(std::string simName, Time start, Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

StarSimHelperTc201::~StarSimHelperTc201()
{
    NS_LOG_FUNCTION(this);
}

void
StarSimHelperTc201::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    uint8_t protocolNumber = IsTcpTransport() ? TcpL4Protocol::PROT_NUMBER : UdpL4Protocol::PROT_NUMBER;
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // Install packet filters for each output port
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls = {0, 0};
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // root classify rule: one flow goto hp, another goto lp
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        // 配置流流向
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            rootFilter->AddClassifyRule(protocolNumber,
                                        m_spokeInterfaces.GetAddress(sid),
                                        m_spokeInterfaces.GetAddress(sid - m_nReceivers),
                                        Ipv4Mask::GetOnes(),
                                        Ipv4Mask::GetOnes(),
                                        0,
                                        0xffff,
                                        0,
                                        0xffff,
                                        priCls[sid - m_nReceivers]);
        }
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 (hp)
            std::vector<uint8_t> qCls = {0, 0};
            Ptr<QueueDiscClass> l2Cls = rootQdisc->GetQueueDiscClass(l2id);
            Ptr<QueueDisc> l2Qdisc = l2Cls->GetQueueDisc();
            Ptr<FiveTuplePacketFilter> l2Filter = CreateObject<FiveTuplePacketFilter>();
            for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
            {
                l2Filter->AddClassifyRule(protocolNumber,
                                          m_spokeInterfaces.GetAddress(sid),
                                          m_spokeInterfaces.GetAddress(sid - m_nReceivers),
                                          Ipv4Mask::GetOnes(),
                                          Ipv4Mask::GetOnes(),
                                          0,
                                          0xffff,
                                          0,
                                          0xffff,
                                          qCls[sid - m_nReceivers]);
            }
            l2Qdisc->AddPacketFilter(l2Filter);
        }
    }
}

} // namespace hb
} // namespace ns3

int
main(int argc, char* argv[])
{
    // pbs
    CommandLine cmd(__FILE__);
    double Deephir_threshold = 0.2;
    uint64_t flow_rate = 100;
    uint64_t change_time_us = 2;   //增加一个参数
    uint64_t if_change_threshold = 0;
    std::string algorithm_name = "BMS";
    std::string transport = "udp";  // 默认 TCP
    cmd.AddValue("Deephir_threshold", "deephir阈值", Deephir_threshold);
    cmd.AddValue("if_change_threshold", "是否改变DT阈值", if_change_threshold);
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    cmd.AddValue("change_time_us", "background change time(us)",change_time_us);
    cmd.AddValue("flow_rate", "流量速率", flow_rate);
    cmd.AddValue("transport","传输协议：tcp 或 udp",
                transport);
                
    std::cout << "是否读取到了" << Deephir_threshold << std::endl;
    cmd.Parse(argc, argv);
    Config::SetDefault("ns3::SwitchMmu::nextFilePath",
                    StringValue(""));
    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    Config::SetDefault("ns3::SwitchMmu::Deeohir_threshold", DoubleValue(Deephir_threshold));
    Config::SetDefault("ns3::SwitchMmu::if_change_threshold", UintegerValue(1));
    Config::SetDefault("ns3::SwitchMmu::if_test8",
                    UintegerValue(0));
    if (!algorithm_name.compare("pbs"))
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(2)); // pbs
    }
    else
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(5)); // BMS
    }

    uint32_t numSpokes = 4;
    uint32_t numReceivers = 2;
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("2000Gbps");
    Time sendLinkDelay = MicroSeconds(1);

    hb::StarSimHelperTc201 simHelper("test-tc2-08-new", Seconds(0), Seconds(sim_time));
    simHelper.SetTransportProtocol(transport);
    simHelper.ConfigTransport();
    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);

    const DataRate backgroundRateHigh("500Gbps");   //端口1的500速率
    const DataRate backgroundRateLow("100Gbps");    // 端口1变速
    const DataRate burstRate("1000Gbps");   //端口2的速率

    const Time changeTime = MicroSeconds(change_time_us);   //x时间

    // 切换到100Gbps之后，再等待固定200us
    const Time burstStart = changeTime + MicroSeconds(200);   //端口2间隔200
    const Time burstDuration = MicroSeconds(32);   //持续时间
    const Time burstEnd = burstStart + burstDuration;   
    // 最大x为64us 背景流继续运行到400us，覆盖整个突发阶段
    const Time backgroundStop = MicroSeconds(400);  
    std::cout << "Deephir_threshold="<< Deephir_threshold << "M" << std::endl;
    std::cout << "change_time_us=" << change_time_us<< "us" << std::endl;
    std::cout << "burst_start_us=" << burstStart.GetMicroSeconds()  << "us"  << std::endl;
    std::cout << "burst_end_us=" << burstEnd.GetMicroSeconds() << "us" << std::endl;

    // 端口1：0～x us，以500Gbps发送
    simHelper.AddFlow(3,1, MicroSeconds(0), changeTime, backgroundRateHigh);
    // 端口1：x us以后降为100Gbps
    simHelper.AddFlow(3, 1,  changeTime, backgroundStop, backgroundRateLow);
    // 端口2：在x+200us时注入1000Gbps突发，持续32us
    simHelper.AddFlow(2, 0, burstStart, burstEnd, burstRate);



    simHelper.EnableHbmThroughputTracing();
    simHelper.EnableBufferUsageTracing();
    simHelper.EnableBmResultTracing();
    simHelper.EnablePortThroughputTracing();
    simHelper.EnableQueueThroughputTracing();
    simHelper.EnableWCacheThroughputTracing();
    simHelper.EnableSramThroughputTracing();
    simHelper.EnableQueueWCacheTracing();
    simHelper.EnableQueueSramTracing();
    simHelper.EnableQueueHbmTracing();

    simHelper.Run();

    return 0;
}
