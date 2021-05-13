#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <algorithm>
#include <thread>
#include <chrono>

#include "io/input_preloader.h"

#include "xml/selection_options_impl.h"
#include "xml/xml_request.h"
#include "xml/tickdb_class_descriptor.h"
#include "xml/cursor/reset_request.h"
#include "xml/cursor/close_request.h"
#include "xml/cursor/modify_entities_request.h"
#include "xml/cursor/modify_types_request.h"
#include "xml/cursor/modify_streams_request.h"

#include "tickdb/tickstream_impl.h"

#include "tickdb_http.h"
#include "tickcursor_http.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace dx_thread;

#define THIS_IMPL (impl(this))


#if VERBOSE_CURSOR >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

#define LOGHDR "%s"
#define ID (this->textId().c_str())


#if VERBOSE_CURSOR >= 2
#define DBGLOG_MSG DBGLOG
#define DBG_DUMP dbg_dump
#else
#define DBGLOG_MSG (void)
#define DBG_DUMP (void)
#endif


INLINE TickCursor::TickCursor()
{
    // Do Absolutely Nothing
    static_assert(1 == sizeof(*this), "API object size should be 1(actually 0)");
}


TickCursorImpl::TickCursorImpl(TickDbImpl &db, uint64 uid) :
    state(CursorStateEnum::STARTED),

    db_(db),
    bufferStart_(NULL),
    bufferAllocPtr_(NULL),
    activeInputStream_(NULL),
    preloaderInputStream_(NULL),
    reader_(NULL),

    isWithinMessageBlock_(false),
    isIdle_(false),
    readMessageDone_(false),
    isInterrupted_(false),
    isInNext_(false),

    isClosed_(false),
    isOpen_(false),

    shadowCopyUpdated_(false),
    cursorId_(-1),
    subscriptionTimeMs_(TIMESTAMP_NULL),
    id_(uid)
{
    isOpen_ = false;
    subState_[0].isResetInProgress_ = subState_[0].readingRestarted_ = false;
    subState_[0].lastExpectedCommandId_ = subState_[0].firstExpectedCommandId_ = -1;
    mainState_ = shadowState_ = &subState_[0];
    mainState_->clearStreamsArray();
    snprintf(id_.textId, sizeof(id_.textId), "%s-Cursor-%llu", db_.textId(), (ulonglong)id_.id);
    id_.textId[sizeof(id_.textId) - 1] = '\0';
}


TickCursorImpl::TickCursorImpl(TickCursorImpl &other, uint64_t uid) :
    state(other.state),

    db_(other.db_),
    bufferStart_(NULL),
    bufferAllocPtr_(NULL),
    activeInputStream_(NULL),
    preloaderInputStream_(NULL),
    reader_(NULL),

    isWithinMessageBlock_(false),
    isInterrupted_(false),
    isInNext_(false),

    isClosed_(false),
    isOpen_(false),

    shadowCopyUpdated_(other.shadowCopyUpdated_),
    cursorId_(other.cursorId_),
    id_(uid)
{ 
    if (other.isOpen_ || other.isClosed_ || other.state != CursorState::STARTED) {
        THROW_DBGLOG("TickCursorImpl(&other) : Only can copy non-open TickCursor");
    }

    subState_[0] = other.subState_[0];
    subState_[1] = other.subState_[1];

    assert(&other.subState_[0] == other.mainState_);
    assert(other.mainState_ == other.shadowState_);
    mainState_ = shadowState_ = &subState_[0];
    mainState_->clearStreamsArray();

    isOpen_ = false;

    snprintf(id_.textId, sizeof(id_.textId) - 1, "%s-Cursor-%llu", db_.textId(), (ulonglong)id_.id);
}


TickCursor::~TickCursor()
{
    // Delegated to implementation class via delete operator
    DBGLOG_VERBOSE(LOGHDR ".~TickCursor(): destructor is delegated to impl", impl(this)->id_.textId);
}


TickCursorImpl::~TickCursorImpl()
{
    DBGLOG(LOGHDR ".~TickCursorImpl(): destructor started", impl(this)->id_.textId);
    freeResources();
    DBGLOG(LOGHDR ".~TickCursorImpl(): deleted", impl(this)->id_.textId);
}


void TickCursor::operator delete(void * ptr, size_t sz)
{
    if (NULL != ptr) {
        if (sz == sizeof(TickCursor)) {
            delete impl((TickCursor *)ptr);
        }
        else {
            DBGLOG_VERBOSE(LOGHDR ".operator delete %u", impl((TickCursor *)ptr)->id_.textId, (unsigned)sz);
            ::operator delete(ptr);
        }
    }
}


TickCursorImpl& TickCursorImpl::operator = (const TickCursorImpl &other)
{
    THROW_DBGLOG(LOGHDR "::operator=() : Can not assign TickCursor class", ID);
}


void TickCursorImpl::SubscriptionState::clearStreamsArray()
{
    for (auto &i : tickdbStreams_) {
        if (NULL != i) {
            delete i;
        }
    }

    tickdbStreams_.resize(1);
    tickdbStreams_[0] = NULL;
}


void TickCursorImpl::closeConnection()
{
    if (NULL != activeInputStream_) {
        DBGLOG_VERBOSE(LOGHDR ": closing previously open transport connection", ID);
        if (NULL != preloaderInputStream_) {
            InputStreamPreloader * preloader = static_cast<InputStreamPreloader *>(preloaderInputStream_);
            // TODO: Looks like kludge, decide who is responsible for destroying source stream
            delete preloader->source();
        }

        delete activeInputStream_;
        activeInputStream_ = preloaderInputStream_ = NULL;
    }
}


void TickCursorImpl::freeResources()
{
    this->close(false, true);

    closeConnection();
  
    if (NULL != reader_) {
        reader_->invalidate();
        delete reader_;
        reader_ = NULL;
    }

    if (NULL != bufferAllocPtr_) {
        ::free(bufferAllocPtr_);
        bufferAllocPtr_ = bufferStart_ = NULL;
    }

    mainState_->clearStreamsArray();
    mainState_->tables_.clear();
    *other(mainState_) = *mainState_;
}


void TickCursorImpl::finalStats()
{
    DBGLOG(LOGHDR ": Cursor closed. Stats: %s", ID, getStatsUpdate(false).c_str());
    DX_CLEAR_NI(prev);
    DBGLOG(LOGHDR ": Total: %s", ID, getStatsUpdate(false).c_str());
}


