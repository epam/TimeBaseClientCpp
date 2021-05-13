#pragma once
#include "xml_request.h"


namespace DxApiImpl {

    class GetServerTimeRequest : public XmlRequest {
    public:
        GetServerTimeRequest(TickDbImpl &db);

        int64_t getTime();
    };
}