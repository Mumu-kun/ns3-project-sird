#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IncastBaseline");

void
BytesInQueueTrace(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

int
main(int argc, char* argv[])
{
    std::string protocol = "udp";
    CommandLine cmd;
    cmd.AddValue("protocol", "udp/sird/dctcp", protocol);
    cmd.Parse(argc, argv);

    std::string filename = "scratch/my-simulations/incast-udp";

    // Queue tracing disabled for speed
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(filename + "-queue.tr");

    Time::SetResolution(Time::NS);
    // Logging disabled for speed - enable only for debugging
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Params - Scaled down 10x: 10Gbps link with 1.7Gbps saturators
    // Maintains same 2% oversubscription: 6 × 1.7Gbps = 10.2Gbps > 10Gbps
    std::string dataRate = "10Gbps";
    std::string delay = "9.1us"; // Half-RTT=18us (propagation delay unchanged)

    uint32_t queuePacketSize = 9 * 1024; // 9KB
    // Keep original packet counts for queue (larger buffer relative to BDP at 10Gbps)
    uint32_t bdpPackets = 24;
    uint32_t BPackets = (uint32_t)(1.5 * bdpPackets); // 36 packets
    double minThPackets = 0.5 * bdpPackets;           // 12 packets
    double maxThPackets = 1.0 * bdpPackets;           // 24 packets

    // Scaled-down from paper: 10MB → 100KB (100x smaller), maintains same ratio
    uint32_t saturationPacketSize = 9 * 1024; // 9KB jumbo frames (matches paper)
    // uint32_t probePacketSize = 8;             // 8B
    uint32_t probePacketSize = 500; // 500B

    bool enableSwitchEcn = true;
    // Set default parameters for RED queue disc
    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(enableSwitchEcn));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(queuePacketSize));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    // BDP-limited buffer
    Config::SetDefault("ns3::RedQueueDisc::MaxSize",
                       QueueSizeValue(QueueSize(std::to_string(BPackets) + "p")));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(minThPackets));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(maxThPackets));

    uint32_t numSaturators = 6;
    uint32_t probeSenderIdx = 6; // 7th sender probes

    double simTime = 0.1; // Short sim time: 100ms is enough to capture incast behavior

    //// Topology
    // Nodes
    NodeContainer senderNodes;
    senderNodes.Create(numSaturators + 1); // 7 saturators + 1 probe
    Ptr<Node> receiverNode = CreateObject<Node>();
    Ptr<Node> switchNode = CreateObject<Node>();

    // Links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    // Senders <-> Switch
    std::vector<NetDeviceContainer> senderToSwitchLinks;
    senderToSwitchLinks.reserve(numSaturators + 1);
    for (std::size_t i = 0; i < senderNodes.GetN(); i++)
    {
        senderToSwitchLinks.push_back(p2p.Install(senderNodes.Get(i), switchNode));
    }
    // Receiver <-> Switch
    NetDeviceContainer receiverToSwitchLink = p2p.Install(receiverNode, switchNode);

    InternetStackHelper stack;
    stack.InstallAll();

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    QueueDiscContainer qdiscs = tch.Install(receiverToSwitchLink.Get(1));
    Ptr<QueueDisc> q = qdiscs.Get(0);
    // Queue tracing disabled for speed
    q->TraceConnectWithoutContext("BytesInQueue", MakeBoundCallback(&BytesInQueueTrace, stream));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    std::vector<Ipv4InterfaceContainer> ipSenderToSwitch;
    ipSenderToSwitch.reserve(numSaturators + 1);
    for (std::size_t i = 0; i < senderNodes.GetN(); i++)
    {
        ipSenderToSwitch.push_back(address.Assign(senderToSwitchLinks[i]));
        address.NewNetwork();
    }
    Ipv4InterfaceContainer ipReceiverToSwitch = address.Assign(receiverToSwitchLink);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address serverIP = ipReceiverToSwitch.GetAddress(0);

    //// Applications
    // Two services on receiver:
    // Port 1234: PacketSink for saturator traffic (one-way)
    // Port 1235: UdpEchoServer for probe traffic (request-reply for latency)
    uint16_t satPort = 1234;
    uint16_t probePort = 1235;

    // PacketSink for saturators (receives 10MB requests, no reply)
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), satPort));
    ApplicationContainer sinkApp = sink.Install(receiverNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // Echo server for probe (receives small requests, sends reply for latency measurement)
    UdpEchoServerHelper echoServer(probePort);
    ApplicationContainer serverApps = echoServer.Install(receiverNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Saturators (Senders 0-5) - Scaled 10x: 1.7Gbps each into 10Gbps link
    // Creates 10.2Gbps > 10Gbps = congestion (same 2% oversubscription)
    OnOffHelper satClient("ns3::UdpSocketFactory", InetSocketAddress(serverIP, satPort));
    satClient.SetAttribute("DataRate", StringValue("1.7Gbps"));
    satClient.SetAttribute("PacketSize", UintegerValue(saturationPacketSize)); // 9KB packets
    satClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    satClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer satApps;
    for (uint32_t i = 0; i < numSaturators; i++)
    {
        satApps.Add(satClient.Install(senderNodes.Get(i)));
    }
    satApps.Start(Seconds(0.01));
    satApps.Stop(Seconds(simTime));

    // Probe (Sender 6) - sends small requests to EchoServer, measures RTT
    // 50μs interval = ~1600 samples in 80ms, good for CDF
    UdpEchoClientHelper probeClient(serverIP, probePort);
    probeClient.SetAttribute("MaxPackets", UintegerValue(0));
    probeClient.SetAttribute("Interval", TimeValue(MicroSeconds(50)));
    probeClient.SetAttribute("PacketSize", UintegerValue(probePacketSize));

    ApplicationContainer probeApps = probeClient.Install(senderNodes.Get(probeSenderIdx));
    probeApps.Start(Seconds(0.02));
    probeApps.Stop(Seconds(simTime));

    //// Simulation
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->SetAttribute("DelayBinWidth",
                          DoubleValue(0.0000001)); // 0.1μs bins for good latency resolution

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile(filename + ".flowmon", true, true);

    Simulator::Destroy();
    NS_LOG_INFO("Incast simulation complete. Check " << filename << ".flowmon");
    return 0;
}
