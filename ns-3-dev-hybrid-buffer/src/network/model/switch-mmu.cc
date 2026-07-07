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
    // Predict_Flag_First=true;
    // YRF_Flag_result=true;//第一个周期开始时为true,表示初始化时周期存储片上,false时存储片外

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

    CountDramBandwidth();
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
    if (if_test8)
    {
        if (m_bmAlgorithm == DEEPHIR)
        {
            std::ostringstream thresholdDirectory;
            thresholdDirectory << std::fixed
                               << std::setprecision(1)
                               << Deeohir_threshold
                               << "M";
            outputDirectory /= thresholdDirectory.str();
        }

        outputDirectory /= std::to_string(flow_rate);
    }
    else if (if_test9)
    {
        if (m_bmAlgorithm == DEEPHIR)
        {
            std::ostringstream thresholdDirectory;

            thresholdDirectory << std::fixed
                               << std::setprecision(1)
                               << Deeohir_threshold
                               << "M";
            outputDirectory /= thresholdDirectory.str();
        }
    }
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

    ReadDram_Size_Cycle[port][priority][qIndex] += dramsize; // 统计周期内从DRAM读出的数据包大小

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
    Packet_Num_Cycle.resize(m_nPorts);
    Packet_Size_Cycle.resize(m_nPorts);
    Packet_Size_Cycle_Max.resize(m_nPorts);
    ReadSram_Size_Cycle.resize(m_nPorts);
    UsedSram_Size_Cycle.resize(m_nPorts);
    WriteDram_Size_Cycle.resize(m_nPorts);
    ReadDram_Size_Cycle.resize(m_nPorts);
    ReadSram_Rate_Cycle.resize(m_nPorts);
    WriteDram_Rate_Cycle.resize(m_nPorts);
    WriteDram_Rate_Cycle_last.resize(m_nPorts);
    ReadDram_Rate_Cycle.resize(m_nPorts);
    simulation_start.resize(m_nPorts);
    m_Cost_ETC.resize(m_nPorts);
    EWMA_R.resize(m_nPorts);
    YRF_Flag_result.resize(m_nPorts);
    Predict_Flag_First.resize(m_nPorts);
    Timer_Mill.resize(m_nPorts);
    eta.resize(m_nPorts);
    cs_out_array.resize(m_nPorts);
    delta_Q_array.resize(m_nPorts);
    utility.resize(m_nPorts);
    decision.resize(m_nPorts);
    Sr_last.resize(m_nPorts);
    Dr_last.resize(m_nPorts);
    Pqs_last.resize(m_nPorts);
    T_seq.resize(m_nPorts);
    drop_flag.resize(m_nPorts);
    drop_DRAM_last.resize(m_nPorts);
    drop_real_per_period.resize(m_nPorts);
    AI.resize(m_nPorts);
    MD.resize(m_nPorts);
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
            Packet_Num_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            Packet_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            Packet_Size_Cycle_Max[i][j].resize(m_nQueuesPerPort, 0);
            ReadSram_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            UsedSram_Size_Cycle[i][j].resize(m_nQueuesPerPort), 0;
            WriteDram_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            ReadDram_Size_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            ReadSram_Rate_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            WriteDram_Rate_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            WriteDram_Rate_Cycle_last[i][j].resize(m_nQueuesPerPort, 0);
            ReadDram_Rate_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            simulation_start[i][j].resize(m_nQueuesPerPort);
            m_Cost_ETC[i][j].resize(m_nQueuesPerPort, NanoSeconds(min_T));
            EWMA_R[i][j].resize(m_nQueuesPerPort, 0);
            YRF_Flag_result[i][j].resize(m_nQueuesPerPort, 1);
            Predict_Flag_First[i][j].resize(m_nQueuesPerPort, 1);
            Timer_Mill[i][j].resize(m_nQueuesPerPort);
            eta[i][j].resize(m_nQueuesPerPort, 1e-2);
            cs_out_array[i][j].resize(m_nQueuesPerPort, 0);
            delta_Q_array[i][j].resize(m_nQueuesPerPort, 0);
            utility[i][j].resize(m_nQueuesPerPort, 0);
            decision[i][j].resize(m_nQueuesPerPort, 0);
            Sr_last[i][j].resize(m_nQueuesPerPort, 0);
            Dr_last[i][j].resize(m_nQueuesPerPort, 0);
            Pqs_last[i][j].resize(m_nQueuesPerPort, 0);
            T_seq[i][j].resize(m_nQueuesPerPort, 0);
            drop_flag[i][j].resize(m_nQueuesPerPort, 1);
            drop_DRAM_last[i][j].resize(m_nQueuesPerPort, 0);
            drop_real_per_period[i][j].resize(m_nQueuesPerPort, 0);
            AI[i][j].resize(m_nQueuesPerPort, 1 * 1e3);
            MD[i][j].resize(m_nQueuesPerPort, 1);
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
        Packet_Num_Cycle[i].resize(m_nPriorities);
        Packet_Size_Cycle[i].resize(m_nPriorities);
        Packet_Size_Cycle_Max[i].resize(m_nPriorities);
        ReadSram_Size_Cycle[i].resize(m_nPriorities);
        UsedSram_Size_Cycle[i].resize(m_nPriorities);
        WriteDram_Size_Cycle[i].resize(m_nPriorities);
        ReadDram_Size_Cycle[i].resize(m_nPriorities);
        ReadSram_Rate_Cycle[i].resize(m_nPriorities);
        WriteDram_Rate_Cycle[i].resize(m_nPriorities);
        WriteDram_Rate_Cycle_last[i].resize(m_nPriorities);
        ReadDram_Rate_Cycle[i].resize(m_nPriorities);
        simulation_start[i].resize(m_nPriorities);
        m_Cost_ETC[i].resize(m_nPriorities);
        EWMA_R[i].resize(m_nPriorities);
        YRF_Flag_result[i].resize(m_nPriorities);
        Predict_Flag_First[i].resize(m_nPriorities);
        Timer_Mill[i].resize(m_nPriorities);
        eta[i].resize(m_nPriorities);
        cs_out_array[i].resize(m_nPriorities);
        delta_Q_array[i].resize(m_nPriorities);
        utility[i].resize(m_nPriorities);
        decision[i].resize(m_nPriorities);
        Sr_last[i].resize(m_nPriorities);
        Dr_last[i].resize(m_nPriorities);
        Pqs_last[i].resize(m_nPriorities);
        T_seq[i].resize(m_nPriorities);
        drop_flag[i].resize(m_nPriorities);
        drop_DRAM_last[i].resize(m_nPriorities);
        drop_real_per_period[i].resize(m_nPriorities);
        AI[i].resize(m_nPriorities);
        MD[i].resize(m_nPriorities);
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
SwitchMmu::CheckDeepHirBmAlgorithm(
    Ptr<Packet>
        packet) // 该函数用于检查基准缓存管理算法，根据输入的数据包信息和当前交换机状态，确定输入数据包的缓存位置
{
    NS_LOG_FUNCTION(this << packet);
    static uint64_t total_packet_num = 0;
    total_packet_num++;
    // cout << "total_packet_num:" << total_packet_num << endl;
    // 跑的测试脚本8 要更改BMS阈值
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
    // uint64_t dramUsed = m_offChipBuffer->GetDramUsed();

    // uint64_t pth= m_onChipBufferSize;
    // PortType type = m_portRates[port];

    // cout << Simulator::Now().GetNanoSeconds() << " xzx_debug_sign: " << " port: " << port
    //      << " queuused: " << qlen / 1024 / 1024 << endl;

    double DT_ths = (1.0 / m_activeQueNum[port]) * m_onChipBufferRemain;
    double DT_thd = (1.0 / m_activeQueNum[port]) * dramRemain;

    uint64_t DT_Threshold = DT_alpha * m_onChipBufferRemain; // DT;  alpha=2； 单位 B    动态阈值 = α × 当前剩余的片上缓存

    const uint64_t Pqs =UsedSram_Size_Cycle[port][priority][qIndex];  // --sj 从三维数组或三维容器中，读取某个具体队列当前占用的 SRAM 大小。

    bmResult = BmResult(DROP); // 否则，直接丢弃数据包

    if (print_flag == 1){
        // 打印当前端口队列、SRAM队列、DRAM队列
        cout << "当前端口队列长度: " << qlen << " 剩余SRAM缓存: " << m_onChipBufferRemain << "剩余DRAM缓存: " << dramRemain << endl;
    }
    
    NS_ASSERT_MSG(priority <= 1, "优先级只有2个");
    if ((qlen + pktSize) <= m_wredTh[priority] && m_onChipBufferRemain >= pktSize && (qlen + pktSize) <= DT_Threshold)
    {
            bmResult = BmResult(TO_ONCHIPBUFFER);
            if (print_flag == 1){
            cout << "Time:" << Simulator::Now() << "  packet:" << packet->GetUid() <<" pktSize: "<<packet->GetSize()<< " 端口:" << port
                 << " 存入片内" << endl;
            }
    }
    else
    {
        if ((wcacheSize - wcacheUsed) >= pktSize && dramRemain >= pktSize)
        {
            if (print_flag == 1){
            cout << "Time:" << Simulator::Now() << "  packet:" << packet->GetUid() <<" pktSize: "<<packet->GetSize()
                 << " 端口:" << port << " 存入片外" << endl;
            }
            bmResult = BmResult(TO_OFFCHIPBUFFER);
        }
    }
    if (bmResult == DROP)
    {
        if (wcacheSize - wcacheUsed < pktSize)
        {
            if (print_flag == 1){
            cout << "Time:" << Simulator::Now() << "  packet:" << packet->GetUid()
                 << " 端口:" << port << " 丢包原因:Dram带宽不足(wcahe不够)" << endl;
            }
        }
        else
        {
            if (print_flag == 1){
            cout << "Time:" << Simulator::Now() << " packet:" << packet->GetUid()
                 << " 端口:" << port << " 丢包原因:未知" << endl;
            }
        }
    }

    return bmResult;
}

