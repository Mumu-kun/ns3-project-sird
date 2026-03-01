#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UnloadedBaseline");

int
main(int argc, char* argv[])
{
    std::string filename = "scratch/my-simulations/unloaded-udp";

    Time::SetResolution(Time::NS);

    // Params - Same as incast but NO saturators
    std::string dataRate = "10Gbps";
    std::string delay = "9.1us"; // Half-RTT=18us

    uint32_t queuePacketSize = 9 * 1024; // 9KB
    uint32_t bdpPackets = 24;
    uint32_t BPackets = (uint32_t)(1.5 * bdpPackets); // 36 packets

    // uint32_t probePacketSize = 8;   // 8B for small probe
    uint32_t probePacketSize = 500; // 500B probe (match your incast setup)

    // Disable ECN for simplicity in unloaded case
    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(false));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(queuePacketSize));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize",
                       QueueSizeValue(QueueSize(std::to_string(BPackets) + "p")));

    double simTime = 0.05; // Shorter - no need to wait for congestion

    //// Topology - Just 1 sender (probe) + 1 receiver
    NodeContainer senderNodes;
    senderNodes.Create(1); // Only probe sender
    Ptr<Node> receiverNode = CreateObject<Node>();
    Ptr<Node> switchNode = CreateObject<Node>();

    // Links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    // Sender <-> Switch
    NetDeviceContainer senderToSwitchLink = p2p.Install(senderNodes.Get(0), switchNode);

    // Receiver <-> Switch
    NetDeviceContainer receiverToSwitchLink = p2p.Install(receiverNode, switchNode);

    InternetStackHelper stack;
    stack.InstallAll();

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    tch.Install(receiverToSwitchLink.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipSenderToSwitch = address.Assign(senderToSwitchLink);
    address.NewNetwork();
    Ipv4InterfaceContainer ipReceiverToSwitch = address.Assign(receiverToSwitchLink);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address serverIP = ipReceiverToSwitch.GetAddress(0);

    //// Applications - Only echo server and probe (NO saturators!)
    uint16_t probePort = 1235;

    // Echo server for probe
    UdpEchoServerHelper echoServer(probePort);
    ApplicationContainer serverApps = echoServer.Install(receiverNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Probe - sends requests to measure unloaded latency
    UdpEchoClientHelper probeClient(serverIP, probePort);
    probeClient.SetAttribute("MaxPackets", UintegerValue(0));
    probeClient.SetAttribute("Interval", TimeValue(MicroSeconds(50))); // 50μs interval
    probeClient.SetAttribute("PacketSize", UintegerValue(probePacketSize));

    ApplicationContainer probeApps = probeClient.Install(senderNodes.Get(0));
    probeApps.Start(Seconds(0.01));
    probeApps.Stop(Seconds(simTime));

    //// Simulation
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.0000001)); // 0.1μs bins

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile(filename + ".flowmon", true, true);

    Simulator::Destroy();
    NS_LOG_INFO("Unloaded simulation complete. Check " << filename << ".flowmon");
    return 0;
}
