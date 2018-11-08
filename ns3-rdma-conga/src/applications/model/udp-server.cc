/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "packet-loss-counter.h"

#include "ns3/seq-ts-header.h"
#include "udp-server.h"
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpServer");
NS_OBJECT_ENSURE_REGISTERED (UdpServer);

uint32_t nextreceive = 0;
uint32_t nextreceivepre = 0;
uint32_t flag = 0;
TypeId
UdpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpServer")
    .SetParent<Application> ()
    .AddConstructor<UdpServer> ()
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&UdpServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketWindowSize",
                   "The size of the window used to compute the packet loss. This value should be a multiple of 8.",
                   UintegerValue (32),
                   MakeUintegerAccessor (&UdpServer::GetPacketWindowSize,
                                         &UdpServer::SetPacketWindowSize),
                   MakeUintegerChecker<uint16_t> (8,256))
	.AddAttribute("flow_size",
		  "The size of flow(KB)",
		  UintegerValue(32),
		  MakeUintegerAccessor(&UdpServer::m_flowsize),
		  MakeUintegerChecker<uint32_t>())
	  .AddAttribute("flowstarttime",
		  "The size of flow(packet)",
		  TimeValue(Seconds(1)),
		  MakeTimeAccessor(&UdpServer::m_flowst),
		  MakeTimeChecker())
  ;
  return tid;
}

UdpServer::UdpServer ()
  : m_lossCounter (0)
{
  NS_LOG_FUNCTION (this);
  m_received=0;
}

UdpServer::~UdpServer ()
{
  NS_LOG_FUNCTION (this);
}

uint16_t
UdpServer::GetPacketWindowSize () const
{
  return m_lossCounter.GetBitMapSize ();
}

void
UdpServer::SetPacketWindowSize (uint16_t size)
{
  m_lossCounter.SetBitMapSize (size);
}

uint32_t
UdpServer::GetLost (void) const
{
  return m_lossCounter.GetLost ();
}

uint32_t
UdpServer::GetReceived (void) const
{

  return m_received;

}

void
UdpServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
UdpServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                   m_port);
      m_socket->Bind (local);
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (),
                                                   m_port);
      m_socket6->Bind (local);
    }
  m_count = Simulator::Now();
  m_socket6->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));

}

void
UdpServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
UdpServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  uint32_t currentSequenceNumber;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          SeqTsHeader seqTs;
          packet->RemoveHeader (seqTs);
          currentSequenceNumber = seqTs.GetSeq ();
          if (InetSocketAddress::IsMatchingType (from))
            {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs ());
            }
          else if (Inet6SocketAddress::IsMatchingType (from))
            {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< Inet6SocketAddress::ConvertFrom (from).GetIpv6 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs ());
            }

          m_lossCounter.NotifyReceived (currentSequenceNumber);
		  /*if (m_received == 0) {
			  m_flowst= Simulator::Now();
			  printf("The flowst is:%d", m_flowst);
		  }*/
          m_received++;
		  if (currentSequenceNumber == nextreceive)
		  {
			  nextreceive++;
		  }
		  if (flag == 0)
		  {
			  flag = 1;
			  printtp();
		  }
		  if (m_lossCounter.GetlastMaxSeqNum() >= m_flowsize - 1)
		  {
			  m_flowet = Simulator::Now();
			  m_flowtp = m_flowsize * 1000 * 8 / (m_flowet.GetSeconds() - m_flowst.GetSeconds());
			  std::cout << "flowst:" << m_flowst << " flowet:" << m_flowet << " flowsize:" << m_flowsize << " flowtp:" << m_flowtp << " chazhi: " << (m_flowet.GetSeconds() - m_flowst.GetSeconds());
			  std::cout << " m_received:" << m_received << " lost:" << m_lossCounter.GetLost() << "m_lastMaxSeqNum:" << m_lossCounter.GetlastMaxSeqNum() << std::endl;
		  }
		  std::ofstream outfile;
		  outfile.open("D:\\data\\packetseq(72).txt", std::ios::app);
		  outfile << Simulator::Now() << " " << m_lossCounter.GetlastMaxSeqNum() << " " << currentSequenceNumber << " " <<nextreceive<<" "<<m_received << std::endl;
		  outfile.close();

        }
	  /*if (Simulator::Now() - m_count > Seconds(0.5)) {

		  std::cout << "The flowst is:" << m_flowst << "  The flowet is:" << m_flowet << "  The flowsize is:" << m_flowsize << "  The flowtp is:" << m_flowtp << "  The chaizhi is: " << (m_flowet.GetSeconds() - m_flowst.GetSeconds()) << std::endl;
	  }*/
	  /*
	  if (m_received>4000)
	  {
		  m_received = 0;
		  Ptr<Packet> p = Create<Packet> (64);
		  std::stringstream peerAddressStringStream;
		  if (Ipv4Address::IsMatchingType (m_peerAddress))
		  {
			  peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);
		  }
	  } 
	  */
    }
}

void
UdpServer::printtp()
{
	double temp = nextreceive - nextreceivepre;
	nextreceivepre = nextreceive;
	std::ofstream outfile;
	outfile.open("D:\\data\\servertp(72).txt", std::ios::app);
	outfile << Simulator::Now() << " " << nextreceive << " " << temp << " " << (double)temp * 1000 * 8 / 0.001 << std::endl;
	outfile.close();
	Simulator::Schedule(Seconds(0.001), &UdpServer::printtp, this);
}

void
UdpServer::SetRemote (Ipv4Address ip, uint16_t port)
{
	m_peerAddress = Address(ip);
	m_peerPort = port;
}


} // Namespace ns3
