#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ReverseProxyLoadBalancerSimulation");

class BackendApp : public Application
{
  public:
    static TypeId GetTypeId();
    void Configure(uint32_t serverId, uint16_t listenPort, Ipv4Address proxyAddress);
    uint32_t GetHandledRequests() const;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint32_t m_serverId = 0;
    uint16_t m_listenPort = 0;
    Ipv4Address m_proxyAddress;
    uint32_t m_handledRequests = 0;
};

NS_OBJECT_ENSURE_REGISTERED(BackendApp);

TypeId
BackendApp::GetTypeId()
{
    static TypeId tid =
        TypeId("BackendApp").SetParent<Application>().SetGroupName("Tutorial").AddConstructor<BackendApp>();
    return tid;
}

void
BackendApp::Configure(uint32_t serverId, uint16_t listenPort, Ipv4Address proxyAddress)
{
    m_serverId = serverId;
    m_listenPort = listenPort;
    m_proxyAddress = proxyAddress;
}

uint32_t
BackendApp::GetHandledRequests() const
{
    return m_handledRequests;
}

void
BackendApp::StartApplication()
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_listenPort));
    m_socket->Connect(InetSocketAddress(m_proxyAddress, 9000));
    m_socket->SetRecvCallback(MakeCallback(&BackendApp::HandleRead, this));
}

void
BackendApp::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
BackendApp::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        ++m_handledRequests;
        socket->Send(packet);
    }
}

class LoadBalancerApp : public Application
{
  public:
    static TypeId GetTypeId();
    void Configure(std::string algorithm,
                   const std::vector<Ipv4Address>& backendAddresses,
                   const std::vector<uint16_t>& backendPorts,
                   const std::vector<uint32_t>& weights);
    const std::vector<uint32_t>& GetForwardedRequests() const;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void HandleRead(Ptr<Socket> socket);
    uint32_t SelectBackend();
    int FindBackend(const Address& source) const;

    Ptr<Socket> m_socket;
    std::vector<Ptr<Socket>> m_backendSockets;
    std::vector<Ipv4Address> m_backendAddresses;
    std::vector<uint16_t> m_backendPorts;
    std::vector<uint32_t> m_weights;
    std::vector<uint32_t> m_activeConnections;
    std::vector<uint32_t> m_forwardedRequests;
    std::map<uint32_t, Address> m_pendingRequests;
    std::string m_algorithm;
    uint32_t m_weightedIndex = 0;
};

NS_OBJECT_ENSURE_REGISTERED(LoadBalancerApp);

TypeId
LoadBalancerApp::GetTypeId()
{
    static TypeId tid = TypeId("LoadBalancerApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<LoadBalancerApp>();
    return tid;
}

void
LoadBalancerApp::Configure(std::string algorithm,
                           const std::vector<Ipv4Address>& backendAddresses,
                           const std::vector<uint16_t>& backendPorts,
                           const std::vector<uint32_t>& weights)
{
    m_algorithm = algorithm;
    m_backendAddresses = backendAddresses;
    m_backendPorts = backendPorts;
    m_weights = weights;
    m_activeConnections.assign(backendAddresses.size(), 0);
    m_forwardedRequests.assign(backendAddresses.size(), 0);
}

const std::vector<uint32_t>&
LoadBalancerApp::GetForwardedRequests() const
{
    return m_forwardedRequests;
}

void
LoadBalancerApp::StartApplication()
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9000));
    m_socket->SetRecvCallback(MakeCallback(&LoadBalancerApp::HandleRead, this));

    for (uint32_t i = 0; i < m_backendAddresses.size(); ++i)
    {
        Ptr<Socket> backendSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        backendSocket->Connect(InetSocketAddress(m_backendAddresses[i], m_backendPorts[i]));
        m_backendSockets.push_back(backendSocket);
    }
}

void
LoadBalancerApp::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }

    for (const auto& socket : m_backendSockets)
    {
        socket->Close();
    }
    m_backendSockets.clear();
}

uint32_t
LoadBalancerApp::SelectBackend()
{
    if (m_algorithm == "least")
    {
        return static_cast<uint32_t>(std::distance(
            m_activeConnections.begin(),
            std::min_element(m_activeConnections.begin(), m_activeConnections.end())));
    }

    uint32_t totalWeight = 0;
    for (uint32_t weight : m_weights)
    {
        totalWeight += weight;
    }

    uint32_t position = m_weightedIndex++ % totalWeight;
    for (uint32_t i = 0; i < m_weights.size(); ++i)
    {
        if (position < m_weights[i])
        {
            return i;
        }
        position -= m_weights[i];
    }
    return 0;
}

