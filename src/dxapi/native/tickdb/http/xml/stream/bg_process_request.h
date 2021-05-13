#pragma once

#include "stream_request.h"


namespace DxApiImpl {

    class GetBgProcessRequest : public StreamRequest {
    public:
        GetBgProcessRequest(const DxApi::TickStream *stream)
            : StreamRequest(stream, "getBgProcess")
        {
        }

        bool getResponse(DxApi::BackgroundProcessInfo *bgProcessInfo);
    };
}