#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>

#include "io/output_preloader.h"
#include "xml/stream/schema_request.h"

#include "tickdb_http.h"
#include "tickloader_http.h"


using namespace dx_thread;
using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace DxApiImpl::TickLoaderNs;



#define THIS_IMPL (impl(this))

#define LOGHDR "%s"
#define ID (this->textId())

#if VERBOSE_LOADER >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

#if VERBOSE_LOADER >= 2
#define DBGLOG_MSG DBGLOG
#define DBG_DUMP dbg_dump
#else
#define DBGLOG_MSG (void)
#define DBG_DUMP (void)
#endif


const char * infoWriteMode[] = {
    "APPEND",     /* Adds only new data into a stream without truncation */
    "REPLACE",    /* Adds data into a stream and removes previous data older that new time */
    "REWRITE",    /* Adds data into a stream and removes previous data by truncating using first new message time */
    "TRUNCATE"    /* Stream truncated every time when loader writes messages earlier that last */
};

IMPLEMENT_ENUM(uint8_t, WriteMode, false)
{}


const char * infoInterruption[] = {
    "WORKING",
    "STOPPING",
    "CLIENT_FINISHED",
    "SERVER_FINISHED_OK",
    "ERROR",
    "SERVER_FINISHED_ERROR",
    "SERVER_DISCONNECTED",
    "IO_ERROR"
};

IMPLEMENT_ENUM(uint8_t, Interruption, false)
{}


//IMPLEMENT_CLASS_ENUM(uint8_t, TickLoaderImpl, Interruption, false)
//{}

const char * infoError[] = {
    "NO_ERROR",
    "ERROR",
    "HANDSHAKE_ERROR",
    "DISCONNECTION_ERROR", // A subset of IO error
    "IO_ERROR"    
};

IMPLEMENT_ENUM(uint8_t, Error, false)
{}

//IMPLEMENT_CLASS_ENUM(uint8_t, TickLoaderImpl, Error, false)
//{}


DataWriterInternal::DataWriterInternal() : bufferAllocPtr(0), bufferStart(0), depth(0)
{
    dataPtr = 0;
}



byte * DataWriterInternal::onOverflow()
{
    THROW_DBGLOG("TickLoader: Buffer overflow while writing a message with size: %llu", (ulonglong)dataSize());
}


uint8_t * DataWriter::onOverflow()
{
    return impl(this)->onOverflow();
}


TickLoader::TickLoader()
{
    // Do Absolutely Nothing
    static_assert(1 == sizeof(*this), "API object size should be 1(actually 0)");
}


TickLoaderImpl::TickLoaderImpl(TickDbImpl &dbRef, uint64_t uid) :
    dbgProcessingServerResponse_(false), db_(dbRef), manager_(dbRef.loaderManager()), state_(NOT_INITIALIZED),
    clientIsActive_(false), writePtr_(NULL), ioStream_(NULL), preloaderOutputStream_(NULL), minChunkSize_(0x2000), maxChunkSize_(0x8000),
    stream_(NULL), id_(uid), interruption_(Interruption::WORKING), writerFinishedSignal_(WRITER_FINISHED_UNSET)
{
    messageWriter_.dataPtr = NULL;
}


void TickLoader::operator delete(void * ptr, size_t sz)
{
    if (NULL != ptr) {
        if (sz == sizeof(TickLoader)) {
            delete impl((TickLoader *)ptr);
        }
        else {
            DBGLOG_VERBOSE("TickLoader . operator delete %u", (unsigned)sz);
            ::operator delete(ptr);
        }
    }
}


TickLoader::~TickLoader()
{
    // Delegated to implementation class via delete operator
    DBGLOG_VERBOSE(LOGHDR ".~TickLoader(): destructor is delegated to impl", impl(this)->textId());
}


TickLoaderImpl::~TickLoaderImpl()
{
    DBGLOG(LOGHDR ".~TickloaderImpl(): destructor started", impl(this)->textId());
    writerFinishedSignal_ = 0;
    if (isWriting()) {
        DBGLOG(LOGHDR ".~TickLoaderImpl(): ERROR: Still not closed, will attempt to close", ID);
    }

    close();
    DBGLOG(LOGHDR ".~TickloaderImpl(): deleted", impl(this)->textId());
}


INLINE void TickLoaderImpl::resetPointers()
{
    writePtr_ = messageWriter_.resetPointer();
}


// Unwinds message write pointers. Does not reset the DataWriter (it will be reset when a new message is started)
INLINE void TickLoaderImpl::cancelMessage()
{
    assert(MESSAGE_BODY == state_);
    assert(messageStart_ >= messageWriter_.bufferStart + sizeof(uint32));
    ++nMsgCancelled_;
    messageWriter_.dataPtr = messageStart_ - sizeof(uint32);
}


void TickLoaderImpl::flushOutputMessageSynchronized()
{
#if LOADER_FLUSH_TIMER_ENABLED
    while (beingFlushed_.test_and_set(memory_order_acq_rel)) {
        yield();
    }

    // Check again
    if (messageWriter_.dataPtr - messageWriter_.bufferStart >= 0) {
        flushOutputMsg();
    }

    flushable_.clear(memory_order_release);
    beingFlushed_.clear(memory_order_release);
#endif
}


