/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/networkutil.h"
#include "GameClient/ClientInstance.h"

IPEnumeration::IPEnumeration()
{
	m_IPlist = nullptr;
	m_isWinsockInitialized = false;
}

IPEnumeration::~IPEnumeration()
{
	if (m_isWinsockInitialized)
	{
		WSACleanup();
		m_isWinsockInitialized = false;
	}

	EnumeratedIP *ip = m_IPlist;
	while (ip)
	{
		ip = ip->getNext();
		deleteInstance(m_IPlist);
		m_IPlist = ip;
	}
}

#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")

EnumeratedIP * IPEnumeration::getAddresses()
{
	if (m_IPlist)
		return m_IPlist;

	if (!m_isWinsockInitialized)
	{
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return nullptr;
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) != 2)) {
			WSACleanup();
			return nullptr;
		}
		m_isWinsockInitialized = true;
	}

	PMIB_IPADDRTABLE pIPAddrTable = nullptr;
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;

	// Initial call to get the required size
	if (GetIpAddrTable(nullptr, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
	{
		pIPAddrTable = (MIB_IPADDRTABLE *)malloc(dwSize);
	}

	if (pIPAddrTable == nullptr)
	{
		DEBUG_LOG(("Failed to allocate memory for IP Address Table"));
		return nullptr;
	}

	if ((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0)) != NO_ERROR)
	{
		DEBUG_LOG(("GetIpAddrTable failed with error %d", dwRetVal));
		free(pIPAddrTable);
		return nullptr;
	}

	DEBUG_LOG(("GetIpAddrTable returned %d entries", pIPAddrTable->dwNumEntries));

	// TheSuperHackers @feature Add one unique local host IP address for each multi client instance.
	if (rts::ClientInstance::isMultiInstance())
	{
		const UnsignedInt id = rts::ClientInstance::getInstanceId();
		addNewIP(
			127,
			(UnsignedByte)(id >> 16),
			(UnsignedByte)(id >> 8),
			(UnsignedByte)(id),
			AssembleIp(255, 0, 0, 0)); // Standard Class A loopback mask
	}

	for (DWORD i = 0; i < pIPAddrTable->dwNumEntries; i++)
	{
		UnsignedInt ip = pIPAddrTable->table[i].dwAddr;
		UnsignedInt mask = pIPAddrTable->table[i].dwMask;

		// Convert back from network byte order to components for addNewIP
		// Note: dwAddr is already in network byte order? 
		// Wait, the original code used hostEnt->h_addr_list which is in network byte order.
		// addNewIP expects raw bytes as they appear in the IP.
		
		UnsignedByte a = (UnsignedByte)(ip & 0xFF);
		UnsignedByte b = (UnsignedByte)((ip >> 8) & 0xFF);
		UnsignedByte c = (UnsignedByte)((ip >> 16) & 0xFF);
		UnsignedByte d = (UnsignedByte)((ip >> 24) & 0xFF);

		// If it's a loopback (127.x.x.x), we might want to skip it if it's not the primary 127.0.0.1
		// but for now let's just add everything.
		if (ip != 0) // Skip 0.0.0.0
		{
			addNewIP(a, b, c, d, mask);
		}
	}

	free(pIPAddrTable);
	return m_IPlist;
}

void IPEnumeration::addNewIP( UnsignedByte a, UnsignedByte b, UnsignedByte c, UnsignedByte d, UnsignedInt mask )
{
	EnumeratedIP *newIP = newInstance(EnumeratedIP);

	AsciiString str;
	str.format("%d.%d.%d.%d", (int)a, (int)b, (int)c, (int)d);

	UnsignedInt ip = AssembleIp(a, b, c, d);

	newIP->setIPstring(str);
	newIP->setIP(ip);
	newIP->setMask(mask);

	DEBUG_LOG(("IP: 0x%8.8X Mask: 0x%8.8X (%s)", ip, mask, str.str()));

	// Add the IP to the list in ascending order of IP value by default
	// This will be re-sorted by latency if measureLatencies is called.
	if (!m_IPlist)
	{
		m_IPlist = newIP;
		newIP->setNext(nullptr);
	}
	else
	{
		if (newIP->getIP() < m_IPlist->getIP())
		{
			newIP->setNext(m_IPlist);
			m_IPlist = newIP;
		}
		else
		{
			EnumeratedIP *p = m_IPlist;
			while (p->getNext() && p->getNext()->getIP() < newIP->getIP())
			{
				p = p->getNext();
			}
			newIP->setNext(p->getNext());
			p->setNext(newIP);
		}
	}
}

