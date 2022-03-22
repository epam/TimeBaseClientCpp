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

#include "timerange_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;


TimerangeRequest::TimerangeRequest(const TickStream * stream, const std::vector<std::string> * const entities)
    : StreamRequest(stream, "getTimeRange")
{
    if (entities != NULL) {
        for (auto &i : *entities) {
            add("identities", i);
        }
    }
}

using namespace XmlParse;

bool TimerangeRequest::getTimerange(int64_t range[2], bool * isNull)
{
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "getTimeRangeResponse")) {
        responseParseError("<getTimeRangeResponse> not found");
        return false;
    }

    // Initialize empty timerange
    range[0] = INT64_MAX;
    range[1] = INT64_MIN;

    XMLElement * levelElement = rootElement->FirstChildElement("timeRange");
    if (NULL == levelElement) {
        if (NULL != isNull) {
            *isNull = true;
        }
        // If timerange fields we return empty range, but success
        return true;
    }

    const char * from = getText(levelElement, "from");
    const char * to = getText(levelElement, "to");
    if (NULL == from || NULL == to) {
        responseParseError("<timeRange> <from> or <to> not found");
        return false;
    }

    range[0] = strtoll(from, NULL, 10);
    range[1] = strtoll(to, NULL, 10);

    // TODO: check values (use parse template function)
    return true;
}