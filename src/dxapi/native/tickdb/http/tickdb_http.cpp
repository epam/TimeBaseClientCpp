#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "platform/sockets_impl.h"

#include <sstream>
#include <memory>
#include <set>
#include <errno.h>
#include <chrono>
#include <thread>

#include "util/critical_section.h"
#include "util/base64.h"
#include "io/tcp.h"
#include "xml/list_streams_request.h"
#include "xml/load_streams_request.h"
#include "xml/create_stream_request.h"
#include "xml/format_db_request.h"
#include "xml/validate_qql_request.h"


#include "tickcursor_http.h"
#include "tickloader_http.h"
#include "tickdb/tickstream_impl.h"
#include "tickdb/loader_manager.h"
#include "tickdb_http.h"


//#define SOCKET_READ_SIZE_LIMIT 0x400
using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace dx_thread;


#define LOGHDR "%s"
#define ID (this->textId())
//#define ID (this->textId()), (__func__)


#if VERBOSE_TICKDB >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

#if VERBOSE_TICKDB >= 2
//..
#else
//..
#endif

#define THIS_IMPL (impl(this))


#define CHECK_OPEN(LOC) do {                                                \
    if (!isOpen()) {                                                        \
        THROW_DBGLOG(LOGHDR "." LOC "() failed!: TickDb is not open!", ID);     \
        }                                                                   \
} while (0)

#define CHECK_WRITEACCESS(LOC) do {                                         \
    if (isReadOnly()) {                                                     \
        THROW_DBGLOG(LOGHDR "." LOC "() failed : TickDb is opened in readonly mode", ID);  \
    }                                                                       \
} while (0)

#define CHECK_OPEN_WRITEACCESS(msg) do {                                    \
    CHECK_OPEN(msg);                                                        \
    CHECK_WRITEACCESS(msg);                                                 \
} while (0)


TickDb::TickDb()
{}


static dx_thread::atomic_counter<uint64> db_id_counter;

#ifdef _WIN32
static void wsainit()
{

    int errorCode;
    WSADATA wsadata;

    errorCode = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (0 != errorCode) {
        auto out = last_socket_error(&errorCode, "TickDbImpl::TickDbImpl() - WSAStartup()");
        THROW_DBGLOG_IO(IOStreamException, errorCode, "%s", out.c_str());
    }
}

static void wsacleanup()
{
    WSACleanup();
}
#else
static void wsainit() {}
static void wsacleanup() {}
#endif

TickDbImpl::TickDbImpl() :
    onStreamDeletedCallback(NULL), onDisconnectedCallback(NULL),

    id_(db_id_counter.get()),

    readOnlyMode_(true), isOpened_(false), isClosed_(false), socketsInitialized_(false), 
    exceptionCork_(false), nBlockedExceptionThreads_(0),

    loaderManager_(*this), sessionHandler_(*this)
    
{
    //if (!socketsInitialized_) {
#if MANAGEMENT_THREAD_ENABLED
    init_management_thread();
#endif
    socketsInitialized_ = true;
    // Create unique text id for this object
    snprintf(id_.textId, sizeof(id_.textId), "TickDb-%llu", (ulonglong)id_.id);
    id_.textId[sizeof(id_.textId) - 1] = '\0';
    streamCache_.setId(id_.textId);
    wsainit();
}


TickDbImpl::~TickDbImpl()
{
    DBGLOG(LOGHDR ".~TickDbImpl(): destructor STARTED", impl(this)->textId());
    close();

    //if (socketsInitialized_) {
        wsacleanup();
    //}
    
    DBGLOG(LOGHDR ".~TickDbImpl(): destructor FINISHED", impl(this)->textId());
}


bool TickDbImpl::isOpen() const
{
    return isOpened_;
}


bool TickDbImpl::testConnection() const
{
    SOCKET s = createConnection(TCP::NoDelayMode::NORMAL, true);
    bool result = isValidSocket(s);
    if (result) closesocket(s);
    return result;
}


bool TickDbImpl::isReadOnly() const 
{
    return readOnlyMode_;
}


void TickDbImpl::close()
{
    srw_write section(thisLock_);
    DBGLOG(LOGHDR ".close(%s)", ID, uri.full.c_str());
    DBGLOG_VERBOSE(LOGHDR ".close(): flushing delayed exceptions", ID);
    flushExceptions();
    DBGLOG_VERBOSE(LOGHDR ".close(): stopping loader manager", ID);
    size_t lc = loaderManager_.loadersCount();
    const char *isOpenedStr = isOpened_ ? "Open" : "Closed";
    if (0 != lc) {
        DBGLOG_VERBOSE(LOGHDR ".close(): trying to stop %u orphaned loaders while deleting '%s' TickDB connection",
            ID, (unsigned)lc, isOpenedStr);
    }
    else {
        DBGLOG(LOGHDR ".close(): No orphaned loaders", ID);
    }

    loaderManager_.stop();
    sessionHandler_.stop();

    if (isOpen()) {
        concurrent_ptr_set_iter iter(cursors_);

        if (iter.size()) {
            DBGLOG(LOGHDR ".close(): trying to stop %u orphaned cursors while deleting '%s' TickDB connection",
                ID, (unsigned)iter.size(), isOpenedStr);

            TickCursorImpl * i;
            while (NULL != (i = (TickCursorImpl *)iter.next())) {
                if (cursors_.find(i)) {
                    i->close(isOpened_, true); // No throw, we want to close all cursors no matter what
                }
            }

            DBGLOG(LOGHDR ".close(): done", ID, (unsigned)iter.size());
        }
        else {
            DBGLOG(LOGHDR ".close(): No orphaned cursors", ID);
        }

        // Maybe move this code here
        /*auto streams = streamCache_.allStreams();
        for (auto i : streams) {
            assert(NULL != i);
            if (NULL != i) while (LockType::NO_LOCK != impl(i)->unlock()) {}
        }*/

        DBGLOG(LOGHDR ".close(): No orphaned cursors", ID);
        streamCache_.clear();
        this->isOpened_ = false;
    }

    isClosed_ = true;
}


