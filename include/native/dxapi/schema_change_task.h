/*
 * Copyright 2021 EPAM Systems, Inc
 *
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
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
        
        Nullable<std::unordered_map<std::string, std::string> > defaults;
        Nullable<std::unordered_map<std::string, std::string> > mappings;

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
