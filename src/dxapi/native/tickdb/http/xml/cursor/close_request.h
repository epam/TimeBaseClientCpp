#pragma once

#include "cursor_request.h"

namespace DxApiImpl {
    class CloseRequest : public CursorRequest {
    public:
        CloseRequest(TickDbImpl &db, int64_t id, int64_t time) : CursorRequest(db, "closeRequest", id, time)
        {
        }
    };
}