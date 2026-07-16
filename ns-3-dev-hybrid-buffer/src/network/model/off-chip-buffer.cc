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
 * Authors: Shunlei Yang <yxlzqmysl0405@stu.xjtu.edu.cn>
 */

#include "off-chip-buffer.h"

#include "switch-mmu.h"

#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/point-to-point-reorder-net-device.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffChipBuffer");//这是一个宏，用于定义一个名为"OffChipBuffer"的日志组件，用于在程序中记录日志信息

NS_OBJECT_ENSURE_REGISTERED(OffChipBuffer);//这是一个宏，用于确保OffChipBuffer类在NS-3对象系统中注册，以便在需要时能够正确地创建该类的实例

OffChipBuffer::Stats::Stats()
    : nTotalDramStoredPackets(0),
      nTotalWcacheStoredPackets(0)
{
}

TypeId
OffChipBuffer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffChipBuffer")
            .SetParent<Object>()
            .SetGroupName("Network")
            .AddConstructor<OffChipBuffer>()
            .AddAttribute(
                "WcacheSize",
                "The Write-in Cache Buffer Size",
                // I don't why, but without static_cast, there will be linking error
                UintegerValue(static_cast<uint64_t>(DEFAULT_WCACHE_SIZE)),   //400KB
                MakeUintegerAccessor(&OffChipBuffer::SetWcacheSize, &OffChipBuffer::GetWcacheSize),
                MakeUintegerChecker<uint64_t>())
            .AddAttribute(
                "DramSize",
                "The DRAM Size",
                // I don't why, but without static_cast, there will be linking error
                UintegerValue(static_cast<uint64_t>(DEFAULT_OFFCHIPBUFER_SIZE)),   //10GB
                MakeUintegerAccessor(&OffChipBuffer::SetDramSize, &OffChipBuffer::GetDramSize),
                MakeUintegerChecker<uint64_t>())
            .AddAttribute("ChannelDelay",
                          "The delay from the switching chip to the channel of the off chip buffer",
                          TimeValue(NanoSeconds(0)),
                          MakeTimeAccessor(&OffChipBuffer::m_channelDelay),
                          MakeTimeChecker())
            // R-R Conflict
            .AddAttribute("TimeIntervalBetweenConflictReadRequest",
                          "The time interval between two Bank Conflict Packet Read Request.",
                          TimeValue(NanoSeconds(20)),   // 50
                          MakeTimeAccessor(&OffChipBuffer::m_tInterBankCxReadReqGap),
                          MakeTimeChecker())
            // W-R Conflict Time
            .AddAttribute("TimeInterBetweenWRRequest",
                          "The time to wait between Packet Write and Packet Read Request.",
                          TimeValue(NanoSeconds(10)),   // 15
                          MakeTimeAccessor(&OffChipBuffer::m_tInterWRReqGap),
                          MakeTimeChecker())
            // R-W Conflict Time
            .AddAttribute("TimeIntervalBetweenRWRequest",
                          "The time to wait between Packet Read and Packet Write Request.",
                          TimeValue(NanoSeconds(10)),   // 20
                          MakeTimeAccessor(&OffChipBuffer::m_tInterRWReqGap),
                          MakeTimeChecker())
            // Probility of R-R Conflict
            .AddAttribute("ProbOfBankCxReadReq",
                          "The Probility of Bank Conflict Between two Read Request.",
                          DoubleValue(0.005),
                          MakeDoubleAccessor(&OffChipBuffer::m_probOfBankCxReadReqs),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("HbmBitWidth",
                          "The HBM BitWidth",                                        //1024bit = 128byte
                          UintegerValue(static_cast<uint64_t>(DEFAULT_HBM_BITWIDTH)),//表示HBM（High Bandwidth Memory）的位宽
                          MakeUintegerAccessor(&OffChipBuffer::m_dramBitWidth),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("HbmBurstLength",
                          "The data Length of one Burst Mode HBM W/R op.",
                          UintegerValue(static_cast<uint64_t>(DEFAULT_HBM_BURST_LENGTH)),//4
                          MakeUintegerAccessor(&OffChipBuffer::m_dramBurstLength),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("HbmBusFrequency",
                          "The HBM working Data Bus Frequency.",
                          UintegerValue(static_cast<uint64_t>(DEFAULT_HBM_BUS_FREQUENCY)),// 1000/2000MHZ,表示HBM的数据总线频率
                          MakeUintegerAccessor(&OffChipBuffer::m_dramBusFrequency),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("HbmAtomicOpLength",
                          "The HBM Atomic Operation Length.",  //表示HBM（High Bandwidth Memory）的原子操作长度
                          UintegerValue(static_cast<uint64_t>(DEFAULT_HBM_ATOMIC_LENGTH)),// Atomic Op = 2 HBM Burst Mode op.
                          MakeUintegerAccessor(&OffChipBuffer::m_dramAtomicOpLength),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute(
                "ArbitrationAlgorithm",
                "The HBM W/R Arbitration Algorithm.",//表示HBM（High Bandwidth Memory）的读写调度算法
                EnumValue(OffChipBuffer::ArbAlgorithm(CLASSICALWRR)),
                MakeEnumAccessor(&OffChipBuffer::SetArbAlgorithm, &OffChipBuffer::GetArbAlgorithm),
                MakeEnumChecker(OffChipBuffer::INTERLEAVEDWRR,
                                "InterleavedWrr",
                                OffChipBuffer::CLASSICALWRR,
                                "ClassicalWrr"))
            .AddAttribute("WRRWriteWeight",
                          "The WRR Algorithm Write Weight.",
                          UintegerValue(20),//设置了属性的默认值为20，表示写权重的初始值为20
                          MakeUintegerAccessor(&OffChipBuffer::m_wrrWWeight),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("WRRReadWeight",
                          "The WRR Algorithm Read Weight.",
                          UintegerValue(30),//设置了属性的默认值为30，表示读权重的初始值为30
                          MakeUintegerAccessor(&OffChipBuffer::m_wrrRWeight),
                          MakeUintegerChecker<uint32_t>())

            //
            // Trace Source
            //
            .AddTraceSource("DramReadStart",
                            "A packet is to be read from the DRAM",
                            MakeTraceSourceAccessor(&OffChipBuffer::m_traceDramReadStart),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("DramWriteStart",
                            "A packet is to be written into the DRAM",
                            MakeTraceSourceAccessor(&OffChipBuffer::m_traceDramWriteStart),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("DramReadComplete",
                            "A packet has been read from the DRAM",
                            MakeTraceSourceAccessor(&OffChipBuffer::m_traceDramReadComplete),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("DramWriteComplete",
                            "A packet has been written into the DRAM",
                            MakeTraceSourceAccessor(&OffChipBuffer::m_traceDramWriteComplete),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("WCacheReadComplete",
                            "A part of Packet has been read from the DRAM",
                            MakeTraceSourceAccessor(&OffChipBuffer::m_traceWcacheReadComplete),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("WCacheWriteComplete",
                            "A part of Packet has been written from the DRAM",
                            MakeTraceSourceAccessor(&OffChipBuffer::m_traceWcacheWriteComplete),
                            "ns3::Packet::TracedCallback")                            

        ;

    return tid;
}

OffChipBuffer::OffChipBuffer()
    : m_wrrQueIndex(0),
      m_interleavedWrrRoundCnt(0),
      m_classicalWrrPacketCnt(0),
      m_nWrite2ReadCx(0),
      m_nRead2WriteCx(0),
      m_ntwoBankCxRead(0),
      m_ntwoBankCxWrite(0),
      m_state(IDLE),
      m_wcacheUsed(0),
      m_nWcachePackets(0),
      m_dramUsed(0),
      m_nDramPackets(0),
      m_writingPacket(nullptr),
      m_writingStopFlag(false),
      m_thresholdOfReadRequest(0),
      m_stats(),
      m_cancelWriteTime(0)
{
    NS_LOG_FUNCTION(this);

    // Clear some container
    m_writeRequestVec.clear();
    m_readRequestVec.clear();
}

OffChipBuffer::~OffChipBuffer()
{
    NS_LOG_FUNCTION(this);
}

const OffChipBuffer::Stats&
OffChipBuffer::GetStats()
{
    NS_LOG_FUNCTION(this);
    return m_stats;
}



void
OffChipBuffer::SetArbAlgorithm(OffChipBuffer::ArbAlgorithm alg)
{
    NS_LOG_FUNCTION(this << alg);

    m_arbAlgorithm = alg;//m_arbAlgorithm = alg：将传入的算法类型赋值给OffChipBuffer类的成员变量m_arbAlgorithm，从而设置了对象的调度算法
}

OffChipBuffer::ArbAlgorithm
OffChipBuffer::GetArbAlgorithm() const
{
    NS_LOG_FUNCTION(this);
    return m_arbAlgorithm;
}
//这个函数用于设置OffChipBuffer类中的成员变量m_mmu，并通过对象聚合获取与SwitchMmu对象相关联的Node对象
bool
OffChipBuffer::SetMmu(Ptr<SwitchMmu> mmu)
{
    NS_LOG_FUNCTION(this << mmu);//记录当前函数的调用，输出当前对象的指针和传入的SwitchMmu对象指针，通常用于调试目的

    if (mmu == nullptr)//检查传入的SwitchMmu对象是否为空指针，如果是空指针，则直接返回false，表示设置失败
    {
        return false;
    }
    m_mmu = mmu;//将传入的SwitchMmu对象赋值给OffChipBuffer类的成员变量m_mmu，完成SwitchMmu对象的设置
    // Get node by object aggregation//通过对象聚合，从SwitchMmu对象中获取与之相关联的Node对象，并将其赋值给OffChipBuffer类的成员变量m_node
    m_node = m_mmu->GetObject<Node>();
    return true;//返回true，表示设置SwitchMmu对象成功
}

Ptr<SwitchMmu>
OffChipBuffer::GetMmu() const
{
    NS_LOG_FUNCTION(this);
    return m_mmu;
}

bool
OffChipBuffer::SetWcacheSize(uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_wcacheSize = size;

    return true;
}

uint32_t
OffChipBuffer::GetWcacheSize() const
{
    NS_LOG_FUNCTION(this);
    return m_wcacheSize;
}

void
OffChipBuffer::SetDramSize(uint64_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_dramSize = size;
}

uint64_t
OffChipBuffer::GetDramSize() const
{
    return m_dramSize;
}

uint64_t
OffChipBuffer::GetDramBandwidth() const
{
    return m_dramBusFrequency * m_dramBitWidth * 8 * 1e-9; //  1e9 HZ * 128Bytes *1e-9 *8 = 1GHZ * 1024bit = 1024*1 G b /s = 1024 Gbps   每纳秒处理1Kb，每s处理1KB*1e9=1000Gb，i.e. 1000Gbps
    //static const uint64_t DEFAULT_HBM_BITWIDTH = 128;             // 1024bit = 128byte
    //static const uint64_t DEFAULT_HBM_BURST_LENGTH = 4;           // 4
    //static const uint64_t DEFAULT_HBM_BUS_FREQUENCY = 1000000000; // 1000MHZ 
}


void
OffChipBuffer::SetChannelDelay(Time t)
{
    NS_LOG_FUNCTION(this << t);
    m_channelDelay = t;
}

Time
OffChipBuffer::GetChannelDelay() const
{
    NS_LOG_FUNCTION(this);
    return m_channelDelay;
}

void
OffChipBuffer::Show()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("\nOffChipBuffer Info: ");
    NS_LOG_DEBUG("\tThe Write-in Cache Size: " << m_wcacheSize);
    NS_LOG_DEBUG("\tThe DRAM Buffer Size: " << m_dramSize);

    // TODO:
    // May need some further LOG which is about ArbAlgorithm and on/off chip BM algorithm.
}

void
OffChipBuffer::ShowCounters()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("\nOffChipBuffer Counters: ");
    NS_LOG_DEBUG("\tUsed Write-in Cache: " << m_wcacheUsed);
    NS_LOG_DEBUG("\tThe Packets num in Write-in Cache: " << m_nWcachePackets);
    NS_LOG_DEBUG("\tUsed The HBM: " << m_dramUsed);
    NS_LOG_DEBUG("\tThe Packets num in: " << m_nDramPackets);
}
//这段代码实现了OffChipBuffer类中的DoReadWriteInterleavedWrr函数，用于执行基于Interleave-WRR（交替权重循环调度）算法的读写操作
OffChipBuffer::ArbResult //函数返回类型为ArbResult，表示调度结果
OffChipBuffer::DoReadWriteInterleavedWrr()
{
    NS_LOG_FUNCTION(this);

    uint32_t m = std::max(m_wrrRWeight, m_wrrWWeight);//取写入权重m_wrrWWeight和读取权重m_wrrRWeight中的最大值
    // Use a uint32_t to represent arbresult:
    // 0 stands for no result, 1 for PKTWRITE and 2 for PKTREAD.//调度结果，0表示无结果，1表示写入（PKTWRITE），2表示读取（PKTREAD）
    uint32_t result = 0;

    /**
     * Note:
     *
     * - If The Arbitration result lead to Write But the //如果仲裁结果导致写操作，但写请求队列为空，则将机会给予读操作，反之亦然
     * Write Request Queue is empty, just give the chance
     * to the Read, vice versa.
     * - For this algorithm of several layers of loops, //对于这个多层循环的算法，由于需要在多个函数调用的断点之间继续执行，可能需要仔细考虑一些边界条件
     * due to the need to continue between breakpoints
     * of several function calls, there may be some corner
     * condition here that need to be carefully considered.
     * - The Method use here is Interleave-WRR. //使用的方法是Interleave-WRR（交替权重循环调度）
     * For the example for Interleave-WRR : 3:2 W:R
     * The WRR Result will be like W-R-W-R-W. And too many W-R
     * Exachange(whether W-R or R-W) will lead to DRAM Bus Bubble
     * and low DRAM Bandwith at last which is not we want.
     * At here we set Patch size, so that the W:R 3:2, Patch Size 5 //例如，对于权重比例为3:2的W:R（写入:读取），WRR结果将会是W-R-W-R-W的交替顺序
     * will be like: WWWWW(5W)-RRRRR(5R)-WWWWW(5W)-RRRRR(5R)-WWWWW(5R)
     */
    while (true)
    {
        // re-initialize
        if (m_interleavedWrrRoundCnt == m)//m_interleavedWrrRoundCnt,用于记录当前轮次的计数
        {
            m_interleavedWrrRoundCnt = 0;//用于记录当前循环的轮次计数，当轮次达到 m 时，重置为0
            m_wrrQueIndex = 0;//m_wrrQueIndex：用于记录当前队列的索引
        }
        // main loop of WrrCnt
        while (m_interleavedWrrRoundCnt < m)//外层循环控制 m_interleavedWrrRoundCnt 小于 m 时的主要逻辑
        {
            // reinitialize for Queue Index overflow.
            if (m_wrrQueIndex == 2)//用于记录当前队列的索引，当索引达到2时，会增加 m_interleavedWrrRoundCnt 并重置索引为0
            {
                m_interleavedWrrRoundCnt++;
                m_wrrQueIndex = 0;
            }
            // main loop of QueIndex.
            for (; m_wrrQueIndex < 2; m_wrrQueIndex++)//内层循环控制 m_wrrQueIndex 在0和1之间循环
            {
                if (m_wrrQueIndex == 0 && m_writeRequestVec.size() &&
                    m_interleavedWrrRoundCnt < m_wrrWWeight)
                {
                    // return PKTWRITE,如果 m_wrrQueIndex 为0 且写请求队列非空且当前轮次小于写权重 m_wrrWWeight 时，返回 PKTWRITE 表示写操作
                    result = 1;
                    break;
                }
                else if (m_wrrQueIndex == 1 && m_readRequestVec.size() &&
                         m_interleavedWrrRoundCnt < m_wrrRWeight)
                {
                    // return PKTREAD,如果 m_wrrQueIndex 为1 且读请求队列非空且当前轮次小于读权重 m_wrrRWeight 时，返回 PKTREAD 表示读操作
                    result = 2;
                    break;
                }
            }
            // return condition.
            if (result)//如果满足条件，则打印调试信息并返回相应的操作类型
            {
                NS_LOG_DEBUG("\t\tRoundCnt: " << m_interleavedWrrRoundCnt
                                              << " Queue Index: " << m_wrrQueIndex);
                m_wrrQueIndex++;//在返回条件下，将 m_wrrQueIndex 增加1，并根据 result 的值返回相应的操作类型
                if (result == 1)
                {
                    NS_LOG_DEBUG("\t\tThe Wrr Result refers to be Writing next cycle!");
                    return PKTWRITE;
                }
                else if (result == 2)
                {
                    NS_LOG_DEBUG("\t\tThe Wrr Result refers to be Reading next cycle!");
                    return PKTREAD;
                }
            }
        }
    }
}

OffChipBuffer::ArbResult
OffChipBuffer::DoReadWriteClassicalWrr()
{
    NS_LOG_FUNCTION(this);
    // Use a uint32_t to represent arbresult:
    // 0 stands for no result, 1 for PKTWRITE and 2 for PKTREAD.
    uint32_t result = 0;

    /**
     * For Classical Wrr
     * Thing become much easier. For example: W:R = 3:2
     * The result will be like: W-W-W-R-R...
     *
     * The implementation method is similar to interleavedWRR.
     */
    while (true)//外层循环控制经典 WRR 算法的主要逻辑
    {
        if (m_wrrQueIndex == 2)//m_wrrQueIndex用于记录当前队列的索引，当索引达到2时，重置为0
        {
            m_wrrQueIndex = 0;
            m_classicalWrrPacketCnt = 0;//m_classicalWrrPacketCnt 用于记录当前轮次已经处理的数据包数量
        }
        // main loop of QueIndex.
        for (; m_wrrQueIndex < 2; m_wrrQueIndex++)//内层循环控制 m_wrrQueIndex 在0和1之间循环
        {
            if (m_wrrQueIndex == 0 && m_writeRequestVec.size() &&
                m_classicalWrrPacketCnt < m_wrrWWeight)
            {
                // PKTWRITE Result,如果 m_wrrQueIndex 为0 且写请求队列非空且当前轮次处理的数据包数量小于写权重 m_wrrWWeight 时，返回 PKTWRITE 表示写操作
                result = 1;
                break;
            }
            else if (m_wrrQueIndex == 1 && m_readRequestVec.size() &&
                     m_classicalWrrPacketCnt < m_wrrRWeight)
            {
                // PKTREAD Result,如果 m_wrrQueIndex 为1 且读请求队列非空且当前轮次处理的数据包数量小于读权重 m_wrrRWeight 时，返回 PKTREAD 表示读操作
                result = 2;
                break;
            }
        }
        // return condition.
        if (result)
        {
            NS_LOG_DEBUG("\t\tPacketCount: " << m_classicalWrrPacketCnt
                                             << " Queue Index: " << m_wrrQueIndex);
            // for every round with a return, we need to add Packet Cnt up.
            m_classicalWrrPacketCnt++;
            if (result == 1)
            {
                // if the Write Deficit is up to Weight just turn to read
                // and set PacketCnt to be 0.
                //如果当前操作是写操作且已处理数据包数量达到写权重，则重置 m_classicalWrrPacketCnt 为0 并将 m_wrrQueIndex 设置为1，表示下一轮应执行读操作
                if (m_classicalWrrPacketCnt >= m_wrrWWeight)
                {
                    m_classicalWrrPacketCnt = 0;
                    // The Weight of Write is done, goto write.
                    m_wrrQueIndex = 1;
                }
                NS_LOG_DEBUG("\t\tThe Wrr Result refers to be Writing next cycle!");
                return PKTWRITE;
            }
            else if (result == 2)
            {
                // if the Write Deficit is up to Weight just turn to read
                // and set PacketCnt to be 0.
                //如果当前操作是读操作且已处理数据包数量达到读权重，则重置 m_classicalWrrPacketCnt 为0 并将 m_wrrQueIndex 设置为0，表示下一轮应执行写操作
                if (m_classicalWrrPacketCnt >= m_wrrRWeight)
                {
                    m_classicalWrrPacketCnt = 0;
                    // The Weight of Write is done, goto write.
                    m_wrrQueIndex = 0;
                }
                NS_LOG_DEBUG("\t\tThe Wrr Result refers to be Reading next cycle!");
                return PKTREAD;
            }
        }
    }
}

OffChipBuffer::ArbResult
OffChipBuffer::ArbitrateWR()
{
    NS_LOG_FUNCTION(this);

    // If the both Requsest Queue is empty, should not use Arbitration Algorithm.
    //首先，该函数确保写请求队列 m_writeRequestVec 或读请求队列 m_readRequestVec 中至少有一个非空，否则会触发断言错误
    NS_ASSERT_MSG(m_writeRequestVec.size() || m_readRequestVec.size(),
                  "When the arbitration algorithm start, the queue should"
                  "not be empty!!!");

    /**
     * To avoid too long Read Queue lead to long Read starving, set the Threshold here.
     * if the readRequestVec is longer than Threshold we need to read it right now, to keep
     * the Output port line rate.
     */
    //如果读请求队列中的请求数量超过了设定的阈值 m_thresholdOfReadRequest，则直接返回 PKTREAD，以避免读请求积压过多导致读操作长时间被阻塞
    if (m_thresholdOfReadRequest && m_readRequestVec.size() >= m_thresholdOfReadRequest)
    {
        NS_LOG_LOGIC("Too many ReadReqs waiting, Just Return the Arb Result to be Read.");
        return PKTREAD;
    }

    /**
     * Note:
     * In order to avoid HBM overflow, if the write size of the head of the queue is larger
     * than the current DRAM capacity, we only allow DRAM read at that moment.
     * So if the WCache-Overflow ASSERT(To assert that the WCache is overflow, at function
     * 'SendWriteRequestStart') is triggered, there may be two resons:
     * 1. The data written into WCache is so fast that even if there is still space in DRAM,
     *    the Data cannot be written into Dram immediately. At this case, Wcache will over-
     *    flow very quickly. The BM should avoid this situation by take DRAM Write-in Bandwidth
     *    into its consideration.
     * 2. DRAM is full and cannot absorb anymore, but the Wcache still write in. BM should take
     *    DRAM Remain Size into PacketLocation consideration.
     */
    uint32_t psize = 0;
    if (m_writeRequestVec.size())
    {
        psize = m_writeRequestVec[0]->GetSize() + SwitchMmu::IPV4_INPUT_PKTSIZE_CORRECTION;
        // There is no free place in Dram for next write-in pkt.
        if (GetDramRemain() < psize)
        {
            // Can not write any more so just turn to read.
            if (m_readRequestVec.size())
            {
                NS_LOG_LOGIC("The HBM is full, so we just allow HBM Read now!");
                return PKTREAD;//如果DRAM剩余容量不足以容纳队首请求，则只允许进行读操作
            }
            else
            {
                // There is no Read request need to be satisfied, and free place in Dram
                // to write-in. OffChipBuffer cannot write or read anymore. So Just return
                // NONE, that the OffChipBuffer Should Stop.
                return NONE;//果DRAM已满且没有未满足的读请求，则返回 NONE，表示不执行任何操作，OffChipBuffer 应该停止
            }
        }
    }

    // TODO:
    // This is easy edition to allocate the bandwidth.
    // At this initial edition, The algorithm
    // Just set WRR and make Write : Read = 3 : 2.
    // There may be more complex edition further.
    // So we just left a frame work here.
    switch (m_arbAlgorithm)
    {
    case INTERLEAVEDWRR:
        return DoReadWriteInterleavedWrr();//如果 m_arbAlgorithm 为 INTERLEAVEDWRR，则调用 DoReadWriteInterleavedWrr() 函数执行交错权重循环调度
    case CLASSICALWRR:
        return DoReadWriteClassicalWrr();//如果 m_arbAlgorithm 为 CLASSICALWRR，则调用 DoReadWriteClassicalWrr() 函数执行经典 WRR 调度
    default:
        return DoReadWriteInterleavedWrr();//如果 m_arbAlgorithm 不匹配任何已知算法，则默认调用 DoReadWriteInterleavedWrr() 函数
    }
}

void
OffChipBuffer::ScheduleCycle()
{
    NS_LOG_FUNCTION(this);//NS_LOG_FUNCTION(this)记录了函数调用

    EventId id;//声明了一个EventId id用于存储事件ID

    /**
     * There may be a need to log the packet path.
     * So we just log something here, like packet uid.
     */
    NS_LOG_DEBUG("This cycle start at " << Simulator::Now());//通过NS_LOG_DEBUG输出当前周期开始的时间
    NS_ASSERT_MSG(m_state != INITIALIZING,
                  "When the Cycle Start,"
                  "The HBM Must Be Ready to W/R!!");//通过NS_ASSERT_MSG检查状态是否为INITIALIZING，如果不是则输出错误信息

    m_state = BUSY;//将状态初始设置为BUSY，表示当前处于忙碌状态

    // There is no W/R Request need to be satisfied.
    if (!m_writeRequestVec.size() && !m_readRequestVec.size())//如果没有写入请求和读取请求需要处理，则将状态设置为IDLE，并返回
    {
        NS_LOG_DEBUG("\tThere is no work left anymore,"
                     "Just Stop it and Set state back to IDLE!");
        m_state = IDLE;
        return;
    }

    // W/R Arbitration and get the W/R request.
    OffChipBuffer::ArbResult result = ArbitrateWR();//调用ArbitrateWR方法进行W/R请求的仲裁，获取仲裁结果

    if (result == NONE)//如果结果为NONE，则将状态设置为IDLE，并返回
    {
        //So just set it to be IDLE. and wait for another Request to wakt it up.
        m_state = IDLE;
        return;
    }
    //下面是根据上一个仲裁结果和当前结果计算等待时间ReqGapTime，用于处理不同类型请求之间的时间间隔
    Time ReqGapTime = Seconds(0);
    // The Time need to wait between two Conflict W/R req.
    if (m_lastArbResult == PKTWRITE)
    {
        /**
         * In order to increase the efficiency of data writing,
         * generally speaking, DRAM writing will use an efficient
         * address selection algorithm to avoid Bank conflicts
         * between writing. So we can just ignore Dram WW Conflict.
         */ //忽略写-写冲突
        if (result == PKTREAD)
        {
            // W-R Conflict.
            ReqGapTime = m_tInterWRReqGap;//10ns
            m_nWrite2ReadCx++;////!< The Write-Read Conflict Num.加1
        }
    }
    else if (m_lastArbResult == PKTREAD)
    {
        if (result == PKTWRITE)
        {
            // R-W Conflict.
            ReqGapTime = m_tInterRWReqGap;//10ns
            m_nRead2WriteCx++;//!< The Read-Write Conflict Num.加1
        }
        else if (result == PKTREAD && ((double)(rand() % 100) / 100) < m_probOfBankCxReadReqs)
        {
            ReqGapTime = m_tInterBankCxReadReqGap;//写-写冲突，20ns
            m_ntwoBankCxRead++;//!< The Bank Conflict Num Between two Read Request.
        }//如果当前结果仍然是PKTREAD，并且随机数小于m_probOfBankCxReadReqs的概率，表示两个读请求之间存在Bank冲突
    }

    Time wait = Seconds(0.0);//初始化等待时间wait为0秒
    // Get the delta time;
    if (m_lastArbResult != NONE)//如果上一个仲裁结果m_lastArbResult不是NONE（表示上一个周期有操作），则计算时间差delta
    {
        Time delta(Simulator::Now() - m_lastCycleEndTime);//获取当前时间与上一个周期结束时间之间的时间差delta

        if (delta < ReqGapTime)//判断delta是否小于等待时间ReqGapTime,如果是，则将等待时间设置为ReqGapTime减去delta，即还需等待的时间
            wait = ReqGapTime - delta;
    }

    if (wait == Seconds(0.0))//如果等待时间wait为0秒，则输出调试信息表示两个周期之间没有时间间隔
    {
        NS_LOG_DEBUG("There is no Time Interval between two Cycle!");
    }
    else//否则，输出等待时间wait的值（以纳秒为单位）
    {
        NS_LOG_DEBUG("The WAIT between two req will be" << wait.As(Time::NS) << "!");
    }

    // Time Wait Between Two HBM operation.
    //根据等待时间wait调度下一个周期的开始，调用OffChipBuffer类中的ScheduleCycleStart方法，并传入参数result
    id = Simulator::Schedule(wait, &OffChipBuffer::ScheduleCycleStart, this, result);
    if (result == PKTWRITE)//如果result为PKTWRITE，则将返回的调度事件id存储在m_interWriteReqWaitEvent中
        m_interWriteReqWaitEvent = id;
}

//据传入的ArbResult参数来确定下一个周期的操作是读操作还是写操作
void
OffChipBuffer::ScheduleCycleStart(OffChipBuffer::ArbResult result)
{
    m_lastArbResult = result;
    Ptr<Packet> p = nullptr;
    uint32_t psize = 0;

    // After Determine the Req to satisfy, clear the Req in Queue.
    //首先根据传入的result参数（PKTWRITE或PKTREAD）确定下一个周期的操作类型。
    if (result == PKTWRITE)//如果result为PKTWRITE，表示下一个周期是写操作
    {
        /**
         * TODO:
         * In order to prevent conflicts in data packet scheduling
         * and DRAM writing, but also to improve the fairness between
         * the writing efficiency of different data flows.
         * We may need to select Write-in Request in random sequence,
         * but it may increase the complexity of hardware queue management.
         * So we still need to figure it out how to deal with it.
         */
        // Get and Pop a req element in front.
        int place = rand() % m_writeRequestVec.size();//通过随机选择一个位置，从m_writeRequestVec中获取一个请求元素，并将其存储在指针p中
        p = m_writeRequestVec[place];
        psize = p->GetSize() + SwitchMmu::IPV4_INPUT_PKTSIZE_CORRECTION;//计算请求数据包的大小psize，并将其设置为p的大小加上一个常数
        m_writingPacket = p;//将选中的数据包p存储在m_writingPacket中，并从m_writeRequestVec中移除该请求元素
        // Clear front Req in the Vec which has been selected.
        auto itr = std::find(m_writeRequestVec.begin(), m_writeRequestVec.end(), p);
        m_writeRequestVec.erase(itr);
    }
    else if (result == PKTREAD)//如果result为PKTREAD，表示下一个周期是读操作
    {
        /**
         * In order to ensure the export line speed as much as possible,
         * we may consider that the next Arbitration result should be PKTREAD
         * in many corner cases in the arbitration logic. So that the number
         * of read operations may even be greater than the number of requests
         * that need to be fulfilled. It may lead to some problem that can not
         * be predicted.
         */
        if (m_readRequestVec.size() == 0)//如果m_readRequestVec为空，则调用ScheduleCycle方法并返回
        {
            ScheduleCycle();
            return;
        }

        /**
         * Note:
         * Unlike the write process, the sequence of the read process has a
         * corresponding Qdisc scheduler for each port(As for real switch maybe
         * a global scheduler for all packet scheduling). Reading in order can
         * ensure that the overall delay and performance are more stable and less shake.
         */
        // Pop a req element in front.
        p = m_readRequestVec[0];//从m_readRequestVec中获取第一个请求元素，并将其存储在指针p中
        psize = p->GetDramStoredSize();//计算请求数据包的大小psize，并将其设置为p的存储大小（GetDramStoredSize）
        // Clear front Req in the Vec which has been selected.
        auto itr = m_readRequestVec.begin();
        m_readRequestVec.erase(itr);//从m_readRequestVec中移除选中的请求元素
    }

    /**
     * Note:
     *
     * According to HW, When they select the data packet to store in the DRAM,
     * they will deliberately ensure that there is no Bank Cx between several
     * reads and writes of a packet.
     *
     * And usually the size of an Internet data packet is not so large that
     * W/R the data packet will definitely access the same Bank continuously.
     *
     * Therefore, we can directly simplify the W/R of a packet to several DRAM
     * burst lengths W/R, without considering the possible Bank Cx between them.
     */

    // Calculate How Much data a HBM op can operate.
    //计算一个HBM（High Bandwidth Memory）操作可以处理的数据量，即burstSize为DRAM位宽（m_dramBitWidth）乘以DRAM burst长度（m_dramBurstLength）
    uint32_t burstSize = m_dramBitWidth * m_dramBurstLength;//m_dramBitWidth=1024bit = 128byte; m_dramBurstLength=4
    // Calculate How Much hbm op need to do.
    uint32_t opTimes = psize / burstSize;//计算需要对数据包进行多少次HBM操作，即opTimes为数据包大小（psize）除以burstSize

    // The Packet will be split into opTimes cell to write in.
    if (psize % (burstSize) != 0)//如果数据包大小不能整除burstSize，则opTimes加一，以确保数据包被分割成opTimes个单元进行写入
    {
        opTimes++;
    }

    //TOOD：给packet设置一个变量finishTime = Simulator::now() + opTimes * 2ns?

    // tracing,根据result的值（PKTWRITE或PKTREAD），调用相应的跟踪函数（m_traceDramWriteStart或m_traceDramReadStart）
    switch (result)
    {
    case PKTWRITE:
        m_traceDramWriteStart(p);
        break;
    case PKTREAD:
        m_traceDramReadStart(p);
        break;
    default:
        break;
    }
    //输出调试信息，包括数据包的唯一标识（UID）和HBM Burst Mode的操作次数
    NS_LOG_DEBUG("\t\tThe Arbitration Result Packet Uid: (" << p->GetUid() << ")");
    NS_LOG_DEBUG("\t\tThe HBM Burst Mode Optimes of this packet Op is " << opTimes);

    ScheduleCycleAtomic(p, result, opTimes);//调用ScheduleCycleAtomic方法，传入数据包p、操作类型result和操作次数opTimes，用于执行DRAM操作的调度
}

void
OffChipBuffer::ScheduleCycleAtomic(Ptr<Packet> packet,
                                   OffChipBuffer::ArbResult result,
                                   uint64_t leftOpCnt)
{
    NS_LOG_FUNCTION(this);

    // If have done all the work or Need Stop writing.
    if (leftOpCnt == 0 || m_writingStopFlag)//首先，方法检查剩余操作次数leftOpCnt是否为0或写入停止标志m_writingStopFlag是否为真
    {
        // Need to stop writing.
        if (m_writingStopFlag && leftOpCnt != 0)//如果m_writingStopFlag为真且leftOpCnt不为0，表示写入被读取操作中断，需要停止写入
        {
            //输出调试信息，包括写入操作被中断的数据包的唯一标识（UID）和剩余操作次数
            NS_LOG_DEBUG("The Writing is interrupted by Read!");
            NS_LOG_DEBUG("The Writing Packet Uid: (" << packet->GetUid() << ")");
            NS_LOG_DEBUG("The Op left in WCache is: " << leftOpCnt);
            //将m_writingStopFlag重置为假，将m_writingPacket设为nullptr
            m_writingStopFlag = false;
            m_writingPacket = nullptr;
        }
        //调用ScheduleCycleComplete方法完成周期调度
        ScheduleCycleComplete(packet, result);//如果leftOpCnt为0或m_writingStopFlag为真，表示已经完成了所有工作或需要停止写入
        return;//直接返回，等待下一个周期再次调度
    }

    uint32_t psize = packet->GetSize();
    uint32_t opTimes = 0;
    uint32_t opsize = 0;

    // Calculate the Opsize and optimes.
    if (result == PKTWRITE)//根据ArbResult的值（PKTWRITE或PKTREAD）来确定是写入操作还是读取操作
    {
        // First Write-in.
        if (packet->GetLocation() == Packet::WCACHE)
        {
            NS_LOG_DEBUG("\tPktWrite: ");
            NS_LOG_DEBUG("\t\tSet Packet to Status: WRITINGTOOFFCHIPBUFFER!");

            // TODO:
            packet->SetLocation(Packet::WRITINGTOOFFCHIPBUFFER);//如果数据包的位置为WCACHE，则将数据包的位置设置为WRITINGTOOFFCHIPBUFFER
        }

        opTimes = std::min(leftOpCnt, m_dramAtomicOpLength);//计算操作次数opTimes为剩余操作次数和DRAM原子操作长度的较小值
        opsize = opTimes * m_dramBitWidth * m_dramBurstLength;//计算操作大小opsize为操作次数乘以DRAM位宽乘以DRAM突发长度
        psize += SwitchMmu::IPV4_INPUT_PKTSIZE_CORRECTION;//输入数据包的大小psize增加一个常量值

        opsize = std::min(opsize, psize - packet->GetDramStoredSize());//将操作大小opsize限制为不超过数据包剩余DRAM存储大小

        /**
         * Note:
         * The Dram Size should be modified before the atomic Op completed,
         * so that if there is read during the writing period, the counters
         * will not be wrong.
         */
        packet->AddMmuDramStoredSize(opsize);//将数据包的DRAM存储大小增加操作大小，更新WCache使用量和DRAM使用量
        m_wcacheUsed -= opsize;
        m_dramUsed += opsize;

        // The psize be bigger than Packet Dram Size at any time.
        NS_ASSERT_MSG(psize >= packet->GetDramStoredSize(),
                    "The Packet Dram size is bigger than packet size!!!");//断言数据包的DRAM存储大小不会超过数据包大小

        // All Pkt in Dram
        if (packet->GetDramStoredSize() == psize)//如果数据包的DRAM存储大小等于数据包大小，则将数据包位置设置为OFFCHIPBUFFER
        {
            packet->SetLocation(Packet::OFFCHIPBUFFER);
            // Write-in-Cache PKts Num --.
            m_nWcachePackets --;//更新WCache和DRAM中的数据包数量
            m_nDramPackets ++;
        }
    }
    else if (result == PKTREAD)//直接进行DRAM读取操作，不会被中断
    {
        // For Dram Read, it won't be interrupted, so just do it all together.
        NS_LOG_DEBUG("\tPktRead: ");

        psize = packet->GetSize();
        opTimes = leftOpCnt;
        opsize = packet->GetDramStoredSize();
        NS_ASSERT_MSG(opsize <= psize, "The Op size must"
                            "be less than the packet size.");

        // tracing
        m_traceDramReadComplete(packet);

        /**
         * Note:
         * The Dram Size should be modified before the atomic Op completed,
         * so that if there is read during the writing period, the counters
         * will not be wrong.
         */
        packet->SubtractMmuDramStoredSize(opsize);
        NS_ASSERT_MSG(m_dramUsed >= opsize, "The Dram left size"
                                "should always be bigger than opsize!!");
        m_dramUsed -= opsize;//将数据包的DRAM存储大小减去操作大小，更新DRAM使用量
    }

    leftOpCnt -= opTimes;//更新剩余操作次数，减去本次操作的次数,然后根据本次操作的次数乘以DRAM突发长度以及DRAM总线频率，计算出原子操作完成的时间
    Time AtomicCompleteTime =
        Seconds(((double)(opTimes * m_dramBurstLength)) / (double)m_dramBusFrequency);

    NS_LOG_DEBUG("\t\tThe Operation Packet Uid: (" << packet->GetUid() << ")");
    NS_LOG_DEBUG("\t\tOpStartTime: " << Simulator::Now().As(Time::NS) << "!");
    NS_LOG_DEBUG("\t\tAtomic Operating Time: " << AtomicCompleteTime);
    //将下一个原子操作的执行调度到AtomicCompleteTime时间点。
    //传递给ScheduleCycleAtomic方法的参数包括当前对象指针this，数据包指针packet，操作结果result和剩余操作次数leftOpCnt
    Simulator::Schedule(AtomicCompleteTime,
                        &OffChipBuffer::ScheduleCycleAtomic,
                        this,
                        packet,
                        result,
                        leftOpCnt);
}

void
OffChipBuffer::ScheduleCycleComplete(Ptr<Packet> packet, OffChipBuffer::ArbResult result)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("\tThis cycle complete at " << Simulator::Now());//输出当前周期结束的时间

    uint32_t psize = packet->GetSize();//获取数据包的大小

    if (result == PKTWRITE)//如果是PKTWRITE，则增加数据包大小并执行一些清除操作，增加HBM总数据包计数，进行跟踪
    {
        psize += SwitchMmu::IPV4_INPUT_PKTSIZE_CORRECTION;

        // Clear Writing Packets
        m_writingPacket = 0;

        // Add HBM total Packets Counter
        m_stats.nTotalDramStoredPackets++;
        // tracing
        m_traceDramWriteComplete(packet);
    }
    else if (result == PKTREAD)//如果是PKTREAD，则根据数据包存储大小进行处理，调用FetchComplete方法，告知目标网络设备可以发送数据包
    {
        // HBM clear
        if (packet->GetDramStoredSize() == 0)
        {
            m_nDramPackets -= 1;
        }
        // After Finished the Packet HBM Read Modify some counters
        m_mmu->FetchComplete(packet);

        // Add target netdevice enqueue to tell the netdevice that
        // Packet has been Read from Buffer. It is Time to send it.
        uint32_t port = packet->GetMmuUsedPort();
        if (!m_node)
        {
            m_node = m_mmu->GetObject<Node>();
            NS_ASSERT_MSG(m_node, "Cannot find node aggregated to this mmu");
        }
        Ptr<NetDevice> dev = m_node->GetDevice(port);
        m_mmu->HandleRequest(dev);
    }

    // There may be need a Gap Time Between Two Packet Handle Cycle.//根据操作结果输出不同的调试信息
    if (result == PKTREAD)
    {
        NS_LOG_DEBUG("\tPktRead: ");
    }
    else
    {
        NS_LOG_DEBUG("\tPktWrite: ");
    }

    NS_LOG_DEBUG("\t\tThe Operation Packet Uid: (" << packet->GetUid() << ")");
    NS_LOG_DEBUG("\t\tEndTime: " << Simulator::Now().As(Time::NS) << "!");

    // Log End Time
    m_lastCycleEndTime = Simulator::Now();//记录当前周期结束的时间作为上一个周期结束时间

    // Reschedule
    ScheduleCycle();//调用ScheduleCycle方法，重新调度下一个周期的处理

    return;
}


bool
OffChipBuffer::Write(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    //获取数据包使用的端口、队列ID和优先级，以及数据包的位置
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    Packet::Location location = packet->GetLocation();

    // If already in the OFFCHIP or Wcache, Write should return error.
    //如果数据包已经在OffChipBuffer中写入、OffChipBuffer中、或者在Wcache中，则直接返回错误
    if (location == Packet::WRITINGTOOFFCHIPBUFFER ||
        location == Packet::OFFCHIPBUFFER ||
        location == Packet::WCACHE)
    {
        return false;
    }

    NS_LOG_DEBUG("OffChipBuffer: Write.");
    NS_LOG_DEBUG("Write Start Time: " << Simulator::Now().As(Time::S));
    NS_LOG_DEBUG("The Write Packet Uid: (" << packet->GetUid() << ")");
    NS_LOG_DEBUG("\tThe input Packet's output port: " << port << " Queue Index: " << qIndex
                                                      << " Packet Priority: " << priority);

    // Add Total packets counters.
    m_stats.nTotalWcacheStoredPackets++;//增加总的写入数据包计数器

    return SendWriteRequestStart(packet);//调用SendWriteRequestStart方法发送写请求并返回结果
}

bool
OffChipBuffer::SendWriteRequestStart(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    NS_LOG_DEBUG("Write Request Send Begin!");
    NS_LOG_DEBUG("The HBM Packet of Write Req is (" << packet->GetUid() << ")");

    uint32_t psize = packet->GetSize() + SwitchMmu::IPV4_INPUT_PKTSIZE_CORRECTION;

    // Check if wcache is still enough for the packet;

    NS_LOG_DEBUG("\tWcache Size:: " << m_wcacheSize << " usedWcacheSize:: " << m_wcacheUsed
                                    << " psize: " << psize);
    // WCache-Overflow ASSERT.
    NS_ASSERT_MSG(
        m_wcacheUsed <= m_wcacheSize &&
            psize <= (m_wcacheSize - m_wcacheUsed),
        "Packet size exceeds the remaining wcache space!");

    // Write in the wcache First.
    // First, Modify some Wcache counters.
    m_wcacheUsed += psize;//更新wcache的已使用大小和包数量
    m_nWcachePackets++;

    // Second, add the Address Map.
    // set the Map to be 1 to represent the Packet is in wcache.
    packet->SetLocation(Packet::WCACHE);//置数据包的位置为wcache

    m_traceWcacheWriteComplete(packet);

    // Send Request to HBM.
    // TODO:
    // 1. Check if Schedule 0.0s to be legal.
    //发送写请求到HBM
    if (m_dramBusFrequency)
    {
        if (m_channelDelay == Seconds(0.0))
        {
            SendWriteRequestComplete(packet);//如果m_dramBusFrequency为真且m_channelDelay为0秒，则立即调用SendWriteRequestComplete方法
        }
        else//否则，使用Simulator::Schedule延迟调用SendWriteRequestComplete方法
        {
            Simulator::Schedule(m_channelDelay, &OffChipBuffer::SendWriteRequestComplete, this, packet);
        }
    }


    // After wcache Ingress and set req push back into Queue,
    // the Write should return 'true'.
    // TODO:
    // 1. Check if there are some Write-in Failure situation.
    return true;//返回true表示写请求成功发送
}

void
OffChipBuffer::SendWriteRequestComplete(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("The Write Request Get to The HBM at " << Simulator::Now().As(Time::S));

    // If the Packet is not in wcache now, It means that all Packet has been
    // fetched by switch, so it does not need write to HBM any more, just return.
    if (packet->GetLocation() == Packet::NOTINBUFFER)//如果数据包不在缓存中（NOTINBUFFER），表示所有数据包已经被交换机获取，无需再写入HBM，直接返回
    {
        NS_LOG_DEBUG("\t\tCondition 1: Should not in Req Queue! Drop it!");
        return;
    }

    // Push the Address into the Write-Req-Queue.
    m_writeRequestVec.push_back(packet);//将地址推入写请求队列（m_writeRequestVec）
    NS_LOG_DEBUG("Write Request push in Vec.");

    // If the HBM engine is IDLE Now, Start it.
    if (m_state == IDLE)//果HBM引擎当前处于空闲状态（IDLE），则启动HBM引擎
    {
        NS_LOG_DEBUG("\tWrite: The Off-Chip-Buffer is free now, just kick it!");
        ScheduleCycle();//用ScheduleCycle方法，开始处理下一个周期的操作
    }
}

bool
OffChipBuffer::Read(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    Packet::Location location = packet->GetLocation();

    // Not in OffChipBuffer.
    //如果数据包不在Off-Chip缓存（WRITINGTOOFFCHIPBUFFER、OFFCHIPBUFFER、WCACHE）中，则输出错误信息并返回false
    if (location != Packet::WRITINGTOOFFCHIPBUFFER &&
        location != Packet::OFFCHIPBUFFER &&
        location != Packet::WCACHE)
    {
        NS_LOG_ERROR("Could not read the packet which is not in OFFCHIPBUFFER!\n");
        return false;
    }
    //输出调试信息，表示Off-Chip缓存正在读取数据包，显示读取开始时间、数据包唯一标识符以及相关MMU信息
    NS_LOG_DEBUG("OffChipBuffer: Read!");
    NS_LOG_DEBUG("Read Start Time: " << Simulator::Now().As(Time::S));
    NS_LOG_DEBUG("The Read Packet Uid: (" << packet->GetUid() << ")");
    NS_LOG_DEBUG("\tThe input Packet's Output port: " << port << " Queue Index: " << qIndex
                                                      << " Packet Priority: " << priority);

    return SendReadRequestStart(packet);//调用SendReadRequestStart方法开始发送读请求，并根据数据包的位置进行相应操作
}

bool
OffChipBuffer::SendReadRequestStart(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    NS_LOG_DEBUG("Read Request Send Begin!");
    NS_LOG_DEBUG("The HBM Packet of Read REQ IS (" << packet->GetUid() << ")");

    // Check whether the Pkt is in wcache or not.
    //检查数据包的位置，如果数据包在写缓存（WRITINGTOOFFCHIPBUFFER）或者写缓存中（WCACHE），则执行以下操作
    if (packet->GetLocation() == Packet::WRITINGTOOFFCHIPBUFFER ||
        packet->GetLocation() == Packet::WCACHE)
    {
        NS_ASSERT_MSG(packet->GetDramStoredSize() < packet->GetSize(),
                      "The status of Packet location is not right!");//断言数据包的DRAM存储大小小于数据包的大小，如果不满足则输出错误信息
        // still in wcache not full in HBM.
        NS_LOG_DEBUG("\tWcache hit!!");//输出调试信息，表示数据包在写缓存中

        // For all the condition that we directly read the Packet from
        // Wcache, we need to 'Undo' the corresponding Write Operation.
        CancelWrite(packet); //调用CancelWrite方法取消对应的写操作
    }
    //如果数据包在写缓存或者Off-Chip缓存（WRITINGTOOFFCHIPBUFFER或者OFFCHIPBUFFER），则执行以下操作
    if (packet->GetLocation() == Packet::WRITINGTOOFFCHIPBUFFER ||
        packet->GetLocation() == Packet::OFFCHIPBUFFER)
    {
        NS_ASSERT_MSG(packet->GetDramStoredSize() != 0,
                     "The Status of Packet Location is not right!");//断言数据包的DRAM存储大小不为0，如果不满足则输出错误信息
        // The Packet has some part(or fully) in Dram.
        NS_LOG_DEBUG("\tDram hit!!");//出调试信息，表示数据包在DRAM中

        // Send Request to HBM.
        if (m_channelDelay == Seconds(0.0))//根据通道延迟（m_channelDelay）决定是立即发送读请求完成信号还是延迟发送
        {
            SendReadRequestComplete(packet);//如果通道延迟为0，直接调用SendReadRequestComplete方法完成读请求
        }
        else //如果通道延迟不为0，使用Simulator::Schedule延迟发送读请求完成信号
        {
            Simulator::Schedule(m_channelDelay,
                                &OffChipBuffer::SendReadRequestComplete,
                                this,
                                packet);
        }
    }

    return true;
}

void
OffChipBuffer::SendReadRequestComplete(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    NS_LOG_DEBUG("The Read Request Get to The HBM at " << Simulator::Now().As(Time::S));
    // Push the Req into the Read-Req-Queue
    m_readRequestVec.push_back(packet);       //将读请求数据包添加到读请求队列m_readRequestVec中
    NS_LOG_DEBUG("Read Request push in Vec"); //出调试信息，表示读请求已经被添加到队列中

    // If the HBM Engine is free, start it.
    if (m_state == IDLE) //如果HBM引擎当前空闲（状态为IDLE），则启动HBM引擎
    {
        NS_LOG_DEBUG("\tThe Off-Chip-Buffer is free now, just kick it!");
        ScheduleCycle(); //启动HBM引擎的方法是调用ScheduleCycle方法
    }
}

//取消HBM写操作,在OffChipBuffer::SendReadRequestStart(Ptr<Packet> packet)中调用
void
OffChipBuffer::CancelWrite(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    /**
     * Note:
     * This func is used to cancel the HBM Write operation. But for the complex
     * mechanism of OffChipBuffer Writing. The Cancel process may have three conditions:
     *
     * 1. The Write Request is in the Channel, not in the Request Vec yet//写请求在通道中，但尚未加入请求队列
     * In this condition, the Req in Channel may be a lot. So, we may not be
     * able to find the corresponding req. To solve it, before enqueue in the
     * writeVec, we will check if the req is legal.
     *
     * 2. The Write Request is in waiting the Request Vec now, but not staisfied.//写请求当前在请求队列中等待，但尚未满足条件
     * Solution: Find the Write Request and Clear it.
     *
     * 3. The write is being done by HBM, but not completed write in.//写操作正在HBM中进行，但尚未完成
     * Solution: Stop the HBM Writing Operation, and Start another HBM 'Cycle'.
     */
    uint32_t pDramSize = packet->GetDramStoredSize();//获取数据包的DRAM存储大小和总大小，计算wcache大小
    uint32_t psize = packet->GetSize();
    uint32_t pWcacheSize = psize - pDramSize;

    NS_LOG_DEBUG("Packet Dram Size: " << pDramSize);
    NS_LOG_DEBUG("Packet Wcache Size: " << pWcacheSize);

    // Update some counters.
    m_wcacheUsed -= pWcacheSize;//新wcache已使用大小和包数量
    m_nWcachePackets--;

    // trace
    m_traceWcacheReadComplete(packet);//跟踪wcache读完成的情况

    // Condition 3:
    if (m_writingPacket == packet)//如果当前正在写的数据包是要取消的数据包，则停止HBM写操作
    {
        NS_LOG_DEBUG("\t\tCondition 3: Cancel Write operation!");
        m_writingStopFlag = true;
    }
    else
    {
        // Condition 1, 2:
        auto itr = find(m_writeRequestVec.begin(), m_writeRequestVec.end(), packet);
        if (itr == m_writeRequestVec.end())//如果数据包不在写请求队列中，则输出调试信息
        {
            // Condition 1.
            // The req is at Channel.
            // Just Print out something.
            NS_LOG_DEBUG("\t\tThe packet that switch want to fetch "
                         "is not in the OffChipBuffer Req Vec!");
        }
        else//否则，从写请求队列中清除该数据包，并根据情况判断是否需要重新调度循环
        {
            // Condition 2.
            // Clear the m_writeRequestVec
            NS_LOG_DEBUG("\t\tCondition 2: Clear the Request Queue!");
            m_writeRequestVec.erase(itr);

            /**
             * If clear the last write req and it May need to reschedule
             * The Cycle and redo the W/R Arbitration.
             */
            if (m_writeRequestVec.size() == 0 && m_interWriteReqWaitEvent.IsRunning())
            {
                Simulator::Cancel(m_interWriteReqWaitEvent);
                ScheduleCycle();
            }
        }
    }

    // If all in WCache.
    if (packet->GetDramStoredSize() == 0)//如果数据包完全在wcache中，则通知MMU数据包的获取操作已完成
    {
        m_mmu->FetchComplete(packet);
    }
}

uint64_t
OffChipBuffer::GetWcacheUsed() const
{
    NS_LOG_FUNCTION(this);
    return m_wcacheUsed;
}

uint64_t
OffChipBuffer::GetDramUsed() const
{
    NS_LOG_FUNCTION(this);
    return m_dramUsed;
}

uint64_t
OffChipBuffer::GetDramRemain() const
{
    NS_LOG_FUNCTION(this);
    return m_dramSize - m_dramUsed;
}
} // namespace ns3
