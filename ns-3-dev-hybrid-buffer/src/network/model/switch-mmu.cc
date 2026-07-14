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

#include "switch-mmu.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "off-chip-buffer.h"

#include "ns3/boolean.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-reorder-net-device.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

using namespace std;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE(
    "SwitchMmu"); // NS_LOG_COMPONENT_DEFINE 宏用于定义一个日志组件，其中参数 "SwitchMmu"
                  // 是该日志组件的名称，控制日志输出级别和过滤日志消息

NS_OBJECT_ENSURE_REGISTERED(SwitchMmu); // 一个宏，用于确保特定类在运行时被注册了

SwitchMmu::Stats::
    Stats() // SwitchMmu的内部类Stats的构造函数的实现，在构造函数中，对Stats类的成员变量
    : nTotalStoredPackets(
          0), // nTotalStoredPackets、nTotalBmDropPackets和nTotalOnChipBufferStoredPackets进行了初始化。
      nTotalBmDropPackets(0),
      nTotalBmDropPacketsSize(0),
      nTotalOnChipBufferStoredPackets(0),
      perusStoredPackets(0)
{
}

TypeId                 // TypeId是一个类型，表示某个类型的标识符。
SwitchMmu::GetTypeId() // 段代码是C++中类SwitchMmu的成员函数GetTypeId的实现
{
    static TypeId
        tid = // static关键字使得tid成为静态局部变量，即在函数调用结束后，tid的值仍然保留，直到程序结束
        TypeId(
            "ns3::SwitchMmu") // TypeId("ns3::SwitchMmu")创建了一个TypeId对象，表示类型为ns3::SwitchMmu
            .SetParent<Object>()         // 设置SwitchMmu类的父类为Object
            .SetGroupName("Network")     // 设置SwitchMmu类所属的组名为"Network"
            .AddConstructor<SwitchMmu>() // 添加SwitchMmu类的构造函数
            .AddAttribute( // 添加一个属性，包括属性名称、属性描述、属性值、属性访问器和属性检查器。具体参数解释如下：
                "SwitchMemType", // 属性名称为"SwitchMemType"
                "The Switch memory type(only SRAM or hybrid buffer)",
                EnumValue(SwitchMmu::BufferModel(
                    ONOFFCHIP)), // 属性值，使用枚举值ONOFFCHIP初始化BufferModel
                MakeEnumAccessor(&SwitchMmu::SetMemType,
                                 &SwitchMmu::GetMemType), // 属性访问器，用于设置和获取属性值
                MakeEnumChecker(
                    SwitchMmu::ONCHIP,
                    "OnChip",
                    SwitchMmu::ONOFFCHIP,
                    "OnOffChip")) // 属性检查器，用于检查属性值的有效性，接受两对参数，每对参数包括一个枚举值和对应的描述
            .AddAttribute(
                "OnChipBufferSize", // 属性名
                "Onchip buffer size",
                // I don't why, but without static_cast, there will be linking error
                UintegerValue(static_cast<uint64_t>(
                    DEFAULT_ONCHIPBUFFER_SIZE)), // 使用DEFAULT_ONCHIPBUFFER_SIZE的值初始化为64位无符号整数类型
                MakeUintegerAccessor(
                    &SwitchMmu::SetOnChipBufferSize, // 属性访问器，用于设置和获取属性值
                    &SwitchMmu::GetOnChipBufferSize),
                MakeUintegerChecker<
                    uint64_t>()) // 属性检查器，用于检查属性值的有效性，这里指定了属性值为64位无符号整数类型
            .AddAttribute("ReorderBufferSize",
                          "Reorder buffer size",
                          UintegerValue(static_cast<uint64_t>(DEFAULT_REORDERBUFFER_SIZE)),
                          MakeUintegerAccessor(&SwitchMmu::SetReorderBufferSize,
                                               &SwitchMmu::GetReorderBufferSize),
                          MakeUintegerChecker<uint64_t>())
            /**
             * Note:
             * As tests suggested: The CreateObject (), calling GetTypeId after
             * Constructor function. So the counters 'resize' work should be finished
             * by setting functions like 'SetPortNum' and 'SetQueueNum'. Also the
             * sequence of Attribute setting function is the sequence of Atrribute.
             * So that the calling sequence will be as follow:
             * SetPortNum -> SetPriorityNum -> SetQueueNum -> Set...
             * If there is a need to refactor the attr here, please pay attention.
             */
            .AddAttribute(
                "PortNumber",
                "The number of ports",
                UintegerValue(128), // 表示属性的默认值为64，即默认端口数量为64。
                MakeUintegerAccessor(&SwitchMmu::SetNPorts, &SwitchMmu::GetNPorts),
                MakeUintegerChecker<
                    uint32_t>()) // 建了一个检查器，用于检查属性值的类型是否为uint32_t，即无符号32位整数类型
            .AddAttribute(
                "PriorityNumber",
                "The number of priorities",
                UintegerValue(2), // 表示属性的默认值为2，即默认优先级数量为2
                MakeUintegerAccessor(&SwitchMmu::SetNPriorities, &SwitchMmu::GetNPriorities),
                MakeUintegerChecker<
                    uint32_t>()) // 创建了一个检查器，用于检查属性值的类型是否为uint32_t，即无符号32位整数类型
            .AddAttribute("QueueNumber",
                          "The number of queues",
                          UintegerValue(8), // 表示属性的默认值为8，即默认队列数量为8
                          MakeUintegerAccessor(&SwitchMmu::SetNQueues, &SwitchMmu::GetNQueues),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Simlulator_time_stop",
                          "The number of queues",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&SwitchMmu::Simlulator_time_stop),
                          MakeDoubleChecker<double>())
            .AddAttribute("now_algorithm_name",
                          "Current buffer management algorithm directory name",
                          StringValue("BMS"), //先默认设置为BMS
                          MakeStringAccessor(&SwitchMmu::now_algorithm_name),
                          MakeStringChecker())
            .AddAttribute("BMAlgorithm",
                          "Buffer management algorithm",
                          EnumValue(SwitchMmu::BmAlgorithm(TDT)),
                          MakeEnumAccessor(&SwitchMmu::SetBmAlgorithm, &SwitchMmu::GetBmAlgorithm),
                          MakeEnumChecker(SwitchMmu::HW,
                                          "HW",
                                          SwitchMmu::YSL,
                                          "YSL",
                                          SwitchMmu::DEEPHIR,
                                          "DEEPHIR",
                                          SwitchMmu::TDT,
                                          "TDT",
                                          SwitchMmu::BASELINE,
                                          "BASELINE",
                                          SwitchMmu::YRF,
                                          "YRF"))
            .AddAttribute("baseFilePath",
                        "Base directory for output files",
                        StringValue("./"),
                        MakeStringAccessor(&SwitchMmu::baseFilePath),
                        MakeStringChecker())
            .AddAttribute(
                        "nextFilePath",
                        "Additional output subdirectory",
                        StringValue(""),
                        MakeStringAccessor(&SwitchMmu::nextFilePath),
                        MakeStringChecker())
            .AddAttribute("if_change_threshold",
                          "改变DT阈值标志",
                          UintegerValue(0),
                          MakeUintegerAccessor(&SwitchMmu::if_change_threshold),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("flow_rate",
                          "流量速度",
                          UintegerValue(100),
                          MakeUintegerAccessor(&SwitchMmu::flow_rate),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("if_test8",
                          "是否是测试用例8",
                          UintegerValue(0),
                          MakeUintegerAccessor(&SwitchMmu::if_test8),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("if_test9",
                          "是否是测试用例9",
                          UintegerValue(0),
                          MakeUintegerAccessor(&SwitchMmu::if_test9),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Deeohir_threshold",
                          "Deephir阈值",
                          DoubleValue(2),
                          MakeDoubleAccessor(&SwitchMmu::Deeohir_threshold),
                          MakeDoubleChecker<double>())
            .AddAttribute("EWMA_W",
                          "FBM 平滑因子γ",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&SwitchMmu::EWMA_W),
                          MakeDoubleChecker<double>())
            .AddAttribute("eta_MD",
                          "FBM MD因子η",
                          DoubleValue(1),
                          MakeDoubleAccessor(&SwitchMmu::eta_MD),
                          MakeDoubleChecker<double>())              
            .AddAttribute("LruUpdateTimeWindow", // 是属性的名称，表示LRU更新状态之间的时间
                          "The Time between LRU Update Status",
                          TimeValue(MicroSeconds(
                              100)), // 属性的默认值为100微秒，即LRU更新状态之间的时间默认为100微秒
                          MakeTimeAccessor(&SwitchMmu::m_updateLruTimeWindow),
                          MakeTimeChecker())
            .AddAttribute("CycleTimeLength",
                          "The Time length of cycle",
                          TimeValue(MicroSeconds(1)),
                          MakeTimeAccessor(&SwitchMmu::m_CycleTimeLength),
                          MakeTimeChecker())
            .AddTraceSource(
                "Store", //"Store"是追踪源的名称，表示一个数据包已经被存储
                "A packet has been stored",
                MakeTraceSourceAccessor(
                    &SwitchMmu::
                        m_traceStore), // 创建了一个追踪源访问器，其中SwitchMmu::m_traceStore是用于存储追踪信息的成员变量。这样可以通过访问器来添加、存储和获取追踪信息
                "ns3::Packet::TracedCallback") // 是追踪源的类型，表示追踪的信息类型为ns3::Packet的追踪回调函数
            .AddTraceSource("Fetch",
                            "A packet has been fetched",
                            MakeTraceSourceAccessor(&SwitchMmu::m_traceFetch),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("CheckAdmission",
                            "A packet has been checked admission",
                            MakeTraceSourceAccessor(&SwitchMmu::m_traceCheckAdmission),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("PacketsInBuffer",
                            "Number of packets currently stored in the buffer",
                            MakeTraceSourceAccessor(&SwitchMmu::m_nPackets),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("SramReadComplete",
                            "A packet has been read from the SRAM",
                            MakeTraceSourceAccessor(&SwitchMmu::m_traceSramReadComplete),
                            "ns3::Packet::TracedCallBack")
            .AddTraceSource("SramWriteComplete",
                            "A packet has been written into SRAM",
                            MakeTraceSourceAccessor(&SwitchMmu::m_traceSramWriteComplete),
                            "ns3::Packet::TracedCallBack");

    return tid;
}

// 这段代码是一个类中的成员函数，用于返回该类的实例类型ID
TypeId                               // 是一个类型，通常用于标识类的类型
SwitchMmu::GetInstanceTypeId() const // 是一个成员函数，用于返回SwitchMmu类的实例类型ID
{
    return GetTypeId(); // GetTypeId()是一个函数，通常是从父类中继承而来，用于获取当前类的类型ID
}

SwitchMmu::SwitchMmu() // 构造函数SwitchMmu::SwitchMmu()被定义,来完成对象的初始化工作
{
    NS_LOG_FUNCTION(this);

    m_nPackets = 0;
    m_offChipBuffer = nullptr;

    // Set the default value of HW BM
    m_enableOnChipPdp = true;
    m_cgMax = {{1500 * 1024, 1200 * 1024}, // m_cgMax[type][pri]
               {1700 * 1024, 1400 * 1024},
               {2300 * 1024, 1700 * 1024}};
    m_cgMin = {{1100 * 1024, 700 * 1024}, // m_cgMin[type][pri]
               {1200 * 1024, 800 * 1024},
               {1800 * 1024, 1100 * 1024}};
    m_longCgQlen = {{130 * 1024, 80 * 1024}, // m_longCgQlen[portType][priority]
                    {150 * 1024, 90 * 1024},
                    {210 * 1024, 120 * 1024}};

    m_wcacheFullTh = {350 * 1024, 200 * 1024}; // m_wcacheFullTh[priority]
    m_wcacheCgTh = {200 * 1024, 150 * 1024};   // m_wcacheCgTh[priority]
    m_alphaOfPort = {0.5, 2, 4};               // m_alphaOfPort[portType]
    m_alphaOfPriority = {18, 2};               // m_alphaOfPriority[priority]

    m_alphaOfQueue = {
        // m_alphaOfQueue[portType][type][priority][qIndex]
        {{{8, 6, 5},
          {9.0 / 16, 9.0 / 16, 9.0 / 16, 9.0 / 16, 17.0 / 32}}, // 100Gbps port,上行数据流
         {{10, 9, 8.5}, {5.0 / 8, 5.0 / 8, 5.0 / 8, 5.0 / 8, 9.0 / 16}}}, // 100Gbps port,下行数据流
        {{{32, 16, 12}, {3.0 / 4, 3.0 / 4, 3.0 / 4, 3.0 / 4, 5.0 / 8}}, // 400Gbps port,上行数据流
         {{10, 9, 8.5}, {5.0 / 8, 5.0 / 8, 5.0 / 8, 5.0 / 8, 9.0 / 16}}}, // 100Gbps port,下行数据流
        {{{64, 32, 16}, {1.0, 1.0, 1.0, 1.0, 3.0 / 4}}, // 800Gbps port,上行数据流
         {{64, 32, 16}, {1.0, 1.0, 1.0, 1.0, 3.0 / 4}}} // 100Gbps port,下行数据流
    };
    // m_wredTh = {13100000, 13100000};//加权随机早期丢弃（Weighted Random Early
    // Detection，WRED）是一种流量管理技术，用于避免网络拥塞,m_wredTh[priority]
    m_wredTh = {2 * 1024 * 1024, 2 * 1024 * 1024}; // 2MB
    YRF_Flag_First = true;                         // 第一个周期开始时为true
    DeepHir_Flag = false;

    W = 0.9;
    W1 = 0.45;
    W2 = 1;
    W3 = 2;
    m_dramAlphaOfPriority = {18, 2};   // 3DT算法
    m_wcacheAlphaOfPriority = {18, 2}; // 3DT算法
    m_dramAlphaOfQueue = {{{12, 11, 10}, {3.0 / 4, 3.0 / 4, 3.0 / 4, 3.0 / 4, 5.0 / 8}}, // 3DT算法
                          {{10, 8, 7}, {5.0 / 8, 5.0 / 8, 5.0 / 8, 5.0 / 8, 9.0 / 16}}};
    m_wcacheAlphaOfQueue = {
        {{12, 11, 10}, {3.0 / 4, 3.0 / 4, 3.0 / 4, 3.0 / 4, 5.0 / 8}}, // 3DT算法
        {{10, 8, 7}, {5.0 / 8, 5.0 / 8, 5.0 / 8, 5.0 / 8, 9.0 / 16}}};

    LossPacketNum_Last = 0;
    LossPacketNumTotalSizeLast = 0;
    Timer_Mill_Loss = Seconds(0);

    Simulator::Schedule(NanoSeconds(Dram_Bandwidth_Timer), &SwitchMmu::CountDramBandwidth, this);
    Simulator::Schedule(NanoSeconds(Sram_ThroughputDiff_Timer), &SwitchMmu::CountSramThroughputDiff, this);
    // CountDramBandwidth();
    // CountSramThroughputDiff();
}

// 析构函数中，通常会执行一些清理工作，比如释放资源、关闭文件等操作。在这里的代码中，使用了NS_LOG_FUNCTION宏，可能是用于调试目的，用于输出函数的调用信息
// 这段代码定义了一个空的析构函数~SwitchMmu()，并使用NS_LOG_FUNCTION宏输出函数调用信息
SwitchMmu::~SwitchMmu()
{
    NS_LOG_FUNCTION(this);
}
//文件的写入改了个位置，
std::string
SwitchMmu::GetLossPacketFilePath() const  
{
    namespace fs = std::filesystem;
    // baseFilePath 已经是：
    // .../tests/data/pbs/tc2-05/
    fs::path outputDirectory(baseFilePath);
    // tc2-08 等特殊实验需要继续增加参数目录
    return (outputDirectory / "loss_packet.csv").string();
}


void
SwitchMmu::InitializeLossPacketFile()
{
    namespace fs = std::filesystem;
    const std::string fileName = GetLossPacketFilePath();
    const fs::path filePath(fileName);
    const fs::path outputDirectory = filePath.parent_path();
    std::error_code errorCode;
    fs::create_directories(outputDirectory, errorCode);

    if (errorCode)
    {
        NS_FATAL_ERROR("创建 loss_packet.csv 输出目录失败。" << " directory=" << outputDirectory.string() << " error=" << errorCode.message());
    }
    std::ofstream fout(fileName, std::ios::out | std::ios::trunc);
    if (!fout.is_open())
    {
        NS_FATAL_ERROR(
            "无法创建 loss_packet.csv。"
            << " fileName=" << fileName);
    }
    /*
     * 下面实际写入6列数据，因此表头也必须是6列。
     */
    // fout << "TimeStart,"
    //         "TimeEnd,"
    //         "LossPacketSizeKbit,"
    //         "CumulativeLossRate,"
    //         "PeriodLossRate,"
    //         "CumulativeLossPacketNum"
    //      << std::endl;
    fout.close();
    std::cout << "[LossPacketCSV] 创建成功: "<< fileName << std::endl;
}

// 这段代码定义了一个返回m_stats成员变量常引用的成员函数GetStats()，并在函数内部输出函数的调用信息
const SwitchMmu::Stats&
SwitchMmu::GetStats()
{
    NS_LOG_FUNCTION(this);
    return m_stats;
}

// Used to track changes in Wcache and Dram registers
void
SwitchMmu::ReadWcacheComplete(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this);

    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pDramSize = packet->GetDramStoredSize(); // 数据包存储在DRAM中的大小
    uint32_t psize = packet->GetSize();               // 数据包总大小
    uint32_t pWcacheSize = psize - pDramSize;         // 得到数据包存储在Wcache中的大小

    m_wcacheQlen[port][priority][qIndex] -= pWcacheSize; // 计算Wcache中剩余存储大小
    m_priWcacheUsed[priority] -= pWcacheSize;            // 优先级量剩余大小
}

void
SwitchMmu::WriteWcacheComplete(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this);

    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t psize =
        packet->GetSize() +
        SwitchMmu::
            IPV4_INPUT_PKTSIZE_CORRECTION; // IPV4_INPUT_PKTSIZE_CORRECTION=22,为一个常量，为修正而存在

    m_wcacheQlen[port][priority][qIndex] += psize; // 写数据包并更新Wcache存储值大小
    m_priWcacheUsed[priority] += psize;
}

void
SwitchMmu::ReadDramComplete(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this);

    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t dramsize = packet->GetDramStoredSize();

    m_dramQlen[port][priority][qIndex] -= dramsize;
    m_priDramUsed[priority] -= dramsize;
}

void
SwitchMmu::WriteDramComplete(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this);

    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t dramsize = packet->GetDramStoredSize();
    WriteDram_Size_Cycle[port][priority][qIndex] += dramsize; // 统计周期内写入DRAM的数据包大小

    m_dramQlen[port][priority][qIndex] += dramsize; // 从Wcache中读取数据包并更新DRAM存储值大小
    m_priDramUsed[priority] += dramsize;
    m_wcacheQlen[port][priority][qIndex] -=
        dramsize; // Wcache中的数据包被写入DRAM并更新Wcache存储值大小
    m_priWcacheUsed[priority] -= dramsize;
}

