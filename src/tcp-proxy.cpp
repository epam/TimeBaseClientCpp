#define _CRT_SECURE_NO_WARNINGS

#include "tickdb/common.h"
#include <memory>
#include <string>
#include <dxapi/dxapi.h>
#include "proxy/common.h"

extern "C" {
#include "platform/socket.h"
}

#define MAX_READ_SIZE 0x400
#define MAX_READ_ALLOC_SIZE 0x400
#define MAX_WRITE_SIZE 0x2000


using namespace DxApi;
using namespace DxApiImpl;
using namespace std;

FILE *infoOutputFile;

#ifndef forn
#define forn(i, n) for (intptr_t i = 0, i##_count = (intptr_t)(n); i < i##_count; ++i)
#endif

#ifndef _rdtsc
#define _rdtsc()  __rdtsc()
#endif
#ifndef _rdtsc
#define _CDECL __cdecl
#endif


static double timetodouble(uint64_t t)
{
    return 1E-9 * t;
}

void err(const char *s, ...)
{
    va_list p;
    fprintf(stderr, "ERR: ");
    va_start(p, s);
    vfprintf(stderr, s, p);
    va_end(p);
    fprintf(stderr, "\n");
    //fflush(stderr);
}

int __cdecl ivfprintf(FILE * file, const char * fmt, va_list args)
{
    if (NULL == file)
        return 0;
    return vfprintf(file, fmt, args);
}


int iprintf(const char *fmt, ...)
{
    va_list p;
    int ret;
    va_start(p, fmt);
    ret = ivfprintf(infoOutputFile, fmt, p);
    va_end(p);
    return ret;
}


enum class IterationResult : unsigned {
    FAILURE,
    SUCCESS,
    FINISHED,
    IDLE
};

typedef IterationResult IR;

#define ITERATION_FINISHED(x) (0 == ((unsigned)(x) & 1))

typedef union _DX_SOCKET_ADDRESS
{
    uint16 Family;
    struct sockaddr_in IPv4;
    //struct sockaddr_in6 IPv6;
} DX_SOCKET_ADDRESS;

enum CH  { CLIENT, HOST };
enum RWE { RD, WR, ER };

const char *sideNames[2] = { "client", "host" };

class Buffer {
public:
    inline uintptr_t nBytesRemaining()      const   { return data_.size() - writeOffset_; }
    inline uintptr_t nBytesUnread()         const   { assert(writeOffset_ >= readOffset_); return writeOffset_ - readOffset_; }

    // Maximum number of bytes that can be added 
    inline uintptr_t writeSize()            const   { return std::min(nBytesRemaining(), (uintptr_t)MAX_READ_SIZE); }
    // Maximum number of bytes that can be read out in one attempt
    inline uintptr_t readSize()             const   { return std::min(nBytesUnread(), (uintptr_t)MAX_WRITE_SIZE); }
    // Return max size reached
    inline uintptr_t maxSize()              const   { return maxSize_; }

    inline uint64_t nBytesWritten()        const    { return nBytesWritten_; }
    inline uint64_t nBytesRead()           const    { return nBytesRead_; }
    inline uint64_t nBytesUnreadTotal()    const    { return nBytesWritten() - nBytesRead(); }

    char * writePtr()                               { return (char *)&data_[writeOffset_]; }
    char * readPtr()                                { return (char *)&data_[readOffset_]; }

    // to buffer
    void commitWrite(uintptr_t nBytesAdded)
    {
        nBytesWritten_ += nBytesAdded;
        if ((writeOffset_ += nBytesAdded) > maxSize_) {
            maxSize_ = writeOffset_;
        }

        if (data_.size() < writeOffset_ + MAX_READ_SIZE) {
            data_.resize(writeOffset_ + MAX_READ_ALLOC_SIZE);
            // Unfortunately you can't resize vector without fill, that's why I wont resize(capacity()), could be slow
        }
    }

