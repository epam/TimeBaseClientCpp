#include <string.h>
#include "platform/socket.h"

// Small socket library by Boris Chuprin

#if defined(WIN32)
#define PLATFORM_WINDOWS 1

#define INIT_NETWORK                                                \
    WORD version_requested;                                         \
    WSADATA wsadata;                                                \
    /* Initialize Winsock 2.0    Yes, we can do it many times */    \
    version_requested = MAKEWORD(2, 0);                             \
    if (0 != WSAStartup(version_requested, &wsadata))               \
        return INVALID_SOCKET;

#else
#undef PLATFORM_WINDOWS
#include <unistd.h>
#include <netdb.h>
// Use errno for Windows too?
#include <errno.h>
#define INIT_NETWORK
#endif

SOCKET socket_init_tcp()
{
    INIT_NETWORK
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

SOCKET socket_init_udp()
{
    INIT_NETWORK
    return socket(AF_INET, SOCK_STREAM, IPPROTO_UDP);
}

int socket_close(SOCKET socket)
{
#if defined(WIN32)
    return closesocket(socket);
#else
    return close(socket);
#endif
}

int socket_nagle_algorithm_enable(SOCKET socket, int enable)
{
    /* enabled TCP_NODELAY flag _disables_ nagle algorithm */
    enable = !enable;
    return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(enable));
}


int socket_non_blocking_enable(SOCKET socket, int enable)
{
    int result;
#if defined(PLATFORM_WINDOWS)
    unsigned long mode = 0 != enable;
    result = ioctlsocket(socket, FIONBIO, &mode);
#else /* defined(PLATFORM_WINDOWS) */
    result = fcntl(socket, F_GETFL);
    if (-1 != result)
        result = fcntl(socket, F_SETFL, (result | O_NONBLOCK) - (enable ? 0 : O_NONBLOCK));
#endif /* !defined(PLATFORM_WINDOWS) */
    return result;
}


int socket_connect(SOCKET socket, const char * host_name, unsigned port)
{
    struct sockaddr_in addr;
    struct in_addr host_address;
    struct hostent* host;

    /* Check, if socket is valid */
    if (INVALID_SOCKET == socket)
        return -1;

    /* Check, if port number is valid */
    if ((port - 1) > (65535 - 1))
        return -1;

    host = gethostbyname((const char *)host_name);
    if (NULL == host)
        return -1;

    memcpy(&host_address, host->h_addr_list[0], host->h_length);

    addr.sin_family = AF_INET;
    addr.sin_addr = host_address;
    addr.sin_port = htons((uint16_t)port);

    if (-1 == addr.sin_addr.s_addr)
        return -1;

    return connect(socket, (struct sockaddr*)&addr, sizeof(addr));
}


int socket_get_error(SOCKET socket)
{
    int error = 0;
    socklen_t length = sizeof(error);
    getsockopt(socket, SOL_SOCKET, SO_ERROR, (char *)&error, &length);
    return error;
}


int socket_error_is_temporary(int error_code)
{
#ifdef PLATFORM_WINDOWS
    return WSAEWOULDBLOCK == error_code || WSAEINPROGRESS == error_code || WSAEALREADY == error_code || WSAENOBUFS == error_code;
#else
    return EAGAIN == error_code || EWOULDBLOCK == error_code;
#endif
}


#if 0
int socket_recv(SOCKET socket, char *data, size_t size_left)
{
    int size_received;

    while (0 != size_left) {
        data += (size_received = recv(socket, data, size_left, 0));
        if (size_received <= 0)
            return -1;
        size_left -= size_received;
    }

    return 0;
}

/* Send specified amount of bytes. Blocking call. Returns 0 on success, non-zero on error or disconnect */
int socket_send(SOCKET socket, const char *data, size_t size_left)
{
    int size_sent;

    while (0 != size_left) {
        data += (size_sent = send(socket, data, size_left, 0));
        if (size_sent <= 0)
            return -1;
        size_left -= size_sent;
    }

    return 0;
}
#endif