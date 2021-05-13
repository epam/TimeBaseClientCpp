#pragma once

#include "stream_request.h"


namespace DxApiImpl {

    class PeriodicityRequest : public StreamRequest {
    public:
        PeriodicityRequest(const DxApi::TickStream * stream);
        bool getPeriodicity(DxApi::Interval * interval);
    };
}