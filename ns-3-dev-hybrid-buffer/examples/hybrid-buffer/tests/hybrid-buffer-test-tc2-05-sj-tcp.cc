/*
 * Copyright (c) 2022 Xi'an Jiaotong University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors: Yuqi Liu <liuyuqi526@126.com>
 */

/*
 * 实验场景：
 *
 * 一、2-to-1持续拥塞
 *
 *      node 3 ----\
 *                  \
 *                   switch ---- node 0
 *                  /
 *      node 4 ----/
 *
 * node 3、node 4的接入链路均为100 Gbps；
 * node 0的出口链路为100 Gbps；
 * 两条长流共同竞争node 0的出口，形成2-to-1持续拥塞。
 *
 *
 * 二、周期性10-to-1突发
 *
 *      node 5  ----\
 *      node 6  -----\
 *      node 7  ------\
 *      node 8  -------\
 *      node 9  --------\
 *                       >---- switch ---- node 0
 *      node 10 --------/
 *      node 11 -------/
 *      node 12 ------/
 *      node 13 -----/
 *      node 14 ----/
 *
 * 每轮包含10条突发流；
 * 每条突发流为250 KB；
 * 每轮突发总量：
 *
 *      250 KB × 10 = 2.5 MB
 *
 * 突发时间：
 *
 *      5 ms
 *      10 ms
 *      15 ms
 *      20 ms
 *
 * 长流和突发流均发送到node 0，竞争同一个出口端口。
 */

#include "../helper/star-sim-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace std;
using namespace ns3;

namespace ns3
{

namespace hb
{

NS_LOG_COMPONENT_DEFINE("HybridBufferTest");

/*
 * 本测试使用的星型拓扑辅助类。
 */
class StarSimHelperTc202 : public StarSimHelper
{
  public:
    StarSimHelperTc202(std::string simName,
                       Time start = Seconds(0),
                       Time stop = Seconds(1));

    ~StarSimHelperTc202() override;

