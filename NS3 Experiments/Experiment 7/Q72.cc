/*
 * ================================================================
 *  udp-two-ports-loop.cc
 * ================================================================
 * Same experiment as before (2 nodes, 1 p2p link, UdpClient/UdpServer
 * on ports 5000 and 6000) BUT this version loops through several
 * latency values AUTOMATICALLY in one single run, and prints both
 * result tables for you at the end. No need to re-run the command
 * by hand for every latency.
 *
 * HOW TO RUN:
 *   ./ns3 run scratch/udp-two-ports-loop
 *
 * That's it — one command gives you every row of both tables.
 * ================================================================
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpTwoPortsLoop");

// ---------------------------------------------------------------
// This function builds the topology, runs ONE simulation for a
// given delay, and returns the throughput of flow1 (and flow2 if
// enabled). We call this function once per latency value in a loop.
// ---------------------------------------------------------------
void RunOnce (std::string delay,
              bool enableFlow2,
              double &throughput1Out,
              double &throughput2Out)
{
  std::string linkBandwidth = "100Mbps";
  std::string dataRate1     = "5Mbps";
  std::string dataRate2     = "5Mbps";
  uint32_t    packetSize    = 1024;
  double      simTime       = 5.0;

  // STEP 1: topology
  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (linkBandwidth));
  p2p.SetChannelAttribute ("Delay", StringValue (delay)); // <- latency for this iteration

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  Ipv4Address node2Address = interfaces.GetAddress (1);

  // STEP 2: flow 1, port 5000
  uint16_t port1 = 5000;
  UdpServerHelper server1 (port1);
  ApplicationContainer serverApps1 = server1.Install (nodes.Get (1));
  serverApps1.Start (Seconds (0.0));
  serverApps1.Stop  (Seconds (simTime + 1.0));

  DataRate rate1 (dataRate1);
  double interval1 = (packetSize * 8.0) / rate1.GetBitRate ();
  uint32_t maxPackets1 = (uint32_t) ((simTime / interval1) + 10);

  UdpClientHelper client1 (node2Address, port1);
  client1.SetAttribute ("MaxPackets", UintegerValue (maxPackets1));
  client1.SetAttribute ("Interval",   TimeValue (Seconds (interval1)));
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps1 = client1.Install (nodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop  (Seconds (simTime + 1.0));

  // STEP 3: flow 2, port 6000 (only if enabled)
  ApplicationContainer serverApps2;
  uint16_t port2 = 6000;
  if (enableFlow2)
    {
      UdpServerHelper server2 (port2);
      serverApps2 = server2.Install (nodes.Get (1));
      serverApps2.Start (Seconds (0.0));
      serverApps2.Stop  (Seconds (simTime + 1.0));

      DataRate rate2 (dataRate2);
      double interval2 = (packetSize * 8.0) / rate2.GetBitRate ();
      uint32_t maxPackets2 = (uint32_t) ((simTime / interval2) + 10);

      UdpClientHelper client2 (node2Address, port2);
      client2.SetAttribute ("MaxPackets", UintegerValue (maxPackets2));
      client2.SetAttribute ("Interval",   TimeValue (Seconds (interval2)));
      client2.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApps2 = client2.Install (nodes.Get (0));
      clientApps2.Start (Seconds (1.0));
      clientApps2.Stop  (Seconds (simTime + 1.0));
    }

  // Pcap file name includes the delay so each iteration gets its own file
  std::string pcapName = "udp-two-ports-" + delay + (enableFlow2 ? "-both" : "-flow1only");
  p2p.EnablePcapAll (pcapName);

  Simulator::Stop (Seconds (simTime + 2.0));
  Simulator::Run ();

  Ptr<UdpServer> udpServer1 = DynamicCast<UdpServer> (serverApps1.Get (0));
  throughput1Out = (udpServer1->GetReceived () * packetSize * 8.0) / (simTime * 1e6);

  if (enableFlow2)
    {
      Ptr<UdpServer> udpServer2 = DynamicCast<UdpServer> (serverApps2.Get (0));
      throughput2Out = (udpServer2->GetReceived () * packetSize * 8.0) / (simTime * 1e6);
    }
  else
    {
      throughput2Out = 0.0;
    }

  // IMPORTANT: reset the simulator so the next loop iteration starts fresh
  Simulator::Destroy ();
}

int
main ()
{
  // The latency values we want to test -- add/remove values here freely
  std::vector<std::string> delays = {"2ms", "5ms", "10ms", "20ms"};

  // ---------------------------------------------------------------
  // TABLE 1: single flow (port 5000 only) -- Requirement #3
  // ---------------------------------------------------------------
  std::cout << "\n===== TABLE 1: Single UDP flow (port 5000 only) =====\n";
  std::cout << std::left << std::setw(12) << "Latency"
            << std::setw(18) << "Throughput(Mbps)" << std::endl;

  std::vector<double> table1Results;
  for (const auto &delay : delays)
    {
      double t1 = 0.0, t2 = 0.0;
      RunOnce (delay, false, t1, t2);   // enableFlow2 = false
      table1Results.push_back (t1);

      std::cout << std::left << std::setw(12) << delay
                << std::setw(18) << t1 << std::endl;
    }

  // ---------------------------------------------------------------
  // TABLE 2: both flows active (ports 5000 and 6000) -- Requirement #7
  // ---------------------------------------------------------------
  std::cout << "\n===== TABLE 2: Two UDP flows active (ports 5000 & 6000) =====\n";
  std::cout << std::left << std::setw(12) << "Latency"
            << std::setw(20) << "Flow1(Mbps)"
            << std::setw(20) << "Flow2(Mbps)" << std::endl;

  for (const auto &delay : delays)
    {
      double t1 = 0.0, t2 = 0.0;
      RunOnce (delay, true, t1, t2);    // enableFlow2 = true
      std::cout << std::left << std::setw(12) << delay
                << std::setw(20) << t1
                << std::setw(20) << t2 << std::endl;
    }

  std::cout << "\nDone. Pcap files were saved per latency/scenario "
               "(e.g. udp-two-ports-5ms-both-0-0.pcap) -- open them in "
               "Wireshark and filter udp.port==5000 or udp.port==6000.\n";

  return 0;
}

/*
 * ================================================================
 * HOW TO USE
 * ================================================================
 * 1) Copy this file into your ns-3 "scratch" folder:
 *        <ns3-folder>/scratch/udp-two-ports-loop.cc
 *
 * 2) Build:
 *        ./ns3 build
 *
 * 3) Run (single command, no flags needed):
 *        ./ns3 run scratch/udp-two-ports-loop
 *
 *    It will print two tables straight to your terminal:
 *      TABLE 1 -> Latency vs Throughput (single flow)
 *      TABLE 2 -> Latency vs Flow1 Throughput vs Flow2 Throughput
 *
 *    Just copy those numbers into your report.
 *
 * 4) Want to test different/more latencies? Edit this one line
 *    near the top of main():
 *        std::vector<std::string> delays = {"2ms", "5ms", "10ms", "20ms"};
 *    Add or remove values, e.g. {"1ms","2ms","5ms","10ms","20ms","50ms"}.
 *
 * 5) Wireshark: every latency/scenario combo gets its own pcap file,
 *    named like:
 *        udp-two-ports-5ms-both-0-0.pcap   (Node1 side)
 *        udp-two-ports-5ms-both-1-0.pcap   (Node2 side)
 *    Filter with "udp.port == 5000" or "udp.port == 6000" to verify
 *    each flow reaches the correct server.
 * ================================================================
 */