/* 
 Network topology

        n0                                             n5
         \                                            /
          \ 10.0.1.0                                 / 10.1.1.0
           \                                        /
   10.0.2.0 \               192.168.1.0            /  10.1.2.0
 n1--------- Router n3---------------------Router n4----------n6
            /                                      \
           /                                        \
          /  10.0.3.0                                \ 10.1.3.0
         /                                            \
       n2                                             n7

 - Flow from n0 to n7 using BulkSendApplication.
 - Flow from n1 to n6 using BulkSendApplication.
 - Flow from n2 to n5 using BulkSendApplication.

 - Tracing of queues and packet receptions to file "*.tr" and
   "*.pcap" when tracing is turned on.
*/

// System includes.
#include <string>
#include <fstream>
#include <map>
// NS3 includes.
#include "ns3/flow-monitor-module.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-global-routing-helper.h"


using namespace ns3;

// Constants.

#define ENABLE_PCAP      true     // Set to "true" to enable pcap
#define ENABLE_TRACE     true     // Set to "true" to enable trace
#define BIG_QUEUE        2000      // Packets
#define QUEUE_SIZE       100       // Packets
#define START_TIME       0.0       // Seconds
#define STOP_TIME        2000.0    // Seconds
#define R_TO_R_BW        "10Mbps" // Router to router
#define R_TO_R_DELAY     "20ms"
#define S_TO_R_BW        "8Mbps" // Server to router
#define S_TO_R_DELAY     "10ms"
#define R_TO_C_BW        "8Mbps"  // Router to client 
#define R_TO_C_DELAY     "1ms"

#define PACKET_SIZE      1000   
//Bytes, Classic data size, set as 2*n_packet. The real packet size is PACKET_SIZE+HEADER_SIZE+QED_SIZE 
#define HEADER_SIZE       16 //Quantum header size, bytes,  Integer multiple of 16!
#define RED_SIZE     24 //Redundancy length.   Bytes  =2*n_red
#define error_p      0.0//lost packet probability.

#define MAX_BYTES  10000000//Bytes to send. 
// Uncomment one of the below.
//#define TCP_PROTOCOL     "ns3::TcpNewReno"
#define TCP_PROTOCOL     "ns3::TcpNewReno"
//#define TCP_PROTOCOL     "ns3::TcpNewReno"，"ns3::TcpBbr"
#define DATA_RETRIES 20
//Retransmission Upper Bound
// For logging. 