    void SetupRouterPacketFilter() override;
};

StarSimHelperTc202::StarSimHelperTc202(std::string simName,
                                       Time start,
                                       Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

StarSimHelperTc202::~StarSimHelperTc202()
{
    NS_LOG_FUNCTION(this);
}

/*
 * 交换机分类规则。
 *
 * 当前实验中，所有TCP流都进入class 0。
 *
 * 因此：
 *
 * 1. 两条持续长流；
 * 2. 所有周期性突发流；
 *
 * 都会进入相同流量类别并竞争同一个出口队列。
 */
void
StarSimHelperTc202::SetupRouterPacketFilter()
{
    NS_LOG_FUNCTION(this);

    Ptr<TrafficControlLayer> tc =
        m_hub->GetObject<TrafficControlLayer>();

    NS_ABORT_MSG_IF(tc == nullptr,
                    "交换机没有安装TrafficControlLayer");

    /*
     * 为交换机的每个输出端口安装分类器。
     */
    for (uint32_t portId = 0; portId < m_nSpokes; ++portId)
    {
        Ptr<QueueDisc> rootQdisc =
            tc->GetRootQueueDiscOnDevice(m_hubDevices.Get(portId));

        NS_ABORT_MSG_IF(rootQdisc == nullptr,
                        "输出端口没有安装根QueueDisc，portId="
                            << portId);

        /*
         * 根QueueDisc分类器：
         *
         * 匹配所有TCP报文，并将其分类到class 0。
         */
        Ptr<FiveTuplePacketFilter> rootFilter =
            CreateObject<FiveTuplePacketFilter>();

        rootFilter->AddClassifyRule(
            TcpL4Protocol::PROT_NUMBER,
            Ipv4Address::GetAny(),
            Ipv4Address::GetAny(),
            Ipv4Mask::GetZero(),
            Ipv4Mask::GetZero(),
            0,
            0xffff,
            0,
            0xffff,
            0);

        rootQdisc->AddPacketFilter(rootFilter);

        /*
         * 为根QueueDisc下面的各个子QueueDisc安装TCP分类器。
         */
        for (uint32_t classId = 0;
             classId < rootQdisc->GetNQueueDiscClasses();
             ++classId)
        {
            Ptr<QueueDiscClass> queueDiscClass =
                rootQdisc->GetQueueDiscClass(classId);

            if (queueDiscClass == nullptr)
            {
                continue;
            }

            Ptr<QueueDisc> childQdisc =
                queueDiscClass->GetQueueDisc();

            if (childQdisc == nullptr)
            {
                continue;
            }

            Ptr<FiveTuplePacketFilter> childFilter =
                CreateObject<FiveTuplePacketFilter>();

            childFilter->AddClassifyRule(
                TcpL4Protocol::PROT_NUMBER,
                Ipv4Address::GetAny(),
                Ipv4Address::GetAny(),
                Ipv4Mask::GetZero(),
                Ipv4Mask::GetZero(),
                0,
                0xffff,
                0,
                0xffff,
                0);

            childQdisc->AddPacketFilter(childFilter);
        }
    }
}

} // namespace hb

} // namespace ns3

int
main(int argc, char* argv[])
{
    /*
     * ============================================================
     * 一、命令行参数
     * ============================================================
     */

    CommandLine cmd(__FILE__);

    double Deephir_threshold = 0.2;
    uint64_t if_change_threshold = 0;
    std::string algorithm_name = "BMS";

    cmd.AddValue("Deephir_threshold",
                 "DeepHiR阈值",
                 Deephir_threshold);

    cmd.AddValue("if_change_threshold",
                 "是否改变DT阈值",
                 if_change_threshold);

    cmd.AddValue("algorithm_name",
                 "缓存管理算法名称：BMS或者pbs",
                 algorithm_name);

    cmd.Parse(argc, argv);

    /*
     * ============================================================
     * 二、拓扑参数
     * ============================================================
     *
     * 接收节点：
     *
     *      node 0
     *      node 1
     *      node 2
     *
     * 发送节点：
     *
     *      node 3 ～ node 21
     *
     * 本实验实际使用：
     *
     *      持续流发送端：node 3、node 4
     *      突发流发送端：node 5 ～ node 14
     *      拥塞接收端：node 0
     */

    const uint32_t numSpokes = 22;
    const uint32_t numReceivers = 3;

    /*
     * 仿真时间为26 ms。
     *
     * 最后一次突发发生在20 ms，
     * 因此还剩6 ms用于观察最后一次突发后的缓存变化。
     */
    const double simTimeSeconds = 0.026;

    /*
     * 接收方向出口链路。
     */
    const DataRate recvLinkCapacity("100Gbps");
    const Time recvLinkDelay = MicroSeconds(20);

    /*
     * 发送端接入链路。
     */
    const DataRate sendLinkCapacity("100Gbps");
    const Time sendLinkDelay = MicroSeconds(20);

    /*
     * ============================================================
     * 三、缓存管理算法配置
     * ============================================================
     */

    Config::SetDefault(
        "ns3::SwitchMmu::nextFilePath",
        StringValue("tc2-05/"));

    Config::SetDefault(
        "ns3::SwitchMmu::now_algorithm_name",
        StringValue(algorithm_name));

    Config::SetDefault(
        "ns3::SwitchMmu::Deeohir_threshold",
        DoubleValue(Deephir_threshold));

    Config::SetDefault(
        "ns3::SwitchMmu::if_change_threshold",
        UintegerValue(if_change_threshold));

    Config::SetDefault(
        "ns3::SwitchMmu::if_test9",
        UintegerValue(0));

    /*
     * 根据命令行参数选择缓存管理算法。
     */
    if (algorithm_name == "pbs")
    {
        Config::SetDefault(
            "ns3::SwitchMmu::BMAlgorithm",
            EnumValue(2));

        Config::SetDefault(
            "ns3::SwitchMmu::now_algorithm_name",
            StringValue("pbs"));

        std::cout << "当前缓存管理算法：pbs"
                  << std::endl;
    }
    else
    {
        Config::SetDefault(
            "ns3::SwitchMmu::BMAlgorithm",
            EnumValue(5));

        Config::SetDefault(
            "ns3::SwitchMmu::now_algorithm_name",
            StringValue("BMS"));

        std::cout << "当前缓存管理算法：BMS"
                  << std::endl;
    }

    /*
     * ============================================================
     * 四、创建仿真对象并配置拓扑
     * ============================================================
     */

    hb::StarSimHelperTc202 simHelper(
        "test-tc2-05",
        Seconds(0),
        Seconds(simTimeSeconds));

    simHelper.ConfigTopology(
        numSpokes,
        numReceivers,
        recvLinkCapacity,
        recvLinkDelay,
        sendLinkCapacity,
        sendLinkDelay);

    /*
     * 使用TCP DCTCP。
     *
     * 注意：
     * TCP突发会受到初始拥塞窗口、ACK、ECN和拥塞窗口的影响，
     * 因此缓存曲线可能不会像UDP那样形成完全垂直的突发尖峰。
     */
    simHelper.ConfigTransport(
        "tcp",
        "ns3::TcpDctcp");

    /*
     * ============================================================
     * 五、配置2-to-1持续拥塞流
     * ============================================================
     */

    /*
     * 所有流的目的节点均为node 0。
     *
     * 因此所有流都会竞争node 0对应的同一个100 Gbps出口。
     */
    const uint32_t congestedDstId = 0;

    /*
     * 两条持续长流：
     *
     *      node 3 -> node 0
     *      node 4 -> node 0
     *
     * 两个发送端总输入能力约为：
     *
     *      100 Gbps × 2 = 200 Gbps
     *
     * 出口能力只有：
     *
     *      100 Gbps
     *
     * 因此形成2-to-1持续拥塞。
     */

    /*
     * 每条长流设置为500 MB。
     *
     * 26 ms内，一个100 Gbps链路的理论最大传输量约为：
     *
     *      100 Gbps × 0.026 s ÷ 8
     *      = 325 MB
     *
     * 使用500 MB可以保证长流不会在仿真结束前提前完成。
     */
    const uint64_t persistentFlowSizeBytes =
        500ULL * 1000ULL * 1000ULL;

    simHelper.AddFlow(
        3,                              // srcId
        congestedDstId,                 // dstId
        Seconds(0.0),                   // start
        Seconds(simTimeSeconds),        // stop
        DataRate("2000Gbps"),           // 应用发送速率
        persistentFlowSizeBytes);        // 流大小

    simHelper.AddFlow(
        4,
        congestedDstId,
        Seconds(0.0),
        Seconds(simTimeSeconds),
        DataRate("2000Gbps"),
        persistentFlowSizeBytes);

    /*
     * ============================================================
     * 六、配置周期性10-to-1突发流
     * ============================================================
     */

    /*
     * 突发发送端为：
     *
     *      node 5
     *      node 6
     *      node 7
     *      node 8
     *      node 9
     *      node 10
     *      node 11
     *      node 12
     *      node 13
     *      node 14
     *
     * 共10个发送端。
     */
    const uint32_t firstBurstSrcId = 5;
    const uint32_t burstSenderCount = 10;

    /*
     * 每条突发流为250 KB。
     *
     * 当前按照十进制KB计算：
     *
     *      250 KB = 250000 bytes
     */
    const uint64_t burstFlowSizeBytes =
        250ULL * 1000ULL;

    /*
     * 突发时间：
     *
     *      5 ms
     *      10 ms
     *      15 ms
     *      20 ms
     */
    const uint32_t firstBurstTimeMs = 5;
    const uint32_t lastBurstTimeMs = 20;
    const uint32_t burstIntervalMs = 5;

    uint32_t burstRoundCount = 0;
    uint32_t totalBurstFlowCount = 0;

    for (uint32_t burstTimeMs = firstBurstTimeMs;
         burstTimeMs <= lastBurstTimeMs;
         burstTimeMs += burstIntervalMs)
    {
        ++burstRoundCount;

        std::cout
            << "创建第" << burstRoundCount
            << "轮10-to-1突发："
            << "启动时间=" << burstTimeMs << " ms，"
            << "单条流=" << burstFlowSizeBytes << " bytes，"
            << "本轮总量="
            << burstFlowSizeBytes * burstSenderCount
            << " bytes"
            << std::endl;

        /*
         * 每轮同时创建10条流。
         */
        for (uint32_t senderIndex = 0;
             senderIndex < burstSenderCount;
             ++senderIndex)
        {
            /*
             * senderIndex为0～9，
             * 因此srcId为5～14。
             */
            const uint32_t srcId =
                firstBurstSrcId + senderIndex;

            simHelper.AddFlow(
                srcId,
                congestedDstId,
                MilliSeconds(burstTimeMs),
                Seconds(simTimeSeconds),
                DataRate("2000Gbps"),
                burstFlowSizeBytes);

            ++totalBurstFlowCount;
        }
    }

    /*
     * ============================================================
     * 七、打印流量配置
     * ============================================================
     */

    std::cout << std::endl;
    std::cout << "========================================"
              << std::endl;

    std::cout << "实验流量配置"
              << std::endl;

    std::cout << "仿真时间："
              << simTimeSeconds * 1000
              << " ms"
              << std::endl;

    std::cout << "拥塞出口对应接收节点：node "
              << congestedDstId
              << std::endl;

    std::cout << "持续拥塞流数量：2"
              << std::endl;

    std::cout << "持续流发送端：node 3、node 4"
              << std::endl;

    std::cout << "突发轮数："
              << burstRoundCount
              << std::endl;

    std::cout << "每轮突发流数量："
              << burstSenderCount
              << std::endl;

    std::cout << "突发流总数量："
              << totalBurstFlowCount
              << std::endl;

    std::cout << "每条突发流大小："
              << burstFlowSizeBytes
              << " bytes"
              << std::endl;

    std::cout << "每轮突发总量："
              << burstFlowSizeBytes * burstSenderCount
              << " bytes"
              << std::endl;

    std::cout << "突发时间：5、10、15、20 ms"
              << std::endl;

    std::cout << "出口链路带宽："
              << recvLinkCapacity
              << std::endl;

    std::cout << "发送端接入带宽："
              << sendLinkCapacity
              << std::endl;

    std::cout << "========================================"
              << std::endl;
    std::cout << std::endl;

    /*
     * ============================================================
     * 八、开启结果跟踪
     * ============================================================
     */

    /*
     * HBM/DRAM吞吐量。
     */
    simHelper.EnableHbmThroughputTracing();

    /*
     * 整体缓存占用。
     */
    simHelper.EnableBufferUsageTracing();

    /*
     * 缓存管理算法决策结果。
     */
    simHelper.EnableBmResultTracing();

    /*
     * 端口吞吐量。
     */
    simHelper.EnablePortThroughputTracing();

    /*
     * 队列吞吐量。
     */
    simHelper.EnableQueueThroughputTracing();

    /*
     * 写缓存吞吐量。
     */
    simHelper.EnableWCacheThroughputTracing();

    /*
     * SRAM吞吐量。
     */
    simHelper.EnableSramThroughputTracing();

    /*
     * 各队列写缓存占用。
     */
    simHelper.EnableQueueWCacheTracing();

    /*
     * 各队列SRAM占用。
     */
    simHelper.EnableQueueSramTracing();

    /*
     * 各队列HBM/DRAM占用。
     */
    simHelper.EnableQueueHbmTracing();

    /*
     * ============================================================
     * 九、运行仿真
     * ============================================================
     */

    simHelper.Run();

    return 0;
}