INLINE void TickLoaderImpl::endMessage()
{
    assert(isWritingMessageBody());

    byte *messageStart = this->messageStart_;
    byte *dataPtr = messageWriter_.finishMessage(messageStart);

    // Calculate and store size of the entire message
    uintptr messageBodySize = (dataPtr - messageStart);
    DataWriterInternal::storeBE<uint32_t>(messageStart - sizeof(uint32), (uint32_t)messageBodySize);

#if VERBOSE_LOADER >= 1
    if (shouldLogMessages_) {
        DBG_DUMP(string(ID).append("-msg-").append(timestamp_ns2str(_loadBE<int64>(messageStart), false)).append("-").append(*tables_.getMessageTypeName(messageStart[sizeof(uint64) + sizeof(uint16)])).c_str(), messageStart - 4, messageBodySize + 4);
    }
#endif

    ++nMsgWritten_;
    // Flush, if necessary
    auto minChunkSize = (intptr)minChunkSize_;

    if (0 == minChunkSize) {
        // No concurrency expected for low-latency version
        flushOutputMsg();
    }
    else {
#if !LOADER_FLUSH_TIMER_ENABLED
        if (dataPtr - messageWriter_.bufferStart >= minChunkSize) {
            flushOutputMsg();
        }
#else
        if (dataPtr - messageWriter_.bufferStart >= minChunkSize) {
            flushOutputMessageSynchronized();
        }
        else {
            flushable_.test_and_set(memory_order_release);

        }
#endif
    }

    state_ = MESSAGE_START;
}


INLINE void TickLoaderImpl::writeMessageBlockEnd()
{
    messageWriter_.putUInt32(MESSAGE_BLOCK_TERMINATOR_RECORD);
}


INLINE void TickLoaderImpl::writeMessageBlockStart()
{
    messageWriter_.putByte(MESSAGE_BLOCK_ID);
}


INLINE void TickLoaderImpl::endMessageIfNeeded()
{
    if (isWritingMessageBody()) {
        endMessage();
    }
}


INLINE void TickLoaderImpl::startMessageBlockIfNeeded()
{
    if (!isWritingMessage()) {
        writeMessageBlockStart();
        state_ = MESSAGE_START;
    }
}


INLINE void TickLoaderImpl::endMessageBlockIfNeeded()
{
    if (isWritingMessage()) {
        if (isWritingMessageBody()) {
            endMessage();
        }
        state_ = NOT_IN_MESSAGE_BLOCK;
        writeMessageBlockEnd();
    }
}


void TickLoaderImpl::writeInstrumentBlock(const std::string &instrumentName)
{
    endMessageBlockIfNeeded();
    checkWriterInterruption();
    messageWriter_.writeByte(INSTRUMENT_BLOCK_ID);
    messageWriter_.writeUTF8(instrumentName);
}


void TickLoaderImpl::endWriting()
{
    endMessageBlockIfNeeded();
    flushOutputMsg();
    messageWriter_.writeByte(TERMINATOR_BLOCK_ID);
    state_ = FINISHED_WRITING;
}


INLINE void TickLoaderImpl::transmitBuffer()
{
    auto ios = ioStream_;
    if (NULL != ios) {
        ios->write(writePtr_, messageWriter_.dataPtr - writePtr_);
    }
}


bool TickLoaderImpl::getInterruptedException(TickLoaderInterruptedException & e)
{
    string err;
    Error::Enum error = error_.code;
    if (!error) {
        return false;
    }

    format_string(&err, "%s: Interrupted with error: %s:%s: %s", ID, Interruption(interruption_).toString(), Error(error).toString(),
        serverErrorText().c_str());

    DBGLOG_VERBOSE(LOGHDR ".onInterruption(): %s", ID, err.c_str());
    e = TickLoaderInterruptedException(err);
    return true;
}


NOINLINE void TickLoaderImpl::onInterruption()
{
    TickLoaderInterruptedException ex;
    if (getInterruptedException(ex)) {
        throw ex;
    }
    else {
        DBGLOG_VERBOSE(LOGHDR ": Writer interrupted succesfully", ID);
        throw TickLoaderClosedException();
    }
}


INLINE void TickLoaderImpl::checkWriterInterruption()
{
    if (interruption_ != Interruption::WORKING) {
        onInterruption();
    }
}


template<bool ISMSGDATA> INLINE void TickLoaderImpl::flushOutputInternal()
{
    assert(writePtr_ <= messageWriter_.dataPtr);
    // Set 
    clientIsActive_ = true;

#if VERBOSE_LOADER >= 1
    if (shouldLogDataBlocks_) {
        char ofs[80];
        snprintf(ofs, sizeof(ofs), "%08llX-%04llX", (ulonglong)ioStream_->nBytesWritten(), (ulonglong)(messageWriter_.dataPtr - writePtr_));
        DBG_DUMP(string(ID).append(ISMSGDATA ? "mblk" : "pblk").append(ofs).c_str(),
            writePtr_, messageWriter_.dataPtr - writePtr_);
    }
#endif

    // If we are not yet forcibly interrupted, write data 
    if (interruption_ <= Interruption::STOPPING) {
#if !LOADER_DONT_SEND
        transmitBuffer();
#else
        if (!ISMSGDATA) {
            transmitBuffer();
        }
#endif
    }

    resetPointers();
}


void TickLoaderImpl::flushOutput()
{
    flushOutputInternal<false>();
}


void TickLoaderImpl::flushOutputMsg()
{
    flushOutputInternal<true>();
}


