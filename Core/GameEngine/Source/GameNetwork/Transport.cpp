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

#include "Common/crc.h"
#include "GameNetwork/Transport.h"
#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/IPEnumeration.h"


//--------------------------------------------------------------------------
// Packet-level encryption is an XOR operation, for speed reasons.  To get
// the max throughput, we only XOR whole 4-byte words, so the last bytes
// can be non-XOR'd.

// This assumes the buf is a multiple of 4 bytes.  Extra is not encrypted.
static inline void encryptBuf( unsigned char *buf, Int len )
{
	UnsignedInt mask = 0x0000Fade;

	UnsignedInt *uintPtr = (UnsignedInt *) (buf);

	for (int i=0 ; i<len/4 ; i++) {
		*uintPtr = (*uintPtr) ^ mask;
		*uintPtr = htonl(*uintPtr);
		uintPtr++;
		mask += 0x00000321; // just for fun
	}
}

// This assumes the buf is a multiple of 4 bytes.  Extra is not encrypted.
static inline void decryptBuf( unsigned char *buf, Int len )
{
	UnsignedInt mask = 0x0000Fade;

	UnsignedInt *uintPtr = (UnsignedInt *) (buf);

	for (int i=0 ; i<len/4 ; i++) {
		*uintPtr = htonl(*uintPtr);
		*uintPtr = (*uintPtr) ^ mask;
		uintPtr++;
		mask += 0x00000321; // just for fun
	}
}

//--------------------------------------------------------------------------

Transport::Transport()
{
	m_winsockInit = false;
	m_portRerouted = false;
	m_requestedPort = 0;
	m_instanceOffset = 0;
	m_udpsock = nullptr;
	m_portBase = 0;
}

Transport::~Transport()
{
	reset();
}

Bool Transport::init( AsciiString ip, UnsignedShort port )
{
	return init(ResolveIP(ip), port);
}

Bool Transport::init( UnsignedInt ip, UnsignedShort port )
{
	// ----- Initialize Winsock -----
	if (!m_winsockInit)
	{
		WORD verReq = MAKEWORD(2, 2);
		WSADATA wsadata;

		int err = WSAStartup(verReq, &wsadata);
		if (err != 0) {
			return false;
		}

		if ((LOBYTE(wsadata.wVersion) != 2) || (HIBYTE(wsadata.wVersion) !=2)) {
			WSACleanup();
			return false;
		}
		m_winsockInit = true;
	}

	// ------- Bind our port --------
	delete m_udpsock;
	m_udpsock = NEW UDP();

	if (!m_udpsock)
		return false;

	UnsignedShort basePort = m_portBase ? m_portBase : port;
	UnsignedShort requestedPort = port;
	UnsignedShort boundPort = 0;

	if (m_udpsock->Bind(ip, requestedPort) == 0)
	{
		boundPort = requestedPort;
	}
	else
	{
		for (UnsignedInt offset = 0; offset < LAN_MAX_CANDIDATE_PORTS; ++offset)
		{
			UnsignedShort candidatePort = getRealPortFromInstanceOffset(basePort, offset);
			if (candidatePort != requestedPort)
			{
				if (m_udpsock->Bind(ip, candidatePort) == 0)
				{
					boundPort = candidatePort;
					break;
				}
			}
		}
	}

	if (boundPort == 0) {
		DEBUG_CRASH(("Could not bind to 0x%8.8X:%d", ip, port));
		DEBUG_LOG(("Transport::init - Failure to bind socket on any candidate port"));
		delete m_udpsock;
		m_udpsock = nullptr;
		return false;
	}

	m_requestedPort = basePort;
	m_port = boundPort;
	m_instanceOffset = getInstanceOffsetFromRealPort(basePort, boundPort);
	m_portRerouted = (boundPort != basePort);

	if (m_portRerouted)
	{
		DEBUG_LOG(("Transport::init - Port %d in use, auto-switched to port %d", basePort, boundPort));
	}

	// ------- Clear buffers --------
	int i=0;
	for (; i<MAX_MESSAGES; ++i)
	{
		m_outBuffer[i].length = 0;
		m_inBuffer[i].length = 0;
#if defined(RTS_DEBUG)
		m_delayedInBuffer[i].message.length = 0;
#endif
	}
	for (i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		m_incomingBytes[i] = 0;
		m_outgoingBytes[i] = 0;
		m_unknownBytes[i] = 0;
		m_incomingPackets[i] = 0;
		m_outgoingPackets[i] = 0;
		m_unknownPackets[i] = 0;
	}
	m_statisticsSlot = 0;
	m_lastSecond = timeGetTime();

	m_port = boundPort;

#if defined(RTS_DEBUG)
	if (TheGlobalData->m_latencyAverage > 0 || TheGlobalData->m_latencyNoise)
		m_useLatency = true;

	if (TheGlobalData->m_packetLoss)
		m_usePacketLoss = true;
#endif

	return true;
}