// 这段代码是一个名为SwitchMmu的类中的一个成员函数SetNode的实现
// 该函数接受一个类型为Ptr<Node>的参数node，并将其设置为类中的成员变量m_node。
void
SwitchMmu::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

// 该函数用于在对象销毁时执行必要的清理操作
void
SwitchMmu::DoDispose()
{
    NS_LOG_FUNCTION(
        this); // 调用了NS_LOG_FUNCTION宏，该宏通常用于记录函数的调用信息。在这里，它记录了当前对象的地址（this）
    m_node = nullptr; // 将类中的成员变量m_node设置为nullptr，以释放对节点的引用
    Object::DoDispose(); // 调用了基类Object的DoDispose函数，以执行基类的清理操作
}

// 该函数用于在对象初始化时执行必要的操作
void
SwitchMmu::DoInitialize()
{
    NS_LOG_FUNCTION(this); // 记录了当前对象的地址（this）

    m_node = nullptr; // m_node设置为nullptr，以确保在初始化时节点指针为nullptr

    InitializeLossPacketFile();
    Object::DoInitialize(); // 调用了基类Object的DoInitialize函数，以执行基类的初始化操作
}

// 该函数用于在新的聚合对象通知时执行必要的操作
void
SwitchMmu::NotifyNewAggregate()
{
    NS_LOG_FUNCTION(this);
    if (!m_node)
    {
        Ptr<Node> node = this->GetObject<Node>();
        // verify that it's a valid node and that
        // the node was not set before
        if (node)
        {
            this->SetNode(node); // 调用SetNode函数，将获取的节点指针设置为对象的节点
        }
    }
    Object::NotifyNewAggregate(); // 调用了基类Object的NotifyNewAggregate函数，以执行基类的相关操作
}

// 该函数用于设置交换机内存类型，并在需要时初始化片外缓冲区
void
SwitchMmu::SetMemType(BufferModel type)
{
    NS_LOG_FUNCTION(
        this
        << type); // 函数内部调用了NS_LOG_FUNCTION宏，记录了当前对象的地址和传入的交换机内存类型，用于跟踪函数的调用

    m_memType = type;
    /**
     * If the switch memory type is the 'Hybrid Buffer', the
     * Off Chip Buffer will be initialized.
     */
    // 如果交换机内存类型为ONOFFCHIP（表示混合缓冲区），并且片外缓冲区指针m_offChipBuffer为空，则会执行以下操作：
    if (m_memType == ONOFFCHIP && m_offChipBuffer == nullptr)
    {
        Ptr<OffChipBuffer> offChipBuffer = CreateObject<
            OffChipBuffer>(); // 调用CreateObject<OffChipBuffer>()创建一个OffChipBuffer对象
        AttachOffChipBuffer(
            offChipBuffer); // 调用AttachOffChipBuffer(offChipBuffer)函数，将创建的片外缓冲区对象附加到交换机对象上
    }
}

// 获取交换机当前的内存类型，并将其返回
SwitchMmu::BufferModel
SwitchMmu::GetMemType() const
{
    NS_LOG_FUNCTION(this);
    return m_memType;
}

// 这段代码是SwitchMmu类中的另一个成员函数SetNPorts的实现。该函数用于设置交换机的端口数量，并根据端口数量初始化和调整一些计数器和数据结构
void
SwitchMmu::SetNPorts(uint32_t nPorts)
{
    NS_LOG_FUNCTION(this << nPorts);

    m_nPorts = nPorts; // 交换机的端口数量

    // initialize and resize some counters
    m_activeQueNum.resize(m_nPorts, 0);
    m_activePortNum.resize(m_nPorts, 0);
    m_priDpUsed.resize(m_nPorts);
    m_qlens.resize(m_nPorts);
    m_alpha.resize(m_nPorts);

    m_cgStatus.resize(m_nPorts);
    m_qUsed.resize(m_nPorts);
    m_pUsed.resize(m_nPorts);
    m_qMaxUsed.resize(m_nPorts);
    m_qTotalRcvd.resize(m_nPorts);

    m_portRates.resize(m_nPorts, Gbps100);

    m_wcacheQlen.resize(m_nPorts);
    m_dramQlen.resize(m_nPorts);
    m_sramQlen.resize(m_nPorts);

    // LRU related
    m_cgTimer.resize(m_nPorts);
    Packet_Size_Cycle.resize(m_nPorts);
    Packet_Size_Cycle_Max.resize(m_nPorts);
    ReadSram_Size_Cycle.resize(m_nPorts);
    UsedSram_Size_Cycle.resize(m_nPorts);
    WriteDram_Size_Cycle.resize(m_nPorts);
    ReadSram_Rate_Cycle.resize(m_nPorts);
    WriteDram_Rate_Cycle.resize(m_nPorts);
    WriteDram_Rate_Cycle_last.resize(m_nPorts);
    simulation_start.resize(m_nPorts);
    m_Cost_ETC.resize(m_nPorts);
    EWMA_R.resize(m_nPorts);
    storeDecision.resize(m_nPorts);
    Timer_Mill.resize(m_nPorts);
    utility.resize(m_nPorts);
    Sr_last.resize(m_nPorts);
    qlen_last.resize(m_nPorts);
    Dr_last.resize(m_nPorts);
    Qis_last.resize(m_nPorts);
    T_seq.resize(m_nPorts);
    drop_real_per_period.resize(m_nPorts);
    perPktDecisionFlag.resize(m_nPorts);
    perPktDecisionCount.resize(m_nPorts);
}

uint32_t
SwitchMmu::GetNPorts() const
{
    NS_LOG_FUNCTION(this);
    return m_nPorts;
}

// 该函数用于设置每个端口的队列数量，并根据队列数量初始化和调整一些计数器和数据结构。
void
SwitchMmu::SetNQueues(uint32_t nQueues)
{
    NS_LOG_FUNCTION(this << nQueues);

    m_nQueuesPerPort =
        nQueues; // 函数将传入的队列数量nQueues赋值给成员变量m_nQueuesPerPort，表示每个端口的队列数量
    // initialize and resize counters.
    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        m_qlens[i].resize(m_nQueuesPerPort,
                          0); // 对每个端口的队列长度m_qlens和队列权重m_alpha 进行初始化或调整
        // The initial value of alpha is 1.
        m_alpha[i].resize(m_nQueuesPerPort, 1);

        m_cgTimer[i].resize(m_nQueuesPerPort, 0);

        for (uint32_t j = 0; j < m_nPriorities; j++)
        {
            m_qUsed[i][j].resize(m_nQueuesPerPort, 0);
            m_qMaxUsed[i][j].resize(m_nQueuesPerPort, 0);
            m_qTotalRcvd[i][j].resize(m_nQueuesPerPort, 0); // 队列总接收量
            m_wcacheQlen[i][j].resize(m_nQueuesPerPort, 0);
            m_dramQlen[i][j].resize(m_nQueuesPerPort, 0);
            m_sramQlen[i][j].resize(m_nQueuesPerPort, 0);
            m_cgStatus[i][j].resize(m_nQueuesPerPort, SwitchMmu::NOT_CONGESTION);
            Packet_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            Packet_Size_Cycle_Max[i][j].resize(m_nQueuesPerPort, 0);
            ReadSram_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            UsedSram_Size_Cycle[i][j].resize(m_nQueuesPerPort), 0;
            WriteDram_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            ReadSram_Rate_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            WriteDram_Rate_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            WriteDram_Rate_Cycle_last[i][j].resize(m_nQueuesPerPort, 0);
            simulation_start[i][j].resize(m_nQueuesPerPort);
            m_Cost_ETC[i][j].resize(m_nQueuesPerPort, NanoSeconds(min_T));
            EWMA_R[i][j].resize(m_nQueuesPerPort, 0);
            storeDecision[i][j].resize(m_nQueuesPerPort, 1);
            Timer_Mill[i][j].resize(m_nQueuesPerPort);
            utility[i][j].resize(m_nQueuesPerPort, 0);
            Sr_last[i][j].resize(m_nQueuesPerPort, 0);
            qlen_last[i][j].resize(m_nQueuesPerPort, 0);
            Dr_last[i][j].resize(m_nQueuesPerPort, 0);
            Qis_last[i][j].resize(m_nQueuesPerPort, 0);
            T_seq[i][j].resize(m_nQueuesPerPort, 0);
            drop_real_per_period[i][j].resize(m_nQueuesPerPort, 0);
            perPktDecisionFlag[i][j].resize(m_nQueuesPerPort, 0);
            perPktDecisionCount[i][j].resize(m_nQueuesPerPort, 0);
        }
    }
}

