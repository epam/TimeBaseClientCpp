#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <chrono>
#include <unordered_set>

//#include "util/cstr.h"
#include "http/tickdb_http.h"
#include "session_handler.h"
#include "tickstream_impl.h"
#include "http/xml/xml_request.h"
#include "http/xml/load_streams_request.h"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace dx_thread;


//#define GET_STREAMS_TIMEOUT_NS UINT64_C(1000000000)
#define GET_STREAMS_TIMEOUT_NS INT64_C(100000000000)
#define IDLE_UNLOCK_TIMEOUT_NS INT64_C(100000000)

// Time spent in one read attempt when waiting for the next event from the server
#define SH_READ_TIMEOUT_US 5000

// Yield thread after this delay
#define IDLE_YIELD_DELAY_NS INT64_C(10000000)

// Yield thread after this delay
#define PING_PERIOD_NS INT64_C(5000000000)

#define SESSION_STOPPING_WAIT_MAX_MS 10000
#define SESSION_STOPPING_WAIT_MIN_MS 2


#if VERBOSE_TICKDB_MANAGER_THREAD >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

#if VERBOSE_TICKDB_MANAGER_THREAD >= 2
#else
#endif


#define BASIC_WRITER_BUFFER_SIZE 0x11000

#define LOGHDR "%s.sessionHandler"
#define ID (this->db_.textId())


#define READ_LOCK srw_read lock(thisLock_);
#define WRITE_LOCK srw_write lock(thisLock_);


#define DUMMY_STREAM_NAME "53yregerlatnwg3rtxmewhfxswdrf"

static bool parseStreamDef(LoadStreamsRequest::Response &streams, XmlInput &data);


//template <typename T> NOINLINE static cstr to_cstr(const T &x) { return cstr(toString(x)); }

// Returns true if this instance of the manager is running
INLINE bool SessionHandler::isRunning() const
{
    return isRunning_;
}


INLINE TickStreamCache& SessionHandler::cache()
{
    return db_.streamCache_;
}


void SessionHandler::stop()
{
    bool stopped;
    DBGLOG_VERBOSE(LOGHDR ".sessionHandler.stop(): STARTED", ID);
    shouldStop_ = true;

    // Thread proc. will stop and delete itself
    while (true) {
        {
            yield_lock section(threadObjSection_);
            stopped = !isRunning() && NULL == thread_;
        }

        if (stopped) {
            DBGLOG_VERBOSE(LOGHDR ".sessionHandler.stop(): Thread is finished", ID);
            break;
        }

        this_thread::yield();
    }

    if (isRunning() != (NULL != thread_)) {
        DBGLOG(LOGHDR ".sessionHandler.stop(): ERROR: isRunning() is %d, but thread is %p !!", ID, (int)isRunning(), thread_);
    }

    DBGLOG_VERBOSE(LOGHDR ".sessionHandler.stop(): FINISHED", ID);
}


SessionHandler::SessionHandler(TickDbImpl &db) : db_(db), thread_(NULL), isRunning_(false), isStopped_(false), shouldStop_(false), interrupted_(false),
    nStreamDefRequestsSent_(0), nPropertyRequestsSent_(0),
    nStreamDefRequestsReceived_(0), nPropertyRequestsReceived_(0), 
    bufferAllocPtr_(NULL), ioStream_(NULL)
{
}


SessionHandler::~SessionHandler()
{
    DBGLOG(LOGHDR ".~sessionHandler(): destructor started", ID);

    if (NULL != thread_) {
        DBGLOG(".~sessionHandler(): ERROR: thread is still not NULL! Detaching thread.");
        //thread_->detach();
        delete thread_;
    }

    DBGLOG(LOGHDR ".~sessionHandler(): deleted", ID);
}


void SessionHandler::dbgBreakConnection()
{
    yield_lock section(writerSection_);
    if (NULL != ioStream_) {
        ioStream_->close();
    }
}


