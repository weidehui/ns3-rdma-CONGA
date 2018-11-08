/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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
 */

#include "qbb-channel.h"
#include "qbb-net-device.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <iostream>
#include <fstream>
#include "ns3/uinteger.h"
#include "ns3/global-router-interface.h"
#include "ns3/ipv4-global-routing.h"

NS_LOG_COMPONENT_DEFINE ("QbbChannel");

namespace ns3 {

const float alpha = 0.1;
NS_OBJECT_ENSURE_REGISTERED (QbbChannel);

TypeId 
QbbChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::QbbChannel")
    .SetParent<Channel> ()
    .AddConstructor<QbbChannel> ()
    .AddAttribute ("Delay", "Transmission delay through the channel",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&QbbChannel::m_delay),
                   MakeTimeChecker ())
    .AddTraceSource ("TxRxQbb",
                     "Trace source indicating transmission of packet from the QbbChannel, used by the Animation interface.",
                     MakeTraceSourceAccessor (&QbbChannel::m_txrxQbb))
	.AddAttribute("maxCE","The max of CE",
				  UintegerValue(0),
		          MakeUintegerAccessor(&QbbChannel::m_maxCE),
				  MakeUintegerChecker<uint32_t>())
	.AddAttribute("Stage", "The Stage of CE",
				  UintegerValue(0),
				  MakeUintegerAccessor(&QbbChannel::m_stage),
				  MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

//
// By default, you get a channel that 
// has an "infitely" fast transmission speed and zero delay.
QbbChannel::QbbChannel()
  :
    PointToPointChannel (),
	CECount(0)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_nDevices = 0;
  dretimer();
  //printqueue();
}


void
QbbChannel::dretimer() {
	//printf("I'm comig the dretimer \n");
	CECount = (uint32_t)(CECount * alpha);
	//EventId eid;
	//printf("%d\n", CECount);
	if (CECount < 1) CECount = 0;
	//for (int i = 0;i < m_nDevices;i++) {
	//	//std::cout << "I'coming in for;" << std::endl;
	//	Ptr<QbbNetDevice> devA = m_link[i].m_src;
	//	Ptr<QbbNetDevice> devB = m_link[i].m_dst;
	//	//std::cout << dev->GetNode()->GetId() << std::endl;
	//	if (devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId() == 0) {
	//		std::ofstream outfile;
	//		outfile.open("D:\\data\\CECountP0(6).txt", std::ios::app);
	//		outfile << CECount << std::endl;
	//		outfile.close();
	//	}
	//	if (devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId() == 1) {
	//		std::ofstream outfile;
	//		outfile.open("D:\\data\\CECountP1(6).txt", std::ios::app);
	//		outfile << CECount << std::endl;
	//		outfile.close();
	//	}
	//}

	Simulator::Schedule(Seconds(0.0001), &QbbChannel::dretimer, this);
}

void
QbbChannel::Attach (Ptr<QbbNetDevice> device)
{
  
  //std::cout << m_nDevices << " " << N_DEVICES << "\n";
  //fflush(stdout);
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT_MSG (m_nDevices < N_DEVICES, "Only two devices permitted");
  NS_ASSERT (device != 0);

  m_link[m_nDevices++].m_src = device;
//
// If we have both devices connected to the channel, then finish introducing
// the two halves and set the links to IDLE.
//
  if (m_nDevices == N_DEVICES)
    {
      m_link[0].m_dst = m_link[1].m_src;
      m_link[1].m_dst = m_link[0].m_src;
      m_link[0].m_state = IDLE;
      m_link[1].m_state = IDLE;
    }


}