uint32_t
SwitchMmu::GetNQueues() const
{
    NS_LOG_FUNCTION(this);
    return m_nQueuesPerPort;
}

// 该函数用于设置每个端口的优先级数量，并根据优先级数量初始化和调整一些计数器和数据结构。
void
SwitchMmu::SetNPriorities(uint32_t nPriorities)
{
    NS_LOG_FUNCTION(this << nPriorities);

    m_nPriorities =
        nPriorities; // 函数将传入的优先级数量nPriorities赋值给成员变量m_nPriorities，表示每个端口的优先级数量
    // initialize and resize counters
    m_priOnChipUsed.resize(m_nPriorities, 0);
    m_priWcacheUsed.resize(m_nPriorities, 0);
    m_priDramUsed.resize(m_nPriorities, 0);
    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        m_priDpUsed[i].resize(m_nPriorities, 0);
        m_qUsed[i].resize(m_nPriorities);
        m_qMaxUsed[i].resize(m_nPriorities);
        m_qTotalRcvd[i].resize(m_nPriorities);
        m_wcacheQlen[i].resize(m_nPriorities);
        m_dramQlen[i].resize(m_nPriorities);
        m_sramQlen[i].resize(m_nPriorities);
        m_cgStatus[i].resize(m_nPriorities);
        Packet_Size_Cycle[i].resize(m_nPriorities);
        Packet_Size_Cycle_Max[i].resize(m_nPriorities);
        ReadSram_Size_Cycle[i].resize(m_nPriorities);
        UsedSram_Size_Cycle[i].resize(m_nPriorities);
        WriteDram_Size_Cycle[i].resize(m_nPriorities);
        ReadSram_Rate_Cycle[i].resize(m_nPriorities);
        WriteDram_Rate_Cycle[i].resize(m_nPriorities);
        WriteDram_Rate_Cycle_last[i].resize(m_nPriorities);
        simulation_start[i].resize(m_nPriorities);
        m_Cost_ETC[i].resize(m_nPriorities);
        EWMA_R[i].resize(m_nPriorities);
        storeDecision[i].resize(m_nPriorities);
        Timer_Mill[i].resize(m_nPriorities);
        utility[i].resize(m_nPriorities);
        Sr_last[i].resize(m_nPriorities);
        qlen_last[i].resize(m_nPriorities);
        Dr_last[i].resize(m_nPriorities);
        Qis_last[i].resize(m_nPriorities);
        T_seq[i].resize(m_nPriorities);
        drop_real_per_period[i].resize(m_nPriorities);
        perPktDecisionFlag[i].resize(m_nPriorities);
        perPktDecisionCount[i].resize(m_nPriorities);
    }
}

uint32_t
SwitchMmu::GetNPriorities() const
{
    NS_LOG_FUNCTION(this);
    return m_nPriorities;
}

// 函数将传入的缓存管理算法类型bmtype赋值给成员变量m_bmAlgorithm，表示当前使用的缓存管理算法类型
void
SwitchMmu::SetBmAlgorithm(BmAlgorithm bmtype)
{
    NS_LOG_FUNCTION(this << bmtype);
    m_bmAlgorithm = bmtype;

    if (m_bmAlgorithm == BASELINE) // 如果当前缓存管理算法类型为BASELINE
    {
        // SetOnChipBufferSize(10L <<
        // 30);//则调用SetOnChipBufferSize函数设置On-Chip缓存的大小为10GB（即左移30位，相当于10乘以2的30次方字节）
    }

    if (m_bmAlgorithm == DEEPHIR) // 如果当前缓存管理算法类型为BASELINE
    {
        now_algorithm_name = "BMS";
    }

    if (m_bmAlgorithm == TDT) // 如果当前缓存管理算法类型为BASELINE
    {
        now_algorithm_name = "pbs";
    }

    if (m_bmAlgorithm == YSL) // 如果当前缓存管理算法类型为YSL
    {
        UpdateLruTimer(); // 调用UpdateLruTimer函数更新LRU计时器
    }
}

SwitchMmu::BmAlgorithm
SwitchMmu::GetBmAlgorithm() const
{
    NS_LOG_FUNCTION(this);
    return m_bmAlgorithm;
}

// 该函数用于设置交换机的On-Chip缓存大小，并初始化On-Chip缓存的剩余空间
void
SwitchMmu::SetOnChipBufferSize(uint64_t size)
{
    NS_LOG_FUNCTION(
        this
        << size); // 函数内部调用了NS_LOG_FUNCTION宏，记录了当前对象的地址和传入的缓存大小size，用于跟踪函数的调用
    m_onChipBufferSize =
        size; // 函数将传入的缓存大小size赋值给成员变量m_onChipBufferSize，表示设置交换机的On-Chip缓存大小
    m_onChipBufferRemain =
        size; // 函数将传入的缓存大小size也赋值给成员变量m_onChipBufferRemain，表示初始化On-Chip缓存的剩余空间为设置的缓存大小
}

uint64_t
SwitchMmu::GetOnChipBufferSize() const
{
    NS_LOG_FUNCTION(this);
    return m_onChipBufferSize;
}

// 该函数用于设置交换机的重排序缓存大小，并初始化重排序缓存的剩余空间
void
SwitchMmu::SetReorderBufferSize(uint64_t size)
{
    NS_LOG_FUNCTION(
        this << size); // 函数内部调用了NS_LOG_FUNCTION宏，记录了当前对象的地址和传入的缓存大小size
    m_reorderBufferSize =
        size; // 函数将传入的缓存大小size赋值给成员变量m_reorderBufferSize，表示设置交换机的重排序缓存大小
    m_reorderBufferRemain =
        size; // 函数将传入的缓存大小size也赋值给成员变量m_reorderBufferRemain，表示初始化重排序缓存的剩余空间为设置的缓存大小
}

uint64_t
SwitchMmu::GetReorderBufferSize() const
{
    NS_LOG_FUNCTION(this);
    return m_reorderBufferSize;
}

uint64_t
SwitchMmu::GetReorderBufferRemain() const
{
    NS_LOG_FUNCTION(this);
    return m_reorderBufferRemain;
}

// 该函数用于更新重排序缓存的剩余空间，根据参数pktSize和inc的值来增加或减少缓存的剩余空间
void
SwitchMmu::UpdateReorderBufferRemain(uint32_t pktSize, bool inc)
{
    NS_LOG_FUNCTION(this);
    if (inc) // 函数根据参数inc的值来判断是增加还是减少重排序缓存的剩余空间。如果inc为true，表示增加缓存的剩余空间；如果inc为false，表示减少缓存的剩余空间
    { // 如果需要增加缓存的剩余空间，函数会检查增加后的剩余空间是否超过了缓存的总大小，如果超过则会输出错误信息
        NS_ASSERT_MSG(m_reorderBufferRemain + pktSize <= m_reorderBufferSize,
                      "Error when increase reorder buffer remain size");
        m_reorderBufferRemain += pktSize; // 然后将pktSize加到m_reorderBufferRemain中
    }
    else
    { // 如果需要减少缓存的剩余空间，函数会首先检查剩余空间是否大于等于0，如果小于0则会输出错误信息
        NS_ASSERT_MSG(m_reorderBufferRemain >= 0, "Error when decrease reorder buffer remain size");
        if (m_reorderBufferRemain <
            pktSize) // 然后根据pktSize的大小来更新剩余空间，如果剩余空间小于pktSize，则将剩余空间设为0
        {
            m_reorderBufferRemain = 0;
        }
        else
        {
            m_reorderBufferRemain -= pktSize; // 否则减去pktSize
        }
    }
}

uint64_t
SwitchMmu::GetOnChipBufferRemain() const
{
    NS_LOG_FUNCTION(this);
    return m_onChipBufferRemain;
}

void
SwitchMmu::
    Show() // 该函数用于显示交换机的基本信息，包括端口数量、每个端口的队列数量、优先级数量以及内存类型
{
    // NS_LOG_UNCOND("运行到Show()这里");

    NS_LOG_FUNCTION(this);

    // LOG OUT The fundamental info about the switch.
    NS_LOG_DEBUG("The Switch Port Num: " << m_nPorts);
    NS_LOG_DEBUG("The Switch Queue Num per port: " << m_nQueuesPerPort);
    NS_LOG_DEBUG("The Switch Priority Num: " << m_nPriorities);
    NS_LOG_DEBUG("The Switch Mem Type: ");
    if (m_memType ==
        ONOFFCHIP) // 如果内存类型为ONOFFCHIP，则还会调用m_offChipBuffer对象的Show函数展示更多关于离片缓存的信息
    {
        NS_LOG_DEBUG("ONOFFCHIP");
        m_offChipBuffer->Show();
    }
    else
    {
        NS_LOG_DEBUG("ONCHIP");
    }
}

void
SwitchMmu::
    ShowCounters() // 该函数用于显示交换机的计数器信息，包括各个优先级的使用情况、每个端口的活跃队列数量、队列的参数（Alpha和长度）以及片外缓存的计数器信息
{
    NS_LOG_FUNCTION(this);
    // DEBUG USAGE: Add to the place that you wanna to show counters values.
    NS_LOG_DEBUG("Remain size of onchip buffer: "
                 << m_onChipBufferRemain); // 输出"OnChip"缓存的剩余大小m_onChipBufferRemain

    NS_LOG_DEBUG("Priority Num:  " << m_nPriorities);
    for (uint32_t i = 0; i < m_nPriorities; i++)
    { ////遍历每个优先级，输出该优先级在"OnChip"缓存中的使用计数器m_priOnChipUsed[i]
        NS_LOG_DEBUG("Priority " << i << " Used onchip counters: " << m_priOnChipUsed[i]);
    }
    for (uint32_t i = 0; i < 4; i++)
    {
        NS_LOG_DEBUG("The Port Index: " << i);
        NS_LOG_DEBUG(
            "\tThe Active Queue Num of this Port: "
            << m_activeQueNum[i]); // 遍历每个端口，输出该端口的活跃队列数量m_activeQueNum[i]
        for (uint32_t j = 0; j < m_nQueuesPerPort; j++)
        {
            NS_LOG_DEBUG("\tThe Queue Index: " << j);
            NS_LOG_DEBUG(
                "\t\tThe Queue Alpha: "
                << m_alpha[i][j]); // 以及每个队列的Alpha参数m_alpha[i][j]和长度m_qlens[i][j]
            NS_LOG_DEBUG("\t\tThe Queue Length: " << m_qlens[i][j]);
        }

        for (
            uint32_t p = 0; p < m_nPriorities;
            p++) // 遍历每个端口和优先级，输出该端口和优先级对应的"OnChip"缓存使用计数器m_priDpUsed[i][p]
        {
            NS_LOG_DEBUG("\tThe Priority Num: " << p);
            NS_LOG_DEBUG(
                "\t\tThe Priority Used OnChip in this Destination Port: " << m_priDpUsed[i][p]);
        }
    }

    // OffChipBuffer
    m_offChipBuffer
        ->ShowCounters(); // 调用m_offChipBuffer对象的ShowCounters函数，展示片外缓存的计数器信息
}

// 只使用一个缓存：SRAM
SwitchMmu::BmResult
SwitchMmu::CheckBaselineBmAlgorithm(
    Ptr<Packet>
        packet) // 该函数用于检查基准缓存管理算法，根据输入的数据包信息和当前交换机状态，确定输入数据包的缓存位置
{
    NS_LOG_FUNCTION(this << packet);
    BmResult bmResult;
    // uint32_t port = packet->GetMmuUsedPort();
    // uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    // uint64_t qlen = m_qUsed[port][priority][qIndex];
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();

    // if (m_onChipBufferRemain >= pktSize)
    // //如果队列长度加上数据包大小不超过权重随机早期丢弃（WRED）阈值，则将数据包放入"OnChip"缓存
    // {
    //     bmResult = BmResult(TO_ONCHIPBUFFER);
    // }
    // else
    // {
    //     bmResult = BmResult(DROP);//否则，直接丢弃数据包
    // }

    if ((wcacheSize - wcacheUsed) >= pktSize && dramRemain >= pktSize)
    {
        bmResult = BmResult(TO_OFFCHIPBUFFER);
    }
    else
    {
        bmResult = BmResult(DROP); // 否则，直接丢弃数据包
    }
    return bmResult;
}

