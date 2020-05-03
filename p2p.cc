//           
// Network topology
//
//       n0 ------------ (n1/router) -------------- n2
//            10.1.1.x                192.168.1.x
//       10.1.1.1    10.1.1.2   192.16.1.1     192.168.1.2
//
// - Flow from n0 to n2 using BulkSendApplication.
//
// - Tracing of queues and packet receptions to file "*.tr" and
//   "*.pcap" when tracing is turned on.
//

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
#define START_TIME       0      // Seconds
#define STOP_TIME        2000.0       // Seconds
#define S_TO_R_BW        "150Mbps" // Server to router
#define S_TO_R_DELAY     "10ms"
#define R_TO_C_BW        "10Mbps"  // Router to client (bttlneck)
#define R_TO_C_DELAY     "1ms"
#define PACKET_SIZE      1000   
//Bytes, Classic data size, set as 2*n_packet. The real packet size is PACKET_SIZE+HEADER_SIZE+QED_SIZE 
#define HEADER_SIZE       16 //Quantum header size, bytes,  Integer multiple of 16!
#define RED_SIZE     24 //Redundancy length.   Bytes  =2*n_red
#define error_p      0.0 //lost probability: 0, 0.01, 0.05, 0.1.
#define MAX_BYTES  20000000//Bytes, Set as 2*n_data. 10000 means 5000 qbytes to be sent. 
#define TCP_PROTOCOL     "ns3::TcpNewReno" //Congestion control     "ns3::TcpNewReno"，"ns3::TcpBbr"
#define DATA_RETRIES 20 //Retransmission Upper Bound
// For logging. 

NS_LOG_COMPONENT_DEFINE ("main");
/////////////////////////////////////////////////
int main (int argc, char *argv[]) {

  /////////////////////////////////////////
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
  // Note, BBR' still works without this.
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
  Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(DATA_RETRIES));
  Config::SetDefault("ns3::TcpSocketBase::MinRto", StringValue("50000000ns"));

  /////////////////////////////////////////
  // Create nodes.
  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;  // 0=sender, 1=router, 2=receiver
  nodes.Create(3);

  /////////////////////////////////////////
  // Create channels.
  NS_LOG_INFO("Creating channels.");
  NodeContainer n0_to_r = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer r_to_n1 = NodeContainer(nodes.Get(1), nodes.Get(2));


  /////////////////////////////////////
  //Create error model.
  RngSeedManager::SetSeed(2);
  Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (error_p));
  Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
  Config::SetDefault ("ns3::RateErrorModel::RanVar", StringValue ("ns3::UniformRandomVariable[Min=0|Max=1]"));
  ObjectFactory factory;
  factory.SetTypeId ("ns3::RateErrorModel");
  Ptr<ErrorModel> error_model = factory.Create<ErrorModel> ();


 /////////////////////////////////////////
  // Create links.
  NS_LOG_INFO("Creating links.");
  int mtu = 1500;
  PointToPointHelper p2p;


  // Router to Client. Lost probability=0.
  p2p.SetDeviceAttribute("DataRate", StringValue (R_TO_C_BW));
  p2p.SetChannelAttribute("Delay", StringValue (R_TO_C_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NS_LOG_INFO("Router queue size: "<< QUEUE_SIZE);
  p2p.SetQueue("ns3::DropTailQueue",
               "Mode", StringValue ("QUEUE_MODE_PACKETS"),
               "MaxPackets", UintegerValue(QUEUE_SIZE));
  NetDeviceContainer devices2 = p2p.Install(r_to_n1);

  // Server to Router. Lost probability=p.
  p2p.SetDeviceAttribute("DataRate", StringValue (S_TO_R_BW));
  p2p.SetChannelAttribute("Delay", StringValue (S_TO_R_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  p2p.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (error_model));
  NetDeviceContainer devices1 = p2p.Install(n0_to_r);

  /////////////////////////////////////////
  // Install Internet stack.
  NS_LOG_INFO("Installing Internet stack.");
  InternetStackHelper internet;
  internet.Install(nodes);
  
  /////////////////////////////////////////
  // Add IP addresses.
  NS_LOG_INFO("Assigning IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(devices1);

  ipv4.SetBase("191.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  /////////////////////////////////////////
  // Create apps.
  NS_LOG_INFO("Creating applications.");
  //NS_LOG_INFO("  Bulk send.");

  // Well-known port for server.
  uint16_t port = 911;  

  // Source (at node 0).

  //BulkSendHelper
// QidSendHelper
   BulkSendHelper source("ns3::TcpSocketFactory",
                        InetSocketAddress(i1i2.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  source.SetAttribute("MaxBytes", UintegerValue(MAX_BYTES));
  source.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
  ApplicationContainer apps = source.Install(nodes.Get(0));
  apps.Start(Seconds(START_TIME));
  apps.Stop(Seconds(STOP_TIME));

  // Sink (at node 2).
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(2));
  apps.Start(Seconds(START_TIME));
  apps.Stop(Seconds(STOP_TIME));
  Ptr<PacketSink> p_sink = DynamicCast<PacketSink> (apps.Get(0)); 

  //Install Flowmonitor.
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowmonitor=flowHelper.InstallAll();

  /////////////////////////////////////////
  // Setup tracing (as appropriate).
  if (ENABLE_TRACE) {
    NS_LOG_INFO("Enabling trace files.");
    AsciiTraceHelper ath;
    p2p.EnableAsciiAll(ath.CreateFileStream("trace.tr"));
  }  
  if (ENABLE_PCAP) {
    NS_LOG_INFO("Enabling pcap files.");
    p2p.EnablePcapAll("shark", true);
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

  //Flow monitor output.
  flowmonitor->CheckForLostPackets ();
  std::map<FlowId,FlowMonitor::FlowStats> stats=flowmonitor->GetFlowStats();
  double localThrou2=0.0;
  double localThrou3=0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
	    if (i==stats.begin ())
	           {
	                NS_LOG_INFO( "--------------------------------------------\n");
	    	        NS_LOG_INFO( "Lost Packets = " << i->second.lostPackets << "\n");
	    	        NS_LOG_INFO( "TxPackets = " << i->second.txPackets << "\n");
	    	        NS_LOG_INFO( "RxPackets= " << i->second.rxPackets << "\n");
	    	        NS_LOG_INFO( "Real Lost Probability= " << (double)i->second.lostPackets/(double)i->second.txPackets << "\n");
	    	        NS_LOG_INFO( "timeFirstTxPacket = " << i->second.timeFirstTxPacket.GetSeconds() << "\n");
	    	        NS_LOG_INFO( "timeLastRxPacket = " << i->second.timeLastRxPacket.GetSeconds() << "\n");
	    	        localThrou2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024;
	    	        localThrou3=MAX_BYTES* 4.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024;
	    	        NS_LOG_INFO( "TxThroughput: " <<  localThrou2 << " Mbps\n");
	    	        NS_LOG_INFO( "Real RxThroughput: " <<  localThrou3 << " Mqbps\n");
	    	        NS_LOG_INFO( "--------------------------------------------\n");
	           }

        }

  /////////////////////////////////////////
  // Ouput stats.
  NS_LOG_INFO("Total bytes received: " << p_sink->GetTotalRx()/(PACKET_SIZE+HEADER_SIZE+RED_SIZE)*PACKET_SIZE);
  NS_LOG_INFO("Done.");

  // Done.
  Simulator::Destroy();
  return 0;
}
