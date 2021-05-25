/*
  Copyright 2021 EPAM Systems, Inc

  See the NOTICE file distributed with this work for additional information
  regarding copyright ownership. Licensed under the Apache License,
  Version 2.0 (the "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
  License for the specific language governing permissions and limitations under
  the License.
 */
#include "tickdb/common.h"

#include "get_server_time_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


GetServerTimeRequest::GetServerTimeRequest(TickDbImpl &db) : XmlRequest(db, "getServerTime", true)
{
}


using namespace XmlParse;
int64_t GetServerTimeRequest::getTime()
{
    if (!executeAndParseResponse()) {
        return DxApi::TIMESTAMP_NULL;
    }

    XMLElement *rootElement = response_.root();

    if (!nameEquals(rootElement, "getServerTimeResponse")) {
        return DxApi::TIMESTAMP_NULL;
    }

    const char *data = getText(rootElement, "time");
    return NULL != data ? strtoll(data, NULL, 10) : DxApi::TIMESTAMP_NULL;
}