int
LoadBalancerApp::FindBackend(const Address& source) const
{
    if (!InetSocketAddress::IsMatchingType(source))
    {
        return -1;
    }

    Ipv4Address sourceAddress = InetSocketAddress::ConvertFrom(source).GetIpv4();
    for (uint32_t i = 0; i < m_backendAddresses.size(); ++i)
    {
        if (sourceAddress == m_backendAddresses[i])
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void
LoadBalancerApp::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        uint32_t requestId = 0;
        if (packet->GetSize() >= sizeof(requestId))
        {
            packet->CopyData(reinterpret_cast<uint8_t*>(&requestId), sizeof(requestId));
        }

        int backend = FindBackend(from);
        if (backend >= 0)
        {
            auto pending = m_pendingRequests.find(requestId);
            if (pending != m_pendingRequests.end())
            {
                socket->SendTo(packet, 0, pending->second);
                m_activeConnections[backend]--;
                m_pendingRequests.erase(pending);
            }
            continue;
        }

        uint32_t selected = SelectBackend();
        m_pendingRequests[requestId] = from;
        ++m_activeConnections[selected];
        ++m_forwardedRequests[selected];
        m_backendSockets[selected]->Send(packet);
    }
}

class ClientApp : public Application
{
  public:
    static TypeId GetTypeId();
    void Configure(Ipv4Address proxyAddress, uint32_t requestCount, Time interval);
    uint32_t GetCompletedRequests() const;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void SendNextRequest();
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    Ipv4Address m_proxyAddress;
    uint32_t m_requestCount = 0;
    uint32_t m_nextRequest = 0;
    uint32_t m_completedRequests = 0;
    Time m_interval;
};

NS_OBJECT_ENSURE_REGISTERED(ClientApp);

TypeId
ClientApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ClientApp").SetParent<Application>().SetGroupName("Tutorial").AddConstructor<ClientApp>();
    return tid;
}

void
ClientApp::Configure(Ipv4Address proxyAddress, uint32_t requestCount, Time interval)
{
    m_proxyAddress = proxyAddress;
    m_requestCount = requestCount;
    m_interval = interval;
}

uint32_t
ClientApp::GetCompletedRequests() const
{
    return m_completedRequests;
}

void
ClientApp::StartApplication()
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(InetSocketAddress(m_proxyAddress, 9000));
    m_socket->SetRecvCallback(MakeCallback(&ClientApp::HandleRead, this));
    Simulator::ScheduleNow(&ClientApp::SendNextRequest, this);
}

void
ClientApp::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
ClientApp::SendNextRequest()
{
    if (m_nextRequest >= m_requestCount)
    {
        return;
    }

    uint32_t requestId = m_nextRequest++;
    Ptr<Packet> packet = Create<Packet>(reinterpret_cast<const uint8_t*>(&requestId), sizeof(requestId));
    m_socket->Send(packet);
    Simulator::Schedule(m_interval, &ClientApp::SendNextRequest, this);
}

void
ClientApp::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        ++m_completedRequests;
    }
}

