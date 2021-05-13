#include "tickdb/common.h"

#include <dxapi/dxapi.h>
#include "http/tickdb_http.h"
#include "tickstream_impl.h"
#include "tickstream_cache.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace dx_thread;


//DxApi::TickStream * TickStreamCache::add(const std::string &key, const DxApi::StreamOptions options)
#define LOGHDR "%s"
#define ID textId_

INLINE TickStreamCache::StreamEntry * TickStreamCache::getEntry(const std::string &key)
{
    srw_read section(thisLock_);
    return TickStreamCacheInternal::getEntry(key);
}


std::vector<std::string> TickStreamCache::allKeys()
{
    srw_read section(thisLock_);
    return TickStreamCacheInternal::allKeys();
}


std::vector<TickStreamCache::Stream *> TickStreamCache::allStreams()
{
    srw_read section(thisLock_);

    return TickStreamCacheInternal::allStreams();
}


void TickStreamCache::add(Stream * stream)
{
    srw_write section(thisLock_);

    TickStreamCacheInternal::add(stream);
}


// Remove stream from cache (from this point on getStream on this stream will fail)
//TickStream * TickStreamCache::remove(const std::string &key)
//{
//    srw_write section(thisLock_);
//
//    return TickStreamCacheInternal::remove(key);
//}


TickStream * TickStreamCache::tryRemove(const std::string &key)
{
    srw_write section(thisLock_);
    return TickStreamCacheInternal::tryRemove(key);
}


// Remove stream from cache (from this point on getStream on this stream will fail)
void TickStreamCache::clear()
{
    srw_write section(thisLock_);
    TickStreamCacheInternal::clear();
}


bool TickStreamCache::matchKeys(const std::vector<std::string> &keys)
{
    srw_write section(thisLock_);

    // Mark all streams as deleted, later "undelete" all updated or added
    TickStreamCacheInternal::markAllForRemoval(true);

    return 0 == TickStreamCacheInternal::unmarkForRemoval(keys);
}


//unsigned TickStreamCache::synchronize(TickDbImpl &db, std::vector<StreamInfo> &newStreams)
//{
//    srw_write section(thisLock_);
//
//    return TickStreamCacheInternal::synchronize(db, newStreams);
//}


TickStreamCache::TickStreamCache()
{
    // Do nothing
}


TickStreamCache::~TickStreamCache()
{
    clear();
    // TODO: Close all locks
}


void TickStreamCache::setId(const char * id)
{
    size_t l;
    memcpy(textId_, id, l = std::min(sizeof(textId_) - 20, strlen_ni(id)));
    textId_[l] = '\0';
    strcat(textId_, ".tickStreamCache");
}


// Get stream handle from cache
INLINE TickStreamCache::StreamEntry * TickStreamCacheInternal::getEntry(const std::string &key)
{
    StreamMap::iterator i = data_.find(key);
    if (data_.end() == i) {
        return NULL;
    }
    else {
        return &(i->second);
    }
}


void TickStreamCacheInternal::clear()
{
    for (auto p : data_) {
        StreamEntry &s = p.second;
        unlockAndDisposeStream(s);
    }

    data_.clear();
}


// Remove stream from cache (from this point on getStream on this stream will fail)
DxApi::TickStream * TickStreamCacheInternal::remove(const std::string &key)
{
    auto found = TickStreamCacheInternal::get(key);
    if (NULL == found) {
        THROW_DBGLOG(LOGHDR ".remove(): ERR: Stream not found: %s", ID, key.c_str());
    }

    // unlock etc. not called, because the stream is assumed to be deleted
    data_.erase(key);
    return found;
}


DxApi::TickStream * TickStreamCacheInternal::tryRemove(const std::string &key)
{
    auto found = TickStreamCacheInternal::get(key);
    if (NULL != found) {
        data_.erase(key);
    }

    return found;
}


std::vector<std::string> TickStreamCacheInternal::allKeys()
{
    std::vector<std::string> keys;

    for (auto const &i : data_) {
        keys.push_back(i.first);
        assert(i.first == impl(i.second.stream)->key());
    }

    return keys;
}


