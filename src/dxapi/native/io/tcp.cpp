#include "tickdb/common.h"
#include "platform/sockets_impl.h"

#include "tcp.h"


using namespace std;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;


#if 1==VERBOSE_SOCKETS
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

IOStreamTCP::IOStreamTCP(SOCKET s) :
    socket_(s), nBytesRead_(0), nBytesWritten_(0),
    disconnectionCallback_(NULL),
    closed_(false), inputClosed_(false), outputClosed_(false)

{ }


IOStreamTCP::~IOStreamTCP()
{
    closesocket();
    DBGLOG_VERBOSE("TCP stream deleted");
}


size_t IOStreamTCP::nBytesAvailable() const
{
    unsigned long n;

    if (inputClosed_) {
        return 0;
    }

    if (ioctlsocket(socket_, FIONREAD, &n) < 0)
    {
        int errorCode;
        auto out = last_socket_error(&errorCode, "IOStreamTCP::nBytesAvailable()");
        THROW_DBGLOG_IO(IOStreamException, errorCode, "%s", out.c_str());
    }

    return (unsigned)n;
}


size_t IOStreamTCP::write(const byte *data, size_t size)
{
    if (outputClosed_) {
        THROW_DBGLOG_IO(IOStreamException, ENOTCONN, "IOStreamTCP::write(): ERR: closed");
    }

    int result = send(socket_, (const char *)data, (int)size, 0);
    //        int result = size; // DBG
    if (SOCKET_ERROR == result) {
        std::string out;
        int errorCode;
        std::string err = last_socket_error(&errorCode, "IOStreamTCP::write()");
        DBGLOGERR(&out, "%s", err.c_str());
        invokeDisconnectionCallback(err, false);
        throw IOStreamException(errorCode, out);
    }
    nBytesWritten_ += (unsigned)result;
    return (unsigned)result;
}


size_t IOStreamTCP::read(byte *to, size_t minSize, size_t maxSize)
{
    if (inputClosed_) {
        THROW_DBGLOG_IO(IOStreamException, ENOTCONN, "IOStreamTCP::read(): ERR: closed");
    }

    assert(maxSize >= minSize);
    size_t bytesRead = 0;

    while (bytesRead < minSize) {
        // TODO: unsigned chars by default!
        int readSize;
#ifdef SOCKET_READ_SIZE_LIMIT
        readSize = (int)min(maxSize - bytesRead, (size_t)SOCKET_READ_SIZE_LIMIT);
#else
        readSize = (int)(maxSize - bytesRead);
#endif
        assert(readSize >= 0 && (unsigned)readSize <= maxSize);
        int result = recv(socket_, (char *)to, readSize, 0);
        if (result <= 0) {
            if (result < 0) {
                int error = socket_errno();
                if (WSAESHUTDOWN != error) {
                    std::string out;
                    int errorCode;
                    DBGLOGERR(&out, "%s", last_socket_error(&errorCode, "IOStreamTCP::read()").c_str());
                    invokeDisconnectionCallback(out, true);
                    throw IOStreamException(errorCode, out);
                }
            }
            else {
                const char *err = "read(): input stream disconnected";
                DBGLOG(err);
                //invokeDisconnectionCallback(out, true);
                throw IOStreamDisconnectException();
            }
        }

        bytesRead += (unsigned)result;
        to += (unsigned)result;
    }
    assert(bytesRead <= maxSize);
    nBytesRead_ += bytesRead;
    return bytesRead;
}


