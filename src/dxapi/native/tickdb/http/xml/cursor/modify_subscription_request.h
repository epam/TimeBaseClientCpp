#pragma once

#include "cursor_request.h"

namespace DxApiImpl {
    class ModifySubscriptionRequest : public CursorRequest {
    public:
        ModifySubscriptionRequest(TickDbImpl &db, const char * requestName, int64_t id, int64_t time, SubscrChangeAction changeMode)
            : CursorRequest(db, requestName, id, time)
        {
            add("mode", changeMode.toString());
        }
    };
}