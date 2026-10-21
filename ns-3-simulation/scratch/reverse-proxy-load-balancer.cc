#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
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
    void Configure(uint16_t listenPort, Ipv4Address proxyAddress);
    uint32_t GetHandledRequests() const;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_listenPort = 0;
    Ipv4Address m_proxyAddress;
    uint32_t m_handledRequests = 0;
};

NS_OBJECT_ENSURE_REGISTERED(BackendApp);

TypeId
BackendApp::GetTypeId()
{
    static TypeId tid = TypeId("BackendApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<BackendApp>();
    return tid;
}

void
BackendApp::Configure(uint16_t listenPort, Ipv4Address proxyAddress)
{
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
                if (m_activeConnections[backend] > 0)
                {
                    --m_activeConnections[backend];
                }
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
    void Configure(Ipv4Address proxyAddress,
                   uint32_t requestCount,
                   uint32_t firstRequestId,
                   uint32_t packetSize,
                   Time interval,
                   bool measureRecovery,
                   Time recoveryAt);
    uint32_t GetSentRequests() const;
    uint32_t GetCompletedRequests() const;
    uint64_t GetReceivedPayloadBytes() const;
    double GetAverageRttSeconds() const;
    bool HasRecovered() const;
    double GetRecoveryTimeSeconds() const;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void SendNextRequest();
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    Ipv4Address m_proxyAddress;
    uint32_t m_requestCount = 0;
    uint32_t m_firstRequestId = 0;
    uint32_t m_nextRequest = 0;
    uint32_t m_sentRequests = 0;
    uint32_t m_completedRequests = 0;
    uint32_t m_packetSize = 0;
    uint64_t m_receivedPayloadBytes = 0;
    Time m_interval;
    Time m_rttSum = Seconds(0);
    bool m_measureRecovery = false;
    Time m_recoveryAt = Seconds(0);
    bool m_hasRecovered = false;
    Time m_recoveryTime = Seconds(0);
    std::map<uint32_t, Time> m_requestSendTimes;
};

NS_OBJECT_ENSURE_REGISTERED(ClientApp);

TypeId
ClientApp::GetTypeId()
{
    static TypeId tid = TypeId("ClientApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<ClientApp>();
    return tid;
}

void
ClientApp::Configure(Ipv4Address proxyAddress,
                     uint32_t requestCount,
                     uint32_t firstRequestId,
                     uint32_t packetSize,
                     Time interval,
                     bool measureRecovery,
                     Time recoveryAt)
{
    m_proxyAddress = proxyAddress;
    m_requestCount = requestCount;
    m_firstRequestId = firstRequestId;
    m_packetSize = packetSize;
    m_interval = interval;
    m_measureRecovery = measureRecovery;
    m_recoveryAt = recoveryAt;
}

uint32_t
ClientApp::GetSentRequests() const
{
    return m_sentRequests;
}

uint32_t
ClientApp::GetCompletedRequests() const
{
    return m_completedRequests;
}

uint64_t
ClientApp::GetReceivedPayloadBytes() const
{
    return m_receivedPayloadBytes;
}

double
ClientApp::GetAverageRttSeconds() const
{
    return m_completedRequests == 0 ? 0.0 : m_rttSum.GetSeconds() / m_completedRequests;
}

bool
ClientApp::HasRecovered() const
{
    return m_hasRecovered;
}

double
ClientApp::GetRecoveryTimeSeconds() const
{
    return m_recoveryTime.GetSeconds();
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

    uint32_t requestId = m_firstRequestId + m_nextRequest++;
    std::vector<uint8_t> payload(m_packetSize, 0);
    std::copy(reinterpret_cast<const uint8_t*>(&requestId),
              reinterpret_cast<const uint8_t*>(&requestId) + sizeof(requestId),
              payload.begin());
    Ptr<Packet> packet = Create<Packet>(payload.data(), payload.size());
    m_requestSendTimes[requestId] = Simulator::Now();
    ++m_sentRequests;
    m_socket->Send(packet);
    Simulator::Schedule(m_interval, &ClientApp::SendNextRequest, this);
}

void
ClientApp::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        uint32_t requestId = 0;
        if (packet->GetSize() >= sizeof(requestId))
        {
            packet->CopyData(reinterpret_cast<uint8_t*>(&requestId), sizeof(requestId));
        }

        auto sentAt = m_requestSendTimes.find(requestId);
        if (sentAt != m_requestSendTimes.end())
        {
            Time rtt = Simulator::Now() - sentAt->second;
            m_rttSum += rtt;
            m_requestSendTimes.erase(sentAt);
            ++m_completedRequests;
            m_receivedPayloadBytes += packet->GetSize();

            if (m_measureRecovery && !m_hasRecovered && Simulator::Now() >= m_recoveryAt)
            {
                m_hasRecovered = true;
                m_recoveryTime = Simulator::Now() - m_recoveryAt;
            }
        }
    }
}

