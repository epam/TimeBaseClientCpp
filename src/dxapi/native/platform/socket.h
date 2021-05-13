#ifndef _NOOP_NET_H_
#define _NOOP_NET_H_

/*****************************************************************************************
* Minimal wrapper for TCP/IP sockets V1.1 (C) by Boris Chuprin 2013-2015
* free for use for any purpose, but I refuse to be held responsible for any outcome
*****************************************************************************************/

//#include "platform.h"

#include <stdlib.h>
#include <stdint.h>

#if defined(WIN32)
#include <WinSock2.h>

typedef int socklen_t;
#define socket_last_error()                                WSAGetLastError()

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

typedef int SOCKET;
#define INVALID_SOCKET                                   -1

#define socket_last_error()                              errno
#endif

/*
 * Create new network socket. Returns INVALID_SOCKET on failure
 */
SOCKET socket_init_tcp();

/*
 * Create new network socket. Returns INVALID_SOCKET on failure
 */
SOCKET socket_init_udp();

/*
 * Close the socket. Returns 0 on success, platform-dependent error code otherwise
 */
int socket_close(SOCKET socket);

/*
 * Use non-zero parameter to enable, 0 to disable nagle algorithm for a socket. Returns 0 on success.
 */
int socket_nagle_algorithm_enable(SOCKET socket, int enable);

/*
* Use non-zero parameter to enable, 0 to disable non-blocking mode for a socket. Returns 0 on success.
*/
int socket_non_blocking_enable(SOCKET socket, int enable);

/*
 * Conect socket to the specified address. Returns 0 on success.
 */
int socket_connect(SOCKET socket, const char *host_name, unsigned port);

// get system-dependent error code for _some_ socket operations
int socket_get_error(SOCKET socket);

/*
return non-zero value if the system-dependent error code (returned with socket_last_error()) represents temporary error
 that can go away on the next attempt: "operation would block", "resource is blocked", "no buffer space left"
*/
int socket_error_is_temporary(int error_code);

#if 0
/*
 * Receive specified number of bytes. Blocking call. Returns 0 on success, non-zero on error or disconnect
 */
int socket_recv(SOCKET socket, char *data, size_t size);

/*
 * Send specified number of bytes. Blocking call. Returns 0 on success, non-zero on error or disconnect
 */
int socket_send(SOCKET socket, const char *data, size_t size);
#endif


#endif /* _NOOP_NET_H_ */
