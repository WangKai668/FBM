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

#include "off-chip-buffer.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-reorder-net-device.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/nstime.h"

using namespace std;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SwitchMmu");//NS_LOG_COMPONENT_DEFINE 宏用于定义一个日志组件，其中参数 "SwitchMmu" 是该日志组件的名称，控制日志输出级别和过滤日志消息

NS_OBJECT_ENSURE_REGISTERED(SwitchMmu);//一个宏，用于确保特定类在运行时被注册了

SwitchMmu::Stats::Stats() //SwitchMmu的内部类Stats的构造函数的实现，在构造函数中，对Stats类的成员变量
    : nTotalStoredPackets(0), //nTotalStoredPackets、nTotalBmDropPackets和nTotalOnChipBufferStoredPackets进行了初始化。
      nTotalBmDropPackets(0),
      nTotalBmDropPacketsSize(0),
      nTotalOnChipBufferStoredPackets(0)
{
}

TypeId //TypeId是一个类型，表示某个类型的标识符。
SwitchMmu::GetTypeId() //段代码是C++中类SwitchMmu的成员函数GetTypeId的实现
{
    static TypeId tid = //static关键字使得tid成为静态局部变量，即在函数调用结束后，tid的值仍然保留，直到程序结束
        TypeId("ns3::SwitchMmu") //TypeId("ns3::SwitchMmu")创建了一个TypeId对象，表示类型为ns3::SwitchMmu
            .SetParent<Object>() //设置SwitchMmu类的父类为Object
            .SetGroupName("Network") //设置SwitchMmu类所属的组名为"Network"
            .AddConstructor<SwitchMmu>()//添加SwitchMmu类的构造函数
            .AddAttribute( //添加一个属性，包括属性名称、属性描述、属性值、属性访问器和属性检查器。具体参数解释如下：
                "SwitchMemType", //属性名称为"SwitchMemType"
                "The Switch memory type(only SRAM or hybrid buffer)",
                EnumValue(SwitchMmu::BufferModel(ONOFFCHIP)), //属性值，使用枚举值ONOFFCHIP初始化BufferModel
                MakeEnumAccessor(&SwitchMmu::SetMemType, &SwitchMmu::GetMemType), //属性访问器，用于设置和获取属性值
                MakeEnumChecker(SwitchMmu::ONCHIP, "OnChip", SwitchMmu::ONOFFCHIP, "OnOffChip")) //属性检查器，用于检查属性值的有效性，接受两对参数，每对参数包括一个枚举值和对应的描述
            .AddAttribute("OnChipBufferSize", //属性名
                          "Onchip buffer size",
                          // I don't why, but without static_cast, there will be linking error
                          UintegerValue(static_cast<uint64_t>(DEFAULT_ONCHIPBUFFER_SIZE)), //使用DEFAULT_ONCHIPBUFFER_SIZE的值初始化为64位无符号整数类型
                          MakeUintegerAccessor(&SwitchMmu::SetOnChipBufferSize, //属性访问器，用于设置和获取属性值
                                               &SwitchMmu::GetOnChipBufferSize),
                          MakeUintegerChecker<uint64_t>()) //属性检查器，用于检查属性值的有效性，这里指定了属性值为64位无符号整数类型
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
            .AddAttribute("PortNumber",
                          "The number of ports",
                          UintegerValue(64), //表示属性的默认值为64，即默认端口数量为64。
                          MakeUintegerAccessor(&SwitchMmu::SetNPorts, &SwitchMmu::GetNPorts),
                          MakeUintegerChecker<uint32_t>()) //建了一个检查器，用于检查属性值的类型是否为uint32_t，即无符号32位整数类型
            .AddAttribute(
                "PriorityNumber",
                "The number of priorities",
                UintegerValue(2), //表示属性的默认值为2，即默认优先级数量为2
                MakeUintegerAccessor(&SwitchMmu::SetNPriorities, &SwitchMmu::GetNPriorities),
                MakeUintegerChecker<uint32_t>()) //创建了一个检查器，用于检查属性值的类型是否为uint32_t，即无符号32位整数类型
            .AddAttribute("QueueNumber",
                          "The number of queues",
                          UintegerValue(8), //表示属性的默认值为8，即默认队列数量为8
                          MakeUintegerAccessor(&SwitchMmu::SetNQueues, &SwitchMmu::GetNQueues),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Simlulator_time_stop",
                          "The number of queues",
                          DoubleValue(1.0), //表示属性的默认值为8，即默认队列数量为8
                          MakeDoubleAccessor(&SwitchMmu::Simlulator_time_stop),
                          MakeDoubleChecker<double>())
            .AddAttribute("BMAlgorithm",
                          "Buffer management algorithm",
                          EnumValue(SwitchMmu::BmAlgorithm(YRF)), 
                          MakeEnumAccessor(&SwitchMmu::SetBmAlgorithm, &SwitchMmu::GetBmAlgorithm),
                          MakeEnumChecker(SwitchMmu::HW, "HW", SwitchMmu::YSL, "YSL",
                                          SwitchMmu::TDT, "TDT", SwitchMmu::BASELINE, "BASELINE", SwitchMmu::YRF, "YRF"))
            .AddAttribute("LruUpdateTimeWindow", //是属性的名称，表示LRU更新状态之间的时间
                          "The Time between LRU Update Status",
                          TimeValue(MicroSeconds(100)), //属性的默认值为100微秒，即LRU更新状态之间的时间默认为100微秒
                          MakeTimeAccessor(&SwitchMmu::m_updateLruTimeWindow),
                          MakeTimeChecker())
            .AddAttribute("CycleTimeLength",
                      "The Time length of cycle",
                      TimeValue(MicroSeconds(1)),
                      MakeTimeAccessor(&SwitchMmu::m_CycleTimeLength),
                      MakeTimeChecker())
            .AddTraceSource("Store", //"Store"是追踪源的名称，表示一个数据包已经被存储
                            "A packet has been stored",
                            MakeTraceSourceAccessor(&SwitchMmu::m_traceStore), //创建了一个追踪源访问器，其中SwitchMmu::m_traceStore是用于存储追踪信息的成员变量。这样可以通过访问器来添加、存储和获取追踪信息
                            "ns3::Packet::TracedCallback") //是追踪源的类型，表示追踪的信息类型为ns3::Packet的追踪回调函数
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
                            "ns3::Packet::TracedCallBack")
        ;

    return tid;
}

//这段代码是一个类中的成员函数，用于返回该类的实例类型ID
TypeId //是一个类型，通常用于标识类的类型
SwitchMmu::GetInstanceTypeId() const //是一个成员函数，用于返回SwitchMmu类的实例类型ID
{
    return GetTypeId(); //GetTypeId()是一个函数，通常是从父类中继承而来，用于获取当前类的类型ID
}

