// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <vector>
#include <iomanip>
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/simulator.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"
#include "ns3/seq-ts-header.h"
#include "ipv4-global-routing.h"
#include "global-route-manager.h"
#include "ns3/node.h"
#include <cmath>
#include <iostream>
#include<stdio.h>
#include "ns3/qbb-header.h" 
#include <fstream>
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE ("Ipv4GlobalRouting");
int countpre0=0;
int countnow0=0;
int countpre1=0;
int countnow1=0;
int countpre=0;
int countnow=0;
int flag = 0;
int seq = 0;
namespace ns3 {
	const float alpha = 0.1;
	NS_OBJECT_ENSURE_REGISTERED(Ipv4GlobalRouting);

	/* see http://www.iana.org/assignments/protocol-numbers */
	const uint8_t TCP_PROT_NUMBER = 6;
	const uint8_t UDP_PROT_NUMBER = 17;
	const Time TimeoutInterval = Seconds(0.0005);
	TypeId
		Ipv4GlobalRouting::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::Ipv4GlobalRouting")
			.SetParent<Object>()
			.SetGroupName("Internet")
			.AddConstructor<Ipv4GlobalRouting>()
			.AddAttribute("RandomEcmpRouting",
				"Set to true if packets are randomly routed among ECMP; set to false for using only one route consistently",
				BooleanValue(false),
				MakeBooleanAccessor(&Ipv4GlobalRouting::m_randomRouting),
				MakeBooleanChecker())
			.AddAttribute("CONGARouting",
				"Set to true if packets are randomly routed among CONGA; set to false for using only one route consistently",
				BooleanValue(false),
				MakeBooleanAccessor(&Ipv4GlobalRouting::m_CONGARouting),
				MakeBooleanChecker())
			.AddAttribute("FlowEcmpRouting",
				"Set to true if flows are randomly routed among ECMP; set to false for using only one route consistently",
				BooleanValue(false),
				MakeBooleanAccessor(&Ipv4GlobalRouting::m_flowEcmpRouting),
				MakeBooleanChecker())
            .AddAttribute("FlowInterval",
				"When this timeout expires, ",
				TimeValue(Seconds(0.001)),
				MakeTimeAccessor(&Ipv4GlobalRouting::m_flowInterval),
				MakeTimeChecker())
			.AddAttribute("RespondToInterfaceEvents",
				"Set to true if you want to dynamically recompute the global routes upon Interface notification events (up/down, or add/remove address)",
				BooleanValue(false),
				MakeBooleanAccessor(&Ipv4GlobalRouting::m_respondToInterfaceEvents),
				MakeBooleanChecker())	
			.AddAttribute("S_Number",
				"Set the number of Switch",
				UintegerValue(0),
				MakeUintegerAccessor(&Ipv4GlobalRouting::m_s),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("E_Number",
				"Set the number of Switch",
				UintegerValue(0),
				MakeUintegerAccessor(&Ipv4GlobalRouting::m_e),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("Number",
				"Set the number of Switch",
				UintegerValue(3),
				MakeUintegerAccessor(&Ipv4GlobalRouting::m_n),
				MakeUintegerChecker<uint32_t>())
			;
		return tid;
	}
	Ipv4GlobalRouting::Ipv4GlobalRouting()
		: m_CONGARouting(false),
		m_flowEcmpRouting(false),
		m_respondToInterfaceEvents(false),
		m_e(6),
		m_s(5),
		m_n(2)
	{
		NS_LOG_FUNCTION_NOARGS();

		m_rand = CreateObject<UniformRandomVariable>();
		forwardtb = new uint32_t *[m_e + 1];
		//int i;
		for (int i = m_s; i <= m_e; i++) {
			forwardtb[i] = new uint32_t[m_n];
			for (int j = 0;j < m_n;j++) {
				forwardtb[i][j] = 0;
			}
		}

		cachetb = new uint32_t *[m_e + 1];
		//int i;
		for (int i = m_s; i <= m_e; i++) {
			cachetb[i] = new uint32_t[m_n];
			for (int j = 0;j < m_n;j++) {
				cachetb[i][j] = 0;
			}
		}

		flagiter = new uint32_t[m_e + 1];
		for (int i = m_s;i <= m_e;i++) {
			flagiter[i] = 0;
		}
		//printf("m_s is:%d\n", m_s);
		//printf("m_e is:%d\n", m_e);
		//printf("m_n is:%d\n", m_n);
		ftb = new flowlettable[m_n];
		for (int i = 0;i < m_n;i++)
		{
			ftb[i].port = 0;
			ftb[i].agebit = Seconds(0);
		}

		//std::cout <<"****"<< m_flowInterval << std::endl;
	}

	Ipv4GlobalRouting::~Ipv4GlobalRouting()
	{
		NS_LOG_FUNCTION_NOARGS();
	}

	void
		Ipv4GlobalRouting::AddHostRouteTo(Ipv4Address dest,
			Ipv4Address nextHop,
			uint32_t interface)
	{
		NS_LOG_FUNCTION(dest << nextHop << interface);
		Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry();
		*route = Ipv4RoutingTableEntry::CreateHostRouteTo(dest, nextHop, interface);
		m_hostRoutes.push_back(route);
	}

	void
		Ipv4GlobalRouting::AddHostRouteTo(Ipv4Address dest,
			uint32_t interface)
	{
		NS_LOG_FUNCTION(dest << interface);
		Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry();
		*route = Ipv4RoutingTableEntry::CreateHostRouteTo(dest, interface);
		m_hostRoutes.push_back(route);
	}

	void
		Ipv4GlobalRouting::AddNetworkRouteTo(Ipv4Address network,
			Ipv4Mask networkMask,
			Ipv4Address nextHop,
			uint32_t interface)
	{
		NS_LOG_FUNCTION(network << networkMask << nextHop << interface);
		Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry();
		*route = Ipv4RoutingTableEntry::CreateNetworkRouteTo(network,
			networkMask,
			nextHop,
			interface);
		m_networkRoutes.push_back(route);
	}

	void
		Ipv4GlobalRouting::AddNetworkRouteTo(Ipv4Address network,
			Ipv4Mask networkMask,
			uint32_t interface)
	{
		NS_LOG_FUNCTION(network << networkMask << interface);
		Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry();
		*route = Ipv4RoutingTableEntry::CreateNetworkRouteTo(network,
			networkMask,
			interface);
		m_networkRoutes.push_back(route);
	}

	void
		Ipv4GlobalRouting::AddASExternalRouteTo(Ipv4Address network,
			Ipv4Mask networkMask,
			Ipv4Address nextHop,
			uint32_t interface)
	{
		NS_LOG_FUNCTION(network << networkMask << nextHop);
		Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry();
		*route = Ipv4RoutingTableEntry::CreateNetworkRouteTo(network,
			networkMask,
			nextHop,
			interface);
		m_ASexternalRoutes.push_back(route);
	}


	// This function is used to spread the routing of flows across equal
	// cost paths, by calculating an integer based on the five-tuple in the headers
	//
	// It assumes that if a transport protocol value is specified in the header, 
	// that a transport header with port numbers is prepended to the ipPayload 
	//
	uint32_t
		Ipv4GlobalRouting::GetTupleValue(const Ipv4Header &header, Ptr<const Packet> ipPayload)
	{
		// We do not care if this value rolls over
		uint32_t tupleValue = (header.GetSource().Get() * 59) ^ header.GetDestination().Get() ^ header.GetProtocol();
		switch (header.GetProtocol())
		{
		case UDP_PROT_NUMBER:
		{
			UdpHeader udpHeader;
			Ptr<Packet> p = ipPayload->Copy();
			p->RemoveHeader(udpHeader);
			SeqTsHeader seqh;
			p->RemoveHeader(seqh);
			//ipPayload->PeekHeader (udpHeader);
			NS_LOG_DEBUG("Found UDP proto and header: " <<
				udpHeader.GetSourcePort() << ":" <<
				udpHeader.GetDestinationPort());
			tupleValue ^= (udpHeader.GetSourcePort() << 16);
			tupleValue ^= udpHeader.GetDestinationPort();
			tupleValue ^= seqh.GetPG();
			break;
		}
		case TCP_PROT_NUMBER:
		{
			TcpHeader tcpHeader;
			ipPayload->PeekHeader(tcpHeader);
			NS_LOG_DEBUG("Found TCP proto and header: " <<
				tcpHeader.GetSourcePort() << ":" <<
				tcpHeader.GetDestinationPort());
			tupleValue ^= (tcpHeader.GetSourcePort() << 16);
			tupleValue ^= tcpHeader.GetDestinationPort();
			break;
		}
		default:
		{
			NS_LOG_DEBUG("Udp or Tcp header not found");
			break;
		}
		}
		return tupleValue;
	}


	uint32_t int2ip(Ipv4Address dest) {  
	  //string s;
		uint32_t ipint = dest.Get();
		uint32_t tokenInt = 0;
		uint32_t leftValue = ipint;
		uint32_t result = 0;
		for (int i = 0; i < 4; i++) {
			int temp = pow(256, 3 - i);
			tokenInt = leftValue / temp;
			leftValue %= temp;
			//    itoa(tokenInt, ipToken, 10); //non-standard function
			if (i == 2) {
				result = tokenInt;
			}
		}
		return result;
	}

	Ptr<Ipv4Route>
		// - Ipv4GlobalRouting::LookupGlobal (Ipv4Address dest, Ptr<NetDevice> oif)
		Ipv4GlobalRouting::LookupGlobal(const Ipv4Header &header, Ptr<Packet> ipPayload, Ptr<NetDevice> oif)
	{    
		//printf("I'm coming the LookuoGlobal\n");
		NS_LOG_FUNCTION_NOARGS();
		// - NS_LOG_LOGIC ("Looking for route for destination " << dest);
		NS_ABORT_MSG_IF(m_CONGARouting && m_flowEcmpRouting, "Ecmp mode selection");
		NS_LOG_LOGIC("Looking for route for destination " << header.GetDestination());

		Ptr<Ipv4Route> rtentry = 0;
		// store all available routes that bring packets to their destination
		typedef std::vector<Ipv4RoutingTableEntry*> RouteVec_t;
		RouteVec_t allRoutes;

		NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
		for (HostRoutesCI i = m_hostRoutes.begin();
			i != m_hostRoutes.end();
			i++)
		{
			NS_ASSERT((*i)->IsHost());
			// - if ((*i)->GetDest ().IsEqual (dest)) 
			if ((*i)->GetDest().IsEqual(header.GetDestination()))
			{
				if (oif != 0)
				{
					if (oif != m_ipv4->GetNetDevice((*i)->GetInterface()))
					{
						NS_LOG_LOGIC("Not on requested interface, skipping");
						continue;
					}
				}
				allRoutes.push_back(*i);
				NS_LOG_LOGIC(allRoutes.size() << "Found global host route" << *i);
			}
		}
	//	printf("the LookupGLobal allRoutes.size is :%d \n", allRoutes.size());
		if (allRoutes.size() == 0) // if no host route is found
		{
			NS_LOG_LOGIC("Number of m_networkRoutes" << m_networkRoutes.size());
			for (NetworkRoutesI j = m_networkRoutes.begin();
				j != m_networkRoutes.end();
				j++)
			{
				Ipv4Mask mask = (*j)->GetDestNetworkMask();
				Ipv4Address entry = (*j)->GetDestNetwork();
				// - if (mask.IsMatch (dest, entry)) 
				if (mask.IsMatch(header.GetDestination(), entry))
				{
					if (oif != 0)
					{
						if (oif != m_ipv4->GetNetDevice((*j)->GetInterface()))
						{
							NS_LOG_LOGIC("Not on requested interface, skipping");
							continue;
						}
					}
					allRoutes.push_back(*j);
					NS_LOG_LOGIC(allRoutes.size() << "Found global network route" << *j);
				}
			}
		}
		//printf("the second LookupGLobal allRoutes.size is :%d \n", allRoutes.size());
		if (allRoutes.size() == 0)  // consider external if no host/network found
		{
			//printf("I'm comig the size=0 \n");
			for (ASExternalRoutesI k = m_ASexternalRoutes.begin();
				k != m_ASexternalRoutes.end();
				k++)
			{
				Ipv4Mask mask = (*k)->GetDestNetworkMask();
				Ipv4Address entry = (*k)->GetDestNetwork();
				//if (mask.IsMatch (dest, entry))
				if (mask.IsMatch(header.GetDestination(), entry))
				{
					NS_LOG_LOGIC("Found external route" << *k);
					if (oif != 0)
					{
						if (oif != m_ipv4->GetNetDevice((*k)->GetInterface()))
						{
							NS_LOG_LOGIC("Not on requested interface, skipping");
							continue;
						}
					}
					allRoutes.push_back(*k);
					break;
				}
			}
		}
		if (allRoutes.size() > 0) // if route(s) is found
		{
			// select one of the routes uniformly at random if random
			// ECMP routing is enabled, or map a flow consistently to a route
			// if flow ECMP routing is enabled, or otherwise always select the 
			// first route
			uint32_t selectIndex;
			if (m_randomRouting)
			{
				selectIndex = m_rand->GetInteger(0, allRoutes.size() - 1);
				//std::cout << "I.m coming m_randomRouting:" << m_flowEcmpRouting<<std::endl;
				/*std::ofstream outfile;
				outfile.open("D:\\data\\lookupGobalkey(2).txt", std::ios::app);
				outfile << Simulator::Now() << " " << int2ip(header.GetDestination()) << std::endl;
				outfile.close();*/
			}
			else  if (m_flowEcmpRouting && (allRoutes.size() > 1))
			{
				selectIndex = GetTupleValue(header, ipPayload) % allRoutes.size();
				//std::cout << "I.m coming" << std::endl;
				/*std::ofstream outfile;
				outfile.open("D:\\data\\lookupGobalkey(2).txt", std::ios::app);
				outfile <<Simulator::Now()<<" "<<int2ip(header.GetDestination()) <<std::endl;
				 outfile.close();
				*/
			}
			else
			{
				selectIndex = 0;
			}
			Ipv4RoutingTableEntry* route = allRoutes.at(selectIndex);
			// create a Ipv4Route object from the selected routing table entry
			rtentry = Create<Ipv4Route>();
			rtentry->SetDestination(route->GetDest());
			// XXX handle multi-address case
			rtentry->SetSource(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal());
			//printf("The lookupglobal nodeid is:%d\n", int2ip(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal()));
			rtentry->SetGateway(route->GetGateway());
			uint32_t interfaceIdx = route->GetInterface();
			rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
			Ptr<NetDevice>ND = m_ipv4->GetNetDevice(interfaceIdx);
			Ptr<Node> n = ND->GetNode();
			//printf("The lookupglobal ipPayload set before nodeid is:%d\n", ipPayload->GetNodeId());
			/*if (ipPayload->GetNodeId() == 9) {
				ipPayload->SetNodeId(int2ip(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal()));
                printf("I'm set the NodeId The lookupglobal ipPayload nodeid is:%d*******\n", int2ip(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal()));
				printf("The lookupglobal ipPayload nodeid is:%d\n", n->GetId());
			}*/
			
			//printf("The lookupglobal find out\n");
			//countnow++;
			//if (flag == 0)
			//{
			//	flag = 1;
			//	//printtpP0();
			//	//printtpP1();
			//	printtpAll();
			//}

			return rtentry;
		}
		else
		{
			return 0;
		}
	}
	int
		Ipv4GlobalRouting::SelectPath(uint32_t destid)
	{
		int selectIndex=0;
		typedef std::vector<int> Devnum;
		Devnum devnum;
		int flag = 100000000000;

		if (destid<m_s || destid>m_e)
		{
			fprintf(stderr, "forwardtb destid out of range! destid=%d\n", destid);
		}
		for (int i = 0;i < m_n;i++)
		{
			if (forwardtb[destid][i] < flag) flag = forwardtb[destid][i];
			// printf("The check path is:%d,The number of forwardtb is:%d\n", i, forwardtb[destid][i]);

		}

		for (int i = 0;i < m_n;i++)
		{
			if (forwardtb[destid][i] == flag) devnum.push_back(i);
			//printf("The selected path is:%d\n", i);
		}
		//int selectIndex;
		if (devnum.size() > 0)
		{
			uint32_t temp = m_rand->GetInteger(0, devnum.size() - 1);
			selectIndex = devnum[temp];
			//printf("The selected path is:%d\n", selectIndex);
		}
		return selectIndex;
	}
	Ptr<Ipv4Route>
		Ipv4GlobalRouting::LookupGlobal1(const Ipv4Header &header, Ptr<Packet> p, Ipv4Address dest, Ptr<NetDevice> oif)
	{
		//forwardtb[0][0]=200;
		//forwardtb[0][1]=309;
		//forwardtb[0][2]=406;
		NS_LOG_FUNCTION(this << dest << oif);
		NS_LOG_LOGIC("Looking for route for destination " << dest);
		p->SetTimeStamp(Simulator::Now());
		Ptr<Ipv4Route> rtentry = 0;
		// store all available routes that bring packets to their destination
		typedef std::vector<Ipv4RoutingTableEntry*> RouteVec_t;
		RouteVec_t allRoutes;

		NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
		for (HostRoutesCI i = m_hostRoutes.begin();
			i != m_hostRoutes.end();
			i++)
		{
			NS_ASSERT((*i)->IsHost());
			if ((*i)->GetDest().IsEqual(dest))
			{
				if (oif != 0)
				{
					if (oif != m_ipv4->GetNetDevice((*i)->GetInterface()))
					{
						NS_LOG_LOGIC("Not on requested interface, skipping");
						continue;
					}
				}
				allRoutes.push_back(*i);
				NS_LOG_LOGIC(allRoutes.size() << "Found global host route" << *i);
			}
		}
		//printf("the CONGA allRoutes.size is :%d \n", allRoutes.size());
		if (allRoutes.size() == 0) // if no host route is found
		{
			NS_LOG_LOGIC("Number of m_networkRoutes" << m_networkRoutes.size());
			for (NetworkRoutesI j = m_networkRoutes.begin();
				j != m_networkRoutes.end();
				j++)
			{
				Ipv4Mask mask = (*j)->GetDestNetworkMask();
				Ipv4Address entry = (*j)->GetDestNetwork();
				if (mask.IsMatch(dest, entry))
				{
					if (oif != 0)
					{
						if (oif != m_ipv4->GetNetDevice((*j)->GetInterface()))
						{
							NS_LOG_LOGIC("Not on requested interface, skipping");
							continue;
						}
					}
					allRoutes.push_back(*j);
					NS_LOG_LOGIC(allRoutes.size() << "Found global network route" << *j);
				}
			}
		}
		//printf("the second CONGA allRoutes.size is :%d \n", allRoutes.size());
		if (allRoutes.size() == 0)  // consider external if no host/network found
		{
			for (ASExternalRoutesI k = m_ASexternalRoutes.begin();
				k != m_ASexternalRoutes.end();
				k++)
			{
				Ipv4Mask mask = (*k)->GetDestNetworkMask();
				Ipv4Address entry = (*k)->GetDestNetwork();
				if (mask.IsMatch(dest, entry))
				{
					NS_LOG_LOGIC("Found external route" << *k);
					if (oif != 0)
					{
						if (oif != m_ipv4->GetNetDevice((*k)->GetInterface()))
						{
							NS_LOG_LOGIC("Not on requested interface, skipping");
							continue;
						}
					}
					allRoutes.push_back(*k);
					break;
				}
			}
		}
		//printf("the CONGA third allRoutes.size is :%d \n", allRoutes.size());
		if (allRoutes.size() > 0) // if route(s) is found
		{
			// pick up one of the routes uniformly at random if random
			// ECMP routing is enabled, or always select the first route
			// consistently if random ECMP routing is disabled
			int selectIndex;
			if (m_CONGARouting)
			{
				//printf("I'm comig the CONGARouting \n");
		        //printf("dest is:%d\n", dest.Get());
				//printf("dest node is:%d\n", int2ip(dest));
				if (allRoutes.size() == 1) {
					//printf("When the allRoutes=1\n");
					int FBPath = p->GetFBPath();  
					//printf("The FBPath is :%d\n",FBPath);
					int FBMetric = p->GetFBMetric();
					//printf("The FBMetric is :%d\n", FBMetric);
					int id = p->GetNodeId(); 
					//printf("The GetNodeId is :%d\n", id);

					if (id<m_s || id>m_e)
					{
						fprintf(stderr, "forwardtb id out of range! id=%d\n", id);
					}
					forwardtb[id][FBPath] = FBMetric; 

					int CE = p->GetCE();
					//printf("The CE is :%d\n", CE);
					int LBTag = p->GetLBTag(); 
					//printf("The LBTag is :%d\n", LBTag);
					cachetb[id][LBTag] = p->GetCE();
					std::ofstream outfile;
					outfile.open("D:\\data\\cachetb(72).txt", std::ios::app);
					outfile << Simulator::Now() << " " <<id<<" "<< cachetb[id][0] << " " << cachetb[id][1] << " " << p->GetCE()<< " "<< p->GetLBTag() <<std::endl;
					//outfile << "aaaaa";
					outfile.close();
					p->SetCE(0);
					selectIndex = 0;
				}
				else if (allRoutes.size() > 1) 
				{
					
					//printf("When the allRoutes>1\n");
					int destid = int2ip(dest);
					//printf("The destid is:%d\n",destid);
					if (destid<0 || destid>m_e)
					{
						fprintf(stderr, "flagiter id out of range! id=%d\n", destid);
					}
					p->SetFBPath(flagiter[destid]);
					//printf("The flagiter[destid] is:%d\n", flagiter[destid]);
					p->SetFBMetric(cachetb[destid][flagiter[destid]]);
					flagiter[destid]++;
					if (flagiter[destid] == m_n) {
						flagiter[destid] = 0;
					}
					uint32_t  key= GetTupleValue(header, p) % allRoutes.size();
					if (p->GetTimeStamp() - ftb[key].agebit > TimeoutInterval || ftb[key].agebit == Seconds(0))
					{
						//std::cout << "time:" << Simulator::Now();
						ftb[key].port = SelectPath(destid);
						ftb[key].agebit = Simulator::Now();
						//std::cout << "  path:" << ftb[key].port << "  congestion:" << forwardtb[destid][ftb[key].port];
						qbbHeader qbbh;
						//std::cout << " packet:" << p->GetSeqs()<< std::endl;
						if (destid == 6)
						{
							std::ofstream outfile;
							outfile.open("D:\\data\\flowlet1ms(72).txt", std::ios::app);
							outfile << Simulator::Now() << " " <<destid<<" "<< ftb[key].port << " " << forwardtb[destid][0] << " " << forwardtb[destid][1] << " " << seq << std::endl;
							//outfile << "aaaaa";
							outfile.close();

						}
					}
					selectIndex = ftb[key].port;
					if (p->GetNodeId() == 5)
					{
						countnow++;
						if (ftb[key].port == 1) countnow1++;
						if (ftb[key].port == 0) countnow0++;
					}
					
					if (p->GetNodeId() == 5 && flag == 0)
					{
						flag = 1;
						printtpP0();
						printtpP1();
						printtpAll();
					}
					std::ofstream outfile;
					outfile.open("D:\\data\\packet1ms(72).txt", std::ios::app);
					outfile << Simulator::Now() << " " << destid << " " << ftb[key].port << " " << forwardtb[destid][0] << " " << forwardtb[destid][1] << " " << seq << std::endl;
					//outfile << "aaaaa";
					outfile.close();
					p->SetLBTag(selectIndex);
					if (selectIndex == 0)
					{
						CECount0 += p->GetSize();
						if (CECount0 > 62000) CECount0 = 62000;
					}
					if (selectIndex == 1)
					{
						CECount1 += p->GetSize();
						if (CECount1 > 62000) CECount1 = 62000;
					}
					m_order0 = (uint32_t)(((float)CECount0 / (m_maxCE + 1))*m_stage);
					forwardtb[destid][0] = m_order0;
					m_order1 = (uint32_t)(((float)CECount1 / (m_maxCE + 1))*m_stage);
					forwardtb[destid][1] = m_order1;
					//if (destid == 6)
					//{
					//	std::ofstream outfile;
					//	outfile.open("D:\\data\\CECount(51).txt", std::ios::app);
					//	outfile << Simulator::Now() << " " << CECount0 << " "<<m_order0<<" " << CECount1<<" "<<m_order1 << std::endl;
					//	//outfile << "aaaaa";
					//	outfile.close();
					//}
				}
			}
			else
			{
				//printf("I'm coming the CONGA else\n");
				selectIndex = 0;
			}
			Ipv4RoutingTableEntry* route = allRoutes.at(selectIndex);
			// create a Ipv4Route object from the selected routing table entry
			rtentry = Create<Ipv4Route>();
			rtentry->SetDestination(route->GetDest());
			/// \todo handle multi-address case
			Ipv4Address source = m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal();
			
			rtentry->SetSource(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal());
			rtentry->SetGateway(route->GetGateway());
			uint32_t interfaceIdx = route->GetInterface();
			rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
			Ptr<NetDevice>ND = m_ipv4->GetNetDevice(interfaceIdx);
			Ptr<Node> n = ND->GetNode();

			return rtentry;
		}
		else
		{
			return 0;
		}
	}

	uint32_t
		Ipv4GlobalRouting::GetNRoutes(void) const
	{
		NS_LOG_FUNCTION_NOARGS();
		uint32_t n = 0;
		n += m_hostRoutes.size();
		n += m_networkRoutes.size();
		n += m_ASexternalRoutes.size();
		return n;
	}

	Ipv4RoutingTableEntry *
		Ipv4GlobalRouting::GetRoute(uint32_t index) const
	{
		NS_LOG_FUNCTION(index);
		if (index < m_hostRoutes.size())
		{
			uint32_t tmp = 0;
			for (HostRoutesCI i = m_hostRoutes.begin();
				i != m_hostRoutes.end();
				i++)
			{
				if (tmp == index)
				{
					return *i;
				}
				tmp++;
			}
		}
		index -= m_hostRoutes.size();
		uint32_t tmp = 0;
		if (index < m_networkRoutes.size())
		{
			for (NetworkRoutesCI j = m_networkRoutes.begin();
				j != m_networkRoutes.end();
				j++)
			{
				if (tmp == index)
				{
					return *j;
				}
				tmp++;
			}
		}
		index -= m_networkRoutes.size();
		tmp = 0;
		for (ASExternalRoutesCI k = m_ASexternalRoutes.begin();
			k != m_ASexternalRoutes.end();
			k++)
		{
			if (tmp == index)
			{
				return *k;
			}
			tmp++;
		}
		NS_ASSERT(false);
		// quiet compiler.
		return 0;
	}
	void
		Ipv4GlobalRouting::RemoveRoute(uint32_t index)
	{
		NS_LOG_FUNCTION(index);
		if (index < m_hostRoutes.size())
		{
			uint32_t tmp = 0;
			for (HostRoutesI i = m_hostRoutes.begin();
				i != m_hostRoutes.end();
				i++)
			{
				if (tmp == index)
				{
					NS_LOG_LOGIC("Removing route " << index << "; size = " << m_hostRoutes.size());
					delete *i;
					m_hostRoutes.erase(i);
					NS_LOG_LOGIC("Done removing host route " << index << "; host route remaining size = " << m_hostRoutes.size());
					return;
				}
				tmp++;
			}
		}
		index -= m_hostRoutes.size();
		uint32_t tmp = 0;
		for (NetworkRoutesI j = m_networkRoutes.begin();
			j != m_networkRoutes.end();
			j++)
		{
			if (tmp == index)
			{
				NS_LOG_LOGIC("Removing route " << index << "; size = " << m_networkRoutes.size());
				delete *j;
				m_networkRoutes.erase(j);
				NS_LOG_LOGIC("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size());
				return;
			}
			tmp++;
		}
		index -= m_networkRoutes.size();
		tmp = 0;
		for (ASExternalRoutesI k = m_ASexternalRoutes.begin();
			k != m_ASexternalRoutes.end();
			k++)
		{
			if (tmp == index)
			{
				NS_LOG_LOGIC("Removing route " << index << "; size = " << m_ASexternalRoutes.size());
				delete *k;
				m_ASexternalRoutes.erase(k);
				NS_LOG_LOGIC("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size());
				return;
			}
			tmp++;
		}
		NS_ASSERT(false);
	}

	int64_t
		Ipv4GlobalRouting::AssignStreams(int64_t stream)
	{
		NS_LOG_FUNCTION(this << stream);
		m_rand->SetStream(stream);
		return 1;
	}

	void
		Ipv4GlobalRouting::DoDispose(void)
	{
		NS_LOG_FUNCTION_NOARGS();
		for (HostRoutesI i = m_hostRoutes.begin();
			i != m_hostRoutes.end();
			i = m_hostRoutes.erase(i))
		{
			delete (*i);
		}
		for (NetworkRoutesI j = m_networkRoutes.begin();
			j != m_networkRoutes.end();
			j = m_networkRoutes.erase(j))
		{
			delete (*j);
		}
		for (ASExternalRoutesI l = m_ASexternalRoutes.begin();
			l != m_ASexternalRoutes.end();
			l = m_ASexternalRoutes.erase(l))
		{
			delete (*l);
		}

		Ipv4RoutingProtocol::DoDispose();
	}

	// Formatted like output of "route -n" command
	void
		Ipv4GlobalRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const
	{
		std::ostream* os = stream->GetStream();
		if (GetNRoutes() > 0)
		{
			*os << "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface" << std::endl;
			for (uint32_t j = 0; j < GetNRoutes(); j++)
			{
				std::ostringstream dest, gw, mask, flags;
				Ipv4RoutingTableEntry route = GetRoute(j);
				dest << route.GetDest();
				*os << std::setiosflags(std::ios::left) << std::setw(16) << dest.str();
				gw << route.GetGateway();
				*os << std::setiosflags(std::ios::left) << std::setw(16) << gw.str();
				mask << route.GetDestNetworkMask();
				*os << std::setiosflags(std::ios::left) << std::setw(16) << mask.str();
				flags << "U";
				if (route.IsHost())
				{
					flags << "H";
				}
				else if (route.IsGateway())
				{
					flags << "G";
				}
				*os << std::setiosflags(std::ios::left) << std::setw(6) << flags.str();
				// Metric not implemented
				*os << "-" << "      ";
				// Ref ct not implemented
				*os << "-" << "      ";
				// Use not implemented
				*os << "-" << "   ";
				if (Names::FindName(m_ipv4->GetNetDevice(route.GetInterface())) != "")
				{
					*os << Names::FindName(m_ipv4->GetNetDevice(route.GetInterface()));
				}
				else
				{
					*os << route.GetInterface();
				}
				*os << std::endl;
			}
		}
	}

	Ptr<Ipv4Route>
		Ipv4GlobalRouting::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
	{
	    //printf("Im'coming the RouteOutput \n");
		//printf("The CONGAState is: %d\n",m_CONGARouting);
		//
		// First, see if this is a multicast packet we have a route for.  If we
		// have a route, then send the packet down each of the specified interfaces.
		//
		if (header.GetDestination().IsMulticast())
		{
			//printf("Im'coming the RouteOutput Multicast \n");
			NS_LOG_LOGIC("Multicast destination-- returning false");
			return 0; // Let other routing protocols try to handle this
		}
		//
		// See if this is a unicast packet we have a route for.
		//
		NS_LOG_LOGIC("Unicast destination- looking up");
		// - Ptr<Ipv4Route> rtentry = LookupGlobal (header.GetDestination (), oif);
		Ptr<Ipv4Route> rtentry;
		//p->SetSeqs(seq);
		seq++;
		if (m_CONGARouting) {
			printf("Im'coming the RouteOutput CONGA lookupgolbal\n");
			//printf("The RouteOutput lookupglobal packet set before nodeid is:%d\n", ->GetId());
			Ptr<Packet> packet = p->Copy();
			//printf("The RouteOutput lookupglobal1 packet set before nodeid is:%d\n", packet->GetNodeId());
			//printf("The packet info is:%d,\n",packet->GetCE());
			//printf("The header source is:%d,\n", int2ip(header.GetSource()));
			rtentry = LookupGlobal1(header,packet,header.GetDestination(), oif);
		}
		else {
			//printf("Im'coming the RouteOutput LookupGlobal \n");
			//printf("The RouteOutput lookupglobal packet set before nodeid is:%d\n", p->GetNodeId());
			//p->SetNodeId(9);
			//p->SetCE(0);
			//printf("The RouteOutput lookupglobal packet set after nodeid is:%d\n", p->GetNodeId());
			//printf("The header source is:%d,\n", int2ip(header.GetSource()));
			rtentry = LookupGlobal(header, p, oif);
		}
		if (rtentry)
		{
			sockerr = Socket::ERROR_NOTERROR;
		}
		else
		{
			sockerr = Socket::ERROR_NOROUTETOHOST;
		}
		return rtentry;
	}

	bool
		Ipv4GlobalRouting::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb,
			LocalDeliverCallback lcb, ErrorCallback ecb)
	{
		Ptr<Packet>packet = p->Copy();
		//std::ofstream outfile;
		//outfile.open("D:\\data\\pCE£¨5£©.txt", std::ios::app);
		//outfile << header.GetSource() << " " <<  packet->GetCE() << std::endl;
		////outfile << "aaaaa";
		//outfile.close();
		//printf("Im'coming the RouteIutput \n");
		//printf("The CONGAState is: %d\n", m_CONGARouting);
		NS_LOG_FUNCTION(this << packet << header << header.GetSource() << header.GetDestination() << idev);
		// Check if input device supports IP
		NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
		uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);
		//std::cout<<header.GetSource() << "  " << header.GetDestination()<<
		qbbHeader qbbh;
		/*if (p->PeekHeader(qbbh) > 0) {
			std::cout << "I find qbbheader" << header.GetSource() << "    " << header.GetDestination() << std::endl;
		}
		else std::cout << header.GetSource() << "  " << header.GetDestination() << std::endl;*/

		
		if (header.GetDestination().IsMulticast())
		{
			NS_LOG_LOGIC("Multicast destination-- returning false");
			return false; // Let other routing protocols try to handle this
		}

		if (header.GetDestination().IsBroadcast())
		{
			NS_LOG_LOGIC("For me (Ipv4Addr broadcast address)");
			// TODO:  Local Deliver for broadcast
			// TODO:  Forward broadcast
		}

		// TODO:  Configurable option to enable RFC 1222 Strong End System Model
		// Right now, we will be permissive and allow a source to send us
		// a packet to one of our other interface addresses; that is, the
		// destination unicast address does not match one of the iif addresses,
		// but we check our other interfaces.  This could be an option
		// (to remove the outer loop immediately below and just check iif).
		for (uint32_t j = 0; j < m_ipv4->GetNInterfaces(); j++)
		{
			for (uint32_t i = 0; i < m_ipv4->GetNAddresses(j); i++)
			{
				Ipv4InterfaceAddress iaddr = m_ipv4->GetAddress(j, i);
				Ipv4Address addr = iaddr.GetLocal();
				if (addr.IsEqual(header.GetDestination()))
				{
					if (j == iif)
					{
						NS_LOG_LOGIC("For me (destination " << addr << " match)");
					}
					else
					{
						NS_LOG_LOGIC("For me (destination " << addr << " match) on another interface " << header.GetDestination());
					}
					lcb(packet, header, iif);
					return true;
				}
				if (header.GetDestination().IsEqual(iaddr.GetBroadcast()))
				{
					NS_LOG_LOGIC("For me (interface broadcast address)");
					lcb(packet, header, iif);
					return true;
				}
				NS_LOG_LOGIC("Address " << addr << " not a match");
			}
		}
		// Check if input device supports IP forwarding
		if (m_ipv4->IsForwarding(iif) == false)
		{
			NS_LOG_LOGIC("Forwarding disabled for this interface");
			ecb(packet, header, Socket::ERROR_NOROUTETOHOST);
			return false;
		}
		// Next, try to find a route
		NS_LOG_LOGIC("Unicast destination- looking up global route");
		// - Ptr<Ipv4Route> rtentry = LookupGlobal (header.GetDestination ());

		Ptr<Ipv4Route> rtentry;
		//Ptr<Packet> packet = p->Copy();
		if (m_CONGARouting) {
			//printf("Im'coming the CONGA RouteIutput lookupgolbal\n");
			//Ptr<Packet> packet = p->Copy();
			//printf("The Input packet info is:%d,%d\n", packet->GetCE(),packet->GetFBPath());
			//printf("The header source is:%d,\n", int2ip(header.GetSource()));
			//printf("The RouteIntput CONGA lookupglobal packet set before nodeid is:%d\n", packet->GetNodeId());
			if (packet->GetNodeId() == -1) {
                packet->SetNodeId(int2ip(header.GetSource()));
				//std::cout << "nodeid:" << int2ip(header.GetSource()) << std::endl;
			}
			if (flagdre == 0)
			{
				//std::cout << "dretimer" << std::endl;
				dretimer0();
				//dretimer1();
				flagdre = 1;
			}
			rtentry = LookupGlobal1(header,packet, header.GetDestination());
			//printf("RouteIntput packet nodeid:%d\n", packet->GetNodeId());
		}
		else {
			//printf("Im'coming the RouteIutput lookupgolbal\n");
			//printf("The RouteIntput lookupglobal packet set before nodeid is:%d\n", packet->GetNodeId());
			//printf("The header source is:%d,\n", int2ip(header.GetSource()));
			rtentry = LookupGlobal(header, packet);
			//printf("packet nodeid:%d\n", packet->GetNodeId());
		}
		if (rtentry != 0)
		{
			NS_LOG_LOGIC("Found unicast destination- calling unicast callback");
			ucb(rtentry, packet, header);
			return true;
		}
		else
		{
			NS_LOG_LOGIC("Did not find unicast destination- returning false");
			return false; // Let other routing protocols try to handle this
						  // route request.
		}
	}
	void
		Ipv4GlobalRouting::NotifyInterfaceUp(uint32_t i)
	{
		NS_LOG_FUNCTION(this << i);
		if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0)  // avoid startup events
		{
			GlobalRouteManager::DeleteGlobalRoutes();
			GlobalRouteManager::BuildGlobalRoutingDatabase();
			GlobalRouteManager::InitializeRoutes();
		}
	}