// Open is always called immediately after creation
TickLoaderImpl * TickLoaderImpl::open(const TickStream * stream, const LoadingOptions &options)
{
    string response;
    clientIsActive_ = false;

    shouldLogDataBlocks_ = false;

#if VERBOSE_LOADER >= 2
    shouldLogMessages_ = true;
#else
    shouldLogMessages_ = false;
#endif

// Debug code
#if 0 && defined(_DEBUG)
    static const char * logged_streams[] = {
        "STS#SYS#System#",
    };

    for (auto i : logged_streams) if (NULL != strstr(stream->key().c_str(), i)) {
        printf("!!!!!!!!!!!!!!!!!!!!: Loader open for %s\n", stream->key().c_str());
        shouldLogMessages_ = true;
    }
    else {
        shouldLogMessages_ = false;
    }
#endif

    nMsgWritten_ = nMsgCancelled_ = 0;
    stream_ = stream;
    if (NULL == stream) {
        THROW_DBGLOG("%s-Loader-%llu[].open(NULL) Stream can't be NULL!", db_.textId(), id_.id);
    }


    // Assign unique text id for logging purposes
    snprintf(id_.textId, sizeof(id_.textId), "%s-Loader-%llu[%s]", db_.textId(), (ulonglong)id_.id,
        CharBuffer<0xE0>::shortened(impl(stream)->key()).c_str());

    id_.textId[sizeof(id_.textId) - 1] = '\0';

    // Read non-abstract message types
    vector<string> types = SchemaRequest(stream).getMessageTypes();
    if (0 == types.size()) {
        THROW_DBGLOG(LOGHDR ".open(): No message types defined for stream", ID);
    }

    tables_.clear();
    forn (i, types.size()) {
        tables_.registerMessageDescriptor((unsigned)i, types[i], "", "");
        tables_.typeIdLocal2Remote.set(i, (uint32_t)i);
    }

    bool minLatency = options.minLatency || TICKLOADER_FORCE_MINLATENCY;

    // Allocate buffer (currently using constant size)
    byte * allocPtr = (byte *)malloc(WRITE_BUFFER_ALLOC_SIZE);
    
    if (NULL == allocPtr) {
        DBGLOGERR((std::string*)NULL, LOGHDR ".open(): Not enough memory for message buffer", ID);
        throw std::bad_alloc();
    }

    // Align to 4k page boundary and add 8-byte front guard area
    byte * bufPtr = (byte *)DX_POINTER_ALIGN_UP(allocPtr, 0x1000);
    assert(NULL != bufPtr);

    messageWriter_.setBuffer(allocPtr, bufPtr + 8, bufPtr + MAX_MESSAGE_SIZE + 64);

    IOStream * ioStream = db_.createConnectionStream(minLatency ? TCP::NO_DELAY : TCP::NORMAL, true);
    this->ioStream_ = ioStream;
    resetPointers();

    auto & o = messageWriter_;

    DBGLOG_VERBOSE(LOGHDR ".open(): Opening loader..", ID);

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
        state_ = NOT_INITIALIZED;
        error_.code = Error::HANDSHAKE_ERROR;
        interruption_ = Interruption::SERVER_FINISHED_ERROR;
        readErrorResponse();
        cleanup();
        // No need to remove from loader, because it is not yet added
        std::string out;
        DBGLOGERR(&out, LOGHDR ".open(): Loader handshake error: %s", ID, serverErrorText().c_str());
        throw XmlErrorResponseException(401, out);
    }

    DBGLOG_VERBOSE(LOGHDR ".open(): Handshake suceeded. Initializing upload", ID);

    o.writeByte(REQ_UPLOAD_DATA);
    o.writeByte(0);                     // UseCompression
    o.writeByte(1);                     // isBigEndian
    o.writeUTF8(stream->key());
    o.writeByte(options.writeMode);
    //messageWriter_.writeInt16(11); // max allowed errors
    flushOutput();

    //assert(0 == handshake);
    DBGLOG(LOGHDR ".open(): Ready", ID);

    if (minLatency || TICKLOADER_FORCE_FLUSH) {
        minChunkSize_ = 0;
    }

    manager_.add(this);
    state_ = WRITING;
    
    //TODO: Debug
    //DBGLOG(LOGHDR ".open(): Ready /2", ID);
    //this_thread::sleep_for(chrono::milliseconds(1));

    return this;
}


template<typename T> T TickLoaderImpl::read()
{
    byte buf[sizeof(T)];
    uintptr length;
    length = ioStream_->read((char*)buf, sizeof(T), sizeof(T));
    return _loadBE<T>(buf);
}


bool TickLoaderImpl::readUTF8(std::string &s)
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


std::string TickLoaderImpl::readUTF8()
{
    std::string s;
    readUTF8(s);
    return s;
}


NOINLINE void TickLoaderImpl::readErrorResponse(ErrorInfo &error)
{
    readUTF8(error.className);
    readUTF8(error.msg);
}


void TickLoaderImpl::readErrorResponse()
{
    readErrorResponse(error_);
}


void TickLoaderImpl::readTypesSubscriptionChange()
{
    vector<string> types;
    bool all = 0 != read<byte>();
    bool added = 0 != read<byte>();
    DBGLOG_VERBOSE(LOGHDR ".readTypesSubscriptionChange(all:%d, added:%d)", ID, (int)all, (int)added);
    if (!all) {
        int32 size = read<int32>();
        //DBGLOG_VERBOSE(LOGHDR ".readTypesSubscriptionChange(): size = %d", ID, (int)size);
        assert(size >= 0);
        forn(i, size) {
            string s = readUTF8();
            types.emplace_back(s);
        }
    }
    
    DBGLOG_VERBOSE(LOGHDR ".readTypesSubscriptionChange(): STARTED", ID);
    dispatchTypesSubscriptionChange(all, added, types);
    DBGLOG_VERBOSE(LOGHDR ".readTypesSubscriptionChange(): FINISHED", ID);
}


void TickLoaderImpl::readEntitiesSubscriptionChange()
{
    vector<std::string> entities;
    bool all = 0 != read<byte>();
    bool added = 0 != read<byte>();
    DBGLOG_VERBOSE(LOGHDR ".readEntitiesSubscriptionChange(all:%d, added:%d)", ID, (int)all, (int)added);
    if (!all) {
        int32 size = read<int32>();
        assert(size >= 0);
        forn(i, size) {
            string s = readUTF8();
            entities.emplace_back(s);
        }
    }

    DBGLOG_VERBOSE(LOGHDR ".readEntitiesSubscriptionChange(): STARTED", ID);
    dispatchEntitiesSubscriptionChange(all, added, entities);
    DBGLOG_VERBOSE(LOGHDR ".readEntitiesSubscriptionChange(): FINISHED", ID);
}