SwitchMmu::SwitchMmu() //构造函数SwitchMmu::SwitchMmu()被定义,来完成对象的初始化工作
{
    NS_LOG_FUNCTION(this);

    m_nPackets = 0;
    m_offChipBuffer = nullptr;

    // Set the default value of HW BM
    m_enableOnChipPdp = true;
    m_cgMax = {{1500 * 1024, 1200 * 1024},      //m_cgMax[type][pri]
               {1700 * 1024, 1400 * 1024},
               {2300 * 1024, 1700 * 1024}};
    m_cgMin = {{1100 * 1024, 700 * 1024},       //m_cgMin[type][pri]
               {1200 * 1024, 800 * 1024},
               {1800 * 1024, 1100 * 1024}};
    m_longCgQlen = {{130 * 1024, 80 * 1024},  //m_longCgQlen[portType][priority]
                    {150 * 1024, 90 * 1024},
                    {210 * 1024, 120 * 1024}};

    m_wcacheFullTh = {350 * 1024, 200 * 1024};//m_wcacheFullTh[priority]
    m_wcacheCgTh = {200 * 1024, 150 * 1024};  //m_wcacheCgTh[priority]
    m_alphaOfPort = {0.5, 2, 4};              //m_alphaOfPort[portType]
    m_alphaOfPriority = {18, 2};              //m_alphaOfPriority[priority]

    m_alphaOfQueue = {                        //m_alphaOfQueue[portType][type][priority][qIndex]
        {{{8, 6, 5}, {9.0/16, 9.0/16, 9.0/16, 9.0/16, 17.0/32}},//100Gbps port,上行数据流
         {{10, 9, 8.5}, {5.0/8, 5.0/8, 5.0/8, 5.0/8, 9.0/16}}}, //100Gbps port,下行数据流
        {{{32, 16, 12}, {3.0/4, 3.0/4, 3.0/4, 3.0/4, 5.0/8}},   //400Gbps port,上行数据流
         {{10, 9, 8.5}, {5.0/8, 5.0/8, 5.0/8, 5.0/8, 9.0/16}}}, //100Gbps port,下行数据流
        {{{64, 32, 16}, {1.0, 1.0, 1.0, 1.0, 3.0/4}},           //800Gbps port,上行数据流
         {{64, 32, 16}, {1.0, 1.0, 1.0, 1.0, 3.0/4}}}           //100Gbps port,下行数据流
    };
    //m_wredTh = {13100000, 13100000};//加权随机早期丢弃（Weighted Random Early Detection，WRED）是一种流量管理技术，用于避免网络拥塞,m_wredTh[priority]
    m_wredTh = {2000 * 1024, 2000 * 1024};//加权随机早期丢弃（Weighted Random Early Detection，WRED）是一种流量管理技术，用于避免网络拥塞,m_wredTh[priority]
    YRF_Flag_First=true;//第一个周期开始时为true
    
    //Predict_Flag_First=true;
    //YRF_Flag_result=true;//第一个周期开始时为true,表示初始化时周期存储片上,false时存储片外

    W=0.9;
    W1=0.45;
    W2=1;
    W3=2;
    m_dramAlphaOfPriority = {18, 2};          //3DT算法
    m_wcacheAlphaOfPriority = {18, 2};        //3DT算法
    m_dramAlphaOfQueue = {{{12, 11, 10}, {3.0 / 4, 3.0 / 4, 3.0 / 4, 3.0 / 4, 5.0 / 8}}, //3DT算法
                          {{10, 8, 7}, {5.0 / 8, 5.0 / 8, 5.0 / 8, 5.0 / 8, 9.0 / 16}}};
    m_wcacheAlphaOfQueue = {{{12, 11, 10}, {3.0 / 4, 3.0 / 4, 3.0 / 4, 3.0 / 4, 5.0 / 8}}, //3DT算法
                            {{10, 8, 7}, {5.0 / 8, 5.0 / 8, 5.0 / 8, 5.0 / 8, 9.0 / 16}}};

    //同时输出到文件中便于观察数据
    std::string baseFilePath = "/home/dell6/yrf/pba/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/";
    std::string fileName;
    // 使用字符串流动态构建文件路径
    std::stringstream filePathStream;
    filePathStream << baseFilePath << "loss_packet.csv";
    fileName = filePathStream.str();
    ofstream fout(fileName, ios::app);
    fout <<"TimeStart,TimeEnd,LossPacketNum,LossPacketRate"<<endl;
    fout.close();


    //同时输出到文件中便于观察数据***********************
    std::string filepath = "/home/dell6/yrf/pba/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/";
    std::string filename1;
    // 使用字符串流动态构建文件路径
    std::stringstream filepathstream;
    filepathstream << filepath << "cost_etc_test.csv";
    filename1 = filepathstream.str();
    ofstream fout1(filename1, ios::app);
    fout1 <<"TimeStart,TimeEnd,Cost_min_S,ETC_S,Cost_min_S,ETC_D"<<endl;
    fout1.close();

    LossPacketNum_Last=0;
    LossPacketNumTotalSizeLast=0;
    Timer_Mill_Loss=Seconds(0);
}

//析构函数中，通常会执行一些清理工作，比如释放资源、关闭文件等操作。在这里的代码中，使用了NS_LOG_FUNCTION宏，可能是用于调试目的，用于输出函数的调用信息
//这段代码定义了一个空的析构函数~SwitchMmu()，并使用NS_LOG_FUNCTION宏输出函数调用信息
SwitchMmu::~SwitchMmu()
{
    NS_LOG_FUNCTION(this);
}
//这段代码定义了一个返回m_stats成员变量常引用的成员函数GetStats()，并在函数内部输出函数的调用信息
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
    uint32_t pDramSize = packet->GetDramStoredSize();//数据包存储在DRAM中的大小
    uint32_t psize = packet->GetSize();//数据包总大小
    uint32_t pWcacheSize = psize - pDramSize;//得到数据包存储在Wcache中的大小

    m_wcacheQlen[port][priority][qIndex] -= pWcacheSize;//计算Wcache中剩余存储大小
    m_priWcacheUsed[priority] -= pWcacheSize;//优先级量剩余大小
}

void
SwitchMmu::WriteWcacheComplete(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this);

    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t psize = packet->GetSize() + SwitchMmu::IPV4_INPUT_PKTSIZE_CORRECTION;//IPV4_INPUT_PKTSIZE_CORRECTION=22,为一个常量，为修正而存在

    m_wcacheQlen[port][priority][qIndex] += psize;//写数据包并更新Wcache存储值大小
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

    ReadDram_Size_Cycle[port][priority][qIndex] += dramsize;//统计周期内从DRAM读出的数据包大小

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
    WriteDram_Size_Cycle[port][priority][qIndex] += dramsize;//统计周期内写入DRAM的数据包大小

    m_dramQlen[port][priority][qIndex] += dramsize;//从Wcache中读取数据包并更新DRAM存储值大小
    m_priDramUsed[priority] += dramsize;
    m_wcacheQlen[port][priority][qIndex] -= dramsize;//Wcache中的数据包被写入DRAM并更新Wcache存储值大小
    m_priWcacheUsed[priority] -= dramsize;
}
//这段代码是一个名为SwitchMmu的类中的一个成员函数SetNode的实现
//该函数接受一个类型为Ptr<Node>的参数node，并将其设置为类中的成员变量m_node。
void
SwitchMmu::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

//该函数用于在对象销毁时执行必要的清理操作
void
SwitchMmu::DoDispose()
{
    NS_LOG_FUNCTION(this);//调用了NS_LOG_FUNCTION宏，该宏通常用于记录函数的调用信息。在这里，它记录了当前对象的地址（this）
    m_node = nullptr;//将类中的成员变量m_node设置为nullptr，以释放对节点的引用
    Object::DoDispose();//调用了基类Object的DoDispose函数，以执行基类的清理操作
}
//该函数用于在对象初始化时执行必要的操作
void
SwitchMmu::DoInitialize()
{
    NS_LOG_FUNCTION(this);//记录了当前对象的地址（this）

    m_node = nullptr;//m_node设置为nullptr，以确保在初始化时节点指针为nullptr

    Object::DoInitialize();//调用了基类Object的DoInitialize函数，以执行基类的初始化操作
}
//该函数用于在新的聚合对象通知时执行必要的操作
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
            this->SetNode(node);//调用SetNode函数，将获取的节点指针设置为对象的节点
        }
    }
    Object::NotifyNewAggregate();//调用了基类Object的NotifyNewAggregate函数，以执行基类的相关操作
}
//该函数用于设置交换机内存类型，并在需要时初始化片外缓冲区
void
SwitchMmu::SetMemType(BufferModel type)
{
    NS_LOG_FUNCTION(this << type);//函数内部调用了NS_LOG_FUNCTION宏，记录了当前对象的地址和传入的交换机内存类型，用于跟踪函数的调用

    m_memType = type;
    /**
     * If the switch memory type is the 'Hybrid Buffer', the
     * Off Chip Buffer will be initialized.
     */
    //如果交换机内存类型为ONOFFCHIP（表示混合缓冲区），并且片外缓冲区指针m_offChipBuffer为空，则会执行以下操作：
    if (m_memType == ONOFFCHIP && m_offChipBuffer == nullptr)
    {
        Ptr<OffChipBuffer> offChipBuffer = CreateObject<OffChipBuffer>(); //调用CreateObject<OffChipBuffer>()创建一个OffChipBuffer对象
        AttachOffChipBuffer(offChipBuffer);//调用AttachOffChipBuffer(offChipBuffer)函数，将创建的片外缓冲区对象附加到交换机对象上
    }
}
//获取交换机当前的内存类型，并将其返回
SwitchMmu::BufferModel
SwitchMmu::GetMemType() const
{
    NS_LOG_FUNCTION(this);
    return m_memType;
}
//这段代码是SwitchMmu类中的另一个成员函数SetNPorts的实现。该函数用于设置交换机的端口数量，并根据端口数量初始化和调整一些计数器和数据结构
void
SwitchMmu::SetNPorts(uint32_t nPorts)
{
    NS_LOG_FUNCTION(this << nPorts);

    m_nPorts = nPorts;//交换机的端口数量

    // initialize and resize some counters
    m_activeQueNum.resize(m_nPorts, 0);
    m_priDpUsed.resize(m_nPorts);
    m_qlens.resize(m_nPorts);
    m_alpha.resize(m_nPorts);

    m_cgStatus.resize(m_nPorts);
    m_qUsed.resize(m_nPorts);
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
    ReadDram_Rate_Cycle.resize(m_nPorts);
    simulation_start.resize(m_nPorts);
    m_Cost_ETC.resize(m_nPorts);
    EWMA_R.resize(m_nPorts);
    YRF_Flag_result.resize(m_nPorts);
    Predict_Flag_First.resize(m_nPorts);
    Timer_Mill.resize(m_nPorts);
    T_seq.resize(m_nPorts);

}