string TickCursorImpl::textId() const
{
    string out;
    const char * separator = "";

    out.append(id_.textId);
    out.append("[");
    out.append(options.reverse ? "{" : "}");
    out.append(options.qql.is_set() && options.qql.get().size() != 0 ? "Q" : "S");
    out.append(isOpen_ ? "O" : "C");
    out.append(options.live ? "L" : "B");
    out.append(extraTraitsText);
    out.append(" ");

    // TODO: move this text id into state
    SubscriptionState s = *shadowState_;
    assert(s.tickdbStreams_.size() >= 1);
    for (auto i = s.tickdbStreams_.cbegin() + 1; i != s.tickdbStreams_.cend(); ++i) {
        out.append(separator);
        out.append("'");
        out.append((**i).key());
        out.append("'");
        separator = ",";
    }

    out.append("]");
    return out;
}


void TickCursorImpl::statsCheckpoint()
{
    this->prev = this->stats;
    periodStartTimestamp_ = lastTimestamp_;
}


string TickCursorImpl::getStatsUpdate(bool reset)
{
    auto nMessagesReceived = this->stats.nMessagesProcessed - this->prev.nMessagesProcessed;
    auto nMessagesSkipped = this->stats.nMessagesSkipped - this->prev.nMessagesSkipped;
    auto lastTimestamp = lastTimestamp_;
    auto prevTimestamp = periodStartTimestamp_;

    char timebuf[0x400];

    snprintf(timebuf, sizeof(timebuf), "(%s-%s): %llu recv - %llu skipped = %llu", TS_NS2STR(prevTimestamp), TS_NS2STR(lastTimestamp),
        (ulonglong)nMessagesReceived, (ulonglong)nMessagesSkipped, (ulonglong)(nMessagesReceived - nMessagesSkipped));
    timebuf[sizeof(timebuf) - 1] = '\0';

    if (reset) {
        this->prev = this->stats;
        periodStartTimestamp_ = lastTimestamp;
    }

    return string(timebuf);
}


bool TickCursorImpl::isSystemMessageGuid(const char * guid)
{
    const char * messages[] = {
        "SYS:RealTimeStartMessage:1",
        "SYS:DataLossMessage:1"
    };

    for (auto i : messages) {
        if (0 == strcmp_ni(guid, i))
            return true;
    }

    return false;
}


void TickCursorImpl::registerNewInstrument(SubscriptionState &s, unsigned newId, const std::string &instr)
{
    unsigned expectedId = (unsigned)s.tables_.instruments.size();
    if (expectedId != newId) {
        THROW_DBGLOG(LOGHDR ": Instrument cache out of sync: New entity id: %u was unexpected, the next expected value is: %u", ID, newId, expectedId);
    }

    DBGLOG_VERBOSE(LOGHDR ": added instrument (%s)=%u", ID, instr.c_str(), newId);

    s.tables_.registerInstrument(newId, instr);
    auto i = s.isUnregisteredSymbolSkipped_.find(instr);
    if (s.isUnregisteredSymbolSkipped_.cend() != i) {
        // We had a filter entry for this instrument before it came up
        s.isSymbolSkipped_.set(newId, i->second);
        s.isUnregisteredSymbolSkipped_.erase(i);
    }
}


void TickCursorImpl::registerNewStream(SubscriptionState &s, unsigned newId, const string &streamKey)
{
    auto &streamKeys = s.tables_.streamKeys_;
    unsigned expectedId = (unsigned)streamKeys.size();
    if (expectedId != newId) {
        THROW_DBGLOG(LOGHDR ": Cursor stream key cache out of sync: New stream index: %u was unexpected, the next expected value is: %u", ID, newId, expectedId);
    }

    unsigned foundId = s.tables_.findStreamId(streamKey);
    if (emptyStreamId != foundId) {
        THROW_DBGLOG(LOGHDR ": Cursor stream key cache out of sync: New stream index: %u has the same key as previously registered: %u", ID, newId, foundId);
    }

    DBGLOG_VERBOSE(LOGHDR ": added stream key (%s)=%u", ID, streamKey.c_str(), newId);
    streamKeys.set(newId, new string(streamKey));

    const auto i = s.isUnregisteredStreamSkipped_.find(streamKey);
    if (s.isUnregisteredStreamSkipped_.cend() != i) {
        // We had a filter entry for this stream key before it came up.
        s.isStreamSkipped_.set(newId, i->second);
        s.isUnregisteredStreamSkipped_.erase(i);
    }
}


void TickCursorImpl::registerNewMessageType(SubscriptionState &s, unsigned newMsgDescId, const string &data)
{
    unsigned expectedId = (unsigned)s.tables_.messageDescriptors.size();
    if (expectedId != newMsgDescId) {
        THROW_DBGLOG(LOGHDR ": Cursor symbol cache out of sync: New message type id: %u was unexpected, the next expected value is: %u",
            ID, newMsgDescId, expectedId);
    }

    //DBGLOG("New Message type :\n%s", data.c_str());

    Schema::TickDbClassDescriptorImpl classDescriptor;

    if (!classDescriptor.fromXML(data)) {
        THROW_DBGLOG(LOGHDR ": unable to parse messageDescriptor with id %u from XML text: %s",
            ID, newMsgDescId, data.c_str());
    }

    const string &typeName = classDescriptor.className;
    const string &typeGuid = classDescriptor.guid;

    // NOTE: Previously we also saved the info about stream id etc. This code is now removed and streams are identified and filtered by stream id
    // due to impossibility of distinguishing messages by GUID.

    auto msgDesc = s.tables_.registerMessageDescriptor(newMsgDescId, typeName, typeGuid, data);
    auto &filter = s.isMessageDescriptorSkipped_;
    uint8_t isSkipped = false;

    // Verify correctness
    assert(msgDesc->id == newMsgDescId);
    assert(newMsgDescId == s.tables_.findMsgDescByType(typeName));

    {
        const auto i = s.isUnregisteredMessageTypeSkipped_.find(typeName);

        if (s.isUnregisteredMessageTypeSkipped_.cend() != i) {
            // We had a filter entry for this message before it came up.
            // Supposedly, we have never encountered this message type. TODO: Maybe we added it while preparing filtering tables?

            isSkipped = i->second;
            s.isUnregisteredMessageTypeSkipped_.erase(i);
        }
        else {
            unsigned sibling = msgDesc->nextSameType;
            // Propagate filter value from sibling(all messages with the same name share filtering status), or use current default value
            isSkipped = EMPTY_MSG_TYPE_ID == sibling ? filter.defaultValue() : filter.get(sibling);
        }
    }

    filter.set(newMsgDescId, isSkipped);

    const char *sysmsg = "";
    if (isSystemMessageGuid(typeGuid.c_str())) {
        sysmsg = "system ";
        assert(newMsgDescId < COUNTOF(isSystemMessage_));
        isSystemMessage_[newMsgDescId] = true;
    }

    DBGLOG(LOGHDR ": Added %smessage descriptor: %s(%s) = %u", ID, sysmsg, typeName.c_str(), typeGuid.c_str(), newMsgDescId);
}

