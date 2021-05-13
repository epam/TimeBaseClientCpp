#include "tickdb/common.h"

#include "xml_request.h"
#include "tickdb_class_descriptor.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;
using namespace Schema;
using namespace XmlParse;

std::vector<TickDbClassDescriptor> TickDbClassDescriptor::parseDescriptors(const std::string &messageDescriptorSchema, bool parseFields) {
    return TickDbClassDescriptorImpl::parseDescriptors(messageDescriptorSchema, parseFields);
}

TickDbClassDescriptorImpl::TickDbClassDescriptorImpl()
{
}


bool TickDbClassDescriptorImpl::fromXML(XMLElement *root, bool parseFields)
{
    if (!parse(className, getText(root, "name"))) {
        className.clear();
        // We now allow unnamed message types and bind them to name ""
    }

    if (!parse(guid, getText(root, "guid"))) {
        return false;
    }

    const char *parentGuid = getText(root, "parent");
    const char *classType = root->Attribute("xsi:type");

    this->parentGuid = NULL != parentGuid ? parentGuid : "";

    // Try to calculate enum size by checking all its values
    if (0 == _strcmpi(classType, "enumclass")) {
        unsigned enumSizeMax = 0;

        FOR_XML_ELEMENTS(root, value, "value") {
            const char *enumValue = getText(value, "value");
            if (NULL != enumValue) {
                int64 x = strtoll(enumValue, NULL, 10);
                unsigned enumSize = x < INT32_MIN || x > INT32_MAX ? 3 : x < INT16_MIN || x > INT16_MAX ? 2 : x < INT8_MIN || x > INT8_MAX ? 1 : 0;
                enumSizeMax = max(enumSizeMax, enumSize);

                const char *symbol = getText(value, "symbol");
                if (NULL != symbol) {
                    if (x < 0 || x > 2048) {
                        THROW_DBGLOG("Invalid enum value: %lld. Valid range [0, 2048].", (long long)x);
                    }

                    string symbol_str = symbol;
                    symbolToEnumValue[symbol_str] = (int32_t)x;
                    while (enumSymbols.size() <= (size_t)x) {
                        enumSymbols.emplace_back("");
                    }

                    enumSymbols[x] = symbol_str;
                }
            }
        }

        enumType = FieldType::fromInt((int)FieldType::ENUM8 + enumSizeMax);
    }
    else if (0 == _strcmpi(classType, "recordclass")) {
        // TODO: Use reference to enum class to set field type
        if (parseFields) {
            FOR_XML_ELEMENTS(root, field, "field") {
                const char *ftype = field->Attribute("xsi:type");
                if (NULL != ftype && 0 == strcmp("nonStaticDataField", ftype)) {
                    FieldInfo f;

                    if (parse(f.name, getText(field, "name"))) {
                        XMLElement * fieldTypeElement = field->FirstChildElement("type");
                        parseDataType(f.dataType, fieldTypeElement);

                        parse(f.relativeTo, getText(field, "relativeTo"));
                        fields.push_back(f);
                    }
                }
            }
        }
    }
    else {
        THROW_DBGLOG("TickDbClassDescriptor::fromXML(): Unknown class type: %192s", classType);
    }

    return true;
}


bool TickDbClassDescriptorImpl::parseDataType(DataType &dataType, XMLElement *dataTypeNode)
{
    dataType.encodingName.clear();
    dataType.descriptorGuid.clear();
    dataType.isNullable = false;

    if (NULL != dataTypeNode) {
        const char *type = dataTypeNode->Attribute("xsi:type");
        dataType.typeName = type;

        const char *encoding = getText(dataTypeNode, "encoding");
            // parse returns false and does not change target if parsing failed
        parse(dataType.encodingName, encoding);
        //f.setType(string(type).append("-").append(encoding).c_str());

        parse(dataType.descriptorGuid, getText(dataTypeNode, "descriptor"));

        XMLElement *typesElement = dataTypeNode->FirstChildElement("types");
        while (NULL != typesElement) {
            dataType.types.emplace_back(typesElement->GetText());
            typesElement = typesElement->NextSiblingElement("types");
        }

        parse(dataType.isNullable, getText(dataTypeNode, "nullable"));

        string typeDescription = type;
        if (NULL != encoding) {
            typeDescription.append("-").append(encoding);
        }

        dataType.descriptor = FieldTypeDescriptor(FieldType::fromString(typeDescription), 0, dataType.isNullable);

        if (typeDescription == "array") {
            XMLElement * subType = dataTypeNode->FirstChildElement("type");
            if (NULL == subType) {
                THROW_DBGLOG("Array element type is empty.");
            }

            DataType *elementType = new DataType();
            parseDataType(*elementType, subType);
            dataType.elementType = shared_ptr<DataType>(elementType);
        }
    } else {
        // = NULL
        dataType.descriptor = FieldTypeDescriptor(FieldType::Enum::UNKNOWN, 0, false);
    }

    return true;
}


