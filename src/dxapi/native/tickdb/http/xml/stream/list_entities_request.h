#pragma once

#include "stream_request.h"

namespace DxApiImpl {

    class ListEntitiesRequest : public StreamRequest {
    public:
        ListEntitiesRequest(const DxApi::TickStream * stream);

        bool listEntities(std::vector<std::string>&);
    };
}