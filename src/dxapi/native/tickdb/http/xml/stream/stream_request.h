#pragma once

#include "tickdb/tickstream_impl.h"
#include "../xml_request.h"

namespace DxApiImpl {

    class StreamRequest : public XmlRequest {
    public:
        StreamRequest(const DxApi::TickStream * stream, const char * requestName) : XmlRequest(impl(impl(stream)->db()), requestName)
        {
            if (DxApi::LockType::NO_LOCK != impl(stream)->lockType()) {
                add("token", impl(stream)->lockToken());
            }

            add("stream", impl(stream)->key());
        }

    };
}