bool TickDbClassDescriptorImpl::fromXML(const string &messageDescriptorSchema, bool parseFields)
{
    XMLDocument xmlDocument;
    
    auto n = messageDescriptorSchema.size();
    XMLError error = xmlDocument.Parse(0 != n ? messageDescriptorSchema.data() : "", n);

    if (XML_NO_ERROR != error)
        return false;

    auto root = xmlDocument.RootElement();
    auto firstClassDescriptorElement = NULL != root ? root->FirstChildElement("classDescriptor") : NULL;
    if (NULL == firstClassDescriptorElement)
        return false;

    return fromXML(firstClassDescriptorElement, parseFields);
}


bool TickDbClassDescriptorImpl::parseDescriptors(vector<TickDbClassDescriptor> &classes, XMLElement *root, bool parseFields)
{
    if (NULL == root)
        return false;

    FOR_XML_ELEMENTS(root, child, "classDescriptor") {
        const char *name = getText(child, "name");
        const char *guid = getText(child, "guid");
        XMLElement *parent = child->FirstChildElement("parent");
        const char *classType = child->Attribute("xsi:type");
        if (/*some qql result sets may not have type name NULL != name && */ 
            NULL != guid) {
            TickDbClassDescriptorImpl cls;
            if (!cls.fromXML(child, parseFields)) {
                return false;
            }

            classes.push_back(cls);
        }
    }

    intptr_t contentClassIndex = 0;
    FOR_XML_ELEMENTS(root, i, "contentClassId") {
        const char *contentGuid = i->GetText();
        if (NULL != contentGuid) {
            for (intptr_t j = contentClassIndex, count = (intptr_t)classes.size(); j < count; ++j) {
                auto &cl = classes[j];
                if (contentGuid == cl.guid) {
                    cl.isContentClass = true;
                    // All content classes will be moved on top of the array and sorted in the order of appereance of their <contentClassId>
                    swap(cl, classes[contentClassIndex++]);
                    break;
                }
            }
        }
    }


    // Hmm, O(n^2)
    intptr_t nClasses = classes.size();
    for (auto &c : classes) {
        const string &parentGuid = c.parentGuid;
        // Find parent class and link to it
        if (0 != parentGuid.length()) {
            forn(j, nClasses) {
                const string &parentGuid2 = classes[j].guid;
                if (parentGuid2 == parentGuid) {
                    c.parentIndex = j;
                    // TODO: FIX IT! LOST REFERENCE (python) + check getClassDescriptors() result
                    // TODO: check if switch to indices fixed whatever problems existed before
                    break;
                }
            }
        }
        for (auto &field : c.fields) {
            if ("enum" == field.dataType.typeName) {
                const string &enumClassGuid = field.dataType.descriptorGuid;
                for (auto &c2 : classes) {
                    if (enumClassGuid == c2.guid) {
                        field.dataType.descriptor = FieldTypeDescriptor(c2.enumType, 0, field.dataType.isNullable);
                    }
                }
            }
        }
    }

    return true;
}


bool TickDbClassDescriptorImpl::parseDescriptors(std::vector<TickDbClassDescriptor> &descriptors, const string &messageDescriptorSchema, bool parseFields)
{
    XMLDocument xmlDocument;

    auto n = messageDescriptorSchema.size();
    XMLError error = xmlDocument.Parse(0 != n ? messageDescriptorSchema.data() : "", n);
    if (XML_NO_ERROR != error)
        return false;

    return parseDescriptors(descriptors, xmlDocument.RootElement(), parseFields);
}


vector<TickDbClassDescriptor> TickDbClassDescriptorImpl::parseDescriptors(const string &messageDescriptorSchema, bool parseFields)
{
    vector<TickDbClassDescriptor> descriptors;
    parseDescriptors(descriptors, messageDescriptorSchema, parseFields);

    return descriptors;
}


bool TickDbClassDescriptorImpl::guidsFromXML(vector<pair<string, string>> &result, const string& messageDescriptorSchema)
{
    vector<TickDbClassDescriptor> descriptors;
    if (!parseDescriptors(descriptors, messageDescriptorSchema, false))
        return false;

    for (auto &descr : descriptors) {
        if (descr.isContentClass) {
            result.emplace_back(descr.className, descr.guid);
        }
    }

    return true;
}


vector<pair<string, string>> TickDbClassDescriptorImpl::guidsFromXML(const string &messageDescriptorSchema)
{
    vector<pair<string, string>> result;
    guidsFromXML(result, messageDescriptorSchema);

    return result;
}
