#pragma once
#include "xml_request.h"
#include <set>

namespace DxApiImpl {

    class ListStreamsRequest : public XmlRequest {
    public:
        ListStreamsRequest(TickDbImpl &db);
        std::vector<std::string> getStreamKeysAsVector();
        std::set<std::string> getStreamKeysAsSet();

    private:
        void * getFirstElement();
    };
}