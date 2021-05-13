#include "tickdb/common.h"

#include "stream_options_impl.h"
#include "load_streams_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;


LoadStreamsRequest::LoadStreamsRequest(TickDbImpl &db) : XmlRequest(db, "loadStreams", true)
{ }


using namespace XmlParse;


//
//bool LoadStreamsRequest::parseStreamOptions(DxApi::StreamOptions &opt, XMLElement * e)
//{
//    // TODO:
//    //opt.bufferOptions = ;
//
//    const char * tmp;
//    StreamOptions opt_new;
//    opt = std::move(opt_new);
//    StreamOptionsImpl &o = static_cast<StreamOptionsImpl&>(opt);
//    
//    
//#define GETSTR(NAME) tmp = getText(e, #NAME); if(tmp != NULL) o.NAME = tmp;
//
//    
//    GETSTR(name);
//    GETSTR(description);
//    GETSTR(owner);
//    GETSTR(metadata);
//    
//    
//    // ...
//    return true;
//}


bool LoadStreamsRequest::getStreams(Response &streams, const std::string &from)
{
    noRequestNeeded_ = true;
    response_.assign(from); // TODO: avoid copying
    return getStreams(streams);
}


bool LoadStreamsRequest::getStreams(Response &streams)
{
    
    if (!executeAndParseResponse()) {
        return false;
    }

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "loadStreamsResponse")) {
        return false;
    }

    XMLElement * streamsElement = rootElement->FirstChildElement("streams");
    XMLElement * optionsElement = rootElement->FirstChildElement("options");

    if (NULL == streamsElement) {
        DBGLOG("streams element not found");
        return false;
    }

    if (NULL == optionsElement) {
        DBGLOG("options element not found");
        return false;
    }

    streams.clear();

    FOR_XML_ELEMENTS(optionsElement, e, "item")  {
        StreamInfo itemContent;
        //if (!parseStreamOptions(itemContent.options, e)) {
        if (!impl(itemContent.options).fromXML(e)) {
            return false;
        }
        streams.push_back(itemContent);
    }

    intptr i = 0, n = streams.size();

    FOR_XML_ELEMENTS(streamsElement, e, "item") {

        // Here we can catch a case where this array is longer than previous
        if (++i > n) 
            break;

        if (!parse(streams[i - 1].key, e->GetText())) {
            return false;
        }
    }

    if (i != n) {
        DBGLOG("length of 'streams' and 'options' arrays is different in LoadStreamsResponse");
        return false;
    }

    return true;
}