    // from buffer
    void commitRead(uintptr_t nBytesRead)
    {
        nBytesRead_ += nBytesRead;
        readOffset_ += nBytesRead;
        assert(writeOffset_ >= readOffset_);
    }

    // Reset read/write pointers, discarding any data, but without resizing content
    void reset()       { readOffset_ = writeOffset_ = 0; }

    // Completely reset this buffer object
    void clear()       { reset(); data_.resize(writeOffset_ + MAX_READ_ALLOC_SIZE); maxSize_ = 0; }

    // Discard all data that was already written, shift unwritten data to the start of the array
    void compact()
    {
        uintptr_t nBytesUnread = this->nBytesUnread();
        if (0 != nBytesUnread) {
            if (readOffset_ >= nBytesUnread) {
                memcpy(&data_[0], readPtr(), nBytesUnread);
            }
            else {
                memmove(&data_[0], readPtr(), nBytesUnread);
            }
        }
        readOffset_ = 0;
        writeOffset_ = nBytesUnread;
    }

    Buffer() : readOffset_(0), writeOffset_(0), maxSize_(0), nBytesWritten_(0), nBytesRead_(0)
    {
        data_.resize(MAX_READ_ALLOC_SIZE);
    }

protected:
    uintptr_t writeOffset_;    // data will be stored into buffer at this pointer
    uintptr_t readOffset_;     // data will be read out from this pointer
    uintptr_t maxSize_;
    uint64_t  nBytesWritten_;   // to buffer
    uint64_t  nBytesRead_;      // from buffer
    vector<byte> data_;
};


class Session {
public:
    string name;            // Name of this particular connection (determine at runtime? use counter?)
    uintptr_t nBytesToPreload;

    uint64_t tData1stReceivedFrom[2], tDataLastReceivedFrom[2];
    uint64_t tData1stSentFrom[2], tDataLastSentFrom[2];
    uint64_t  tPreloadFinished, tStart, tEnd;

    bool isPreloadFinished;

    byte chrwe[2][4];       // client/host connection can read/write/have error  [0][0] means client connection can read some data

    Buffer buf[2];          // client/host buffer

    uint16 remotePort;      // Will connect to this port
    string remoteHostName;  // Will connect to this hostname

    SOCKET sock[2];    // Socket for connected client
    DX_SOCKET_ADDRESS clientAddress;    // Address of the connected client
    uint16 clientPort;                  // Port of the connected client
    uint32 clientIP4;                   // IP4 address of the connected client

    //uint32 localIP4;                    // IP4 address outgoing socket is bound to
    DX_SOCKET_ADDRESS localAddress;    // Address of the connected client
    
    bool connect();                     // Process incoming connection
    IterationResult process();                        // Main work loop for threaded mode
    void sock_close(intptr_t iSide);
    inline bool sock_opened(intptr_t iSide) const { return INVALID_SOCKET != sock[iSide]; }

    void kill_connection(intptr_t iSide, int result);
    //int run();                          // Main work loop for threaded mode

    Session();
    //Session(const Session &other);
    ~Session();
protected:
    void print_buffer_stats(intptr iSide);
    //uint64 nBytesRecvFrom[2];
    //uint64 nBytesSentFrom[2];
};


template <class T> class unique_ptr_array : public vector<unique_ptr<T>> {
public:
    // Add new element
    inline T& add(T * element)
    {
        push_back(unique_ptr<T>(element));
        updated = true;
        return *element;
    }
    
    // Add new copy of the first element
    inline T& add()
    {
        updated = true;
        return add(new T(*front()));
    }
    
    // delete last element
    inline void delete_last()
    {
        pop_back();
        updated = true;
    }
    
    // delete element at index
    void delete_at(intptr index)
    {
        uintptr size_ = size();
        assert((uintptr)index < size_);
        if ((uintptr)index + 1 != size_) {
            std::swap(operator[](index), back());
        }
        pop_back();
        updated = true;
    }

protected:
    bool updated; // Yes, we only update the flag for few operations
    unique_ptr_array() : vector<unique_ptr<T>>(), updated(false) {}
};