//void TickCursorImpl::registerNewMessageType(unsigned newId, const std::string & newName)
//{
//}
//
//
//void TickCursorImpl::registerNewEntityId(unsigned newId, const std::string & newName, InstrumentType newType)
//{
//}


void TickCursorImpl::readMessageType()
{
    string      newMessageDescriptorText;
    unsigned    descriptorId = reader_->getUInt16();

    if (!reader_->getUTF8(newMessageDescriptorText)) {
        THROW_DBGLOG(LOGHDR ".readMessageType(): Unable to read message definition for message id: %u", ID, descriptorId);
    }

    {
        srw_write section(thisLock_);
        synchronizeCheck();
        registerNewMessageType(*mainState_, descriptorId, newMessageDescriptorText);
    }
}


void TickCursorImpl::readEntity()
{
    string          symbol;
    unsigned        symbolId = reader_->getUInt16();
    reader_->getUTF8(symbol);

    {
        srw_write section(thisLock_);
        synchronizeCheck();
        registerNewInstrument(*mainState_, symbolId, symbol);
    }
}


void TickCursorImpl::readStreamKey()
{
    std::string         newStreamKey;
    unsigned streamId = reader_->getByte();
    if ((0xFF & emptyStreamId) == streamId) {
        THROW_DBGLOG(LOGHDR ".readStreamKey(%s -> 255): streamId=255 is reserved for NULL messages (too many streams in cursor?)",
            ID, newStreamKey.c_str());
    }

    reader_->getUTF8(newStreamKey);

    {
        srw_write section(thisLock_);
        synchronizeCheck();
        registerNewStream(*mainState_, streamId, newStreamKey);
    }
}


// Skip a sequence of messages until message block ends
void TickCursorImpl::skipMessageBlock()
{
    DataReaderBaseImpl &reader = *this->reader_;

    while (1) {
        unsigned messageSize = reader.getUInt32();
        if ((unsigned)MESSAGE_BLOCK_TERMINATOR_RECORD == messageSize) {
            isWithinMessageBlock_ = false;
            return;
        }

        reader.DataReader::skip(messageSize);
    }
}


// Log message content as-is. There's no warranty that the full message body is logged, this is just a debug code writen for the most common case
// Only the header is guaranteed to be written, body may or may not be saved completely
void TickCursorImpl::dbg_dumpMessage(const MessageHeader &msg, const char *text)
{
#if VERBOSE_CURSOR < 3
    DBGLOG(LOGHDR ": %s: %s: %s, payload %u b", ID, text, TS_NS2STR(msg.timestamp), (*mainState_->tables_.getMessageTypeName(msg.typeId)).c_str(), (unsigned)msg.length - CURSOR_MESSAGE_HEADER_SIZE);
#else
    size_t messageBodySize = msg.length - CURSOR_MESSAGE_HEADER_SIZE; // We already read the header
    size_t sz = min(messageBodySize, reader_->nBytesAvailable());
    byte *buffer = (byte *)alloca(sz + 0x20); // Allocate on stack. Size of the buffer is limited by READ_BUFFER_SIZE anyway
    assert(sz <= READ_BUFFER_SIZE);

    DBGLOG(LOGHDR ": dumping msg: %s: %s", ID, TS_NS2STR(msg.timestamp), (*mainState_->tables_.getMessageTypeName(msg.typeId)).c_str());

    if (NULL != buffer) {
        byte * wp = buffer;
        // Restore header in its original form, as we already read it and it could disappear from the input buffer by now
        static_assert((CURSOR_MESSAGE_HEADER_SIZE + sizeof(uint32)) == 16, "Verify that message size did not change");
        _storeBE(wp, (uint32_t)msg.length);         wp += sizeof(uint32_t);
        _storeBE(wp, (uint64_t)msg.timestamp);      wp += sizeof(uint64_t);
        _storeBE(wp, (uint16_t)msg.entityId);       wp += sizeof(uint16_t);
        _storeBE(wp, (uint8_t)msg.typeId);          wp += sizeof(uint8_t);
        _storeBE(wp, (uint8_t)msg.streamId);        wp += sizeof(uint8_t);
        memcpy(wp, reader_->peek(sz), sz);

        // CursorID-textParm-typeName
        DBG_DUMP(this->textId().append("-").append(text).append("-").append(timestamp_ns2str(msg.timestamp, false)).append("-").append(*mainState_->tables_.getMessageTypeName(msg.typeId)).c_str(),
            buffer, messageBodySize + (CURSOR_MESSAGE_HEADER_SIZE + sizeof(uint32)));
    }
#endif
}


