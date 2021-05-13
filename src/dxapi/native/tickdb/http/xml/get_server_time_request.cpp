#include "tickdb/common.h"

#include "get_server_time_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


GetServerTimeRequest::GetServerTimeRequest(TickDbImpl &db) : XmlRequest(db, "getServerTime", true)
{
}


using namespace XmlParse;
int64_t GetServerTimeRequest::getTime()
{
    if (!executeAndParseResponse()) {
        return DxApi::TIMESTAMP_NULL;
    }

    XMLElement *rootElement = response_.root();

    if (!nameEquals(rootElement, "getServerTimeResponse")) {
        return DxApi::TIMESTAMP_NULL;
    }

    const char *data = getText(rootElement, "time");
    return NULL != data ? strtoll(data, NULL, 10) : DxApi::TIMESTAMP_NULL;
}