class SessionArray : public unique_ptr_array<Session> {
public:
    string localHostName;   // TODO:
    string remoteHostName;
    uint32 localIP4;        // TODO:
    uint16 localPort;       // Will listen on this port
    uint16 remotePort;      // Will connect to this port
    uint64 nBytesToPreload;

    bool accept();
    IterationResult select_iteration();

    bool open();
    Session& base() { assert(0 != size());  return *front(); }

    SessionArray::~SessionArray();
    SessionArray() : unique_ptr_array<Session>(), listener(INVALID_SOCKET), localIP4(INADDR_LOOPBACK),
        nBytesToPreload(0), nSessionsAccepted(0)
    {
        add(new Session);
    }

protected:
    SOCKET listener;
    DX_SOCKET_ADDRESS localAddress;
    uint64 nSessionsAccepted;
    
};

void sockaddr_set_ip4(DX_SOCKET_ADDRESS * address, uint32 host, uint16 port)
{
    DX_CLEAR(*address);
    address->IPv4.sin_family = AF_INET;
    address->IPv4.sin_addr.s_addr = htonl(host);
    address->IPv4.sin_port = htons(port);
}


void sockaddr_get_ip4(const DX_SOCKET_ADDRESS * address, uint32 * host, uint16 * port)
{
    *host = ntohl(address->IPv4.sin_addr.s_addr);
    *port = ntohs(address->IPv4.sin_port);
}

// threaded mode is unused for now
//int serverThread(Session * This)
//{
//    int ret = This->run();
//    delete This;
//}




bool SessionArray::open()
{
    listener = socket_init_tcp();
    base().remoteHostName = remoteHostName;
    base().remotePort = remotePort;

    sockaddr_set_ip4(&localAddress, localIP4, localPort);
    if (0 != bind(listener, (struct sockaddr *)&localAddress, sizeof(DX_SOCKET_ADDRESS))) {
        err("bind(localhost): Socket error :%d", socket_last_error());
        return false;
    }

    if (0 != listen(listener, -1)) {
        err("listen(): Socket error :%d", socket_last_error());
        return false;
    }

    if (0 != socket_non_blocking_enable(listener, true)) {
        err("open(): unable to set non-blocking mode for server socket, error: ", socket_last_error());
        return false;
    }

    return true;
}


bool SessionArray::accept()
{
    socklen_t addrLen = sizeof(DX_SOCKET_ADDRESS);
    Session &base = *front();
    base.sock[CLIENT] = ::accept(listener, (struct sockaddr *)&base.clientAddress, &addrLen);
    if (INVALID_SOCKET != base.sock[CLIENT]) {
        sockaddr_get_ip4(&base.clientAddress, &base.clientIP4, &base.clientPort);
        ++nSessionsAccepted;
        // Setup new session
        stringstream s;
        s << "Session" << nSessionsAccepted;
        base.name = s.str();
        base.nBytesToPreload = (uintptr)nBytesToPreload;
        base.tStart = time_ns();
        iprintf("Connecting %s.. ", s.str().c_str());
        if (add().connect()) {
            iprintf("success.\n");
            return true;
        }
        iprintf("failure!\n");
        delete_last();
        // TODO: if multithreaded mode spawn new thread every time
        // serverThread(new Session(base));
    }
    else {
        err("accept(): Error while trying to accept connection: ", socket_last_error());
    }
    return false;
}


