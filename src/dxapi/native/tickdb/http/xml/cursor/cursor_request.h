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

#include "../xml_request.h"

namespace DxApiImpl {

    class CursorRequest : public XmlRequest {
    public:
        CursorRequest(TickDbImpl &db, const char * requestName, int64_t id, int64_t time) : XmlRequest(db, requestName) // , id_(id), time_(time)
        {
            add("id", id);
            add("time", time);
        }

        // returns -1 on failure
        int64_t execute()
        {
            if (!executeAndParseResponse())
                return -1;

            tinyxml2::XMLElement * rootElement = response_.root();

            if (!XmlParse::nameEquals(rootElement, "cursorResponse")) {
                return -1;
            }

            const char * data = XmlParse::getText(rootElement, "serial");
            return NULL != data ? strtoll(data, NULL, 10) : -1;
        }
    };
}