#define SOCKCHECK(x, s, NOTHROW) do { if (0 != x) { errMsg = s; noThrow = NOTHROW; goto error; } } while (0)


SOCKET TickDbImpl::createConnection(enum TCP::NoDelayMode noDelayMode) const
{
    return createConnection(noDelayMode, false);
}


SOCKET TickDbImpl::createConnection() const
{
    return createConnection(TCP::NoDelayMode::NORMAL, false);
}


SOCKET TickDbImpl::createConnection(enum TCP::NoDelayMode noDelayMode, bool nothrowOnWrongAddr) const
{
    int result;
    int socketOptionsFlag;

    const char *errMsg = NULL;
    bool noThrow = false;

    struct sockaddr_in local_addr; // , remote_addr;
    struct addrinfo *addr = NULL, *current_addr, hints;
    SOCKET s0 = INVALID_SOCKET;

    ::memset(&local_addr, 0, sizeof(local_addr));

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = 0;

    s0 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (!isValidSocket(s0)) {
        SOCKCHECK(-1, "socket() call failed!", false);
    }

    // Enable SO_REUSEADDR
    socketOptionsFlag = 1;
    result = setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, (char *)&socketOptionsFlag, sizeof(socketOptionsFlag));
    SOCKCHECK(result, "setsockopt(SO_REUSEADDR =1) failed!", false);

    result = ::bind(s0, (struct sockaddr *)&local_addr, sizeof(local_addr));
    SOCKCHECK(result, "bind() failed!", false);

    ::memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    result = getaddrinfo(uri.host.c_str(), uri.portString.c_str(), &hints, &addr);
    SOCKCHECK(result, "getaddrinfo() failed!", nothrowOnWrongAddr);
    
    current_addr = addr;
    for (current_addr = addr; current_addr != NULL; current_addr = current_addr->ai_next) {
        // Connect to server.
        result = connect(s0, current_addr->ai_addr, (int)current_addr->ai_addrlen);
        if (SOCKET_ERROR == result) {
            continue;
        }

        break;
    }

    SOCKCHECK(result, "connect() failed!", false);
    freeaddrinfo(addr);
    addr = NULL;

    // Set no-delay mode
    //if (noDelayMode) {
    //    DBGLOG("..setting TCP_NODELAY to 1");
    //}
    socketOptionsFlag = noDelayMode != 0;
    result = setsockopt(s0, IPPROTO_TCP, TCP_NODELAY, (char *)&socketOptionsFlag, sizeof(socketOptionsFlag));

    SOCKCHECK(result, "setsockopt(TCP_NODELAY) failed!", false); // Not a critical error, maybe shouldn't throw

    return s0;

error:

    string out, socketMsg;
    int errorCode;
    socketMsg = last_socket_error(&errorCode, "");
    if (NULL != addr) {
        freeaddrinfo(addr);
        addr = NULL;
    }
       //.append(" / ").append(socketErrMsg))
       ;

    if (isValidSocket(s0)) {
        closesocket(s0);
        s0 = INVALID_SOCKET;
    }

    DBGLOGERR(&out, LOGHDR ": %s (%s) %s", ID, errMsg, uri.full.c_str(), socketMsg.c_str());
    if (!noThrow) {
        throw DxApiImpl::IOStreamException(errorCode, out);
    }
    else {
        return INVALID_SOCKET;
    }
}


bool TickDbImpl::open(bool readOnlyMode)
{
    srw_write section(thisLock_);
    if (isClosed_) {
        std::string out;
        DBGLOGERR(&out, LOGHDR ".open(): ERROR: Already closed!", ID, uri.full.c_str());
        return false;
    }

    // We estabilish 1 TCP connection at open, immediately, just for testing
    do {

        isOpened_ = testConnection();
        if (!isOpened_) {
            DBGLOG(LOGHDR ".open(): Unable to estabilish connection to: %s", ID, uri.full.c_str());
            break;
        }
        
        readOnlyMode_ = readOnlyMode;
        //listStreamsAsStringVector();
        if (!loaderManager_.start()) {
            isOpened_ = false;
            DBGLOG(LOGHDR ".open(): Unable to start loaderManager", ID);
            break;
        }

        exceptionCork_ = true;
        sessionHandler_.start();
        // vector<string> v;
        // sessionHandler_.getStreams(v);
    } while (0);
    
    return isOpen();
}