IterationResult SessionArray::select_iteration()
{
    fd_set set_r, set_w, set_e;
    struct timeval tv = { 0, 10 };
    int result = 0;
    IterationResult retVal = IR::IDLE;

    // KLUDGE:
    base().sock[HOST] = base().sock[CLIENT] = INVALID_SOCKET;

    // TODO: Do not add Sessions if there are too many
    // TODO: round-robin select if there are many sessions

    // Winsock fd_set is slow, maybe should iterate it myself later
    FD_ZERO(&set_r);
    

    SOCKET socketmax = listener;
    for (intptr iSession = 1, count = std::min(30, (int)size()); iSession < count; ++iSession) {
        Session &s = *operator[](iSession);
        SOCKET sock;
        forn (iSide, 2) {
            if (INVALID_SOCKET != (sock = s.sock[iSide])) {
                if (socketmax < sock) { 
                    socketmax = sock;
                }
                FD_SET(sock, &set_r);
            }
        }
        memset(s.chrwe, 0, sizeof(s.chrwe));
    }
    
    set_e = set_r;
    set_w = set_r;
    FD_SET(listener, &set_e);
    FD_SET(listener, &set_r);

    result = select((int)socketmax + 1, (fd_set *)&set_r, (fd_set *)&set_w, (fd_set *)&set_e, &tv);
    switch (result) {
    case 0:
        return IR::IDLE; // Timeout
    case -1:
        err("select(): returned with error");
        return IR::FAILURE;
    }
    if (FD_ISSET(listener, &set_e)) {
        err("select(): error on listening socket: %d", socket_last_error());
        return IR::FAILURE;
    }

    if (FD_ISSET(listener, &set_r)) {
        retVal = IR::SUCCESS;
        accept();
    }

    for (intptr iSession = 1, count = std::min(30, (int)size()); iSession < count; ++iSession) {
        Session &s = *operator[](iSession);
        SOCKET sock;
        forn (iSide, 2) {
            if (INVALID_SOCKET != (sock = s.sock[iSide])) {
                s.chrwe[0][3] = (
                    (s.chrwe[iSide][2] = FD_ISSET(sock, &set_e)) |
                    (s.chrwe[iSide][1] = FD_ISSET(sock, &set_w)) |
                    (s.chrwe[iSide][0] = FD_ISSET(sock, &set_r))
                );
            }
        }
    }

    for (intptr i = 1, count = std::min(30, (int)size()); i < count; ++i) {
        Session &s = *operator[](i);
        if (s.chrwe[0][3]) {
            IterationResult ir = s.process();
            if (ITERATION_FINISHED(ir)) {
                //iprintf("deleting %s\n", s.name.c_str());
                delete_at(i);
                --count;
                if (1 == size()) {
                    iprintf("All sessions finished\n\n");
                    nSessionsAccepted = 0;
                }
                retVal = IR::SUCCESS;
            }
            else if (IR::IDLE != ir) {
                retVal = IR::SUCCESS;
            }
        }
    }

    return retVal;
}


SessionArray::~SessionArray()
{
    if (INVALID_SOCKET != listener) socket_close(listener);
}


Session::~Session()
{
    sock_close(HOST);
    sock_close(CLIENT);
}

Session::Session() : remoteHostName("localhost"), remotePort(0), clientIP4(0)
{
    sock[CLIENT] = sock[HOST] = 0;
    //DX_CLEAR(nBytesRecvFrom);
    //DX_CLEAR(nBytesSentFrom);
}


//Session::Session(const Session &other)
//{
//}

// Caller is supposed to delete this Session if connect() returned false, freeing sockets
bool Session::connect()
{
    // At this point we already have initialized clientSocket field

    // Initialize clientsocket
    sock[HOST] = socket_init_tcp();
    sockaddr_set_ip4(&localAddress, INADDR_ANY, 0);
    if (0 != bind(sock[HOST], (struct sockaddr *)&localAddress, sizeof(DX_SOCKET_ADDRESS))) {
        err("bind(localhost): Socket error :%d", socket_last_error());
        return false;
    }

    struct addrinfo * addr = NULL, * current_addr, hints;
    char portName[32];
    DX_CLEAR(hints);

    // TODO: refactor
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    sprintf(portName, "%d", (int)remotePort);
    // Resolve the server address and port
    int result = getaddrinfo(remoteHostName.c_str(), portName, &hints, &addr);
    if (0 != result) {
        err("connect(): unable to resolve address '%s' error: %d", remoteHostName.c_str(), result);
        return false;
    }

    result = -1;
    for (current_addr = addr; current_addr != NULL; current_addr = current_addr->ai_next) {
        result = ::connect(sock[HOST], current_addr->ai_addr, (int)current_addr->ai_addrlen);
        if (0 != result)
            continue;
        break;
    }

    if (0 != result) {
        err("connect(): unable to connect to '%s' error: %d", remoteHostName.c_str(), socket_last_error());
        return false;
    }

    if (0 != socket_non_blocking_enable(sock[CLIENT], true)) {
        err("connect(): unable to set non-blocking mode for incoming socket, error: ", socket_last_error());
        return false;
    }

    if (0 != socket_non_blocking_enable(sock[HOST], true)) {
        err("connect(): unable to set non-blocking mode for outgoing socket, error: ", socket_last_error());
        return false;
    }

    isPreloadFinished = false;

    DX_CLEAR(tData1stReceivedFrom);
    DX_CLEAR(tDataLastReceivedFrom);
    DX_CLEAR(tData1stSentFrom);
    DX_CLEAR(tDataLastSentFrom);
    tPreloadFinished = tStart;
    tEnd = UINT64_MAX;

    return true;
}

