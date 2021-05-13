#include "tickdb/common.h"

#include "list_streams_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


ListStreamsRequest::ListStreamsRequest(TickDbImpl &db) : XmlRequest(db, "listStreams", true)
{ }


using namespace XmlParse;

// Returns NULL if error or array is empty
void * ListStreamsRequest::getFirstElement()
{
    if (!executeAndParseResponse())
        return NULL;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "listStreamsResponse")) {
        return NULL;
    }

    XMLElement *levelElement = rootElement->FirstChildElement("streams");

    if (NULL == levelElement)
        return NULL;

    return levelElement->FirstChildElement("item");
}


vector<string> ListStreamsRequest::getStreamKeysAsVector()
{
    vector<string> result;

    for (XMLElement * item = (XMLElement *)getFirstElement();  NULL != item; item = item->NextSiblingElement()) {
        result.push_back(item->GetText());
    }

    return result;
}


set<string> ListStreamsRequest::getStreamKeysAsSet()
{
    set<string> result;
    
    for (XMLElement * item = (XMLElement *)getFirstElement(); NULL != item; item = item->NextSiblingElement()) {
        result.emplace(item->GetText());
    }

    return result;
}


    
