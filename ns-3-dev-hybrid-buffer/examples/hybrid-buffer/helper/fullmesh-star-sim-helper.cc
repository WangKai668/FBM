#include "fullmesh-star-sim-helper.h"

#include "../applications/fullmesh-helper.h"

namespace ns3
{

namespace hb
{

NS_LOG_COMPONENT_DEFINE("FullmeshStarSimHelper");

FullmeshStarSimHelper::FullmeshStarSimHelper(std::string simName, Time start, Time stop)
    : StarSimHelper(simName, start, stop)
{
    NS_LOG_FUNCTION(this << simName);
}

FullmeshStarSimHelper::~FullmeshStarSimHelper()
{
    NS_LOG_FUNCTION(this);
}

void
FullmeshStarSimHelper::CreateTraffic()
{
    NS_LOG_FUNCTION(this);
    uint16_t listenPort = 1000 - 1;
    uint16_t size = m_fullmeshSocket.size();
    for (uint32_t i = 0; i < size; i ++)
    {
        uint32_t length = m_fullmeshSocket[i].size();
        for (uint32_t sid = 0; sid < length; sid++)
        {
            listenPort++;

            FullmeshHelper fullmeshHelper("ns3::UdpSocketFactory");

            if (m_socketType == "tcp")
            {
                fullmeshHelper.SetAttribute("Protocol", StringValue("ns3::TcpSocketFactory"));
                if (m_pktSize) {
                    fullmeshHelper.SetAttribute("SendSize", UintegerValue(m_pktSize));
                } else {
                    fullmeshHelper.SetAttribute("SendSize", UintegerValue(TCPPAYLOAD_BYTES));
                }
            }
            else
            {
                fullmeshHelper.SetAttribute("Protocol", StringValue("ns3::UdpSocketFactory"));
                if (m_pktSize) {
                    fullmeshHelper.SetAttribute("SendSize", UintegerValue(m_pktSize));
                } else {
                    fullmeshHelper.SetAttribute("SendSize", UintegerValue(UDPPAYLOAD_BYTES));
                }
            }
            fullmeshHelper.SetAttribute("MaxBytes", UintegerValue(0));
            ApplicationContainer sourceApp = fullmeshHelper.Install(m_spokes.Get(m_fullmeshSocket[i][sid] + m_nReceivers));
            Ptr<FullmeshApplication> fullmeshApp = DynamicCast<FullmeshApplication>(sourceApp.Get(0));

            Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), listenPort));
            ApplicationContainer sinkApp;

            for (uint32_t rid = 0; rid < length; rid++)
            {
                AddressValue recvAddress(
                    InetSocketAddress(m_spokeInterfaces.GetAddress(m_fullmeshSocket[i][rid]), listenPort));
                if (m_socketType == "tcp")
                {
                    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
                    sinkApp = sinkHelper.Install(m_spokes.Get(m_fullmeshSocket[i][rid]));
                }
                else
                {
                    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
                    sinkApp = sinkHelper.Install(m_spokes.Get(m_fullmeshSocket[i][rid]));
                }
                sinkApp.Start(m_startTime);
                sinkApp.Stop(m_stopTime);
                m_sinkApps.push_back(sinkApp);

                fullmeshApp->AddRemote(recvAddress.Get(), m_fullmeshWeight[i][rid]);
            }
            sourceApp.Start(m_startTime);
            sourceApp.Stop(m_stopTime);
            m_sourceApps.push_back(sourceApp);
        }
    }


    // normal flow.
    uint16_t baseListenPort = 1000;

    for (auto& flow : m_flows)
    {
        NS_LOG_LOGIC(flow);
        uint16_t listenPort = baseListenPort + m_spokes.Get(flow.dstId)->GetNApplications();
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), listenPort));
        ApplicationContainer sinkApp;

        AddressValue recvAddress(
            InetSocketAddress(m_spokeInterfaces.GetAddress(flow.dstId), listenPort));
        ApplicationContainer sourceApp;
        if (m_socketType == "tcp")
        {
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            sinkApp = sinkHelper.Install(m_spokes.Get(flow.dstId));
            uint32_t payload = flow.pktSize ? flow.pktSize : TCPPAYLOAD_BYTES;

            BulkSendHelper tcpSendHelper("ns3::TcpSocketFactory", Address());
            tcpSendHelper.SetAttribute("Remote", recvAddress);
            tcpSendHelper.SetAttribute("SendSize", UintegerValue(payload));
            tcpSendHelper.SetAttribute("MaxBytes", UintegerValue(flow.size));
            sourceApp = tcpSendHelper.Install(m_spokes.Get(flow.srcId));
        }
        else
        {
            PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
            sinkApp = sinkHelper.Install(m_spokes.Get(flow.dstId));
            int32_t payload = flow.pktSize ? flow.pktSize : UDPPAYLOAD_BYTES;

            OnOffHelper udpSendHelper("ns3::UdpSocketFactory", Address());
            udpSendHelper.SetAttribute("Remote", recvAddress);
            udpSendHelper.SetAttribute("MaxBytes", UintegerValue(flow.size));
            udpSendHelper.SetConstantRate(ConvertPhyRateToAppRate(flow.rate), payload);
            udpSendHelper.SetAttribute("OnTime", StringValue(flow.onTime));
            udpSendHelper.SetAttribute("OffTime", StringValue(flow.offTime));
            sourceApp = udpSendHelper.Install(m_spokes.Get(flow.srcId));
        }

        sinkApp.Start(flow.startTime);
        sinkApp.Stop(flow.stopTime);
        sourceApp.Start(flow.startTime);
        sourceApp.Stop(flow.stopTime);
        m_sinkApps.push_back(sinkApp);
        m_sourceApps.push_back(sourceApp);
    }
}

void
FullmeshStarSimHelper::SetUpFullMeshTraffic(std::vector<std::vector<int>>* fullmeshSocket, std::vector<std::vector<int>>* fullmeshWeight)
{
    NS_LOG_FUNCTION(this);

    // The fullmesh can be within some port group.
    // so use a Two-dimensional array to represent fullmesh port group.
    m_fullmeshSocket = (*fullmeshSocket);
    m_fullmeshWeight = (*fullmeshWeight);
}

void
FullmeshStarSimHelper::SetPktSize(uint32_t size)
{
    NS_LOG_FUNCTION(this);

    m_pktSize = size;
    Config::SetDefault("ns3::PointToPointNetDevice::Mtu", UintegerValue(m_pktSize));
}

} // namespace hb

} // namespace ns3