INLINE size_t IOStreamTCP::tryReadImpl(byte *to, size_t maxSize, timeval &timeout)
{
    if (inputClosed_) {
        THROW_DBGLOG_IO(IOStreamException, ENOTCONN, "IOStreamTCP::tryRead(): ERR: closed");
    }

    if (0 != maxSize) {
        if (NULL == to) {
            THROW_DBGLOG("IOStreamTCP::tryRead(): 'to' parameter is NULL with non-zero maxSize = %llu", (ulonglong)maxSize);
        }
    }

    fd_set r, e;
    
    int result, error;

    FD_ZERO(&r);
    FD_ZERO(&e);
    FD_SET(socket_, &r);
    FD_SET(socket_, &e);

    if (SOCKET_ERROR == (result = select((int)socket_ + 1, &r, NULL, &e, &timeout))) {
        if (result < 0) {
            error = socket_errno();

            if (WSAESHUTDOWN != error) {
                std::string out;
                int errorCode;
                DBGLOGERR(&out, "%s", last_socket_error(&errorCode, "IOStreamTCP::tryRead() select()").c_str());
                invokeDisconnectionCallback(out, true);
                throw IOStreamException(errorCode, out);
            }
        }
    }

    if (0 == result)
        return 0;

    if (FD_ISSET(socket_, &r)) {
        if (0 == maxSize) {
            size_t n = nBytesAvailable();
            if (0 == n) {
                const char *err = "tryRead(): input stream disconnected";
                DBGLOG(err);
                //invokeDisconnectionCallback(out, true);
                throw IOStreamDisconnectException();
            }
            else {
                return 0;
            }
        }
        else {
            return read(to, 1, maxSize);
        }
    }

    // If we are here, error flag must be set
    assert(FD_ISSET(socket_, &e));

    socklen_t len = sizeof(error);
    if (0 != getsockopt(socket_, SOL_SOCKET, SO_ERROR, (char *)&error, &len)) {
        THROW_DBGLOG("%s", last_socket_error(nullptr, "IOStreamTCP::tryRead() getsockopt(SO_ERROR)").c_str());
    }
    else {
        THROW_DBGLOG("%s", format_socket_error(error, "IOStreamTCP::tryRead() select()").c_str());
    }
}


size_t IOStreamTCP::tryRead(byte *to, size_t maxSize)
{
    timeval timeout = { 0, 0 };
    return tryReadImpl(to, maxSize, timeout);
}


size_t IOStreamTCP::tryRead(byte *to, size_t maxSize, uint64_t timeout_us)
{
    timeval timeout = { 
        (long)(timeout_us / 1000000), 
#if defined(__APPLE__)
        static_cast<__darwin_suseconds_t>(timeout_us % 1000000)
#else
        (long)(timeout_us % 1000000) 
#endif
    };
    return tryReadImpl(to, maxSize, timeout);
}


uint64_t IOStreamTCP::nBytesRead() const
{
    return nBytesRead_;
};


uint64_t IOStreamTCP::nBytesWritten() const
{
    return nBytesWritten_;
};


void IOStreamTCP::closesocket()
{
    closed_ = inputClosed_ = outputClosed_ = true;
    SOCKET sock = this->socket_;
    if (INVALID_SOCKET != sock) {
        this->socket_ = INVALID_SOCKET;
        ::shutdown(sock , SD_BOTH);
        ::closesocket(sock);
        DBGLOG_VERBOSE("closesocket(): TCP socket 0x%p deleted", (void*)(size_t)sock);
    }
}


void IOStreamTCP::closeInput()
{
    inputClosed_ = true;
    if (INVALID_SOCKET != this->socket_) {
        ::shutdown(this->socket_, SD_RECEIVE);
    }
}


void IOStreamTCP::closeOutput()
{
    outputClosed_ = true;
    if (INVALID_SOCKET != this->socket_) {
        ::shutdown(this->socket_, SD_SEND);
    }
}


void IOStreamTCP::close()
{
    /*if (INVALID_SOCKET != this->socket_) {
        shutdown(this->socket_, SD_BOTH);
    }*/
    closesocket();
}


bool IOStreamTCP::isInputClosed() const
{
    return inputClosed_;
}


bool IOStreamTCP::isOutputClosed() const
{
    return outputClosed_;
}


bool IOStreamTCP::isClosed() const
{
    return closed_;
}


void IOStreamTCP::flush()
{
    THROW_DBGLOG("IOStreamTCP::flush(): Not implemented");
}

void IOStreamTCP::abort()
{
    THROW_DBGLOG("IOStreamTCP::abort(): Not implemented");
}


void IOStreamTCP::setDisconnectionCallback(DisconnectionCallback callback, void *arg)
{
    disconnectionCallback_ = callback;
    disconnectionCallbackArg_ = arg;
}


bool IOStreamTCP::invokeDisconnectionCallback(const char *text, bool isRead)
{
    auto cb = disconnectionCallback_;
    return NULL != cb ? cb(this, disconnectionCallbackArg_, text) : true;
}


bool IOStreamTCP::invokeDisconnectionCallback(const std::string &text, bool isRead)
{
    return invokeDisconnectionCallback(text.c_str(), isRead);
}