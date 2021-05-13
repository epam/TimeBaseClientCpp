#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "http/tickdb_http.h"
#include "tickstream_impl.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace dx_thread;


#define IMPL (impl(this))
#define THIS_IMPL IMPL

#define LOGHDR "%s"
#define ID (this->textId())

#if VERBOSE_TICKDB >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

// TODO: add ID field to request handlers

// Tick Stream contains information about the particular stream and reference to database object used to create it
// It lives in cache (TickStreamCache)


// Public API operations:
INLINE TickCursor * TickStreamImpl::select(TimestampMs time, const SelectionOptions &options, const std::vector<std::string> * types, const std::vector<std::string> * entities) const
{
    vector<const TickStream *> streams;
    streams.push_back(this);
    return db_.select(time, &streams, options, types, entities);
}


INLINE TickCursor * TickStreamImpl::createCursor(const SelectionOptions &options) const
{
    return db_.createCursor(this, options);
}


INLINE TickLoader * TickStreamImpl::createLoader(const LoadingOptions &options) const
{
    return db_.createLoader(this, options);
}

INLINE vector<std::string> TickStreamImpl::listEntities() const
{
    vector<std::string> out;
    DBGLOG(LOGHDR ".listEntities(..)", ID);
    if (!impl(db_).stream_listEntities(this, out)) {
        THROW_DBGLOG(LOGHDR ".listEntities(): failed!", ID);
    }

    return out;
}


bool TickStreamImpl::clear(const vector<std::string> * const entities) const
{
    DBGLOG(LOGHDR ".clear(..)", ID);
    if (!impl(db_).stream_clear(this, entities)) {
        THROW_DBGLOG(LOGHDR ".clear(): failed!", ID);
    }

    return true;
}


bool TickStreamImpl::clear(const std::vector<std::string> &entities) const
{
    return clear(&entities);
}


bool TickStreamImpl::truncate(TimestampMs millisecondTime, const vector<std::string> * const entities) const
{
    DBGLOG(LOGHDR ".truncate(%lld, ..)", ID, (longlong)millisecondTime);
    if (!impl(db_).stream_truncate(this, millisecondTime, entities)) {
        THROW_DBGLOG(LOGHDR ".truncate(): failed!", ID);
    }

    return true;
}


bool TickStreamImpl::truncate(TimestampMs millisecondTime, const std::vector<std::string> &entities) const
{
    return truncate(millisecondTime, &entities);
}


bool TickStreamImpl::purge(TimestampMs millisecondTime) const
{
    DBGLOG(LOGHDR ".purge(%lld)", ID, (longlong)millisecondTime);
    if (!impl(db_).stream_purge(this, millisecondTime)) {
        THROW_DBGLOG(LOGHDR ".purge(%ll ms): failed!", ID, millisecondTime);
    }

    return true;
}


bool TickStreamImpl::deleteStream()
{
    DBGLOG(LOGHDR ".deleteStream()", ID);
    
    if (!impl(db_).stream_delete(this)) {
        THROW_DBGLOG(LOGHDR ".deleteStream(): failed!", ID);
    }

    return true;
}


bool TickStreamImpl::getBackgroundProcessInfo(BackgroundProcessInfo *to) const
{
    DBGLOG(LOGHDR ".getBackgroundProcessInfo()", ID);

    if (!impl(db_).stream_getBackgroundProcessInfo(this, to)) {
        THROW_DBGLOG(LOGHDR ".getBackgroundProcessInfo(): failed!", ID);
    }

    return true;
}


bool TickStreamImpl::abortBackgroundProcess() const
{
    DBGLOG(LOGHDR ".abortBackgroundProcess()", ID);

    if (!impl(db_).stream_abortBackgroundProcess(this)) {
        THROW_DBGLOG(LOGHDR ".abortBackgroundProcess(): failed!", ID);
    }

    return true;
}


bool TickStreamImpl::setSchema(bool isPolymorphic, const Nullable<string> &schema)
{
    DBGLOG(LOGHDR ".setSchema(polymorphic: %d, ..)", ID, (int)isPolymorphic);

    if (!impl(db_).stream_setSchema(this, isPolymorphic, schema)) {
        THROW_DBGLOG(LOGHDR ".setSchema(): failed!", ID);
    }

    return true;
}


bool TickStreamImpl::setSchema(const StreamOptions & options)
{
    return setSchema(options.polymorphic, options.metadata);
}


bool TickStreamImpl::updateLocalSchema(const StreamOptions &options)
{
    if (options.metadata == options_.metadata && options_.polymorphic == options.polymorphic) {
        return false;
    }

    options_.metadata = options.metadata;
    options_.polymorphic = options.polymorphic;
    return true;
}


bool TickStreamImpl::changeSchema(const SchemaChangeTask &task) const
{
    DBGLOG(LOGHDR ".changeSchema()", ID);
    if (!impl(db_).stream_changeSchema(this, task)) {
        THROW_DBGLOG(LOGHDR ".changeSchema() failed!", ID);
    }

    return true;
}


