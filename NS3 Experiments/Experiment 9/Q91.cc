/*
 * Simulation and Performance Analysis of TCP Congestion Control Algorithms
 * using NS-3 -- Vehicular Ad Hoc Network (VANET) scenario.
 *
 * Scenario:
 *   5 vehicles send TCP traffic (representing traffic info / camera data /
 *   road-condition updates) through a Roadside Unit (RSU) router, across a
 *   bottleneck link representing "the Internet", to a Traffic Control
 *   Server.
 *
 *   Vehicle1 ──┐
 *   Vehicle2 ──┤
 *   Vehicle3 ──┼── RSU (router) ===Internet(bottleneck)=== Traffic Server
 *   Vehicle4 ──┤
 *   Vehicle5 ──┘
 *
 * All 5 vehicles compete for the same bottleneck link, which is what
 * creates congestion -- exactly the condition needed to compare how
 * TCP Reno, TCP Cubic, and TCP BBR behave.
 *
 * The whole simulation is wrapped in a function and run once per TCP
 * variant, one after another, in a single program execution.
 *
 *
 * Outputs (per variant):
 *   - Printed FlowMonitor stats: throughput, delay, packet loss per vehicle
 *   - Jain's fairness index across the 5 vehicle flows
 *   - cwnd_<variant>_vehicleN.dat : congestion window over time, for plotting
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpVariantsVanet");

const uint32_t kNumVehicles = 5;

// ---------- Congestion window tracing ----------
static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

static void TraceCwnd(std::string variantName, uint32_t nodeId)
{
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream =
        ascii.CreateFileStream("cwnd_" + variantName + "_vehicle" + std::to_string(nodeId) + ".dat");
    std::ostringstream path;
    path << "/NodeList/" << nodeId << "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
    Config::ConnectWithoutContext(path.str(), MakeBoundCallback(&CwndChange, stream));
}

void RunSimulation(std::string tcpTypeId, std::string variantName)
{
    // ---------- Select the TCP variant for this run ----------
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                        TypeIdValue(TypeId::LookupByName(tcpTypeId)));

    // ---------- Parameters ----------
    std::string vehicleLinkBw = "10Mbps";
    std::string vehicleLinkDelay = "2ms";
    std::string internetBw = "5Mbps";   // bottleneck: shared "Internet" link
    std::string internetDelay = "20ms"; // WAN-like latency to the server
    uint32_t queueSizePkts = 50;
    double simTime = 20.0;
    uint32_t tcpPacketSize = 1024;

    // ---------- Nodes ----------
    NodeContainer vehicles;
    vehicles.Create(kNumVehicles);
    NodeContainer rsu;
    rsu.Create(1);
    NodeContainer server;
    server.Create(1);

    // ---------- Links: each vehicle <-> RSU ----------
    PointToPointHelper vehicleLink;
    vehicleLink.SetDeviceAttribute("DataRate", StringValue(vehicleLinkBw));
    vehicleLink.SetChannelAttribute("Delay", StringValue(vehicleLinkDelay));

    std::vector<NetDeviceContainer> vehicleDevices(kNumVehicles);
    for (uint32_t i = 0; i < kNumVehicles; ++i)
    {
        NodeContainer pair(vehicles.Get(i), rsu.Get(0));
        vehicleDevices[i] = vehicleLink.Install(pair);
    }

    // ---------- Link: RSU <-> Server (the bottleneck "Internet" link) ----------
    PointToPointHelper internetLink;
    internetLink.SetDeviceAttribute("DataRate", StringValue(internetBw));
    internetLink.SetChannelAttribute("Delay", StringValue(internetDelay));
    internetLink.SetQueue("ns3::DropTailQueue<Packet>",
                           "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSizePkts)));

    NodeContainer rsuServer(rsu.Get(0), server.Get(0));
    NetDeviceContainer internetDevices = internetLink.Install(rsuServer);

    // ---------- Internet stack ----------
    InternetStackHelper stack;
    stack.InstallAll();

    // ---------- IP addressing ----------
    Ipv4AddressHelper addr;
    std::vector<Ipv4InterfaceContainer> vehicleIfaces(kNumVehicles);
    for (uint32_t i = 0; i < kNumVehicles; ++i)
    {
        std::ostringstream base;
        base << "10.1." << (i + 1) << ".0";
        addr.SetBase(base.str().c_str(), "255.255.255.0");
        vehicleIfaces[i] = addr.Assign(vehicleDevices[i]);
    }

    addr.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIfaces = addr.Assign(internetDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---------- TCP sink on the traffic server ----------
    uint16_t basePort = 5000;
    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;

    for (uint32_t i = 0; i < kNumVehicles; ++i)
    {
        uint16_t port = basePort + i;
        Address sinkAddr(InetSocketAddress(internetIfaces.GetAddress(1), port));

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
        ApplicationContainer sink = sinkHelper.Install(server.Get(0));
        sink.Start(Seconds(0.0));
        sink.Stop(Seconds(simTime));
        sinkApps.Add(sink);

        BulkSendHelper source("ns3::TcpSocketFactory", sinkAddr);
        source.SetAttribute("MaxBytes", UintegerValue(0)); // send for the whole sim
        source.SetAttribute("SendSize", UintegerValue(tcpPacketSize));
        ApplicationContainer src = source.Install(vehicles.Get(i));
        src.Start(Seconds(1.0));
        src.Stop(Seconds(simTime));
        sourceApps.Add(src);

        // Hook up cwnd tracing just after this vehicle's socket is created
        Simulator::Schedule(Seconds(1.001), &TraceCwnd, variantName, vehicles.Get(i)->GetId());
    }

    // ---------- FlowMonitor ----------
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // ---------- Print per-flow statistics + fairness ----------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n===== Results (TCP variant = " << variantName << ") =====\n";

    std::vector<double> throughputs; // for Jain's fairness index

    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        double duration = flow.second.timeLastRxPacket.GetSeconds() -
                           flow.second.timeFirstTxPacket.GetSeconds();
        double throughputMbps = 0.0;
        if (duration > 0)
            throughputMbps = (flow.second.rxBytes * 8.0) / duration / 1e6;

        double avgDelayMs = 0.0;
        if (flow.second.rxPackets > 0)
            avgDelayMs = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) * 1000.0;

        uint64_t lost = flow.second.txPackets - flow.second.rxPackets;
        double lossPct = (flow.second.txPackets > 0)
                              ? (100.0 * lost / flow.second.txPackets)
                              : 0.0;

        std::cout << "\nFlow " << flow.first << ": " << t.sourceAddress
                  << " -> " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets:  " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets:  " << flow.second.rxPackets << "\n";
        std::cout << "  Packet Loss: " << lost << " (" << lossPct << " %)\n";
        std::cout << "  Throughput:  " << throughputMbps << " Mbps\n";
        std::cout << "  Avg Delay:   " << avgDelayMs << " ms\n";

        // Only count the vehicle->server data flows (source port >= basePort)
        // towards fairness -- this filters out any reverse-direction ACK flows.
        if (t.destinationPort >= basePort && t.destinationPort < basePort + kNumVehicles)
        {
            throughputs.push_back(throughputMbps);
        }
    }

    // ---------- Jain's fairness index over the 5 vehicle flows ----------
    if (!throughputs.empty())
    {
        double sum = 0.0, sumSq = 0.0;
        for (double x : throughputs)
        {
            sum += x;
            sumSq += x * x;
        }
        double n = static_cast<double>(throughputs.size());
        double fairness = (sum * sum) / (n * sumSq);
        std::cout << "\nJain's Fairness Index (" << variantName << "): " << fairness << "\n";
    }

    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // ---------- Run once for each TCP congestion-control variant ----------
    std::vector<std::pair<std::string, std::string>> variants = {
        {"ns3::TcpNewReno", "Reno"},
        {"ns3::TcpCubic", "Cubic"},
        {"ns3::TcpBbr", "BBR"}};

    for (auto const &v : variants)
    {
        RunSimulation(v.first, v.second);
    }

    return 0;
}

/*
 * ---------------------------------------------------------------------
 * Why low delay and reliable communication matter for ITS (for the report)
 * ---------------------------------------------------------------------
 * - Safety-critical timing: road-condition and camera data (e.g. sudden
 *   braking, obstacle detection) must reach the traffic server and other
 *   vehicles fast enough to act on -- high delay can turn a preventable
 *   collision into an unavoidable one.
 * - Traffic efficiency: signal timing, congestion rerouting, and platooning
 *   decisions are only useful if they're based on current, not stale, data.
 * - Reliability: lost updates can mean a hazard is never reported, or a
 *   vehicle acts on outdated information -- so packet loss directly
 *   translates into safety and coordination risk, not just a slower app.
 * - This is why the choice of TCP variant matters here: Reno/Cubic prioritize
 *   throughput but react to loss as a congestion signal (potentially adding
 *   delay via retransmission), while BBR models the path's bandwidth and RTT
 *   directly, often achieving lower queuing delay -- a desirable property for
 *   latency-sensitive ITS traffic.
 * ---------------------------------------------------------------------
 */