NS_LOG_COMPONENT_DEFINE ("main");
/////////////////////////////////////////////////
int main (int argc, char *argv[]) {

  /////////////////////////////////////////
  // Turn on logging for this script.
  // Note: for BBR', other components that may be
  // of interest include "TcpBbr" and "BbrState".
  LogComponentEnable("main", LOG_LEVEL_INFO);
  CommandLine cmd;
  cmd.Parse (argc, argv);
  /////////////////////////////////////////
  // Setup environment
  Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                     StringValue(TCP_PROTOCOL));
  // Report parameters.
  NS_LOG_INFO("TCP protocol: " << TCP_PROTOCOL);
  NS_LOG_INFO("Server to Router Bwdth: " << S_TO_R_BW);
  NS_LOG_INFO("Server to Router Delay: " << S_TO_R_DELAY);
  NS_LOG_INFO("Router to Client Bwdth: " << R_TO_C_BW);
  NS_LOG_INFO("Router to Client Delay: " << R_TO_C_DELAY);
  NS_LOG_INFO("n_packet (qbytes): " << PACKET_SIZE/2);
  NS_LOG_INFO("Quantum header size (bytes): " << HEADER_SIZE);
  NS_LOG_INFO("n_red (qbytes): " << HEADER_SIZE/2);
  NS_LOG_INFO("Qubit data size (qbytes): " << MAX_BYTES/2);
  NS_LOG_INFO("Lost probability: " << error_p);
  // Set real segment size (otherwise, ns-3 default is 536).
  Config::SetDefault("ns3::TcpSocket::SegmentSize",
                     UintegerValue(PACKET_SIZE+HEADER_SIZE+RED_SIZE)); 
  Config::SetDefault("ns3::TcpTxBuffer::HEADERSIZE",
                     UintegerValue(HEADER_SIZE)); 
  Config::SetDefault("ns3::TcpTxBuffer::REDSIZE",
                     UintegerValue(RED_SIZE)); 

  // Turn off delayed ack (so, acks every packet).
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
  Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(DATA_RETRIES));
  Config::SetDefault("ns3::TcpSocketBase::MinRto", StringValue("50000000ns"));

  /////////////////////////////////////////
  // Create nodes.
  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;  
  nodes.Create(8);

  /////////////////////////////////////////
  // Create channels.
  NS_LOG_INFO("Creating channels.");
  NodeContainer n03 = NodeContainer(nodes.Get(0), nodes.Get(3));
  NodeContainer n13 = NodeContainer(nodes.Get(1), nodes.Get(3));
  NodeContainer n23 = NodeContainer(nodes.Get(2), nodes.Get(3));
  NodeContainer n34 = NodeContainer(nodes.Get(3), nodes.Get(4));
  NodeContainer n45 = NodeContainer(nodes.Get(4), nodes.Get(5));
  NodeContainer n46 = NodeContainer(nodes.Get(4), nodes.Get(6));
  NodeContainer n47 = NodeContainer(nodes.Get(4), nodes.Get(7));
 


  /////////////////////////////////////
  //Create error model.
  RngSeedManager::SetSeed(3);
  Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (error_p));
  Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
  Config::SetDefault ("ns3::RateErrorModel::RanVar", StringValue ("ns3::UniformRandomVariable[Min=0|Max=1]"));
  ObjectFactory factory;
  factory.SetTypeId ("ns3::RateErrorModel");
  Ptr<ErrorModel> error_model = factory.Create<ErrorModel> ();


 /////////////////////////////////////////
  // Create links.
  NS_LOG_INFO("Creating links.");
  // Server to Router.
  int mtu = 1500;
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute("DataRate", StringValue (S_TO_R_BW));
  p2p1.SetChannelAttribute("Delay", StringValue (S_TO_R_DELAY));
  p2p1.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NetDeviceContainer devices1 = p2p1.Install(n03);
  NetDeviceContainer devices2 = p2p1.Install(n13);
  NetDeviceContainer devices3 = p2p1.Install(n23);
  // Router to Router.
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", StringValue (R_TO_R_BW));
  p2p2.SetChannelAttribute("Delay", StringValue (R_TO_R_DELAY));
  p2p2.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  p2p2.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (error_model));
  NetDeviceContainer devices4 = p2p2.Install(n34);
 

  // Router to Client.
  PointToPointHelper p2p3;
  p2p3.SetDeviceAttribute("DataRate", StringValue (R_TO_C_BW));
  p2p3.SetChannelAttribute("Delay", StringValue (R_TO_C_DELAY));
  p2p3.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NS_LOG_INFO("Router queue size: "<< QUEUE_SIZE);
  p2p3.SetQueue("ns3::DropTailQueue",
               "Mode", StringValue ("QUEUE_MODE_PACKETS"),
               "MaxPackets", UintegerValue(QUEUE_SIZE));
  NetDeviceContainer devices5 = p2p1.Install(n45);
  NetDeviceContainer devices6 = p2p1.Install(n46);
  NetDeviceContainer devices7 = p2p1.Install(n47);

  /////////////////////////////////////////
  // Install Internet stack.
  NS_LOG_INFO("Installing Internet stack.");
  InternetStackHelper internet;
  internet.Install(nodes);
  
  /////////////////////////////////////////
  // Add IP addresses.
  NS_LOG_INFO("Assigning IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i3 = ipv4.Assign(devices1);
  ipv4.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i3 = ipv4.Assign(devices2);
  ipv4.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign(devices3);
  ipv4.SetBase("191.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign(devices4);
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = ipv4.Assign(devices5);
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i6 = ipv4.Assign(devices6);
  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i7 = ipv4.Assign(devices7);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  /////////////////////////////////////////
  // Create apps.
  NS_LOG_INFO("Creating applications.");
  //NS_LOG_INFO("  Bulk send.");

  // Well-known port for server.
  uint16_t port = 911;  

  // Source (at node 0,1,2).

  //BulkSendHelper
   BulkSendHelper source1("ns3::TcpSocketFactory",
                        InetSocketAddress(i4i7.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  source1.SetAttribute("MaxBytes", UintegerValue(MAX_BYTES));
  source1.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
  ApplicationContainer apps1 = source1.Install(nodes.Get(0));
  apps1.Start(Seconds(START_TIME));
  apps1.Stop(Seconds(STOP_TIME));

  BulkSendHelper source2("ns3::TcpSocketFactory",
                        InetSocketAddress(i4i6.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  source2.SetAttribute("MaxBytes", UintegerValue(MAX_BYTES));
  source2.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
  ApplicationContainer apps2 = source2.Install(nodes.Get(1));
  apps2.Start(Seconds(START_TIME+5));
  apps2.Stop(Seconds(STOP_TIME));

  BulkSendHelper source3("ns3::TcpSocketFactory",
                        InetSocketAddress(i4i5.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  source3.SetAttribute("MaxBytes", UintegerValue(MAX_BYTES));
  source3.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
  ApplicationContainer apps3 = source3.Install(nodes.Get(2));
  apps3.Start(Seconds(START_TIME+20));
  apps3.Stop(Seconds(STOP_TIME));

  // Sink (at node 5,6,7).
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
 ApplicationContainer apps4 = sink.Install(nodes.Get(5));
 ApplicationContainer apps5 = sink.Install(nodes.Get(6));
 ApplicationContainer apps6 = sink.Install(nodes.Get(7));
  apps4.Start(Seconds(START_TIME));
  apps4.Stop(Seconds(STOP_TIME));
  apps5.Start(Seconds(START_TIME));
  apps5.Stop(Seconds(STOP_TIME));
  apps6.Start(Seconds(START_TIME));
  apps6.Stop(Seconds(STOP_TIME));




  /////////////////////////////////////////
  // Setup tracing (as appropriate).
  if (ENABLE_TRACE) {
    NS_LOG_INFO("Enabling trace files.");
    AsciiTraceHelper ath;
    p2p1.EnableAsciiAll(ath.CreateFileStream("trace1.tr"));
    p2p2.EnableAsciiAll(ath.CreateFileStream("trace2.tr"));
    p2p3.EnableAsciiAll(ath.CreateFileStream("trace3.tr"));
  }  
  if (ENABLE_PCAP) {
    NS_LOG_INFO("Enabling pcap files.");
    p2p1.EnablePcapAll("shark", true);
    p2p2.EnablePcapAll("shark", true);
    p2p3.EnablePcapAll("shark", true);
  }
  /////////////////////////////////////////
  // Run simulation.
  NS_LOG_INFO("Running simulation.");
  Simulator::Stop(Seconds(STOP_TIME));
  NS_LOG_INFO("Simulation time: [" << 
              START_TIME << "," <<
              STOP_TIME << "]");
  NS_LOG_INFO("---------------- Start -----------------------");
  Simulator::Run();
  NS_LOG_INFO("---------------- Stop ------------------------");





  NS_LOG_INFO("Done.");

  // Done.
  Simulator::Destroy();
  return 0;
}
