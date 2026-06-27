#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

// 拥塞窗口变化回调函数
void CwndChange(uint32_t oldCwnd, uint32_t newCwnd) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\tCWND: " << newCwnd / 1024.0 << " KB");
}

// 接收数据包回调函数
void ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\tReceived " << packet->GetSize() << " bytes from "
                          << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }
    }
}

int main(int argc, char *argv[]) {
    // 设置日志组件
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 创建节点
    NodeContainer nodes;
    nodes.Create(2);

    // 创建点对点链路
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // 安装协议栈
    InternetStackHelper stack;
    stack.Install(nodes);

    // 分配IP地址
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 创建TCP接收端
    uint16_t port = 50000;
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // 获取接收套接字并设置回调
    Ptr<Socket> recvSocket = DynamicCast<PacketSink>(sinkApps.Get(0))->GetListeningSocket();
    recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // 创建TCP发送端
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer sourceApps = onoff.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // 获取发送套接字并连接CongestionWindow追踪源
    Ptr<Socket> sendSocket = DynamicCast<OnOffApplication>(sourceApps.Get(0))->GetSocket();
    if (sendSocket) {
        Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(sendSocket);
        if (tcpSocket) {
            tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange));
        }
    }

    // 启用全局跟踪（可选）
    // Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow",
    //                              MakeCallback(&CwndChange));

    // 运行仿真
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}