void Session::sock_close(intptr_t iSide)
{
    if (INVALID_SOCKET != sock[iSide]) {
        socket_close(sock[iSide]);
        sock[iSide] = INVALID_SOCKET;
    }
}


void Session::kill_connection(intptr_t iSide, int result)
{
    int er;
    switch (result) {
    case 0:
        iprintf("%s: %s disconnected\n", name.c_str(), sideNames[iSide]);
    case -9000:
        break;
    default:
        er = socket_last_error();
        if (0 != er) {
            err("%s: error in %s-side socket: %d", name.c_str(), sideNames[iSide], socket_last_error());
        }
        else {
            iprintf("%s: disconnecting from %s\n", name.c_str(), sideNames[iSide]);
        }
    }
    socket_close(sock[iSide]);
    sock[iSide] = INVALID_SOCKET;
    memset(chrwe[iSide], 0, sizeof(chrwe[0]));
}



void Session::print_buffer_stats(intptr iSide)
{
#define TSPAN(side, phase) (tDataLast##phase##From[side] - tData1st##phase##From[side])
#define PERF(side, phase, bufferphase) 1E-6 * TSPAN(side, phase), (TSPAN(side, phase) ? (1E9/(1 << 20)) * buf[side].nBytes##bufferphase() / TSPAN(side, phase) : 0.0)

    uint64_t sz = buf[iSide].nBytesRead();
    iprintf("%llu bytes to %s", (ulonglong)sz, sideNames[iSide ^ 1]);
    if (sz > MAX_READ_SIZE * 4) {
        iprintf(" in %lfms(%.3lfMB/s)->%lfms(%.3lfMB/s)", PERF(iSide, Received, Written), PERF(iSide, Sent, Read));
    }
}

