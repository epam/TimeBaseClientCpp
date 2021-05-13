#pragma once

#include "dxcommon.h"
#include <stdint.h>
#include <sstream>
#include <string>
#include <unordered_map>

namespace DxApi {
    class SchemaChangeTask {
    public:
        bool                    polymorphic;
        bool                    background;
        Nullable<std::string>   schema;
        
        Nullable<std::unordered_map<std::string, std::string>> defaults;
        Nullable<std::unordered_map<std::string, std::string>> mappings;

    public:
        SchemaChangeTask(bool isPolymorphic, bool isInBackground, const std::string *nullableSchema) :
            polymorphic(isPolymorphic), background(isInBackground)
        {
            if (NULL != nullableSchema) {
                schema = *nullableSchema;
            }
        }


        SchemaChangeTask(bool isPolymorphic, bool isInBackground, const Nullable<std::string> &nullableSchema) :
            SchemaChangeTask(isPolymorphic, isInBackground, nullableSchema.is_null() ? NULL : &nullableSchema.get())
        {
        }
    };
}