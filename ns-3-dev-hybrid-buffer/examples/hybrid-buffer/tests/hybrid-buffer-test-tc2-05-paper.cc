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
 * 网络拓扑结构说明：
 *
 *         n1            n3
 *            \        /
 *  120Gbps,   \      /   100Gbps,
 *  1ms     .   \    /  . 1ms
 *          .     n0    .
 *          .   /    \  .
 *             /      \
 *            /        \
 *         n2            n4
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 * - Traffic: n1, n2 send 1-to-1 traffic to n3, n4. one is 120Gbps congested
 *            flow to HP queue, another starts 120Gbps burst flow to LP queue
 *            when congested flow reaches steady state.
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
// 定义日志组件 HybridBufferTest
NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

// StarSimHelperTc201 继承自 StarSimHelper，用于自定义路由器包过滤器的设置
class StarSimHelperTc201 : public StarSimHelper
{
  public:
    // 构造函数，传入仿真名称、起止时间
    StarSimHelperTc201(std::string simName, Time start = Seconds(0), Time stop = Seconds(1));
    ~StarSimHelperTc201() override;

    // 重载设置路由器包过滤器的函数
    void SetupRouterPacketFilter() override;
};

// 构造函数实现
StarSimHelperTc201::StarSimHelperTc201(std::string simName, Time start, Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName); // 日志记录构造过程
}

// 析构函数实现
StarSimHelperTc201::~StarSimHelperTc201()
{
    NS_LOG_FUNCTION(this); // 日志记录析构过程
}

