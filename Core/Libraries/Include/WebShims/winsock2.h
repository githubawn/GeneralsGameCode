#pragma once
#include "WebWin32.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

typedef struct hostent {
    char* h_name;
    char** h_aliases;
    short h_addrtype;
    short h_length;
    char** h_addr_list;
} HOSTENT, *PHOSTENT, *LPHOSTENT;

#define h_addr h_addr_list[0]

inline struct hostent* gethostbyname(const char* name) { return NULL; }
inline uint32_t inet_addr(const char* cp) { return 0; }
inline char* inet_ntoa(struct in_addr in) { return NULL; }

#define ADDR_ANY 0
#define INADDR_ANY 0
#define INADDR_NONE 0xffffffff

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6