void
SwitchMmu::CountDramBandwidth()
{
    WriteDram_Rate_Total = WriteDram_Size_Total / Dram_Bandwidth_Timer * 8; // B / ns *8= Gbps
    ReadDram_Rate_Total = ReadDram_Size_Total / Dram_Bandwidth_Timer * 8;
    WriteDram_Size_Total = 0;
    ReadDram_Size_Total = 0;
    Simulator::Schedule(NanoSeconds(Dram_Bandwidth_Timer), &SwitchMmu::CountDramBandwidth, this);
}

SwitchMmu::BmResult
SwitchMmu::Check3DTBmAlgorithm(Ptr<Packet> packet) // PBS-new   //kkkk
{
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort();         // 端口号
    uint32_t qIndex = packet->GetMmuUsedQueueId();    // 队列号
    uint32_t priority = packet->GetMmuUsedPriority(); // 优先级
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION; // 数据包在缓存中实际占用的大小，
    constexpr double EPS = 1e-12;
    constexpr double BYTES_PER_MB = 1e6;  //单位转换的时候使用
    constexpr double AI_NS = 1000.0;      // AI = 1 us = 1000 ns  一开始的数值
    constexpr double MD_EPSILON = 5.0;    // MD 公式中的 epsilon  （40）公式里用的参数
    constexpr double T_MAX_NS = 32000.0;  // 最大周期 32 us

    // Utility 中 U2 的权重
    // U = U1 + eta × U2  
    const double utilityEta = std::max(static_cast<double>(eta[port][priority][qIndex]), EPS);
    // 周期乘性减小使用独立的 eta_MD，不能与 Utility 权重混用。
    const double mdEta = std::max(static_cast<double>(eta_MD), EPS);

    // 根据端口类型取得实际出口带宽，单位 Gbps。
    double mu = 100.0;
    // switch (m_portRates[port])
    // {
    // case Gbps100:
    //     mu = 100.0;
    //     break;
    // case Gbps400:
    //     mu = 400.0;
    //     break;
    // case Gbps800:
    //     mu = 800.0;
    //     break;
    // default:
    //     NS_FATAL_ERROR("Unsupported port rate type, port=" << port);
    // }
    BmResult bmResult = BmResult(DROP);  //默认一开始丢包

    uint64_t qlen = m_qUsed[port][priority][qIndex];   // 当前队列总占用包括 SRAM 和 DRAM，单位：Byte
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();   // 写缓存使用量和总容量，
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();   // 总容量，
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();   // DRAM 剩余容量

    double Pqs = static_cast<double>(UsedSram_Size_Cycle[port][priority][qIndex]);  // 当前队列位于 SRAM 中的数据量
    double math_mETC =static_cast<double>(m_Cost_ETC[port][priority][qIndex].GetNanoSeconds());   // 当前配置的 ETC 周期长度，单位：ns
    double cycleTimeNs =static_cast<double>((Simulator::Now() - simulation_start[port][priority][qIndex]) .GetNanoSeconds());  // 当前 ETC 周期已经经过的实际时间，

    const double S = static_cast<double>(m_onChipBufferSize);       // SRAM 总容量，B
    const double D = m_offChipBuffer->GetDramBandwidth();           // DRAM 总带宽，Gbps
    const double Sr = static_cast<double>(m_onChipBufferRemain);    // SRAM 剩余容量，B
    const double Dr = std::max(0.0, D - WriteDram_Rate_Total - ReadDram_Rate_Total);    // 当前 DRAM 剩余带宽

    const double dtThresholdBytes = DT_alpha * Sr;  //DT_Threshold = DT_alpha × SRAM剩余容量

    const bool hasWcacheSpace = wcacheUsed <= wcacheSize && (wcacheSize - wcacheUsed) >= pktSize;  //是否还有写的空间
    const bool hasDramSpace = dramRemain >= pktSize;   //是否还有Dram的空间


    auto printPacketAdmissionResult = [&](bool intendedStoreSram)
    {
        if (print_flag != 1)
        {
            return;
        }

        if (bmResult == BmResult(DROP) &&
            intendedStoreSram &&
            Pqs + static_cast<double>(pktSize) > dtThresholdBytes)
        {
            cout << "Pqs + pktSize:" << Pqs + pktSize
                 << "| DT_Threshold:" << dtThresholdBytes
                 << "  DT阈值丢包" << endl;
        }

        if (bmResult == BmResult(TO_ONCHIPBUFFER))
        {
            cout << "Time:" << Simulator::Now()
                 << "  packet:" << packet->GetUid()
                 << " pktSize: " << packet->GetSize()
                 << " 端口:" << port
                 << " 存入片内" << endl;
        }
        else if (bmResult == BmResult(TO_OFFCHIPBUFFER))
        {
            cout << "Time:" << Simulator::Now()
                 << "  packet:" << packet->GetUid()
                 << " pktSize: " << packet->GetSize()
                 << " 端口:" << port
                 << " 存入片外" << endl;
        }
        else
        {
            if (!hasWcacheSpace)
            {
                cout << "Time:" << Simulator::Now()
                     << "  packet:" << packet->GetUid()
                     << " 端口:" << port
                     << " 丢包原因:Dram带宽不足(wcahe不够)"
                     << endl;
            }
            else
            {
                cout << "Time:" << Simulator::Now()
                     << " packet:" << packet->GetUid()
                     << " 端口:" << port
                     << " 丢包原因:未知"
                     << endl;
            }
        }
    };

    // 长时间无流量后，开始新一轮流量,当空闲时间超过 2 × ETC 时，认为上一轮流量结束，,注意：先重置上一轮状态，再统计当前数据包，避免当前数据包被清零操作直接丢失。

    if (math_mETC > 0.0 && cycleTimeNs > 2.0 * math_mETC)
    {
        Predict_Flag_First[port][priority][qIndex] = 1;

        m_Cost_ETC[port][priority][qIndex] = NanoSeconds(min_T);
        math_mETC = static_cast<double>(m_Cost_ETC[port][priority][qIndex].GetNanoSeconds());
        // 保存新周期起点的资源状态
        Sr_last[port][priority][qIndex] = Sr;
        Dr_last[port][priority][qIndex] = Dr;
        Pqs_last[port][priority][qIndex] = Pqs;

        ReadSram_Size_Cycle[port][priority][qIndex] = 0;
        WriteDram_Size_Cycle[port][priority][qIndex] = 0;
        ReadDram_Size_Cycle[port][priority][qIndex] = 0;

        ReadSram_Rate_Cycle[port][priority][qIndex] = 0.0;
        WriteDram_Rate_Cycle[port][priority][qIndex] = 0.0;
        WriteDram_Rate_Cycle_last[port][priority][qIndex] = 0.0;
        ReadDram_Rate_Cycle[port][priority][qIndex] = 0.0;

        Packet_Num_Cycle[port][priority][qIndex] = 0;
        Packet_Size_Cycle[port][priority][qIndex] = 0;

        EWMA_R[port][priority][qIndex] = 0.0;
        utility[port][priority][qIndex] = 0.0;
        cs_out_array[port][priority][qIndex] = 0.0;
        delta_Q_array[port][priority][qIndex] = 0.0;

        simulation_start[port][priority][qIndex] = Simulator::Now();
        cycleTimeNs = 0.0;

        T_seq[port][priority][qIndex] = 0;
        drop_real_per_period[port][priority][qIndex] = 0;

    }

    //  统计当前 ETC 周期总到达量
    Packet_Size_Cycle[port][priority][qIndex] += pktSize;
    Packet_Num_Cycle[port][priority][qIndex] += 1;

    //  第一周期准入
    if (math_mETC > 0.0 && Predict_Flag_First[port][priority][qIndex] == 1)
    {
        if (Pqs + static_cast<double>(pktSize) <= dtThresholdBytes && m_onChipBufferRemain >= pktSize)
        {
            bmResult = BmResult(TO_ONCHIPBUFFER);
            YRF_Flag_result[port][priority][qIndex] = true;
            Ctag[packet->GetUid()] = std::make_pair(1, static_cast<size_t>(pktSize));
        }
        else if (hasWcacheSpace && hasDramSpace)
        {
            bmResult = BmResult(TO_OFFCHIPBUFFER);
            YRF_Flag_result[port][priority][qIndex] = false;
            Ctag[packet->GetUid()] = std::make_pair(0, static_cast<size_t>(pktSize));
        }
        else
        {
            bmResult = BmResult(DROP);
            Ctag.erase(packet->GetUid());
        }

        // 保存第一周期起点状态。
        simulation_start[port][priority][qIndex] = Simulator::Now();
        EWMA_R[port][priority][qIndex] = 0.0;
        Sr_last[port][priority][qIndex] = Sr;
        Dr_last[port][priority][qIndex] = Dr;
        Pqs_last[port][priority][qIndex] = Pqs;
        WriteDram_Rate_Cycle_last[port][priority][qIndex] = 0.0;

        Predict_Flag_First[port][priority][qIndex] = 2;
        T_seq[port][priority][qIndex] = 0;

        if (print_flag == 1)
        {
            cout << "PBSFirstPeriod"
                 << " time:" << Simulator::Now().GetNanoSeconds()
                 << " port:" << port
                 << " priority:" << priority
                 << " qIndex:" << qIndex
                 << " direction:"
                 << (YRF_Flag_result[port][priority][qIndex] ? "SRAM" : "DRAM")
                 << " result:" << static_cast<int>(bmResult)
                 << endl;
        }
        printPacketAdmissionResult(
            YRF_Flag_result[port][priority][qIndex]);

        return bmResult;
    }

    // 当前 ETC 周期尚未结束：严格保持本周期方向
    // 不允许 SRAM -> DRAM 或 DRAM -> SRAM 的静默 fallback。
    if (math_mETC > cycleTimeNs && Predict_Flag_First[port][priority][qIndex] != 1)
    {
        const bool currentStoreSram = YRF_Flag_result[port][priority][qIndex];
        if (currentStoreSram)
        {
            if (Pqs + static_cast<double>(pktSize) > dtThresholdBytes)
            {
                bmResult = BmResult(DROP);
            }
            else if (m_onChipBufferRemain >= pktSize)
            {
                bmResult = BmResult(TO_ONCHIPBUFFER);
            }
            else
            {
                bmResult = BmResult(DROP);
            }
        }
        else
        {
            if (hasWcacheSpace && hasDramSpace)
            {
                bmResult = BmResult(TO_OFFCHIPBUFFER);
            }
            else
            {
                bmResult = BmResult(DROP);
            }
        }
        if (bmResult == BmResult(TO_ONCHIPBUFFER))
        {
            Ctag[packet->GetUid()] = std::make_pair(1, static_cast<size_t>(pktSize));
        }
        else if (bmResult == BmResult(TO_OFFCHIPBUFFER))
        {
            Ctag[packet->GetUid()] = std::make_pair(0, static_cast<size_t>(pktSize));
        }
        else
        {
            Ctag.erase(packet->GetUid());
        }
        printPacketAdmissionResult(currentStoreSram);
        return bmResult;
    }

    // 当前 ETC 周期结束
    const double actualPeriodNs = std::max(cycleTimeNs, EPS);
    if (math_mETC <= EPS)
    {
        math_mETC = actualPeriodNs;
    }
    // 保存上一周期真正执行的方向
    const bool previousStoreSram = YRF_Flag_result[port][priority][qIndex];

    // 保留并计算周期速率
    const double previousQueueDramWriteRate = WriteDram_Rate_Cycle[port][priority][qIndex];
    WriteDram_Rate_Cycle_last[port][priority][qIndex] = previousQueueDramWriteRate;

    ReadSram_Rate_Cycle[port][priority][qIndex] =
        static_cast<double>(ReadSram_Size_Cycle[port][priority][qIndex]) *
        8.0 / actualPeriodNs;

    WriteDram_Rate_Cycle[port][priority][qIndex] =
        static_cast<double>(WriteDram_Size_Cycle[port][priority][qIndex]) *
        8.0 / actualPeriodNs;

    ReadDram_Rate_Cycle[port][priority][qIndex] =
        static_cast<double>(ReadDram_Size_Cycle[port][priority][qIndex]) *
        8.0 / actualPeriodNs;

    // 本周期总到达量和总到达速率
    const double actualArrivalBytes = static_cast<double>(Packet_Size_Cycle[port][priority][qIndex]);
    const double actualArrivalRate =  actualArrivalBytes * 8.0 / actualPeriodNs; // Gbps
    
    const double Cqs =
        ReadSram_Rate_Cycle[port][priority][qIndex];
    const double temp_for_record_last_ewma_r =
        EWMA_R[port][priority][qIndex];
    const double temp_for_record_last_T =
        math_mETC;
    const double temp_for_record_last_cs_out =
        cs_out_array[port][priority][qIndex];
    const double temp_for_record_last_delta_q =
        delta_Q_array[port][priority][qIndex];

    //  更新输入速率 EWMA

    const double gamma = std::clamp(static_cast<double>(EWMA_W), 0.0, 1.0);

    if (T_seq[port][priority][qIndex] == 0 || EWMA_R[port][priority][qIndex] <= EPS)
    {
        EWMA_R[port][priority][qIndex] = actualArrivalRate;
    }
    else
    {
        EWMA_R[port][priority][qIndex] =  gamma * EWMA_R[port][priority][qIndex] + (1 - gamma) * actualArrivalRate;
    }

    const double ewmaRate = std::max(0.0, EWMA_R[port][priority][qIndex]);


    if (print_flag == 1)
    {
        cout << "wktest port " << port << " priority " << priority  << " qIndex " << qIndex << endl;

        cout << Simulator::Now().GetNanoSeconds()
             << " states_in_end_of_period "
             << T_seq[port][priority][qIndex]
             << " port: " << port
             << " lambda: " << ewmaRate
             << "  miu(Gbps): " << Cqs   << "  Pqs(MB): " << Pqs * 1e-6
             << "  Sr(MB): " << Sr * 1e-6
             << "  Dr(Gbps): " << Dr  << "  S(MB): " << S * 1e-6
             << "  D(Gbps): " << D
             << " cycle_time(ns): " << cycleTimeNs
             << " math_ETC(ns): " << math_mETC
             << " ReadSram_Size_Cycle[port][priority][qIndex]: " << ReadSram_Size_Cycle[port][priority][qIndex]
             << " Packet_Size_Cycle[port][priority][qIndex]: " << Packet_Size_Cycle[port][priority][qIndex]
             << "  WriteDram_Size_Cycle[port][priority][qIndex] "<< WriteDram_Size_Cycle[port][priority][qIndex]
             << " qlen(MB): " << qlen * 1e-6
             << " activePort: " << m_activeQueNumSwitch
             << endl;
    }


    // 计算上一周期实际 Utility U*
    double U1Ss = 0.0;
    double U2Ss = 0.0;
    double U1Ds = 0.0;
    double U2Ds = 0.0;
    double U_star = 0.0;
    const double actualDeltaQiS = Pqs - Pqs_last[port][priority][qIndex];

    const double safeSrLast = std::max(Sr_last[port][priority][qIndex], EPS);

    // Qs(t) + DeltaQ - alpha * (Sr(t) - DeltaQ)
    const double actualSramPressureBytes =
        Pqs_last[port][priority][qIndex] +
        actualDeltaQiS -
        DT_alpha *
            (Sr_last[port][priority][qIndex] - actualDeltaQiS);

    U1Ss =1.0 / std::max(1.0, actualSramPressureBytes / BYTES_PER_MB);

    // SRAM 占用增加时 DeltaQ > 0，U2Ss 应为负数。
    U2Ss = -actualDeltaQiS / safeSrLast;
    //实际带宽
    const double actualAvailableDramRate =std::max(Dr_last[port][priority][qIndex] + WriteDram_Rate_Cycle_last[port][priority][qIndex],EPS);
    //可用带宽
    const double actualDramExcessBytes = std::max(0.0, actualArrivalRate - actualAvailableDramRate) * actualPeriodNs / 8.0;
    //实际效用
    U1Ds =
        1.0 /
        std::max(1.0, actualDramExcessBytes / BYTES_PER_MB);

    U2Ds = -actualArrivalRate / actualAvailableDramRate;

    if (previousStoreSram)
    {
        U_star = U1Ss + utilityEta * U2Ss;  //拉平一下
    }
    else
    {
        U_star = U1Ds + utilityEta * U2Ds;
    }

    NS_ASSERT_MSG(std::isfinite(U_star), "PBS U_star is NaN or Inf");
    if (print_flag == 1)
    {
        cout << " real: "
             << " U1S: " << U1Ss<< " U2S: " << U2Ss
             << " U1D: " << U1Ds<< " U2D: " << U2Ds
             << " Real_Drop:"
             << drop_real_per_period[port][priority][qIndex]
             << endl;
    }
    // 根据预测误差更新下一周期 T
    const double lastUtility = utility[port][priority][qIndex];

    const bool hasValidLastPrediction =  T_seq[port][priority][qIndex] > 0;

    const double deltaU = hasValidLastPrediction ? std::fabs(lastUtility - U_star) : 0.0;  //预测的误差大小

    double MD1 = mdEta /std::max(mdEta + MD_EPSILON * deltaU, EPS);  //MD的更新
    MD1 = std::clamp(MD1, 0.0, 1.0);

    double nextPeriodNs =  std::min((math_mETC + AI_NS) * MD1, T_MAX_NS);  //T(t+1)=min[(T(t)+AI)×MD,Tmax​]
    nextPeriodNs =  std::max(nextPeriodNs, static_cast<double>(min_T));

    const int64_t roundedPeriodNs =
        std::max<int64_t>(static_cast<int64_t>(min_T),
                          static_cast<int64_t>(std::llround(nextPeriodNs)));

    m_Cost_ETC[port][priority][qIndex] =  NanoSeconds(roundedPeriodNs);

    const double newT = static_cast<double>( m_Cost_ETC[port][priority][qIndex].GetNanoSeconds());
    const double sQuarterDrainTimeNs =
        ewmaRate > EPS
            ? (S / 4.0) * 8.0 / ewmaRate
            : 0.0;

    if (print_flag == 1)
    {
        cout << Simulator::Now().GetNanoSeconds()
             << " New_Period T(ns):" << newT
             << " S/4/ewma_r(ns): " << sQuarterDrainTimeNs
             << "  U_star[T]: " << U_star
             << "  U[T]: " << lastUtility
             << " T(t): " << math_mETC
             << " AI: " << AI_NS
             << " MD: " << MD1
             << " Tmax: " << T_MAX_NS
             << " |U-U*|: " << deltaU
             << " eta_MD: " << mdEta
             << " gamma: " << gamma
             << endl;

        const double actualSelectedU2 =
            previousStoreSram ? U2Ss : U2Ds;

        const double oldPredictedU2 =
            previousStoreSram
                ? -temp_for_record_last_delta_q /
                      std::max(
                          Sr_last[port][priority][qIndex],
                          EPS)
                : -temp_for_record_last_ewma_r /
                      std::max(
                          Dr_last[port][priority][qIndex],
                          EPS);

        cout << "分析---- 时间:"
             << Simulator::Now().GetNanoSeconds()
             << " "
             << "决策位置:"
             << (previousStoreSram ? 1 : 0)
             << " "
             << "predical lambda:"
             << temp_for_record_last_ewma_r
             << " "
             << "period:"
             << temp_for_record_last_T
             << " "
             << "predical CS_OUT:"
             << temp_for_record_last_cs_out
             << " "
             << "predical delta_Q_i_s:"
             << temp_for_record_last_delta_q
             << " "
             << "last Sr:"
             << Sr_last[port][priority][qIndex]
             << " "
             << "last Dr:"
             << Dr_last[port][priority][qIndex]
             << " "
             << "real C_i_in:"
             << Packet_Size_Cycle[port][priority][qIndex]
             << " "
             << "real C_i_sout:"
             << ReadSram_Size_Cycle[port][priority][qIndex]
             << " "
             << "U2star:"
             << actualSelectedU2
             << " "
             << "port:"
             << port
             << " "
             << "U2:"
             << oldPredictedU2
             << " "
             << endl;
    }
    // 分别预测下一周期选择 SRAM/DRAM 时的结果

    const double QiS = std::max(0.0, Pqs);
    const double QiD = std::max(0.0, static_cast<double>(qlen) - QiS);

    NS_ASSERT_MSG(QiS <= static_cast<double>(qlen) + 1e-6,
                  "PBS counter error: QiS is greater than qlen");

    const bool isMixed = QiS > EPS && QiD > EPS;
    const bool currentStoreSram = previousStoreSram;
    const double lambda = ewmaRate;             // Gbps
    const double incomingBytes = lambda * newT / 8.0;
    const double serviceBytes = mu * newT / 8.0;

    // 假设下一周期选择 SRAM
    double sramOutIfSram = 0.0;
    if (serviceBytes <= QiS)
    {
        // 一个周期连原有 SRAM 数据都发不完
        sramOutIfSram = serviceBytes;
    }
    else
    {
        // 发送完旧 SRAM 数据需要的时间，单位 ns
        const double oldSramDrainTimeNs = QiS * 8.0 / mu;

        // 发送完旧 SRAM 数据后的剩余时间
        const double remainingTimeNs = std::max( 0.0, newT - oldSramDrainTimeNs);

        // 剩余时间内可从 SRAM 发出的新到达数据
        const double newSramDataOut =std::min(lambda, mu) * remainingTimeNs / 8.0;

        sramOutIfSram =QiS + newSramDataOut;
    }

    sramOutIfSram = std::clamp(sramOutIfSram,0.0, std::min(serviceBytes, QiS + incomingBytes));

    const double deltaQIfSram =  incomingBytes - sramOutIfSram;

    // 假设下一周期选择 DRAM
    double sramOutIfDram = 0.0;
    if (serviceBytes <= QiD)
    {
        // 服务能力全部用于 DRAM 数据，SRAM 没有机会输出。
        sramOutIfDram = 0.0;
    }
    else
    {
        // 发完 DRAM 数据后，剩余服务能力只能输出原有 SRAM 数据。
        const double remainingServiceBytes = serviceBytes - QiD;
        sramOutIfDram = std::min(QiS, remainingServiceBytes);
    }

    sramOutIfDram =  std::clamp(sramOutIfDram,  0.0,  std::min(QiS, serviceBytes));

    // 选择 DRAM 时没有新数据进入 SRAM，SRAM 只因输出而减少。
    const double deltaQIfDram = -sramOutIfDram;
    if (print_flag == 1)
    {
        cout << "T轮从Sram读出实际数据量："
             << ReadSram_Size_Cycle[port][priority][qIndex]
             << "B"
             << endl;

        cout << "预测T+1轮从Sram读出数据量："
             << (currentStoreSram
                     ? sramOutIfSram
                     : sramOutIfDram)
             << "B"
             << endl;

        cout << "T+1轮的totalSize:"
             << serviceBytes
             << "B"
             << endl;

        cout << "PBSStorageState"
             << " port:" << port
             << " priority:" << priority
             << " qIndex:" << qIndex
             << " Qi:" << qlen
             << " QiS:" << QiS
             << " QiD:" << QiD
             << " mixed:" << isMixed
             << " currentStore:"
             << (currentStoreSram ? "SRAM" : "DRAM")
             << " lambda:" << lambda
             << " u:" << mu
             << " outsIfSram:" << sramOutIfSram
             << " outsIfDram:" << sramOutIfDram
             << endl;
    }
    // 计算两个候选方向的预测 Utility
    const double safeSr = std::max(Sr, EPS);
    const double predictedSramPressureBytes = QiS + deltaQIfSram -   DT_alpha * (Sr - deltaQIfSram);

    const double U_sram_1 =  1.0 / std::max(1.0, predictedSramPressureBytes / BYTES_PER_MB);

    const double U_sram_2 = -deltaQIfSram / safeSr;
    const double U_sram =  U_sram_1 + utilityEta * U_sram_2;

    const double availableDramRate =  std::max(Dr + WriteDram_Rate_Cycle[port][priority][qIndex],    EPS);

    const double predictedDramExcessBytes = std::max(0.0, lambda - availableDramRate) *    newT / 8.0;

    const double U_dram_1 =  1.0 / std::max(1.0, predictedDramExcessBytes / BYTES_PER_MB);

    const double U_dram_2 = -lambda / availableDramRate;
    const double U_dram = U_dram_1 + utilityEta * U_dram_2;

    drop_DRAM_last[port][priority][qIndex] =  predictedDramExcessBytes / BYTES_PER_MB;

    drop_flag[port][priority][qIndex] =   (U_sram_1 < 1.0 - 1e-9 && U_dram_1 < 1.0 - 1e-9)    ? -1    : 1;


    // 选择下一周期方向
    const bool candidateStoreSram = U_sram >= U_dram;  //看这个U的比较结果

    // 队列已经混合时，不允许立即改变当前方向。
    const bool nextStoreSram =  isMixed ? currentStoreSram : candidateStoreSram;
    if (print_flag == 1)
    {
        if (nextStoreSram)
        {
            cout << "决策结果:存片内 " << endl;
        }
        else
        {
            cout << "决策结果:存片外 " << endl;
        }
    }
    // 最终记录值必须对应最终方向，而不是混合约束前的候选方向。
    const double C_S_OUT = nextStoreSram ? sramOutIfSram : sramOutIfDram;

    const double delta_Q_i_S =  nextStoreSram ? deltaQIfSram : deltaQIfDram;

    utility[port][priority][qIndex] =  nextStoreSram ? U_sram : U_dram;

    cs_out_array[port][priority][qIndex] = C_S_OUT;
    delta_Q_array[port][priority][qIndex] = delta_Q_i_S;
    decision[port][priority][qIndex] = nextStoreSram ? 1 : 0;
    YRF_Flag_result[port][priority][qIndex] = nextStoreSram;

    if (print_flag == 1)
    {
        cout << Simulator::Now().GetNanoSeconds()
             << " StoringDecision: "
             << " U_sram: " << U_sram
             << " U_dram: " << U_dram
             << " U_s1: " << U_sram_1
             << " U_s2: " << U_sram_2
             << " U_d1: " << U_dram_1
             << " U_d2: " << U_dram_2
             << " eta: " << utilityEta
             << " delta_Q_i_S: " << delta_Q_i_S
             << " ReadDram_Rate_Cycle[port][priority][qIndex]-"
             << "WriteDram_Rate_Cycle[port][priority][qIndex] "
             << ReadDram_Rate_Cycle[port][priority][qIndex]
             << " - "
             << WriteDram_Rate_Cycle[port][priority][qIndex]
             << endl;

        cout << "PBSDecision"
             << " mixed:" << isMixed
             << " current:"
             << (currentStoreSram ? "SRAM" : "DRAM")
             << " candidate:"
             << (candidateStoreSram ? "SRAM" : "DRAM")
             << " final:"
             << (nextStoreSram ? "SRAM" : "DRAM")
             << " outs:" << C_S_OUT
             << " deltaQiS:" << delta_Q_i_S
             << endl;
    }

    //  严格按照最终方向准入当前数据包  不允许方向为 SRAM 时回退到 DRAM，也不允许反向回退。
    int finalDecision = 2; // 0:DRAM，1:SRAM，2:DROP
    if (nextStoreSram)
    {
        if (Pqs + static_cast<double>(pktSize) > dtThresholdBytes)
        {
            bmResult = BmResult(DROP);
        }
        else if (m_onChipBufferRemain < pktSize)
        {
            bmResult = BmResult(DROP);
        }
        else
        {
            bmResult = BmResult(TO_ONCHIPBUFFER);
            finalDecision = 1;
        }
    }
    else
    {
        if (hasWcacheSpace && hasDramSpace)
        {
            bmResult = BmResult(TO_OFFCHIPBUFFER);
            finalDecision = 0;
        }
        else
        {
            bmResult = BmResult(DROP);
        }
    }

    if (bmResult == BmResult(TO_ONCHIPBUFFER))
    {
        Ctag[packet->GetUid()] =
            std::make_pair(1, static_cast<size_t>(pktSize));
    }
    else if (bmResult == BmResult(TO_OFFCHIPBUFFER))
    {
        Ctag[packet->GetUid()] =
            std::make_pair(0, static_cast<size_t>(pktSize));
    }
    else
    {
        Ctag.erase(packet->GetUid());
    }
    printPacketAdmissionResult(nextStoreSram);
    if (print_flag == 1)
    {
        cout << Simulator::Now().GetNanoSeconds()
             << " middle_value_for_plot: "
             << " port: " << port
             << " CurrentCycle(T'th): "
             << T_seq[port][priority][qIndex]
             << " newT[T+1](ns): " << newT
             << " newU[T+1] Usram: " << U_sram
             << " Udram: " << U_dram
             << " lambda: " << lambda
             << "  miu(Gbps): " << Cqs
             << "  Sr(MB): " << Sr * 1e-6
             << "  Dr(Gbps): " << Dr
             << "  U_star[T]: " << U_star
             << "  U[T]: " << lastUtility
             << " Storing_decision(0片外-1片内-2丢包): "
             << decision[port][priority][qIndex]
             << " final_dicision " << finalDecision
             << " delta_Q: " << lambda * newT
             << " U_s1: " << U_sram_1
             << " U_s2: " << U_sram_2
             << " U_d1: " << U_dram_1
             << " U_d2: " << U_dram_2
             << " MD: " << MD1
             << endl;

        cout << "最终决策结果:0片外 1片内 2丢包：   "
             << bmResult
             << endl;
    }

    if (port == 0)
    {
        std::string filename1;
        std::stringstream filepathstream1;

        filepathstream1
            << baseFilePath
            + now_algorithm_name
            + "/"
            + nextFilePath
            << "cost_etc_test_port0.csv";

        filename1 = filepathstream1.str();

        ofstream fout1(filename1, ios::app);
        fout1 << simulation_end.GetNanoSeconds()
              << ","
              << Simulator::Now().GetNanoSeconds()
              << ","
              << U_sram
              << ","
              << U_dram
              << ","
              << newT
              << endl;
        fout1.close();
    }

    if (port == 1)
    {
        std::string filename2;
        std::stringstream filepathstream2;

        filepathstream2
            << baseFilePath
            + now_algorithm_name
            + "/"
            + nextFilePath
            << "cost_etc_test_port1.csv";

        filename2 = filepathstream2.str();

        ofstream fout2(filename2, ios::app);
        fout2 << simulation_end.GetNanoSeconds()
              << ","
              << Simulator::Now().GetNanoSeconds()
              << ","
              << U_sram
              << ","
              << U_dram
              << ","
              << newT
              << endl;
        fout2.close();
    }

    simulation_end = Simulator::Now();

    if (print_flag == 1)
    {
        cout << "PBSPeriodEnd"
             << " time:" << Simulator::Now().GetNanoSeconds()
             << " port:" << port
             << " priority:" << priority
             << " qIndex:" << qIndex
             << " oldT:" << math_mETC
             << " actualPeriod:" << actualPeriodNs
             << " newT:" << newT
             << " arrivalRate:" << actualArrivalRate
             << " ewmaRate:" << ewmaRate
             << " mu:" << mu
             << " QiS:" << QiS
             << " QiD:" << QiD
             << " mixed:" << isMixed
             << " previousDirection:"
             << (previousStoreSram ? "SRAM" : "DRAM")
             << " candidateDirection:"
             << (candidateStoreSram ? "SRAM" : "DRAM")
             << " nextDirection:"
             << (nextStoreSram ? "SRAM" : "DRAM")
             << " actualDeltaQiS:" << actualDeltaQiS
             << " sramOutIfSram:" << sramOutIfSram
             << " sramOutIfDram:" << sramOutIfDram
             << " deltaQIfSram:" << deltaQIfSram
             << " deltaQIfDram:" << deltaQIfDram
             << " U1Ss:" << U1Ss
             << " U2Ss:" << U2Ss
             << " U1Ds:" << U1Ds
             << " U2Ds:" << U2Ds
             << " Ustar:" << U_star
             << " lastUtility:" << lastUtility
             << " deltaU:" << deltaU
             << " MD:" << MD1
             << " Usram:" << U_sram
             << " Udram:" << U_dram
             << " C_S_OUT:" << C_S_OUT
             << " delta_Q_i_S:" << delta_Q_i_S
             << " finalDecision:" << finalDecision
             << endl;
    }

    //保存当前周期结束状态，供下一周期计算 U*
    Sr_last[port][priority][qIndex] = Sr;
    Dr_last[port][priority][qIndex] = Dr;
    Pqs_last[port][priority][qIndex] = Pqs;

    ReadSram_Size_Cycle[port][priority][qIndex] = 0;
    WriteDram_Size_Cycle[port][priority][qIndex] = 0;
    ReadDram_Size_Cycle[port][priority][qIndex] = 0;

    Packet_Num_Cycle[port][priority][qIndex] = 0;
    Packet_Size_Cycle[port][priority][qIndex] = 0;

    simulation_start[port][priority][qIndex] = Simulator::Now();
    T_seq[port][priority][qIndex] += 1;
    drop_real_per_period[port][priority][qIndex] = 0;

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
    case (YRF):
        result = CheckYRFBmAlgorithm(packet);
        // std::cout<<"YRF存储位置： "<<result<<endl;
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

    // cout<<"当前时间"<<Simulator::Now().GetMicroSeconds()<<endl;

    double timermicro = (Simulator::Now() - Timer_Mill_Loss).GetMicroSeconds();
    if (timermicro >= 1.0)
    {
        // cout<<"m_dequePktCnt"<<m_dequePktCnt<<endl;
        // cout<<"丢包数: "<<m_stats.nTotalBmDropPackets<<endl;
        // cout<<"总到达数: "<<m_stats.nTotalStoredPackets+m_stats.nTotalBmDropPackets<<endl;
       
        double LossPacketNumTotal =
            static_cast<double>(m_stats.nTotalBmDropPackets);           // 累积丢包数量
        double LossPacketNum = LossPacketNumTotal - LossPacketNum_Last; // 本轮丢包数量
        double LossPacketNumTotalSize =
            static_cast<double>(m_stats.nTotalBmDropPacketsSize); // 累积丢包总大小
        double LossPacketSize =
            LossPacketNumTotalSize - LossPacketNumTotalSizeLast; // 1us内的丢包量大小（B）
        double LossPacketRate;
        if (LossPacketNumTotal != 0)
        {
            LossPacketRate = (static_cast<double>(m_stats.nTotalBmDropPackets) /
                              (m_stats.nTotalStoredPackets + m_stats.nTotalBmDropPackets + 1.0)) *
                             100.0;
        }
        else
        {
            LossPacketRate = 0;
        }
        // 获取和初始化阶段完全一致的文件路径
        const std::string fileName = GetLossPacketFilePath();
        std::ofstream fout(fileName, std::ios::out | std::ios::app);
        if (!fout.is_open())
        {
            NS_FATAL_ERROR(
                "写入 loss_packet.csv 时打开文件失败。"
                << " fileName=" << fileName
                << " baseFilePath=" << baseFilePath
                << " now_algorithm_name=" << now_algorithm_name
                << " nextFilePath=" << nextFilePath);
        }
        const double periodPacketNumber = static_cast<double>(m_stats.perusStoredPackets) +LossPacketNum;
        double periodLossRate = 0.0;
        if (periodPacketNumber > 0.0)
        {
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

SwitchMmu::BmResult
SwitchMmu::CheckYRFBmAlgorithm(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    // NS_LOG_UNCOND("运行到SwitchMmu中检查算法这里");
    uint32_t port = packet->GetMmuUsedPort(); // 得到的是目的地端口
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    uint64_t qlen = m_qUsed[port][priority][qIndex];
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();
    uint64_t dramUsed = m_offChipBuffer->GetDramUsed();
    uint64_t dramSize = m_offChipBuffer->GetDramSize();

    double optimal_etc; // 记录最小值
    Flow type = down;
    BmResult bmResult;
    PortType portType = m_portRates[port];
    // cout<<"Packet_Num_Cycle["<<port<<"]["<<priority<<"]["<<qIndex<<"]：
    // "<<Packet_Num_Cycle[port][priority][qIndex]<<endl;

    Packet_Size_Cycle[port][priority][qIndex] += pktSize;
    Packet_Num_Cycle[port][priority][qIndex] += 1;

    m_LastCycleTimeLength = m_CycleTimeLength;
    double math_mETC = m_Cost_ETC[port][priority][qIndex].GetNanoSeconds();

    double cycle_time =
        (Simulator::Now() - simulation_start[port][priority][qIndex]).GetNanoSeconds();
    if (math_mETC > 0 && YRF_Flag_First == true)
    {
        m_LastCycleTimeLength = m_CycleTimeLength;
        bmResult = BmResult(TO_ONCHIPBUFFER);
        YRF_Flag_result[port][priority][qIndex] = true;
        YRF_Flag_First = false;
        simulation_start[port][priority][qIndex] = Simulator::Now();
        return bmResult;
    }
    else if (math_mETC > cycle_time && YRF_Flag_First == false)
    {
        bmResult = BmResult(DROP);
        if (YRF_Flag_result[port][priority][qIndex] == true)
        {
            if (m_onChipBufferRemain >= (pktSize))
            {
                bmResult = BmResult(TO_ONCHIPBUFFER);
            }
        }
        else
        {
            cout << "pktSize： " << pktSize << endl;
            cout << "wcacheSize: " << wcacheSize << "wcacheUsed: " << wcacheUsed << endl;
            if ((wcacheSize - wcacheUsed) > pktSize && dramRemain > pktSize)
            {
                bmResult = BmResult(TO_OFFCHIPBUFFER);
            }
        }
        return bmResult;
    }
    else
    {
        if (qlen == 0 && m_onChipBufferRemain >= (pktSize))
        {
            cout << "第0种情况 " << endl;
            m_Cost_ETC[port][priority][qIndex] = NanoSeconds(0);
            YRF_Flag_result[port][priority][qIndex] = true;
            bmResult = BmResult(TO_ONCHIPBUFFER);
            return bmResult;
        }
        else
        {
            // cout<<"流["<<port<<"]["<<priority<<"]["<<qIndex<<"]的周期结束**"<<endl;
            // cout<<"Packet_Size_Cycle["<<port<<"]["<<priority<<"]["<<qIndex<<"]：
            // "<<Packet_Size_Cycle[port][priority][qIndex]*8<<endl;
            // cout<<"上周期流["<<port<<"]["<<priority<<"]["<<qIndex<<"]
            // 的周期长度"<<(Simulator::Now()-simulation_start[port][priority][qIndex]).GetNanoSeconds()/1000000000.0<<endl;

            double timermicro =
                (Simulator::Now() - Timer_Mill[port][priority][qIndex]).GetMicroSeconds();
            if (timermicro >= 1.0)
            {
                if (Predict_Flag_First[port][priority][qIndex] == true)
                {
                    EWMA_R[port][priority][qIndex] =
                        Packet_Size_Cycle[port][priority][qIndex] * 8.0 / timermicro;
                    Predict_Flag_First[port][priority][qIndex] = false;
                }
                else
                {
                    EWMA_R[port][priority][qIndex] =
                        W * EWMA_R[port][priority][qIndex] +
                        (1 - W) * Packet_Size_Cycle[port][priority][qIndex] * 8.0 / timermicro;
                }

                Packet_Size_Cycle[port][priority][qIndex] = 0;

                /* code */
                ReadSram_Rate_Cycle[port][priority][qIndex] =
                    ReadSram_Size_Cycle[port][priority][qIndex] / timermicro;
                WriteDram_Rate_Cycle[port][priority][qIndex] =
                    WriteDram_Size_Cycle[port][priority][qIndex] / timermicro;
                ReadDram_Rate_Cycle[port][priority][qIndex] =
                    ReadDram_Size_Cycle[port][priority][qIndex] / timermicro;
                ReadSram_Size_Cycle[port][priority][qIndex] = 0;
                WriteDram_Size_Cycle[port][priority][qIndex] = 0;
                ReadDram_Size_Cycle[port][priority][qIndex] = 0;
                Timer_Mill[port][priority][qIndex] = Simulator::Now();
            }

            simulation_start[port][priority][qIndex] = Simulator::Now();

            Packet_Num_Cycle[port][priority][qIndex] = 0;
            QueueStatus cgStatus = GetQueueStatus(port, qIndex, priority);

            Sq = (m_onChipBufferSize - m_onChipBufferRemain) * 8.0;
            Wq = wcacheUsed * 8.0;
            Dq = dramUsed * 8.0;
            Cqs = ReadSram_Rate_Cycle[port][priority][qIndex] * 1000000 * 8.0;
            // Cdw=WriteDram_Rate_Cycle[port][priority][qIndex]*1000000*8.0;
            Cqd = ReadDram_Rate_Cycle[port][priority][qIndex] * 1000000 * 8.0;
            Cq = Cqs + Cqd; // 优先级队列被端口读取的速率，若始终读一条队列

            double Pqs = UsedSram_Size_Cycle[port][priority][qIndex] * 8.0;
            double Pqd = (qlen - UsedSram_Size_Cycle[port][priority][qIndex]) * 8.0;

            double Pri_alpha = 1.0 / m_activeQueNum[port]; // 队列阈值系数1/m_activeQueNum[port]

            double ewma_r = EWMA_R[port][priority][qIndex] * 1000000;

            double Pths = (Pri_alpha * m_onChipBufferRemain) * 8.0;
            double Pthd = (Pri_alpha * dramRemain) * 8.0;
            // int64_t Pth=(qlen+Pri_alpha*(m_onChipBufferRemain+dramRemain))*8.0;
            // int64_t S=m_onChipBufferSize*8.0;
            // int64_t D=dramSize*8.0;

            cout << "ewma_r:" << ewma_r << endl;
            cout << "Cqs:" << Cqs << endl;
            cout << "Cqd:" << Cqd << endl;
            cout << "Cq:" << Cq << endl;
            cout << "Pths:" << Pths << endl;
            cout << "Pthd:" << Pthd << endl;
            cout << "m_activeQueNum[" << port << "]:" << m_activeQueNum[port] << endl;
            cout << "Simulator::Now():" << Simulator::Now().GetSeconds() << endl;

            cout << "**************** " << endl;
            cout << "假设存SRAM: " << endl;

            if (ewma_r < Cqs)
            {
                ETC_S0 = Pqs / (Cqs - ewma_r);
                Cost_S0 = (Pths - (ewma_r - Cq) * ETC_S0) / Pths + W1 * W2 * ewma_r * ETC_S0;
                Cost_min_S = Cost_S0;
                ETC_S = ETC_S0;
                cout << "ETC_S0: " << ETC_S2 << " Cost_S0: " << Cost_S2 << endl;
            }
            else
            {
                ETC_S2 = Pths / (ewma_r - Cqs);
                Cost_S2 = (Pths - (ewma_r - Cqs) * ETC_S2) / Pths + W1 * W2 * ewma_r * ETC_S2;
                Cost_min_S = Cost_S2;
                ETC_S = ETC_S2;
                cout << "ETC_S2: " << ETC_S2 << " Cost_S2: " << Cost_S2 << endl;
            }

            cout << "假设存DRAM: " << endl;
            if (ewma_r < Cqd)
            {
                ETC_D0 = Pqd / (Cqd - ewma_r);
                Cost_D0 = (Pthd - (ewma_r - Cqd) * ETC_D0) / Pthd + W1 * W3 * ewma_r * ETC_D0;
                Cost_min_D = Cost_D0;
                ETC_D = ETC_D0;
            }
            else
            {
                ETC_D0 = Pths / (ewma_r - Cqd);
                Cost_D0 = (Pthd - (ewma_r - Cqd) * ETC_D0) / Pthd + W1 * W3 * ewma_r * ETC_D0;

                ETC_D1 = Pthd / (ewma_r - Cqd);
                Cost_D1 = (Pthd - (ewma_r - Cqd) * ETC_D1) / Pthd + W1 * W3 * ewma_r * ETC_D1;

                if (Cost_D0 < Cost_D1)
                {
                    Cost_min_D = Cost_D0;
                    ETC_D = ETC_D0;
                    cout << "ETC_D0: " << ETC_D0 << " Cost_D0: " << Cost_D0 << endl;
                }
                else
                {
                    Cost_min_D = Cost_D1;
                    ETC_D = ETC_D1;
                    cout << "ETC_D1: " << ETC_D1 << " Cost_D1: " << Cost_D1 << endl;
                }
            }

            cout << "计算结果: " << endl;
            cout << "ETC_S: " << ETC_S << " Cost_min_S: " << Cost_min_S << endl;
            cout << "ETC_D: " << ETC_D << " Cost_min_D: " << Cost_min_D << endl;

            // 判断SRAM和DRAM时谁的Cost最小
            if (Cost_min_S <= Cost_min_D)
            {
                // 存SRAM,周期为ETC_S
                m_Cost_ETC[port][priority][qIndex] = NanoSeconds(ETC_S); //*1000000000);//1000000
                YRF_Flag_result[port][priority][qIndex] = true;
                if (m_onChipBufferRemain > (pktSize))
                {
                    bmResult = BmResult(TO_ONCHIPBUFFER);
                }
                // else{
                //     bmResult = BmResult(DROP);
                // }
                else if ((wcacheSize - wcacheUsed) > pktSize && dramRemain > pktSize)
                {
                    bmResult = BmResult(TO_OFFCHIPBUFFER);
                }
                optimal_etc = ETC_S;

                cout << "决策结果:存片内 " << endl;
            }
            else
            {
                m_Cost_ETC[port][priority][qIndex] = NanoSeconds(ETC_D); //*1000000000);//1000000
                YRF_Flag_result[port][priority][qIndex] = false;
                if ((wcacheSize - wcacheUsed) > pktSize && dramRemain > pktSize)
                {
                    bmResult = BmResult(TO_OFFCHIPBUFFER);
                }
                else
                {
                    bmResult = BmResult(DROP);
                }
                cout << "决策结果:存片外 " << endl;
                optimal_etc = ETC_D;
            }
        } // 不是优质流
    }
    // cout<<"F["<<port<<"]["<<priority<<"]["<<qIndex<<"]
    // ETC:"<<m_Cost_ETC[port][priority][qIndex]<<endl;
    if (port == 0)
    {
        // 同时输出到文件中便于观察数据***********************
        std::string filename1;
        // 使用字符串流动态构建文件路径
        std::stringstream filepathstream1;
        filepathstream1 << baseFilePath + now_algorithm_name + "/" + nextFilePath
                        << "cost_etc_test_port0.csv";
        filename1 = filepathstream1.str();
        ofstream fout1(filename1, ios::app);
        fout1 << simulation_end.GetSeconds() << "," << Simulator::Now().GetSeconds() << ","
              << Cost_min_S / 1000000000.0 << "," << Cost_min_D / 1000000000.0 << ","
              << optimal_etc * 1000 << endl;
        fout1.close();
    }

    if (port == 1)
    {
        // 同时输出到文件中便于观察数据***********************
        std::string filename2;
        // 使用字符串流动态构建文件路径
        std::stringstream filepathstream2;
        filepathstream2 << baseFilePath + now_algorithm_name + "/" + nextFilePath
                        << "cost_etc_test_port1.csv";
        filename2 = filepathstream2.str();
        ofstream fout2(filename2, ios::app);
        fout2 << simulation_end.GetSeconds() << "," << Simulator::Now().GetSeconds() << ","
              << Cost_min_S / 1000000000.0 << "," << Cost_min_D / 1000000000.0 << ","
              << optimal_etc * 1000 << endl;
        fout2.close();
    }

    simulation_end = Simulator::Now();

    return bmResult;
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
        m_stats
            .nTotalOnChipBufferStoredPackets++; // 更新相关统计信息，包括OnChip缓冲区存储数据包数量、剩余空间、优先级使用量等
        packet->SetLocation(
            Packet::ONCHIPBUFFER); // 将数据包位置标记为OnChipBuffer，并更新OnChip缓冲区剩余空间
        UsedSram_Size_Cycle[port][priority][qIndex] += psize;
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

        // 从Ctag中删除这个包
        if (Ctag.count(packet->GetUid()))
        {
            Ctag.erase(packet->GetUid());
        }

        // Trace.
        m_traceSramReadComplete(packet);
    }
    else if (packet->GetLocation() == Packet::WRITINGTOOFFCHIPBUFFER ||
             packet->GetLocation() == Packet::OFFCHIPBUFFER ||
             packet->GetLocation() == Packet::WCACHE)
    {
        ReadDram_Size_Total += psize;
        // 从Ctag中删除这个包
        if (Ctag.count(packet->GetUid()))
        {
            Ctag.erase(packet->GetUid());
        }
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
