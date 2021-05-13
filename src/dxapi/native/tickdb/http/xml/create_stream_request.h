#pragma once

#include "basic_request.h"
#include "stream_options_impl.h"

namespace DxApiImpl {
    class CreateStreamRequest : public BasicRequest {
    public:
        CreateStreamRequest(TickDbImpl &db, const std::string &key, const DxApi::StreamOptions &so) : BasicRequest(db, "createStreamRequest")
        {
            add("key", key).add('\n').add("options", impl(so));
        }
    };


    class CreateFileStreamRequest : public BasicRequest {
    public:
        CreateFileStreamRequest(TickDbImpl &db, const std::string &key, const std::string &dataFile) : BasicRequest(db, "createFileStreamRequest")
        {
            add("key", key).add('\n').add("dataFile", dataFile);
        }
    };
}