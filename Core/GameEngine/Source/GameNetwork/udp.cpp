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

// FILE: Udp.cpp //////////////////////////////////////////////////////////////
// Implementation of UDP socket wrapper class (taken from wnet lib)
// Author: Matthew D. Campbell, July 2001
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/GameEngine.h"
//#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/udp.h"

// TheSuperHackers @build bobtista 13/06/2026 BSD sockets take socklen_t* for the
// address-length args where winsock takes int*; alias so both toolchains build
// (Windows/VC6 keep int).
#ifdef _WIN32
typedef int ggc_socklen_t;
#else
typedef socklen_t ggc_socklen_t;
#endif


//-------------------------------------------------------------------------

#if defined(__EMSCRIPTEN__)
// TheSuperHackers @feature githubawn 27/06/2026 WebAssembly LAN networking.
// Browsers cannot open raw UDP/TCP sockets, so on Emscripten the UDP class is
// reimplemented on top of a single WebSocket to a relay (relay.py). Every datagram is
// framed [srcIP|srcPort|dstIP|dstPort|payload] (big-endian header) and the relay routes
// unicast by destination virtual IP and 255.255.255.255 broadcast (LANAPI discovery) to
// all peers. One WebSocket is shared by every UDP in the process; inbound datagrams are
// demuxed into per-bound-port queues. Everything runs on the main browser thread (LAN is
// polled each frame), so the WebSocket callbacks and Read()/Write() never race.
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <map>
#include <deque>
#include <vector>
#include <cstring>
#include <cstdio>

