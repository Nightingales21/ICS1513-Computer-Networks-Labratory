/*
 * Assignment 10 - Comparative evaluation of RIP, OSPF and AODV in NS-3
 *
 * Topology (all links point-to-point):
 *
 *   S0..S3 --100M/1ms--> R0 ==== R1 ==== R4 <--100M/1ms-- D0..D3
 *   (Dept A)              \                /               (Dept B)
 *                          R2 ==== R3 ====
 *
 *   Primary path : R0-R1-R4         (2 core hops)
 *   Backup path  : R0-R2-R3-R4      (3 core hops)
 *   All core links: 2 Mbps, 10 ms.  The R1-R4 link is the one that fails.
 *
 * Scenarios (--scenario=):
 *   normal     : 2 CBR/UDP flows (0.8 Mbps total, below bottleneck capacity)
 *   congestion : 8 CBR/UDP flows (3.2 Mbps total > 2 Mbps bottleneck)
 *   failure    : same load as normal, link R1-R4 goes down at t = failTime
 *
 * Protocols (--proto=):
 *   rip   : ns3::Rip on routers, static default routes on hosts
 *   ospf  : link-state routing using ns-3 global routing (SPF/Dijkstra over the
 *           full topology database). ns-3 has no native OSPF, so this is used as
 *           an OSPF-equivalent; after a failure the SPF recomputation is
 *           triggered after --ospfDelay seconds (failure detection + LSA flooding
 *           + SPF hold time).
 *   aodv  : ns3::aodv on every node (reactive, on-demand route discovery)
 *
 * Outputs (in --outDir):
 *   summary_<proto>_<scenario>.csv : PDR, mean delay, throughput
 *   rt_<proto>_<scenario>.txt      : routing-table snapshots of every node,
 *                                    taken every --snap seconds (used by
 *                                    plot_results.py to count table updates)
 */

/*
 * Assignment 10 - Comparative evaluation of RIP, OSPF and AODV in NS-3
 *
 * Topology (all links point-to-point):
 *
 *   S0..S3 --100M/1ms--> R0 ==== R1 ==== R4 <--100M/1ms-- D0..D3
 *   (Dept A)              \                /               (Dept B)
 *                          R2 ==== R3 ====
 *
 *   Primary path : R0-R1-R4         (2 core hops)
 *   Backup path  : R0-R2-R3-R4      (3 core hops)
 *   All core links: 2 Mbps, 10 ms.  The R1-R4 link is the one that fails.
 */

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingComparison");

static const uint16_t BASE_PORT = 9000;

static void
FailLink(Ptr<Ipv4> a, uint32_t ia, Ptr<Ipv4> b, uint32_t ib)
{
    a->SetDown(ia);
    b->SetDown(ib);
    NS_LOG_UNCOND("t=" << Simulator::Now().GetSeconds() << "s : link R1-R4 DOWN");
}

