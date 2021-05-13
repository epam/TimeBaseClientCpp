#pragma once

#include "../xml_request.h"

namespace DxApiImpl {

    class CursorRequest : public XmlRequest {
    public:
        CursorRequest(TickDbImpl &db, const char * requestName, int64_t id, int64_t time) : XmlRequest(db, requestName) // , id_(id), time_(time)
        {
            add("id", id);
            add("time", time);
        }

        // returns -1 on failure
        int64_t execute()
        {
            if (!executeAndParseResponse())
                return -1;

            tinyxml2::XMLElement * rootElement = response_.root();

            if (!XmlParse::nameEquals(rootElement, "cursorResponse")) {
                return -1;
            }

            const char * data = XmlParse::getText(rootElement, "serial");
            return NULL != data ? strtoll(data, NULL, 10) : -1;
        }
    };
}