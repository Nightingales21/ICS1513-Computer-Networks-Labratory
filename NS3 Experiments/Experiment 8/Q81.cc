/*
 * Simulation and Performance Analysis of TCP and UDP Traffic Using NS-3
 *
 * Topology (dumbbell):
 *
 *        10Mbps,2ms          10Mbps,2ms
 *   n0 ------------- r1 ==bottleneck== r2 ------------- n3
 *                     |                 |
 *   n1 --------------- (10Mbps,2ms)     |
 *                                       n2 (10Mbps,2ms from r2)
 *
 *   TCP flow: n0 -> n3
 *   UDP flow: n1 -> n2
 *
 * Experiment 1 (bottleneck bandwidth) is run automatically: the whole
 * simulation is wrapped in a function and executed once for each
 * bandwidth in the list {2Mbps, 5Mbps, 10Mbps}. Results for all three
 * runs are printed one after another.
 *
 * Usage:
 *   ./ns3 run tcp-udp-sim
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpUdpAssignment");

void RunSimulation(std::string bottleneckBw)
{
    // ---------- Parameters (defaults match the assignment) ----------
    std::string bottleneckDelay = "10ms";
    std::string accessBw = "10Mbps";
    std::string accessDelay = "2ms";
    uint32_t queueSizePkts = 50;
    double simTime = 20.0;
    uint32_t tcpPacketSize = 1024;
    uint32_t udpPacketSize = 512;
    std::string udpRate = "1Mbps";

    // ---------- Nodes ----------
    NodeContainer n0, n1, n2, n3, routers;
    n0.Create(1);
    n1.Create(1);
    n2.Create(1);
    n3.Create(1);
    routers.Create(2); // routers.Get(0) = r1, routers.Get(1) = r2

    NodeContainer n0r1(n0.Get(0), routers.Get(0));
    NodeContainer n1r1(n1.Get(0), routers.Get(0));
    NodeContainer r1r2(routers.Get(0), routers.Get(1));
    NodeContainer r2n2(routers.Get(1), n2.Get(0));
    NodeContainer r2n3(routers.Get(1), n3.Get(0));

    // ---------- Point-to-point links ----------
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessBw));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBw));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    // DropTail queue with the requested size (DropTailQueue is the ns-3 default)
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>",
                            "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSizePkts)));

    NetDeviceContainer d_n0r1 = accessLink.Install(n0r1);
    NetDeviceContainer d_n1r1 = accessLink.Install(n1r1);
    NetDeviceContainer d_r1r2 = bottleneckLink.Install(r1r2);
    NetDeviceContainer d_r2n2 = accessLink.Install(r2n2);
    NetDeviceContainer d_r2n3 = accessLink.Install(r2n3);

    // ---------- Internet stack ----------
    InternetStackHelper stack;
    stack.InstallAll();

    // ---------- IP addressing ----------
    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0r1 = addr.Assign(d_n0r1);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1r1 = addr.Assign(d_n1r1);

    addr.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1r2 = addr.Assign(d_r1r2);

    addr.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r2n2 = addr.Assign(d_r2n2);

    addr.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r2n3 = addr.Assign(d_r2n3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---------- TCP flow: n0 -> n3 ----------
    uint16_t tcpPort = 5000;
    Address tcpSinkAddr(InetSocketAddress(i_r2n3.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddr);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(n3.Get(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    BulkSendHelper tcpSource("ns3::TcpSocketFactory", tcpSinkAddr);
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0)); // send for the whole sim time
    tcpSource.SetAttribute("SendSize", UintegerValue(tcpPacketSize));
    ApplicationContainer tcpApp = tcpSource.Install(n0.Get(0));
    tcpApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(simTime));

    // ---------- UDP flow: n1 -> n2 ----------
    uint16_t udpPort = 6000;
    Address udpSinkAddr(InetSocketAddress(i_r2n2.GetAddress(1), udpPort));
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", udpSinkAddr);
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(n2.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udpSource("ns3::UdpSocketFactory", udpSinkAddr);
    udpSource.SetAttribute("DataRate", StringValue(udpRate));
    udpSource.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    udpSource.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpSource.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer udpApp = udpSource.Install(n1.Get(0));
    udpApp.Start(Seconds(1.0));
    udpApp.Stop(Seconds(simTime));

    // ---------- Tracing ----------
    AsciiTraceHelper ascii;
    std::string tag = "assignment_" + bottleneckBw;
    accessLink.EnableAsciiAll(ascii.CreateFileStream(tag + ".tr"));
    accessLink.EnablePcapAll(tag);
    bottleneckLink.EnablePcap(tag + "-bottleneck", d_r1r2, true);

    // ---------- FlowMonitor ----------
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // ---------- Print per-flow statistics ----------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n===== Results (bottleneck = " << bottleneckBw << ") =====\n";
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::string proto = (t.protocol == 6) ? "TCP" : (t.protocol == 17) ? "UDP"
                                                                           : "OTHER";

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

        std::cout << "\nFlow " << flow.first << " (" << proto << ") "
                  << t.sourceAddress << " -> " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets:        " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets:        " << flow.second.rxPackets << "\n";
        std::cout << "  Packet Loss:       " << lost << " (" << lossPct << " %)\n";
        std::cout << "  Throughput:        " << throughputMbps << " Mbps\n";
        std::cout << "  Avg End-to-End Delay: " << avgDelayMs << " ms\n";

        if (proto == "TCP")
        {
            // Simple retransmission-rate estimate: every lost original packet
            // is eventually retransmitted by TCP, so lost packets are a
            // reasonable proxy for the number of retransmissions.
            std::cout << "  Approx. TCP Retransmissions: " << lost
                      << " (" << lossPct << " % of packets sent)\n";
        }
    }

    Simulator::Destroy();
}

int main()
{
    std::vector<std::string> bottleneckRates = {"2Mbps", "5Mbps", "10Mbps"};

    for (const std::string &bw : bottleneckRates)
    {
        RunSimulation(bw);
    }

    return 0;
}