	void
		Ipv4GlobalRouting::NotifyInterfaceDown(uint32_t i)
	{
		NS_LOG_FUNCTION(this << i);
		if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0)  // avoid startup events
		{
			GlobalRouteManager::DeleteGlobalRoutes();
			GlobalRouteManager::BuildGlobalRoutingDatabase();
			GlobalRouteManager::InitializeRoutes();
		}
	}

	void
		Ipv4GlobalRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
	{
		NS_LOG_FUNCTION(this << interface << address);
		if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0)  // avoid startup events
		{
			GlobalRouteManager::DeleteGlobalRoutes();
			GlobalRouteManager::BuildGlobalRoutingDatabase();
			GlobalRouteManager::InitializeRoutes();
		}
	}

	void
		Ipv4GlobalRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
	{
		NS_LOG_FUNCTION(this << interface << address);
		if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0)  // avoid startup events
		{
			GlobalRouteManager::DeleteGlobalRoutes();
			GlobalRouteManager::BuildGlobalRoutingDatabase();
			GlobalRouteManager::InitializeRoutes();
		}
	}

	void
		Ipv4GlobalRouting::SetIpv4(Ptr<Ipv4> ipv4)
	{
		NS_LOG_FUNCTION(this << ipv4);
		NS_ASSERT(m_ipv4 == 0 && ipv4 != 0);
		m_ipv4 = ipv4;
	}

	void 
		Ipv4GlobalRouting::printtpP0()
	{
		double temp = countnow0 - countpre0;
		countpre0 = countnow0;
		std::ofstream outfile;
		outfile.open("D:\\data\\tpP0(72).txt", std::ios::app);
		outfile << Simulator::Now() << " " <<countnow0<<" "<< temp <<" "<< (double)temp*1000*8/0.001 << std::endl;
		outfile.close();
		Simulator::Schedule(Seconds(0.001), &Ipv4GlobalRouting::printtpP0, this);
	}
	void
		Ipv4GlobalRouting::printtpP1()
	{
		double temp = countnow1 - countpre1;
		countpre1 = countnow1;
		std::ofstream outfile;
		outfile.open("D:\\data\\tpP1(72).txt", std::ios::app);
		outfile << Simulator::Now() << " " << countnow1 << " " << temp << " " << (double)temp * 1000 * 8 / 0.001 << std::endl;
		outfile.close();
		Simulator::Schedule(Seconds(0.001), &Ipv4GlobalRouting::printtpP1, this);
	}

	void
		Ipv4GlobalRouting::printtpAll()
	{
		double temp = countnow - countpre;
		countpre = countnow;
		std::ofstream outfile;
		outfile.open("D:\\data\\tpAll(72).txt", std::ios::app);
		outfile << Simulator::Now() << " " << countnow << " " << temp << " " << (double)temp * 1000 * 8 / 0.001 << std::endl;
		outfile.close();
		Simulator::Schedule(Seconds(0.001), &Ipv4GlobalRouting::printtpAll, this);
	}

	void
		Ipv4GlobalRouting::Setforwardtb(int id, int Path,int order)
	{
		forwardtb[id][Path] = order;
	}

	void 
		Ipv4GlobalRouting::dretimer0()
	{
		//std::cout << "I'coming dretimer0" << std::endl;
		  CECount0 = (uint32_t)(CECount0 * alpha);
		  CECount1 = (uint32_t)(CECount1 * alpha);
		//EventId eid;
		//printf("%d\n", CECount);
		//if (CECount0 < 1) CECount0 = 0;
		  /*std::ofstream outfile;
		  outfile.open("D:\\data\\dretimer0(51).txt", std::ios::app);
		  outfile << Simulator::Now() << " " << CECount0 << std::endl;
		  outfile.close();
		  outfile.open("D:\\data\\dretimer1(51).txt", std::ios::app);
		  outfile << Simulator::Now() << " " << CECount1 << std::endl;
		  outfile.close();*/
		Simulator::Schedule(Seconds(0.0001), &Ipv4GlobalRouting::dretimer0, this);
	}

	//void
	//	Ipv4GlobalRouting::dretimer1()
	//{
	//	//std::cout << "I'coming dretimer1" << std::endl;
	//	CECount1 = (uint32_t)(CECount1 * alpha);
	//	//EventId eid;
	//	//printf("%d\n", CECount);
	//	//if (CECount1 < 1) CECount1 = 0;
	//	Simulator::Schedule(Seconds(0.0001), &Ipv4GlobalRouting::dretimer1, this);
	//}

} // namespace ns3