void TickLoaderImpl::dispatchError(int code, const std::string &s)
{
    vector<ErrorListener *> listeners;
    {
        srw_read section(errorListenersLock_);
        for (auto i : errorListeners_) {
            listeners.emplace_back(i);
        }
    }

    for (auto i : listeners) {
        i->onError("loaderException", s);
    }
}


void TickLoaderImpl::dispatchTypesSubscriptionChange(bool all, bool added, const std::vector<std::string> &types)
{
    vector<SubscriptionListener *> listeners;
    {
        srw_read section(subscriptionListenersLock_);
        for (auto i : subscriptionListeners_) {
            listeners.push_back(i);
        }
    }

    for (auto i : listeners) {
        try {
            all ?
                (added ? i->allTypesAdded() : i->allTypesRemoved())
                :
                (added ? i->typesAdded(types) : i->typesRemoved(types));
        }
        catch (const std::exception &e) {
            std::string errmsg;
            format_string(&errmsg, LOGHDR ".dispatchTypesSubscriptionChange(all:%d, added:%d) ERROR: std::exception: %s",
                ID, (int)all, (int)added, e.what());
            DXAPIIMPL_DISPATCH_LOG();
            DXAPIIMPL_DISPATCH_THROW();
        }
        catch (...) {
            std::string errmsg;
            format_string(&errmsg, LOGHDR ".dispatchTypesSubscriptionChange(all:%d, added:%d) ERROR: system exception!",
                ID, (int)all, (int)added);
            DXAPIIMPL_DISPATCH_LOG();
            DXAPIIMPL_DISPATCH_THROW();
        }
    }
}


void TickLoaderImpl::dispatchEntitiesSubscriptionChange(bool all, bool added, const std::vector<std::string> &entities)
{
    vector<SubscriptionListener *> listeners;
    {
        srw_read section(subscriptionListenersLock_);
        for (auto i : subscriptionListeners_) {
            listeners.push_back(i);
        }
    }

    for (auto i : listeners) {
        try {
            all ?
                (added ? i->allEnititesAdded() : i->allEnititesRemoved())
                :
                (added ? i->entitiesAdded(entities) : i->entitiesRemoved(entities));
        }
        catch (const std::exception &e) {
            std::string errmsg;
            format_string(&errmsg, LOGHDR ".dispatchEntitiesSubscriptionChange(all:%d, added:%d) ERROR: std::exception: %s",
                ID, (int)all, (int)added, e.what());
            DXAPIIMPL_DISPATCH_LOG();
            DXAPIIMPL_DISPATCH_THROW();
        }
        catch (...) {
            std::string errmsg;
            format_string(&errmsg, LOGHDR ".dispatchEntitiesSubscriptionChange(all:%d, added:%d) ERROR: system exception!",
                ID, (int)all, (int)added);
            DXAPIIMPL_DISPATCH_LOG();
            DXAPIIMPL_DISPATCH_THROW();
        }
    }
}



//void TickLoaderImpl::dispatchSubscriptionChange(int code, const std::string &s)
//{
//    unordered_set<SubscriptionListener *> listeners;
//    {
//        srw_read section(subListenersLock_);
//        listeners = errorListeners_;
//    }
//
//    for (auto i : listeners) {
//        
//    }
//}


bool TickLoaderImpl::processResponse()
{
    auto ioStream = ioStream_;
    if (interruption_ > Interruption::STOPPING || NULL == ioStream) {
        return true;
    }

    dbgProcessingServerResponse_ = true;
    try {
        byte buf[0x80];
        int result;
        while (0 != ioStream->tryRead(buf, 1)) {
            unsigned command = buf[0];
            switch (command) {
            case ERROR_BLOCK_ID:
                {
                    string s;
                    result = read<int32_t>();
                    s = readUTF8();
                    DBGLOG(LOGHDR ".processResponse(): Loading error: %d (%s)", ID, (int)result, s.c_str());
                    dispatchError(result, s);
                    break;
                }
            case LOADER_KEEPALIVE_ID:
                //DBGLOG_VERBOSE(LOGHDR ".processResponse(): Keepalive marker received", ID);
                break;

            case LOADER_RESPONSE_BLOCK_ID:
                result = read<int32_t>();
                TickLoaderNs::Interruption::Enum interruptionCode;
                if (RESP_ERROR == result) {
                    error_.code = Error::ERROR;
                    readErrorResponse();
                    DBGLOG(LOGHDR ".processResponse(): Loading finished with error, error recorded", ID);
                    interruptionCode = Interruption::SERVER_FINISHED_ERROR;
                }
                else {
                    DBGLOG_VERBOSE(LOGHDR ".processResponse(): Loading succesful, result = %d", ID, (int)result);
                    interruptionCode = Interruption::SERVER_FINISHED_OK;
                }

                interruption_ = interruptionCode;
                dbgProcessingServerResponse_ = false;
                return true;

            // Subscription change listener
            case TYPE_BLOCK_ID:
                readTypesSubscriptionChange();
                break;

            case INSTRUMENT_BLOCK_ID:
                readEntitiesSubscriptionChange();
                break;

            default:
                error_.className.clear();
                format_string(&error_.msg, "Unexpected token received from the server: %02X", command);
                DBGLOG(LOGHDR ".processResponse(): unrecognized token %02X: %s", ID, command, serverErrorText().c_str());
                error_.code = Error::IO_ERROR;
                interruption_ = Interruption::SERVER_FINISHED_ERROR; // TODO: new error signatures
                ioStream_->closeInput();
            }
        }
    }

    catch (const IOStreamDisconnectException &) {
        error_.code = Error::IO_ERROR;
        interruption_ = Interruption::SERVER_DISCONNECTED;
        DBGLOG(LOGHDR ".processResponse(): WRN: disconnected from the server side", ID);
        format_string(&error_.msg, "disconnected by server");
        ioStream_->close();
    }

    catch (const IOStreamException &e) {
        error_.code = Error::IO_ERROR;
        interruption_ = Interruption::SERVER_DISCONNECTED;
        DBGLOG(LOGHDR ".processResponse(): WRN: Socket exception: %s", ID, e.what());
        format_string(&error_.msg, "IOStream exception: %s", e.what());
        ioStream_->close();
    }

    catch (const std::exception &e) {
        error_.code = Error::ERROR;
        interruption_ = Interruption::ERROR;
        DBGLOG(LOGHDR ".processResponse(): ERR: Exception: %s", ID, e.what());
        format_string(&error_.msg, "std::exception: %s", e.what());
    }

    catch (...) {
        error_.code = Error::ERROR;
        interruption_ = Interruption::ERROR;
        DBGLOG(LOGHDR ".processResponse(): ERR: Unknown exception", ID);
        format_string(&error_.msg, LOGHDR ".processResponse(): Unknown exception", ID);
    }

    dbgProcessingServerResponse_ = false;
    return false;
}