IterationResult Session::process()
{
    IterationResult retVal = IR::IDLE;

    //bool error = false;
    forn (iSide, 2) {
        if (chrwe[iSide][ER] && INVALID_SOCKET != sock[iSide]) {
            int er = socket_last_error();
            bool temperror = 0 != socket_error_is_temporary(er);
            err("%s: %s error in %s-side socket: %d", name.c_str(), temperror ? "Recoverable" : "Unrecoverable", sideNames[iSide], er);
            if (!temperror) {
                kill_connection(iSide, -9000);
            }
            retVal = IR::FAILURE;
        }
    }

    // TODO: recv/send flags
    
    // Read incoming data
    forn (iSide, 2) {
        if (chrwe[iSide][RD]) {
            Buffer &buffer = buf[iSide];
            if (HOST == iSide && isPreloadFinished && buffer.nBytesRead() < nBytesToPreload) {
                // Do not read any more data until all preloaded data is sent to client
                assert(buffer.nBytesWritten() >= nBytesToPreload);
                continue;
            }
            retVal = IR::SUCCESS;
            int result = recv(sock[iSide], buffer.writePtr(), (int)buffer.writeSize(), 0);
            if (result <= 0) {
                // Error or disconnected
                if (0 == result || !socket_error_is_temporary(socket_last_error())) {
                    kill_connection(iSide, result);
                }
            }
            else {
                uint64 t = time_ns();
                if (!buffer.maxSize()) {
                    tData1stReceivedFrom[iSide] = t;
                }
                tDataLastReceivedFrom[iSide] = t;
                buffer.commitWrite(result);
                //nBytesRecvFrom[iSide] += result;
            }
        }
    }
    
    forn(iSide, 2) {
        Buffer &buffer = buf[iSide];
        if (0 != buffer.nBytesUnread()) {
            if (sock_opened(iSide ^ 1)) {
                uint64 t = time_ns();
                if (HOST == iSide) {
                    if (!isPreloadFinished) {
                        if (buffer.nBytesWritten() < nBytesToPreload && sock_opened(iSide)) {
                            // Do not send any data to client if preload is not yet finished
                            continue;
                        }
                        isPreloadFinished = true;
                        tPreloadFinished = tData1stSentFrom[iSide] = t;
                    }
                    else {
                        if (!buffer.nBytesRead()) {
                            tData1stSentFrom[iSide] = t;
                        }
                    }
                }
                else {
                    // Client
                    if (0 == buffer.nBytesRead()) {
                        char * p = (char *)buffer.readPtr();
                        if (sock_opened(iSide) && 0 == memcmp(p, "POST /tb/xml HTTP", std::min((size_t)17, (size_t)buffer.nBytesWritten()))) {
                            uintptr match = 0;
                            static const char matchString[] = CRLFCRLF;
                            forn(i, buffer.nBytesWritten()) {
                                char c = p[i];
                                if (matchString[match++] != c) {
                                    match = 0;
                                }
                                else if (4 == match) {
                                    // Got complete header, analyse it!
                                    *buffer.writePtr() = '\0'; // terminate
                                    const char * cl;
                                    if (NULL == (cl = strstr(p, "Content-Length:"))) {
                                        goto not_select_request;
                                    }

                                    if (buffer.nBytesWritten() < (unsigned)i + strtoul(cl + 15, NULL, 10)) {
                                        continue;
                                    }

                                    // Got header and body

                                    if (NULL == strstr(p + i, "<select version")
                                        || NULL != strstr(p + i, "<live>true</live>")) {
                                        // Not preloading non-select requests and live select requests
                                        nBytesToPreload = 0;
                                    }
                                    else {
                                        iprintf("%s - Detected select request, will preload\n", name.c_str());
                                    }
                                    // Finally send to the recipient
                                    goto perform_send;
                                }
                            }
                            // HTTP request header is not yet finished (and connection not yet closed), so do not relay the data for now
                            continue;
                        }
                    not_select_request:
                        nBytesToPreload = 0;

                    perform_send:
                        if (!buffer.nBytesRead()) {
                            tData1stSentFrom[iSide] = t;
                        }
                    }
                }

                retVal = IR::SUCCESS;
                int result = send(sock[iSide ^ 1], buffer.readPtr(), (int)buffer.readSize(), 0);
                if (result <= 0) {
                    // Error or disconnected
                    if (!socket_error_is_temporary(socket_last_error())) {
                        kill_connection(iSide ^ 1, result);
                    }
                }
                else {
                    buffer.commitRead(result);
                    tDataLastSentFrom[iSide] = t;
                    //nBytesSentFrom[iSide] += result;
                }

                if (0 == buffer.nBytesUnread()) {
                    // TODO: check that when preloaded data is completely sent, the buffer read/write pointers are reset
                    buffer.reset();
                }
            }
            else {
                // If "send" connection is closed, discard all unsent data (but the buffer will remember the amount of that data)
                buffer.reset();
            }
        }
    }
        
    forn(iSide, 2) {
        // If this side is closed, opposite is opened, but we have nothing to send, close opposite
        if (!sock_opened(iSide) && sock_opened(iSide ^ 1) && 0 == buf[iSide].nBytesUnread()) {
            retVal = IR::SUCCESS;
            kill_connection(iSide ^ 1, -1);
        }
    }

    if (!sock_opened(HOST) && !sock_opened(CLIENT)) {
        uint64_t discarded[2] = { buf[CLIENT].nBytesUnreadTotal(), buf[HOST].nBytesUnreadTotal() };
        tEnd = time_ns();
        forn(iSide, 2) if (0 != discarded[iSide]) {
            err("%s discarded %llu bytes from %s due to broken connection", name.c_str(), (ulonglong)discarded[iSide], sideNames[iSide]);
        }

        if (0 == buf[CLIENT].maxSize() && 0 == buf[HOST].maxSize()) {
            iprintf("%s finished in %lfms, nothing sent or received\n", name.c_str(), 1E-6 * (tEnd - tStart));
        }
        else {
            iprintf("%s finished in %lfms,\n\t", name.c_str(), 1E-6 * (tEnd - tStart/*tData1stReceivedFrom[CLIENT]*/));
            print_buffer_stats(CLIENT);
            iprintf(",\n\t");
            print_buffer_stats(HOST);
            iprintf("\n");
        }
        return IR::FINISHED;
    }

    return retVal;
}





