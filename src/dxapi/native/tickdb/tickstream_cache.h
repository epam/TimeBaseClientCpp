#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "util/srw_lock.h"
#include <unordered_map>


namespace DxApiImpl {

    class TickDbImpl;
    // TODO: make all operations atomic later

    struct StreamInfo {
        std::string key;
        DxApi::StreamOptions options;
    };


    class TickStreamCacheInternal {
    protected:
        typedef dx_thread::srw_read srw_read;

    public:
        typedef DxApi::TickStream Stream;

        struct StreamEntry {
            DxApi::TickStream * stream;
            bool deleted;
        };

    protected:
        // READ METHODS

        size_t size() const;

        // Returns the full list of stream keys in the cache
        std::vector<std::string> allKeys();
        std::vector<Stream *> allStreams();

        // Get stream handle from cache
        //ConstElement get(const std::string &key) const;
        Stream *  get(const std::string &key) const;
        StreamEntry * getEntry(const std::string &key);


        // WRITE METHODS

        // Add stream content to cache
        //Element add(const std::string &key, const DxApi::StreamOptions options);
        void add(Stream * stream);

        // Remove stream from cache (from this point on getStream on this stream will fail)
        DxApi::TickStream * remove(const std::string &key);
        DxApi::TickStream * tryRemove(const std::string &key);

        INLINE void remove(const Stream * stream) { remove(stream->key()); }

        unsigned synchronize(DxApiImpl::TickDbImpl &db, std::vector<StreamInfo> &newStreams);

        void clear();


        void unlockAndDisposeStream(StreamEntry &stream);
        //const StreamInfo & getInfo(const std::string &key) const;

        void markAllForRemoval(bool value);

        void markForRemoval(const std::string &key);

        // Returns number of stream still marked or UINTPTR_MAX  if one of the streams is not found
        // Basically, nonzero value means streams dont match
        uintptr unmarkForRemoval(const std::vector<std::string> &streamKeys);

        unsigned removeMarked();


        TickStreamCacheInternal();
        ~TickStreamCacheInternal();
    protected:

        typedef std::unordered_map<std::string, StreamEntry> StreamMap;

        dx_thread::srw_lock thisLock_;
        StreamMap data_;
        char textId_[0x40];
        DISALLOW_COPY_AND_ASSIGN(TickStreamCacheInternal);
    }; // class TickStreamCacheInternal


    class TickStreamCache : public TickStreamCacheInternal {
    public:

        // READ METHODS

        size_t size();

        // Returns the full list of stream keys in the cache
        std::vector<std::string> allKeys();
        std::vector<Stream *> allStreams();

        // Get stream handle from cache
        //ConstElement get(const std::string &key) const;
        Stream *  get(const std::string &key);
        StreamEntry * getEntry(const std::string &key);


        // WRITE METHODS

        // Add stream content to cache
        //Element add(const std::string &key, const DxApi::StreamOptions options);
        void add(Stream * stream);

        // Remove stream from cache (from this point on getStream on this stream will fail)
        //DxApi::TickStream * remove(const std::string &key);
        //INLINE DxApi::TickStream * remove(const Stream * stream) { return remove(stream->key()); }

        DxApi::TickStream * tryRemove(const std::string &key);

        // Return true if the list of keys matched the list of contained stream key
        bool matchKeys(const std::vector<std::string> &keys);

        //unsigned synchronize(DxApiImpl::TickDbImpl &db, std::vector<StreamInfo> &newStreams);
        void clear();
        void setId(const char * id);

        TickStreamCache();
        ~TickStreamCache();

        DISALLOW_COPY_AND_ASSIGN(TickStreamCache);
    }; // class TickStreamCache


    // Get stream handle from cache
    INLINE DxApi::TickStream * TickStreamCache::get(const std::string &key)
    {
        srw_read section(thisLock_);
        return TickStreamCacheInternal::get(key);
    }


    INLINE size_t TickStreamCache::size()
    {
        srw_read section(thisLock_);
        return TickStreamCacheInternal::size();
    }

    INLINE size_t TickStreamCacheInternal::size() const { return data_.size(); }
    

    // Get stream handle from cache
    INLINE DxApi::TickStream * TickStreamCacheInternal::get(const std::string &key) const
    {
        StreamMap::const_iterator i = data_.find(key);
        if (data_.end() == i) {
            return NULL;
        }
        else {
            return i->second.stream;
        }
    }
}