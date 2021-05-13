#include "tickdb/common.h"

#include "periodicity_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;


PeriodicityRequest::PeriodicityRequest(const DxApi::TickStream * stream) : StreamRequest(stream, "getPeriodicity")
{ }


// This is how the response looks
/*
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<getPeriodicityResponse>
<periodicity>IRREGULAR</periodicity>
</getPeriodicityResponse>
*/

using namespace XmlParse;

bool PeriodicityRequest::getPeriodicity(Interval * interval)
{
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();
    
    do {
        const char * dataPtr = NULL;
        char * endPtr = NULL;

        if (!nameEquals(rootElement, "getPeriodicityResponse"))
            break;

        dataPtr = getText(rootElement, "periodicity");
        if (NULL == dataPtr)
            break;

        //const char * dataPtr = strdup(dataPtr);
        if (strhasprefix(dataPtr, "IRREGULAR", 9)) {
            interval->numUnits = 0;
            interval->unitType = TimeUnit::UNKNOWN;
            return true;
        }

        int64_t numUnits = strtoll(dataPtr, &endPtr, 10);
        if (NULL == endPtr || dataPtr == endPtr)
            break;

        TimeUnit unitType = TimeUnit::fromChar(*endPtr);
        if (TimeUnit::UNKNOWN == unitType)
            break;

        interval->numUnits = numUnits;
        interval->unitType = unitType;

        return true;
    } while (0);

    return false;
}