void TickLoaderImpl::closeConnection()
{
    IOStream * ios = (IOStream *)dx_thread::atomic_exchange((void *&)ioStream_, (void *)NULL);
    
    if (NULL != ios) {
        DBGLOG_VERBOSE(LOGHDR ": closing previously open transport connection", ID);
        ios->close();
        delete ios;
    }
    else {
        DBGLOG_VERBOSE(LOGHDR ": transport connection already closed", ID);
    }
}


void TickLoaderImpl::cleanup(bool deregister)
{
    if (deregister) {
        manager_.remove(this, false);
    }

    closeConnection();

    if (!interruption_) {
        interruption_ = Interruption::CLIENT_FINISHED;
    }
}


void TickLoaderImpl::finish()
{
    finishInternal(false);
}


void TickLoaderImpl::finishInternal(bool calledFromMainThread)
{
    if (isWriting()) {
        try {
            clientIsActive_ = false;
            {
                // Set interruption state. This signals other thread that use this loader to stop
                interruption_ = Interruption::STOPPING;

                static_assert(LOADER_STOPPING_WAIT_MIN_SLEEP_MS > 1, "LOADER_STOPPING_WAIT_MIN_SLEEP_MS > 1");
                int sleep_ms = LOADER_STOPPING_WAIT_MIN_SLEEP_MS;
                int i = LOADER_STOPPING_WAIT_TIMEOUT_MS;

                // try to stop the client that may still be calling next/send/writeXX, lock-free way
                for (; i > 0; i -= sleep_ms) {
                    this_thread::sleep_for(chrono::milliseconds(sleep_ms));
                    if (!clientIsActive_) {
                        // If not writing message body and not active, assume that class is interrupted
                        // If "I'm finished" signal is set externally, assume it is safe to dispose
                        if (isWriterFinished() || !isWritingMessageBody())
                            break;

                        // Otherwise, wait and increase delay, since it is possible that the other thread is not getting a timeslice
                        sleep_ms = sleep_ms * 3 / 2;
                        if (sleep_ms > LOADER_STOPPING_WAIT_MAX_SLEEP_MS) {
                            sleep_ms = LOADER_STOPPING_WAIT_MAX_SLEEP_MS;
                        }
                    }
                    else {
                        // Still active. Reset flag and wait.
                        clientIsActive_ = false;
                    }
                }
            }

            if (clientIsActive_) {
                DBGLOG(LOGHDR ".finish(): ERR: Client still doing something, probably going to crash!", ID);
            }

            if (Interruption::STOPPING != interruption_) {
                DBGLOG(LOGHDR ".finish(): ERR: Interruption state has unexpected value: %s", ID, Interruption(interruption_).toString());
            }

            if (isWritingMessageBody()) {
                DBGLOG(LOGHDR ".finish(): WRN: Message is still open, will be cancelled, header and %lld bytes discarded", ID,
                    (longlong)(messageWriter_.dataPtr - messageStart_ + LOADER_MESSAGE_HEADER_SIZE));
                cancelMessage();
                state_ = MESSAGE_START;
            }

            if (NULL == ioStream_) {
                DBGLOG(LOGHDR ".finish(): ERR: ioStream already == NULL!", ID);
            }

            bool waitResponse = false;

            auto ios = ioStream_;
            if (interruption_ <= Interruption::STOPPING) {
                if (NULL != ios && !ios->isOutputClosed()) {
                    DBGLOG_VERBOSE(LOGHDR ".finish(): finishing writing", ID);
                    endWriting();
                    flushOutput();
                    ios->closeOutput();
                    waitResponse = true;
                }
                else {
                    DBGLOG(LOGHDR ".finish(): WRN: can't finish writing - already closed. intr: %s", ID, Interruption(interruption_).toString());
                }

                state_ = FINISHED_WRITING;
            }
            else {
                state_ = FINISHED_WRITING;
                DBGLOG(LOGHDR ".finish(): writing aborted unexpectedly. intr: %s", ID, Interruption(interruption_).toString());
            }

            // If we succesfully got here, attempt clean disconnection
            DBGLOG_VERBOSE(LOGHDR ".finish(): Trying to remove itself from the pool. Wait for server response: %s", ID, waitResponse ? "true" : "false");
            bool success = manager_.remove(this, waitResponse);

            if (!success) {
                THROW_DBGLOG(LOGHDR ".finish(): Loader is unable to remove itself from LoaderManager", ID);
            }

#if VERBOSE_LOADER >= 1
            DBGLOG_VERBOSE(LOGHDR ": Closed. Messages: %llu Cancelled: %llu Bytes: %lld\n"
                , ID, (ulonglong)nMsgWritten_, (ulonglong)nMsgCancelled_, (longlong)(NULL != ioStream_ ? ioStream_->nBytesWritten() : -1));
#endif

            cleanup();
            if (Error::NO_ERROR != error_.code) {
                DBGLOG(LOGHDR ": ERR: Loader finished with %s: %s", ID, Error(error_.code).toString(), serverErrorText().c_str());
            }
        }

        catch (const IOStreamDisconnectException &) {
            state_ = FINISHED_WRITING;
            interruption_ = Interruption::SERVER_DISCONNECTED;
            DBGLOG(LOGHDR ".finish(): WARNING: disconnected from the server side", ID);
            cleanup();
            THROW_DBGLOG_EX(TickLoaderClosedException, LOGHDR ": Unexpected disconnection %s", ID, serverErrorText().c_str());
            return;
        }

        catch (const IOStreamException &e) {
            state_ = FINISHED_WRITING;
            error_.code = Error::IO_ERROR;
            interruption_ = Interruption::SERVER_DISCONNECTED;
            cleanup();
            THROW_DBGLOG_EX(TickLoaderClosedException, LOGHDR ".finish(): IO error: %s: %s", ID, e.what(), serverErrorText().c_str());
            return;
        }

        catch (const std::exception &e) {
            state_ = FINISHED_WRITING;
            error_.code = Error::ERROR;
            interruption_ = Interruption::ERROR;
            format_string(&error_.msg, "std::exception: %s", e.what());
            DBGLOG(LOGHDR ".finish(): %s", ID, error_.msg.c_str());
            cleanup();
            throw;
        }

        catch (...) {
            state_ = FINISHED_WRITING;
            error_.code = Error::ERROR;
            interruption_ = Interruption::ERROR;
            format_string(&error_.msg, "UNKNOWN_ERROR! %s", ID, serverErrorText().c_str());
            DBGLOG(LOGHDR ".finish(): %s", ID, error_.msg.c_str());
            cleanup();
            throw;
        }
    }
    else {
        cleanup(); // Just in case
    }
}


