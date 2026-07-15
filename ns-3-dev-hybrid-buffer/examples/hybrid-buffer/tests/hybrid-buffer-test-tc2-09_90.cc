/*
 * Copyright (c) 2022 Xi'an Jiaotong University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Authors: Yuqi Liu <liuyuqi526@126.com>
 */

/*
 * Network topology
 *
 *         n1            n3
 *            \        /
 * 1200Gbps,   \      /   100Gbps,
 * 1ms      .   \    /  . 1ms
 *          .     n0    .
 *          .   /    \  .
 *             /      \
 *            /        \
 *         n2            n4
 *
 * - all net devices are reorder-point-to-point net devices
 * - all links are point-to-point links with indicated one-way BW/delay
 * - DropTail queues with backpressure from NetDeviceQueueInterface
 */

#include "../helper/star-sim-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

void
split(const string& s, vector<string>& tokens, const string& delimiters)
{
    string::size_type start = s.find_first_not_of(delimiters, 0);
    string::size_type pos = s.find_first_of(delimiters, 0);
    while (pos != string::npos || start != string::npos)
    {
        tokens.emplace_back(s.substr(start, pos - start));
        start = s.find_first_not_of(delimiters, pos);
        pos = s.find_first_of(delimiters, start);
    }
}

using namespace ns3;

namespace ns3
{

namespace hb
{
NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

class StarSimHelperTc202 : public StarSimHelper
{
  public:
    StarSimHelperTc202(std::string simName,
                       Time start = Seconds(0),
                       Time stop = Seconds(1));

    ~StarSimHelperTc202() override;
    void SetupRouterPacketFilter() override;
};
StarSimHelperTc202::StarSimHelperTc202(std::string simName, Time start,Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

StarSimHelperTc202::~StarSimHelperTc202()
{
    NS_LOG_FUNCTION(this);
}

void
StarSimHelperTc202::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);
    uint8_t protocolNumber = IsTcpTransport() ? TcpL4Protocol::PROT_NUMBER : UdpL4Protocol::PROT_NUMBER;
    Ptr<TrafficControlLayer> tc =
        m_hub->GetObject<TrafficControlLayer>();
    // 为每个输出端口安装分组过滤器
    for (uint32_t i = 0; i < m_nSpokes; i++)
    {
        std::vector<uint8_t> priCls = {0, 0, 0, 0, 0, 0};
        Ptr<QueueDisc> rootQdisc =
            tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(i));
        Ptr<FiveTuplePacketFilter> rootFilter =
            CreateObject<FiveTuplePacketFilter>();
        for (uint32_t sid = m_nReceivers; sid < m_nSpokes; sid++)
        {
            for (uint32_t cid = 0; cid < m_nReceivers; cid++)
            {
                rootFilter->AddClassifyRule(protocolNumber,
                                            m_spokeInterfaces.GetAddress(sid),
                                            m_spokeInterfaces.GetAddress(cid),
                                            Ipv4Mask::GetOnes(),
                                            Ipv4Mask::GetOnes(),
                                            0,
                                            0xffff,
                                            0,
                                            0xffff,
                                            0);//priCls.at(sid - m_nReceivers));
            }
        }
        rootQdisc->AddPacketFilter(rootFilter);