// 设置路由器包过滤器，实现流量分类到不同优先级队列
void
StarSimHelperTc201::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    uint8_t protocolNumber = IsTcpTransport() ? TcpL4Protocol::PROT_NUMBER : UdpL4Protocol::PROT_NUMBER;
    
    Ptr<TrafficControlLayer> tc = m_hub->GetObject<TrafficControlLayer>();
    // 为每个输出端口安装包过滤器
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls = {0, 0, 0}; // 优先级分类数组
        Ptr<QueueDisc> rootQdisc = tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        // 根队列分类规则：一条流进入高优先级队列，另一条进入低优先级队列
        Ptr<FiveTuplePacketFilter> rootFilter = CreateObject<FiveTuplePacketFilter>();
        // 配置流的流向
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

        // 为每个二级队列（如高优先级队列）安装包过滤器
        for (uint32_t l2id = 0; l2id < rootQdisc->GetNQueueDiscClasses(); l2id++)
        {
            // layer 2 (hp)
            std::vector<uint8_t> qCls = {0, 0, 0};
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

// 主函数，仿真入口
int
main(int argc, char* argv[])
{
    // LogComponentEnable("HybridBufferTest", LOG_LEVEL_ALL);
    // LogComponentEnable("SimHelper", LOG_LEVEL_ALL);
    // LogComponentEnable("StarSimHelper", LOG_LEVEL_ALL);
    // LogComponentEnable("SwitchMmu", LOG_LEVEL_ALL);
    // LogComponentEnable("OffChipBuffer", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointReorderNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointReorderHelper", LOG_LEVEL_ALL);
    // LogComponentEnable("PrioQueueDisc", ns3::LOG_LEVEL_ALL);
    // LogComponentEnable("DrrQueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("FifoQueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("QueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("FiveTuplePacketFilter", LOG_LEVEL_ALL);
    // LogComponentEnable("TrafficControlLayer", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);

    // 解析命令行参数，支持算法名选择
    CommandLine cmd(__FILE__);
    std::string algorithm_name = "BMS"; // 默认算法名
    std::string transport = "udp";  // 默认 TCP
    cmd.AddValue("algorithm_name", "算法名", algorithm_name);
    cmd.AddValue("transport","传输协议：tcp 或 udp",
            transport);
    cmd.Parse(argc, argv);
    std::cout << "传输协议：" << transport << std::endl;
    // 仿真参数设置
    double ewma_w = 0.1; // EWMA 权重
    double eta_md = 1;   // eta_MD 参数

    uint32_t numSpokes = 6;      // 星型拓扑的总节点数
    uint32_t numReceivers = 3;   // 接收节点数
    double sim_time = 0.2;       // 仿真总时长（秒）
    DataRate recvLinkCapacity = DataRate("100Gbps"); // 接收链路带宽
    Time recvLinkDelay = MicroSeconds(1);            // 接收链路延迟
    DataRate sendLinkCapacity = DataRate("2000Gbps"); // 发送链路带宽
    Time sendLinkDelay = MicroSeconds(1);              // 发送链路延迟

    // 创建仿真辅助对象，传入仿真名称和时间
    hb::StarSimHelperTc201 simHelper("test-tc2-05", Seconds(0), Seconds(sim_time));
    simHelper.SetTransportProtocol(transport);
    simHelper.ConfigTransport();
    // 配置星型拓扑结构
    simHelper.ConfigTopology(numSpokes,
                             numReceivers,
                             recvLinkCapacity,
                             recvLinkDelay,
                             sendLinkCapacity,
                             sendLinkDelay);

    // simHelper.AddFlow(2, 0, Seconds(1), Seconds(15));
    // simHelper.AddFlow(3, 1, Seconds(8), Seconds(15));
    // 配置流量参数
    uint32_t sendRate1 = 200; // 第一个流的速率（Gbps）
    uint32_t sendRate2 = 900; // 第二个流的速率（Gbps）
    std::string rate1 = std::to_string(sendRate1) + "Gbps";
    std::string rate2 = std::to_string(sendRate2) + "Gbps";

    // 添加流量，指定发送端、接收端、起止时间和速率
    simHelper.AddFlow(5, 2, Seconds(0.0000), Seconds(0.0005), DataRate(rate1));
    simHelper.AddFlow(3, 0, Seconds(0.0000), Seconds(0.0005), DataRate(rate1));
    simHelper.AddFlow(4, 1, Seconds(0.00021), Seconds(0.00023), DataRate(std::to_string(1000) + "Gbps"));   //rate2));
    simHelper.AddFlow(3, 0, Seconds(0.0010), Seconds(0.0015), DataRate(rate1));
    simHelper.AddFlow(4, 1, Seconds(0.0014), Seconds(0.00143), DataRate(rate2));

    // 配置仿真结果输出路径和算法参数
    Config::SetDefault("ns3::SwitchMmu::nextFilePath", StringValue("tc2-05/"));
    Config::SetDefault("ns3::SwitchMmu::now_algorithm_name", StringValue(algorithm_name));
    if (!algorithm_name.compare("pbs"))
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(2)); // pbs 算法
        Config::SetDefault("ns3::SwitchMmu::EWMA_W", DoubleValue(ewma_w)); // pbs 算法参数
        Config::SetDefault("ns3::SwitchMmu::eta_MD", DoubleValue(eta_md)); // pbs 算法参数
    }
    else
    {
        Config::SetDefault("ns3::SwitchMmu::BMAlgorithm", EnumValue(5)); // BMS 算法
    }

    // 启用各类数据跟踪，便于后续分析
    simHelper.EnableHbmThroughputTracing();      // HBM 吞吐量跟踪
    simHelper.EnableBufferUsageTracing();        // 缓存使用率跟踪
    simHelper.EnableBmResultTracing();           // 算法结果跟踪
    simHelper.EnablePortThroughputTracing();     // 端口吞吐量跟踪
    simHelper.EnableQueueThroughputTracing();    // 队列吞吐量跟踪
    simHelper.EnableWCacheThroughputTracing();   // WCache 吞吐量跟踪
    simHelper.EnableSramThroughputTracing();     // SRAM 吞吐量跟踪
    simHelper.EnableQueueWCacheTracing();        // 队列 WCache 跟踪
    simHelper.EnableQueueSramTracing();          // 队列 SRAM 跟踪
    simHelper.EnableQueueHbmTracing();           // 队列 HBM 跟踪
    
    // 启动仿真
    simHelper.Run();
    return 0;
}
