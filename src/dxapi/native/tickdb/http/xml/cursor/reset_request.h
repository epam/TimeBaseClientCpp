#pragma once

#include "cursor_request.h"

namespace DxApiImpl {
    class ResetRequest : public CursorRequest {
    public:
        ResetRequest(TickDbImpl &db, int64_t id, int64_t time) : CursorRequest(db, "resetRequest", id, time)
        {
        }
    };
}