void Transport::reset()
{
	delete m_udpsock;
	m_udpsock = nullptr;

	if (m_winsockInit)
	{
		WSACleanup();
		m_winsockInit = false;
	}
}

UnsignedInt Transport::makeInstanceIP(UnsignedInt realIP, UnsignedInt instanceOffset)
{
	return realIP ^ (instanceOffset << 28);
}

UnsignedShort Transport::getRealPortFromInstanceOffset(UnsignedShort basePort, UnsignedInt offset)
{
	if (offset == 0) return basePort;
	if (basePort == 8086)
	{
		UnsignedShort port = basePort + (UnsignedShort)offset;
		if (port >= NETWORK_BASE_PORT_NUMBER) port++;
		return port;
	}
	else if (basePort == NETWORK_BASE_PORT_NUMBER)
	{
		return (UnsignedShort)(8100 + offset);
	}
	return basePort + (UnsignedShort)offset;
}

UnsignedInt Transport::getInstanceOffsetFromRealPort(UnsignedShort basePort, UnsignedShort realPort)
{
	if (realPort == basePort) return 0;
	if (basePort == 8086)
	{
		if (realPort > NETWORK_BASE_PORT_NUMBER)
			return realPort - basePort - 1;
		return realPort - basePort;
	}
	else if (basePort == NETWORK_BASE_PORT_NUMBER)
	{
		if (realPort >= 8100)
			return realPort - 8100;
	}
	return realPort - basePort;
}

UnsignedShort Transport::lookupRealPort(UnsignedInt instanceIP) const
{
	std::map<UnsignedInt, RealEndpoint>::const_iterator it = m_instanceToReal.find(instanceIP);
	if (it == m_instanceToReal.end())
		return 0;
	return it->second.port;
}

