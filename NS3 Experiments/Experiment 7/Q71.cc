#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

/*core-module —cl the simulation engine itself (the clock, event scheduler, logging)
network-module — the basic building blocks: nodes, packets, network devices
internet-module — IP addressing and routing (makes nodes act like real internet hosts)
point-to-point-module — a specific type of link (a direct cable between exactly two nodes)
applications-module — traffic generators/receivers (the things that actually send and receive data)*/

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleNetwork");

int main()
{
NodeContainer nodes;
nodes.Create(4); //Create 4 Nodes

PointToPointHelper p2p;
p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps")); // 
p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    /*DataRate = 5 Mbps — how fast the cable can push bits (bandwidth)
    Delay = 2 ms — how long it takes a signal to physically travel across the cable (propagation delay), like the delay of light traveling down a fiber*/
NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
NetDeviceContainer d32 = p2p.Install(nodes.Get(3), nodes.Get(2));

InternetStackHelper internet;
internet.Install(nodes);

    //Assign IP addresess
Ipv4AddressHelper address;

address.SetBase("10.1.1.0", "255.255.255.0");
Ipv4InterfaceContainer i01 = address.Assign(d01);
address.SetBase("10.1.2.0", "255.255.255.0");
Ipv4InterfaceContainer i12 = address.Assign(d12);
address.SetBase("10.1.3.0", "255.255.255.0");
Ipv4InterfaceContainer i32 = address.Assign(d32);

    //Enable Routing
Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    //Flow n0 - n2
uint16_t port1 = 5000;
PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));

ApplicationContainer sinkApp1 = sink1.Install(nodes.Get(2));
sinkApp1.Start(Seconds(1.0));
sinkApp1.Stop(Seconds(10.0));

OnOffHelper source1("ns3::UdpSocketFactory", InetSocketAddress(i12.GetAddress(1), port1));
source1.SetAttribute("DataRate", StringValue("1Mbps"));
source1.SetAttribute("PacketSize", UintegerValue(512));

ApplicationContainer sourceApp1 = source1.Install(nodes.Get(0));
sourceApp1.Start(Seconds(2.0));
sourceApp1.Stop(Seconds(10.0));

    //Flow 2 n3 - n0: Same thing but at 500 Kbps
uint16_t port2 = 6000;
PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));

ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(0));
sinkApp2.Start(Seconds(1.0));
sinkApp2.Stop(Seconds(10.0));

OnOffHelper source2("ns3::UdpSocketFactory", InetSocketAddress(i01.GetAddress(0), port2));
source2.SetAttribute("DataRate", StringValue("500Kbps"));
source2.SetAttribute("PacketSize",UintegerValue(512));

ApplicationContainer sourceApp2 = source2.Install(nodes.Get(3));
sourceApp2.Start(Seconds(3.0));
sourceApp2.Stop(Seconds(10.0));

    //Run the Sim
Simulator::Stop(Seconds(10.0));
Simulator::Run();

    // Display received packets
Ptr<PacketSink> sinkPtr1 = DynamicCast<PacketSink>(sinkApp1.Get(0));
Ptr<PacketSink> sinkPtr2 = DynamicCast<PacketSink>(sinkApp2.Get(0));

std::cout << "Flow 1 (n0 -> n2): "
<< sinkPtr1->GetTotalRx()
<< " bytes received" << std::endl;
std::cout << "Flow 2 (n3 -> n0): "
<< sinkPtr2->GetTotalRx()
<< " bytes received" << std::endl;
Simulator::Destroy();
return 0;
}