// Blocks until succesfully reads message or end-of-block is reached
// true  - if reads valid message passed through filterss
// false - if end-of-messageblock
bool TickCursorImpl::readNextMessageInBlock(MessageHeader &msg)
{
    DataReaderBaseImpl &reader = *this->reader_;

    // We should never ever enter readNextMessage() if reset mode is activated

    assert(isWithinMessageBlock_);
    while (1) {
        unsigned messageSize = reader.getUInt32();
        if ((unsigned)MESSAGE_BLOCK_TERMINATOR_RECORD == messageSize) {
            isWithinMessageBlock_ = false;
            return false;
        }

        msg.length = messageSize;
        reader.setMessageSize(messageSize);

        if (!in_range(messageSize, (unsigned)CURSOR_MESSAGE_HEADER_SIZE, (unsigned)MAX_MESSAGE_SIZE + 1)) {
            THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextMessage(): Incorrect message size: %u", ID, messageSize);
        }

        // read: timestamp instrument_index type_index body
        lastTimestamp_ = msg.timestamp = reader.getInt64();

        unsigned currentEntityId = reader.getInt16();
        msg.entityId = currentEntityId;

        unsigned currentDescriptorId = reader.getByte();
        msg.typeId = currentDescriptorId;

        // This will convert 255 to -1, all other byte values are left unchanged
        intptr_t currentStreamId = (intptr_t)((reader.getByte() + 1) & 0xFF) - 1;
        msg.streamId = (int32_t)currentStreamId;

        //DBGLOG("msg.entityId=%u", currentEntityId);

        // At this point the whole header is read from the input stream

        if (shadowCopyUpdated_) {
            srw_write section(thisLock_); // TODO: srw_read?
                
            assert(shadowState_ != mainState_);
            mainState_ = shadowState_;
            shadowCopyUpdated_ = false; // TODO: combined with flag?

            SubscriptionState &s = *mainState_;
            if (s.readingRestarted_) {
                s.readingRestarted_ = false;
                setState(CursorState::STARTED);
            }
        }

        const SubscriptionState &s = *mainState_;

        if (!s.tables_.isRegisteredInstrumentId(currentEntityId)) {
            THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextMessage(): unexpected EntityIndex = %u", ID, currentEntityId);
        }

        if (!s.tables_.isRegisteredMsgDescId(currentDescriptorId)) {
            THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextMessage(): unexpected MessageDescriptorIndex = %u", ID, currentDescriptorId);
        }

        // Special code to consider currectStreamId = -1
        if (currentStreamId >= (intptr_t)s.tables_.streamKeys_.size()) {
            THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextMessage(): unexpected StreamIndex = %u", ID, (byte)currentStreamId);
        }

        //unsigned currentUserTypeIndex = s.tables_.messageTypeRemote2Local[currentTypeId];
        //msg.typeIdLocal = currentUserTypeIndex;
        msg.typeIdLocal = 0;        
        
        ++stats.nMessagesProcessed;
        // TODO: use arithmetic OR for speed

        // Everything is skipped during Reset
        if (s.isResetInProgress_)
            goto skip;

        // System messages are always allowed, unless Reset
        // The rest is filtered by instrument and type
        if (!isSystemMessage_[currentDescriptorId] &&
            (s.isMessageDescriptorSkipped_.get(currentDescriptorId) || s.isSymbolSkipped_.get(currentEntityId)))
            goto skip;

#if defined(_DEBUG) && VERBOSE_CURSOR >= 1
        if (isSystemMessage_[currentDescriptorId]) {
            DBGLOG(LOGHDR ": %s system msg received. Stats: %s", ID, getMessageTypeName(currentDescriptorId)->c_str(), getStatsUpdate(true).c_str());

            /*if (NULL != strstr(getMessageTypeName(currentDescriptorId)->c_str(), "RealTimeStartMessage")) {
            }*/
        }
#endif

#if VERBOSE_CURSOR >= 2
        dbg_dumpMessage(msg, "msg");
#endif
        
        msg.cursorState = setState(CursorState::READING); // TODO: Better solution later, no time
        
        return true;

    skip:
#if VERBOSE_CURSOR >= 2
        dbg_dumpMessage(msg, "skip");
#endif

//#if VERBOSE_CURSOR_FILTERS >= 2
//        DBGLOG(LOGHDR ": msg skipped by filter", ID);
//#endif
        ++stats.nMessagesSkipped;
        reader_->nextMessage();
    }
}


NOINLINE void TickCursorImpl::synchronizeDo()
{
    assert(shadowState_ != mainState_);
    (SubscriptionState * volatile &)mainState_ = shadowState_;
    shadowCopyUpdated_ = false;
}


INLINE void TickCursorImpl::synchronizeCheck()
{
    if (shadowCopyUpdated_) {
        synchronizeDo();
    }
}


NOINLINE uint8_t TickCursorImpl::setStateLocked(CursorState newState)
{
    // TODO: use atomics
    srw_write section(thisLock_);
    return (volatile uint8_t &)state = newState;
}


// False means the cursor is stopped, true means that 1 record id is processed
bool TickCursorImpl::readNextRecord()
{
    byte id = reader_->getByte();
    isIdle_ = false;

    switch (id) {
    case TYPE_BLOCK_ID:
        readMessageType();
        break;

    case INSTRUMENT_BLOCK_ID:
        readEntity();
        break;

    case STREAM_BLOCK_ID:
        readStreamKey();
        break;

    case MESSAGE_BLOCK_ID:
        isWithinMessageBlock_ = true;
        break;

    case ERROR_BLOCK_ID:
    {
        int8 code = reader_->getByte();
        string text;
        reader_->getUTF8(text);
        DBGLOGERR((string *)NULL, LOGHDR ".readNextRecord(): Server-side error %d: %s", ID, (int)code, text.c_str());
        throw TickCursorServerError((int)code, text.c_str());
    }

    case TERMINATOR_BLOCK_ID:
    {
        srw_write section(thisLock_);

        synchronizeCheck();
        SubscriptionState &s = *mainState_;

        if (options.live && !isInterrupted_) {
            DBGLOG(LOGHDR ".readNextRecord(): WARNING: Server just sent 'terminator block id' to live cursor!!!", ID);
        }

        if (s.isExecutingSubscriptions() && !isInterrupted_) {
            DBGLOG_VERBOSE(LOGHDR ": Terminator marker ignored", ID);
            break;
        }

        setState(CursorState::END); // We are under lock already
        finalStats();
        return false;
    }

    case PING_BLOCK_ID:
        DBGLOG_VERBOSE(LOGHDR ": Ping received. Stats: %s", ID, getStatsUpdate(false).c_str());
        isIdle_ = true;
        break;

    case CURSOR_BLOCK_ID:
    {
        int64 id = reader_->getInt64();
        if (cursorId_ != -1 && cursorId_ != id) {
            THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextRecord(): Redundant cursor id record encountered with different id: %lld instead of %lld",
                ID, (longlong)id, (longlong)cursorId_);
        }

        cursorId_ = id;
        DBGLOG(LOGHDR ": Cursor id assigned by server: %lld", ID, (longlong)id);
    }
    break;

    case COMMAND_BLOCK_ID:
    {
        int64 cmdid = reader_->getInt64();
        DBGLOG_VERBOSE(LOGHDR ": cmdid: %lld", ID, (longlong)cmdid);

        {
            static const char* isLastStr[2] = { "", " - all commands done" };
            srw_write section(thisLock_);
            synchronizeCheck();

            SubscriptionState &s = *mainState_;
            assert(s.firstExpectedCommandId_ <= s.lastExpectedCommandId_);
            if (cmdid == s.firstExpectedCommandId_) {
                bool isLast = s.firstExpectedCommandId_ == s.lastExpectedCommandId_;

                do {
                    if (s.isResetInProgress_) {
                        if (cmdid == s.lastResetCommandId_) {
                            s.isResetInProgress_ = false;
                            s.lastResetCommandId_ = -1;
                            DBGLOG(LOGHDR ": Cursor reset completed%s Stats: %s", ID, isLastStr[isLast], getStatsUpdate(true).c_str());
                            break;
                        }
                        else {
                            if (!in_range(s.lastResetCommandId_, s.firstExpectedCommandId_, s.lastExpectedCommandId_ + 1)) {
                                THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextRecord(): Logic error - invalid reset command id during reset operation: (%lld, [%lld, %lld])", ID, (longlong)s.lastResetCommandId_,
                                    (longlong)s.firstExpectedCommandId_, (longlong)s.lastExpectedCommandId_);
                            }
                        }
                    }

                    DBGLOG(LOGHDR ": Cursor subscription change completed%s", ID, isLastStr[isLast]);
                } while (0);


                ++s.firstExpectedCommandId_;
                if (isLast) {
                    s.firstExpectedCommandId_ = s.lastExpectedCommandId_ = -1;
                }
            }
            else {
                THROW_DBGLOG_EX(TickCursorError, LOGHDR ": Unexpected cursor subscription command id. Expected: %lld, Received: %lld ",
                    ID, (longlong)s.firstExpectedCommandId_, (longlong)cmdid);
            }
        }
    }
    break;

    default:
        THROW_DBGLOG_EX(TickCursorError, LOGHDR ".readNextRecord(): Invalid block marker = %d", ID, (int)id);
    }

    return true;
}