bool
QbbChannel::TransmitStart (
  Ptr<Packet> p,
  Ptr<QbbNetDevice> src,
  Time txTime)
{
  NS_LOG_FUNCTION (this << p << src);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);

  uint32_t wire = src == m_link[0].m_src ? 0 : 1;

 // Ptr<Packet>ptemp = p->Copy();
  CECount += p->GetSize();
  if (CECount > 62000) CECount = 62000;
  m_order = (uint32_t)(((float)CECount / (m_maxCE + 1))*m_stage);
  /*std::ofstream outfile;
  outfile.open("D:\\data\\psize(3).txt", std::ios::app);
  outfile <<Simulator::Now()<<" "<< p->GetSize() <<" "<<p->GetCE()<< " "<<CECount<< std::endl;
  outfile.close();*/
  /*Ptr<Node> node = src->GetNode();
  if (m_order != m_orderpre)
  {
	  m_orderpre = m_order;
	  if (p->GetNodeId() == 5)
	  {
		  Ptr<GlobalRouter> router = node->GetObject<GlobalRouter>();
		  Ptr<Ipv4GlobalRouting> gr = router->GetRoutingProtocol();
		  int Path = p->GetLBTag();
		  gr->Setforwardtb(6, Path, m_order);
		  std::ofstream outfile;
		  outfile.open("D:\\data\\localdre(31).txt", std::ios::app);
		  outfile <<Simulator::Now()<<" "<< Path <<" "<<m_order<<std::endl;
		  outfile.close();	  
	  }
  }*/

  if (m_order > p->GetCE()&& src->GetNode()->GetId()!=4&& src->GetNode()->GetId()!=5) {
	  p->SetCE(m_order);
  }
 // std::cout << m_maxCE << " " << m_stage << " " << m_order << std::endl;
  //for (int i = 0;i < m_nDevices;i++) {
	 // //std::cout << "I'coming in for;" << std::endl;
	 // Ptr<QbbNetDevice> devA = m_link[i].m_src;
	 // Ptr<QbbNetDevice> devB = m_link[i].m_dst;
	 // //std::cout << dev->GetNode()->GetId() << std::endl;
	 // /*if (devA == src && devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId() == 0) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\CE20(22).txt", std::ios::app);
		//  outfile <<Simulator::Now()<<" "<< p->GetCE() << " "<<CECount<< std::endl;
		//  outfile.close();
	 // }
	 // if (devA == src && devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId() == 1) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\CE21(22).txt", std::ios::app);
		//  outfile << Simulator::Now() << " " << p->GetCE() << " " << CECount << std::endl;
		//  outfile.close();
	 // }
	 // if (devA == src && devA->GetNode()->GetId() == 0 && devB->GetNode()->GetId() == 3) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\CE03(22).txt", std::ios::app);
		//  outfile << Simulator::Now() << " " << p->GetCE() << " " << CECount << std::endl;
		//  outfile.close();
	 // }
	 // if (devA == src && devA->GetNode()->GetId() == 1 && devB->GetNode()->GetId() == 3) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\CE13(22).txt", std::ios::app);
		//  outfile << Simulator::Now() << " " << p->GetCE() << " " << CECount << std::endl;
		//  outfile.close();
	 // }*/
	 // /*if (devA == src && devA->GetNode()->GetId() == 4 && devB->GetNode()->GetId() == 2) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\CE42(4).txt", std::ios::app);
		//  outfile << Simulator::Now() << " " << p->GetCE() << " " << CECount << std::endl;
		//  outfile.close();
	 // }*/
	 // /*if (devA == src && devA->GetNode()->GetId() == 0 && devB->GetNode()->GetId() == 2) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\packet02(23).txt", std::ios::app);
		//  outfile << Simulator::Now() << " " << p->GetNodeId() << std::endl;
		//  outfile.close();
	 // }
	 // if (devA == src && devA->GetNode()->GetId() == 1 && devB->GetNode()->GetId() == 2) {
		//  std::ofstream outfile;
		//  outfile.open("D:\\data\\packet12(23).txt", std::ios::app);
		//  outfile << Simulator::Now() << " " << p->GetNodeId() << std::endl;
		//  outfile.close();
	 // }*/
  //}
  Simulator::ScheduleWithContext (m_link[wire].m_dst->GetNode ()->GetId (),
                                  txTime + m_delay, &QbbNetDevice::Receive,
                                  m_link[wire].m_dst, p);
 // printf("The QbbChannel packetNodeId after is:%d\n", p->GetNodeId());
  // Call the tx anim callback on the net device
  m_txrxQbb (p, src, m_link[wire].m_dst, txTime, txTime + m_delay);
  return true;
}