namespace
{
	inline void putBE32(unsigned char *p, UnsignedInt v) { p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16); p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v; }
	inline void putBE16(unsigned char *p, UnsignedShort v) { p[0]=(unsigned char)(v>>8); p[1]=(unsigned char)v; }
	inline UnsignedInt getBE32(const unsigned char *p) { return ((UnsignedInt)p[0]<<24)|((UnsignedInt)p[1]<<16)|((UnsignedInt)p[2]<<8)|(UnsignedInt)p[3]; }
	inline UnsignedShort getBE16(const unsigned char *p) { return (UnsignedShort)(((UnsignedInt)p[0]<<8)|(UnsignedInt)p[1]); }

	struct WsDatagram
	{
		UnsignedInt   srcIP;
		UnsignedShort srcPort;
		std::vector<unsigned char> data;
	};

	class WsRelay
	{
	public:
		static WsRelay &get() { static WsRelay s; return s; }

		void connect()
		{
			if (m_sock || !emscripten_websocket_is_supported())
				return;
			char url[256];
			EM_ASM({
				var h = (typeof location !== 'undefined' && location.hostname) ? location.hostname : 'localhost';
				stringToUTF8('ws://' + h + ':8090', $0, 256);
			}, url);
			EmscriptenWebSocketCreateAttributes attr;
			emscripten_websocket_init_create_attributes(&attr);
			attr.url = url;
			attr.createOnMainThread = EM_TRUE;
			m_sock = emscripten_websocket_new(&attr);
			if (m_sock <= 0) { m_sock = 0; return; }
			emscripten_websocket_set_onopen_callback(m_sock, this, &WsRelay::onOpenCb);
			emscripten_websocket_set_onmessage_callback(m_sock, this, &WsRelay::onMessageCb);
			emscripten_websocket_set_onclose_callback(m_sock, this, &WsRelay::onCloseCb);
			emscripten_websocket_set_onerror_callback(m_sock, this, &WsRelay::onErrorCb);
		}

		UnsignedInt localIP() const { return m_assignedIP; }

		void registerPort(UnsignedShort port) { m_queues[port]; }
		void unregisterPort(UnsignedShort port) { m_queues.erase(port); }

		void send(UnsignedInt srcIP, UnsignedShort srcPort, UnsignedInt dstIP, UnsignedShort dstPort,
			const unsigned char *p, UnsignedInt len)
		{
			if (!m_open)
				return;
			std::vector<unsigned char> buf(12 + len);
			putBE32(&buf[0], srcIP); putBE16(&buf[4], srcPort);
			putBE32(&buf[6], dstIP); putBE16(&buf[10], dstPort);
			if (len) memcpy(&buf[12], p, len);
			emscripten_websocket_send_binary(m_sock, buf.data(), (uint32_t)buf.size());
		}

		bool recv(UnsignedShort port, WsDatagram &out)
		{
			std::map<UnsignedShort, std::deque<WsDatagram> >::iterator it = m_queues.find(port);
			if (it == m_queues.end() || it->second.empty())
				return false;
			out = it->second.front();
			it->second.pop_front();
			return true;
		}

	private:
		WsRelay() : m_sock(0), m_open(false), m_assignedIP(0) {}

		void handleText(const char *s)
		{
			unsigned a, b, c, d;
			if (s && strncmp(s, "IP ", 3) == 0 && sscanf(s + 3, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
				m_assignedIP = (a << 24) | (b << 16) | (c << 8) | d;
		}

		void handleBinary(const unsigned char *p, int len)
		{
			if (len < 12)
				return;
			WsDatagram dg;
			dg.srcIP   = getBE32(p);
			dg.srcPort = getBE16(p + 4);
			UnsignedShort dstPort = getBE16(p + 10);
			dg.data.assign(p + 12, p + len);
			m_queues[dstPort].push_back(dg);
		}

		static EM_BOOL onOpenCb(int, const EmscriptenWebSocketOpenEvent *, void *ud) { ((WsRelay *)ud)->m_open = true; return EM_TRUE; }
		static EM_BOOL onCloseCb(int, const EmscriptenWebSocketCloseEvent *, void *ud) { ((WsRelay *)ud)->m_open = false; return EM_TRUE; }
		static EM_BOOL onErrorCb(int, const EmscriptenWebSocketErrorEvent *, void *) { return EM_TRUE; }
		static EM_BOOL onMessageCb(int, const EmscriptenWebSocketMessageEvent *e, void *ud)
		{
			WsRelay *self = (WsRelay *)ud;
			if (e->isText) self->handleText((const char *)e->data);
			else           self->handleBinary(e->data, (int)e->numBytes);
			return EM_TRUE;
		}

		EMSCRIPTEN_WEBSOCKET_T m_sock;
		bool m_open;
		UnsignedInt m_assignedIP;
		std::map<UnsignedShort, std::deque<WsDatagram> > m_queues;
	};
} // namespace

// Exposed to IPEnumeration / SDL3Main so the engine can learn its relay-assigned LAN IP
// and start connecting early (well before the user reaches the LAN lobby).
extern "C" void ggc_ws_connect(void) { WsRelay::get().connect(); }
extern "C" unsigned int ggc_ws_local_ip(void) { WsRelay &r = WsRelay::get(); r.connect(); return r.localIP(); }

UDP::UDP() { fd = 0; myIP = 0; myPort = 0; m_lastError = 0; WsRelay::get().connect(); }
UDP::~UDP() { if (fd) WsRelay::get().unregisterPort(myPort); }

Int UDP::Bind(const char * /*Host*/, UnsignedShort port) { return Bind((UnsignedInt)0, port); }

Int UDP::Bind(UnsignedInt IP, UnsignedShort Port)
{
	WsRelay &r = WsRelay::get();
	r.connect();
	// The caller passes the local IP it chose (the 127.0.0.N selected in Options, surfaced
	// via IPEnumeration / m_defaultIP). That value is this client's identity on the relay:
	// the relay learns it from the src field of our datagrams and routes peers' unicasts to
	// it. Fall back to the relay-assigned IP only if none was supplied.
	myIP = IP ? IP : r.localIP();
	if (Port == 0)
	{
		static UnsignedShort s_ephemeral = 50000;
		Port = ++s_ephemeral;         // implicit bind -> pick a high ephemeral port
	}
	myPort = Port;
	r.registerPort(myPort);
	fd = 1;                          // non-zero = bound
	return OK;
}

Int UDP::getLocalAddr(UnsignedInt &ip, UnsignedShort &port)
{
	if (myIP == 0) myIP = WsRelay::get().localIP();
	ip = myIP;
	port = myPort;
	return OK;
}

Int UDP::SetBlocking(Int /*block*/) { return OK; }  // always non-blocking on web

Int UDP::Write(const unsigned char *msg, UnsignedInt len, UnsignedInt IP, UnsignedShort port)
{
	if ((IP == 0) || (port == 0)) return ADDRNOTAVAIL;
	UnsignedInt srcIP = myIP ? myIP : WsRelay::get().localIP();
	WsRelay::get().send(srcIP, myPort, IP, port, msg, len);
	return (Int)len;
}

Int UDP::Read(unsigned char *msg, UnsignedInt len, sockaddr_in *from)
{
	WsDatagram dg;
	if (!WsRelay::get().recv(myPort, dg))
		return 0;                     // no data pending (caller treats 0 as would-block)
	UnsignedInt n = (UnsignedInt)dg.data.size();
	if (n > len) n = len;
	if (n) memcpy(msg, &dg.data[0], n);
	if (from)
	{
		memset(from, 0, sizeof(*from));
		from->sin_family = AF_INET;
		from->sin_addr.s_addr = htonl(dg.srcIP);
		from->sin_port = htons(dg.srcPort);
	}
	return (Int)n;
}

void UDP::ClearStatus() { m_lastError = 0; }
UDP::sockStat UDP::GetStatus() { return OK; }
Int UDP::SetInputBuffer(UnsignedInt /*bytes*/) { return TRUE; }
Int UDP::SetOutputBuffer(UnsignedInt /*bytes*/) { return TRUE; }
int UDP::GetInputBuffer() { return 0; }
int UDP::GetOutputBuffer() { return 0; }
Int UDP::AllowBroadcasts(Bool /*status*/) { return TRUE; }

#ifdef DEBUG_LOGGING
AsciiString GetWSAErrorString( Int error ) { AsciiString s; s.format("err %d", error); return s; }
#endif

#else // !__EMSCRIPTEN__

#ifdef DEBUG_LOGGING

#define CASE(x) case (x): return #x;

AsciiString GetWSAErrorString( Int error )
{
	switch (error)
	{
		CASE(WSABASEERR)
		CASE(WSAEINTR)
		CASE(WSAEBADF)
		CASE(WSAEACCES)
		CASE(WSAEFAULT)
		CASE(WSAEINVAL)
		CASE(WSAEMFILE)
		CASE(WSAEWOULDBLOCK)
		CASE(WSAEINPROGRESS)
		CASE(WSAEALREADY)
		CASE(WSAENOTSOCK)
		CASE(WSAEDESTADDRREQ)
		CASE(WSAEMSGSIZE)
		CASE(WSAEPROTOTYPE)
		CASE(WSAENOPROTOOPT)
		CASE(WSAEPROTONOSUPPORT)
		CASE(WSAESOCKTNOSUPPORT)
		CASE(WSAEOPNOTSUPP)
		CASE(WSAEPFNOSUPPORT)
		CASE(WSAEAFNOSUPPORT)
		CASE(WSAEADDRINUSE)
		CASE(WSAEADDRNOTAVAIL)
		CASE(WSAENETDOWN)
		CASE(WSAENETUNREACH)
		CASE(WSAENETRESET)
		CASE(WSAECONNABORTED)
		CASE(WSAECONNRESET)
		CASE(WSAENOBUFS)
		CASE(WSAEISCONN)
		CASE(WSAENOTCONN)
		CASE(WSAESHUTDOWN)
		CASE(WSAETOOMANYREFS)
		CASE(WSAETIMEDOUT)
		CASE(WSAECONNREFUSED)
		CASE(WSAELOOP)
		CASE(WSAENAMETOOLONG)
		CASE(WSAEHOSTDOWN)
		CASE(WSAEHOSTUNREACH)
		CASE(WSAENOTEMPTY)
		CASE(WSAEPROCLIM)
		CASE(WSAEUSERS)
		CASE(WSAEDQUOT)
		CASE(WSAESTALE)
		CASE(WSAEREMOTE)
		CASE(WSAEDISCON)
		CASE(WSASYSNOTREADY)
		CASE(WSAVERNOTSUPPORTED)
		CASE(WSANOTINITIALISED)
		CASE(WSAHOST_NOT_FOUND)
		CASE(WSATRY_AGAIN)
		CASE(WSANO_RECOVERY)
		CASE(WSANO_DATA)
		default:
		{
			AsciiString ret;
			ret.format("Not a Winsock error (%d)", error);
			return ret;
		}
	}
	return AsciiString::TheEmptyString; // will not be hit, ever.
}

#undef CASE

#endif // defined(RTS_DEBUG)

//-------------------------------------------------------------------------

UDP::UDP()
{
  fd=0;
}

UDP::~UDP()
{
	if (fd)
		closesocket(fd);
}

Int UDP::Bind(const char *Host,UnsignedShort port)
{
  struct hostent *hostStruct;
  struct in_addr *hostNode;

  if (isdigit(Host[0]))
    return ( Bind( ntohl(inet_addr(Host)), port) );

  hostStruct = gethostbyname(Host);
  if (hostStruct == nullptr)
    return (0);
  hostNode = (struct in_addr *) hostStruct->h_addr;
  return ( Bind(ntohl(hostNode->s_addr),port) );
}

// You must call bind, implicit binding is for sissies
//   Well... you can get implicit binding if you pass 0 for either arg
Int UDP::Bind(UnsignedInt IP,UnsignedShort Port)
{
  int retval;
  int status;

  IP=htonl(IP);
  Port=htons(Port);

  addr.sin_family=AF_INET;
  addr.sin_port=Port;
  addr.sin_addr.s_addr=IP;
  fd=socket(AF_INET,SOCK_DGRAM,DEFAULT_PROTOCOL);
  #ifdef _WIN32
  if (fd==SOCKET_ERROR)
    fd=-1;
  #endif
  if (fd==-1)
    return(UNKNOWN);

  retval=bind(fd,(struct sockaddr *)&addr,sizeof(addr));

  #ifdef _WIN32
  if (retval==SOCKET_ERROR)
	{
    retval=-1;
		m_lastError = WSAGetLastError();
	}
  #endif
  if (retval==-1)
  {
    status=GetStatus();
    //CERR("Bind failure (" << status << ") IP " << IP << " PORT " << Port )
    return(status);
  }

  ggc_socklen_t namelen=sizeof(addr);
  getsockname(fd, (struct sockaddr *)&addr, &namelen);

  myIP=ntohl(addr.sin_addr.s_addr);
  myPort=ntohs(addr.sin_port);

  retval=SetBlocking(FALSE);
  if (retval==-1)
    fprintf(stderr,"Couldn't set nonblocking mode!\n");

  return(OK);
}

Int UDP::getLocalAddr(UnsignedInt &ip, UnsignedShort &port)
{
  ip=myIP;
  port=myPort;
  return(OK);
}


// private function
Int UDP::SetBlocking(Int block)
{
  #ifdef _WIN32
   unsigned long flag=1;
   if (block)
     flag=0;
   int retval;
   retval=ioctlsocket(fd,FIONBIO,&flag);
   if (retval==SOCKET_ERROR)
     return(UNKNOWN);
   else
     return(OK);
  #else  // UNIX
   int flags = fcntl(fd, F_GETFL, 0);
   if (block==FALSE)          // set nonblocking
     flags |= O_NONBLOCK;
   else                       // set blocking
     flags &= ~(O_NONBLOCK);

   if (fcntl(fd, F_SETFL, flags) < 0)
   {
     return(UNKNOWN);
   }
   return(OK);
  #endif
}


Int UDP::Write(const unsigned char *msg,UnsignedInt len,UnsignedInt IP,UnsignedShort port)
{
  Int retval;
  struct sockaddr_in to;

  // This happens frequently
  if ((IP==0)||(port==0)) return(ADDRNOTAVAIL);

#ifdef _UNIX
  errno=0;
#endif
  to.sin_port=htons(port);
  to.sin_addr.s_addr=htonl(IP);
  to.sin_family=AF_INET;

  ClearStatus();
  retval=sendto(fd,(const char *)msg,len,0,(struct sockaddr *)&to,sizeof(to));
  #ifdef _WIN32
  if (retval==SOCKET_ERROR)
	{
    retval=-1;
		m_lastError = WSAGetLastError();
#ifdef DEBUG_LOGGING
		static Int errCount = 0;
#endif
		DEBUG_ASSERTLOG(errCount++ > 100, ("UDP::Write() - WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
	}
  #endif

  return(retval);
}

Int UDP::Read(unsigned char *msg,UnsignedInt len,sockaddr_in *from)
{
  Int retval;
  ggc_socklen_t alen=sizeof(sockaddr_in);

  if (from!=nullptr)
  {
    retval=recvfrom(fd,(char *)msg,len,0,(struct sockaddr *)from,&alen);
    #ifdef _WIN32
    if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				// failing because of a blocking error isn't really such a bad thing.
				m_lastError = WSAGetLastError();
#ifdef DEBUG_LOGGING
				static Int errCount = 0;
#endif
				DEBUG_ASSERTLOG(errCount++ > 100, ("UDP::Read() - WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
				retval = -1;
			} else {
				retval = 0;
			}
		}
    #endif
  }
  else
  {
    retval=recvfrom(fd,(char *)msg,len,0,nullptr,nullptr);
    #ifdef _WIN32
    if (retval==SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				// failing because of a blocking error isn't really such a bad thing.
				m_lastError = WSAGetLastError();
#ifdef DEBUG_LOGGING
				static Int errCount = 0;
#endif
				DEBUG_ASSERTLOG(errCount++ > 100, ("UDP::Read() - WSA error is %s", GetWSAErrorString(WSAGetLastError()).str()));
				retval = -1;
			} else {
				retval = 0;
			}
		}
    #endif
  }
  return(retval);
}


void UDP::ClearStatus()
{
  #ifndef _WIN32
  errno=0;
  #endif

	m_lastError = 0;
}

UDP::sockStat UDP::GetStatus()
{
	Int status = m_lastError;
 #ifdef _WIN32
  //int status=WSAGetLastError();
  switch (status) {
    case NO_ERROR:
      return OK;
    case WSAEINTR:
      return INTR;
    case WSAEINPROGRESS:
      return INPROGRESS;
    case WSAECONNREFUSED:
      return CONNREFUSED;
    case WSAEINVAL:
      return INVAL;
    case WSAEISCONN:
      return ISCONN;
    case WSAENOTSOCK:
      return NOTSOCK;
    case WSAETIMEDOUT:
      return TIMEDOUT;
    case WSAEALREADY:
      return ALREADY;
    case WSAEWOULDBLOCK:
      return WOULDBLOCK;
    case WSAEBADF:
      return BADF;
    default:
      return (UDP::sockStat)status;
  }
 #else
  //int status=errno;
  switch (status) {
    case 0:
      return OK;
    case EINTR:
      return INTR;
    case EINPROGRESS:
      return INPROGRESS;
    case ECONNREFUSED:
      return CONNREFUSED;
    case EINVAL:
      return INVAL;
    case EISCONN:
      return ISCONN;
    case ENOTSOCK:
      return NOTSOCK;
    case ETIMEDOUT:
      return TIMEDOUT;
    case EALREADY:
      return ALREADY;
    case EAGAIN:
      return AGAIN;
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
      return WOULDBLOCK;
#endif
    case EBADF:
      return BADF;
    default:
      return UNKNOWN;
  }
 #endif
}



/*
//
// Wait for net activity on this socket
//
int UDP::Wait(Int sec,Int usec,fd_set &returnSet)
{
  fd_set inputSet;

  FD_ZERO(&inputSet);
  FD_SET(fd,&inputSet);

  return(Wait(sec,usec,inputSet,returnSet));
}
*/

/*
//
// Wait for net activity on a list of sockets
//
int UDP::Wait(Int sec,Int usec,fd_set &givenSet,fd_set &returnSet)
{
  Wtime        timeout,timenow,timethen;
  fd_set       backupSet;
  int          retval=0,done,givenMax;
  Bool         noTimeout=FALSE;
  timeval      tv;

  returnSet=givenSet;
  backupSet=returnSet;

  if ((sec==-1)&&(usec==-1))
    noTimeout=TRUE;

  timeout.SetSec(sec);
  timeout.SetUsec(usec);
  timethen+=timeout;

  givenMax=fd;
  for (UnsignedInt i=0; i<(sizeof(fd_set)*8); i++)   // i=maxFD+1
  {
    if (FD_ISSET(i,&givenSet))
      givenMax=i;
  }
  ///DBGMSG("WAIT  fd="<<fd<<"  givenMax="<<givenMax);

  done=0;
  while( ! done)
  {
    if (noTimeout)
      retval=select(givenMax+1,&returnSet,0,0,nullptr);
    else
    {
      timeout.GetTimevalMT(tv);
      retval=select(givenMax+1,&returnSet,0,0,&tv);
    }

    if (retval>=0)
      done=1;

    else if ((retval==-1)&&(errno==EINTR))  // in case of signal
    {
      if (noTimeout==FALSE)
      {
        timenow.Update();
        timeout=timethen-timenow;
      }
      if ((noTimeout==FALSE)&&(timenow.GetSec()==0)&&(timenow.GetUsec()==0))
        done=1;
      else
        returnSet=backupSet;
    }
    else  // maybe out of memory?
    {
      done=1;
    }
  }
  ///DBGMSG("Wait retval: "<<retval);
  return(retval);
}
*/




// Set the kernel buffer sizes for incoming, and outgoing packets
//
// Linux seems to have a buffer max of 32767 bytes for this,
//  (which is the default). If you try and set the size to
//  greater than the default it just sets it to 32767.

Int UDP::SetInputBuffer(UnsignedInt bytes)
{
   int retval,arg=bytes;

   retval=setsockopt(fd,SOL_SOCKET,SO_RCVBUF,
     (char *)&arg,sizeof(int));
   if (retval==0)
     return(TRUE);
   else
     return(FALSE);
}

// Same note goes for the output buffer

Int UDP::SetOutputBuffer(UnsignedInt bytes)
{
   int retval,arg=bytes;

   retval=setsockopt(fd,SOL_SOCKET,SO_SNDBUF,
     (char *)&arg,sizeof(int));
   if (retval==0)
     return(TRUE);
   else
     return(FALSE);
}

// Get the system buffer sizes

int UDP::GetInputBuffer()
{
   int retval,arg=0; ggc_socklen_t len=sizeof(int);

   retval=getsockopt(fd,SOL_SOCKET,SO_RCVBUF,
     (char *)&arg,&len);
   return(arg);
}


int UDP::GetOutputBuffer()
{
   int retval,arg=0; ggc_socklen_t len=sizeof(int);

   retval=getsockopt(fd,SOL_SOCKET,SO_SNDBUF,
     (char *)&arg,&len);
   return(arg);
}

Int UDP::AllowBroadcasts(Bool status)
{
	int retval;
	BOOL val = status;
	retval = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *)&val, sizeof(BOOL));
	if (retval == 0)
		return TRUE;
	else
		return FALSE;
}

#endif // __EMSCRIPTEN__
