#include "tickdb/common.h"

#include "timerange_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;


TimerangeRequest::TimerangeRequest(const TickStream * stream, const std::vector<std::string> * const entities)
    : StreamRequest(stream, "getTimeRange")
{
    addItemArray("identities", entities);
}

using namespace XmlParse;

bool TimerangeRequest::getTimerange(int64_t range[2], bool * isNull)
{
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "getTimeRangeResponse")) {
        responseParseError("<getTimeRangeResponse> not found");
        return false;
    }

    // Initialize empty timerange
    range[0] = INT64_MAX;
    range[1] = INT64_MIN;

    XMLElement * levelElement = rootElement->FirstChildElement("timeRange");
    if (NULL == levelElement) {
        if (NULL != isNull) {
            *isNull = true;
        }
        // If timerange fields we return empty range, but success
        return true;
    }

    const char * from = getText(levelElement, "from");
    const char * to = getText(levelElement, "to");
    if (NULL == from || NULL == to) {
        responseParseError("<timeRange> <from> or <to> not found");
        return false;
    }

    range[0] = strtoll(from, NULL, 10);
    range[1] = strtoll(to, NULL, 10);

    // TODO: check values (use parse template function)
    return true;
}