TickStream * TickDbImpl::getStream(const std::string &key)
{
    if (!isOpen()) {
        return NULL;
    }

    TickStream * stream = streamCache_.get(key);
    if (NULL != stream) {
        return stream;
    }

    DBGLOG(LOGHDR ".getStream(%s): cache returned NULL, updating..", ID, key.c_str());

    if (0 == streamCache_.size()) {
        updateCache();
    }

#if USE_SH
    // Request data for this stream, then try getting it from the cache again
    sessionHandler_.getStreamSynchronous(key.c_str());
    return streamCache_.get(key);

#else
    // Could use getTimeRange or GetPeriodicity, but then I'd again need a version that doesn't throw error exceptions
    set<string> keys(listStreamsAsStringSet());

    if (keys.find(key) != keys.end()) {
        DBGLOG(LOGHDR ".getStream('%s'): Stream Cache update triggered", ID, key.c_str());
        updateCache();
        return streamCache_.get(key);
    }
#endif

    return NULL;
}

bool TickDbImpl::format()
{
    if (isOpen()) {
        THROW_DBGLOG("%s: format() : TickDb is open, should be closed!", ID);
    }

    FormatDbRequest req(*this);
    req.allowUnopenedDb = true;
    bool success = req.execute();
    if (success) {
        streamCache_.clear();
    }

    return success;
}


bool TickDbImpl::validateQql(QqlState &out, const std::string &qql)
{
    srw_read section(thisLock_);
    CHECK_OPEN("ValidateQql");
    ValidateQqlRequest req(*this, qql);
    auto success = req.getState(out);
    if (!success) {
        THROW_DBGLOG(LOGHDR "ValidateQql request failed!", ID);
    }

    return true;
}


TickStream * TickDbImpl::createStream(const std::string &key, const std::string &name, const std::string &description, int distributionFactor)
{
    if (!isOpen()) {
        return NULL;
    }

    CHECK_WRITEACCESS("createStream");

    StreamOptions options;
    options.name = name;
    options.description = description;
    options.distributionFactor = distributionFactor;
    
    return this->createStream(key, options);
}


void TickDbImpl::disposeStream(DxApi::TickStream *stream)
{
    if (NULL != stream) {
        auto streamImpl = impl(stream);
        DBGLOG_VERBOSE(LOGHDR ".disposeStream('%s')", ID, streamImpl->key_.c_str());
        streamImpl->key_.append("[DEAD]");
        delete streamImpl;
    }
}


TickStreamImpl* TickDbImpl::allocateStream(const std::string &key, const DxApi::StreamOptions &options)
{
    TickStreamImpl * created = new TickStreamImpl(*this, key, options, stream_id_counter_.get()); // +std::move as optimization ?
    if (NULL == created) {
        THROW_DBGLOG(LOGHDR "Unable to create TickStreamImpl for key '%s'", ID, key.c_str());
    }

    return created;
}


TickCursorImpl* TickDbImpl::allocateCursor()
{
    TickCursorImpl *created = new TickCursorImpl(*this, cursor_id_counter_.get());
    if (NULL == created) {
        THROW_DBGLOG(LOGHDR "Unable to create Tick Cursor Object", ID);
    }
    return created;
}

void TickDbImpl::remove(TickCursorImpl * cursor)
{
    if (NULL != cursors_.find(cursor)) {
        DBGLOG(LOGHDR ": Cursor %s removes itself from autodeletion pool", ID, cursor->textId().c_str());
        cursors_.remove(cursor);
    }
}


void TickDbImpl::add(TickCursorImpl * cursor)
{
    DBGLOG(LOGHDR ": Cursor %s added itself to the pool", ID, cursor->textId().c_str());
    cursors_.add(cursor);
}


TickLoaderImpl* TickDbImpl::allocateLoader()
{
    TickLoaderImpl * created = new TickLoaderImpl(*this, loader_id_counter_.get());
    if (NULL == created) {
        THROW_DBGLOG(LOGHDR "Unable to create Tick Loader Object", ID);
    }

    return created;
}



TickStreamImpl* TickDbImpl::addStream(TickStreamImpl * stream)
{
    assert(stream);
    streamCache_.add(stream);
    DBGLOG_VERBOSE(LOGHDR ".addStream(): Tickstream '%s' added to cache", ID, impl(stream)->key().c_str());
    return stream;
}


TickStream* TickDbImpl::createStream(const std::string &key, const StreamOptions &options)
{
    if (!isOpen()) {
        return NULL;
    }
    
    CHECK_WRITEACCESS("createStream");

    DBGLOG_VERBOSE(LOGHDR ".createStream(): createRequest for stream '%s' is being sent", ID, key.c_str());
    CreateStreamRequest req(*this, key, options);

    if (req.execute()) {
#ifdef USE_SH
        return getStream(key);
#else
        return addStream(allocateStream(key, options));
#endif

    }
    else {
        DBGLOG_VERBOSE(LOGHDR ".createStream(): createRequest for stream '%s' failed", ID, key.c_str());
        return NULL;
    }
}


TickStream* TickDbImpl::createFileStream(const std::string &key, const std::string &dataFile)
{
    if (!isOpen()) {
        return NULL;
    }

    CHECK_WRITEACCESS("createFileStream");
    DBGLOG_VERBOSE(LOGHDR ".createFileStream(): createRequest for file stream '%s' is being sent", ID, key.c_str());
    CreateFileStreamRequest req(*this, key, dataFile);

    if (req.execute()) {
        return getStream(key);
    }
    else {
        DBGLOG_VERBOSE(LOGHDR ".createFileStream(): createRequest for file stream '%s' failed", ID, key.c_str());
        return NULL;
    }
}


//INLINE vector<string> TickDbImpl::listStreamsAsStringVector()
//{
//    ListStreamsRequest req(*this);
//    return req.getStreamKeysAsVector();
//}
//
//
//INLINE set<string> TickDbImpl::listStreamsAsStringSet()
//{
//    ListStreamsRequest req(*this);
//    return req.getStreamKeysAsSet();
//}


