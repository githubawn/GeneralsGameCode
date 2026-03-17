#pragma once
// Stub winsock.h for web build
#include "WebWin32.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct hostent HOSTENT;

// Compatibility for sin_addr.S_un.S_addr
#define S_un
#define S_addr s_addr

// getsockname shim for socklen_t
#ifdef getsockname
#undef getsockname
#endif
#define getsockname(s, n, l) ::getsockname(s, n, (socklen_t*)(l))