std::string TickLoaderImpl::serverErrorText()
{
    string s;
    size_t l1 = error_.className.size();
    size_t l2 = error_.msg.size();
    if (l1 + l2 != 0) {
        s.append(error_.className).append(": ").append(error_.msg);
    }

    return s;
}


INLINE void TickLoaderImpl::close()
{
    if (state_ != NOT_INITIALIZED) {
        finish();
        if (NULL != messageWriter_.bufferAllocPtr) {
            ::free(messageWriter_.bufferAllocPtr);
            messageWriter_.bufferAllocPtr = messageWriter_.dataPtr = NULL;
        }

        state_ = NOT_INITIALIZED;
    }
}



INLINE void TickLoaderImpl::commitMessage()
{
    DBGLOG_MSG(LOGHDR ".commitMessage()", ID);
    if (MESSAGE_BODY == state_) {
        endMessage();
    }

    //dispatchError(-1, "Blablabla");
}

// TODO: Deprecated
INLINE void TickLoaderImpl::send()
{
    return commitMessage();
}

INLINE void TickLoaderImpl::flush() {
    // flush only after commit message and before writing the next one
    if (MESSAGE_START == state_) {
        flushOutputMsg();
    }
}

INLINE unsigned TickLoaderImpl::getInstrumentId(const std::string &instr)
{
    unsigned entityId = tables_.findInstrument(instr);
    if (emptyEntityId == entityId) {
        entityId = (unsigned)tables_.instruments.size();
        DBGLOG_VERBOSE(LOGHDR ".getEntityId(): Registering Symbol '%s'=%u", ID, instr.c_str(), (unsigned)entityId);
        bool result = tables_.registerInstrument(entityId, instr);
        assert(tables_.instruments.size() == entityId + 1);
        writeInstrumentBlock(instr);
    }

    return entityId;
}


INLINE unsigned TickLoaderImpl::getMessageDescriptorId(const std::string &typeName)
{

    unsigned messageTypeId = tables_.findMsgDescByType(typeName);
    if (EMPTY_MSG_TYPE_ID == messageTypeId) {
        THROW_DBGLOG(LOGHDR ".getMessageDescriptor(): ERROR: Unknown message type: %s", ID, typeName.c_str());
    }

    return messageTypeId;
}


INLINE unsigned TickLoaderImpl::getMessageDescriptorId(unsigned typeId)
{

    unsigned messageTypeId = tables_.typeIdLocal2Remote.get(typeId);
    if (EMPTY_MSG_TYPE_ID == messageTypeId) {
        THROW_DBGLOG(LOGHDR ".getMessageDescriptor(): ERROR: Unknown user message type id: %u", ID, typeId);
    }

    return messageTypeId;
}


INLINE void TickLoaderImpl::registerMessageType(unsigned localTypeId, const std::string &typeName)
{
    tables_.registerLocalMessageType(localTypeId, typeName);
    unsigned messageTypeId = tables_.typeIdLocal2Remote[localTypeId];
    DBGLOG_VERBOSE(LOGHDR " Registered message type '%s' = %u(-> %u)", ID, typeName.c_str(), (unsigned)localTypeId, (unsigned)messageTypeId);
}

// Finish previous message, if started and write header for the new one, if needed
template<bool SHOULD_COMMIT> INLINE DataWriter& TickLoaderImpl::nextInternal(unsigned remoteMessageTypeId, unsigned remoteEntityId, TimestampMs timestamp)
{
    clientIsActive_ = true;

    if (shouldLogMessages_) {
        printf("!!!!!!!!!!!!!!!!!!!!: Loader %s: Symbol: %s\n", 
            ID,
            tables_.getInstrument(remoteEntityId).c_str());
    }
    
    switch (state_) {
    case NOT_IN_MESSAGE_BLOCK:
        messageWriter_.putByte(MESSAGE_BLOCK_ID);
        state_ = MESSAGE_START; // Slightly redundant
        break;

    case MESSAGE_BODY:
        if (SHOULD_COMMIT) {
            endMessage();
        }
        else {
            cancelMessage();
            state_ = MESSAGE_START;
        }
        break;

    case MESSAGE_START:
        break;

    default:
        {
            checkWriterInterruption(); // If interrupted, ignore state
            std::string out;
            DBGLOGERR(&out, LOGHDR ".next(): unknown state", ID);
            throw logic_error(out);
        }
    }

    checkWriterInterruption();
    state_ = MESSAGE_BODY;
    
    this->messageStart_ = messageWriter_.startMessage(); // This will reserve header field and save pointer to the byte following it
    messageWriter_.writeTimestamp(timestamp);
    messageWriter_.writeUInt16(remoteEntityId);
    messageWriter_.writeByte(remoteMessageTypeId);

    return messageWriter_;
}


