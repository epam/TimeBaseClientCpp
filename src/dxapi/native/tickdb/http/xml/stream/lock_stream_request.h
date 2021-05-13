#pragma once

#include "stream_request.h"


namespace DxApiImpl {

    class LockUnlockRequest : public StreamRequest {
    public:
        const std::string &id() const;
        bool write() const;

    protected:
        friend class LockStreamRequest;
        friend class UnlockStreamRequest;

        LockUnlockRequest(const DxApi::TickStream * stream, const char * name, bool write);
        bool execute();

        std::string id_;
        bool write_;
    };


    class LockStreamRequest : public LockUnlockRequest {
    public:
        LockStreamRequest(const DxApi::TickStream * stream, const std::string &sid, bool write, int64_t timeout);

        bool execute();
    };


    class UnlockStreamRequest : public LockUnlockRequest {
    public:
        UnlockStreamRequest(const DxApi::TickStream * stream, const std::string &id, bool write);

        bool execute();
    };


    INLINE const std::string& LockUnlockRequest::id() const { return id_; }
    INLINE bool LockUnlockRequest::write() const { return write_; }
}