bool SessionHandler::verifyConnection()
{
    {
        yield_lock section(writerSection_);
        if (NULL == ioStream_)
            return false;
    }

    auto tmp = nStreamDefRequestsReceived_;
    volatile auto requestId = requestGetDummyStream();

    forn(i, 5) {
        this_thread::sleep_for(chrono::milliseconds(2));
        if (nStreamDefRequestsReceived_ != tmp) {
            return true;
        }
    }

    // If socket still present, assume the connection is still active
    yield_lock section(writerSection_);
    return NULL != ioStream_;
}


void SessionHandler::threadProcStatic(SessionHandler &self)
{
    try {
        self.threadProc();
    }
    catch (...) {
        dx_assert(!"Uncaught exception in SessionHandler threadproc!!!", "ERR: Uncaught exception in SessionHandler threadproc!!!");
    }
}


SessionHandler::Result SessionHandler::receive_STREAM_DELETED()
{
    string key;
    readUTF8(key);

    DBGLOG_VERBOSE(LOGHDR ": '%s' deleted", ID, key.c_str());
    db_.onStreamDeleted(key);

    return OK;
}


SessionHandler::Result SessionHandler::receive_STREAM_CREATED()
{
    string key;
    readUTF8(key);

    DBGLOG_VERBOSE(LOGHDR ": '%s' created", ID, key.c_str());
    neededStreamDefs.add(key);

    return OK;
}


SessionHandler::Result SessionHandler::receive_STREAM_RENAMED()
{
    string oldKey, newKey;
    readUTF8(oldKey);
    readUTF8(newKey);

    DBGLOG_VERBOSE(LOGHDR ": '%s' renamed to '%s'", ID, oldKey.c_str(), newKey.c_str());
    db_.onStreamDeleted(oldKey);
    neededStreamDefs.add(newKey);

    return OK;
}


SessionHandler::Result SessionHandler::receive_STREAMS_DEFINITION()
{
    uint64 requestId = -1;
    try {
        LoadStreamsRequest::Response newStreams;
        requestId = read<uint64>();
        size_t sz = read<int32_t>();

        XmlInput xml;
        LoadStreamsRequest req(this->db_);

        if (0 == sz)
            goto nothing;

        if (sz > INT32_MAX) {
            THROW_DBGLOG(LOGHDR ".receive_STREAMS_DEFINITION(): ERR: XML size is too big : %lld", ID, (longlong)sz);
        }

        
        xml.resize(sz);
        ioStream_->read(&xml[0], sz, sz);

#ifdef _DEBUG
        DBGLOG_VERBOSE(LOGHDR ".receive_STREAMS_DEFINITION(): finished reading stream metadata: %lld, %08X", ID, (longlong)sz, (unsigned)crc32(xml));
#else
        DBGLOG_VERBOSE(LOGHDR ".receive_STREAMS_DEFINITION(): finished reading stream metadata: %lldb", ID, (longlong)sz);
#endif

        //printf("%d\n%s\n", (unsigned)sz, xml.c_str());

        
        if (!req.getStreams(newStreams, xml)) {
            THROW_DBGLOG(LOGHDR ".receive_STREAMS_DEFINITION(): ERR: unable to parse XML", ID);
        }
        else {
            DBGLOG_VERBOSE(LOGHDR ".receive_STREAMS_DEFINITION(): parsed stream metadata", ID);
        }

        for (auto &i : newStreams) {
            auto s = cache().get(i.key);

            if (NULL == s) {
                db_.addStream(db_.allocateStream(i.key, i.options));
            }
            else {
                impl(s)->updateOptions(i.options);
            }
        }

        // Mark those streams as recently received
        for (auto &i : newStreams) {
            knownStreamDefs.add(i.key);
        }

nothing:
        if (++nStreamDefRequestsReceived_ != requestId) {
            DBGLOG(LOGHDR ".receive_STREAMS_DEFINITION(): ERR: #%llu received, #%llu expected!!!!!",
                ID, (ulonglong)requestId, (ulonglong)nStreamDefRequestsReceived_);
        }
        else {
            if (0 == sz) {
                DBGLOG(LOGHDR ".receive_STREAMS_DEFINITION(): _empty_ #%llu received", ID, (ulonglong)nStreamDefRequestsReceived_);
            }
            else {
                DBGLOG_VERBOSE(LOGHDR ".receive_STREAMS_DEFINITION(): #%llu received", ID, (ulonglong)nStreamDefRequestsReceived_);
            }
        }

        return OK;
    }
    catch (...) {
        if (++nStreamDefRequestsReceived_ != requestId) {
            DBGLOG(LOGHDR ".receive_STREAMS_DEFINITION(): ERR: #%llu received, #%llu expected!!!!!",
                ID, (ulonglong)requestId, (ulonglong)nStreamDefRequestsReceived_);
        }
        else {
            DBGLOG_VERBOSE(LOGHDR ".receive_STREAMS_DEFINITION(): #%llu received", ID, (ulonglong)nStreamDefRequestsReceived_);
        }

        throw;
    }
}