vector<TickStream *> TickDbImpl::listStreams()
{
    //vector<unique_ptr<TickStream>> v;

    CHECK_OPEN("listStreams");
    
    DBGLOG(LOGHDR ".listStreams()..", ID);

#if USE_SH
    
    
    // Serve this request from the cache, unless cache is empty

    /*if (0 == streamCache_.size()) {
        sessionHandler_.getStreamsSynchronous();
    }*/

    sessionHandler_.getStreamsSynchronous();

#else
    vector<string> newStreamKeys(listStreamsAsStringVector());

    if (!streamCache_.matchKeys(newStreamKeys)) {
        DBGLOG(LOGHDR ".listStreams(): Stream Cache update triggered", ID);
        updateCache();
    }
#endif

    DBGLOG(LOGHDR ".listStreams(): returning %u streams", ID, (unsigned)streamCache_.size());
    return streamCache_.allStreams();
}


// UpdateCache will currently read all streams
void TickDbImpl::updateCache()
{
#if !USE_SH
    LoadStreamsRequest req(*this);
    LoadStreamsRequest::Response newStreams;

    if (!req.getStreams(newStreams)) {
        THROW_DBGLOG(LOGHDR "Unable to read stream metadata!", ID);
    }

    DBGLOG(LOGHDR ".updateCache()", ID);
    streamCache_.synchronize(*this, newStreams);
#else

    DBGLOG(LOGHDR ".updateCache()", ID);
    sessionHandler_.getStreamsSynchronous();
#endif
}


// Internal method that implements all other methods
TickCursor* TickDbImpl::select(TimestampMs time, SelectionOptionsImpl &options, const vector<const TickStream *> *streams, const vector<string> *types, const vector<std::string> *entities,
    const string *qql, const vector<QueryParameter> *qqlParameters)
{
    if (!isOpen()) {
        return NULL;
    }

    // Create cursor implementation class, pass parameters and call its internal select method that does the actual work
    TickCursorImpl *cursor = allocateCursor();

    if (TIMESTAMP_NULL != time) {
        if (options.reverse) {
            options.to = time;
        }
        else {
            options.from = time;
        }
    }

    if (nullptr != qql) {
        options.qql = *qql;
        if (nullptr != qqlParameters) {
            auto &params = options.qqlParameters;
            params.reserve(qqlParameters->size());
            params.clear();
            for (auto const &i : *qqlParameters) {
                if (0 == i.name.length()) {
                    THROW_DBGLOG(LOGHDR "select() : QQL parameter name field can't be empty", ID);
                }

                if (0 == i.type.length()) {
                    THROW_DBGLOG(LOGHDR "select() : QQL parameter type field can't be empty", ID);
                }

                params.push_back(i);
            }
        }
    }

    cursor->options = options;
    cursor->setStreams(streams);
    cursor->select(types, entities);

    return cursor;
}


TickCursor* TickDbImpl::select(TimestampMs time, const SelectionOptions &options, const vector<const TickStream *> *streams, const vector<string> *types, const vector<std::string> *entities,
    const string *qql, const vector<QueryParameter> *qqlParameters)
{
    SelectionOptionsImpl opt(options);
    return this->select(time, opt, streams, types, entities, qql, qqlParameters);
}


TickCursor* TickDbImpl::select(TimestampMs time, const vector<const TickStream *> *streams, const SelectionOptions &options, const vector<string> *types, const vector<std::string> *entities)
{
    return this->select(time, options, streams, types, entities);
}


INLINE TickCursor* TickDbImpl::select(TimestampMs time, const vector<const TickStream *> &streams, const SelectionOptions &options, const vector<string> *types, const vector<std::string> *entities)
{
    return this->select(time, &streams, options, types, entities);
}


TickCursor* TickDbImpl::select(TimestampMs time, const vector<TickStream *> *streams, const SelectionOptions &options, const vector<string> *types, const vector<std::string> *entities)
{
    // C++ is shit.
    vector<const TickStream *> tmp;
    for (auto i : *streams) {
        tmp.push_back(i);
    }

    return this->select(time, tmp, options, types, entities);
}


#define NULL_STREAMS ((const vector<const DxApi::TickStream * > *)NULL)

INLINE TickCursor* TickDbImpl::select(TimestampMs time, const vector<TickStream *> &streams, const SelectionOptions &options, const vector<string> *types, const vector<std::string> *entities)
{
    return this->select(time, &streams, options, types, entities);
}


TickCursor* TickDbImpl::executeQuery(const string &qql, const SelectionOptions &options, TimestampMs time, const vector<std::string> *instruments, const vector<QueryParameter> &params)
{
    return this->select(time, options, NULL_STREAMS,  NULL, instruments, &qql, &params);
}


TickCursor* TickDbImpl::executeQuery(const std::string &qql, const SelectionOptions& options, TimestampMs time, const vector<QueryParameter> &params)
{
    return this->select(time, options, NULL_STREAMS,  NULL, NULL, &qql, &params);
}


TickCursor* TickDbImpl::executeQuery(const std::string &qql, const SelectionOptions &options, const vector<QueryParameter> &params)
{
    return this->select(TIMESTAMP_UNKNOWN, options, NULL_STREAMS, NULL, NULL, &qql, &params);
}