//  DeepHir算法
SwitchMmu::BmResult
SwitchMmu::CheckDeepHirBmAlgorithm(Ptr<Packet> packet) // 该函数用于检查基准缓存管理算法，根据输入的数据包信息和当前交换机状态，确定输入数据包的缓存位置
{
    NS_LOG_FUNCTION(this << packet);
    static uint64_t total_packet_num = 0;
    total_packet_num++;
    if (if_change_threshold)
    {
        m_wredTh = {static_cast<uint64_t>(Deeohir_threshold * 1024 * 1024),
                    static_cast<uint64_t>(Deeohir_threshold * 1024 * 1024)};
        std::cout << "当前Deephir静态阈值:"
                  << static_cast<uint64_t>(Deeohir_threshold * 1024 * 1024) << endl;
        if_change_threshold = 0;
    }

    BmResult bmResult;
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    uint64_t qlen = m_qUsed[port][priority][qIndex];
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();
    uint64_t DT_Threshold = DT_alpha * m_onChipBufferRemain;
    // 当前队列在SRAM中的占用
    const uint64_t Qis =UsedSram_Size_Cycle[port][priority][qIndex];
    // 防止wcacheSize - wcacheUsed发生无符号整数下溢
    const bool hasWcacheSpace =wcacheUsed <= wcacheSize && (wcacheSize - wcacheUsed) >= pktSize;
    const bool hasDramSpace =dramRemain >= pktSize;
    NS_ASSERT_MSG(priority <= 1, "优先级只有2个");
    // 默认决策为丢包
    bmResult = BmResult(DROP);
    // 满足静态阈值、SRAM剩余空间和DT动态阈值时，存入片内SRAM
    if ((qlen + pktSize) <= m_wredTh[priority] &&m_onChipBufferRemain >= pktSize &&(Qis + pktSize) <= DT_Threshold)
    {
        bmResult = BmResult(TO_ONCHIPBUFFER);
        // cout << "Time:" << Simulator::Now() << " packet:" << packet->GetUid()<< " 端口:" << port << " 存入片内" << endl;
    }
    else
    {
        // SRAM不满足条件时，检查是否能够存入片外DRAM
        if ((wcacheSize - wcacheUsed) >= pktSize && dramRemain >= pktSize){
            bmResult = BmResult(TO_OFFCHIPBUFFER);
            // cout << "Time:" << Simulator::Now()     << " packet:" << packet->GetUid() << " 端口:" << port << " 存入片外"  << endl;
        }
    }
    if (bmResult == BmResult(DROP))
    {
        if (wcacheSize - wcacheUsed < pktSize)
        {
            cout << "Time:" << Simulator::Now()<< " packet:" << packet->GetUid()<< " 端口:" << port << " 丢包原因:Dram带宽不足(wcache不够)" << " wcacheUsed/wcacheSize:"<< wcacheUsed << "/" << wcacheSize<< endl;
        }
        else if (!hasDramSpace)
        {
            cout << "Time:" << Simulator::Now() << " packet:" << packet->GetUid() << " 端口:" << port << " 丢包原因:DRAM剩余空间不足"<< " dramRemain/pktSize:"<< dramRemain << "/" << pktSize<< endl;
        }
    }
    if (print_flag == 1)
    {
        // 当前队列总占用
        const uint64_t qiBytes = qlen;
        const uint64_t qiSBytes = (Qis <= qiBytes) ? Qis : qiBytes;
        // 当前队列在DRAM中的占用
        const uint64_t qiDBytes = (qiBytes >= qiSBytes) ? (qiBytes - qiSBytes) : 0;
        // 判断当前队列是否同时存储在SRAM和DRAM
        const bool isMixed = (qiSBytes > 0 && qiDBytes > 0);
        const double arrivalRateActual = 0.0;
        const double ewmaRate = 0.0;
        const uint64_t dropReal = (bmResult == BmResult(DROP)) ? 1 : 0;
        const uint64_t totalArrival = 1;
        uint32_t storeDecision = 0;

        if (bmResult == BmResult(TO_ONCHIPBUFFER))
        {
            storeDecision = 1;
        }
        else if (bmResult == BmResult(TO_OFFCHIPBUFFER))
        {
            storeDecision = 0;
        }

        cout << endl;
        cout << "--------------------------------------------------------------------------"<< endl;
        cout << "DebugDeepHir: "
             << " time: " << Simulator::Now().GetNanoSeconds()<< " port: " << port
             << " periodSeq: " << 0<< " T: " << 0
             << " newT: " << 0<< " Decision(0片外,1片内): " << storeDecision << " bmResult(2丢包): " << bmResult
             << " Usram: " << 0<< " Udram: " << 0 << endl;
        cout << " (1) BufferStates: "
             << " Qi: "<< static_cast<double>(qiBytes) / 1e6 << " QiS: "<< static_cast<double>(qiSBytes) / 1e6
             << " QiD: "<< static_cast<double>(qiDBytes) / 1e6<< " mixed: " << isMixed
             << " Sr: "<< static_cast<double>(m_onChipBufferRemain) / 1e6 << " Dr: "<< static_cast<double>(dramRemain) / 1e6
             << " DT: "<< static_cast<double>(DT_Threshold) / 1e6 << " wCacheUsed/Size: "  << static_cast<double>(wcacheUsed) / 1e6
             << "/" << static_cast<double>(wcacheSize) / 1e6<< endl;
        cout << " (2) RateStates: " << " arrivalRateActual: " << arrivalRateActual<< " ewmaRate: " << ewmaRate << " inBytes: " << 0
             << " outBytesFromSram/outBytesMax: "<< 0 << "/" << 0<< " deltaQiS: " << 0<< " DTnext: " << 0<< endl;
        cout << " (3) Utility_Calculation: " << " U_1s: " << 0<< " U_2s: " << 0
             << " U_1d: " << 0<< " U_2d: " << 0<< endl;
        cout << " (4) T_Calculation: " << " deltaU: " << 0<< " MD: " << 0
             << " U1Ss: " << 0<< " U1Ds: " << 0<< " U2Ss: " << 0<< " U2Ds: " << 0
             << " U_Sstar: " << 0 << " U_Dstar: " << 0  << " drop_real/total_arrival: "<< dropReal << "/" << totalArrival<< endl;
        cout << " (5) DecisionStates: " << " perPktDecisionFlag: " << 0<< " perPktDecisionCount: " << 0<< endl;
        cout << "--------------------------------------------------------------------------" << endl << endl;
    }
    return bmResult;
}

void SwitchMmu::CountDramBandwidth(){
    WriteDram_Rate_Total = WriteDram_Size_Total / Dram_Bandwidth_Timer * 8; // B / ns *8= Gbps
    ReadDram_Rate_Total = ReadDram_Size_Total / Dram_Bandwidth_Timer * 8;
    WriteDram_Size_Total = 0;
    ReadDram_Size_Total = 0;
    Dr_EWMA = Dr_EWMA * 0.9 + std::max(0.0, (1024.0- WriteDram_Rate_Total - ReadDram_Rate_Total)) * 0.1; //m_offChipBuffer->GetDramBandwidth() 空指针
    // cout<< "Time:" << Simulator::Now() << " 片外总写速率: " << WriteDram_Rate_Total << " Gbps, 片外总读速率: " << ReadDram_Rate_Total << " Gbps, 片外剩余带宽: " << Dr_EWMA << " Gbps" <<"  WriteDram_Size_Total "<<WriteDram_Size_Total
    // <<" wcacheUsed "<<m_offChipBuffer->GetWcacheUsed() << " wcacheSize "<<m_offChipBuffer->GetWcacheSize() << endl;
    Simulator::Schedule(NanoSeconds(Dram_Bandwidth_Timer), &SwitchMmu::CountDramBandwidth, this);
}

void SwitchMmu::CountSramThroughputDiff(){
    WriteDram_Rate_Total = WriteSram_Size_Total / Sram_ThroughputDiff_Timer * 8; // B / ns *8= Gbps
    ReadDram_Rate_Total = ReadSram_Size_Total / Sram_ThroughputDiff_Timer * 8;
    DiffSram_Rate_Total = WriteDram_Rate_Total - ReadDram_Rate_Total;
    WriteSram_Size_Total = 0;
    ReadSram_Size_Total = 0;
    DiffSram_Rate_EWMA = DiffSram_Rate_EWMA * 0.9 + DiffSram_Rate_Total * 0.1;
    Simulator::Schedule(NanoSeconds(Sram_ThroughputDiff_Timer), &SwitchMmu::CountSramThroughputDiff, this);
}

