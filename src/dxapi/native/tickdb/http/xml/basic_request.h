#pragma once

#include "xml_request.h"

/* HTTP XML request that doesn;t expect XML response, just return code */
namespace DxApiImpl {
    class BasicRequest : public XmlRequest {
    public:
        BasicRequest(TickDbImpl &db, const char * name, bool noContent = false) : XmlRequest(db, name, noContent)
        {
        }

        bool execute()
        {
            return executeWithTextResponse();
        }
    };
}