uint32_t
SwitchMmu::GetNPorts() const
{
    NS_LOG_FUNCTION(this);
    return m_nPorts;
}
//该函数用于设置每个端口的队列数量，并根据队列数量初始化和调整一些计数器和数据结构。
void
SwitchMmu::SetNQueues(uint32_t nQueues)
{
    NS_LOG_FUNCTION(this << nQueues);

    m_nQueuesPerPort = nQueues;//函数将传入的队列数量nQueues赋值给成员变量m_nQueuesPerPort，表示每个端口的队列数量
    // initialize and resize counters.
    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        m_qlens[i].resize(m_nQueuesPerPort, 0);//对每个端口的队列长度m_qlens和队列权重m_alpha 进行初始化或调整
        // The initial value of alpha is 1.
        m_alpha[i].resize(m_nQueuesPerPort, 1);

        m_cgTimer[i].resize(m_nQueuesPerPort, 0);

        for (uint32_t j = 0; j < m_nPriorities; j++)
        {
            m_qUsed[i][j].resize(m_nQueuesPerPort, 0);
            m_qMaxUsed[i][j].resize(m_nQueuesPerPort, 0);
            m_qTotalRcvd[i][j].resize(m_nQueuesPerPort, 0);//队列总接收量
            m_wcacheQlen[i][j].resize(m_nQueuesPerPort, 0);
            m_dramQlen[i][j].resize(m_nQueuesPerPort, 0);
            m_sramQlen[i][j].resize(m_nQueuesPerPort, 0);
            m_cgStatus[i][j].resize(m_nQueuesPerPort, SwitchMmu::NOT_CONGESTION);
            Packet_Num_Cycle[i][j].resize(m_nQueuesPerPort, 0);
            Packet_Size_Cycle[i][j].resize(m_nQueuesPerPort);
            Packet_Size_Cycle_Max[i][j].resize(m_nQueuesPerPort);
            ReadSram_Size_Cycle[i][j].resize(m_nQueuesPerPort);
            UsedSram_Size_Cycle[i][j].resize(m_nQueuesPerPort);
            WriteDram_Size_Cycle[i][j].resize(m_nQueuesPerPort);
            ReadDram_Size_Cycle[i][j].resize(m_nQueuesPerPort);
            ReadSram_Rate_Cycle[i][j].resize(m_nQueuesPerPort,0);
            WriteDram_Rate_Cycle[i][j].resize(m_nQueuesPerPort,0);
            ReadDram_Rate_Cycle[i][j].resize(m_nQueuesPerPort,0);
            simulation_start[i][j].resize(m_nQueuesPerPort);
            m_Cost_ETC[i][j].resize(m_nQueuesPerPort,NanoSeconds(100));
            EWMA_R[i][j].resize(m_nQueuesPerPort,0);
            YRF_Flag_result[i][j].resize(m_nQueuesPerPort);
            Predict_Flag_First[i][j].resize(m_nQueuesPerPort,true);
            Timer_Mill[i][j].resize(m_nQueuesPerPort);
             T_seq[i][j].resize(m_nQueuesPerPort,0);
        }
    }
}

uint32_t
SwitchMmu::GetNQueues() const
{
    NS_LOG_FUNCTION(this);
    return m_nQueuesPerPort;
}
//该函数用于设置每个端口的优先级数量，并根据优先级数量初始化和调整一些计数器和数据结构。
void
SwitchMmu::SetNPriorities(uint32_t nPriorities)
{
    NS_LOG_FUNCTION(this << nPriorities);

    m_nPriorities = nPriorities;//函数将传入的优先级数量nPriorities赋值给成员变量m_nPriorities，表示每个端口的优先级数量
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
        ReadDram_Rate_Cycle[i].resize(m_nPriorities);
        simulation_start[i].resize(m_nPriorities);
        m_Cost_ETC[i].resize(m_nPriorities);
        EWMA_R[i].resize(m_nPriorities);
        YRF_Flag_result[i].resize(m_nPriorities);
        Predict_Flag_First[i].resize(m_nPriorities);
        Timer_Mill[i].resize(m_nPriorities);
        T_seq[i].resize(m_nPriorities);
    }
}

uint32_t
SwitchMmu::GetNPriorities() const
{
    NS_LOG_FUNCTION(this);
    return m_nPriorities;
}
//函数将传入的缓存管理算法类型bmtype赋值给成员变量m_bmAlgorithm，表示当前使用的缓存管理算法类型
void
SwitchMmu::SetBmAlgorithm(BmAlgorithm bmtype)
{
    NS_LOG_FUNCTION(this << bmtype);
    m_bmAlgorithm = bmtype;

    if (m_bmAlgorithm == BASELINE)//如果当前缓存管理算法类型为BASELINE
    {
        //SetOnChipBufferSize(10L << 30);//则调用SetOnChipBufferSize函数设置On-Chip缓存的大小为10GB（即左移30位，相当于10乘以2的30次方字节）
    }

    if (m_bmAlgorithm == YSL)//如果当前缓存管理算法类型为YSL
    {
        UpdateLruTimer();//调用UpdateLruTimer函数更新LRU计时器
    }
}

SwitchMmu::BmAlgorithm
SwitchMmu::GetBmAlgorithm() const
{
    NS_LOG_FUNCTION(this);
    return m_bmAlgorithm;
}
//该函数用于设置交换机的On-Chip缓存大小，并初始化On-Chip缓存的剩余空间
void
SwitchMmu::SetOnChipBufferSize(uint64_t size)
{
    NS_LOG_FUNCTION(this << size);//函数内部调用了NS_LOG_FUNCTION宏，记录了当前对象的地址和传入的缓存大小size，用于跟踪函数的调用
    m_onChipBufferSize = size;//函数将传入的缓存大小size赋值给成员变量m_onChipBufferSize，表示设置交换机的On-Chip缓存大小
    m_onChipBufferRemain = size;//函数将传入的缓存大小size也赋值给成员变量m_onChipBufferRemain，表示初始化On-Chip缓存的剩余空间为设置的缓存大小
}