SwitchMmu::BmResult SwitchMmu::Check3DTBmAlgorithm(Ptr<Packet> packet) { // FBMM
    NS_LOG_FUNCTION(this << packet);
    /*
        单位：   缓存：Bytes (uint64_t)     速率：Gbps (double)        时间：ns (double) 
        速率计算： Bytes / ns = 8 / 1e-9 bps = 8 Gbps
        缓存计算： Gbps * ns = 1e9 * 1e-9 b = 1/8 Bytes
        时间计算： Bytes / Gbps = 8 / 1e9 s = 8 ns
    */
    //************* 交换机配置 ***************
    uint32_t port = packet->GetMmuUsedPort();         // 端口号
    uint32_t qIndex = packet->GetMmuUsedQueueId();    // 队列号
    uint32_t priority = packet->GetMmuUsedPriority(); // 优先级
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION; // 包长

    uint64_t S = m_onChipBufferSize;                              // SRAM总缓存
    double D = m_offChipBuffer->GetDramBandwidth();             // DRAM总带宽
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();     // wcache总容量
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();     // wcache使用量

    //************* 缓存状态 ***************
    uint64_t qlen = m_qUsed[port][priority][qIndex];   // 当前队列总占用，包括 SRAM 和 DRAM
    uint64_t QiS = UsedSram_Size_Cycle[port][priority][qIndex];  // 当前队列位于SRAM中的数据量
    uint64_t QiD = qlen - QiS;  // 当前队列位于DRAM中的数据量
    bool isMixed = QiS > 0 && QiD > 0;

    uint64_t Sr = m_onChipBufferRemain;    // SRAM剩余容量
    double dtThreshold = DT_alpha * Sr;  //DT_Threshold = DT_alpha × SRAM剩余容量

    double Dr = Dr_EWMA;    // 当前DRAM剩余带宽
    bool hasWcacheSpace = (wcacheSize - wcacheUsed) >= pktSize;  //是否还有写wcache的空间
    bool hasDramSpace = m_offChipBuffer->GetDramRemain() >= pktSize;   //是否还有Dram的空间

    //************* 流量状态 ***************
    Packet_Size_Cycle[port][priority][qIndex] += pktSize; //  统计当前周期总到达数据量
    double lambdaLast = Packet_Size_Cycle[port][priority][qIndex] * 8.0 / (Simulator::Now() - simulation_start[port][priority][qIndex]).GetNanoSeconds();  // 上周期的流量到达速率
    double lambdaEwma; // 当前周期的流量到达率的指数加权移动平均值
    uint64_t inBytes; // 估计的下周期到达数据量 lambda(t+1) * T(t+1)
    uint64_t outBytesFromSram, outBytesMax; // 估计的下周期排出数据量 (估计), 下周期最大排出数据量 mu * T(t+1)
    uint64_t deltaQiS; // 估计的下周期SRAM队列长度变化量 arrivalBytes - serviceBytesFromSRAM
    double DTnext; // 估计的下周期末动态阈值 DT(t+1) 

    //************* 决策相关 ***************
    BmResult bmResult = BmResult(DROP);  //当前存储决策
    bool newStoreDecision;  //新做的存储决策，1为SRAM，0为DRAM
    double cycleTime =(Simulator::Now() - simulation_start[port][priority][qIndex]) .GetNanoSeconds();  // 当前周期已经经过的实际时间
    double U1Ss = 0.0, U2Ss = 0.0, U1Ds = 0.0, U2Ds = 0.0, U_Sstar, U_Dstar, U_star, deltaU; // 上一周期的实际效用
    double MD, newT = AI;
    double U_sram_1, U_sram_2, U_sram, U_dram_1, U_dram_2, U_dram; // 当前周期的效用

    auto printPacketAdmissionResult = [&](bool intendedStoreSram){
        if (print_flag != 1) return;
        // if (bmResult == BmResult(TO_ONCHIPBUFFER)){
        //     cout << "Time:" << Simulator::Now() << "  packet:" << packet->GetUid() << " pktSize: " << packet->GetSize()<< " 端口:" << port<< " 存入片内" << endl;
        // }
        // else if (bmResult == BmResult(TO_OFFCHIPBUFFER)){
        //     cout << "Time:" << Simulator::Now() << "  packet:" << packet->GetUid() << " pktSize: " << packet->GetSize() << " 端口:" << port << " 存入片外" << endl;
        // }
        if (bmResult == BmResult(DROP)){
            if  (intendedStoreSram){
                cout << "Time:" << Simulator::Now() << " SRAM丢包 packet:" << packet->GetUid()<< " pktSize: " << packet->GetSize() << " 端口:" << port
                    << " DT阈值丢包:  Qis:" << QiS << "| DT_Threshold:" << dtThreshold <<" | Sr:"<<Sr << endl;
            }
            else{
                cout << "Time:" << Simulator::Now() << " DRAM丢包 packet:" << packet->GetUid()<< " pktSize: " << packet->GetSize() << " 端口:" << port
                     << " wcacheUsed/wcacheSize:" << wcacheUsed << "/" << wcacheSize<< " DRAM_bandwidth:" << Dr_EWMA << endl;
            }
        }
    };

    ///**************** 第1个周期开始时(包括间隔太久重置周期为1) ******************/
    if ((cycleTime > 2*RTT && qlen == 0) || T_seq[port][priority][qIndex] <= 1){
        cout<< "Time:" << Simulator::Now() << " 端口:" << port << " 队列:" << qIndex << " 优先级:" << priority << " 周期重置为1" << endl;
        if (QiS + pktSize <= dtThreshold && pktSize <= m_onChipBufferRemain){
            bmResult = BmResult(TO_ONCHIPBUFFER);
            storeDecision[port][priority][qIndex] = 1;  // =1为SRAM
        }
        else if (hasWcacheSpace && hasDramSpace) {
            bmResult = BmResult(TO_OFFCHIPBUFFER);
            storeDecision[port][priority][qIndex] = 0;  // =0为DRAM
        }
        printPacketAdmissionResult(storeDecision[port][priority][qIndex]);
    }
    ///**************** 第N个(N>1)周期内 ******************/
    else if (T_seq[port][priority][qIndex] > 1 && cycleTime < m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() && perPktDecisionFlag[port][priority][qIndex] < 3) { // FBM周期决策。当前周期尚未结束：严格保持本周期决策
        if (storeDecision[port][priority][qIndex]){ // =1为SRAM
            if (QiS + pktSize <= dtThreshold && pktSize <= m_onChipBufferRemain) //严格按照存储决策执行，SRAM满了就丢包
                bmResult = BmResult(TO_ONCHIPBUFFER);
        }else{ // =0为DRAM
            if (hasWcacheSpace && hasDramSpace)
                bmResult = BmResult(TO_OFFCHIPBUFFER);
        }
        printPacketAdmissionResult(storeDecision[port][priority][qIndex]);
    }
    else if (T_seq[port][priority][qIndex] > 1 && cycleTime < m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() && perPktDecisionFlag[port][priority][qIndex] == 3) { // 每包决策周期之内
        if (isMixed){ // 队列混合存储，SRAM和DRAM都有数据
            if (storeDecision[port][priority][qIndex]){ 
                if (QiS + pktSize <= dtThreshold && pktSize <= m_onChipBufferRemain && qlen + pktSize <= dtThreshold)
                    bmResult = BmResult(TO_ONCHIPBUFFER);
            }else{
                if (hasWcacheSpace && hasDramSpace)
                    bmResult = BmResult(TO_OFFCHIPBUFFER);
            }
        }
        else{ // 队列单一存储，SRAM或DRAM
            if (QiS + pktSize <= dtThreshold && pktSize <= m_onChipBufferRemain && qlen + pktSize <= dtThreshold){
                bmResult = BmResult(TO_ONCHIPBUFFER);
                if (storeDecision[port][priority][qIndex] == 0 &&  LINK_BW * m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() / 8 <= qlen){
                    if (hasWcacheSpace && hasDramSpace)
                        bmResult = BmResult(TO_OFFCHIPBUFFER); //如果前面是DRAM，且当前队列无法被及时排出，那么久不能切到SRAM
                    storeDecision[port][priority][qIndex] = 0;   
                }else{
                    storeDecision[port][priority][qIndex] = 1;
                }       
            }
            else if (hasWcacheSpace && hasDramSpace){
                bmResult = BmResult(TO_OFFCHIPBUFFER);
                storeDecision[port][priority][qIndex] = 0;
            }
        } 
        printPacketAdmissionResult(storeDecision[port][priority][qIndex]);
    }
    ///**************** 第N个(N>1)周期末 ******************/
    else if (T_seq[port][priority][qIndex] > 1 && cycleTime >= m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() && perPktDecisionFlag[port][priority][qIndex] == 3){ //每包决策周期末
        if (isMixed){ // 队列混合存储，SRAM和DRAM都有数据
            if (storeDecision[port][priority][qIndex]) {
                if (QiS + pktSize <= dtThreshold && pktSize <= m_onChipBufferRemain && qlen + pktSize <= dtThreshold)
                    bmResult = BmResult(TO_ONCHIPBUFFER);
            }else{
                if (hasWcacheSpace && hasDramSpace)
                    bmResult = BmResult(TO_OFFCHIPBUFFER);
            }
        }else{
            if (QiS + pktSize <= dtThreshold && pktSize <= m_onChipBufferRemain && qlen + pktSize <= dtThreshold){
                bmResult = BmResult(TO_ONCHIPBUFFER);
                if (storeDecision[port][priority][qIndex] == 0 &&  LINK_BW * m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() / 8 <= qlen){
                    if (hasWcacheSpace && hasDramSpace)
                        bmResult = BmResult(TO_OFFCHIPBUFFER); //如果前面是DRAM，且当前队列无法被及时排出，那么久不能切到SRAM
                    storeDecision[port][priority][qIndex] = 0;   
                }else{
                    storeDecision[port][priority][qIndex] = 1;
                }       
            }
            else if (hasWcacheSpace && hasDramSpace){
                bmResult = BmResult(TO_OFFCHIPBUFFER);
                storeDecision[port][priority][qIndex] = 0;
            }
        }
        printPacketAdmissionResult(storeDecision[port][priority][qIndex]);
        if (perPktDecisionFlag[port][priority][qIndex])
            perPktDecisionFlag[port][priority][qIndex] -= 1; 
    }
    else if (T_seq[port][priority][qIndex] > 1 && cycleTime >= m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() && perPktDecisionFlag[port][priority][qIndex] < 3){ // FBM周期决策末
        //******* 收集周期末的状态 ******* */
        WriteDram_Rate_Cycle_last[port][priority][qIndex] = WriteDram_Rate_Cycle[port][priority][qIndex];;
        WriteDram_Rate_Cycle[port][priority][qIndex] = WriteDram_Size_Cycle[port][priority][qIndex] * 8.0 / cycleTime;

        EWMA_R[port][priority][qIndex] =  EWMA_W * EWMA_R[port][priority][qIndex] + (1 - EWMA_W) * lambdaLast; //  平滑到达速率
        lambdaEwma = EWMA_R[port][priority][qIndex];

        if (print_flag == 1){
            cout << Simulator::Now().GetNanoSeconds()  << " states_in_end_of_period "  << T_seq[port][priority][qIndex]<< " port: " << port
                 << " lambda: " << lambdaEwma << "  miu(Gbps): " << Cqs   
                 << "  Pqs(MB): " << QiS * 1e-6
                 << "  Sr(MB): " << Sr * 1e-6 << "  Dr(Gbps): " << Dr  << "  S(MB): " << S * 1e-6  << "  D(Gbps): " << D
                 << " cycle_time(ns): " << cycleTime << " math_ETC(ns): " << m_Cost_ETC[port][priority][qIndex].GetNanoSeconds()
                 << " ReadSram_Size_Cycle[port][priority][qIndex]: " << ReadSram_Size_Cycle[port][priority][qIndex]
                 << " Packet_Size_Cycle[port][priority][qIndex]: " << Packet_Size_Cycle[port][priority][qIndex]
                 << "  WriteDram_Size_Cycle[port][priority][qIndex] "<< WriteDram_Size_Cycle[port][priority][qIndex]
                 << " qlen(MB): " << qlen * 1e-6
                 << " activePort: " << m_activeQueNumSwitch
                 << endl;
        }
        //******* 计算上周期的实际效用 ******* */
        U1Ss = 1.0 - drop_real_per_period[port][priority][qIndex] * 1.0 / Packet_Size_Cycle[port][priority][qIndex];  //丢包率=丢包数/总到达包数
        U1Ds = 1.0 - drop_real_per_period[port][priority][qIndex] * 1.0 / Packet_Size_Cycle[port][priority][qIndex];
        U2Ss = (Sr_last[port][priority][qIndex] - (Packet_Size_Cycle[port][priority][qIndex] - ReadSram_Size_Cycle[port][priority][qIndex])) * 1.0 / S
                - 1.0 * qlen_last[port][priority][qIndex] / S;
        // - std::min((Packet_Size_Cycle[port][priority][qIndex] - ReadSram_Size_Cycle[port][priority][qIndex]) * 1.0 
                            // / Sr_last[port][priority][qIndex] + 1.0 * qlen_last[port][priority][qIndex] / S, 10.0);
        U2Ds = (Dr_last[port][priority][qIndex] +  WriteDram_Rate_Cycle_last[port][priority][qIndex] - lambdaLast) / D;
        // -std::min(lambdaLast / (Dr_last[port][priority][qIndex] +  WriteDram_Rate_Cycle_last[port][priority][qIndex]), 10.0);
        U_Sstar = U1Ss + UTILITY_ETA * U2Ss;
        U_Dstar = U1Ds + UTILITY_ETA * U2Ds;
        U_star = storeDecision[port][priority][qIndex] ? U_Sstar : U_Dstar;
        deltaU = T_seq[port][priority][qIndex] > 2 ? std::fabs(utility[port][priority][qIndex] - U_star) : 0.0;

        //******* 计算下一周期的周期长度 T（前瞻范围） ******* */
        MD = UTILITY_ETA / (UTILITY_ETA + MD_EPSILON * deltaU);  //MD的更新
        newT =  std::min((m_Cost_ETC[port][priority][qIndex].GetNanoSeconds() + AI) * MD, RTT);  //T(t+1)=min[(T(t)+AI)×MD,Tmax​]
        if (perPktDecisionFlag[port][priority][qIndex] == 2) // 每包决策后的第一个周期决策，T用默认值即可。
            newT = AI;

        if (print_flag == 1){
            cout << Simulator::Now().GetNanoSeconds()
                 << " New_Period T(ns):" << newT
                 << " S/4/ewma_r(ns): null" << 0
                 << "  U_star[T]: " << U_star
                 << "  U[T]: " << utility[port][priority][qIndex]
                 << " T(t): " << m_Cost_ETC[port][priority][qIndex].GetNanoSeconds()
                 << " AI: " << AI << " MD: " << MD
                 << " Tmax: " << RTT
                 << " |U-U*|: " << deltaU
                 << " eta_MD: " << eta_MD
                 << " gamma: " << gamma
                 << endl;
        }

        //******* 计算下一周期的效用值 ******* */
        inBytes = lambdaEwma * newT / 8.0;  // 下周期到达数据量
        outBytesMax = LINK_BW * newT / 8.0;  // 下周期最大排出数据量
        if (storeDecision[port][priority][qIndex]){ // =1为SRAM  估计下周期从SRAM排出的数据量
            outBytesFromSram = outBytesMax < QiS ?  // 一个周期连原有 SRAM 数据都发不完
                outBytesMax :
                QiS + std::min(lambdaEwma, LINK_BW) * (newT - QiS * 8.0 / LINK_BW) / 8.0;  // 下周期可从 SRAM 发出的新到达数据
        }else{ // =0为DRAM
            outBytesFromSram = outBytesMax < QiD ?
                0.0 :  // 一个周期连原有 DRAM 数据都发不完，SRAM 没有机会输出。
                std::min(lambdaEwma, LINK_BW) * (newT - QiD * 8.0 / LINK_BW) / 8.0;  // 下周期可从 SRAM 发出的新到达数据
        }
        deltaQiS = inBytes - outBytesFromSram;  // 估计的下周期流量对SRAM需求
        DTnext = DT_alpha * (Sr - DiffSram_Rate_EWMA * newT / 8.0); // 计算DT，考虑全局
        U_sram_1 =  1 - std::min(1.0, std::max(0.0, (QiS + deltaQiS - DTnext)*1.0/inBytes ));
        U_dram_1 =  1 - std::max(0.0, (lambdaEwma - Dr - WriteDram_Rate_Cycle[port][priority][qIndex])/lambdaEwma );
        U_sram_2 = (Sr - deltaQiS) * 1.0 / S - 1.0 * qlen / S;
        U_dram_2 = (Dr + WriteDram_Rate_Cycle[port][priority][qIndex] - lambdaEwma) / D;
        // U_sram_2 = - std::min(10.0, deltaQiS * 1.0 / Sr + 1.0 * qlen / S);
        // U_dram_2 = - std::min(10.0, lambdaEwma / (Dr + WriteDram_Rate_Cycle[port][priority][qIndex]));
        U_sram = U_sram_1 + UTILITY_ETA * U_sram_2;
        U_dram = U_dram_1 + UTILITY_ETA * U_dram_2;

        //******* 基于效用值做决策 ******* */
        if (isMixed) {  // 队列已经混合时，不允许改变当前方向。
            newStoreDecision = storeDecision[port][priority][qIndex];
        } else {
            newStoreDecision = U_sram >= U_dram;  // 选择效用更高的方向
            if (storeDecision[port][priority][qIndex] == 0 && outBytesMax < QiD ){ // 上一个周期存DRAM, 并且下一个周期连原有 DRAM 数据都发不完，SRAM 没有机会输出，继续存入 DRAM
                newStoreDecision = 0;   
            }  
        }
        utility[port][priority][qIndex] =  newStoreDecision ? U_sram : U_dram;
        storeDecision[port][priority][qIndex] = newStoreDecision;

        //  按照最终方向准入当前数据包  不允许方向为 SRAM 时回退到 DRAM //bmResult 0:DRAM，1:SRAM，2:DROP
        if (newStoreDecision){ // =1为SRAM
            if (pktSize <= m_onChipBufferRemain && QiS + pktSize <= dtThreshold)
                bmResult = BmResult(TO_ONCHIPBUFFER);
        }
        else{
            if (hasWcacheSpace && hasDramSpace)
                bmResult = BmResult(TO_OFFCHIPBUFFER);
        }
        printPacketAdmissionResult(storeDecision[port][priority][qIndex]);

        if (print_flag == 1){
            cout << Simulator::Now().GetNanoSeconds()<< " middle_value_for_plot: " << " port: " << port << " CurrentCycle(T'th): " << T_seq[port][priority][qIndex]
             << " newT[T+1](ns): " << newT<< " newU[T+1] Usram: " << U_sram  << " Udram: " << U_dram << " lambda: " << lambdaEwma  << "  miu(Gbps): " << Cqs  << "  Sr(MB): " << Sr * 1e-6
             << "  Dr(Gbps): " << Dr << "  U_star[T]: " << U_star<< "  U[T]: " << utility[port][priority][qIndex]
             << " Storing_decision(0片外-1片内-2丢包): " << storeDecision[port][priority][qIndex]<< " final_dicision " << bmResult << " delta_Q: " << deltaQiS
             << " U_s1: " << U_sram_1 << " U_s2: " << U_sram_2 << " U_d1: " << U_dram_1 << " U_d2: " << U_dram_2<< " MD: " << MD << endl;
            cout << "最终决策结果:0片外 1片内 2丢包：   "<< bmResult << endl;
        }      
        
        switch (perPktDecisionFlag[port][priority][qIndex]) {
            case 2: // 每包决策后的第一个周期决策
                perPktDecisionFlag[port][priority][qIndex] -= 1;
                break;
            case 1: // 每包决策后的第二个周期决策
                if (newT - AI < 1e-6) { // 连续进入每包决策
                    perPktDecisionCount[port][priority][qIndex] += 1;
                    perPktDecisionFlag[port][priority][qIndex] = 3;
                    // newT = AI * std::pow(2, perPktDecisionCount[port][priority][qIndex]); // 每包决策周期长度为AI*2^n (2*AI, 4*AI, 8*AI, ...)
                    newT = AI * std::pow(2, 0); // 每包决策周期长度为AI*2^n (2*AI, 4*AI, 8*AI, ...)
                }else{
                    perPktDecisionFlag[port][priority][qIndex] -= 1;
                }
                break;
            case 0: // 每包决策后的第N(N>3)个周期决策，或未经历过每包决策
                if (newT - AI < 1e-6) { // 初次进入每包决策
                    perPktDecisionCount[port][priority][qIndex] = 0;
                    perPktDecisionFlag[port][priority][qIndex] = 3;
                    // newT = AI * std::pow(2, perPktDecisionCount[port][priority][qIndex]); // 每包决策周期长度为AI*2^0 = AI
                    newT = AI * std::pow(2, 0); // 每包决策周期长度为AI*2^0 = AI
                }
                break;
            default:
                NS_ASSERT_MSG(false, "perPktDecisionFlag should be in {0,1,2}");
        }
    }
    //****************** 第一个周期或第N(N>1)个周期末要更新的状态 ******************/
    if ((cycleTime > 2*RTT && qlen == 0) || T_seq[port][priority][qIndex] <= 1 ||
        (T_seq[port][priority][qIndex] > 1 && cycleTime >= m_Cost_ETC[port][priority][qIndex].GetNanoSeconds())){ 
        if (print_flag == 1){
            cout << endl<< "--------------------------------------------------------------------------------"<<endl;
            cout << "DebugFBM: " << " time:" << Simulator::Now().GetNanoSeconds() 
                << " port:" << port << " periodSeq: " << T_seq[port][priority][qIndex] 
                << "  newT:" << newT
                << "  Decision(0片外,1片内):" << storeDecision[port][priority][qIndex] << "  bmResult(2丢包):" << bmResult
                << "  Usram:" << U_sram << "  Udram:" << U_dram
                << endl;
            cout << "  (1) BufferStates: " 
                    << " Qi:" << qlen/1e6 << "  QiS:" << QiS/1e6 << "  QiD:" << QiD/1e6 << "  mixed:" << isMixed
                    << "  Sr:" << Sr/1e6 << "  Dr:" << Dr << "  DT:"<<dtThreshold/1e6 <<"  wcacheUsed/Size:"<<wcacheUsed/1e6<<"/"<<wcacheSize/1e6
                    << endl;
            cout << "  (2) RateStates: " 
                << " arrivalRateActual:" << lambdaLast<< "  ewmaRate:" << lambdaEwma
                << "  inBytes:"<< inBytes/1e6 << "  outBytesFromSram/outBytesMax:" << outBytesFromSram/1e6 << "/" << outBytesMax/1e6
                << "  deltaQiS:" << deltaQiS/1e6 << "  DTnext:" << DTnext/1e6
                << endl;
            cout << "  (3) Utility_Calculation: " 
                << " U_1s:" << U_sram_1 << "  U_2s:" << U_sram_2 << "  U_1d:" << U_dram_1 << "  U_2d:" << U_dram_2
                << endl;
            cout << "  (4) T_Calculation: "  << " deltaU:" << deltaU << "  MD:" << MD 
                << "  U1Ss:" << U1Ss << "  U2Ss:" << U2Ss << "  U1Ds:" << U1Ds << "  U2Ds:" << U2Ds << "  U_Sstar:" << U_Sstar << "  U_Dstar:" << U_Dstar
                <<" drop_real/total_arrival:" << drop_real_per_period[port][priority][qIndex] << "/" << Packet_Size_Cycle[port][priority][qIndex]
                << endl;
            cout << "  (5) DecisionStates: " 
                << "  perPktDecisionFlag:" << perPktDecisionFlag[port][priority][qIndex] << "  perPktDecisionCount:" << perPktDecisionCount[port][priority][qIndex]
                << endl;   
            cout << "--------------------------------------------------------------------------------"<<endl<<endl;    
        }

        if ((cycleTime > 2*RTT && qlen == 0) || T_seq[port][priority][qIndex] <= 1)
            T_seq[port][priority][qIndex] = 2; /*初始化周期序号*/
        else 
            T_seq[port][priority][qIndex] += 1; 

        simulation_start[port][priority][qIndex] = Simulator::Now(); // 周期结束时间
        m_Cost_ETC[port][priority][qIndex] = NanoSeconds(newT);
    
        Sr_last[port][priority][qIndex] = Sr; //周期结束时的Sr
        qlen_last[port][priority][qIndex] = qlen; //周期结束时的Sr
        Dr_last[port][priority][qIndex] = Dr; //周期结束时的Dr
        Qis_last[port][priority][qIndex] = QiS; //周期结束时的QiS，i.e., SRAM队列长度
    
        ReadSram_Size_Cycle[port][priority][qIndex] = 0; // 下一周期的SRAM出数据量，用于计算实际效用 U_2S*
        WriteDram_Size_Cycle[port][priority][qIndex] = 0; // 用于计算下一周期的DRAM写入速度
        Packet_Size_Cycle[port][priority][qIndex] = 0; // 用于计算下一周期的到达速率
        
        drop_real_per_period[port][priority][qIndex] = 0; //每周期的实际丢包数
    }
    return bmResult;
}


