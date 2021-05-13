#pragma once

#include "stream_request.h"


namespace DxApiImpl {

    class TimerangeRequest : public StreamRequest {
    public:
        TimerangeRequest(const DxApi::TickStream * stream, const std::vector<std::string> * const entities = NULL);
        bool getTimerange(int64_t range[2], bool * isNull = NULL);
    };
}