// Finish previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::next(unsigned userTypeId, unsigned entityId, TimestampMs timestamp)
{
    unsigned typeId = getMessageDescriptorId(userTypeId);
    assert(in_range((size_t)typeId, (size_t)0, tables_.messageDescriptors.size()));

    if (!tables_.isRegisteredInstrumentId(entityId)) {
        THROW_DBGLOG(LOGHDR ".next(): used unregistered entityId = %u", ID, entityId);
    }

    return nextInternal<true>(typeId, entityId, timestamp);
}


// Finish previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::next(unsigned userTypeId, const std::string &symbolName, TimestampMs timestamp)
{
    unsigned entityId = getInstrumentId(symbolName);
    unsigned typeId = getMessageDescriptorId(userTypeId);

    assert(in_range((size_t)typeId, (size_t)0, tables_.messageDescriptors.size()));

    return nextInternal<true>(typeId, entityId, timestamp);
}


// Finish previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::next(const std::string &typeName, const std::string &symbolName, TimestampMs timestamp)
{
    unsigned entityId = getInstrumentId(symbolName);
    unsigned typeId = getMessageDescriptorId(typeName);

    return nextInternal<true>(typeId, entityId, timestamp);
}



// Cancel previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::beginMessage(unsigned userTypeId, unsigned entityId, TimestampMs timestamp)
{
    unsigned typeId = getMessageDescriptorId(userTypeId);
    assert(in_range((size_t)typeId, (size_t)0, tables_.messageDescriptors.size()));

    if (!tables_.isRegisteredInstrumentId(entityId)) {
        THROW_DBGLOG(LOGHDR ".beginMessage(): used unregistered entityId = %u", ID, entityId);
    }

    return nextInternal<false>(typeId, entityId, timestamp);
}


// Cancel previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::beginMessage(unsigned userTypeId, const std::string &symbolName, TimestampMs timestamp)
{
    unsigned entityId = getInstrumentId(symbolName);
    unsigned typeId = getMessageDescriptorId(userTypeId);

    assert(in_range((size_t)typeId, (size_t)0, tables_.messageDescriptors.size()));

    return nextInternal<false>(typeId, entityId, timestamp);
}


// Cancel previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::beginMessage(unsigned userTypeId, unsigned entityId, TimestampMs timestamp, bool shouldCommit)
{
    unsigned typeId = getMessageDescriptorId(userTypeId);
    assert(in_range((size_t)typeId, (size_t)0, tables_.messageDescriptors.size()));

    if (!tables_.isRegisteredInstrumentId(entityId)) {
        THROW_DBGLOG(LOGHDR ".beginMessage(): used unregistered entityId = %u", ID, entityId);
    }

    return shouldCommit ? nextInternal<true>(typeId, entityId, timestamp) : nextInternal<false>(typeId, entityId, timestamp);
}


// Cancel previous message, if started and write header for the new one, if needed
INLINE DataWriter& TickLoaderImpl::beginMessage(unsigned userTypeId, const std::string &symbolName, 
    TimestampMs timestamp, bool shouldCommit)
{
    unsigned entityId = getInstrumentId(symbolName);
    unsigned typeId = getMessageDescriptorId(userTypeId);

    assert(in_range((size_t)typeId, (size_t)0, tables_.messageDescriptors.size()));

    return shouldCommit ? nextInternal<true>(typeId, entityId, timestamp) : nextInternal<false>(typeId, entityId, timestamp);
}


INLINE void TickLoaderImpl::preload(uintptr nBytesToPreload, uintptr readBlockSize)
{
    // TODO:
#if 0
    if (NULL == preloaderOutputStream) {
        preloaderOutputStream = new OutputStreamPreloader(activeInputStream, readBlockSize);
        activeInputStream = preloaderInputStream;
        DataReaderBaseImpl * r = static_cast<DataReaderBaseImpl *>(reader);
        r->setInputStream(preloaderInputStream);
    }

    OutputStreamPreloader * preloader = static_cast<OutputStreamPreloader *>(preloaderOutputStream);
    preloader->preload(nBytesToPreload);
#endif
}


// TODO: Later, maybe. Can use __VA_ARGS__
//#define INTERFACE_CLASS TickLoader
//#define IMPL_CLASS TickLoaderImpl
//#define PROXY_METHOD(name, parms) void INTERFACE_CLASS::name() { return static_cast<IMPL_CLASS *>(this)->name(parms); }
//void PROXY_METHOD(finish,)


void TickLoader::finish()
{
    return THIS_IMPL->finish();
}


void TickLoader::close()
{
    return THIS_IMPL->close();
}


void TickLoader::preload(uintptr nBytesToPreload, uintptr writeBlockSize)
{
    return THIS_IMPL->preload(nBytesToPreload, writeBlockSize);
}


void TickLoaderImpl::addListener(ErrorListener * listener)
{
    srw_write section(errorListenersLock_);
    errorListeners_.emplace(listener);
    DBGLOG(LOGHDR ": TickLoader error listener added, n=%u", ID, (unsigned)errorListeners_.size());
}


void TickLoaderImpl::addListener(SubscriptionListener * listener)
{
    srw_write section(subscriptionListenersLock_);
    subscriptionListeners_.emplace(listener);
    DBGLOG(LOGHDR ": TickLoader subscription listener added, n=%u", ID, (unsigned)subscriptionListeners_.size());
}