uint64_t
SwitchMmu::GetDynamicAlphaSramThreshold(
    uint32_t
        qIndex) // 获取动态的SRAM阈值。函数的作用是为每个队列分配一个静态的10KB的SRAM阈值，并返回这个值
{
    // With this simple. we just allocate static 10KB for each Queue.
    // 10KB
    return 10UL << 10; // 返回一个uint64_t类型的值，表示10KB的SRAM阈值
}

SwitchMmu::BmResult
SwitchMmu::CheckYSLBmAlgorithm(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);
    BmResult bmResult;
    bmResult = BmResult(TO_ONCHIPBUFFER);
    return bmResult;
}

SwitchMmu::BmResult
SwitchMmu::CheckHWBmAlgorithm(
    Ptr<Packet>
        packet) // 该函数用于检查HW缓存管理算法，根据输入的数据包信息和当前交换机状态，确定输入数据包的缓存位置
{
    NS_LOG_FUNCTION(this << packet);
    // NS_LOG_UNCOND("运行到SwitchMmu中检查算法这里");

    // This function is about to set the algorithm which help to
    // determine the Buffer Location place of Input Packet.
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    uint64_t qlen = m_qUsed[port][priority][qIndex];
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();
    QueueStatus cgStatus = GetQueueStatus(port, qIndex, priority);
    // cout<<"cgStatus:"<<cgStatus<<endl;
    Flow type = down; // TODO: decide the flow
                      // type,这里的down指的是数据从服务器发送到客户端的方向，为下行数据流
    BmResult bmResult;

    PortType portType = m_portRates[port];

    NS_LOG_DEBUG("Port: " << port << "\tQueue: " << qIndex << "\tPriority: " << priority);
    NS_LOG_DEBUG("Queue Length: " << qlen << "\tWcache Used: " << wcacheUsed);
    NS_LOG_DEBUG("Onchip buffer Remain: " << m_onChipBufferRemain
                                          << "\tDRAM Remain: " << dramRemain);
    NS_LOG_DEBUG("Wcache Full Threshold: " << m_wcacheFullTh[priority]
                                           << "\tWcache Congestion Threshold: "
                                           << m_wcacheCgTh[priority]);
    NS_LOG_DEBUG("Congestion Status: " << cgStatus << "\tLong Congestion Qlen: "
                                       << m_longCgQlen[portType][priority]);
    NS_LOG_DEBUG("Enable on-chip buffer PDP: "
                 << m_enableOnChipPdp
                 << "\tOnChip Buffer Used By Priority: " << m_priOnChipUsed[priority]
                 << "\tUsed By Pri&Dp: " << m_priDpUsed[port][priority]);

    if (qlen == 0 &&
        m_onChipBufferRemain >=
            pktSize) // 如果队列长度为0且"OnChip"缓存剩余空间大于等于数据包大小，则将数据包放入"OnChip"缓存
    {
        bmResult = BmResult(TO_ONCHIPBUFFER);
    }
    else if (
        qlen + pktSize >
        m_wredTh
            [priority]) // 如果数据包大小加上队列长度超过了权重随机早期丢弃（WRED）阈值，则直接丢弃数据包
    {
        bmResult = BmResult(DROP);
    }
    else if (
        cgStatus == CONGESTION &&
        wcacheUsed >=
            m_wcacheFullTh
                [priority]) // 如果队列处于拥塞状态且wcache使用超过了Wcache满阈值，则直接丢弃数据包
    {
        bmResult = BmResult(DROP);
    }
    else if (
        cgStatus == CONGESTION && wcacheUsed >= m_wcacheCgTh[priority] &&
        qlen >
            m_longCgQlen
                [portType]
                [priority]) // 如果队列处于拥塞状态且wcache缓存使用超过了Wcache拥塞阈值且队列长度大于最长拥塞队列长度，则直接丢弃数据包
    {
        bmResult = BmResult(DROP);
    }
    else if (cgStatus == CONGESTION && wcacheUsed < m_wcacheFullTh[priority] &&
             dramRemain >= pktSize)
    {
        bmResult = BmResult(
            TO_OFFCHIPBUFFER); // 如果队列处于拥塞状态且Wcache缓存使用未超过Wcache满阈值且Wcache缓存剩余空间大于等于数据包大小，则将数据包放入片外缓存
    }
    else if (m_onChipBufferRemain >= pktSize &&
             qlen <= m_alphaOfQueue[portType][type][priority][qIndex] * m_onChipBufferRemain &&
             m_priOnChipUsed[priority] < m_alphaOfPriority[priority] * m_onChipBufferRemain &&
             (m_enableOnChipPdp == 0 ||
              m_priDpUsed[port][priority] * (m_alphaOfPriority[priority] + 1) <
                  m_alphaOfPort[portType] * (m_alphaOfPriority[priority] * m_onChipBufferRemain -
                                             m_priOnChipUsed[priority])))
    {
        bmResult = BmResult(
            TO_ONCHIPBUFFER); // 如果"OnChip"缓存剩余空间大于等于数据包大小且满足一定条件，则将数据包放入"OnChip"缓存
    }
    else if (wcacheUsed < m_wcacheFullTh[priority] && dramRemain >= pktSize)
    {
        bmResult = BmResult(
            TO_OFFCHIPBUFFER); // 如果Wcache缓存使用未超过Wcache满阈值且片外缓存剩余空间大于等于数据包大小，则将数据包放入离片缓存
    }
    else
    {
        bmResult = BmResult(DROP); // 否则，直接丢弃数据包
    }
    // cout<<"m_alphaOfQueue["<<portType<<"]["<<type<<"]["<<priority<<"]["<<qIndex<<"]：
    // "<<m_alphaOfQueue[portType][type][priority][qIndex]<<endl;
    return bmResult;
}

/**
 * TODO:!!! There are still something need to be solved.
 * This Function wanna to give an unified interface of Buffer
 * Management Algorithm so that we can use this API to call diff-
 * erent BM Algorithm, like DT, ABM...But there may be a concern
 * that needs to be addressed: the counters of OnChipBuffer and OffChipBuffer
 * are different. Therefore, it is still doubtful whether a unified
 * interface can be used to implement different BM algorithm.
 */
bool
SwitchMmu::CheckBmAdmission(Ptr<Packet> packet, SwitchMmu::BmResult location)
{
    NS_LOG_FUNCTION(this << packet << location);
    // Left for Future Use.
    return true;
}

