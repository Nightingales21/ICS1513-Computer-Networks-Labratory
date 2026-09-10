#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleNetwork");

int main()
{
    NodeContainer nodes;
    nodes.Create(2);

    // 1. Set up point-to-point link BEFORE installing devices
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // 2. Install internet stack (needed before assigning IP addresses)
    InternetStackHelper stack;
    stack.Install(nodes);

    // 3. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // interfaces.GetAddress(0) -> node 0's IP
    // interfaces.GetAddress(1) -> node 1's IP

    // 4. UDP sink (receiver) on node 1
    uint16_t udpPort = 9;
    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer sinkApp = udpSink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // 5. UDP sender (OnOff) on node 0, sending to node 1's address
    Address udpSinkAddress(InetSocketAddress(interfaces.GetAddress(1), udpPort));

    OnOffHelper udpSender("ns3::UdpSocketFactory", udpSinkAddress);
    udpSender.SetAttribute("PacketSize", UintegerValue(512));
    udpSender.SetAttribute("DataRate", StringValue("1Mbps"));
    udpSender.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpSender.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer udpApp = udpSender.Install(nodes.Get(0));
    udpApp.Start(Seconds(1.0));
    udpApp.Stop(Seconds(20.0));

    // 6. Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}