/*
void TickCursorImpl::onDisconnectedInput()
{
    isAtEnd_ = true;
    isWithinMessageBlock_ = false;
    isOpen_ = false;
    spin_wait_ms(1);
}
*/


// true if able to read next valid message
// false, if EOF
INLINE bool TickCursorImpl::readNextMessage(MessageHeader &msg)
{
    if (isWithinMessageBlock_) {
        reader_->nextMessage();

 readmessage:
        if (readNextMessageInBlock(msg)) {
            return true;
        }
    }

    assert(!isWithinMessageBlock_);
    while (readNextRecord()) {
        if (isWithinMessageBlock_) {
            goto readmessage;
        }
    }

    if (state != CursorState::END) {
        setStateLocked(CursorState::END);
    }

    msg.cursorState = state;
    return false;
}

INLINE bool TickCursorImpl::readNextMessageNonBlock(MessageHeader& msg)
{
    if (isWithinMessageBlock_) {
    readmessage:
        if (readNextMessageInBlock(msg)) {
            return true;
        }
        else {
            return false;
        }
    }

    assert(!isWithinMessageBlock_);
    while (readNextRecord()) {
        if (isWithinMessageBlock_) {
            goto readmessage;
        }
        if (isIdle_) {
            return false;
        }
    }

    if (state != CursorState::END) {
        setStateLocked(CursorState::END);
    }

    msg.cursorState = state;
    return false;
}


void TickCursorImpl::skipUntilEOF()
{
    if (isWithinMessageBlock_) {
        reader_->nextMessage();
        skipMessageBlock();
    }

    while (readNextRecord()) {
        if (isWithinMessageBlock_) {
            skipMessageBlock();
        }
    }
}


INLINE bool TickCursorImpl::readUntilCursorId()
{
    // Ignore all messages until CursorId

    // No synchronization here, should be only called from select
    mainState_->isResetInProgress_ = true;
    while (readNextRecord()) {
        if (cursorId_ != -1) {
            mainState_->isResetInProgress_ = false;
            return true;
        }
        assert(!isWithinMessageBlock_);
    }

    mainState_->isResetInProgress_ = false;
    return false;
}


INLINE bool TickCursorImpl::next(MessageHeader* const msg, bool nonBlocking)
{
    assert(!isInNext_ && "C++ - level TickCursor doesn't support concurrent calls");
    isInNext_ = true;
    try {
        assert(NULL != reader_);
        if (state >= CursorState::END) {
            if (isInterrupted_) {
                THROW_DBGLOG_EX(TickCursorInterruptedException, LOGHDR ".readNext(): interrupted", ID);
                msg->cursorState = state;
                readRemainderAndClose(true);
                return isInNext_ = false;
            }

            if (state > CursorState::END || (!mainState_->isExecutingSubscriptions() && 0 == reader_->nBytesAvailable() && 0 == activeInputStream_->nBytesAvailable())) {
                msg->cursorState = state;
                return isInNext_ = false;
            }

            msg->cursorState = setStateLocked(CursorState::STARTED);
        }

        if (nonBlocking) {
            if (isWithinMessageBlock_ && !readMessageDone_) {
                reader_->nextMessage();
                readMessageDone_ = true;
            }

            if (reader_->nBytesAvailableInStream() <= 0 && activeInputStream_->nBytesAvailable() <= 0) {
                return isInNext_ = false;
            }

            readMessageDone_ = false;
        }

        assert(state < CursorState::END);
        bool result = nonBlocking ? readNextMessageNonBlock(*msg) : readNextMessage(*msg);
        isInNext_ = false;
        return result;
    }

    catch (const IOStreamDisconnectException&) {
        DBGLOG(LOGHDR ".next(): Server disconnected", ID);
        msg->cursorState = setState(CursorState::END);
        doClose();
        isInNext_ = false;
        finalStats();
        return false;
    }

    catch (const IOStreamException& e) {
        DBGLOG(LOGHDR ".next(): I/O exception was unexpected: %s", ID, e.what());
        msg->cursorState = setState(CursorState::END);
        doClose();
        isInNext_ = false;
        throw;
    }

    catch (const std::exception& e) {
        DBGLOG(LOGHDR ".next(): std::exception: %s", ID, e.what());
        msg->cursorState = setState(CursorState::END);
        doClose();
        isInNext_ = false;
        throw;
    }

    catch (...) {
        msg->cursorState = setState(CursorState::END);
        isInNext_ = false;
        throw;
    }

    // Unreachable
    dx_assert(false, "This code should be unreachable!");
    msg->cursorState = setState(CursorState::END);
    isInNext_ = false;
    // No return
}


bool TickCursorImpl::allocateInputBuffer()
{
    // Allocate buffer (currently using constant size)
    void * bufPtr = malloc(READ_BUFFER_ALLOC_SIZE);
    bufferAllocPtr_ = bufPtr;

    if (NULL == bufPtr)
        return false;

    // Align to 4k page boundary and add 8-byte front guard area
    bufPtr = bufferStart_ = (byte *)DX_POINTER_ALIGN_UP(bufPtr, 0x1000) + 8;
    return true;
}


//TickCursor& TickCursorImpl::select(const TickStream &stream, Timestamp time, vector<string> types, vector<InstrumentIdentity> entities/*, IOStream * ioStream*/)
//TickCursor& TickCursorImpl::select(const vector<TickStream> &streams, Timestamp time, const vector<string> &types, const vector<InstrumentIdentity> &entities);

void dbg_log_xml(const char * name, const char * s)
{
    FILE * f = NULL;
    static bool opened = false;
    // TODO: Switch to decent logger
    forn(i, 5) {
        f = fopen(dbg_logfile_path("select", "xml").c_str(), opened ? "a+t" : "wt");
        if (NULL != f) {
            break;
        }
        spin_wait_ms(20);
    }

    opened = true;

    if (NULL != f) {
        fprintf(f, "<!-- %s -->\n%s\n", name, s);
        fclose(f);
    }
}