bool TickStreamImpl::getTimeRange(int64_t range[], const vector<std::string> * const entities) const
{
    if (NULL == range) {
        string msg;
        DBGLOGERR(&msg, LOGHDR ".getTimeRange(): ERROR: range[] == [NULL]", ID);
        throw invalid_argument(msg);
    }

    if (!impl(db_).stream_getTimeRange(this, range, entities)) {
        THROW_DBGLOG(LOGHDR ".getTimeRange() failed!", ID);
    }

    return true;
}


bool TickStreamImpl::getPeriodicity(Interval *interval) const
{
    // TODO: cache periodicity
    if (NULL == interval) {
        string msg;
        DBGLOGERR(&msg, LOGHDR ".getPeriodicity(): ERROR: interval == NULL", ID);
        throw invalid_argument(msg);
    }

    if (!impl(db_).stream_getPeriodicity(this, interval)) {
        THROW_DBGLOG(LOGHDR ".getPeriodicity(): failed!", ID);
    }

    return true;
}


LockType TickStreamImpl::lock(LockType lockType, TimestampMs timeoutMs)
{
    srw_write section(thisLock_);
    
    if (LockType::NO_LOCK == lockType) {
        THROW_DBGLOG(LOGHDR ".lock(): ERROR: should be called with lockType != NO_LOCK!", ID);
    }

    if (LockType::NO_LOCK != dbLock_.type) {
        if (dbLock_.type != lockType) {
            THROW_DBGLOG(LOGHDR ".lock(): Trying to lock with a different type (%s instead of %s)",
                ID, lockType.toString(), dbLock_.type.toString());
        }

        DBGLOG_VERBOSE(LOGHDR ".lock(%s, %lld): retain count: %llu -> %llu", ID, lockType.toString(), (longlong)timeoutMs, dbLock_.count, dbLock_.count + 1);
        ++dbLock_.count;
    }
    else {
        DBGLOG_VERBOSE(LOGHDR ".lock(%s, %lld): sending request", ID, lockType.toString(), (longlong)timeoutMs);
        if (impl(db_).stream_lockStream(this, LockType::WRITE_LOCK == lockType, timeoutMs, &dbLock_.token)) {
            dbLock_.count = 1;
            dbLock_.type = lockType;
            DBGLOG_VERBOSE(LOGHDR ".lock(%s, %lld): locked", ID, lockType.toString(), (longlong)timeoutMs);
        }
    }

    return dbLock_.type;
}


LockType TickStreamImpl::unlock()
{
    srw_write section(thisLock_);

    if (LockType::NO_LOCK == lockType())
        return LockType::NO_LOCK;

    auto count = dbLock_.count;
    assert(0 != count);

    if (0 == --count) {
        DBGLOG_VERBOSE(LOGHDR ".unlock(): sending request", ID);
        if (impl(db_).stream_unlockStream(this, LockType::WRITE_LOCK == dbLock_.type, dbLock_.token)) {
            dbLock_.type = LockType::NO_LOCK;
            dbLock_.token.clear();
            dbLock_.count = count;
            DBGLOG(LOGHDR ".unlock(): unlocked", ID);
        }
    }
    else {
        DBGLOG_VERBOSE(LOGHDR ".unlock(): retain count: %llu -> %llu", ID, dbLock_.count, count);
        dbLock_.count = count;
    }

    return dbLock_.type;
}


bool TickStreamImpl::updateOptions(const DxApi::StreamOptions & options)
{
    if (this->options_ == options) {
#ifdef _DEBUG
        DBGLOG(LOGHDR ".updateOptions(): streamOptions match", ID);
#endif
        return false;
    }

    DBGLOG_VERBOSE(LOGHDR ".updateOptions(): streamOptions differ, updating!", ID);
    {
        srw_write lock(thisLock_);
        this->options_ = options;
        ++updateCount_;
        DXAPIIMPL_DISPATCH(onDisconnected, streamUpdatedCallback, (this), ID);
    }

    return true;
}


//bool TickStream::getPeriodicityInternal(TickDb &db, const std::string &key, Interval * interval)
//{
//    return THIS_IMPL_CONST->getPeriodicityInternal(db, key, interval);
//}


INLINE TickStream::TickStream()
{
    // Do Absolutely Nothing
    static_assert(1 == sizeof(*this), "API object size should be 1(actually 0)");
}


TickStreamImpl::DbLock::DbLock() : type(LockType::NO_LOCK), count(0)
{}