static UnsignedInt measureLatencyToIP(UnsignedInt srcIP, UnsignedInt destIP)
{
	if (destIP == 0) return 0xFFFFFFFF;

	HANDLE hIcmpFile = IcmpCreateFile();
	if (hIcmpFile == INVALID_HANDLE_VALUE) return 0xFFFFFFFF;

	char sendData[32] = "Data Buffer";
	DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
	LPVOID replyBuffer = (char *)malloc(replySize);
	if (!replyBuffer) {
		IcmpCloseHandle(hIcmpFile);
		return 0xFFFFFFFF;
	}

	IP_OPTION_INFORMATION options = { 0 };
	options.Ttl = 128;

	// Use IcmpSendEcho2 to specify source address if needed, or just IcmpSendEcho
	// Actually IcmpSendEcho doesn't let us pick source. IcmpSendEcho2 does but it's async.
	// For simplicity, we'll just use IcmpSendEcho and trust the OS routing for now, 
	// OR we use a trick to bind a socket.
	// Actually, on modern Windows, IcmpSendEcho2 can be used synchronously.
	
	DWORD dwRetVal = IcmpSendEcho(hIcmpFile, destIP, sendData, sizeof(sendData), &options, replyBuffer, replySize, 100); // 100ms timeout
	
	UnsignedInt latency = 0xFFFFFFFF;
	if (dwRetVal != 0) {
		PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)replyBuffer;
		latency = pEchoReply->RoundTripTime;
	}

	free(replyBuffer);
	IcmpCloseHandle(hIcmpFile);
	return latency;
}

void IPEnumeration::measureLatencies(UnsignedInt targetIP)
{
	// 1. If targetIP is provided, ping it from each interface.
	// 2. If not, find gateways and ping them.

	ULONG outBufLen = 15000;
	PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
	if (!pAddresses) return;

	ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS;
	DWORD dwRetVal = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
	if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
		free(pAddresses);
		pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
		if (!pAddresses) return;
		dwRetVal = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
	}

	if (dwRetVal == NO_ERROR) {
		for (EnumeratedIP *ip = m_IPlist; ip; ip = ip->getNext()) {
			UnsignedInt bestLatency = 0xFFFFFFFF;
			
			if (targetIP != 0) {
				bestLatency = measureLatencyToIP(ip->getIP(), targetIP);
			} else {
				// Find this IP in the adapters list to get its gateway
				PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
				while (pCurrAddresses) {
					PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
					bool match = false;
					while (pUnicast) {
						if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
							sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
							if (sa_in->sin_addr.s_addr == ip->getIP()) {
								match = true;
								break;
							}
						}
						pUnicast = pUnicast->Next;
					}

					if (match) {
						PIP_ADAPTER_GATEWAY_ADDRESS_LH pGateway = pCurrAddresses->FirstGatewayAddress;
						while (pGateway) {
							if (pGateway->Address.lpSockaddr->sa_family == AF_INET) {
								sockaddr_in *sa_gw = (sockaddr_in *)pGateway->Address.lpSockaddr;
								UnsignedInt lat = measureLatencyToIP(ip->getIP(), sa_gw->sin_addr.s_addr);
								if (lat < bestLatency) bestLatency = lat;
							}
							pGateway = pGateway->Next;
						}
						break;
					}
					pCurrAddresses = pCurrAddresses->Next;
				}
			}
			ip->setLatency(bestLatency);
		}
	}

	if (pAddresses) free(pAddresses);

	// Re-sort the list by latency
	if (m_IPlist && m_IPlist->getNext()) {
		EnumeratedIP *newList = nullptr;
		while (m_IPlist) {
			EnumeratedIP *node = m_IPlist;
			m_IPlist = m_IPlist->getNext();
			
			// Insert node into newList sorted by latency
			if (!newList || node->getLatency() < newList->getLatency()) {
				node->setNext(newList);
				newList = node;
			} else {
				EnumeratedIP *p = newList;
				while (p->getNext() && p->getNext()->getLatency() <= node->getLatency()) {
					p = p->getNext();
				}
				node->setNext(p->getNext());
				p->setNext(node);
			}
		}
		m_IPlist = newList;
	}
}

UnsignedInt IPEnumeration::getBestLatencyIP(UnsignedInt targetIP)
{
	measureLatencies(targetIP);
	if (m_IPlist) {
		// Skip loopback if we have other options?
		// For now just return the first one as it's sorted by latency.
		return m_IPlist->getIP();
	}
	return 0;
}

AsciiString IPEnumeration::getMachineName()
{
	if (!m_isWinsockInitialized)
	{
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return "";
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return "";
		}
		m_isWinsockInitialized = true;
	}

	// get the local machine's host name
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)))
	{
		DEBUG_LOG(("Failed call to gethostname; WSAGetLastError returned %d", WSAGetLastError()));
		return "";
	}

	return AsciiString(hostname);
}


