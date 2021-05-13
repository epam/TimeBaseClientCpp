#pragma once

#include "dxcommon.h"
#include <stdint.h>
#include <sstream>
#include <string>

namespace DxApi {
    class SelectionOptions {
    public:
        int64_t             from;
        int64_t             to;
        bool                useCompression;
        bool                live;
        bool                reverse;
        bool                isBigEndian;
        bool                allowLateOutOfOrder;
        bool                realTimeNotification;
        bool                minLatency;

    public:
        SelectionOptions() :
            from(INT64_MIN),
            to(INT64_MAX),
            useCompression(false),
            live(false),
            reverse(false),
            isBigEndian(true),
            allowLateOutOfOrder(false),
            realTimeNotification(false),
            minLatency(false)
        { };
    };
}