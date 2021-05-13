#include "tickdb/common.h"
#include <dxapi/dxapi.h>


#include "xml/stream/basic_requests.h"
#include "xml/stream/list_entities_request.h"
#include "xml/stream/timerange_request.h"
#include "xml/stream/periodicity_request.h"
#include "xml/stream/lock_stream_request.h"
#include "xml/stream/bg_process_request.h"


#include "tickdb/tickstream_impl.h"
#include "tickdb_http.h"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;


#define CHECK_OPEN(msg) do {                                                \
    if (!isOpen()) {                                                        \
        THROW_DBGLOG(msg "() failed!: TickDb is not open!");                \
        }                                                                   \
} while (0)


#define CHECK_WRITEACCESS(msg) do {                                         \
    if (isReadOnly()) {                                                     \
        THROW_DBGLOG(msg "() failed : TickDb is opened in readonly mode");  \
        }                                                                   \
} while (0)


#define CHECK_OPEN_WRITEACCESS(msg) do {                                    \
    CHECK_OPEN(msg);                                                        \
    CHECK_WRITEACCESS(msg);                                                 \
} while (0)


// Stream lock type
const char * infoLockType[] = {
    "NO_LOCK",
    "READ",
    "WRITE"
};


IMPLEMENT_ENUM(uint8_t, LockType, false)
{}


bool TickDbImpl::stream_delete(DxApi::TickStream *stream)
{
    CHECK_OPEN_WRITEACCESS("TickStream::delete");

    DeleteStreamRequest req(impl(stream));
    if (req.execute()) {
        // TODO: switch back to stream instead of key
        // TODO: Wait for notification instead (but this version works too)
        onStreamDeleted(impl(stream)->key());
        return true;
    }
    
    return false;
}


bool TickDbImpl::stream_listEntities(const DxApi::TickStream *stream, std::vector<std::string> &out) const
{
    ListEntitiesRequest req(stream);
    return req.listEntities(out);
}


bool TickDbImpl::stream_clear(const DxApi::TickStream *stream, const std::vector<std::string> * const entities)
{
    CHECK_OPEN_WRITEACCESS("TickStream::clear");

    ClearStreamRequest req(stream, entities);
    return req.execute();
}


bool TickDbImpl::stream_truncate(const DxApi::TickStream *stream,
    TimestampMs millisecondTime, const vector<std::string> * const entities) const
{
    CHECK_OPEN_WRITEACCESS("TickStream::truncate");

    TruncateStreamRequest req(stream, millisecondTime, entities);
    return req.execute();
}


bool TickDbImpl::stream_purge(const DxApi::TickStream *stream, TimestampMs millisecondTime) const
{
    CHECK_OPEN_WRITEACCESS("TickStream::purge");

    PurgeStreamRequest req(stream, millisecondTime);
    return req.execute();
}


bool TickDbImpl::stream_getBackgroundProcessInfo(const DxApi::TickStream *stream, DxApi::BackgroundProcessInfo *to) const
{
    assert(NULL != to);
    if (NULL == to)
        return false;

    CHECK_OPEN_WRITEACCESS("TickStream::getBackgroundProcessInfo");

    GetBgProcessRequest req(stream);
    return req.getResponse(to);
}


bool TickDbImpl::stream_abortBackgroundProcess(const DxApi::TickStream *stream) const
{
    CHECK_OPEN_WRITEACCESS("TickStream::abortBackgroundProcess");

    AbortBGProcessRequest req(stream);
    return req.execute();
}


bool TickDbImpl::stream_lockStream(const DxApi::TickStream *stream, bool write, int64_t timeoutMs, std::string * const token)  const
{
    CHECK_OPEN("TickStream::lock");

    LockStreamRequest req(stream, sessionId_, write, timeoutMs);
    bool success = req.execute();
    if (success) {
        assert(token);
        if (NULL != token) {
            *token = req.id();
        }
    }

    return success;
}


bool TickDbImpl::stream_unlockStream(const DxApi::TickStream *stream, bool write, const std::string &token) const
{
    CHECK_OPEN("TickStream::unlock");

    UnlockStreamRequest req(stream, token, write);
    return req.execute();
}


bool TickDbImpl::stream_getTimeRange(const DxApi::TickStream *stream, int64_t range[],
    const std::vector<std::string> * const entities) const
{
    assert(NULL != range);
    TimerangeRequest req(stream, entities);
    return req.getTimerange(range);
}


bool TickDbImpl::stream_getPeriodicity(const DxApi::TickStream *stream, DxApi::Interval *interval) const
{
    // TODO: cache periodicity
    assert(NULL != interval);
    PeriodicityRequest req(stream);
    return req.getPeriodicity(interval);
}


bool TickDbImpl::stream_setSchema(const DxApi::TickStream *stream, bool isPolymorphic, const DxApi::Nullable<std::string> &schema)
{
    CHECK_OPEN_WRITEACCESS("TickStream::setSchema");

    SetSchemaRequest req(stream, isPolymorphic, schema);
    bool result = req.execute();
    sessionHandler_.getStreamSynchronous(stream->key());
    return result;
}


bool TickDbImpl::stream_changeSchema(const DxApi::TickStream *stream, const DxApi::SchemaChangeTask &task)
{
    CHECK_OPEN_WRITEACCESS("TickStream::changeSchema");

    ChangeSchemaRequest req(stream, task);
    bool result = req.execute();
    if (!task.background) {
        sessionHandler_.getStreamSynchronous(stream->key());
    }

    return result;
}
