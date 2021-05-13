#include "tickdb/common.h"

#include "list_entities_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;



ListEntitiesRequest::ListEntitiesRequest(const DxApi::TickStream * stream)
    : StreamRequest(stream, "listEntities")
{
}

using namespace XmlGen;

using namespace XmlParse;

bool ListEntitiesRequest::listEntities(vector<std::string>& result)
{
    result.clear();
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "listEntitiesResponse")) {
        return false;
    }

    XMLElement * levelElement = rootElement->FirstChildElement("identities");

    if (NULL != levelElement) {
        FOR_XML_ELEMENTS(levelElement, child, "item") {
            const char * symbol = child->GetText();

            if (NULL == symbol) {
                DBGLOG("listEntities(): Symbol field not found");
                return false;
            }

            result.push_back(symbol);
        }

        return true;
    }
    return false;
}