//int Session::run()
//{
//    try {
//        printf("Request %s finished\n", name.c_str());
//    }
//    catch (exception &e) {
//        printf("EXCEPTION: %s\n", e.what());
//    }
//    return 0;
//}

#define is_arg(value) (0 == _strcmpi(arg, #value))
#define is_parm(value) (0 == _strcmpi(parm, #value))

int main(int argc, char* argv[])
{
    SessionArray server;

    infoOutputFile = stdout;

    server.remoteHostName = "localhost";
    server.remotePort = 8022;
    server.localPort = 8200;
    server.nBytesToPreload = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *parm = i + 1 < argc ? argv[i + 1] : "";
        if (is_arg(-h) || is_arg(--help) || is_arg(/?)) {
            printf("Usage: \n\t-a --host   \thostname\n\t-r --remoteport\tremoteport\n\t-l --localport \tlocalport\n\t-p --preload \tnum_bytes_to_preload\n" );
            exit(0);
        }
        else if (is_arg(--localport) || is_arg(-l)) {
            server.localPort = (uint16)strtol(parm, NULL, 10);
        }
        else if (is_arg(--remoteport) || is_arg(-r)) {
            server.remotePort = (uint16)strtol(parm, NULL, 10);
        }
        else if (is_arg(--host) || is_arg(-a)) {
            server.remoteHostName = parm;
        }
        else if (is_arg(--preload) | is_arg(-p)) {
            server.nBytesToPreload = strtoull(parm, NULL, 10);
        }
        else {
            continue; // If command not recognized, do not skip 1 extra field
        }
        ++i; // Skip parameter
    }

    iprintf("TCP proxy is listening at localhost:%d, redirecting to %s:%d, preload size: %llu\n",
        server.localPort, server.remoteHostName.c_str(), server.remotePort, (ulonglong)server.nBytesToPreload);

    if (!server.open()) {
        err("Unable to start TCP server");
        return -1;
    }

    iprintf("READY.\n");

    IterationResult ir;
    uint64 tIdleStarted = time_ns();
    while (ir = server.select_iteration(), !ITERATION_FINISHED(ir))
    {
        if (IR::IDLE == ir) {
            uint64 tIdle = time_ns();
            if (tIdleStarted > tIdle) tIdleStarted = tIdle;
            tIdle -= tIdleStarted;
            if (tIdle > 100000000) {
                size_t sz = server.size();
                if (sz > 1) {
                    printf("\r%llu sessions idle for %llums  \r", (ulonglong)sz - 1, (ulonglong)tIdle / 1000000);
                }
                else {
                    printf("\rIdle for %llums                  \r", (ulonglong)tIdle / 1000000);
                }
            }
        }
        else {
            tIdleStarted = UINT64_MAX;
        }
    }

    iprintf("FINISHED.\n");
    return 0;
}