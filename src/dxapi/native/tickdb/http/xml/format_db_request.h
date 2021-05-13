#pragma once

#include "basic_request.h"


namespace DxApiImpl {

    class FormatDbRequest : public BasicRequest {
    public:
        FormatDbRequest(TickDbImpl &db) : BasicRequest(db, "format", true)
        {}
    };
}