int
main()
{
    // Hardcoded configuration parameters (previously parsed via CommandLine)
    std::string proto = "rip";       // rip | ospf | aodv
    std::string scenario = "normal"; // normal | congestion | failure
    std::string outDir = "results";
    std::string rate = "400kbps";    // CBR rate per flow
    uint32_t pktSize = 1000;         // bytes
    uint32_t nFlowsNormal = 2;
    uint32_t nFlowsCongestion = 8;
    double appStart = 10.0;          // allow RIP/OSPF to converge first
    double appStop = 40.0;
    double failTime = 25.0;
    double simStop = 45.0;
    double ospfDelay = 0.5;          // s, OSPF detection + flooding + SPF delay
    double snap = 0.25;              // s, routing-table snapshot interval
    uint32_t seed = 1;

    if (proto != "rip" && proto != "ospf" && proto != "aodv")
    {
        NS_FATAL_ERROR("proto must be rip, ospf or aodv");
    }
    if (scenario != "normal" && scenario != "congestion" && scenario != "failure")
    {
        NS_FATAL_ERROR("scenario must be normal, congestion or failure");
    }

    RngSeedManager::SetSeed(seed);
    uint32_t nFlows = (scenario == "congestion") ? nFlowsCongestion : nFlowsNormal;
    bool doFail = (scenario == "failure");
    std::string tag = proto + "_" + scenario;

    // ---- protocol parameters (faster timers so convergence fits the run) ----
    Config::SetDefault("ns3::Rip::UnsolicitedRoutingUpdate", TimeValue(Seconds(5)));
    Config::SetDefault("ns3::Rip::TimeoutDelay", TimeValue(Seconds(15)));
    Config::SetDefault("ns3::Rip::GarbageCollectionDelay", TimeValue(Seconds(10)));
    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(false));

    // ---- nodes ----
    NodeContainer S, D, R;
    S.Create(4); // Department A sources
    D.Create(4); // Department B destinations
    R.Create(5); // R0 ingress, R1 primary mid, R2/R3 backup, R4 egress
    NodeContainer hosts(S, D);
    NodeContainer all(hosts, R);

    // ---- internet stack / routing ----
    InternetStackHelper stack;
    if (proto == "aodv")
    {
        AodvHelper aodv;
        aodv.Set("HelloInterval", TimeValue(Seconds(1)));
        stack.SetRoutingHelper(aodv);
        stack.Install(all);
    }
    else if (proto == "rip")
    {
        RipHelper rip;
        Ipv4StaticRoutingHelper staticRh;
        Ipv4ListRoutingHelper list;
        list.Add(staticRh, 0);
        list.Add(rip, 10);
        stack.SetRoutingHelper(list);
        stack.Install(R);
        InternetStackHelper hostStack; // default static (+global, unused) routing
        hostStack.Install(hosts);
    }
    else // ospf (link-state / global SPF)
    {
        stack.Install(all);
    }

    // ---- links ----
    PointToPointHelper access, core;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", StringValue("1ms"));
    core.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    core.SetChannelAttribute("Delay", StringValue("10ms"));cd

    Ipv4AddressHelper addr;
    uint32_t subnet = 1;
    auto assign = [&](NetDeviceContainer d) {
        std::ostringstream ss;
        ss << "10.1." << subnet++ << ".0";
        addr.SetBase(ss.str().c_str(), "255.255.255.0");
        return addr.Assign(d);
    };

    std::vector<Ipv4Address> dstAddr(4);
    Ipv4StaticRoutingHelper hostRouting;

    for (uint32_t i = 0; i < 4; ++i)
    {
        Ipv4InterfaceContainer ic = assign(access.Install(S.Get(i), R.Get(0)));
        if (proto == "rip")
        {
            hostRouting.GetStaticRouting(S.Get(i)->GetObject<Ipv4>())
                ->SetDefaultRoute(ic.GetAddress(1), 1);
        }
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
        Ipv4InterfaceContainer ic = assign(access.Install(D.Get(i), R.Get(4)));
        dstAddr[i] = ic.GetAddress(0);
        if (proto == "rip")
        {
            hostRouting.GetStaticRouting(D.Get(i)->GetObject<Ipv4>())
                ->SetDefaultRoute(ic.GetAddress(1), 1);
        }
    }

    // primary path R0-R1-R4
    assign(core.Install(R.Get(0), R.Get(1)));
    NetDeviceContainer r14 = core.Install(R.Get(1), R.Get(4));
    assign(r14);
    // backup path R0-R2-R3-R4
    assign(core.Install(R.Get(0), R.Get(2)));
    assign(core.Install(R.Get(2), R.Get(3)));
    assign(core.Install(R.Get(3), R.Get(4)));

    if (proto == "ospf")
    {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    // ---- traffic: CBR/UDP flows, flow i : S[i%4] -> D[i%4], port 9000+i ----
    for (uint32_t i = 0; i < nFlows; ++i)
    {
        uint32_t s = i % 4;
        uint32_t d = i % 4;
        uint16_t port = BASE_PORT + i;

        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sa = sink.Install(D.Get(d));
        sa.Start(Seconds(0.0));
        sa.Stop(Seconds(simStop));

        OnOffHelper on("ns3::UdpSocketFactory", InetSocketAddress(dstAddr[d], port));
        on.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        on.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        on.SetConstantRate(DataRate(rate), pktSize);
        ApplicationContainer ca = on.Install(S.Get(s));
        ca.Start(Seconds(appStart + 0.1 * i));
        ca.Stop(Seconds(appStop));
    }

    // ---- link failure (R1-R4) ----
    if (doFail)
    {
        Ptr<Ipv4> ip1 = R.Get(1)->GetObject<Ipv4>();
        Ptr<Ipv4> ip4 = R.Get(4)->GetObject<Ipv4>();
        uint32_t if1 = ip1->GetInterfaceForDevice(r14.Get(0));
        uint32_t if4 = ip4->GetInterfaceForDevice(r14.Get(1));
        Simulator::Schedule(Seconds(failTime), &FailLink, ip1, if1, ip4, if4);
        if (proto == "ospf")
        {
            Simulator::Schedule(Seconds(failTime + ospfDelay),
                                &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);
        }
        // RIP reacts to the interface-down event itself (triggered update);
        // AODV reacts through RERR / Hello loss.
    }

    // ---- routing table snapshots (post-processed to count updates) ----
    Ptr<OutputStreamWrapper> rtStream =
        Create<OutputStreamWrapper>(outDir + "/rt_" + tag + ".txt", std::ios::out);
    Ipv4RoutingHelper::PrintRoutingTableAllEvery(Seconds(snap), rtStream, Time::S);

    // ---- flow monitor ----
    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> monitor = fmh.InstallAll();

    Simulator::Stop(Seconds(simStop));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> cls = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    uint64_t tx = 0, rx = 0, rxBytes = 0;
    double delaySum = 0;
    for (const auto& kv : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = cls->FindFlow(kv.first);
        if (t.destinationPort < BASE_PORT || t.destinationPort >= BASE_PORT + 100)
        {
            continue; // ignore control traffic (RIP 520, AODV 654, ...)
        }
        tx += kv.second.txPackets;
        rx += kv.second.rxPackets;
        rxBytes += kv.second.rxBytes;
        delaySum += kv.second.delaySum.GetSeconds();
    }
    double pdr = tx ? 100.0 * rx / tx : 0.0;
    double delayMs = rx ? 1000.0 * delaySum / rx : 0.0;
    double thr = rxBytes * 8.0 / (appStop - appStart) / 1e6;

    std::ofstream out(outDir + "/summary_" + tag + ".csv");
    out << "protocol,scenario,flows,tx_packets,rx_packets,pdr_percent,avg_delay_ms,throughput_mbps\n";
    out << proto << "," << scenario << "," << nFlows << "," << tx << "," << rx << ","
        << std::fixed << std::setprecision(3) << pdr << "," << delayMs << "," << thr << "\n";
    out.close();

    std::cout << "[" << tag << "] flows=" << nFlows << " tx=" << tx << " rx=" << rx
              << " PDR=" << pdr << "% delay=" << delayMs << "ms thr=" << thr << "Mbps\n";

    Simulator::Destroy();
    return 0;
}