uint64_t
SwitchMmu::GetOnChipBufferSize() const
{
    NS_LOG_FUNCTION(this);
    return m_onChipBufferSize;
}
//该函数用于设置交换机的重排序缓存大小，并初始化重排序缓存的剩余空间
void
SwitchMmu::SetReorderBufferSize(uint64_t size)
{
    NS_LOG_FUNCTION(this << size);//函数内部调用了NS_LOG_FUNCTION宏，记录了当前对象的地址和传入的缓存大小size
    m_reorderBufferSize = size;//函数将传入的缓存大小size赋值给成员变量m_reorderBufferSize，表示设置交换机的重排序缓存大小
    m_reorderBufferRemain = size;//函数将传入的缓存大小size也赋值给成员变量m_reorderBufferRemain，表示初始化重排序缓存的剩余空间为设置的缓存大小
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
//该函数用于更新重排序缓存的剩余空间，根据参数pktSize和inc的值来增加或减少缓存的剩余空间
void
SwitchMmu::UpdateReorderBufferRemain(uint32_t pktSize, bool inc)
{
    NS_LOG_FUNCTION(this);
    if (inc)//函数根据参数inc的值来判断是增加还是减少重排序缓存的剩余空间。如果inc为true，表示增加缓存的剩余空间；如果inc为false，表示减少缓存的剩余空间
    {   //如果需要增加缓存的剩余空间，函数会检查增加后的剩余空间是否超过了缓存的总大小，如果超过则会输出错误信息
        NS_ASSERT_MSG(m_reorderBufferRemain + pktSize <= m_reorderBufferSize,
                      "Error when increase reorder buffer remain size");
        m_reorderBufferRemain += pktSize;//然后将pktSize加到m_reorderBufferRemain中
    }
    else
    {   //如果需要减少缓存的剩余空间，函数会首先检查剩余空间是否大于等于0，如果小于0则会输出错误信息
        NS_ASSERT_MSG(m_reorderBufferRemain >= 0,
                      "Error when decrease reorder buffer remain size");
        if (m_reorderBufferRemain < pktSize)//然后根据pktSize的大小来更新剩余空间，如果剩余空间小于pktSize，则将剩余空间设为0
        {
            m_reorderBufferRemain = 0;
        }
        else
        {
            m_reorderBufferRemain -= pktSize;//否则减去pktSize
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
SwitchMmu::Show()//该函数用于显示交换机的基本信息，包括端口数量、每个端口的队列数量、优先级数量以及内存类型
{
    //NS_LOG_UNCOND("运行到Show()这里");

    NS_LOG_FUNCTION(this);

    // LOG OUT The fundamental info about the switch.
    NS_LOG_DEBUG("The Switch Port Num: " << m_nPorts);
    NS_LOG_DEBUG("The Switch Queue Num per port: " << m_nQueuesPerPort);
    NS_LOG_DEBUG("The Switch Priority Num: " << m_nPriorities);
    NS_LOG_DEBUG("The Switch Mem Type: ");
    if (m_memType == ONOFFCHIP)//如果内存类型为ONOFFCHIP，则还会调用m_offChipBuffer对象的Show函数展示更多关于离片缓存的信息
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
SwitchMmu::ShowCounters()//该函数用于显示交换机的计数器信息，包括各个优先级的使用情况、每个端口的活跃队列数量、队列的参数（Alpha和长度）以及片外缓存的计数器信息
{
    NS_LOG_FUNCTION(this);
    // DEBUG USAGE: Add to the place that you wanna to show counters values.
    NS_LOG_DEBUG("Remain size of onchip buffer: " << m_onChipBufferRemain);//输出"OnChip"缓存的剩余大小m_onChipBufferRemain

    NS_LOG_DEBUG("Priority Num:  " << m_nPriorities);
    for (uint32_t i = 0; i < m_nPriorities; i++)
    {   ////遍历每个优先级，输出该优先级在"OnChip"缓存中的使用计数器m_priOnChipUsed[i]
        NS_LOG_DEBUG("Priority " << i << " Used onchip counters: " << m_priOnChipUsed[i]);
    }
    for (uint32_t i = 0; i < 4; i++)
    {
        NS_LOG_DEBUG("The Port Index: " << i);
        NS_LOG_DEBUG("\tThe Active Queue Num of this Port: " << m_activeQueNum[i]);//遍历每个端口，输出该端口的活跃队列数量m_activeQueNum[i]
        for (uint32_t j = 0; j < m_nQueuesPerPort; j++)
        {
            NS_LOG_DEBUG("\tThe Queue Index: " << j);
            NS_LOG_DEBUG("\t\tThe Queue Alpha: " << m_alpha[i][j]);//以及每个队列的Alpha参数m_alpha[i][j]和长度m_qlens[i][j]
            NS_LOG_DEBUG("\t\tThe Queue Length: " << m_qlens[i][j]);
        }

        for (uint32_t p = 0; p < m_nPriorities; p++)//遍历每个端口和优先级，输出该端口和优先级对应的"OnChip"缓存使用计数器m_priDpUsed[i][p]
        {
            NS_LOG_DEBUG("\tThe Priority Num: " << p);
            NS_LOG_DEBUG(
                "\t\tThe Priority Used OnChip in this Destination Port: " << m_priDpUsed[i][p]);
        }
    }

    // OffChipBuffer
    m_offChipBuffer->ShowCounters();//调用m_offChipBuffer对象的ShowCounters函数，展示片外缓存的计数器信息
}

//只使用一个缓存：SRAM
SwitchMmu::BmResult
SwitchMmu::CheckBaselineBmAlgorithm(Ptr<Packet> packet)//该函数用于检查基准缓存管理算法，根据输入的数据包信息和当前交换机状态，确定输入数据包的缓存位置
{
      NS_LOG_FUNCTION(this << packet);
    BmResult bmResult;
    //uint32_t port = packet->GetMmuUsedPort();
    //uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    //uint64_t qlen = m_qUsed[port][priority][qIndex];
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();
    
    // if (m_onChipBufferRemain >= pktSize) //如果队列长度加上数据包大小不超过权重随机早期丢弃（WRED）阈值，则将数据包放入"OnChip"缓存
    // {
    //     bmResult = BmResult(TO_ONCHIPBUFFER);
    // }
    // else 
    // {
    //     bmResult = BmResult(DROP);//否则，直接丢弃数据包
    // }
    
    if ((wcacheSize-wcacheUsed)>= pktSize && dramRemain >= pktSize)
    {
        bmResult = BmResult(TO_OFFCHIPBUFFER);
    }else{
        bmResult = BmResult(DROP);//否则，直接丢弃数据包
    }
    //cout<<"jinru"<<endl;
    return bmResult;
}

SwitchMmu::BmResult
SwitchMmu::Check3DTBmAlgorithm(Ptr<Packet> packet)//西交大随便写的一个算法
{
    NS_LOG_FUNCTION(this << packet);
    BmResult bmResult;
    bmResult = BmResult(TO_ONCHIPBUFFER);
    return bmResult;
}

uint64_t
SwitchMmu::GetDynamicAlphaSramThreshold(uint32_t qIndex)//获取动态的SRAM阈值。函数的作用是为每个队列分配一个静态的10KB的SRAM阈值，并返回这个值
{
    // With this simple. we just allocate static 10KB for each Queue.
    // 10KB
    return 10UL << 10;//返回一个uint64_t类型的值，表示10KB的SRAM阈值
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
SwitchMmu::CheckHWBmAlgorithm(Ptr<Packet> packet)//该函数用于检查HW缓存管理算法，根据输入的数据包信息和当前交换机状态，确定输入数据包的缓存位置
{
    NS_LOG_FUNCTION(this << packet);
    //NS_LOG_UNCOND("运行到SwitchMmu中检查算法这里");
    
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
    //cout<<"cgStatus:"<<cgStatus<<endl;
    Flow type = down; // TODO: decide the flow type,这里的down指的是数据从服务器发送到客户端的方向，为下行数据流
    BmResult bmResult;

    PortType portType = m_portRates[port];

    NS_LOG_DEBUG("Port: " << port << "\tQueue: " << qIndex << "\tPriority: " << priority);
    NS_LOG_DEBUG("Queue Length: " << qlen << "\tWcache Used: " << wcacheUsed);
    NS_LOG_DEBUG("Onchip buffer Remain: " << m_onChipBufferRemain
                                          << "\tDRAM Remain: " << dramRemain);
    NS_LOG_DEBUG("Wcache Full Threshold: " << m_wcacheFullTh[priority]
                                           << "\tWcache Congestion Threshold: "
                                           << m_wcacheCgTh[priority]);
    NS_LOG_DEBUG("Congestion Status: " << cgStatus
                                       << "\tLong Congestion Qlen: " << m_longCgQlen[portType][priority]);
    NS_LOG_DEBUG("Enable on-chip buffer PDP: "
                 << m_enableOnChipPdp
                 << "\tOnChip Buffer Used By Priority: " << m_priOnChipUsed[priority]
                 << "\tUsed By Pri&Dp: " << m_priDpUsed[port][priority]);

    if (qlen == 0 && m_onChipBufferRemain >= pktSize)//如果队列长度为0且"OnChip"缓存剩余空间大于等于数据包大小，则将数据包放入"OnChip"缓存
    {
        bmResult = BmResult(TO_ONCHIPBUFFER);
    }
    else if (qlen + pktSize > m_wredTh[priority])//如果数据包大小加上队列长度超过了权重随机早期丢弃（WRED）阈值，则直接丢弃数据包
    {
        bmResult = BmResult(DROP);
    }
    else if (cgStatus == CONGESTION && wcacheUsed >= m_wcacheFullTh[priority])//如果队列处于拥塞状态且wcache使用超过了Wcache满阈值，则直接丢弃数据包
    {
        bmResult = BmResult(DROP);
    }
    else if (cgStatus == CONGESTION && wcacheUsed >= m_wcacheCgTh[priority] &&
             qlen > m_longCgQlen[portType][priority])//如果队列处于拥塞状态且wcache缓存使用超过了Wcache拥塞阈值且队列长度大于最长拥塞队列长度，则直接丢弃数据包
    {
        bmResult = BmResult(DROP);
    }
    else if (cgStatus == CONGESTION && wcacheUsed < m_wcacheFullTh[priority] && dramRemain >= pktSize)
    {
        bmResult = BmResult(TO_OFFCHIPBUFFER);//如果队列处于拥塞状态且Wcache缓存使用未超过Wcache满阈值且Wcache缓存剩余空间大于等于数据包大小，则将数据包放入片外缓存
    }
    else if (m_onChipBufferRemain >= pktSize &&
             qlen <= m_alphaOfQueue[portType][type][priority][qIndex] * m_onChipBufferRemain &&
             m_priOnChipUsed[priority] < m_alphaOfPriority[priority] * m_onChipBufferRemain &&
             (m_enableOnChipPdp == 0 ||
              m_priDpUsed[port][priority] * (m_alphaOfPriority[priority] + 1) <
                  m_alphaOfPort[portType] * (m_alphaOfPriority[priority] * m_onChipBufferRemain -
                                         m_priOnChipUsed[priority]))) 
    {

        bmResult = BmResult(TO_ONCHIPBUFFER);//如果"OnChip"缓存剩余空间大于等于数据包大小且满足一定条件，则将数据包放入"OnChip"缓存
    }
    else if (wcacheUsed < m_wcacheFullTh[priority] && dramRemain >= pktSize)
    {
        bmResult = BmResult(TO_OFFCHIPBUFFER);//如果Wcache缓存使用未超过Wcache满阈值且片外缓存剩余空间大于等于数据包大小，则将数据包放入离片缓存
    }
    else
    {
        bmResult = BmResult(DROP);//否则，直接丢弃数据包
    }
    //cout<<"m_alphaOfQueue["<<portType<<"]["<<type<<"]["<<priority<<"]["<<qIndex<<"]： "<<m_alphaOfQueue[portType][type][priority][qIndex]<<endl;
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
SwitchMmu::CheckIngressAdmission(Ptr<Packet> packet)//检查数据包是否符合准入条件，并调用FindBufferLocation函数来确定数据包的存储位置
{
    NS_LOG_FUNCTION_NOARGS ();
    NS_LOG_FUNCTION(this << packet);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t prio = packet->GetMmuUsedPriority();

    // Check the validity of incoming parameters before go into BM algorithm.//如果数据包为空或者端口号、队列索引或优先级超出范围，则输出错误信息并返回DROP
    if (packet == nullptr || port >= m_nPorts || qIndex >= m_nQueuesPerPort ||
        prio >= m_nPriorities)
    {
        NS_FATAL_ERROR("The input packet has invalid input parameters!!!");
        return DROP;
    }
    //cout<<"检查数据包是否能准入"<<endl;
    return FindBufferLocation(packet);
}

SwitchMmu::BmResult  //BM总入口
SwitchMmu::FindBufferLocation(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    BmResult result = DROP;//初始化result为DROP
    BmAlgorithm bm = m_bmAlgorithm;//并将m_bmAlgorithm赋值给bm
    
    // BM Switch Here.
    switch (bm)
    {
    case (HW):
        result = CheckHWBmAlgorithm(packet);
        break;
    case (TDT):
        result = Check3DTBmAlgorithm(packet); //kkkk
        break;
    case (YSL):
        result = CheckYSLBmAlgorithm(packet);
        break;
    case (BASELINE):
        result = CheckBaselineBmAlgorithm(packet);
        break;
    case (YRF):
        result = CheckYRFBmAlgorithm(packet);
        //std::cout<<"YRF存储位置： "<<result<<endl;
        break;
    default:
        result = SwitchMmu::TO_OFFCHIPBUFFER;
        //std::cout<<"默认存储位置： "<<result<<endl;
    }

    // LOG some BM result.
    if (result == SwitchMmu::DROP)
    {
        m_stats.nTotalBmDropPackets++;
        m_stats.nTotalBmDropPacketsSize+=pktSize;

    }
    m_traceCheckAdmission(packet, result);

    //cout<<"当前时间"<<Simulator::Now().GetMicroSeconds()<<endl;
    
    double timermicro = (Simulator::Now()-Timer_Mill_Loss).GetMicroSeconds();
    if (timermicro>=1.0)
    {
        //cout<<"m_dequePktCnt"<<m_dequePktCnt<<endl;
        //cout<<"丢包数: "<<m_stats.nTotalBmDropPackets<<endl;
        //cout<<"总到达数: "<<m_stats.nTotalStoredPackets+m_stats.nTotalBmDropPackets<<endl;
        cout<<"当前丢包率： "<<(static_cast<double>(m_stats.nTotalBmDropPackets)/(m_stats.nTotalStoredPackets+m_stats.nTotalBmDropPackets))*100.0<<"%"<<endl;
        double LossPacketNumTotal=static_cast<double>(m_stats.nTotalBmDropPackets);
        double LossPacketNum=LossPacketNumTotal-LossPacketNum_Last;
        double LossPacketNumTotalSize=static_cast<double>(m_stats.nTotalBmDropPacketsSize);
        double LossPacketSize=LossPacketNumTotalSize-LossPacketNumTotalSizeLast;
        double LossPacketRate;
        if (LossPacketNumTotal!=0)
        {
            LossPacketRate=(static_cast<double>(m_stats.nTotalBmDropPackets)/(m_stats.nTotalStoredPackets+m_stats.nTotalBmDropPackets+1.0))*100.0;
        }else{
            LossPacketRate=0;
        }
        

        //同时输出到文件中便于观察数据
        std::string baseFilePath = "/home/dell6/yrf/pba/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/";
        std::string fileName;

        // 使用字符串流动态构建文件路径
        std::stringstream filePathStream;
        filePathStream << baseFilePath << "loss_packet.csv";
        fileName = filePathStream.str();
       
        ofstream fout(fileName, ios::app);
        fout <<Timer_Mill_Loss.GetSeconds()<<","<<Simulator::Now().GetSeconds()<<","<<LossPacketSize*8/1000.0<<","<<LossPacketRate<<endl;
        fout.close();

        LossPacketNum_Last=LossPacketNumTotal;
        LossPacketNumTotalSizeLast=LossPacketNumTotalSize;
        Timer_Mill_Loss=Simulator::Now();

    }

    return result;
}

SwitchMmu::BmResult
SwitchMmu::CheckYRFBmAlgorithm(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    //NS_LOG_UNCOND("运行到SwitchMmu中检查算法这里");
    uint32_t port = packet->GetMmuUsedPort(); //得到的是目的地端口
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;
    uint64_t qlen = m_qUsed[port][priority][qIndex];
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();
    uint64_t dramRemain = m_offChipBuffer->GetDramRemain();
    uint64_t dramUsed = m_offChipBuffer->GetDramUsed();
    uint64_t dramSize = m_offChipBuffer->GetDramSize();

    Flow type = down; 
    BmResult bmResult;
    PortType portType = m_portRates[port];
   // cout<<"Packet_Num_Cycle["<<port<<"]["<<priority<<"]["<<qIndex<<"]： "<<Packet_Num_Cycle[port][priority][qIndex]<<endl;

    Packet_Size_Cycle[port][priority][qIndex]+=pktSize;
    Packet_Num_Cycle[port][priority][qIndex]+=1;

    m_LastCycleTimeLength=m_CycleTimeLength;
    double math_mETC=m_Cost_ETC[port][priority][qIndex].GetNanoSeconds();

    double cycle_time=(Simulator::Now()-simulation_start[port][priority][qIndex]).GetNanoSeconds();
    if (math_mETC>0 && YRF_Flag_First==true)
    {
        m_LastCycleTimeLength=m_CycleTimeLength;
        bmResult = BmResult(TO_ONCHIPBUFFER);
        YRF_Flag_result[port][priority][qIndex]=true;
        YRF_Flag_First=false;
        simulation_start[port][priority][qIndex]=Simulator::Now();
        return bmResult;
    }
    else if (math_mETC>cycle_time && YRF_Flag_First==false)
    {
        bmResult = BmResult(DROP);
        if (YRF_Flag_result[port][priority][qIndex]==true)
        {
 
            if(m_onChipBufferRemain>=pktSize){
               
               bmResult = BmResult(TO_ONCHIPBUFFER);
            }
        }else{
            cout<<"pktSize： "<<pktSize<<endl;
            cout<<"wcacheSize: "<<wcacheSize<<"wcacheUsed: "<<wcacheUsed<<endl;
            if((wcacheSize-wcacheUsed)> pktSize && dramRemain > pktSize){
               bmResult = BmResult(TO_OFFCHIPBUFFER);
            }
        } 
        return bmResult;
    }
    else{

        if (qlen==0 && m_onChipBufferRemain>=pktSize)
        {
            cout<<"第0种情况 "<<endl;
            m_Cost_ETC[port][priority][qIndex]=NanoSeconds(0);
            YRF_Flag_result[port][priority][qIndex]=true;
            bmResult = BmResult(TO_ONCHIPBUFFER);
            return bmResult;

        }else{

            //cout<<"流["<<port<<"]["<<priority<<"]["<<qIndex<<"]的周期结束**"<<endl;
            //cout<<"Packet_Size_Cycle["<<port<<"]["<<priority<<"]["<<qIndex<<"]： "<<Packet_Size_Cycle[port][priority][qIndex]*8<<endl;
            //cout<<"上周期流["<<port<<"]["<<priority<<"]["<<qIndex<<"] 的周期长度"<<(Simulator::Now()-simulation_start[port][priority][qIndex]).GetNanoSeconds()/1000000000.0<<endl;
            
            
            double timermicro = (Simulator::Now()-Timer_Mill[port][priority][qIndex]).GetMicroSeconds();
            if (timermicro>=1.0)
            {
                if (Predict_Flag_First[port][priority][qIndex]==true)
                {
                    EWMA_R[port][priority][qIndex]=Packet_Size_Cycle[port][priority][qIndex]*8.0/timermicro;
                    Predict_Flag_First[port][priority][qIndex]=false;
                }else{
                    EWMA_R[port][priority][qIndex]=W*EWMA_R[port][priority][qIndex]+(1-W)*Packet_Size_Cycle[port][priority][qIndex]*8.0/timermicro;
                }

                Packet_Size_Cycle[port][priority][qIndex]=0;

                /* code */
                ReadSram_Rate_Cycle[port][priority][qIndex]=ReadSram_Size_Cycle[port][priority][qIndex]/timermicro;
                WriteDram_Rate_Cycle[port][priority][qIndex]=WriteDram_Size_Cycle[port][priority][qIndex]/timermicro;
                ReadDram_Rate_Cycle[port][priority][qIndex]=ReadDram_Size_Cycle[port][priority][qIndex]/timermicro;
                ReadSram_Size_Cycle[port][priority][qIndex]=0;
                WriteDram_Size_Cycle[port][priority][qIndex]=0;
                ReadDram_Size_Cycle[port][priority][qIndex]=0;
                Timer_Mill[port][priority][qIndex]=Simulator::Now();
            }

            simulation_start[port][priority][qIndex]=Simulator::Now();
            
            Packet_Num_Cycle[port][priority][qIndex]=0;
            QueueStatus cgStatus = GetQueueStatus(port, qIndex, priority);
            
            Sq=(m_onChipBufferSize-m_onChipBufferRemain)*8.0;
            Wq=wcacheUsed*8.0;
            Dq=dramUsed*8.0;
            Cqs=ReadSram_Rate_Cycle[port][priority][qIndex]*1000000*8.0;
            //Cdw=WriteDram_Rate_Cycle[port][priority][qIndex]*1000000*8.0;
            Cqd=ReadDram_Rate_Cycle[port][priority][qIndex]*1000000*8.0;
            Cq=Cqs+Cqd;//优先级队列被端口读取的速率，若始终读一条队列

            double Pqs=UsedSram_Size_Cycle[port][priority][qIndex]*8.0;
            double Pqd=(qlen-UsedSram_Size_Cycle[port][priority][qIndex])*8.0;
            
            double Pri_alpha=1.0/m_activeQueNum[port];//队列阈值系数1/m_activeQueNum[port]

            double ewma_r=EWMA_R[port][priority][qIndex]*1000000;

            double Pths=(Pri_alpha*m_onChipBufferRemain)*8.0;
            double Pthd=(Pri_alpha*dramRemain)*8.0;
            // int64_t Pth=(qlen+Pri_alpha*(m_onChipBufferRemain+dramRemain))*8.0;
            // int64_t S=m_onChipBufferSize*8.0;
            // int64_t D=dramSize*8.0;
 

            cout<<"ewma_r:"<<ewma_r<<endl;
            cout<<"Cqs:"<<Cqs<<endl;
            cout<<"Cqd:"<<Cqd<<endl;
            cout<<"Cq:"<<Cq<<endl;
            cout<<"Pths:"<<Pths<<endl;
            cout<<"Pthd:"<<Pthd<<endl;
            cout<<"m_activeQueNum["<<port<<"]:"<<m_activeQueNum[port]<<endl;
            cout<<"Simulator::Now():"<<Simulator::Now().GetSeconds()<<endl;
      
            cout<<"**************** "<<endl;
            cout<<"假设存SRAM: "<<endl;
            
            if (ewma_r<Cqs)
            {
                ETC_S0=Pqs/(Cqs-ewma_r);
                Cost_S0=(Pths-(ewma_r-Cq)*ETC_S0)/Pths+W1*W2*ewma_r*ETC_S0;
                Cost_min_S=Cost_S0;
                ETC_S=ETC_S0;
                cout<<"ETC_S0: "<<ETC_S2<<" Cost_S0: "<<Cost_S2<<endl;
            }else{

                ETC_S2=Pths/(ewma_r-Cqs);
                Cost_S2=(Pths-(ewma_r-Cqs)*ETC_S2)/Pths+W1*W2*ewma_r*ETC_S2;
                Cost_min_S=Cost_S2;
                ETC_S=ETC_S2;
                cout<<"ETC_S2: "<<ETC_S2<<" Cost_S2: "<<Cost_S2<<endl;

            }

            

             
            cout<<"假设存DRAM: "<<endl;
            if (ewma_r<Cqd)
            {
                ETC_D0=Pqd/(Cqd-ewma_r);
                Cost_D0=(Pthd-(ewma_r-Cqd)*ETC_D0)/Pthd+W1*W3*ewma_r*ETC_D0;
                Cost_min_D=Cost_D0;
                ETC_D=ETC_D0;
            }else{
                ETC_D0=Pths/(ewma_r-Cqd);
                Cost_D0=(Pthd-(ewma_r-Cqd)*ETC_D0)/Pthd+W1*W3*ewma_r*ETC_D0;

                ETC_D1=Pthd/(ewma_r-Cqd);
                Cost_D1=(Pthd-(ewma_r-Cqd)*ETC_D1)/Pthd+W1*W3*ewma_r*ETC_D1;

                if (Cost_D0<Cost_D1)
                {
                    Cost_min_D=Cost_D0;
                    ETC_D=ETC_D0;
                    cout<<"ETC_D0: "<<ETC_D0<<" Cost_D0: "<<Cost_D0<<endl;
                }else{

                    Cost_min_D=Cost_D1;
                    ETC_D=ETC_D1;
                    cout<<"ETC_D1: "<<ETC_D1<<" Cost_D1: "<<Cost_D1<<endl;
                }

            }


            
            
            cout<<"计算结果: "<<endl;
            cout<<"ETC_S: "<<ETC_S<<" Cost_min_S: "<<Cost_min_S<<endl;
            cout<<"ETC_D: "<<ETC_D<<" Cost_min_D: "<<Cost_min_D<<endl;

            //判断SRAM和DRAM时谁的Cost最小
            if(Cost_min_S<=Cost_min_D)
            {
                //存SRAM,周期为ETC_S
                m_Cost_ETC[port][priority][qIndex]=NanoSeconds(ETC_S);//1000000
                YRF_Flag_result[port][priority][qIndex]=true;
                if(m_onChipBufferRemain>pktSize){
                   bmResult = BmResult(TO_ONCHIPBUFFER);
                }else{
                    bmResult = BmResult(DROP);
                }

               cout<<"决策结果:存片内 "<<endl;

            }else{

                m_Cost_ETC[port][priority][qIndex]=NanoSeconds(ETC_D);//1000000
                YRF_Flag_result[port][priority][qIndex]=false;
                if((wcacheSize-wcacheUsed)> pktSize && dramRemain > pktSize){
                   bmResult = BmResult(TO_OFFCHIPBUFFER);
                }else{
                   bmResult = BmResult(DROP);
                }
                cout<<"决策结果:存片外 "<<endl;
            }
        }//不是优质流
    
    }
    //cout<<"F["<<port<<"]["<<priority<<"]["<<qIndex<<"] ETC:"<<m_Cost_ETC[port][priority][qIndex]<<endl;

    //同时输出到文件中便于观察数据***********************
    std::string filepath = "/home/dell6/yrf/pba/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/";
    std::string filename1;
    // 使用字符串流动态构建文件路径
    std::stringstream filepathstream;
    filepathstream << filepath << "cost_etc_test.csv";
    filename1 = filepathstream.str();
    ofstream fout1(filename1, ios::app);
    fout1 <<simulation_end.GetSeconds()<<","<<Simulator::Now().GetSeconds()<<","<<Cost_min_S<<","<<ETC_S*1000000<<","<<Cost_min_D<<","<<ETC_D*1000000<<endl;
    fout1.close();

    simulation_end=Simulator::Now();

    return bmResult;
}

//获取特定端口、队列索引和优先级下的队列状态
SwitchMmu::QueueStatus
SwitchMmu::GetQueueStatus(uint32_t port, uint32_t qIndex, uint32_t pri)
{
    NS_LOG_FUNCTION(this);
    BmAlgorithm bm = m_bmAlgorithm;//获取当前SwitchMmu对象的bmAlgorithm成员变量，并将其赋值给局部变量bm
    SwitchMmu::QueueStatus status;

    // BM Congestion Switch Here.
    switch (bm)
    {
    case (HW):
        status = GetHWCgStatus(port, qIndex, pri);//如果bm为HW或TDT，则调用GetHWCgStatus函数来获取硬件拥塞状态
        break;
    case (TDT):
        status = GetHWCgStatus(port, qIndex, pri);
        break;
    case (YSL):
        status = GetTimerQueueStatus(port, qIndex, pri);//如果bm为YSL，则调用GetTimerQueueStatus函数来获取定时器队列状态
        break;
    case (YRF):
        status = GetHWCgStatus(port, qIndex, pri);
        break;
    default:
        status = SwitchMmu::NOT_CONGESTION;//如果bm的取值不在上述范围内，则将队列状态设置为SwitchMmu::NOT_CONGESTION
    }

    return status;
}

SwitchMmu::QueueStatus
SwitchMmu::GetHWCgStatus(uint32_t port, uint32_t qIndex, uint32_t pri)
{
    NS_LOG_FUNCTION(this << port << qIndex << pri);
    PortType type = m_portRates[port];
    //cout<<"m_cgMax[type][pri]： "<<m_cgMax[type][pri]<<endl;
    if (m_qUsed[port][pri][qIndex] > m_cgMax[type][pri])//判断当前队列的使用量m_qUsed是否大于该端口类型下优先级pri对应的最大拥塞阈值m_cgMax[type][pri]
    {
        m_cgStatus[port][pri][qIndex] = CONGESTION;//如果是，则将该队列的拥塞状态设置为CONGESTION
    }
    else if (m_cgStatus[port][pri][qIndex] == true && m_qUsed[port][pri][qIndex] < m_cgMin[type][pri])//如果该队列当前状态为拥塞状态且队列的使用量小于该端口类型下优先级pri对应的最小拥塞阈值m_cgMin[type][pri]
    {
        m_cgStatus[port][pri][qIndex] = NOT_CONGESTION;//则将该队列的拥塞状态设置为NOT_CONGESTION
    }

    // For HW they just use it to classified Congestion or not.
    return m_cgStatus[port][pri][qIndex];//返回该队列的拥塞状态
}

void
SwitchMmu::Store(Ptr<Packet> packet, SwitchMmu::BmResult location)//紧接上存储决策算法
{
    NS_LOG_FUNCTION(this << packet << location);
    uint32_t port = packet->GetMmuUsedPort();
    uint32_t qIndex = packet->GetMmuUsedQueueId();
    uint32_t priority = packet->GetMmuUsedPriority();
    uint32_t psize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION;

    if (location == DROP)//如果location为DROP，则记录警告信息并返回，表示数据包应该被丢弃
    {
        NS_LOG_WARN("The Packet should drop but not buffer ingress.");
        return;
    }

    m_nPackets++; //增加已处理数据包的计数器
    m_stats.nTotalStoredPackets++;//并更新总存储数据包的计数器

    if (location == TO_ONCHIPBUFFER)
    {   
        //cout<<"m_onChipBufferRemain-Store： "<<m_onChipBufferRemain<<endl;
        //cout<<"pktSize-Store： "<<psize<<endl;
        NS_ASSERT_MSG(m_onChipBufferRemain >= psize,//确保OnChip缓冲区剩余空间大于数据包大小
                      "When decided to ingress "
                      "packet into OnChipBuffer, the Remained size "
                      "should be larger than packet size.");

        NS_LOG_DEBUG("SwitchMmu: Store in OnChip!");
        m_stats.nTotalOnChipBufferStoredPackets++;//更新相关统计信息，包括OnChip缓冲区存储数据包数量、剩余空间、优先级使用量等
        packet->SetLocation(Packet::ONCHIPBUFFER);//将数据包位置标记为OnChipBuffer，并更新OnChip缓冲区剩余空间
        UsedSram_Size_Cycle[port][priority][qIndex]+=psize;
        m_onChipBufferRemain -= psize;
        m_priOnChipUsed[priority] += psize;
        m_priDpUsed[port][priority] += psize;

        // Trace
        m_traceSramWriteComplete(packet);//记录Sram写完成的trace信息
    }
    else //否则，将数据包存储到OffChip缓冲区中
    {
        NS_LOG_DEBUG("SwitchMmu: Store in OffChip!");
        m_offChipBuffer->Write(packet); //调用OffChip缓冲区的Write函数将数据包写入OffChip缓冲区
    }

    // Note Here:
    //
    // After Determine the Packet should be loaded in the Buffer.
    // The counters can be and should be modified immediately no
    // matter where it is decided to save. Because the following
    // input packet and BM algorithm should both take this packet
    // into account. To avoid unexpected count and buffer allocation
    // error!
    if (m_qUsed[port][priority][qIndex] == 0)//如果当前队列m_qUsed[port][priority][qIndex]中没有数据包，则增加活跃队列数m_activeQueNum[port]
    {
        m_activeQueNum[port]++;
    }
    m_qlens[port][qIndex] += psize;//更新队列长度、队列使用量、队列接收总量等信息
    m_qUsed[port][priority][qIndex] += psize;
    m_qTotalRcvd[port][priority][qIndex] += psize;
    if (m_qUsed[port][priority][qIndex] > m_qMaxUsed[port][priority][qIndex])//如果当前队列使用量超过历史最大使用量，则更新历史最大使用量
    {
        m_qMaxUsed[port][priority][qIndex] = m_qUsed[port][priority][qIndex];
    }

    m_enqueTime.push(Simulator::Now());

    m_traceStore(packet);//记录存储数据包的trace信息

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

    NS_ASSERT_MSG(packet != nullptr, "The Reading Packet should not be NULL ptr!");//断言数据包不为空，确保数据包指针有效

    m_qUsed[port][priority][qIndex] -= psize;//减少队列中数据包的使用量m_qUsed[port][priority][qIndex]和队列长度m_qlens[port][qIndex]
    m_qlens[port][qIndex] -= psize;
    if (m_qUsed[port][priority][qIndex] == 0)//果当前队列中没有数据包，则减少活跃队列数m_activeQueNum[port]
    {
        m_activeQueNum[port]--;
    }

    m_nPackets--;//减少已处理数据包的计数器

    // Set the Packet Place to be NOTINBUFFER.
    packet->SetLocation(Packet::NOTINBUFFER);//将数据包位置标记为NOTINBUFFER，表示数据包已不在缓冲区中

    // Set the flag to tell the reorder model that the packet has been fetched
    packet->SetMmuFetchStatus(true);//设置数据包的MMU读取状态为已完成

    // DEBUG Usage: show counters now.
    // ShowCounters ();
    m_traceFetch(packet);//记录数据包读取完成的trace信息
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
    Total_delay+=delay.ToDouble(Time::NS); 
    m_dequePktCnt++;
    m_avgDelay = Total_delay / m_dequePktCnt;
    m_enqueTime.pop();

    cout<<"Total_delay"<<Total_delay<<endl;
    cout<<"m_dequePktCnt"<<m_dequePktCnt<<endl;
    cout<<"m_avgDelay: "<<m_avgDelay/1000.0<<endl;

    if (packet != nullptr)//如果数据包不为空，则获取数据包的大小
    {
        psize = packet->GetSize();
    }

    if (packet->GetLocation() == Packet::ONCHIPBUFFER)//如果数据包在芯片上的缓冲区(ONCHIPBUFFER)中
    // The packet is in the OnChipBuffer(SRAM).
    {
        // Get packet Out.
        ReadSram_Size_Cycle[port][priority][qIndex]+=psize;//统计周期内优先级队列从SRAM读取的数据包大小
        UsedSram_Size_Cycle[port][priority][qIndex]-=psize;

        m_onChipBufferRemain += psize;//增加芯片上缓冲区剩余空间的大小m_onChipBufferRemain
        m_priOnChipUsed[priority] -= psize;//减少优先级对应的芯片上缓冲区使用量m_priOnChipUsed[priority]
        m_priDpUsed[port][priority] -= psize;//端口优先级对应的芯片上缓冲区使用量m_priDpUsed[port][priority]

        // Packets in onchip buffer are considered to be completely fetched
        // immediately. So set the counters back here.
        FetchComplete(packet);//调用FetchComplete函数标记数据包已经完全读取

        // Trace.
        m_traceSramReadComplete(packet);
    }
    else if (packet->GetLocation() == Packet::WRITINGTOOFFCHIPBUFFER ||
             packet->GetLocation() == Packet::OFFCHIPBUFFER ||
             packet->GetLocation() == Packet::WCACHE)
    {
        // The packet is in the OffChipBuffer(WCache or HBM).
        return m_offChipBuffer->Read(packet);//调用m_offChipBuffer->Read(packet)函数处理数据包读取
        cout<<"哈哈哈"<<endl;
    }
    else //如果数据包位置不在缓冲区中，则输出错误信息并返回false
    {
        NS_LOG_ERROR("Cannot Read the Packet which is not in Buffer!");
        return false;
    }

    

    return true;//返回true表示数据包读取成功
}

void
SwitchMmu::SetOnChipPdpStatus(bool status)//这段代码实现了设置芯片上PDP（Packet Data Processor）状态的函数
{
    NS_LOG_FUNCTION(this << status);
    m_enableOnChipPdp = status;//将成员变量m_enableOnChipPdp的值设置为传入的status值
}
//根据传入的flowType、qIndex、priority和alpha值，将m_alphaOfQueue数组中对应位置的值设置为alpha
//这段代码的作用是为特定的流类型、队列索引、优先级设置相应的alpha值
void
SwitchMmu::SetQueueLevelAlpha(uint32_t flowType, uint32_t qIndex, uint32_t priority, uint32_t alpha)
{
    NS_LOG_FUNCTION(this << flowType << priority << qIndex << alpha);
    m_alphaOfQueue[Gbps100][flowType][priority][qIndex] = alpha;
}
//这段代码实现了设置优先级级别alpha值的函数
void
SwitchMmu::SetPriorityLevelAlpha(uint32_t prior, uint32_t alpha)
{
    NS_LOG_FUNCTION(this << prior << alpha);
    m_alphaOfPriority[prior] = alpha;
}
//实现了设置端口级别alpha值的功能
void
SwitchMmu::SetPortLevelAlpha(uint32_t port, uint32_t alpha)
{
    NS_LOG_FUNCTION(this << port << alpha);
    m_alphaOfPort[Gbps100] = alpha;
}
//实现了设置端口速率类型的功能
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
    m_wcacheFullTh[prior] = th;//将m_wcacheFullTh数组中索引为prior的位置的值设置为传入的th值
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
//段代码是一个函数GetOffChipBuffer，用于返回一个指向OffChipBuffer对象的智能指针Ptr<OffChipBuffer>
Ptr<OffChipBuffer>
SwitchMmu::GetOffChipBuffer() const
{
    NS_LOG_FUNCTION(this);
    return m_offChipBuffer;//返回成员变量m_offChipBuffer，该成员变量应该是一个指向OffChipBuffer对象的智能指针
}
//将一个指向OffChipBuffer对象的智能指针附加到SwitchMmu对象上，并进行一系列操作
void
SwitchMmu::AttachOffChipBuffer(Ptr<OffChipBuffer> offChipBuffer)
{
    NS_LOG_FUNCTION(this << offChipBuffer);
    m_offChipBuffer = offChipBuffer;//将传入的offChipBuffer指针赋值给SwitchMmu对象的成员变量m_offChipBuffer
    m_offChipBuffer->SetMmu(this);//用offChipBuffer对象的SetMmu方法，将当前的SwitchMmu对象指针传递给offChipBuffer对象，以建立对象间的关联

    m_offChipBuffer->TraceConnectWithoutContext(//使用TraceConnectWithoutContext方法连接offChipBuffer对象的特定事件信号和SwitchMmu对象的相应处理函数
        "WCacheReadComplete",
        MakeCallback(&SwitchMmu::ReadWcacheComplete, this));//当"WCacheReadComplete"事件发生时，调用SwitchMmu对象的ReadWcacheComplete函数
    m_offChipBuffer->TraceConnectWithoutContext(
        "WCacheWriteComplete",
        MakeCallback(&SwitchMmu::WriteWcacheComplete, this));//当"WCacheWriteComplete"事件发生时，调用SwitchMmu对象的WriteWcacheComplete函数
    m_offChipBuffer->TraceConnectWithoutContext(
        "DramReadComplete",
        MakeCallback(&SwitchMmu::ReadDramComplete, this));//当"DramReadComplete"事件发生时，调用SwitchMmu对象的ReadDramComplete函数
    m_offChipBuffer->TraceConnectWithoutContext(
        "DramWriteComplete",
        MakeCallback(&SwitchMmu::WriteDramComplete, this));//当"DramWriteComplete"事件发生时，调用SwitchMmu对象的WriteDramComplete函数
}
//用于注册设备处理程序（DeviceHandler）和对应的网络设备（NetDevice）到SwitchMmu对象中
void
SwitchMmu::RegisterDeviceHandler(DeviceHandler handler, Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << &handler << device);
    struct SwitchMmu::DeviceHandlerEntry entry;//创建一个SwitchMmu::DeviceHandlerEntry结构体对象entry
    entry.handler = handler;//将传入的handler函数指针和device智能指针存储到entry结构体中
    entry.device = device;
    m_handlers.push_back(entry);//将entry结构体添加到SwitchMmu对象的m_handlers容器中
}
//这段代码是一个函数UnregisterDeviceHandler，用于从SwitchMmu对象中注销特定的设备处理程序（DeviceHandler）
void
SwitchMmu::UnregisterDeviceHandler(DeviceHandler handler)
{
    NS_LOG_FUNCTION(this << &handler);
    for (DeviceHandlerList::iterator i = m_handlers.begin(); i != m_handlers.end(); i++)//遍历m_handlers容器中的所有注册的设备处理程序
    {
        if (i->handler.IsEqual(handler))//对比每个设备处理程序的函数指针是否与传入的handler相等
        {
            m_handlers.erase(i);//如果找到与handler相等的设备处理程序，则从m_handlers容器中删除该条目，并终止循环
            break;
        }
    }
}
//用于处理来自特定网络设备的请求
bool
SwitchMmu::HandleRequest(Ptr<NetDevice> dev)
{
    NS_LOG_FUNCTION(this << dev);
    bool found = false;//初始化一个布尔变量found为false，用于标记是否找到符合条件的设备处理程序
    for (DeviceHandlerList::iterator i = m_handlers.begin(); i != m_handlers.end(); i++)//遍历m_handlers容器中的所有注册的设备处理程序
    {
        if (!(i->device) || ((i->device) && i->device == dev))//对比每个设备处理程序的device字段是否为空或者与传入的dev相等
        {
            i->handler();//如果找到符合条件的设备处理程序，则调用该设备处理程序的处理函数，并将found标记为true
            found = true;
        }
    }
    return found;//返回found，表示是否找到并处理了请求
}
//获取SwitchMmu类中存储的数据包数量，并将其作为uint64_t类型返回
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
//根据LRU（Least Recently Used）算法选择拥塞队列
void
SwitchMmu::SelectLRUCongestion()
{
    NS_LOG_FUNCTION(this);

    uint32_t maxI = 0;//初始化变量maxI、maxJ、maxT和CgQueue，分别用于记录最大的i、j、m_cgTimer值和拥塞队列数量
    uint32_t maxJ = 0;
    uint32_t maxT = 0;
    uint32_t CgQueue = 0;

    // Timer to figure the Congestion Control.
    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        for (uint32_t j = 0; j < m_nQueuesPerPort; j++)
        {
            if (m_cgStatus[i][0][j] == BURST)//使用双重循环遍历m_cgStatus数组，查找状态为BURST的队列，并根据条件进行处理
            {
                if (m_cgTimer[i][j] > 100)//如果某个队列的m_cgTimer超过100，则将该队列状态设为CONGESTION，并增加CgQueue计数
                {
                    m_cgStatus[i][0][j] = CONGESTION;
                    CgQueue ++;
                    break;
                }

                if (maxT < m_cgTimer[i][j] && m_cgStatus[i][0][j] == BURST)//如果存在BURST状态的队列，选择m_cgTimer最大的队列作为LRU队列
                {
                    maxT = m_cgTimer[i][j];
                    maxI = i;
                    maxJ = j;
                }
            }
        }
    }

    if (CgQueue == 0 && m_cgTimer[maxI][maxJ] > 10)//果没有发现拥塞队列，并且LRU队列的m_cgTimer大于10，则将LRU队列状态设为CONGESTION
    {
        m_cgStatus[maxI][0][maxJ] = CONGESTION;
    }

}
//用于更新LRU定时器并定时调度自身以实现周期性更新
void
SwitchMmu::UpdateLruTimer()
{
    NS_LOG_FUNCTION(this);

    for (uint32_t i = 0; i < m_nPorts; i++)
    {
        for (uint32_t j = 0; j < m_nQueuesPerPort; j++)
        {
            if (m_cgStatus[i][0][j] == BURST) {//使用双重循环遍历m_cgStatus数组，对状态为BURST的队列的定时器m_cgTimer进行递增操作
                m_cgTimer[i][j] ++;
            }
        }
    }

    Simulator::Schedule(m_updateLruTimeWindow, &SwitchMmu::UpdateLruTimer, this);//调度下一次UpdateLruTimer函数的执行，以实现定时更新LRU定时器的功能
}
//用于获取特定端口、队列索引和优先级下的队列状态
SwitchMmu::QueueStatus
SwitchMmu::GetTimerQueueStatus(uint32_t port, uint32_t qIndex, uint32_t pri)
{
    NS_LOG_FUNCTION(this << port << qIndex << pri);

    // Maybe Timer.

    // For HW they just use it to classified Congestion or not.
    return m_cgStatus[port][pri][qIndex];
}

} // namespace ns3
