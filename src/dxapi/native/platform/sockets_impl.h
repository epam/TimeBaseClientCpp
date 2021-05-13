/*
 Implementation header for platform sockets
*/


#if defined(_WIN32)

#define CONST const
#include <WinSock2.h>
#include <Ws2tcpip.h>

// Undefine windows.h idiotic macros
#undef CONST
#undef NO_ERROR
#undef ERROR
#undef NO_DATA
#undef min
#undef max

static int socket_errno() { return WSAGetLastError(); }

#else
#include <sys/socket.h> 
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>

typedef int SOCKET;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (~(SOCKET)0)
#endif

#define SOCKET_ERROR   (-1)
#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR

#define WSAESHUTDOWN ESHUTDOWN
#define ioctlsocket ioctl


static int socket_errno() { return errno; }

static void closesocket(SOCKET s) { close(s); }

#endif