uint32_t 
QbbChannel::GetNDevices (void) const
{
  NS_LOG_FUNCTION_NOARGS ();

  //std::cout<<m_nDevices<<"\n";
  //std::cout.flush();


  return m_nDevices;
}

Ptr<QbbNetDevice>
QbbChannel::GetQbbDevice (uint32_t i) const
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT (i < 2);
  return m_link[i].m_src;
}

Ptr<NetDevice>
QbbChannel::GetDevice (uint32_t i) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return GetQbbDevice (i);
}

Time
QbbChannel::GetDelay (void) const
{
  return m_delay;
}

Ptr<QbbNetDevice>
QbbChannel::GetSource (uint32_t i) const
{
  return m_link[i].m_src;
}

Ptr<QbbNetDevice>
QbbChannel::GetDestination (uint32_t i) const
{
  return m_link[i].m_dst;
}

bool
QbbChannel::IsInitialized (void) const
{
  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);
  return true;
}

void
QbbChannel::printqueue()
{
	//std::cout << "I'coming;" << std::endl;
	for (int i = 0;i < m_nDevices;i++) {
		//std::cout << "I'coming in for;" << std::endl;
		Ptr<QbbNetDevice> devA = m_link[i].m_src;
		Ptr<QbbNetDevice> devB = m_link[i].m_dst;
		//std::cout << dev->GetNode()->GetId() << std::endl;
		if (devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId()==0) {
			doprintqueue1(devA);
		}
		if (devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId() == 1) {
			doprintqueue2(devA);
		}
		if (devA->GetNode()->GetId() == 2 && devB->GetNode()->GetId() == 4) {
			doprintqueue3(devA);
		}
	}
}

void
QbbChannel::doprintqueue1(Ptr<QbbNetDevice> dev)
{   
	//<< Simulator::Now() << " " << dev->GetQueue()->GetNPackets() << std::endl;
	Ptr<QbbNetDevice> devtemp = dev;
	std::ofstream outfile;
	outfile.open("D:\\data\\queuelen1msp0£¨5£©.txt", std::ios::app);
	outfile << Simulator::Now() << " " << dev->GetQueue()->GetNPackets()<<std::endl;
	outfile.close();
	Simulator::Schedule(Seconds(0.001), &QbbChannel::doprintqueue1, this, devtemp);
}
void
QbbChannel::doprintqueue2(Ptr<QbbNetDevice> dev)
{
	//<< Simulator::Now() << " " << dev->GetQueue()->GetNPackets() << std::endl;
	Ptr<QbbNetDevice> devtemp = dev;
	std::ofstream outfile;
	outfile.open("D:\\data\\queuelen1msp1£¨5£©.txt", std::ios::app);
	outfile << Simulator::Now() << " " << dev->GetQueue()->GetNPackets() << std::endl;
	outfile.close();
	Simulator::Schedule(Seconds(0.001), &QbbChannel::doprintqueue2, this, devtemp);
}
void
QbbChannel::doprintqueue3(Ptr<QbbNetDevice> dev)
{
	//<< Simulator::Now() << " " << dev->GetQueue()->GetNPackets() << std::endl;
	Ptr<QbbNetDevice> devtemp = dev;
	std::ofstream outfile;
	outfile.open("D:\\data\\queuelen1msstart(5).txt", std::ios::app);
	outfile << Simulator::Now() << " " << dev->GetQueue()->GetNPackets() << std::endl;
	outfile.close();
	Simulator::Schedule(Seconds(0.001), &QbbChannel::doprintqueue3, this, devtemp);
}

} // namespace ns3
