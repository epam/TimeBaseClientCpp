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