TickCursor* TickDbImpl::executeQuery(const std::string &qql, const vector<QueryParameter> &params)
{
    return this->select(TIMESTAMP_UNKNOWN, SelectionOptionsImpl(), NULL, NULL, NULL, &qql, &params);
}


TickCursor * TickDbImpl::createCursor(const DxApi::TickStream *stream, const SelectionOptions &options)
{
    if (NULL != stream) {
        vector<const DxApi::TickStream *> streams(1, stream);
        return this->select(TIMESTAMP_UNKNOWN, options, &streams, NULL, NULL);
    }
    else {
        // This doesn't look good. Oh, well..
        return this->select(TIMESTAMP_UNKNOWN, options, NULL_STREAMS, NULL, NULL);
    }
}

#undef NULL_STREAMS


TickLoader * TickDbImpl::createLoader(const TickStream *stream, const LoadingOptions &options)
{
    CHECK_WRITEACCESS("createLoader");

    if (!isOpen()) {
        return NULL;
    }

    TickLoaderImpl * loader = allocateLoader();
    loader->open(stream, options);
    return loader;
}


void TickDbImpl::setStreamNamespacePrefix(const string &prefix)
{
    setStreamNamespacePrefix(prefix.c_str());
}


void TickDbImpl::setStreamNamespacePrefix(const char *prefix)
{
    if (isOpen()) {
        THROW_DBGLOG(LOGHDR ".setStreamNamespacePrefix(\"%s\") failed : TickDb is already opened", ID, prefix);
    }

    streamNamespacePrefix_ = prefix;
    // TODO: Implement in TickStream-related code (filter all operations on stream by tag, making non-compliant streams "invisible") !!!
}


IOStream * TickDbImpl::createConnectionStream(enum TCP::NoDelayMode noDelayMode, bool filteredExceptions) const
{
    SOCKET socket = createConnection(noDelayMode);
    dx_assert(isValidSocket(socket), "Socket should be valid");
    IOStreamTCP * ioStream = new IOStreamTCP(socket);

#ifdef DELAYED_IO_EXCEPTIONS
    if (filteredExceptions) {
        ioStream->setDisconnectionCallback((IOStream::DisconnectionCallback)onIOStreamDisconnectedCallback, (void *)this);
    }
#endif

    return ioStream;
}


IOStream * TickDbImpl::sendRequest( const char * request,
                                    const char * path,
                                    const std::string * requestBody,
                                    HTTP::ContentType contentType,
                                    HTTP::ContentLength contentLengthEncoding,
                                    TCP::NoDelayMode noDelayMode,
                                    bool filteredExceptions) const
{
    static const char * requestContentTypeExists[2] = { "", CRLF "Content-Type: "};
    static const char * requestContentType[3] = { "", "text/xml", "application/x-www-form-urlencoded" };
    static const char * requestContentLengthEncoding[3] = { "", CRLF "Content-Length: ", CRLF "Transfer-Encoding: chunked" };

    if (NULL == requestBody) {
        contentType = HTTP::ContentType::NONE;
        contentLengthEncoding = HTTP::ContentLength::NONE;
    }
    else {
        if (HTTP::ContentType::NONE == contentType) {
            contentType = HTTP::ContentLength::CHUNKED != contentLengthEncoding ? HTTP::ContentType::XML : HTTP::ContentType::BINARY;
        }

        assert(HTTP::ContentType::NONE != contentType);
        if (HTTP::ContentLength::NONE == contentLengthEncoding) {
            contentLengthEncoding = HTTP::ContentType::BINARY == contentType ? HTTP::ContentLength::CHUNKED : HTTP::ContentLength::FIXED;
        }
    }

    IOStream * ioStream = createConnectionStream(noDelayMode, filteredExceptions);
    const string& host = uri.host;
    char buf[0x3000];
    std::string header;
    uintptr len = host.size();
    if (host.size() > sizeof(buf) - 0x200) {
        delete ioStream;
        THROW_DBGLOG(LOGHDR "sendRequest() : Host name length is too long: %llu", ID, (ulonglong)len);
    }

    // Host name length is already checked, using fixed-size buffer should be safe
    snprintf(buf, sizeof(buf),
        "%s %s HTTP/1.1" \
        CRLF "Host: %s" \
        CRLF "Connection: close" \
        /*content type*/ "%s%s" 
        , request, path
        , host.c_str()
        , requestContentTypeExists[HTTP::ContentType::NONE != contentType]
        , requestContentType[(unsigned)contentType]
        );

    buf[sizeof(buf) - 1] = '\0';

    header.append(buf);

    buf[0] = '\0';
    switch (contentLengthEncoding) {
    case HTTP::ContentLength::FIXED:
        snprintf(buf, sizeof(buf), CRLF "Content-Length: %llu", (ulonglong)requestBody->size());
        break;

    case HTTP::ContentLength::NONE:
    case HTTP::ContentLength::CHUNKED:
        snprintf(buf, sizeof(buf), CRLF "%s", requestContentLengthEncoding[(unsigned)contentLengthEncoding]);
        break;
    }

    header.append(buf).append(CRLF);

    if (basicAuth_.size()) {
        secure_string s(basicAuth_);
        scramble(s);
        header.append("Authorization: Basic ").append(s).append(CRLF);
    }

    header.append(CRLF);
    ioStream->write(header);

    if (NULL != requestBody) {
        ioStream->write(*requestBody);
    }

    return ioStream;
}


static const char * tbPostRequestPath[2] = { "/tb/bin", "/tb/xml" };