struct SimulationResults
{
    uint32_t sentRequests = 0;
    uint32_t completedRequests = 0;
    uint64_t responseBytes = 0;
    std::vector<uint32_t> forwardedRequests;
    uint64_t ipTxPackets = 0;
    uint64_t ipRxPackets = 0;
    uint64_t ipLostPackets = 0;
    double averageRttSeconds = 0.0;
    double throughputMbps = 0.0;
    double deliveryRatio = 0.0;
    bool hasRecoveryMeasurement = false;
    double recoveryTimeSeconds = 0.0;
};

static void
SetBackendLinkState(Ptr<RateErrorModel> firstReceiverError,
                    Ptr<RateErrorModel> secondReceiverError,
                    AnimationInterface* animation,
                    uint32_t backendNodeId,
                    uint32_t backendNumber,
                    bool linkUp)
{
    double packetErrorRate = linkUp ? 0.0 : 1.0;
    firstReceiverError->SetAttribute("ErrorRate", DoubleValue(packetErrorRate));
    secondReceiverError->SetAttribute("ErrorRate", DoubleValue(packetErrorRate));
    animation->UpdateNodeColor(backendNodeId,
                               linkUp ? 0 : 255,
                               linkUp ? 180 : 0,
                               0);
    std::cout << "[" << Simulator::Now().GetSeconds() << " s] Backend link "
              << (linkUp ? "restored" : "failed") << " (Server-" << backendNumber << ")\n";
}

