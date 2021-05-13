#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "tickdb/http/tickdb_http.h"


// Tick Stream contains information about the particular stream and reference to database object used to create it
// It lives in cache (TickStreamCache)

namespace DxApiImpl {
    class TickStreamCache;
    class TickDbImpl;

    class TickStreamImpl : public DxApi::TickStream {
        friend class TickStreamCacheInternal;
        friend class TickDbImpl;
        friend class DxApi::TickStream;

    public:
        typedef void(DXAPI_CALLBACK * StreamUpdatedCallback)(TickStreamImpl * stream);

        StreamUpdatedCallback streamUpdatedCallback;

    public:
        // Properties (read-only)
        INLINE const std::string& key() const           { return key_; };

        INLINE const DxApi::Nullable<std::string>& name() const          { return options_.name; }
        INLINE const DxApi::Nullable<std::string>& description() const   { return options_.description; }
        INLINE const DxApi::Nullable<std::string>& owner() const         { return options_.owner; }
        INLINE const DxApi::Nullable<std::string>& location() const      { return options_.location; }

        INLINE int32_t distributionFactor() const       { return options_.distributionFactor; }
        INLINE const DxApi::Nullable<std::string>& distributionRuleName() const { return options_.distributionRuleName; }

        INLINE DxApi::StreamScope  scope() const        { return options_.scope; }
        INLINE const DxApi::StreamOptions&  options() const { return options_; }

        INLINE bool duplicatesAllowed() const           { return options_.duplicatesAllowed; }
        INLINE bool highAvailability() const            { return options_.highAvailability; }
        INLINE bool unique() const                      { return options_.unique; }
        INLINE bool polymorphic() const                 { return options_.polymorphic; }
        // TODO: Change representation
        INLINE const std::string& periodicity() const   { return options_.periodicity; }
        INLINE const DxApi::Nullable<std::string>& metadata() const      { return options_.metadata; }

        INLINE DxApi::LockType lockType() const         { return dbLock_.type; }
        INLINE const std::string& lockToken() const     { return dbLock_.token; }

        // Public API operations:
        INLINE DxApi::TickCursor * select(DxApi::TimestampMs time, const DxApi::SelectionOptions &options, const std::vector<std::string> * types, const std::vector<std::string> * const entities) const;


        INLINE DxApi::TickCursor * createCursor(const DxApi::SelectionOptions &options) const;


        INLINE DxApi::TickLoader * createLoader(const DxApi::LoadingOptions &options) const;

        INLINE std::vector<std::string> listEntities() const;

        bool clear(const std::vector<std::string> * const entities) const;


        bool clear(const std::vector<std::string> &entities) const;


        bool truncate(DxApi::TimestampMs millisecondTime, const std::vector<std::string> * const entities) const;


        bool truncate(DxApi::TimestampMs millisecondTime, const std::vector<std::string> &entities) const;


        bool purge(DxApi::TimestampMs millisecondTime) const;


        bool deleteStream();

        bool getBackgroundProcessInfo(DxApi::BackgroundProcessInfo *to) const;

        bool abortBackgroundProcess() const;

        // Set stream schema. Executes server request
        // Blocking stream metadata update operation is executed after succesfully executing first request
        bool setSchema(bool isPolymorphic, const DxApi::Nullable<std::string> &schema);
        bool setSchema(const DxApi::StreamOptions & options);

        // Execute schemachangetask
        // If SCT if non-background, blocking stream metadata update operation is executed
        bool changeSchema(const DxApi::SchemaChangeTask &task) const;

        bool getPeriodicity(DxApi::Interval * interval) const;

        bool getTimeRange(int64_t range[], const std::vector<std::string> * const entities) const;

        // Throws no exception
        //bool getPeriodicityInternal(DxApi::TickDb &db, const std::string &key, DxApi::Interval * interval) const;

        // Currently there' no separate lock object
        DxApi::LockType lock(DxApi::LockType lockType, DxApi::TimestampMs timeoutMs);
        DxApi::LockType unlock();

        DxApi::TickDb & db() const { return db_; }

        bool updateOptions(const DxApi::StreamOptions & options);

        INLINE uint64_t updateCount() const { return updateCount_; }

        INLINE const char * textId() const { return id_.textId; }
        INLINE uint64_t id() const { return id_.id; }

    protected:

        // update local schema from supplied streamoptions object, returns true if changed
        bool updateLocalSchema(const DxApi::StreamOptions &options); // TODO: Remove?
        
        TickStreamImpl(DxApi::TickDb &db, const std::string &key, const DxApi::StreamOptions &options, uint64_t uid);
        ~TickStreamImpl();

        static void operator delete(void* ptr, size_t sz);

    protected:
        DxApi::TickDb &db_;
        IdentityInfo id_;
        std::string key_;
        DxApi::StreamOptions options_;

        // Access lock
        dx_thread::srw_lock thisLock_;

        // Data update count
        volatile uint64 updateCount_;

        // TickDb Lock, currently embedded inside TickStream
        struct DbLock {
            DxApi::LockType type;
            uint64_t count;
            std::string token;
            DbLock();
        } dbLock_;

        DISALLOW_COPY_AND_ASSIGN(TickStreamImpl);
    }; // class TickStreamImpl

    
    DEFINE_IMPL_CLASS(DxApi::TickStream, DxApiImpl::TickStreamImpl)

#if 0
    class DXTickStreamImpl : public TickStreamImpl {
    public:
        DXTickStreamImpl::DXTickStreamImpl(DxApi::TickDb &db, const std::string &key, const DxApi::StreamOptions &options);
    };
#endif

}