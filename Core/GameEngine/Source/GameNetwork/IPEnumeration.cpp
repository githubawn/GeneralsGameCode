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
#include <ws2tcpip.h>

#include "GameNetwork/IPEnumeration.h"
#include "GameNetwork/networkutil.h"
#include "GameClient/ClientInstance.h"

//-------------------------------------------------------------------------------

IPEnumeration::IPEnumeration()
{
	m_IPlist = nullptr;
	m_isWinsockInitialized = false;
}

IPEnumeration::~IPEnumeration()
{
	reset();
}

void IPEnumeration::reset()
{
	EnumeratedIP *IP;
	while (m_IPlist) {
		IP = m_IPlist;
		m_IPlist = m_IPlist->getNext();
		deleteInstance(IP);
	}

	if (m_isWinsockInitialized) {
		WSACleanup();
		m_isWinsockInitialized = false;
	}
}

EnumeratedIP * IPEnumeration::getAddresses()
{
	if (m_IPlist) {
		return m_IPlist;
	}

	// Initialize winsock if necessary
	if (!m_isWinsockInitialized) {
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return nullptr;
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return nullptr;
		}
		m_isWinsockInitialized = true;
	}

	SOCKET sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd != INVALID_SOCKET)
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
					 sizeof(InterfaceList), &nBytesReturned, 0, 0) == 0)
		{
			int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
			for (int i = 0; i < nNumInterfaces; ++i)
			{
				if (InterfaceList[i].iiFlags & IFF_LOOPBACK)
					continue;

				if (!(InterfaceList[i].iiFlags & IFF_UP))
					continue;

				sockaddr_in *pAddress = (sockaddr_in *)&InterfaceList[i].iiAddress;
				UnsignedInt ip = ntohl(pAddress->sin_addr.s_addr);
				if (ip == 0 || ip == 0x7F000001)
					continue;

				UnsignedByte a = (UnsignedByte)(ip >> 24);
				UnsignedByte b = (UnsignedByte)(ip >> 16);
				UnsignedByte c = (UnsignedByte)(ip >> 8);
				UnsignedByte d = (UnsignedByte)(ip);
				addNewIP(a, b, c, d);
			}
		}
		closesocket(sd);
	}

	return m_IPlist;
}

void IPEnumeration::addNewIP( UnsignedByte a, UnsignedByte b, UnsignedByte c, UnsignedByte d )
{
	EnumeratedIP *newIP = newInstance(EnumeratedIP);

	AsciiString str;
	str.format("%d.%d.%d.%d", (int)a, (int)b, (int)c, (int)d);

	UnsignedInt ip = AssembleIp(a, b, c, d);

	newIP->setIPstring(str);
	newIP->setIP(ip);

	DEBUG_LOG(("IP: 0x%8.8X (%s)", ip, str.str()));

	// Add the IP to the list in ascending order
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