IOStream * TickDbImpl::sendTbPostRequest(   const std::string * requestBody,
                                            HTTP::ContentType contentType,
                                            HTTP::ContentLength contentLengthEncoding,
                                            TCP::NoDelayMode noDelayMode,
                                            bool filteredExceptions) const
{
    
    return sendRequest("POST", tbPostRequestPath[(unsigned)contentType], requestBody, contentType, contentLengthEncoding, noDelayMode, filteredExceptions);
}


unsigned HTTP::readResponseHeader(ResponseHeader& responseHeader, DataReaderInternal& reader)
{
    const char matchString[] = CRLFCRLF;
    uintptr match = 0;
    responseHeader.clear();

    // Read header, byte after byte until we have empty line
    // We read indefinitely until socket is closed or empty line is received
    for (uintptr match = 0; match < 4;) {
        char c = (char)reader.getByte();
        if (matchString[match++] != c) {
            match = 0;
        }

        responseHeader.push_back(c);
    }

    // TODO: Temporary debug code
    return responseHeader.parse();
}

HTTP::ResponseHeader::ResponseHeader()
: code(0), responseStringEndOffset(0)
{}


std::string HTTP::ResponseHeader::responseText() const
{
    const char * c = c_str();
    unsigned responseStringEndOffset = this->responseStringEndOffset;
    if (responseStringEndOffset < responseStartOffset) {
        responseStringEndOffset = responseStartOffset;
    }

    return std::string(c + responseStartOffset, c + responseStringEndOffset);
}


unsigned HTTP::ResponseHeader::parseException(const char *fmt)
{
    const char * resized = "";
    if (size() > 0x380) {
        resize(0x380);
        resized = "<TRIMMED>";
        THROW_DBGLOG("HTTP::ResponseHeader::parse(): Unexpected HTTP header returned: %s%s", c_str(), resized);
    }

    return 0;
}


unsigned HTTP::ResponseHeader::parse()
{
    const char * responseStr = c_str();
    size_t len = length();
    const char * end = responseStr + len;

    const char * eol = strstr(responseStr + responseStringStartOffset, CRLF);
    if (NULL == eol) {
        return parseException("responseHeader::parse(): Logic error! CRLFCRLF should be present at the end: %s%s");
    }

    responseStringEndOffset = (unsigned)(eol - responseStr);

    if (!strhasprefix(responseStr, len,  "HTTP/1.1 ", 9) || len < responseStringStartOffset) {
        return parseException("HTTP::ResponseHeader::parse(): Unexpected HTTP header returned: %s%s");
    }
    
    char *codeEnd = (char *)end;
    unsigned httpResponseCode = code = (unsigned)strtoul(responseStr + 9, &codeEnd, 10);

    if (httpResponseCode < 100 || httpResponseCode >= 600) {
        limitLengthTo(0x400);
        THROW_DBGLOG("HTTP::parseResponseHeader(): %s: %s", 0 != httpResponseCode ? "Unknown HTTP response code" : "HTTP response code not found", responseText().c_str());
    }
    
    return httpResponseCode;
}


intptr_t HTTP::ResponseHeader::parseContentLength()
{
    size_t ofs = find("Content-Length:");
    if (string::npos == ofs) {
        size_t ofs = find("Encoding: chunked");
        if (string::npos == ofs) {
            // Found neither 'Content-Length:', nor 'Encoding: chunked'
            return 0;
        }
        return -1;
    }
    else {
        return (intptr)strtoull(c_str() + (ofs + 15/*=strlen("Content-Length:")*/), NULL, 10);
    }
}


void HTTP::ResponseHeader::limitLengthTo(unsigned limit)
{
    if (responseStringEndOffset > limit) {
        responseStringEndOffset = limit;
    }
}



bool HTTP::readResponseBody(string& responseBody, DataReaderInternal& reader, intptr contentLength)
{
    responseBody.clear();
    if (contentLength > 0) {
        // Normal encoding
        responseBody.resize(contentLength);
        for (intptr i = 0; i < contentLength; ++i) {
            responseBody[i] = reader.getByte();
        }
        return true;
    }
    if (0 == contentLength) {
        try {
            while (1) responseBody.push_back(reader.getByte());
        }
        catch (IOStreamDisconnectException &) {
            return true;
        }
    }
    else {
        // Chunked encoding Read chunked stream into string
        intptr chunkSize;
        string sizeText;

        while (1) {
            sizeText.clear();
            while (1) {
                byte ch = reader.getByte();
                if (!((ch - '0') < 10U || (ch - 'a') < 6U || (ch - 'A') < 6U)) {
                    if (';' == ch) {
                        // Try to skip chunk extension
                        while ('\r' != reader.getByte())
                        {}
                    } else
                    if (ch != '\r') {
                        // Chunk length is terminated incorrectly
						DBGLOG("chuck length is terminated incorrectly");
                        return false;
                    }

                    break;
                }

                sizeText.push_back(ch);
            }
            
            if (sizeText.size() < 1) {
                // At least one digit required!
				DBGLOG("At least one digit required!");
                return false;
            }

            chunkSize = strtoul(sizeText.c_str(), NULL, 0x10);
            if (0 == chunkSize)
                break;

            reader.skip(1); // Skip LF
            intptr prevSize = responseBody.size();
            responseBody.resize(prevSize + chunkSize);
            for (intptr i = 0; i < chunkSize; ++i) {
                responseBody[prevSize + i] = reader.getByte();
            }

            reader.skip(2); // Skip CRLF
        }

        return true;
    }
}


