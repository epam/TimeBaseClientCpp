#pragma once

#include "../xml_request.h"
#include "../tickdb_class_descriptor.h"
#include "dxapi/schema.h"
#include "stream_request.h"

namespace DxApiImpl {
    class SchemaRequest : public StreamRequest {
    public:
        SchemaRequest(const DxApi::TickStream * stream);
        std::vector<std::string> getMessageTypes();
        std::vector<std::pair<std::string, std::string>> getMessageTypesAndGuids();
        std::vector<Schema::TickDbClassDescriptor> getClassDescriptors();
        
    };
}