void TickLoaderImpl::removeListener(ErrorListener * listener)
{
    srw_write section(errorListenersLock_);
    errorListeners_.erase(listener);
    DBGLOG(LOGHDR ": TickLoader error listener removed, n=%u", ID, (unsigned)errorListeners_.size());
}


void TickLoaderImpl::removeListener(SubscriptionListener * listener)
{
    srw_write section(subscriptionListenersLock_);
    subscriptionListeners_.erase(listener);
    DBGLOG(LOGHDR ": TickLoader subscription listener removed, n=%u", ID, (unsigned)subscriptionListeners_.size());
}


void TickLoader::addListener(ErrorListener * listener)
{
    THIS_IMPL->addListener(listener);
}


void TickLoader::addListener(SubscriptionListener * listener)
{
    THIS_IMPL->addListener(listener);
}


void TickLoader::removeListener(ErrorListener * listener)
{
    THIS_IMPL->removeListener(listener);
}


void TickLoader::removeListener(SubscriptionListener * listener)
{
    THIS_IMPL->removeListener(listener);
}


INLINE size_t TickLoaderImpl::nErrorListeners()
{
    srw_read section(errorListenersLock_);
    return errorListeners_.size();
}


INLINE size_t TickLoaderImpl::nSubscriptionListeners()
{
    srw_read section(subscriptionListenersLock_);
    return subscriptionListeners_.size();
}


size_t TickLoader::nErrorListeners()
{
    return THIS_IMPL->nErrorListeners();
}


size_t TickLoader::nSubscriptionListeners()
{
    return THIS_IMPL->nSubscriptionListeners();
}


INLINE uint64_t TickLoaderImpl::nBytesWritten() const
{
    return NULL != ioStream_ ? ioStream_->nBytesWritten() : 0;
}


uint64_t TickLoader::nBytesWritten() const
{
    return THIS_IMPL->nBytesWritten();
}


bool TickLoader::isClosed() const
{
    return THIS_IMPL->isClosed();
}

const DxApi::TickStream * TickLoader::stream() const
{
    return THIS_IMPL->stream();
}

INLINE uint64_t TickLoaderImpl::nMsgWritten() const
{
    return nMsgWritten_;
}


uint64_t TickLoader::nMsgWritten() const
{
    return THIS_IMPL->nMsgWritten();
}


INLINE uint64_t TickLoaderImpl::nMsgCancelled() const
{
    return nMsgCancelled_;
}


uint64_t TickLoader::nMsgCancelled() const
{
    return THIS_IMPL->nMsgCancelled();
}



INLINE DataWriter& TickLoaderImpl::getWriter()
{
    return THIS_IMPL->messageWriter_;
}



DataWriter& TickLoader::getWriter()
{
    return THIS_IMPL->getWriter();
}


DataWriter& TickLoader::next(const std::string &typeName, const std::string &symbolName, TimestampMs timestamp)
{
    return THIS_IMPL->next(typeName, symbolName, timestamp);
}


DataWriter& TickLoader::next(unsigned messageTypeId, const std::string &symbolName, TimestampMs timestamp)
{
    return THIS_IMPL->next(messageTypeId, symbolName, timestamp);
}


DataWriter& TickLoader::next(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp)
{
    return THIS_IMPL->next(messageTypeId, entityId, timestamp);
}


DataWriter& TickLoader::beginMessage(unsigned messageTypeId, const std::string &symbolName, TimestampMs timestamp)
{
    return THIS_IMPL->beginMessage(messageTypeId, symbolName, timestamp);
}


DataWriter& TickLoader::beginMessage(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp)
{
    return THIS_IMPL->beginMessage(messageTypeId, entityId, timestamp);
}


DataWriter& TickLoader::beginMessage(unsigned messageTypeId, const std::string &symbolName, 
    TimestampMs timestamp, bool commitPrevious = false)
{
    return THIS_IMPL->beginMessage(messageTypeId, symbolName, timestamp, commitPrevious);
}


DataWriter& TickLoader::beginMessage(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp, bool commitPrevious = false)
{
    return THIS_IMPL->beginMessage(messageTypeId, entityId, timestamp, commitPrevious);
}


void TickLoader::send()
{
    return THIS_IMPL->send();
}


void TickLoader::commitMessage()
{
    return THIS_IMPL->commitMessage();
}

void TickLoader::flush()
{
    return THIS_IMPL->flush();
}


void TickLoader::registerMessageType(unsigned newTypeId, const std::string &newTypeName)
{
    return THIS_IMPL->registerMessageType(newTypeId, newTypeName);
}


//unsigned TickLoader::getInstrumentId(const std::string &symbolName, InstrumentType instrumentType)
//{
//    return THIS_IMPL->getInstrumentId(symbolName, instrumentType);
//}


unsigned TickLoader::getInstrumentId(const std::string &instrument)
{
    return THIS_IMPL->getInstrumentId(instrument);
}


void TickLoaderImpl::dbgBreakConnection(bool input, bool output)
{
    auto io = ioStream_;

    if (NULL == io) {
        return;
    }

    if (NULL == ioStream_) {
        return;
    }

    if (input && output) {
        io->close();
    }
    else {
        if (input) {
            io->closeInput();
        }

        if (output) {
            io->closeOutput();
        }
    }
}


void TickLoaderImpl::setShouldLogMessages(bool shouldLog)
{
    shouldLogMessages_ = shouldLog;
}


void TickLoaderImpl::setWriterFinished()
{
    // TODO: compare and set is better, but this will work in out case
    if (NULL != this) {
        if (WRITER_FINISHED_UNSET == writerFinishedSignal_) {
            DxAtomicExchangeUInt32(&writerFinishedSignal_, WRITER_FINISHED_SET);
        }
    }
}