static SimulationResults
RunSimulation(const std::string& algorithm,
              uint32_t requestCount,
              double requestInterval,
              double simulationTime,
              uint32_t packetSize,
              const std::string& dataRate,
              const std::string& delay,
              bool simulateFailure,
              double failureAt,
              double recoveryAt,
              uint32_t failureBackend,
              const std::string& animationFile)
{
    NodeContainer clientNodes;
    clientNodes.Create(20);
    NodeContainer proxyNode;
    proxyNode.Create(1);
    NodeContainer backendNodes;
    backendNodes.Create(3);

    InternetStackHelper internet;
    internet.Install(clientNodes);
    internet.Install(proxyNode);
    internet.Install(backendNodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    Ipv4AddressHelper address;
    std::vector<Ipv4Address> clientProxyAddresses;
    std::vector<Ipv4Address> proxyAddresses;
    std::vector<Ipv4Address> backendAddresses;
    std::vector<NetDeviceContainer> backendLinks;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        NetDeviceContainer devices = pointToPoint.Install(clientNodes.Get(i), proxyNode.Get(0));
        address.SetBase(("10.0." + std::to_string(i + 1) + ".0").c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        clientProxyAddresses.push_back(interfaces.GetAddress(1));
    }

    for (uint32_t i = 0; i < backendNodes.GetN(); ++i)
    {
        NetDeviceContainer devices = pointToPoint.Install(proxyNode.Get(0), backendNodes.Get(i));
        address.SetBase(("10.0." + std::to_string(i + 21) + ".0").c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        proxyAddresses.push_back(interfaces.GetAddress(0));
        backendAddresses.push_back(interfaces.GetAddress(1));
        backendLinks.push_back(devices);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    const std::vector<uint16_t> backendPorts = {9001, 9002, 9003};
    const std::vector<uint32_t> weights = {3, 2, 1};

    Ptr<LoadBalancerApp> loadBalancer = CreateObject<LoadBalancerApp>();
    loadBalancer->Configure(algorithm, backendAddresses, backendPorts, weights);
    proxyNode.Get(0)->AddApplication(loadBalancer);
    loadBalancer->SetStartTime(Seconds(0.1));
    loadBalancer->SetStopTime(Seconds(simulationTime));

    for (uint32_t i = 0; i < backendNodes.GetN(); ++i)
    {
        Ptr<BackendApp> backend = CreateObject<BackendApp>();
        backend->Configure(backendPorts[i], proxyAddresses[i]);
        backendNodes.Get(i)->AddApplication(backend);
        backend->SetStartTime(Seconds(0.1));
        backend->SetStopTime(Seconds(simulationTime));
    }

    std::vector<Ptr<ClientApp>> clients;
    uint32_t nextRequestId = 0;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        uint32_t clientRequests = requestCount / clientNodes.GetN();
        if (i < requestCount % clientNodes.GetN())
        {
            ++clientRequests;
        }

        Ptr<ClientApp> client = CreateObject<ClientApp>();
        client->Configure(clientProxyAddresses[i],
                          clientRequests,
                          nextRequestId,
                          packetSize,
                          Seconds(requestInterval),
                          simulateFailure,
                          Seconds(recoveryAt));
        nextRequestId += clientRequests;
        clientNodes.Get(i)->AddApplication(client);
        client->SetStartTime(Seconds(0.2));
        client->SetStopTime(Seconds(simulationTime));
        clients.push_back(client);
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clientNodes);
    mobility.Install(proxyNode);
    mobility.Install(backendNodes);

    AnimationInterface animation(animationFile);
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        animation.UpdateNodeDescription(clientNodes.Get(i)->GetId(),
                                        "Client-" + std::to_string(i + 1));
        animation.SetConstantPosition(clientNodes.Get(i), 0.0, static_cast<double>(i) * 0.5);
    }
    animation.UpdateNodeDescription(proxyNode.Get(0)->GetId(), "Load Balancer");
    animation.UpdateNodeDescription(backendNodes.Get(0)->GetId(), "Server-1");
    animation.UpdateNodeDescription(backendNodes.Get(1)->GetId(), "Server-2");
    animation.UpdateNodeDescription(backendNodes.Get(2)->GetId(), "Server-3");
    animation.SetConstantPosition(proxyNode.Get(0), 4.0, 2.0);
    animation.SetConstantPosition(backendNodes.Get(0), 8.0, 0.0);
    animation.SetConstantPosition(backendNodes.Get(1), 8.0, 2.0);
    animation.SetConstantPosition(backendNodes.Get(2), 8.0, 4.0);
    animation.UpdateNodeColor(backendNodes.Get(1)->GetId(), 0, 180, 0);

    Ptr<RateErrorModel> firstReceiverError;
    Ptr<RateErrorModel> secondReceiverError;
    if (simulateFailure)
    {
        firstReceiverError = CreateObject<RateErrorModel>();
        secondReceiverError = CreateObject<RateErrorModel>();
        firstReceiverError->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
        secondReceiverError->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
        firstReceiverError->SetAttribute("ErrorRate", DoubleValue(0.0));
        secondReceiverError->SetAttribute("ErrorRate", DoubleValue(0.0));
        backendLinks[failureBackend - 1].Get(0)->SetAttribute("ReceiveErrorModel",
                                                              PointerValue(firstReceiverError));
        backendLinks[failureBackend - 1].Get(1)->SetAttribute("ReceiveErrorModel",
                                                              PointerValue(secondReceiverError));
        Simulator::Schedule(Seconds(failureAt),
                            &SetBackendLinkState,
                            firstReceiverError,
                            secondReceiverError,
                            &animation,
                            backendNodes.Get(failureBackend - 1)->GetId(),
                            failureBackend,
                            false);
        Simulator::Schedule(Seconds(recoveryAt),
                            &SetBackendLinkState,
                            firstReceiverError,
                            secondReceiverError,
                            &animation,
                            backendNodes.Get(failureBackend - 1)->GetId(),
                            failureBackend,
                            true);
    }

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    SimulationResults results;
    results.forwardedRequests = loadBalancer->GetForwardedRequests();
    Time totalRtt = Seconds(0);
    double firstObservedRecovery = 0.0;
    for (const auto& client : clients)
    {
        results.sentRequests += client->GetSentRequests();
        results.completedRequests += client->GetCompletedRequests();
        results.responseBytes += client->GetReceivedPayloadBytes();
        totalRtt += Seconds(client->GetAverageRttSeconds() * client->GetCompletedRequests());
        if (client->HasRecovered() &&
            (!results.hasRecoveryMeasurement ||
             client->GetRecoveryTimeSeconds() < firstObservedRecovery))
        {
            results.hasRecoveryMeasurement = true;
            firstObservedRecovery = client->GetRecoveryTimeSeconds();
        }
    }
    results.averageRttSeconds = results.completedRequests == 0
                                    ? 0.0
                                    : totalRtt.GetSeconds() / results.completedRequests;
    double measurementDuration = simulationTime - 0.2;
    results.throughputMbps = measurementDuration <= 0.0
                                 ? 0.0
                                 : (results.responseBytes * 8.0) / (measurementDuration * 1e6);
    results.deliveryRatio = results.sentRequests == 0
                                ? 0.0
                                : static_cast<double>(results.completedRequests) / results.sentRequests;

    results.recoveryTimeSeconds = firstObservedRecovery;
    flowMonitor->CheckForLostPackets(Seconds(0));
    const auto& flowStats = flowMonitor->GetFlowStats();
    for (const auto& entry : flowStats)
    {
        results.ipTxPackets += entry.second.txPackets;
        results.ipRxPackets += entry.second.rxPackets;
        results.ipLostPackets += entry.second.lostPackets;
    }

    std::cout << "\n=== Reverse Proxy NS-3 Simulation ===\n";
    std::cout << "Algorithm: " << algorithm << "\n";
    std::cout << "Clients: " << clientNodes.GetN() << "\n";
    std::cout << "Link bandwidth: " << dataRate << "\n";
    std::cout << "Link delay: " << delay << "\n";
    std::cout << "Request payload: " << packetSize << " bytes\n";
    std::cout << "Requests sent: " << results.sentRequests << "\n";
    std::cout << "Requests completed: " << results.completedRequests << "\n";
    std::cout << "Request/response loss: "
              << (results.sentRequests - results.completedRequests) << " requests ("
              << (100.0 * (1.0 - results.deliveryRatio)) << "%)\n";
    std::cout << "IP packets: " << results.ipTxPackets << " transmitted, "
              << results.ipRxPackets << " received, " << results.ipLostPackets << " lost\n";
    std::cout << "IP packet loss ratio: "
              << (results.ipTxPackets == 0
                      ? 0.0
                      : 100.0 * results.ipLostPackets / results.ipTxPackets)
              << "%\n";
    std::cout << "Application throughput: " << results.throughputMbps << " Mbps\n";
    std::cout << "Average request/response delay (RTT): " << results.averageRttSeconds * 1000.0
              << " ms\n";
    std::cout << "Packet delivery ratio: " << results.deliveryRatio * 100.0 << "%\n";
    for (uint32_t i = 0; i < results.forwardedRequests.size(); ++i)
    {
        std::cout << "SERVER-" << (i + 1) << " forwarding attempts: "
                  << results.forwardedRequests[i] << "\n";
    }
    if (simulateFailure)
    {
        if (results.hasRecoveryMeasurement)
        {
            std::cout << "Measured recovery time (restoration to first returned response): "
                      << results.recoveryTimeSeconds << " s\n";
        }
        else
        {
            std::cout << "Measured recovery time: no successful response observed before simulation end\n";
        }
    }
    std::cout << "NetAnim output: " << animationFile << "\n";

    Simulator::Destroy();
    return results;
}

int
main(int argc, char* argv[])
{
    std::string algorithm = "wrr";
    uint32_t requestCount = 30;
    double requestInterval = 0.05;
    double simulationTime = 5.0;
    uint32_t packetSize = 1024;
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    bool simulateFailure = false;
    double failureAt = 2.0;
    double recoveryAt = 4.0;
    uint32_t failureBackend = 2;
    bool compare = false;
    std::string animationFile = "reverse-proxy-animation.xml";

    CommandLine commandLine(__FILE__);
    commandLine.AddValue("algorithm", "Load-balancing algorithm: wrr or least", algorithm);
    commandLine.AddValue("requests", "Number of requests across all clients", requestCount);
    commandLine.AddValue("interval", "Seconds between requests from each client", requestInterval);
    commandLine.AddValue("simulationTime", "Simulation duration in seconds", simulationTime);
    commandLine.AddValue("packetSize", "Request and response payload size in bytes", packetSize);
    commandLine.AddValue("dataRate", "Point-to-point link bandwidth, e.g. 2Mbps", dataRate);
    commandLine.AddValue("delay", "Point-to-point channel delay, e.g. 20ms", delay);
    commandLine.AddValue("simulateFailure", "Fail and restore one backend link", simulateFailure);
    commandLine.AddValue("failureAt", "Backend link failure time in seconds", failureAt);
    commandLine.AddValue("recoveryAt", "Backend link restoration time in seconds", recoveryAt);
    commandLine.AddValue("failureBackend", "1-based backend server to disconnect", failureBackend);
    commandLine.AddValue("compare", "Run and compare WRR and least-connections", compare);
    commandLine.AddValue("animationFile", "NetAnim XML output path for a single run", animationFile);
    commandLine.Parse(argc, argv);

    if ((algorithm != "wrr" && algorithm != "least") || requestCount == 0 ||
        requestInterval < 0.0 || simulationTime <= 0.2 || packetSize < sizeof(uint32_t) ||
        failureBackend < 1 || failureBackend > 3)
    {
        NS_FATAL_ERROR("Invalid arguments: use algorithm=wrr|least, requests>0, interval>=0, "
                       "simulationTime>0.2, packetSize>=4, and failureBackend=1..3");
    }
    if (simulateFailure &&
        (failureAt <= 0.2 || recoveryAt <= failureAt || recoveryAt >= simulationTime))
    {
        NS_FATAL_ERROR("Failure simulation requires 0.2 < failureAt < recoveryAt < simulationTime");
    }

    if (!compare)
    {
        RunSimulation(algorithm,
                      requestCount,
                      requestInterval,
                      simulationTime,
                      packetSize,
                      dataRate,
                      delay,
                      simulateFailure,
                      failureAt,
                      recoveryAt,
                      failureBackend,
                      animationFile);
        return 0;
    }

    SimulationResults wrr = RunSimulation("wrr",
                                          requestCount,
                                          requestInterval,
                                          simulationTime,
                                          packetSize,
                                          dataRate,
                                          delay,
                                          simulateFailure,
                                          failureAt,
                                          recoveryAt,
                                          failureBackend,
                                          "reverse-proxy-animation-wrr.xml");
    SimulationResults least = RunSimulation("least",
                                            requestCount,
                                            requestInterval,
                                            simulationTime,
                                            packetSize,
                                            dataRate,
                                            delay,
                                            simulateFailure,
                                            failureAt,
                                            recoveryAt,
                                            failureBackend,
                                            "reverse-proxy-animation-least.xml");

    std::cout << "\n=== Load-Balancing Performance Comparison ===\n";
    std::cout << "Strategy         Throughput (Mbps)   Avg RTT (ms)   Delivery ratio (%)\n";
    std::cout << "WRR              " << wrr.throughputMbps << "                "
              << wrr.averageRttSeconds * 1000.0 << "           "
              << wrr.deliveryRatio * 100.0 << "\n";
    std::cout << "Least connections " << least.throughputMbps << "                "
              << least.averageRttSeconds * 1000.0 << "           "
              << least.deliveryRatio * 100.0 << "\n";
    std::cout << "Each row is measured from its own simulation run.\n";
    return 0;
}
