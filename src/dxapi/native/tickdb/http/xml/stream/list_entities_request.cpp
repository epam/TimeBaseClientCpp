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

#include "list_entities_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;



ListEntitiesRequest::ListEntitiesRequest(const DxApi::TickStream * stream)
    : StreamRequest(stream, "listEntities")
{
}

using namespace XmlGen;

using namespace XmlParse;

bool ListEntitiesRequest::listEntities(vector<std::string>& result)
{
    result.clear();
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "listEntitiesResponse")) {
        return false;
    }

    XMLElement * levelElement = rootElement->FirstChildElement("identities");

    if (NULL != levelElement) {
        FOR_XML_ELEMENTS(levelElement, child, "item") {
            const char * symbol = child->GetText();

            if (NULL == symbol) {
                DBGLOG("listEntities(): Symbol field not found");
                return false;
            }

            result.push_back(symbol);
        }

        return true;
    }
    return false;
}