SwitchMmu::BmResult
SwitchMmu::CheckIngressAdmission(
    Ptr<Packet>
        packet) // 检查数据包是否符合准入条件，并调用FindBufferLocation函数来确定数据包的存储位置
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t prio = packet->GetMmuUsedPriority();

    // Check the validity of incoming parameters before go into BM
    // algorithm.//如果数据包为空或者端口号、队列索引或优先级超出范围，则输出错误信息并返回DROP
    if (packet == nullptr || port >= m_nPorts || qIndex >= m_nQueuesPerPort ||
        prio >= m_nPriorities)
    {
        NS_FATAL_ERROR("The input packet has invalid input parameters!!!");
        return DROP;
    }
    // cout<<"检查数据包是否能准入"<<endl;
    return FindBufferLocation(packet);
}

std::string
to_string_with_precision(double value, int precision)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

SwitchMmu::BmResult // BM总入口
SwitchMmu::FindBufferLocation(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort(); // 得到的是目的地端口
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    BmResult result = DROP;         // 初始化result为DROP
    BmAlgorithm bm = m_bmAlgorithm; // 并将m_bmAlgorithm赋值给bm

    // BM Switch Here.
    switch (bm)
    {
    case (HW):
        result = CheckHWBmAlgorithm(packet);
        break;
    case (TDT):
        result = Check3DTBmAlgorithm(packet);
        break;
    case (YSL):
        result = CheckYSLBmAlgorithm(packet);
        break;
    case (BASELINE):
        result = CheckBaselineBmAlgorithm(packet);
        break;
    case (DEEPHIR):
        result = CheckDeepHirBmAlgorithm(packet); //,CheckBaselineBmAlgorithm
        break;
    default:
        result = SwitchMmu::TO_OFFCHIPBUFFER;
        // std::cout<<"默认存储位置： "<<result<<endl;
    }

    // LOG some BM result.
    if (result == SwitchMmu::DROP)
    {
        m_stats.nTotalBmDropPackets++;
        m_stats.nTotalBmDropPacketsSize += pktSize;
        drop_real_per_period[port][priority][qIndex]++;
    }

    m_traceCheckAdmission(packet, result);

    if ((Simulator::Now() - Timer_Mill_Loss).GetMicroSeconds() >= 1.0){
        // cout<<"m_dequePktCnt"<<m_dequePktCnt<<endl;
        // cout<<"丢包数: "<<m_stats.nTotalBmDropPackets<<endl;
        // cout<<"总到达数: "<<m_stats.nTotalStoredPackets+m_stats.nTotalBmDropPackets<<endl;
       
        double LossPacketNumTotal = m_stats.nTotalBmDropPackets;           // 累积丢包数量
        double LossPacketNum = LossPacketNumTotal - LossPacketNum_Last; // 本轮丢包数量
        double LossPacketNumTotalSize = m_stats.nTotalBmDropPacketsSize; // 累积丢包总大小
        double LossPacketSize = LossPacketNumTotalSize - LossPacketNumTotalSizeLast; // 1us内的丢包量大小（B）
        double LossPacketRate;
        if (LossPacketNumTotal != 0){
            LossPacketRate = (static_cast<double>(m_stats.nTotalBmDropPackets) /
                              (m_stats.nTotalStoredPackets + m_stats.nTotalBmDropPackets + 1.0)) *
                             100.0;
        }
        else{
            LossPacketRate = 0;
        }
        // 获取和初始化阶段完全一致的文件路径
        const std::string fileName = GetLossPacketFilePath();
        std::ofstream fout(fileName, std::ios::out | std::ios::app);
        if (!fout.is_open()) {
            NS_FATAL_ERROR( "写入 loss_packet.csv 时打开文件失败。"
                << " fileName=" << fileName<< " baseFilePath=" << baseFilePath<< " now_algorithm_name=" << now_algorithm_name
                << " nextFilePath=" << nextFilePath);
        }
        const double periodPacketNumber = static_cast<double>(m_stats.perusStoredPackets) +LossPacketNum;
        double periodLossRate = 0.0;
        if (periodPacketNumber > 0.0){
            periodLossRate = LossPacketNum / periodPacketNumber * 100.0;
        }

        fout << Timer_Mill_Loss.GetSeconds() << "," << Simulator::Now().GetSeconds() << "," << LossPacketSize * 8.0 / 1000.0 << ","  << LossPacketRate << ","
            << periodLossRate << "," << LossPacketNumTotal  << std::endl;

        fout.close();

        m_stats.perusStoredPackets = 0;
        LossPacketNum_Last = LossPacketNumTotal;
        LossPacketNumTotalSizeLast = LossPacketNumTotalSize;
        Timer_Mill_Loss = Simulator::Now();
    }

    return result;
}


// 获取特定端口、队列索引和优先级下的队列状态
SwitchMmu::QueueStatus
SwitchMmu::GetQueueStatus(uint32_t port, uint32_t qIndex, uint32_t pri)
{
    NS_LOG_FUNCTION(this);
    BmAlgorithm bm =
        m_bmAlgorithm; // 获取当前SwitchMmu对象的bmAlgorithm成员变量，并将其赋值给局部变量bm
    SwitchMmu::QueueStatus status;

    // BM Congestion Switch Here.
    switch (bm)
    {
    case (HW):
        status = GetHWCgStatus(port,
                               qIndex,
                               pri); // 如果bm为HW或TDT，则调用GetHWCgStatus函数来获取硬件拥塞状态
        break;
    case (TDT):
        status = GetHWCgStatus(port, qIndex, pri);
        break;
    case (YSL):
        status = GetTimerQueueStatus(
            port,
            qIndex,
            pri); // 如果bm为YSL，则调用GetTimerQueueStatus函数来获取定时器队列状态
        break;
    case (YRF):
        status = GetHWCgStatus(port, qIndex, pri);
        break;
    default:
        status = SwitchMmu::
            NOT_CONGESTION; // 如果bm的取值不在上述范围内，则将队列状态设置为SwitchMmu::NOT_CONGESTION
    }

    return status;
}

SwitchMmu::QueueStatus
SwitchMmu::GetHWCgStatus(uint32_t port, uint32_t qIndex, uint32_t pri)
{
    NS_LOG_FUNCTION(this << port << qIndex << pri);
    PortType type = m_portRates[port];
    // cout<<"m_cgMax[type][pri]： "<<m_cgMax[type][pri]<<endl;
    if (m_qUsed[port][pri][qIndex] >
        m_cgMax
            [type]
            [pri]) // 判断当前队列的使用量m_qUsed是否大于该端口类型下优先级pri对应的最大拥塞阈值m_cgMax[type][pri]
    {
        m_cgStatus[port][pri][qIndex] = CONGESTION; // 如果是，则将该队列的拥塞状态设置为CONGESTION
    }
    else if (
        m_cgStatus[port][pri][qIndex] == true &&
        m_qUsed[port][pri][qIndex] <
            m_cgMin
                [type]
                [pri]) // 如果该队列当前状态为拥塞状态且队列的使用量小于该端口类型下优先级pri对应的最小拥塞阈值m_cgMin[type][pri]
    {
        m_cgStatus[port][pri][qIndex] = NOT_CONGESTION; // 则将该队列的拥塞状态设置为NOT_CONGESTION
    }

    // For HW they just use it to classified Congestion or not.
    return m_cgStatus[port][pri][qIndex]; // 返回该队列的拥塞状态
}

void
SwitchMmu::Store(Ptr<Packet> packet, SwitchMmu::BmResult location) // 紧接上存储决策算法
{
    NS_LOG_FUNCTION(this << packet << location);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t psize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;

    // if (m_onChipBufferRemain < psize)
    // {
    //     location = DROP;
    // }

    if (location == DROP) // 如果location为DROP，则记录警告信息并返回，表示数据包应该被丢弃
    {
        NS_LOG_WARN("The Packet should drop but not buffer ingress.");
        return;
    }

    m_nPackets++;                  // 增加已处理数据包的计数器
    m_stats.nTotalStoredPackets++; // 并更新总存储数据包的计数器
    m_stats.perusStoredPackets++;  // 更新每微秒内存储数据包的计数器

    if (location == TO_ONCHIPBUFFER)
    {
        // cout<<"port "<<port <<"  m_onChipBufferRemain-Store： "<<m_onChipBufferRemain <<"
        // pktSize-Store： "<<psize<<endl;
        NS_ASSERT_MSG(m_onChipBufferRemain >= psize, // 确保OnChip缓冲区剩余空间大于数据包大小
                      "When decided to ingress "
                      "packet into OnChipBuffer, the Remained size "
                      "should be larger than packet size.");

        NS_LOG_DEBUG("SwitchMmu: Store in OnChip!");
        m_stats.nTotalOnChipBufferStoredPackets++; // 更新相关统计信息，包括OnChip缓冲区存储数据包数量、剩余空间、优先级使用量等
        packet->SetLocation(Packet::ONCHIPBUFFER); // 将数据包位置标记为OnChipBuffer，并更新OnChip缓冲区剩余空间
        UsedSram_Size_Cycle[port][priority][qIndex] += psize;
        WriteSram_Size_Total += psize;

        m_onChipBufferRemain -= psize;
        m_priOnChipUsed[priority] += psize;
        m_priDpUsed[port][priority] += psize;

        // Trace
        m_traceSramWriteComplete(packet); // 记录Sram写完成的trace信息
    }
    else // 否则，将数据包存储到OffChip缓冲区中
    {
        NS_LOG_DEBUG("SwitchMmu: Store in OffChip!");
        m_offChipBuffer->Write(packet); // 调用OffChip缓冲区的Write函数将数据包写入OffChip缓冲区
        WriteDram_Size_Total += psize;
    }

    // Note Here:
    //
    // After Determine the Packet should be loaded in the Buffer.
    // The counters can be and should be modified immediately no
    // matter where it is decided to save. Because the following
    // input packet and BM algorithm should both take this packet
    // into account. To avoid unexpected count and buffer allocation
    // error!
    if (m_qUsed[port][priority][qIndex] ==
        0) // 如果当前队列m_qUsed[port][priority][qIndex]中没有数据包，则增加活跃队列数m_activeQueNum[port]
    {
        m_activeQueNum[port]++;
        m_activeQueNumSwitch++;
    }

    m_qlens[port][qIndex] += psize; // 更新队列长度、队列使用量、队列接收总量等信息
    m_qUsed[port][priority][qIndex] += psize;
    //--sj  TCP增加的输出
    if(print_flag == 0){
        std::cout << "MMU_PQS"
          << ",time_s=" << Simulator::Now().GetSeconds()
          << ",port=" << port
          << ",priority=" << priority
          << ",queue=" << qIndex
          << ",bytes=" << m_qUsed[port][priority][qIndex]
          << std::endl;
    }
    m_qTotalRcvd[port][priority][qIndex] += psize;
    if (m_qUsed[port][priority][qIndex] >
        m_qMaxUsed[port][priority]
                  [qIndex]) // 如果当前队列使用量超过历史最大使用量，则更新历史最大使用量
    {
        m_qMaxUsed[port][priority][qIndex] = m_qUsed[port][priority][qIndex];
    }

    m_enqueTime.push(Simulator::Now());

    m_traceStore(packet); // 记录存储数据包的trace信息

    // ShowCounters ();
}

void
SwitchMmu::FetchComplete(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t psize = packet->GetSize();

    NS_ASSERT_MSG(
        packet != nullptr,
        "The Reading Packet should not be NULL ptr!"); // 断言数据包不为空，确保数据包指针有效

    m_qUsed[port][priority][qIndex] -=
        psize; // 减少队列中数据包的使用量m_qUsed[port][priority][qIndex]和队列长度m_qlens[port][qIndex]
    m_qlens[port][qIndex] -= psize;
    if (m_qUsed[port][priority][qIndex] ==
        0) // 果当前队列中没有数据包，则减少活跃队列数m_activeQueNum[port]
    {
        m_activeQueNum[port]--;
        m_activeQueNumSwitch--;
    }

    m_nPackets--; // 减少已处理数据包的计数器

    // Set the Packet Place to be NOTINBUFFER.
    packet->SetLocation(
        Packet::NOTINBUFFER); // 将数据包位置标记为NOTINBUFFER，表示数据包已不在缓冲区中

    // Set the flag to tell the reorder model that the packet has been fetched
    packet->SetMmuFetchStatus(true); // 设置数据包的MMU读取状态为已完成

    // DEBUG Usage: show counters now.
    // ShowCounters ();
    m_traceFetch(packet); // 记录数据包读取完成的trace信息
}