SessionHandler::Result SessionHandler::receive_STREAM_PROPERTY_CHANGED()
{
    string key;
    readUTF8(key);
    auto p = readPropertyId();

    DBGLOG_VERBOSE(LOGHDR ": %s.%s changed, retrieving..", ID, key.c_str(), p.toString());
    
    // Current implementation is greedy, it requests all the changes

    requestGetStream(key); // Temporarily we will request the whole stream definition instead of a specific property
    
    //requestGetProperty(key, p);

    return OK;
}


SessionHandler::Result SessionHandler::receive_STREAM_PROPERTY()
{
    string key, param;
    uint64 requestId = -1;

    try {
        requestId = read<uint64>();
        readUTF8(key);
        auto p = readPropertyId(); // may throw?

        //TickStreamPropertyMask mask = 0;
        StreamOptions opt;

        auto streamExt = cache().get(key);
        auto stream = impl(streamExt);
        if (NULL == streamExt) {
            DBGLOG(LOGHDR ".receive_STREAM_PROPERTY(%s): ERROR: stream '%s' not found in cache!", ID, p.toString(), key.c_str());
            stream = NULL; // Just to be safe, already NULL for the curent implementation
        }
        else {
            opt = stream->options();
        }

        puts("WARNING: STREAM_PROPERTY event in unexpected at this time! ");
        DBGLOG(LOGHDR ".receive_STREAM_PROPERTY(%s.%s): ERR: not supported (protocol update is pending)!", ID, key.c_str(), p.toString());

        // 8 properties currently sent by protocol
        switch (p) {
        case TickStreamPropertyId::UNKNOWN:
            goto def;

#define CASE(X) break; case TickStreamPropertyId::X:

            CASE(NAME) {
                readUTF8(param);
                opt.name = param;
            }

            CASE(DESCRIPTION) {
                readUTF8(param);
                opt.description = param;
            }

            CASE(PERIODICITY) {
                readUTF8(param);
                opt.periodicity = param;
            }

            CASE(SCHEMA)  {
                // TODO:
                bool isPolymorphic = 0 != read<byte>();
                // TODO: How length is encoded?
                // NOTE: This does not work, XML is written without length (verified)
                readUTF8(param);
            }

            CASE(ENTITIES) {
                // TODO:
                // ??????
            }

            CASE(TIME_RANGE) {
                // TODO:
                // ??????
            }

            CASE(VERSIONING) {
                // TODO:
                // ??????
            }

            CASE(KEY) {
                readUTF8(param);
                // TODO:
            }

            CASE(BG_PROCESS) {
                // TODO: 
            }

            CASE(OWNER)  {
                readUTF8(param);
                opt.owner = param;
            }

            // These are not currently sent
#if 0
            CASE(HIGH_AVAILABILITY) {
                // ??
            }

            CASE(UNIQUE) {
                // ??
            }

            CASE(BUFFER_OPTIONS) {
                // ??
            }

            CASE(DATA_VERSION) {
                // ??
            }

            CASE(REPLICA_VERSION) {
                // ??
            }

            CASE(WRITER_CREATED) {
                // ???
            }

            CASE(WRITER_CLOSED) {
                // ???
            }

            CASE(SCOPE) {
                // ??
            }

            CASE(DF) {
                // ??
            }

#endif
            break;

        def:
        default:
            ;
        }

        if (++nPropertyRequestsReceived_ != requestId) {
            DBGLOG(LOGHDR ".receive_STREAM_PROPERTY(): ERR: #%llu received, #%llu expected!!!!!",
                ID, (ulonglong)requestId, (ulonglong)nPropertyRequestsReceived_);
        }
        else {
            DBGLOG_VERBOSE(LOGHDR ".receive_STREAM_PROPERTY(): #%llu received", ID, (ulonglong)requestId);
        }
    }
    catch (...) {
        if (++nPropertyRequestsReceived_ != requestId) {
            DBGLOG(LOGHDR ".receive_STREAM_PROPERTY(): ERR: #%llu received, #%llu expected!!!!!",
                ID, (ulonglong)requestId, (ulonglong)nPropertyRequestsReceived_);
        }
        else {
            DBGLOG_VERBOSE(LOGHDR ".receive_STREAM_PROPERTY(): #%llu received", ID, (ulonglong)requestId);
        }
    }

    return OK;
}