bool HTTP::tryExtractTomcatResponseMessage(string& responseBody)
{
    const char * cstr = responseBody.c_str();
    if (const char * start = strstr(cstr, "Status report</p><p><b>message</b>")) {
        again:
        const char * end = strstr(start, "</u>");
        if (NULL != end) {
            start = strstr(start, "<u>");
            if (NULL != start && end > start && end - start > 4) {
                responseBody = string(start + 3, end);
                return true;
            }
            else {
                start = end + 3;
                goto again;
            }
        }
    }

    return false;
}


int TickDbImpl::executeRequest( string& responseBody,
                                const char * request,
                                const char * path,
                                const std::string * requestBody,
                                const char * requestName,
                                bool allowUnopenedDb)
{
    byte buffer[0x2000 + 0x200];
    uintptr nReadTotal = 0;
    intptr contentLength;
    HTTP::ResponseHeader responseHeader;
    unsigned httpResponseCode = 0;

    // No 'isOpen()' check for some requests. 'Format' should be called on closed database 
    requestName = NULL == requestName ? "" : requestName;

    if (!allowUnopenedDb && !isOpen()) {
        DBGLOG(LOGHDR ".executeRequest(%s): TickDb is not in 'open' state", ID, requestName);
        return 0;
    }

    unique_ptr<IOStream> ioStream(sendRequest(request, path, requestBody, HTTP::ContentType::XML, HTTP::ContentLength::FIXED, TCP::NO_DELAY, false));
    DataReaderMsg reader(buffer + 0x100, 0x2000, 0, ioStream.get());

    // This is additional debug logging code for the case http://rm.orientsoft.by/issues/4287 (Garbage response header returned sometimes)
    try {
        httpResponseCode = HTTP::readResponseHeader(responseHeader, reader);
    }
    catch (...) {
        DBGLOG(LOGHDR, ".executeRequest(): ERR: Unable to parse HTTP response header (bytes written: %llu, read: %llu, resp.hdr.size: %llu):\n",
            ID, (ulonglong)ioStream->nBytesWritten(), (ulonglong)ioStream->nBytesRead(), (ulonglong)responseHeader.size(), responseHeader.c_str());
        throw;
    }

    // Not successful?
    contentLength = responseHeader.parseContentLength();
    if (!HttpSuccess(httpResponseCode)) {
        switch (httpResponseCode) {
        case 401:    
            DBGLOG(LOGHDR ".executeRequest(%s): 401 Unauthorized", ID, requestName);
            break;
        }

        if (HTTP::readResponseBody(responseBody, reader, contentLength) && responseBody.length()) {
            HTTP::tryExtractTomcatResponseMessage(responseBody);
        } else {
            // If not able to read response body, copy http error text there
            responseBody = responseHeader.responseText();
        }

        return httpResponseCode;
    }
	
	// todo: close output causes exception due executing response.
    //ioStream->closeOutput();
    bool result = HTTP::readResponseBody(responseBody, reader, contentLength);
    ioStream->close();

    return result ? httpResponseCode : 0;
}


int TickDbImpl::executeTbPostRequest(string& responseBody, const std::string &requestBody, const char * requestName, bool allowUnopenedDb)
{
    return executeRequest(responseBody, "POST", tbPostRequestPath[(unsigned)HTTP::ContentType::XML], &requestBody, requestName, allowUnopenedDb);
}


tinyxml2::XMLError TickDbImpl::xmlRequest(tinyxml2::XMLDocument &doc, const std::string &requestBody) 
{
    string response;
    bool result;

    doc.Clear();
    result = HttpSuccess(executeTbPostRequest(response, requestBody));
    if (false == result) {
        return tinyxml2::XML_ERROR_FILE_READ_ERROR;
    }
    
    return doc.Parse(response.data(), response.size());
}


tinyxml2::XMLError TickDbImpl::xmlRequest(tinyxml2::XMLDocument &doc, const std::stringstream &requestBody)
{
    return xmlRequest(doc, requestBody.str());
}


bool TickDbImpl::setUri(const std::string &uri_)
{
    if (!uri.parse(uri_)) {
        return false;
    }

    if ((uri.protocol != "dxtick" && uri.protocol != "")
        || uri.queryString.size() != 0
        || uri.path.size() != 0
    )
        return false;
    
    if (0 == uri.port) {
        uri.port = TDB::DEFAULT_PORT;
    }

    return true;
}


bool TickDbImpl::setAuth(const char * username, const char * password)
{
    size_t l = strlen(username) + strlen(password) + 1;

    if (NULL == username || l <= 1) {
        return false;
    }

    secure_string tmp;
    tmp.reserve(l);
    tmp.append(username).append(":").append(password);
    Base64MIME::base64_encode(basicAuth_, tmp);
    scramble(basicAuth_);

    username_.clear();
    username_.append(username);
    scramble(username_);

    encodedPass_.clear();
    Base64MIME::base64_encode(encodedPass_, password, strlen(password));
    scramble(encodedPass_);

    return true;
}