enum LogItem {
    LOG_REQUEST,
    LOG_RESP_HEADER,
    LOG_RESP_BODY
};

void TickCursorImpl::log_select(const string &xml, int what, bool isError)
{
#if (DBG_LOG_ENABLED == 1) && (XML_LOG_LEVEL > 0)
#if !defined(_DEBUG)
    if (!isError) {
        return;
    }
    
#endif

    SubscriptionState &s = *mainState_;
    const char * str = xml.c_str();

#if XML_LOG_LEVEL < 2 || VERBOSE_CURSOR < 1
    if (!isError)
        return;
#endif


    switch ((enum LogItem)what) {
    case LOG_REQUEST:
        if (s.nStreams() > 0) {
            dbg_log_xml(this->textId().append(" - select ").c_str(), str);
        }
        else if (options.qql.is_set()) {
            dbg_log_xml(this->textId().append(" - selectQQL " + options.qql.get()).c_str(), str);
        }
        else {
            dbg_log_xml(this->textId().append(" - select without streams&qql").c_str(), str);
        }

        break;

    case LOG_RESP_HEADER:
        dbg_log_xml(this->textId().append(" - response header").c_str(), str);
        break;

    case LOG_RESP_BODY:
        dbg_log_xml(this->textId().append(!isError ? " - response " : " - response (error)").c_str(), str);
        break;
    }

#endif // #if (DBG_LOG_ENABLED == 1) && (XML_LOG_LEVEL > 0)
}



TickCursor& TickCursorImpl::select(const vector<string> * types, const vector<std::string> * entities)
{
    HTTP::ResponseHeader responseHeader;
    bool wasOpen = false;

    if (isOpen_ || isClosed_) {
        THROW_DBGLOG(LOGHDR ".select() : TickCursor already opened or finished!", ID);
    }

    lastTimestamp_  = TIMESTAMP_NULL;

    DX_CLEAR_NI(stats);
    DX_CLEAR_NI(isSystemMessage_);
    
    statsCheckpoint();    

    state = CursorState::END;

    synchronizeCheck();
    SubscriptionState &s = *mainState_;

    const char * selectTagName = options.qql.is_set() ? "selectQQL" : "select";

    XmlGen::XmlOutput xml;
    XmlRequest::begin(&xml, selectTagName);

    assert(s.tickdbStreams_.size() >= 1);
    xml.addItemArray("streams", s.tickdbStreams_.data() + 1, s.nStreams());

    if (dbg_log_enabled) {
        string list;
        const auto &streams = s.tickdbStreams_;
        forn (i, streams.size() - 1) {
            list.append(" ");
            list.append(impl(streams[i + 1])->key());
        }

        DBGLOG("TickCursor.select() for streams: %s", list.c_str());
    }

#if VERBOSE_CURSOR_FILTERS >= 1 || VERBOSE_CURSOR >= 1
    DBGLOG(LOGHDR ".select(%s: S:[%s], T:[%s], I:[%s])", ID, TS_CURSOR2STR(options.from),
        subscriptionToString(s.tickdbStreams_.data() + 1, s.nStreams()).c_str(),
        subscriptionToString(types).c_str(), subscriptionToString(entities).c_str()
        //toString(NULL == types ? NULL : types->data(), NULL == types ? 0 : types->size()).c_str(),
        //toString(NULL == entities ? NULL : entities->data(), NULL == entities ? 0 : entities->size()).c_str()
        );
#endif

    xml.addItemArray("symbols", entities);

    // Message types
    xml.addItemArray("types", types);

    xml << options;
    xml.closeTag(selectTagName).add('\n');

    log_select(xml.str(), LOG_REQUEST, false);

    try {
        IOStream *ioStream_;
        {
            string req_str = xml.str();
            ioStream_ = db_.sendTbPostRequest(&req_str, HTTP::ContentType::XML, HTTP::ContentLength::FIXED, options.live || options.minLatency ? TCP::NO_DELAY : TCP::NORMAL, true);
        }

        activeInputStream_ = ioStream_;

        DBGLOG_VERBOSE(LOGHDR ".select(): Create DataReader", ID);

        // Create reader and try to read response
        //DataReaderBaseImpl * reader = new DataReaderBaseImpl((byte *)buffer, READ_BUFFER_SIZE, 0, ioStream);

        if (!allocateInputBuffer()) {
            DBGLOGERR((string*)NULL, LOGHDR ".select(): Unable to allocate memory for input buffer", ID);
            throw std::bad_alloc();
        }

        DataReaderBaseImpl * reader = new DataReaderMsg(inputBuffer(), inputBufferSize(), 0, ioStream_);
        this->reader_ = reader;
        
        // Read header, byte after byte until we have empty line
        // We read indefinitely until socket is closed or empty line is received
        DBGLOG(LOGHDR ".select(): Start reading response", ID);
        unsigned httpResponseCode = HTTP::readResponseHeader(responseHeader, *reader);

        log_select(responseHeader, LOG_RESP_HEADER, false);

        // Response string now contains the server response header
        intptr contentLength = responseHeader.parseContentLength();
        string responseBody;

        if (!HttpSuccess(httpResponseCode)) {
            switch (httpResponseCode) {
            case 401:
                DBGLOG(LOGHDR ".executeRequest(%s): 401 Unauthorized", ID, selectTagName);
                break;
            }

            dbg_log_xml("ERROR !!!!", " ");
            log_select(xml.str(), LOG_REQUEST, true);
            log_select(responseHeader, LOG_RESP_HEADER, true);
            

            if (HTTP::readResponseBody(responseBody, *reader, contentLength) && responseBody.length()) {
                HTTP::tryExtractTomcatResponseMessage(responseBody);
            }
            else {
                // If not able to read response body, copy http error text there
                responseBody = responseHeader.responseText();
            }

            log_select(responseBody, LOG_RESP_BODY, true);
            DBGLOG(LOGHDR ".select() request failed: %s", ID, responseBody.c_str());
            
            string unescaped;
            XmlParse::unescape(unescaped, responseBody);
            responseBody = move(unescaped);

            throw XmlErrorResponseException(httpResponseCode, responseBody);
        }

        if (-1 == contentLength) {
            DBGLOG_VERBOSE(LOGHDR ".select(): Chunked encoding", ID);
            // Chunked encoding, switch reader object
            reader = new DataReaderMsgHTTPchunked(move(*reader_));
            delete this->reader_;
            this->reader_ = reader;
        }
        else {
            DBGLOG_VERBOSE(LOGHDR ".select(): No chunked encoding", ID);
        }

        DBGLOG_VERBOSE(LOGHDR ".select(): reading Cursor Id", ID);

        isClosed_ = isInNext_ = false;
        s.isResetInProgress_ = false;
        state = CursorState::STARTED;

        cursorId_ = -1;         // No cursor id
        s.lastExpectedCommandId_ = s.firstExpectedCommandId_ = -1;    // No command id
        
        wasOpen = true;

        // Clear message filters. Do not skip anything at this point
        s.clearMessageFilters();

        
        readUntilCursorId();
        // Register in deallocation pool
        db_.add(this);
        isOpen_ = true;
        DBGLOG_VERBOSE(LOGHDR ".select(): cursor opened succesfully", ID);
        return *this;
    }

    catch (const IOStreamDisconnectException &) {
        // If was disconnected while still reading response from server, return error
        // If just empty response, set isAtEnd (TODO: KLUDGE: not compatible with realtime mode?)
        if (wasOpen) {
            DBGLOG(LOGHDR ".select(): Connection closed while reading cursor Id", ID);
        }
        else {
            DBGLOG(LOGHDR ".select(): response with empty payload received", ID);
        }

        doClose();
        throw;
    }

    catch (const IOStreamException& e) {
        DBGLOG(LOGHDR ".select(): I/O exception was unexpected: %s", ID, e.what());
        doClose();
        throw;
    }

    catch (const XmlErrorResponseException &) {
        doClose();
        throw;
    }

    catch (const std::exception &e) {
        DBGLOG(LOGHDR ".select(): std::exception: %s", ID, e.what());
        doClose();
        throw;
    }

    catch (...) {
        DBGLOG(LOGHDR ".select(): ERROR: Unknown exception!%s", ID);
        doClose();
        throw;
    }
}