SessionHandler::Result SessionHandler::receive_SESSION_CLOSED()
{
    // Nothing
    return FINISHED;
}


SessionHandler::Result SessionHandler::receive_STREAMS_CHANGED()
{
    requestGetStreams();
    return OK;
}


SessionHandler::Result SessionHandler::processInput()
{
    auto ioStream = ioStream_;
    if (interrupted_ || NULL == ioStream) {
        return FINISHED;
    }

    try {
        byte buf[0x80];
        auto nRead = ioStream->tryRead(buf, sizeof(int32_t), SH_READ_TIMEOUT_US);

        if (0 != nRead) {
            assert(nRead <= sizeof(int32_t));
            if (nRead < sizeof(int32_t)) {
                ioStream->read(buf + nRead, sizeof(int32_t) - nRead, sizeof(int32_t) - nRead);
            }

            unsigned command = _loadBE<uint32_t>(buf);

            switch (command) {
#define CMD(X) case X: \
DBGLOG(LOGHDR ".processInput(" #X ")", ID); \
    return receive_##X();

            CMD(STREAM_PROPERTY_CHANGED)

            CMD(STREAM_DELETED)

            CMD(STREAM_CREATED)

            CMD(STREAM_RENAMED)

            CMD(STREAMS_DEFINITION)

            CMD(STREAM_PROPERTY)

            CMD(SESSION_CLOSED)

            CMD(STREAMS_CHANGED)

            default:
                format_string(&errorText_, "Unexpected token received from the server: %08X", command);
                DBGLOG(LOGHDR ".processResponse(..): unrecognized token: %08X", ID, command);
                cleanup();
                return FINISHED;
            }

            return OK;
        }

        return NO_DATA;
    }
#undef CMD

    catch (const IOStreamDisconnectException &) {
        DBGLOG(LOGHDR ".processInput(): WARNING: disconnected from the server side", ID);
        format_string(&errorText_, "disconnected by server");
        closeConnection();
        return FINISHED;
    }

    catch (const IOStreamException &e) {
        DBGLOG(LOGHDR ".processInput(): WARNING: Other socket exception", ID);
        format_string(&errorText_, "IOStream exception: %s", e.what());
        closeConnection();
        return FINISHED;
    }

    catch (const std::runtime_error &e) {
        DBGLOG(LOGHDR ".processInput(): ERR: Runtime error caught, ignored: %s", ID, e.what());
        //format_string(&errorText_, "std::exception: %s", e.what());
        return OK;
    }

    catch (const std::exception &e) {
        DBGLOG(LOGHDR ".processInput(): Exception: %s", ID, e.what());
        format_string(&errorText_, "std::exception: %s", e.what());
        return FINISHED;
    }

    catch (...) {
        DBGLOG(LOGHDR ".processInput(): Unknown exception", ID);
        format_string(&errorText_, "Unknown exception");
        return FINISHED;
    }
}


void SessionHandler::threadProcImpl()
{
    DBGLOG(LOGHDR ": sessionHandler started!", ID);

    static_assert(PING_PERIOD_NS > IDLE_YIELD_DELAY_NS, "Ping period must be greater than thread yield/sleep period");

    auto lastDataTimestamp = (int64_t)time_ns();
    auto lastLoggedTimestamp = lastDataTimestamp;
    auto lastPingTimestamp = lastDataTimestamp;
    int64_t logPeriod = INT64_C(5000000000);
    int64_t pingPeriod = PING_PERIOD_NS;

    while (!shouldStop_) {
        auto result = processInput();
        if (FINISHED == result)
            break;

        int64 t = time_ns();
        if (OK == result) {
            lastPingTimestamp = lastDataTimestamp = lastLoggedTimestamp = time_ns();
        }
        else {
            /*if (t - lastDataTimestamp > IDLE_UNLOCK_TIMEOUT_NS && knownStreamDefs.size() != 0) {
            knownStreamDefs.clear();
            }*/

            if (t - lastDataTimestamp > IDLE_YIELD_DELAY_NS) {
                if (t - lastLoggedTimestamp > logPeriod) {
                    lastLoggedTimestamp += logPeriod;
                    DBGLOG_VERBOSE(LOGHDR ": idle", ID);
                }

                // Ping disabled for now
#if 0
                if (t - lastPingTimestamp > pingPeriod) {
                    lastPingTimestamp += pingPeriod;
                    requestGetStream("__PING_u5u7clwetyjui__");
                }
#endif

                this_thread::yield();
            }

        }

        if (0 != neededStreamDefs.size()) {
            concurrent_obj_set_iter<string> iter(neededStreamDefs);
            string key;
            vector<string> keys;
            string logStr;
            DBGLOG_VERBOSE(LOGHDR ": %u stream(s) need to retrieve content", ID, (unsigned)neededStreamDefs.size());

            while (iter.next(key)) {
                keys.push_back(key);
                logStr.append(key).append(" ");
                neededStreamDefs.remove(key);
            }

            DBGLOG(LOGHDR ": requesting stream contents for %u streams: %s", ID, (unsigned)keys.size(), logStr.c_str());
            requestGetStreams(keys);
        }
    }

    DBGLOG_VERBOSE(LOGHDR ": sessionHandler read loop stopped", ID);
    close();
    DBGLOG(LOGHDR ": sessionHandler thread finished!", ID);
}


void SessionHandler::threadProcExceptionWrapper()
{
    try {
        threadProcImpl();
        //dx_thread::atomic_exchange(isStopped_, true);
    }
    catch (const std::exception &e) {
        DBGLOG(LOGHDR ".threadProc(): ERR: Exception caught: %s", ID, e.what());
    }
    catch (...) {
        DBGLOG(LOGHDR ".threadProc(): ERR: System exception caught!", ID);
    }
}


void SessionHandler::threadProc()
{
    isRunning_ = true;
    threadProcExceptionWrapper();
    stopped();
}



void SessionHandler::stopped()
{
    DBGLOG_VERBOSE(LOGHDR ".stopped(): STARTED", ID);
    // Wait for disconnection
    db_.onSessionStopping();

    int i = SESSION_STOPPING_WAIT_MAX_MS;
    // TODO: Temporary change
    /*for (; !db_.allCursorsAndLoadersDisconnected() && i > 0; i -= SESSION_STOPPING_WAIT_MIN_MS) {
        this_thread::sleep_for(chrono::milliseconds(SESSION_STOPPING_WAIT_MIN_MS));
    }*/

    if (i <= 0) {
        DBGLOG(LOGHDR ".stopped(): WRN: Timeout!", ID);
    }

    char myid[800];
    
    assert(strlen(this->db_.textId()) < sizeof(myid));
    strcpy(myid, this->db_.textId());

    {
        yield_lock section(this->threadObjSection_);

        auto thread = this->thread_;
        this->thread_ = NULL;
        if (NULL != thread) {
            thread->detach();
            delete thread;
        }

        isStopped_ = true;
        isRunning_ = false;
    }

    // When event handler is started, this thread is already detached, marked as stopped and its object is destroyed
    // Event is only called when the thread finished by itself, without stop() call
    if (!shouldStop_) {
        db_.onDisconnected();
    }
    DBGLOG_VERBOSE(LOGHDR ".stopped(): flush exceptions... cork = %d", myid, (int)db_.exceptionCork_);
    db_.flushExceptions();

    DBGLOG_VERBOSE(LOGHDR ".stopped(): FINISHED", myid);
}


template<typename T> T SessionHandler::read()
{
    byte buf[sizeof(T)];
    uintptr length;
    length = ioStream_->read((char*)buf, sizeof(T), sizeof(T));
    return _loadBE<T>(buf);
}


TickStreamPropertyId SessionHandler::readPropertyId()
{
    byte p = read<byte>();
    return TickStreamPropertyId((TickStreamPropertyId::Enum)p);
}


bool SessionHandler::readUTF8(std::string &s)
{
    uint16 len = read<uint16>();
    s.clear();
    if (0xFFFF != len) {
        s.resize(len);
        if (len > 0) {
            ioStream_->read(&s[0], len, len);
        }

        return true;
    }

    return false;
}


void SessionHandler::closeConnection()
{
    if (NULL != ioStream_) {
        DBGLOG_VERBOSE(LOGHDR ": closing previously open transport connection", ID);
        ioStream_->close();
        delete ioStream_;
        ioStream_ = NULL;
    }
}

void SessionHandler::cleanup()
{
    closeConnection();
    if (NULL != bufferAllocPtr_) {
        ::free(bufferAllocPtr_);
    }

    neededStreamDefs.clear();
    knownStreamDefs.clear();
   // To unlock threads that possibly waits for this
}


INLINE void SessionHandler::flushOutput()
{
    if (0 != writer_.dataSize()) {
        if (NULL != ioStream_) {
            ioStream_->write(writer_.dataStart(), writer_.dataSize());
        }

        writer_.resetPointer();
    }
}


// Internal method called after stopping the thread
void SessionHandler::close()
{
    DBGLOG_VERBOSE(LOGHDR ".close()", ID);
    try {
        // Using requestClose() leads to problems, and the server works well without it.
#if 0
        requestClose();

        int i = SESSION_STOPPING_WAIT_MAX_MS;
        for (; 0 != i; i -= SESSION_STOPPING_WAIT_MIN_MS) {
            this_thread::sleep_for(chrono::milliseconds(SESSION_STOPPING_WAIT_MIN_MS));
        }

        DBGLOG(LOGHDR ".close(): WRN: Timeout", ID);
#endif
        cleanup();
    }
    catch (IOStreamDisconnectException &) {
        cleanup();
    }
    catch (...) {
        DBGLOG_VERBOSE(LOGHDR ".close(): ERR: EXCEPTION", ID);
        cleanup();
        throw;
    }
}


uint64_t SessionHandler::requestGetStreams()
{
    vector<string> dummy;
    return requestGetStreams(dummy);
}


uint64_t SessionHandler::requestGetStreams(const std::vector<std::string> &keys)
{
    assert((uint32_t)keys.size() <= INT32_MAX);
    DBGLOG_VERBOSE(LOGHDR ".requestGetStreams#%llu()", ID, (ulonglong)nStreamDefRequestsSent_ + 1);
    yield_lock section(writerSection_);
    writer_.writeUInt32(REQ_GET_STREAMS);
    writer_.writeUInt64(++nStreamDefRequestsSent_);
    writer_.writeUInt32((uint32_t)keys.size());

    for (auto &i : keys) {
        if (writer_.remaining() <= i.size() + 4) {
            flushOutput();
        }

        writer_.writeUTF8(i);
    }

    flushOutput();
    DBGLOG_VERBOSE(LOGHDR ".requestGetStreams#%llu(): sent for streams[%u]: %s",
        ID, (ulonglong)nStreamDefRequestsSent_, (unsigned)keys.size(), toString(keys).c_str());

    return nStreamDefRequestsSent_;
}


uint64_t SessionHandler::requestGetProperty(const std::string &key, TickStreamPropertyId id)
{
    DBGLOG_VERBOSE(LOGHDR ".requestGetProperty()", ID);
    {
        yield_lock section(writerSection_);
        writer_.writeUInt32(REQ_GET_STREAM_PROPERTY);
        writer_.writeUInt64(++nPropertyRequestsSent_);
        writer_.writeUTF8(key);
        writer_.writeByte(id);

        flushOutput();
        DBGLOG_VERBOSE(LOGHDR ".requestGetProperty#%llu(%s.%s): sent",
            ID, (ulonglong)nStreamDefRequestsSent_, key.c_str(), id.toString());

        return nPropertyRequestsSent_;
    }
}


void SessionHandler::requestClose()
{
    DBGLOG_VERBOSE(LOGHDR ".requestClose()", ID);
    {
        yield_lock section(writerSection_);
        writer_.writeUInt32(REQ_CLOSE_SESSION);
        flushOutput();
    }
}


void SessionHandler::start()
{
    try {
        // Allocate buffer (currently using constant size)
        byte * allocPtr = (byte *)malloc(BASIC_WRITER_BUFFER_SIZE + 0x1000);
        
        if (NULL == allocPtr) {
            DBGLOGERR((std::string*)NULL, LOGHDR ".open(): Not enough memory for session handler output buffer", ID);
            throw std::bad_alloc();
        }

        bufferAllocPtr_ = allocPtr;

        // Align to 4k page boundary and add 8-byte front guard area
        byte * bufPtr = (byte *)DX_POINTER_ALIGN_UP(allocPtr, 0x1000);
        assert(NULL != bufPtr);

        writer_.setBuffer(allocPtr, bufPtr, bufPtr + BASIC_WRITER_BUFFER_SIZE);

        IOStream * ioStream = db_.createConnectionStream(TCP::NO_DELAY, false);
        this->ioStream_ = ioStream;

        auto & o = writer_;
        o.resetPointer();

        DBGLOG_VERBOSE(LOGHDR ".open(): Opening Timebase session..", ID);

        // TODO: refactor common code
        int remoteVersion;
        o.writeByte(PROTOCOL_INIT);
        o.writeInt16(PROTOCOL_VERSION);
        flushOutput();

        remoteVersion = read<int16>();
        if (remoteVersion != PROTOCOL_VERSION) {
            THROW_DBGLOG(LOGHDR ".open(): Incompatible HTTP-TB protocol version %d. Expected version is %d", ID, remoteVersion, PROTOCOL_VERSION);
        }


        // Transmit credentials
        {
            auto &db = db_;
            secure_string data = db.username();
            scramble(data);
            if (!data.size()) {
                o.writeByte(false);
            }
            else {
                o.writeByte(true);
                o.writeUTF8(data);
                data = db.encodedPass();
                scramble(data);
                o.writeUTF8(data);
            }
        }

        flushOutput();
        int handshake = read<int32>();

        if (RESP_ERROR == handshake) {
            closeConnection();
            // No need to remove from loader, because it is not yet added
            std::string out;
            DBGLOGERR(&out, LOGHDR ".open(): Handshake failed: Wrong username or password", ID);
            throw XmlErrorResponseException(401, out);
        }

        DBGLOG_VERBOSE(LOGHDR ".open(): Handshake suceeded. Initializing session, reading session id", ID);

        o.writeByte(REQ_CREATE_SESSION);
        o.writeByte(0);                     // useCompression = false
        o.writeByte(1);                     // isBigEndian = true (requesting LE stream won't give us noticeable perf. improvement)
        flushOutput();

        // A little hack to force the server to confirm it is working properly by sending a message right away
        requestGetDummyStream();

        // But expect session Id first. If the server is too old, it will send reply to the stream request instead, causing exception
        if (SESSION_STARTED != read<int32_t>()) {
            // TODO: Change exception type?
            THROW_DBGLOG(LOGHDR ".open(): ERR: SESSION_STARTED marker is expected!", ID);
        }

        if (!readUTF8(db_.sessionId_) || 0 == db_.sessionId_.length()) {
            THROW_DBGLOG(LOGHDR ".open(): ERR: Session Id string is expected!", ID);
        }

        DBGLOG_VERBOSE(LOGHDR ".open(): Session id read. Starting worker thread", ID);
        
        // Ok, start worker thread
        thread_ = new std::thread(threadProcStatic, std::ref(*this));
        while (!isRunning_ && !isStopped_) {
            this_thread::yield();
        }
    }

    catch (const IOStreamDisconnectException &) {
        // If was disconnected while still reading response from server, return error
        // If just empty response, set isAtEnd (TODO: KLUDGE: not compatible with realtime mode?)
        DBGLOG(LOGHDR ".open(): ERR: disconnected while connecting", ID);
        cleanup();
        throw;
    }

    catch (...) {
        cleanup();
        throw;
    }
}


INLINE bool SessionHandler::propertyRequestCompleted(uint64_t requestId)
{
    return (int64_t)(nPropertyRequestsReceived_ - requestId) >= 0;
}


INLINE bool SessionHandler::streamDefRequestCompleted(uint64_t requestId)
{
    return (int64_t)(nStreamDefRequestsReceived_ - requestId) >= 0;
}


INLINE bool SessionHandler::isWorking()
{
    return isRunning_ && !(shouldStop_ | /* Yes, | */ interrupted_);
}


uint64_t SessionHandler::requestGetStream(const std::string &key)
{
    vector<string> dummy;
    dummy.emplace_back(key);
    return requestGetStreams(dummy);
}


void SessionHandler::getStreamsSynchronous()
{
    vector<string> dummy;
    return getStreamsSynchronous(dummy);
}


void SessionHandler::getStreamSynchronous(const char * key)
{
    return getStreamSynchronous(string(key));
}


void SessionHandler::getStreamSynchronous(const std::string &key)
{
    vector<string> dummy;
    dummy.emplace_back(key);

    return getStreamsSynchronous(dummy);
}


void SessionHandler::getStreams(const vector<string> &keys)
{
    if (isWorking()) {
        uint64_t requestId = requestGetStreams(keys);
    }
}


uint64_t SessionHandler::requestGetDummyStream()
{
    vector<string> dummy;
    char buffer[0x80];

    snprintf(buffer, sizeof(buffer), "_dummy_fxrsttydvs_%04x%04x", rand(), rand());
    dummy.emplace_back(buffer);

    return requestGetStreams(dummy);
}


void SessionHandler::getStreamsSynchronous(const std::vector<std::string> &keys)
{
    if (isWorking()) {
        concurrent_obj_set<string> requestedLocal; // TODO: No need for concurrency actually. Will correct later

        if (0 == keys.size()) {
            knownStreamDefs.clear();
        }
        else {
            for (auto &i : keys) {
                knownStreamDefs.remove(i);
                requestedLocal.add(i);
            }
        }

        uint64_t requestId = requestGetStreams(keys);

        DBGLOG_VERBOSE(LOGHDR ".getStreamsSync(): waiting for #%llu", ID, (ulonglong)requestId);
        while (!streamDefRequestCompleted(requestId)) {
            this_thread::yield();
        }

        DBGLOG_VERBOSE(LOGHDR ".getStreamsSync(): wait for #%llu done", ID, (ulonglong)requestId);

#if 0
        // Wait till all requested streams are removed from the request queue.
        // TODO: race condition: someone may re-add those streams later and this is beyond our control
        auto start = time_ns();
        while (time_ns() - start < GET_STREAMS_TIMEOUT_NS) {
            if (0 == keys.size()) {
                if (0 != knownStreamDefs.size()) {
                    break;
                    // TODO: This wont work well with empty timebase.
                }
            }
            else {
                for (auto &i : keys) {
                    if (NULL != requestedLocal.find(i)) {
                        if (NULL != knownStreamDefs.find(i)) {
                            requestedLocal.remove(i);
                        }
                    }
                }

                if (0 == requestedLocal.size())
                    break;
            }

            this_thread::yield();
        }
#endif
    }
}


// Force update of a property. Should be used in tests at this moment.
uint64_t SessionHandler::getProperty(const std::string &key, TickStreamPropertyId id)
{
    if (isWorking()) {
        return requestGetProperty(key, id);
    }

    return -1;
}


void SessionHandler::getPropertySynchronous(const std::string &key, TickStreamPropertyId id)
{
    if (isWorking()) {
        uint64_t requestId = requestGetProperty(key, id);

        DBGLOG_VERBOSE(LOGHDR ".getPropertySync(): waiting for #%llu", ID, (ulonglong)requestId);
        while (!propertyRequestCompleted(requestId)) {
            this_thread::yield();
        }

        DBGLOG_VERBOSE(LOGHDR ".getPropertySync(): wait for #%llu done", ID, (ulonglong)requestId);
    }
}