bool
SwitchMmu::Fetch(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t psize = 0;

    Time delay = Simulator::Now() - m_enqueTime.front();
    Total_delay += delay.ToDouble(Time::NS);
    m_dequePktCnt++;
    m_avgDelay = Total_delay / m_dequePktCnt;
    m_enqueTime.pop();

    // cout << Simulator::Now().GetNanoSeconds() << " Total_delay(ms): " << Total_delay / 1000.0
    //      << " m_dequePktCnt: " << m_dequePktCnt << " m_avgDelay(ms): " << m_avgDelay / 1000.0
    //      << endl;

    if (packet != nullptr) // 如果数据包不为空，则获取数据包的大小
    {
        psize = packet->GetSize();
    }

    if (packet->GetLocation() == Packet::ONCHIPBUFFER) // 如果数据包在芯片上的缓冲区(ONCHIPBUFFER)中
    // The packet is in the OnChipBuffer(SRAM).
    {
        // Get packet Out.
        ReadSram_Size_Total += psize;
        ReadSram_Size_Cycle[port][priority][qIndex] +=
            psize; // 统计周期内优先级队列从SRAM读取的数据包大小
        UsedSram_Size_Cycle[port][priority][qIndex] -= psize;

        m_onChipBufferRemain += psize; // 增加芯片上缓冲区剩余空间的大小m_onChipBufferRemain
        m_priOnChipUsed[priority] -=
            psize; // 减少优先级对应的芯片上缓冲区使用量m_priOnChipUsed[priority]
        m_priDpUsed[port][priority] -=
            psize; // 端口优先级对应的芯片上缓冲区使用量m_priDpUsed[port][priority]

        // Packets in onchip buffer are considered to be completely fetched
        // immediately. So set the counters back here.
        FetchComplete(packet); // 调用FetchComplete函数标记数据包已经完全读取

        // Trace.
        m_traceSramReadComplete(packet);
    }
    else if (packet->GetLocation() == Packet::WRITINGTOOFFCHIPBUFFER ||
             packet->GetLocation() == Packet::OFFCHIPBUFFER ||
             packet->GetLocation() == Packet::WCACHE)
    {
        ReadDram_Size_Total += psize;
        if (packet->GetLocation() == Packet::WCACHE)
            cout << "时间：" << Simulator::Now().GetSeconds() << "," << "位置：wcache" << endl;
        if (packet->GetLocation() == Packet::WRITINGTOOFFCHIPBUFFER)
            cout << "时间：" << Simulator::Now().GetSeconds() << "," << "位置：部分wcache部分Dram"
                 << endl;
        // The packet is in the OffChipBuffer(WCache or HBM).
        return m_offChipBuffer->Read(packet); // 调用m_offChipBuffer->Read(packet)函数处理数据包读取
    }
    else // 如果数据包位置不在缓冲区中，则输出错误信息并返回false
    {
        NS_LOG_ERROR("Cannot Read the Packet which is not in Buffer!");
        return false;
    }

    return true; // 返回true表示数据包读取成功
}

void
SwitchMmu::SetOnChipPdpStatus(
    bool status) // 这段代码实现了设置芯片上PDP（Packet Data Processor）状态的函数
{
    NS_LOG_FUNCTION(this << status);
    m_enableOnChipPdp = status; // 将成员变量m_enableOnChipPdp的值设置为传入的status值
}

// 根据传入的flowType、qIndex、priority和alpha值，将m_alphaOfQueue数组中对应位置的值设置为alpha
// 这段代码的作用是为特定的流类型、队列索引、优先级设置相应的alpha值
void
SwitchMmu::SetQueueLevelAlpha(uint32_t flowType, uint32_t qIndex, uint32_t priority, uint32_t alpha)
{
    NS_LOG_FUNCTION(this << flowType << priority << qIndex << alpha);
    m_alphaOfQueue[Gbps100][flowType][priority][qIndex] = alpha;
}

// 这段代码实现了设置优先级级别alpha值的函数
void
SwitchMmu::SetPriorityLevelAlpha(uint32_t prior, uint32_t alpha)
{
    NS_LOG_FUNCTION(this << prior << alpha);
    m_alphaOfPriority[prior] = alpha;
}

// 实现了设置端口级别alpha值的功能
void
SwitchMmu::SetPortLevelAlpha(uint32_t port, uint32_t alpha)
{
    NS_LOG_FUNCTION(this << port << alpha);
    m_alphaOfPort[Gbps100] = alpha;
}

// 实现了设置端口速率类型的功能
void
SwitchMmu::SetPortRateType(uint32_t port, PortType type)
{
    NS_LOG_FUNCTION(this << port << type);
    m_portRates[port] = type;
}

SwitchMmu::PortType
SwitchMmu::GetPortRateType(uint32_t port)
{
    NS_LOG_FUNCTION(this << port);
    return m_portRates[port];
}

void
SwitchMmu::SetWcacheFullTh(uint32_t prior, uint64_t th)
{
    NS_LOG_FUNCTION(this << prior << th);
    m_wcacheFullTh[prior] = th; // 将m_wcacheFullTh数组中索引为prior的位置的值设置为传入的th值
}

void
SwitchMmu::SetWcacheCgTh(uint32_t prior, uint64_t th)
{
    NS_LOG_FUNCTION(this << prior << th);
    m_wcacheCgTh[prior] = th;
}

void
SwitchMmu::SetCgMin(uint32_t pri, uint64_t th)
{
    NS_LOG_FUNCTION(this << pri << th);
    m_cgMin[Gbps100][pri] = th;
}

void
SwitchMmu::SetCgMax(uint32_t pri, uint64_t th)
{
    NS_LOG_FUNCTION(this << pri << th);
    m_cgMax[Gbps100][pri] = th;
}

void
SwitchMmu::SetLongCgQlen(uint32_t pri, uint64_t qlen)
{
    NS_LOG_FUNCTION(this << pri << qlen);
    m_longCgQlen[Gbps100][pri] = qlen;
}

// 段代码是一个函数GetOffChipBuffer，用于返回一个指向OffChipBuffer对象的智能指针Ptr<OffChipBuffer>
Ptr<OffChipBuffer>
SwitchMmu::GetOffChipBuffer() const
{
    NS_LOG_FUNCTION(this);
    return m_offChipBuffer; // 返回成员变量m_offChipBuffer，该成员变量应该是一个指向OffChipBuffer对象的智能指针
}

// 将一个指向OffChipBuffer对象的智能指针附加到SwitchMmu对象上，并进行一系列操作
void
SwitchMmu::AttachOffChipBuffer(Ptr<OffChipBuffer> offChipBuffer)
{
    NS_LOG_FUNCTION(this << offChipBuffer);
    m_offChipBuffer =
        offChipBuffer; // 将传入的offChipBuffer指针赋值给SwitchMmu对象的成员变量m_offChipBuffer
    m_offChipBuffer->SetMmu(
        this); // 用offChipBuffer对象的SetMmu方法，将当前的SwitchMmu对象指针传递给offChipBuffer对象，以建立对象间的关联

    m_offChipBuffer
        ->TraceConnectWithoutContext( // 使用TraceConnectWithoutContext方法连接offChipBuffer对象的特定事件信号和SwitchMmu对象的相应处理函数
            "WCacheReadComplete",
            MakeCallback(
                &SwitchMmu::ReadWcacheComplete,
                this)); // 当"WCacheReadComplete"事件发生时，调用SwitchMmu对象的ReadWcacheComplete函数
    m_offChipBuffer->TraceConnectWithoutContext(
        "WCacheWriteComplete",
        MakeCallback(
            &SwitchMmu::WriteWcacheComplete,
            this)); // 当"WCacheWriteComplete"事件发生时，调用SwitchMmu对象的WriteWcacheComplete函数
    m_offChipBuffer->TraceConnectWithoutContext(
        "DramReadComplete",
        MakeCallback(
            &SwitchMmu::ReadDramComplete,
            this)); // 当"DramReadComplete"事件发生时，调用SwitchMmu对象的ReadDramComplete函数
    m_offChipBuffer->TraceConnectWithoutContext(
        "DramWriteComplete",
        MakeCallback(
            &SwitchMmu::WriteDramComplete,
            this)); // 当"DramWriteComplete"事件发生时，调用SwitchMmu对象的WriteDramComplete函数
}

// 用于注册设备处理程序（DeviceHandler）和对应的网络设备（NetDevice）到SwitchMmu对象中
void
SwitchMmu::RegisterDeviceHandler(DeviceHandler handler, Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << &handler << device);
    struct SwitchMmu::DeviceHandlerEntry
        entry;               // 创建一个SwitchMmu::DeviceHandlerEntry结构体对象entry
    entry.handler = handler; // 将传入的handler函数指针和device智能指针存储到entry结构体中
    entry.device = device;
    m_handlers.push_back(entry); // 将entry结构体添加到SwitchMmu对象的m_handlers容器中
}

// 这段代码是一个函数UnregisterDeviceHandler，用于从SwitchMmu对象中注销特定的设备处理程序（DeviceHandler）
void
SwitchMmu::UnregisterDeviceHandler(DeviceHandler handler)
{
    NS_LOG_FUNCTION(this << &handler);
    for (DeviceHandlerList::iterator i = m_handlers.begin(); i != m_handlers.end();
         i++) // 遍历m_handlers容器中的所有注册的设备处理程序
    {
        if (i->handler.IsEqual(handler)) // 对比每个设备处理程序的函数指针是否与传入的handler相等
        {
            m_handlers.erase(
                i); // 如果找到与handler相等的设备处理程序，则从m_handlers容器中删除该条目，并终止循环
            break;
        }
    }
}

// 用于处理来自特定网络设备的请求
bool
SwitchMmu::HandleRequest(Ptr<NetDevice> dev)
{
    NS_LOG_FUNCTION(this << dev);
    bool found = false; // 初始化一个布尔变量found为false，用于标记是否找到符合条件的设备处理程序
    for (DeviceHandlerList::iterator i = m_handlers.begin(); i != m_handlers.end();
         i++) // 遍历m_handlers容器中的所有注册的设备处理程序
    {
        if (!(i->device) ||
            ((i->device) &&
             i->device == dev)) // 对比每个设备处理程序的device字段是否为空或者与传入的dev相等
        {
            i->handler(); // 如果找到符合条件的设备处理程序，则调用该设备处理程序的处理函数，并将found标记为true
            found = true;
        }
    }
    return found; // 返回found，表示是否找到并处理了请求
}

// 获取SwitchMmu类中存储的数据包数量，并将其作为uint64_t类型返回
uint64_t
SwitchMmu::GetNPackets() const
{
    NS_LOG_FUNCTION(this);

    return m_nPackets;
}

uint64_t
SwitchMmu::GetQueueUsedBuffer(uint32_t port, uint32_t priority, uint32_t qIndex) const
{
    NS_LOG_FUNCTION(this);

    return m_qUsed[port][priority][qIndex];
}

uint64_t
SwitchMmu::GetQueueMaxUsedBuffer(uint32_t port, uint32_t priority, uint32_t qIndex) const
{
    NS_LOG_FUNCTION(this);

    return m_qMaxUsed[port][priority][qIndex];
}

uint64_t
SwitchMmu::GetQueueTotalReceived(uint32_t port, uint32_t priority, uint32_t qIndex) const
{
    NS_LOG_FUNCTION(this);

    return m_qTotalRcvd[port][priority][qIndex];
}

void
SwitchMmu::UpdateQueueStatus(uint32_t port, uint32_t priority, uint32_t qIndex, QueueStatus status)
{
    NS_LOG_FUNCTION(this);

    if (status == NOT_CONGESTION)
    {
        m_cgTimer[port][qIndex] = 0;
    }

    m_cgStatus[port][priority][qIndex] = status;
}

// 根据LRU（Least Recently Used）算法选择拥塞队列
void
SwitchMmu::SelectLRUCongestion()
{
    NS_LOG_FUNCTION(this);

    uint32_t maxI =
        0; // 初始化变量maxI、maxJ、maxT和CgQueue，分别用于记录最大的i、j、m_cgTimer值和拥塞队列数量
    uint32_t maxJ = 0;
    uint32_t maxT = 0;
    uint32_t CgQueue = 0;

    // Timer to figure the Congestion Control.
    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        for (uint32_t j = 0; j < m_nQueuesPerPort; j++)
        {
            if (m_cgStatus[i][0][j] ==
                BURST) // 使用双重循环遍历m_cgStatus数组，查找状态为BURST的队列，并根据条件进行处理
            {
                if (m_cgTimer[i][j] >
                    100) // 如果某个队列的m_cgTimer超过100，则将该队列状态设为CONGESTION，并增加CgQueue计数
                {
                    m_cgStatus[i][0][j] = CONGESTION;
                    CgQueue++;
                    break;
                }

                if (maxT < m_cgTimer[i][j] &&
                    m_cgStatus[i][0][j] ==
                        BURST) // 如果存在BURST状态的队列，选择m_cgTimer最大的队列作为LRU队列
                {
                    maxT = m_cgTimer[i][j];
                    maxI = i;
                    maxJ = j;
                }
            }
        }
    }

    if (CgQueue == 0 &&
        m_cgTimer[maxI][maxJ] >
            10) // 果没有发现拥塞队列，并且LRU队列的m_cgTimer大于10，则将LRU队列状态设为CONGESTION
    {
        m_cgStatus[maxI][0][maxJ] = CONGESTION;
    }
}

// 用于更新LRU定时器并定时调度自身以实现周期性更新
void
SwitchMmu::UpdateLruTimer()
{
    NS_LOG_FUNCTION(this);

    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        for (uint32_t j = 0; j < m_nQueuesPerPort; j++)
        {
            if (m_cgStatus[i][0][j] == BURST)
            { // 使用双重循环遍历m_cgStatus数组，对状态为BURST的队列的定时器m_cgTimer进行递增操作
                m_cgTimer[i][j]++;
            }
        }
    }

    Simulator::Schedule(m_updateLruTimeWindow,
                        &SwitchMmu::UpdateLruTimer,
                        this); // 调度下一次UpdateLruTimer函数的执行，以实现定时更新LRU定时器的功能
}

// 用于获取特定端口、队列索引和优先级下的队列状态
SwitchMmu::QueueStatus
SwitchMmu::GetTimerQueueStatus(uint32_t port, uint32_t qIndex, uint32_t pri)
{
    NS_LOG_FUNCTION(this << port << qIndex << pri);

    // Maybe Timer.

    // For HW they just use it to classified Congestion or not.
    return m_cgStatus[port][pri][qIndex];
}

} // namespace ns3
