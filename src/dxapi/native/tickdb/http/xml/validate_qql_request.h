#pragma once
#include "xml_request.h"
#include "tickdb/qqlstate.h"
#include <set>

namespace DxApiImpl {

    class ValidateQqlRequest : public XmlRequest {
    public:
        ValidateQqlRequest(TickDbImpl &db, const std::string &qql);
        bool getState(QqlState &state);

    //private:
        //void * getFirstElement();
    };
}