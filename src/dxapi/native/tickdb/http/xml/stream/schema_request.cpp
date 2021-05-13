#include "tickdb/common.h"

#include <sstream>
#include <set>

#include "schema_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;
using namespace Schema;


//TODO: DEBUG:
//static std::set<string> types;
//
//extern void schema_dump_types();
//
//void schema_dump_types() {
//    for (auto & i : types) {
//        DBGLOG("%s", i.c_str());
//    }
//}


SchemaRequest::SchemaRequest(const DxApi::TickStream * stream) : StreamRequest(stream, "getSchema")
{ }


// Just experimenting, will be changed later to wrapper class
//#define FIRST(parentElement, name) ((parentElement)->FirstChildElement(name))
//#define FOR_XML_ELEMENTS(parentElement, i, name) for (XMLElement * (i) = (parentElement)->FirstChildElement(name); NULL != (i); (i) = (i)->NextSiblingElement())

// XML element wrapper
//class XMLElem : public XMLElement {
//};


using namespace XmlParse;




vector<TickDbClassDescriptor> SchemaRequest::getClassDescriptors()
{
    vector<TickDbClassDescriptor> classes;

    if (!executeAndParseResponse())
        return classes;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "classSet")) {
        return classes;
    }

    if (!TickDbClassDescriptorImpl::parseDescriptors(classes, rootElement, true)) {
        responseParseError("failed to parse classDescriptor");
        THROW_DBGLOG("failed to parse classDescriptor");
    }

    return classes;
}



vector<string> SchemaRequest::getMessageTypes() {
    vector<string> result;
    vector<TickDbClassDescriptor> descriptors = getClassDescriptors();

    for(auto & descr : descriptors) {
        if (descr.isContentClass) {
            result.push_back(descr.className);
        }
    }
    
    return result;
}


vector<pair<string, string>> SchemaRequest::getMessageTypesAndGuids() {
    vector<pair<string, string>> result;
    vector<TickDbClassDescriptor> descriptors = getClassDescriptors();

    for (auto & descr : descriptors) {
        if (descr.isContentClass) {
            result.emplace_back(descr.className, descr.guid);
        }
    }

    return result;
}