INLINE size_t TickCursorImpl::preload(size_t nBytesToPreload, size_t readBlockSize)
{
    if (NULL == preloaderInputStream_) {
        preloaderInputStream_ = new InputStreamPreloader(activeInputStream_, readBlockSize);
        activeInputStream_ = preloaderInputStream_;
        DataReaderBaseImpl *r = static_cast<DataReaderBaseImpl *>(reader_);
        r->setInputStream(preloaderInputStream_);
    }

    InputStreamPreloader *preloader = static_cast<InputStreamPreloader *>(preloaderInputStream_);
    return preloader->preload(nBytesToPreload);
}


void TickCursorImpl::sendCloseRequest()
{
    CloseRequest req(db_, cursorId_, 0);
    req.noException = true;
    req.execute();
    DBGLOG(LOGHDR ": closeRequest completed", ID);
}

// When this method is called, input stream is supposed to be already broken
void TickCursorImpl::doClose()
{
    state = CursorState::END;
    isWithinMessageBlock_ = false;
    isClosed_ = true;

    if (NULL != reader_) {
        reader_->invalidate();
    }

    closeConnection();
    
    db_.remove(this);
    isOpen_ = false;
}


void TickCursorImpl::readRemainderAndClose(bool fromMainThread)
{
    yield_lock section(disconnectionLock_);
    if (isOpen_) {
        if (fromMainThread) {
            DBGLOG_VERBOSE(LOGHDR ".next(): closing connection", ID);
        }
        else {
            if (isInNext_) {
                DBGLOG(LOGHDR ".close(): ERROR: client is stuck inside next(), but will try to close. Bad things may happen.", ID);
            }
            else {
                DBGLOG_VERBOSE(LOGHDR ".close(): client is not calling next(), closing", ID);
            }
        }

        // server wants me to read some arbitrary amount of remaining data, or it complains exception to log
        reader_->resetMessageSize();
        while (1) reader_->getByte();
        //while (1) printf(" %02X", reader_->getByte());

        // but I'll try reading and skipping proper data blocks
        //skipUntilEOF();
    }

    doClose();
}

void TickCursorImpl::close(bool shouldSendCloseRequest, bool noThrow)
{
    yield_lock section(closeLock_);
    if (isOpen_)
    {
        try {
            DBGLOG_VERBOSE(LOGHDR ".close(): attempting to close", ID);
            // First, we set "stream end flag", break the connection

            if (isInNext_) {
                DBGLOG_VERBOSE(LOGHDR ".close(): client is inside Next()", ID);
            }

            interruptedTimeout_ = false;
            isInterrupted_ = true;
            setState(CursorState::END);

            if (shouldSendCloseRequest) {
                sendCloseRequest();
            }

            forn(i, CURSOR_STOPPING_WAIT_MAX_MS / CURSOR_STOPPING_WAIT_MIN_MS) {
                if (!isOpen_) {
                    goto closed;
                }

                this_thread::sleep_for(chrono::milliseconds(CURSOR_STOPPING_WAIT_MIN_MS));
                if (!isInNext_) {
                    break;
                }
            }

            interruptedTimeout_ = true;
            this_thread::sleep_for(chrono::milliseconds(CURSOR_STOPPING_WAIT_MIN_MS));

            // Still open?

            readRemainderAndClose(false);

        closed:
            DBGLOG_VERBOSE(LOGHDR ".close(): closed", ID);
        }

        catch (const IOStreamDisconnectException&) {
            DBGLOG(LOGHDR ".close(): Server disconnected", ID);
            doClose();
        }

        catch (const IOStreamException& e) {
            DBGLOG(LOGHDR ".close(): I/O exception was unexpected: %s%s", ID, e.what(), noThrow ? " (not rethrown)" : "");
            doClose();
            if (!noThrow)
                throw;
        }

        catch (const std::exception &e) {
            DBGLOG(LOGHDR ".close(): std::exception: %s%s", ID, e.what(), noThrow ? " (not rethrown)" : "");
            doClose();
            if (!noThrow)
                throw;
        }

        catch (...) {
            DBGLOG(LOGHDR ".close(): ERROR: Unknowns exception!%s", ID, noThrow ? " (not rethrown)" : "");
            doClose();
            if (!noThrow)
                throw;
        }
    }
    else {
        DBGLOG_VERBOSE(LOGHDR ".close(): already closed", ID);
    }
}

// API functions definition.


bool TickCursor::next(MessageHeader* const msg)
{
    SR_LOG_READ(bool, Next)
    return THIS_IMPL->next(msg, false);
}

bool TickCursor::nextIfAvailable(MessageHeader* const msg) {
    SR_LOG_READ(bool, Next)
    return THIS_IMPL->next(msg, true);
}

bool TickCursor::isAtEnd() const
{
    return THIS_IMPL->state >= CursorState::END;
}


