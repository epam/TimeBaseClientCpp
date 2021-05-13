#pragma once

#include "dxapi/dxconstants.h"
#include "dxapi/schema.h"
#include "xml_request.h"

namespace Schema {

    class TickDbClassDescriptorImpl : public TickDbClassDescriptor {
    public:
        bool fromXML(tinyxml2::XMLElement *root, bool parseFields = true);
        bool fromXML(const std::string &messageDescriptorSchema, bool parseFields = true);
        static bool parseDataType(DataType &dataType, tinyxml2::XMLElement *dataTypeNode);

        static bool parseDescriptors(std::vector<TickDbClassDescriptor> &classes, tinyxml2::XMLElement *root, bool parseFields = true);
        static bool parseDescriptors(std::vector<TickDbClassDescriptor> &classes, const std::string &messageDescriptorSchema, bool parseFields = true);
        static std::vector<TickDbClassDescriptor> parseDescriptors(const std::string &messageDescriptorSchema, bool parseFields);

        static bool guidsFromXML(std::vector<std::pair<std::string, std::string>> &guids, const std::string &messageDescriptorSchema);
        static std::vector<std::pair<std::string, std::string>> guidsFromXML(const std::string &messageDescriptorSchema);

        TickDbClassDescriptorImpl();
    };
}