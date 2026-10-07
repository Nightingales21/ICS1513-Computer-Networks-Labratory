#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <sstream>
#include <iostream>

using namespace ns3;

int main() {
    Time::SetResolution(Time::NS);

    // Change this variable (1.0, 5.0, 10.0) for your different test cases
    double nodeSpeed = 1.0; 

    NodeContainer wifiNodes, btNodes, zbNodes;
    wifiNodes.Create(2);
    btNodes.Create(2);
    zbNodes.Create(2);

    // Fixed Gateways
    MobilityHelper mobilityFixed;
    mobilityFixed.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityFixed.Install(wifiNodes.Get(0));
    mobilityFixed.Install(btNodes.Get(0));
    mobilityFixed.Install(zbNodes.Get(0));

    // Mobile Nodes
    MobilityHelper mobilityMobile;
    std::ostringstream speedStr;
    speedStr << "ns3::ConstantRandomVariable[Constant=" << nodeSpeed << "]";
    mobilityMobile.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                    "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                    "Distance", DoubleValue(20),
                                    "Speed", StringValue(speedStr.str()));
    
    mobilityMobile.Install(wifiNodes.Get(1));
    mobilityMobile.Install(btNodes.Get(1));
    mobilityMobile.Install(zbNodes.Get(1));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // A. Wi-Fi (Standard Power)
    YansWifiPhyHelper phyWifi = YansWifiPhyHelper();
    phyWifi.SetChannel(channel.Create());
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    NetDeviceContainer wifiDevs = wifi.Install(phyWifi, mac, wifiNodes);

    // B. Bluetooth Emulated (Low Power, Short Range)
    YansWifiPhyHelper phyBt = YansWifiPhyHelper();
    phyBt.SetChannel(channel.Create());
    phyBt.Set("TxPowerStart", DoubleValue(-10.0));
    phyBt.Set("TxPowerEnd", DoubleValue(-10.0));
    WifiHelper wifiBt;
    wifiBt.SetStandard(WIFI_STANDARD_80211b);
    NetDeviceContainer btDevs = wifiBt.Install(phyBt, mac, btNodes);

    // C. Zigbee Emulated (Very Low Power, Sensitive to Movement)
    YansWifiPhyHelper phyZb = YansWifiPhyHelper();
    phyZb.SetChannel(channel.Create());
    phyZb.Set("TxPowerStart", DoubleValue(-20.0)); 
    phyZb.Set("TxPowerEnd", DoubleValue(-20.0));
    WifiHelper wifiZb;
    wifiZb.SetStandard(WIFI_STANDARD_80211b);
    NetDeviceContainer zbDevs = wifiZb.Install(phyZb, mac, zbNodes);

    // Trace Signal Strength
    phyWifi.EnableAsciiAll("campus-signal");

    InternetStackHelper stack;
    stack.Install(wifiNodes);
    stack.Install(btNodes);
    stack.Install(zbNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiIf = address.Assign(wifiDevs);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer btIf = address.Assign(btDevs);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer zbIf = address.Assign(zbDevs);

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps;
    serverApps.Add(server.Install(wifiNodes.Get(0)));
    serverApps.Add(server.Install(btNodes.Get(0)));
    serverApps.Add(server.Install(zbNodes.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper clientWifi(wifiIf.GetAddress(0), port);
    clientWifi.SetAttribute("MaxPackets", UintegerValue(100));
    clientWifi.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    clientWifi.SetAttribute("PacketSize", UintegerValue(512));
    
    UdpEchoClientHelper clientBt(btIf.GetAddress(0), port);
    clientBt.SetAttribute("MaxPackets", UintegerValue(100));
    clientBt.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    clientBt.SetAttribute("PacketSize", UintegerValue(512));

    UdpEchoClientHelper clientZb(zbIf.GetAddress(0), port);
    clientZb.SetAttribute("MaxPackets", UintegerValue(100));
    clientZb.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    clientZb.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    clientApps.Add(clientWifi.Install(wifiNodes.Get(1)));
    clientApps.Add(clientBt.Install(btNodes.Get(1)));
    clientApps.Add(clientZb.Install(zbNodes.Get(1)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Calculate and Print Metrics directly to terminal
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n--- Simulation Results (Speed: " << nodeSpeed << " m/s) ---\n";
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        std::string techName = "Unknown";
        if (t.sourceAddress == "10.1.1.2") techName = "Wi-Fi";
        else if (t.sourceAddress == "10.1.2.2") techName = "Bluetooth (Emulated)";
        else if (t.sourceAddress == "10.1.3.2") techName = "Zigbee (Emulated)";
        else continue; // Skip return echo flows to avoid cluttering output

        double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024;
        double packetLoss = ((i->second.txPackets - i->second.rxPackets) * 100.0) / i->second.txPackets;
        double delay = i->second.rxPackets > 0 ? (i->second.delaySum.GetSeconds() / i->second.rxPackets) * 1000 : 0;

        std::cout << "\nTechnology: " << techName << "\n";
        std::cout << "  Throughput:   " << throughput << " Kbps\n";
        std::cout << "  Packet Loss:  " << packetLoss << " %\n";
        std::cout << "  Avg Delay:    " << delay << " ms\n";
    }
    std::cout << "------------------------------------------\n\n";

    Simulator::Destroy();
    return 0;
}