bool TickCursor::isClosed() const
{
    return !THIS_IMPL->isOpen_;
}


void TickCursor::close()
{
    return THIS_IMPL->close(true, false);
}


TickCursorImpl::SubscriptionState& TickCursorImpl::getWriteShadowState()
{
    SubscriptionState *s;
    if (mainState_ == shadowState_)
    {
        s = shadowState_ = other(mainState_);
        *s = *mainState_;           // Make full copy on write.
        shadowCopyUpdated_ = true;  // Since we made a copy anyway, there's no reason to not switch
    }
    else {
        s = shadowState_;
    }

    return *s;
}


INLINE TickCursorImpl::SubscriptionState& TickCursorImpl::getReadShadowState()
{
    return *shadowState_;
}


TickCursorImpl& TickCursorImpl::setStreams(const vector<const DxApi::TickStream *> *streams)
{
    srw_write section(thisLock_);
    SubscriptionState &s = getWriteShadowState();

    s.clearStreamsArray();

    // NULL does not mean "all streams". We do not allow to subscribe to all streams this way. Null is the same as "no streams"
    if (NULL != streams) {
        auto n = streams->size();
        s.tickdbStreams_.resize(n + 1);
        forn(i, n) {
            s.tickdbStreams_[i + 1] = (*streams)[i];
        }
    }
    
    return *this;
}


size_t          TickCursor::preload(size_t nBytesToPreload, size_t readBlockSize)
{
    return THIS_IMPL->preload(nBytesToPreload, readBlockSize);
}


INLINE unsigned TickCursorImpl::registerUserMessageType(unsigned newTypeId, const string &newTypeName)
{
    srw_write section(thisLock_);
    SubscriptionState &s = getWriteShadowState();

    return s.tables_.registerLocalMessageType(newTypeId, newTypeName);
}

const std::string* TickCursorImpl::getInstrument(unsigned entityId)
{
    srw_read section(thisLock_);
    SymbolTranslator& tables = getReadShadowState().tables_;

    return tables.isRegisteredInstrumentId(entityId) ? &tables.getInstrument(entityId) : NULL;
}


// retrieves name for remote type. Will only return NULL if the server error occured or the message is not server-originated 
const string* TickCursorImpl::getMessageTypeName(unsigned msgDescId)
{
    srw_read section(thisLock_);
    SubscriptionState &s = getReadShadowState();

    return s.tables_.getMessageTypeName(msgDescId);
}


const string* TickCursorImpl::getMessageStreamKey(intptr_t streamId)
{
    if (emptyStreamId == streamId)
        return NULL;

    // TODO: Switch to BASE = -1

    srw_read section(thisLock_);
    SubscriptionState &s = getReadShadowState();

    return s.tables_.streamKeys_.get(streamId);
}


const string* TickCursorImpl::getMessageSchema(unsigned msgDescId)
{
    srw_read section(thisLock_);
    SubscriptionState &s = getReadShadowState();

    return s.tables_.getMessageSchema(msgDescId);
}


unsigned TickCursor::registerMessageType(unsigned newTypeId, const string &newTypeName)
{
    DBGLOG_VERBOSE(LOGHDR ": registering type %s=%u", THIS_IMPL->textId().c_str(), newTypeName.c_str(), newTypeId);
    return THIS_IMPL->registerUserMessageType(newTypeId, newTypeName);
}


const string* TickCursor::getInstrument(unsigned entityId)
{
    return THIS_IMPL->getInstrument(entityId);
}

// retrieves name for remote type. Will only return NULL if the server error occured or the message is not server-originated 
const string* TickCursor::getMessageTypeName(unsigned msgDescId)
{
    return THIS_IMPL->getMessageTypeName(msgDescId);
}


//const string * TickCursor::getMessageTypeName(unsigned msgDescId, unsigned streamId)
//{
//    return THIS_IMPL->getMessageTypeName(msgDescId);
//}


const string* TickCursor::getMessageStreamKey(unsigned streamId)
{
    return THIS_IMPL->getMessageStreamKey(streamId);
}


const string* TickCursor::getMessageSchema(unsigned msgDescId)
{
    return THIS_IMPL->getMessageSchema(msgDescId);
}


INLINE uint64 TickCursorImpl::nBytesReceived() const
{
    return activeInputStream_->nBytesRead();
}


INLINE uint64 TickCursorImpl::nBytesRead() const
{
    return nBytesReceived() - reader_->nBytesAvailable();
}


uint64 TickCursor::nBytesReceived() const
{
    return THIS_IMPL->nBytesReceived();
}


uint64 TickCursor::nBytesRead() const
{
    return THIS_IMPL->nBytesRead();
}


uint64 TickCursorImpl::nMessageBytesRead() const
{
    
    return nBytesRead() - nNonMessageBytesRead_;
}


uint64 TickCursor::nMessageBytesRead() const
{
    THROW_DBGLOG("Not implemented");
    //return THIS_IMPL->nMessageBytesRead();
}


uint64 TickCursor::nMessagesReceived() const
{
    THROW_DBGLOG("Not implemented");
}


uint64 TickCursor::nMessagesRead() const
{
    THROW_DBGLOG("Not implemented");
}


uint64 TickCursor::nMessagesSkipped() const
{
    THROW_DBGLOG("Not implemented");
}


DataReader& TickCursor::getReader() const
{
    return *(THIS_IMPL->reader_);
}


TickDb& TickCursor::getDB() const
{
    return static_cast<TickDb&>(THIS_IMPL->db_);
}


void TickCursorImpl::dbgBreakConnection(bool input, bool output)
{
    auto io = activeInputStream_;
    DBGLOG(LOGHDR ".breakConnection()", ID);
    if (NULL == io) {
        return;
    }

    if (input && output) {
        io->close();
    }
    else if (input) {
        io->closeInput();
    }

}


// TODO: Remove on next cleanup
#if 0
INLINE bool TickCursorImpl::onIOStreamDisconnected(const char * string)
{
    bool retval = false;
    DBGLOG(LOGHDR ".onDisconnected(): STARTED", ID);
    //db_->cursorDisconnected(this);
    // TODO: Change state of the cursor to "closed, wait for external "close" call or for the command from tickdb to unlock the cursor
    // TODO: either block insize dbImpl method, or via flag, set externally by dbImpl. Probably with timeout for extra safety

    DBGLOG(LOGHDR ".onDisconnected(): FINISHED", ID);
    return retval;
}


bool TickCursorImpl::onIOStreamDisconnectedCallback(TickCursorImpl * self, const char * string)
{
    return self->onIOStreamDisconnected(string);
}
#endif