Bool Transport::update()
{
	Bool retval = TRUE;
	if (doRecv() == FALSE && m_udpsock && m_udpsock->GetStatus() == UDP::ADDRNOTAVAIL)
	{
		retval = FALSE;
	}
	DEBUG_ASSERTLOG(retval, ("WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
	if (doSend() == FALSE && m_udpsock && m_udpsock->GetStatus() == UDP::ADDRNOTAVAIL)
	{
		retval = FALSE;
	}
	DEBUG_ASSERTLOG(retval, ("WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
	return retval;
}

Bool Transport::doSend() {
	if (!m_udpsock)
	{
		DEBUG_LOG(("Transport::doSend() - m_udpSock is null!"));
		return FALSE;
	}

	Bool retval = TRUE;

	// Statistics gathering
	UnsignedInt now = timeGetTime();
	if (m_lastSecond + 1000 < now)
	{
		m_lastSecond = now;
		m_statisticsSlot = (m_statisticsSlot + 1) % MAX_TRANSPORT_STATISTICS_SECONDS;
		m_outgoingPackets[m_statisticsSlot] = 0;
		m_outgoingBytes[m_statisticsSlot] = 0;
		m_incomingPackets[m_statisticsSlot] = 0;
		m_incomingBytes[m_statisticsSlot] = 0;
		m_unknownPackets[m_statisticsSlot] = 0;
		m_unknownBytes[m_statisticsSlot] = 0;
	}

	// Send all messages
	int i;
	for (i=0; i<MAX_MESSAGES; ++i)
	{
		if (m_outBuffer[i].length != 0)
		{
			int bytesSent = 0;
			// TheSuperHackers @info The handling of data sizing of the payload within a UDP packet is confusing due to the current networking implementation
			// The max game packet size needs to be smaller than max udp payload by sizeof(TransportMessageHeader)
			// But the max network message size needs to include the bytes of the transport message header and equal the max udp payload
			// Therefore, transmitted data needs to add the extra bytes of the network header to the payloads length
			int bytesToSend = m_outBuffer[i].length + sizeof(TransportMessageHeader);

			UnsignedInt sendAddr = m_outBuffer[i].addr;
			UnsignedShort sendPort = m_outBuffer[i].port;
			if (m_outBuffer[i].addr == INADDR_BROADCAST)
			{
				Bool anySent = FALSE;
				UnsignedShort basePort = m_portBase ? m_portBase : sendPort;

				IPEnumeration IPs;

				for (UnsignedInt o = 0; o < LAN_MAX_CANDIDATE_PORTS; ++o)
				{
					UnsignedShort targetPort = getRealPortFromInstanceOffset(basePort, o);
					if (m_udpsock->Write((unsigned char *)(&m_outBuffer[i]), bytesToSend,
							INADDR_BROADCAST, targetPort) > 0)
					{
						anySent = TRUE;
					}

					EnumeratedIP *IPlist = IPs.getAddresses();
					while (IPlist)
					{
						UnsignedInt ip = IPlist->getIP();
						UnsignedInt mask = IPlist->getSubnetMask();
						UnsignedInt subnetBcast = (ip & mask) | ~mask;

						if (subnetBcast != 0 && subnetBcast != INADDR_BROADCAST)
						{
							if (m_udpsock->Write((unsigned char *)(&m_outBuffer[i]), bytesToSend,
									subnetBcast, targetPort) > 0)
							{
								anySent = TRUE;
							}
						}

						IPlist = IPlist->getNext();
					}
				}

				if (anySent)
				{
					m_outgoingPackets[m_statisticsSlot]++;
					m_outgoingBytes[m_statisticsSlot] += m_outBuffer[i].length + sizeof(TransportMessageHeader);
					m_outBuffer[i].length = 0;
				}
				else
				{
					retval = FALSE;
				}
				continue;
			}

			std::map<UnsignedInt, RealEndpoint>::const_iterator it = m_instanceToReal.find(m_outBuffer[i].addr);
			if (it != m_instanceToReal.end())
			{
				sendAddr = it->second.ip;
				sendPort = it->second.port;
			}
			else
			{
				UnsignedShort basePort = m_portBase ? m_portBase : sendPort;
				UnsignedInt instanceOffset = (m_outBuffer[i].addr >> 28) & 0xF;
				UnsignedInt realIP = m_outBuffer[i].addr & 0x0FFFFFFF;
				if (realIP == 0) realIP = INADDR_BROADCAST;
				sendAddr = realIP;
				sendPort = getRealPortFromInstanceOffset(basePort, instanceOffset);
			}

			// Send this message
			if ((bytesSent = m_udpsock->Write((unsigned char *)(&m_outBuffer[i]), bytesToSend, sendAddr, sendPort)) > 0)
			{
				//DEBUG_LOG(("Sending %d bytes to %d.%d.%d.%d:%d", bytesToSend, PRINTF_IP_AS_4_INTS(m_outBuffer[i].addr), m_outBuffer[i].port));
				m_outgoingPackets[m_statisticsSlot]++;
				m_outgoingBytes[m_statisticsSlot] += m_outBuffer[i].length + sizeof(TransportMessageHeader);
				m_outBuffer[i].length = 0;  // Remove from queue
				if (bytesSent != bytesToSend)
				{
					DEBUG_LOG(("Transport::doSend - wanted to send %d bytes, only sent %d bytes to %d.%d.%d.%d:%d",
						bytesToSend, bytesSent,
						PRINTF_IP_AS_4_INTS(m_outBuffer[i].addr), m_outBuffer[i].port));
				}
			}
			else
			{
				//DEBUG_LOG(("Could not write to socket!!!  Not discarding message!"));
				retval = FALSE;
				//DEBUG_LOG(("Transport::doSend returning FALSE"));
			}
		}
	}

#if defined(RTS_DEBUG)
	// Latency simulation - deliver anything we're holding on to that is ready
	if (m_useLatency)
	{
		for (i=0; i<MAX_MESSAGES; ++i)
		{
			if (m_delayedInBuffer[i].message.length != 0 && m_delayedInBuffer[i].deliveryTime <= now)
			{
				for (int j=0; j<MAX_MESSAGES; ++j)
				{
					if (m_inBuffer[j].length == 0)
					{
						// Empty slot; use it
						memcpy(&m_inBuffer[j], &m_delayedInBuffer[i].message, sizeof(TransportMessage));
						m_delayedInBuffer[i].message.length = 0;
						break;
					}
				}
			}
		}
	}
#endif
	return retval;
}

Bool Transport::doRecv()
{
	if (!m_udpsock)
	{
		DEBUG_LOG(("Transport::doRecv() - m_udpSock is null!"));
		return FALSE;
	}

	Bool retval = TRUE;

	// Read in anything on our socket
	sockaddr_in from;
#if defined(RTS_DEBUG)
	UnsignedInt now = timeGetTime();
#endif
	// TheSuperHackers @info The handling of data sizing of the payload within a UDP packet is confusing due to the current networking implementation
	// The max game packet size needs to be smaller than max udp payload by sizeof(TransportMessageHeader)
	// But the max network message size needs to include the bytes of the transport message header and equal the max udp payload
	// Therefore, when receiving data we use the max udp payload size to receive the game packet payload and network header
	TransportMessage incomingMessage;
	unsigned char *buf = (unsigned char *)&incomingMessage;
	int len = MAX_NETWORK_MESSAGE_LEN;
//	DEBUG_LOG(("Transport::doRecv - checking"));
	while ( (len=m_udpsock->Read(buf, MAX_NETWORK_MESSAGE_LEN, &from)) > 0 )
	{
#if defined(RTS_DEBUG)
		// Packet loss simulation
		if (m_usePacketLoss)
		{
			if ( TheGlobalData->m_packetLoss >= GameClientRandomValue(0, 100) )
			{
				continue;
			}
		}
#endif

//		DEBUG_LOG(("Transport::doRecv - Got something! len = %d", len));
		// Decrypt the packet
//		DEBUG_LOG_RAW(("buffer = "));
//		for (Int munkee = 0; munkee < len; ++munkee) {
//			DEBUG_LOG_RAW(("%02x", *(buf + munkee)));
//		}
//		DEBUG_LOG_RAW(("\n"));
		decryptBuf(buf, len);

		incomingMessage.length = len - sizeof(TransportMessageHeader);

		if (len <= sizeof(TransportMessageHeader) || !isGeneralsPacket( &incomingMessage ))
		{
			DEBUG_LOG(("Transport::doRecv - unknownPacket! len = %d", len));
			m_unknownPackets[m_statisticsSlot]++;
			m_unknownBytes[m_statisticsSlot] += len;
			continue;
		}

		// Something there; stick it somewhere
//		DEBUG_LOG(("Saw %d bytes from %d:%d", len, ntohl(from.sin_addr.S_un.S_addr), ntohs(from.sin_port)));
		m_incomingPackets[m_statisticsSlot]++;
		m_incomingBytes[m_statisticsSlot] += len;

		UnsignedInt msgAddr = ntohl(from.sin_addr.S_un.S_addr);
		UnsignedShort msgPort = ntohs(from.sin_port);
		UnsignedShort basePort = m_portBase ? m_portBase : msgPort;
		UnsignedInt offset = getInstanceOffsetFromRealPort(basePort, msgPort);
		if (offset < LAN_MAX_CANDIDATE_PORTS)
		{
			UnsignedInt instanceIP = makeInstanceIP(msgAddr, offset);
			RealEndpoint ep;
			ep.ip = msgAddr;
			ep.port = msgPort;
			m_instanceToReal[instanceIP] = ep;
			msgAddr = instanceIP;
		}

		for (int i=0; i<MAX_MESSAGES; ++i)
		{
#if defined(RTS_DEBUG)
			// Latency simulation
			if (m_useLatency)
			{
				if (m_delayedInBuffer[i].message.length == 0)
				{
					// Empty slot; use it
					m_delayedInBuffer[i].deliveryTime =
						now + TheGlobalData->m_latencyAverage +
						(Int)(TheGlobalData->m_latencyAmplitude * sin(now * TheGlobalData->m_latencyPeriod)) +
						GameClientRandomValue(-TheGlobalData->m_latencyNoise, TheGlobalData->m_latencyNoise);
					m_delayedInBuffer[i].message.length = incomingMessage.length;
					m_delayedInBuffer[i].message.addr = msgAddr;
					m_delayedInBuffer[i].message.port = msgPort;
					memcpy(&m_delayedInBuffer[i].message, buf, len);
					break;
				}
			}
			else
			{
#endif
				if (m_inBuffer[i].length == 0)
				{
					// Empty slot; use it
					memcpy(&m_inBuffer[i], buf, len);
					m_inBuffer[i].length = incomingMessage.length;
					m_inBuffer[i].addr = msgAddr;
					m_inBuffer[i].port = msgPort;
					break;
				}
#if defined(RTS_DEBUG)
			}
#endif
		}
		//DEBUG_ASSERTCRASH(i<MAX_MESSAGES, ("Message lost!"));
	}

	if (len == -1) {
		// there was a socket error trying to perform a read.
		//DEBUG_LOG(("Transport::doRecv returning FALSE"));
		retval = FALSE;
	}

	return retval;
}

Bool Transport::queueSend(UnsignedInt addr, UnsignedShort port, const UnsignedByte *buf, Int len /*,
						  NetMessageFlags flags, Int id */)
{
	int i;

	if (len < 1 || len > MAX_PACKET_SIZE)
	{
		DEBUG_LOG(("Transport::queueSend - Invalid Packet size"));
		return false;
	}

	for (i=0; i<MAX_MESSAGES; ++i)
	{
		if (m_outBuffer[i].length == 0)
		{
			// Insert data here
			m_outBuffer[i].length = len;
			memcpy(m_outBuffer[i].data, buf, len);
			m_outBuffer[i].addr = addr;
			m_outBuffer[i].port = port;
//			m_outBuffer[i].header.flags = flags;
//			m_outBuffer[i].header.id = id;
			m_outBuffer[i].header.magic = GENERALS_MAGIC_NUMBER;

			CRC crc;
			crc.computeCRC( (unsigned char *)(&(m_outBuffer[i].header.magic)), m_outBuffer[i].length + sizeof(TransportMessageHeader) - sizeof(UnsignedInt) );
//			DEBUG_LOG(("About to assign the CRC for the packet"));
			m_outBuffer[i].header.crc = crc.get();

			// Encrypt packet
//			DEBUG_LOG(("buffer: "));
			encryptBuf((unsigned char *)&m_outBuffer[i], len + sizeof(TransportMessageHeader));
//			DEBUG_LOG((""));

			return true;
		}
	}
	DEBUG_LOG(("Send Queue is getting full, dropping packets"));
	return false;
}

Bool Transport::isGeneralsPacket( TransportMessage *msg )
{
	if (!msg)
		return false;

	if (msg->length < 0 || msg->length > MAX_NETWORK_MESSAGE_LEN)
		return false;

	CRC crc;
//	crc.computeCRC( (unsigned char *)msg->data, msg->length );
	crc.computeCRC( (unsigned char *)(&(msg->header.magic)), msg->length + sizeof(TransportMessageHeader) - sizeof(UnsignedInt) );

	if (crc.get() != msg->header.crc)
		return false;

	if (msg->header.magic != GENERALS_MAGIC_NUMBER)
		return false;

	return true;
}

// Statistics ---------------------------------------------------
Real Transport::getIncomingBytesPerSecond()
{
	Real val = 0.0;
	for (int i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		if (i != m_statisticsSlot)
			val += m_incomingBytes[i];
	}
	return val / (MAX_TRANSPORT_STATISTICS_SECONDS-1);
}

Real Transport::getIncomingPacketsPerSecond()
{
	Real val = 0.0;
	for (int i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		if (i != m_statisticsSlot)
			val += m_incomingPackets[i];
	}
	return val / (MAX_TRANSPORT_STATISTICS_SECONDS-1);
}

Real Transport::getOutgoingBytesPerSecond()
{
	Real val = 0.0;
	for (int i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		if (i != m_statisticsSlot)
			val += m_outgoingBytes[i];
	}
	return val / (MAX_TRANSPORT_STATISTICS_SECONDS-1);
}

Real Transport::getOutgoingPacketsPerSecond()
{
	Real val = 0.0;
	for (int i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		if (i != m_statisticsSlot)
			val += m_outgoingPackets[i];
	}
	return val / (MAX_TRANSPORT_STATISTICS_SECONDS-1);
}

Real Transport::getUnknownBytesPerSecond()
{
	Real val = 0.0;
	for (int i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		if (i != m_statisticsSlot)
			val += m_unknownBytes[i];
	}
	return val / (MAX_TRANSPORT_STATISTICS_SECONDS-1);
}

Real Transport::getUnknownPacketsPerSecond()
{
	Real val = 0.0;
	for (int i=0; i<MAX_TRANSPORT_STATISTICS_SECONDS; ++i)
	{
		if (i != m_statisticsSlot)
			val += m_unknownPackets[i];
	}
	return val / (MAX_TRANSPORT_STATISTICS_SECONDS-1);
}