void TickDbImpl::onStreamDeleted(const std::string &key)
{
    auto removed = streamCache_.tryRemove(key);
    auto onStreamDeletedCallback = this->onStreamDeletedCallback;
    // We only send this notification if the removed stream already exists in cache
    if (NULL != onStreamDeletedCallback && NULL != removed) {
        DBGLOG(LOGHDR ".streamDeleted(%s): removed from cache, now calling callback", ID, key.c_str());
        DXAPIIMPL_DISPATCH(onStreamDeleted, onStreamDeletedCallback, (this, removed), ID);
        disposeStream(removed);
        // TODO: removed stream should be deallocated to avoid memory leaks!!!!
    }
    else {
        DBGLOG(LOGHDR ".streamDeleted(%s): WRN: not found in cache", ID, key.c_str());
    }
}


bool TickDbImpl::allCursorsAndLoadersDisconnected()
{
    // TODO: Currently we don't wait for disconnection of cursors/loaders, because this won't help us anyway
    return true; 
}


void TickDbImpl::onSessionStopping()
{
    DBGLOG(LOGHDR ".onSessionStopping()", ID);
}


// IMPORTANT NOTE: The API client is allowed to delete the TickDb class from the onDisconnected() handler (but not required to do so).
void TickDbImpl::onDisconnected()
{
    // This is necessary to support logging from deleted class
    char myid[800];
    strcpy(myid, this->textId());
    DXAPIIMPL_DISPATCH(onDisconnected, onDisconnectedCallback, (this), myid);
    // At this point this class may already be deleted (destructor already called)

    // TODO: Find a way to possibly flush exceptions if the client DIDN'T do that and didn't delete the class
    // (how do we reliably detect if class pointed by this is already deleted without causing access violation?)
}


void TickDbImpl::dbgBreakConnection()
{
    srw_write section(thisLock_);
    DBGLOG(LOGHDR ".dbgBreakConnection(): STARTED", ID);
    sessionHandler_.dbgBreakConnection();
    DBGLOG(LOGHDR ".dbgBreakConnection(): FINISHED", ID);
}


// todo: figure out what this method stands for
INLINE bool TickDbImpl::onIOStreamDisconnected(IOStream *sender, const char * string)
{
    bool retval = false;
    DBGLOG(LOGHDR ".onIOStreamDisconnected(): STARTED", ID);
    //db_->cursorDisconnected(this);
    if (!sessionHandler_.verifyConnection()) {
        DBGLOG(LOGHDR ".onIOStreamDisconnected(): Master Connection Failure!", ID);

        nBlockedExceptionThreads_.add_yield(+1);
        DBGLOG_VERBOSE(LOGHDR ".onIOStreamDisconnected(): Wait..%s", ID, exceptionCork_ ? "." : "");

        auto start = chrono::steady_clock::now();
        while (exceptionCork_) {
            this_thread::sleep_for(chrono::milliseconds(5));

            // timeout
            auto duration = chrono::duration_cast<chrono::duration<double>>(chrono::steady_clock::now() - start);
            if (duration.count() > 5.0) {
                break;
            }
        }

        nBlockedExceptionThreads_.add_yield(-1);
    }
    else {
        DBGLOG(LOGHDR ".onIOStreamDisconnected(): Master Connection Ok", ID);
    }
 
   DBGLOG(LOGHDR ".onIOStreamDisconnected(): FINISHED", ID);
   return retval;
}


void TickDbImpl::flushExceptions()
{
    unsigned n = nBlockedExceptionThreads_;
    exceptionCork_ = false;
    DBGLOG_VERBOSE(LOGHDR ".flushExceptions():.. cork = %d", ID, (int)exceptionCork_);
    if (0 != n) {
        DBGLOG(LOGHDR ": flushing %u exceptions", ID, n);
        // Make sure none of the threads are still blocked
        do {
            this_thread::sleep_for(chrono::milliseconds(5));
        } while (0 != nBlockedExceptionThreads_);
    }
}


bool TickDbImpl::onIOStreamDisconnectedCallback(IOStream *sender, TickDbImpl * self, const char * string)
{
    return self->onIOStreamDisconnected(sender, string);
}


namespace DxApiImpl {

    struct SubscrString : public string {
        SubscrString(const void * p, size_t n) : notEmpty_(false)
        {
            if (NULL == p) {
                assign("<ALL>");
            }
            else if (0 == n) {
                assign("<NONE>");
            }
        }

        template<typename T> string & append(T x) { auto notEmpty = notEmpty_; notEmpty_ = true; return(notEmpty ? string::append(", ") : *(string*)this).append(x); }

    protected:
        bool notEmpty_;
    };


    std::string subscriptionToString(const std::string s[], size_t n)
    {
        SubscrString out(s, n);
        forn(i, n) {
            out.append(s[i]);
        }

        return out;
    }


    std::string subscriptionToString(const TickStream * const s[], size_t n)
    {
        SubscrString out(s, n);
        forn(i, n) {
            out.append(s[i]->key());
        }

        return out;
    }


    //std::string subscriptionToString(const InstrumentIdentity s[], size_t n)
    //{
    //    SubscrString out(s, n);
    //    forn(i, n) {
    //        out.append(s[i].toString());
    //    }

    //    return out;
    //}

   /* template<typename T> std::string subscriptionToString(const std::vector<T> * x)
    {
        if (NULL == x) {
            return toString((T *)NULL, (size_t)0);
        }

        size_t l = x->size();
        return 0 == l ? toString((const T *)0x64, l) : toString(x->data(), l);
    }

    template std::string subscriptionToString(const std::vector<DxApi::InstrumentIdentity> * x);
    template<> std::string toString(const std::vector<DxApi::TickStream *> * x);
    template std::string toString(const std::vector<std::string> * x);*/
}