int
main(int argc, char* argv[])
{
    std::string algorithm = "wrr";
    uint32_t requestCount = 30;
    double requestInterval = 0.05;
    double simulationTime = 5.0;

    CommandLine commandLine(__FILE__);
    commandLine.AddValue("algorithm", "Load-balancing algorithm: wrr or least", algorithm);
    commandLine.AddValue("requests", "Number of client requests", requestCount);
    commandLine.AddValue("interval", "Seconds between client requests", requestInterval);
    commandLine.AddValue("simulationTime", "Simulation duration in seconds", simulationTime);
    commandLine.Parse(argc, argv);

    if (algorithm != "wrr" && algorithm != "least")
    {
        NS_FATAL_ERROR("algorithm must be wrr or least");
    }

    NodeContainer clientNode;
    clientNode.Create(20);
    NodeContainer proxyNode;
    proxyNode.Create(1);
    NodeContainer backendNodes;
    backendNodes.Create(3);

    InternetStackHelper internet;
    internet.Install(clientNode);
    internet.Install(proxyNode);
    internet.Install(backendNodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    std::vector<Ipv4Address> proxyAddresses;
    std::vector<Ipv4Address> backendAddresses;
    std::vector<Ipv4Address> clientProxyAddresses;
    for (uint32_t i = 0; i < clientNode.GetN(); ++i)
    {
        NetDeviceContainer clientProxyDevices =
            pointToPoint.Install(clientNode.Get(i), proxyNode.Get(0));
        address.SetBase(("10.0." + std::to_string(i + 1) + ".0").c_str(),
                        "255.255.255.0");
        Ipv4InterfaceContainer clientProxyInterfaces = address.Assign(clientProxyDevices);
        clientProxyAddresses.push_back(clientProxyInterfaces.GetAddress(1));
    }

    for (uint32_t i = 0; i < 3; ++i)
    {
        NetDeviceContainer devices = pointToPoint.Install(proxyNode.Get(0), backendNodes.Get(i));
        address.SetBase(("10.0." + std::to_string(i + 21) + ".0").c_str(),
                        "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        proxyAddresses.push_back(interfaces.GetAddress(0));
        backendAddresses.push_back(interfaces.GetAddress(1));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::vector<uint16_t> backendPorts = {9001, 9002, 9003};
    std::vector<uint32_t> weights = {3, 2, 1};

    Ptr<LoadBalancerApp> loadBalancer = CreateObject<LoadBalancerApp>();
    loadBalancer->Configure(algorithm, backendAddresses, backendPorts, weights);
    proxyNode.Get(0)->AddApplication(loadBalancer);
    loadBalancer->SetStartTime(Seconds(0.1));
    loadBalancer->SetStopTime(Seconds(simulationTime));

    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<BackendApp> backend = CreateObject<BackendApp>();
        backend->Configure(i + 1, backendPorts[i], proxyAddresses[i]);
        backendNodes.Get(i)->AddApplication(backend);
        backend->SetStartTime(Seconds(0.1));
        backend->SetStopTime(Seconds(simulationTime));
    }

    std::vector<Ptr<ClientApp>> clients;
    for (uint32_t i = 0; i < clientNode.GetN(); ++i)
    {
        Ptr<ClientApp> client = CreateObject<ClientApp>();
        uint32_t clientRequests = requestCount / clientNode.GetN();
        if (i < requestCount % clientNode.GetN())
        {
            ++clientRequests;
        }
        client->Configure(clientProxyAddresses[i],
                          clientRequests,
                          Seconds(requestInterval));
        clientNode.Get(i)->AddApplication(client);
        client->SetStartTime(Seconds(0.2));
        client->SetStopTime(Seconds(simulationTime));
        clients.push_back(client);
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clientNode);
    mobility.Install(proxyNode);
    mobility.Install(backendNodes);

    AnimationInterface animation("reverse-proxy-animation.xml");
    for (uint32_t i = 0; i < clientNode.GetN(); ++i)
    {
        animation.UpdateNodeDescription(clientNode.Get(i)->GetId(),
                                        "Client-" + std::to_string(i + 1));
    }
    animation.UpdateNodeDescription(proxyNode.Get(0)->GetId(), "Load Balancer");
    animation.UpdateNodeDescription(backendNodes.Get(0)->GetId(), "Server-1");
    animation.UpdateNodeDescription(backendNodes.Get(1)->GetId(), "Server-2");
    animation.UpdateNodeDescription(backendNodes.Get(2)->GetId(), "Server-3");

    for (uint32_t i = 0; i < clientNode.GetN(); ++i)
    {
        animation.SetConstantPosition(clientNode.Get(i),
                                      0.0,
                                      static_cast<double>(i) * 0.5);
    }
    animation.SetConstantPosition(proxyNode.Get(0), 4.0, 2.0);
    animation.SetConstantPosition(backendNodes.Get(0), 8.0, 0.0);
    animation.SetConstantPosition(backendNodes.Get(1), 8.0, 2.0);
    animation.SetConstantPosition(backendNodes.Get(2), 8.0, 4.0);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    std::cout << "\n=== Reverse Proxy NS-3 Simulation ===\n";
    std::cout << "Algorithm: " << algorithm << "\n";
    std::cout << "Clients: " << clientNode.GetN() << "\n";
    std::cout << "Requests sent: " << requestCount << "\n";
    uint32_t completedRequests = 0;
    for (const auto& client : clients)
    {
        completedRequests += client->GetCompletedRequests();
    }
    std::cout << "Requests completed: " << completedRequests << "\n";
    const auto& forwarded = loadBalancer->GetForwardedRequests();
    for (uint32_t i = 0; i < forwarded.size(); ++i)
    {
        std::cout << "SERVER-" << (i + 1) << " handled: " << forwarded[i] << "\n";
    }

    Simulator::Destroy();
    return 0;
}