        for (uint32_t l2id = 0;
             l2id < rootQdisc->GetNQueueDiscClasses();
             l2id++)
        {
            std::vector<uint8_t> qCls = {0, 0, 0, 0, 0, 0};

            Ptr<QueueDiscClass> l2Cls =
                rootQdisc->GetQueueDiscClass(l2id);

            Ptr<QueueDisc> l2Qdisc =
                l2Cls->GetQueueDisc();

            Ptr<FiveTuplePacketFilter> l2Filter =
                CreateObject<FiveTuplePacketFilter>();

            for (uint32_t sid = m_nReceivers;
                 sid < m_nSpokes;
                 sid++)
            {
                for (uint32_t cid = 0;
                     cid < m_nReceivers;
                     cid++)
                {
                    l2Filter->AddClassifyRule(protocolNumber,
                                              m_spokeInterfaces.GetAddress(sid),
                                              m_spokeInterfaces.GetAddress(cid),
                                              Ipv4Mask::GetOnes(),
                                              Ipv4Mask::GetOnes(),
                                              0,
                                              0xffff,
                                              0,
                                              0xffff,
                                              0); //qCls.at(sid - m_nReceivers));
                }
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
    CommandLine cmd(__FILE__);
    double Deephir_threshold = 0.2;
    uint64_t flow_rate = 100;
    uint64_t if_change_threshold = 0;
    std::string algorithm_name = "BMS";
    std::string transport = "udp";  // 默认 TCP
    std::string trafficGenDir;
    // 1：WebSearch；0：Hadoop
    //isWeb：
    //1 = 读取 WebSearch 流量文件
    //0 = 读取 Hadoop/FbHdp 流量文件
    //isIncast：
    ///1 = 在真实背景流量上额外加入 Incast 突发流
    //0 = 只运行真实背景流量，不加入 Incast
    
    int isWeb = 1;
    int isIncast = 0;
    cmd.AddValue("Deephir_threshold","DeepHIR阈值",
                 Deephir_threshold);
    cmd.AddValue("if_change_threshold","是否改变DT阈值",
                 if_change_threshold);
    cmd.AddValue("algorithm_name","算法名",
                 algorithm_name);
    cmd.AddValue("IsWeb", "真实流量使用WebSearch还是Hadoop",
                 isWeb);
    cmd.AddValue("IsIncast","真实流量是否增加Incast",
                 isIncast);
    cmd.AddValue("traffic_gen_dir", "TrafficGen目录，由run-tests.sh传入",
                 trafficGenDir);
    cmd.AddValue("transport","传输协议：tcp 或 udp",
                transport);
    cmd.Parse(argc, argv);
    std::cout << "DeepHIR阈值：" << Deephir_threshold << std::endl;
    std::cout << "算法名称：" << algorithm_name << std::endl;
    std::cout << "IsWeb：" << isWeb << std::endl;
    std::cout << "IsIncast：" << isIncast << std::endl;
    std::cout << "传输协议：" << transport << std::endl;
    if (trafficGenDir.empty())
    {
        std::cerr << "错误：没有收到traffic_gen_dir参数。"<< std::endl;
        std::cerr<< "请检查run-tests.sh是否传入：" << "--traffic_gen_dir=$TRAFFIC_GEN_DIR" << std::endl;
        return 1;
    }
    uint32_t numSpokes = 30;
    uint32_t numReceivers = 6;
    double sim_time = 0.2;
    DataRate recvLinkCapacity = DataRate("100Gbps");
    Time recvLinkDelay = MicroSeconds(1);
    DataRate sendLinkCapacity = DataRate("5000Gbps");
    Time sendLinkDelay = MicroSeconds(1);
    hb::StarSimHelperTc202 simHelper("test-tc2-09", Seconds(0), Seconds(sim_time));
    simHelper.SetTransportProtocol(transport);
    simHelper.ConfigTopology(
        numSpokes,
        numReceivers,
        recvLinkCapacity,
        recvLinkDelay,
        sendLinkCapacity,
        sendLinkDelay);
    double interval = 0.0010;
    double lastTime = 0.0001;
    double nowT = 0.0;
    std::string filename = trafficGenDir + (isWeb ? "/Generated/traffic_web_90.txt"  : "/Generated/traffic_fbhdp.txt");
    std::cout << "TrafficGen目录：" << trafficGenDir << std::endl;
    std::cout << "读取流量文件：" << filename << std::endl;
    std::ifstream ifile(filename);
    if (!ifile.is_open())
    {
        std::cerr << "错误：无法打开流量文件："  << filename << std::endl;
        return 1;
    }
    std::ostringstream buf;
    char ch;
    while (ifile.get(ch))
    {
        buf.put(ch);
    }
    std::string file = buf.str();
    if (file.empty())
    {
        return 1;
    }
    std::vector<std::string> lines;
    split(file, lines, "\n");
    if (lines.empty())
    {
        return 1;
    }
    lines.erase(lines.begin());
    int count = 0;
    for (const auto& line : lines)
    {
        if (line.empty())
        {
            continue;
        }
        std::vector<std::string> words;
        split(line, words, " \t\r");
        if (words.size() < 5)
        {
            std::cerr
                << "警告：流量行格式错误，列数不足5列："
                << line
                << std::endl;

            continue;
        }
        count++;
        std::cout << "第" << count << "条流量：";
        for (const auto& word : words)
        {
            std::cout << word << " ";
        }
        std::cout << std::endl;
        try
        {
            int srcNode = std::stoi(words.at(0));
            int dstNode = std::stoi(words.at(1));
            double startTime = std::stod(words.at(2)) - 2;
            double stopTime =std::stod(words.at(3)) - 2;
            std::string dataRateString = words.at(4);

            if (isIncast){
                while (nowT + interval < std::stod(words.at(2)) - 2)  {
                    // 每隔1ms打一轮突发，突发持续0.5ms  //100Gbps * 1ms = 12.5MB   100Gbps*0.5ms=6.25MB   100Gbps*0.1ms=1.25MB    300Gbps*0.1ms=3.75MB
                    // for (int k = numSpokes-5; k < numSpokes; k++) 
                    //     simHelper.AddFlow((uint32_t)k,
                    //                     0,
                    //                     Seconds(nowT),
                    //                     Seconds(nowT + lastTime),
                    //                     DataRate("100Gbps"));

                    simHelper.AddFlow(30,
                                    0,
                                    Seconds(nowT),
                                    Seconds(nowT + lastTime),
                                    DataRate("300Gbps"));
                    nowT += interval;
                    cout << "流量大小Incast：" << (lastTime) * 300 * 1e9 / 8 / 1e6 << "kB " << 6
                        << "到" << 0 << " " << Seconds(nowT) << "到" << Seconds(nowT + lastTime)
                        << " " << DataRate(words.at(4)) << endl;
                }
            }
            simHelper.AddFlow(
                static_cast<uint32_t>(srcNode),
                static_cast<uint32_t>(dstNode),
                Seconds(startTime),
                Seconds(stopTime),
                DataRate(dataRateString));
            std::string::size_type gbpsPos =
                dataRateString.find("Gbps");
            if (gbpsPos == std::string::npos)
            {
                throw std::runtime_error(
                    "速率格式错误，缺少Gbps后缀：" +
                    dataRateString);
            }
            double numericRate =
                std::stod(dataRateString.substr(0, gbpsPos));
            double flowSizeKb =
                (stopTime - startTime) *
                numericRate *
                1e9 /
                8 /
                1000;
            std::cout
                << "流量大小："
                << flowSizeKb
                << "kB "
                << srcNode
                << "到"
                << dstNode
                << " "
                << Seconds(startTime)
                << "到"
                << Seconds(stopTime)
                << " "
                << DataRate(dataRateString)
                << std::endl;
            flow_rate =
                static_cast<uint64_t>(numericRate);
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "第"
                << count
                << "条流量解析失败："
                << e.what()
                << std::endl;
        }
    }

    std::cout
        << "成功处理流量条数："
        << count
        << std::endl;

    Config::SetDefault(
        "ns3::SwitchMmu::nextFilePath",
        StringValue("tc2-09-90/"));

    Config::SetDefault(
        "ns3::SwitchMmu::now_algorithm_name",
        StringValue(algorithm_name));

    Config::SetDefault(
        "ns3::SwitchMmu::Deeohir_threshold",
        DoubleValue(Deephir_threshold));

    Config::SetDefault(
        "ns3::SwitchMmu::flow_rate",
        UintegerValue(flow_rate));

    Config::SetDefault(
        "ns3::SwitchMmu::if_change_threshold",
        UintegerValue(1));

    Config::SetDefault(
        "ns3::SwitchMmu::if_test9",
        UintegerValue(1));

    if (algorithm_name == "pbs")
    {
        Config::SetDefault(
            "ns3::SwitchMmu::BMAlgorithm",
            EnumValue(2));

        Config::SetDefault(
            "ns3::SwitchMmu::now_algorithm_name",
            StringValue("pbs"));

        std::cout << "当前算法：pbs" << std::endl;
    }
    else
    {
        Config::SetDefault(
            "ns3::SwitchMmu::BMAlgorithm",
            EnumValue(5));

        Config::SetDefault(
            "ns3::SwitchMmu::now_algorithm_name",
            StringValue("BMS"));

        std::cout << "当前算法：BMS" << std::endl;
    }
    // simHelper.EnableHbmThroughputTracing();
    // simHelper.EnableBufferUsageTracing();
    // simHelper.EnableBmResultTracing();
    // simHelper.EnablePortThroughputTracing();
    // simHelper.EnableQueueThroughputTracing();
    // simHelper.EnableWCacheThroughputTracing();
    // simHelper.EnableSramThroughputTracing();
    // simHelper.EnableQueueWCacheTracing();
    // simHelper.EnableQueueSramTracing();
    // simHelper.EnableQueueHbmTracing();

    simHelper.Run();

    return 0;
}