std::vector<TickStreamCache::Stream *> TickStreamCacheInternal::allStreams()
{
    vector<TickStreamCache::Stream *> v;

    for (auto const &i : data_) {
        v.push_back(i.second.stream);
    }

    return v;
}


void TickStreamCacheInternal::add(Stream * stream)
{
    if (NULL == stream) {
        THROW_DBGLOG(LOGHDR ".add(): ERR: Attempting to add NULL stream", ID);
    }

    const string & key = impl(stream)->key();

    if (NULL != get(key)) {
        THROW_DBGLOG(LOGHDR ".add(): ERR: Stream %s already present in the list", ID, key.c_str());
    }

    StreamEntry entry = { stream, false };
    data_[key] = entry;
}


unsigned TickStreamCacheInternal::synchronize(TickDbImpl &db, std::vector<StreamInfo> &newStreams)
{
    unsigned nChanged = 0;

    // Mark all streams as deleted, later "undelete" all updated or added
    markAllForRemoval(true);

    for (auto &s : newStreams) {
        TickStreamCache::StreamEntry * found = getEntry(s.key);
        if (NULL != found) {
            // Update Stream options. Not using timestamps for now

            impl(found->stream)->updateOptions(s.options);

            found->deleted = false;
        }
        else {
            TickStream * created = db.allocateStream(s.key, s.options);
            assert(created);
            assert(NULL == found);

            // Add it
            StreamEntry info = { created, false };
            data_[s.key] = info;

            ++nChanged;
        }
    }

    nChanged += removeMarked();

    return nChanged;
}


void TickStreamCacheInternal::markAllForRemoval(bool value)
{
    for (auto &info : data_) {
        info.second.deleted = value;
    }
}


unsigned TickStreamCacheInternal::removeMarked()
{
    std::vector<std::string> deletedStreamKeys;
    unsigned nDeleted = 0;

    for (auto const &p : data_) if (p.second.deleted) {
        deletedStreamKeys.push_back(p.first);
    }

    for (auto const &s : deletedStreamKeys) {
        remove(s);
        ++nDeleted;
    }

    // New map implementation supports deletion while iterating, can reimplement this to improve perf/code size

    return nDeleted;
}


void TickStreamCacheInternal::markForRemoval(const std::string &key)
{
    StreamEntry * entry = getEntry(key);
    if (NULL != entry) {
        entry->deleted = true;
    }
}


uintptr TickStreamCacheInternal::unmarkForRemoval(const std::vector<std::string> &streamKeys)
{
    uintptr nMarked = 0;

    for (auto const &s : streamKeys) {
        TickStreamCache::StreamEntry * found = getEntry(s);
        if (NULL != found) {
            found->deleted = false;
        }
        else {
            return UINTPTR_MAX;
        }
    }

    for (auto const &i : data_) {
        nMarked += i.second.deleted;
    }

    return nMarked;
}


// Called when stream is removed from cache
void TickStreamCacheInternal::unlockAndDisposeStream(StreamEntry &streamEntry)
{
    TickStreamImpl &s = *impl(streamEntry.stream);
    try {
        while (LockType::NO_LOCK != s.unlock()) {}
    }
    catch (const std::exception &e) {
        DBGLOG(LOGHDR ".clear(): ERR: Failed to unlock stream '%s'. exception: %s", ID, s.key().c_str(), e.what());
        // We don't rethrow the exception. Nobody cares about it downstream
    }
    catch (...) {
        DBGLOG(LOGHDR ".clear(): ERR: Failed to unlock stream '%s'. System exception", ID, s.key().c_str());
    }

    impl(s.db_).disposeStream(&s);
}


TickStreamCacheInternal::TickStreamCacheInternal()
{
    textId_[0] = '\0';
}


TickStreamCacheInternal::~TickStreamCacheInternal()
{
    size_t n = data_.size();
    if (0 != n) {
        DBGLOG(LOGHDR ".~(): ERR: cache still contains %llu streams!", ID, (ulonglong)n);
    }
}



