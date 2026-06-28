#pragma once

// TheSuperHackers @build bobtista 29/04/2026 winsock.h compatibility shim for
// non-Windows builds. Maps the Win32 BSD-derived socket API to native POSIX
// equivalents so WWDownload (FTP client) at least compiles.

#include "windows.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef int SOCKET;
typedef struct hostent HOSTENT;
typedef HOSTENT *PHOSTENT;
typedef HOSTENT *LPHOSTENT;

#ifndef MAKEWORD
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))
#endif

typedef struct WSAData
{
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char *lpVendorInfo;
} WSADATA, *LPWSADATA;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

#ifndef closesocket
#define closesocket(s) (::close(s))
#endif

#ifndef ioctlsocket
#define ioctlsocket(s, cmd, argp) (::ioctl((s), (cmd), (argp)))
#endif

#ifndef WSAGetLastError
#define WSAGetLastError() (errno)
#endif

#ifndef WSAStartup
// TheSuperHackers @bugfix githubawn 27/06/2026 Populate wVersion. Callers (Transport::init,
// IPEnumeration) verify wsadata.wVersion == 2.2 after WSAStartup and bail out otherwise;
// the old no-op left wsadata uninitialized, so that check read stack garbage and LAN
// networking failed ("NETWORK ERROR") on every non-Windows platform. Echo the requested
// version back like the real Winsock does.
static inline int ggc_wsa_startup(WORD wVer, LPWSADATA lpData)
{
    if (lpData)
    {
        lpData->wVersion = wVer;
        lpData->wHighVersion = wVer;
    }
    return 0;
}
#define WSAStartup(wVer, lpData) ggc_wsa_startup((wVer), (lpData))
#endif

#ifndef WSACleanup
#define WSACleanup() (0)
#endif

#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK EAGAIN
#endif

#ifndef WSAECONNRESET
#define WSAECONNRESET ECONNRESET
#endif

#ifndef WSAENOTCONN
#define WSAENOTCONN ENOTCONN
#endif

#ifndef WSAEINPROGRESS
#define WSAEINPROGRESS EINPROGRESS
#endif

#ifndef WSAEALREADY
#define WSAEALREADY EALREADY
#endif

#ifndef WSAEINVAL
#define WSAEINVAL EINVAL
#endif

#ifndef WSAEISCONN
#define WSAEISCONN EISCONN
#endif

#ifndef WSAETIMEDOUT
#define WSAETIMEDOUT ETIMEDOUT
#endif

#ifndef WSAEINTR
#define WSAEINTR EINTR
#endif
