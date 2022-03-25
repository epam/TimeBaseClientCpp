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

#include "tickdb/common.h"

#include "list_spaces_request.h"

using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;


ListSpacesRequest::ListSpacesRequest(const DxApi::TickStream *stream)
    : StreamRequest(stream, "listSpaces")
{
}

using namespace XmlGen;
using namespace XmlParse;

bool ListSpacesRequest::listSpaces(vector<string> &result) {
    result.clear();
    if (!executeAndParseResponse())
        return false;

    XMLElement *rootElement = response_.root();

    if (!nameEquals(rootElement, "listSpacesResponse")) {
        return false;
    }

    XMLElement *spacesElement = rootElement->FirstChildElement("spaces");

    if (NULL != spacesElement) {
        FOR_XML_ELEMENTS(spacesElement, e, "item") {
            if (e->GetText() == NULL) {
                result.push_back("");
            } else {
                result.push_back(e->GetText());
            }
        }
        return true;
    }

    return false;
}