TickStreamImpl::TickStreamImpl(TickDb &db, const std::string &key, const DxApi::StreamOptions &options, uint64_t uid) :
streamUpdatedCallback(NULL), db_(db), id_(uid), key_(key), options_(options), updateCount_(1)
{
    // Assign unique text id for logging purposes
    snprintf(id_.textId, sizeof(id_.textId), "%s-TickStream-%llu[%s]", impl(db_).textId(), (ulonglong)id_.id, CharBuffer<0xE0>::shortened(key).c_str());
    id_.textId[sizeof(id_.textId) - 1] = '\0';

#ifdef _DEBUG
    DBGLOG("Tickstream impl. (%s=" FMT_HEX64 ", this=" FMT_HEX64 "), '%s' created", impl(db).textId(), (ulonglong)&db, (ulonglong)this, key.c_str());
#else
    DBGLOG_VERBOSE("%s Tickstream '%s' added to cache", impl(db).textId(), key.c_str());
#endif
}


#if defined(_DEBUG) && 0
TickStream::~TickStream()
{
    DBGLOG_VERBOSE("Tickstream::~TickStream(%p) - deleting TickStream view object does nothing", this);
}
#else
TickStream::~TickStream()
{}
#endif



TickStreamImpl::~TickStreamImpl()
{
    //DBGLOG_VERBOSE(LOGHDR ": Tickstream implementation object deleted", key().c_str());
}


void TickStream::operator delete(void* ptr, size_t sz)
{
    //DBGLOG_VERBOSE("TickStream::operator delete(%p, %ull) -> does nothing", ptr, (ulonglong)sz);
}


void TickStreamImpl::operator delete(void* ptr, size_t sz)
{
    //DBGLOG_VERBOSE("TickStreamImpl::operator delete(%p, %ull)", ptr, (ulonglong)sz);
    ::operator delete(ptr);
}

/*************************************************************************
* TickStream implementation via proxy methods
*/


const std::string& DxApi::TickStream::key() const
{
    return IMPL->key();
}


int32_t TickStream::distributionFactor() const
{
    return IMPL->distributionFactor();
}


const DxApi::Nullable<std::string>& TickStream::name() const
{
    return IMPL->name();
}


const DxApi::Nullable<std::string>& TickStream::description() const
{
    return IMPL->description();
}


const DxApi::Nullable<std::string>& TickStream::owner() const
{
    return IMPL->owner();
}


const DxApi::Nullable<std::string>& TickStream::location() const
{
    return IMPL->location();
}


const DxApi::Nullable<std::string>& TickStream::distributionRuleName() const
{
    return IMPL->distributionRuleName();
}


DxApi::StreamScope  TickStream::scope() const
{
    return IMPL->scope();
}

bool TickStream::duplicatesAllowed() const
{
    return IMPL->duplicatesAllowed();
}


bool TickStream::highAvailability() const
{
    return IMPL->highAvailability();
}


bool TickStream::unique() const
{
    return IMPL->unique();
}


bool TickStream::polymorphic() const
{
    return IMPL->polymorphic();
}


// TODO: Change representation
const std::string& TickStream::periodicity() const
{
    return IMPL->periodicity();
}


const DxApi::Nullable<std::string>& TickStream::metadata() const
{
    return IMPL->metadata();
}


const StreamOptions&  TickStream::options() const
{
    return IMPL->options();
}


bool TickStream::setSchema(const DxApi::StreamOptions &options)
{
    return IMPL->setSchema(options);
}


TickCursor * TickStream::select(TimestampMs time, const SelectionOptions &options, const std::vector<std::string> * types, const std::vector<std::string> * entities) const
{
    return IMPL->select(time, options, types, entities);
}


TickCursor * TickStream::createCursor(const SelectionOptions &options) const
{
    return IMPL->createCursor(options);
}


TickLoader * TickStream::createLoader(const LoadingOptions &options) const
{
    return IMPL->createLoader(options);
}


vector<std::string> TickStream::listEntities() const
{
    return IMPL->listEntities();
}


bool TickStream::getTimeRange(int64_t range[], const std::vector<std::string> * const entities) const
{
    return IMPL->TickStreamImpl::getTimeRange(range, entities);
}


bool TickStream::getTimeRange(int64_t range[], const std::vector<std::string> &entities) const
{
    return TickStream::getTimeRange(range, &entities);
}


bool TickStream::clear(const vector<std::string> * const entities) const
{
    return IMPL->clear(entities);
}


bool TickStream::clear(const std::vector<std::string> &entities) const
{
    return clear(&entities);
}


bool TickStream::truncate(TimestampMs millisecondTime, const vector<std::string> * const entities) const
{
    return IMPL->truncate(millisecondTime, entities);
}


bool TickStream::truncate(TimestampMs millisecondTime, const std::vector<std::string> &entities) const
{
    return truncate(millisecondTime, &entities);
}


bool TickStream::purge(TimestampMs millisecondTime) const
{
    return IMPL->purge(millisecondTime);
}


bool TickStream::deleteStream()
{
    return IMPL->deleteStream();
}


bool TickStream::abortBackgroundProcess() const
{
    return IMPL->abortBackgroundProcess();
}


bool TickStream::getPeriodicity(Interval * interval) const
{
    return IMPL->getPeriodicity(interval);
}


