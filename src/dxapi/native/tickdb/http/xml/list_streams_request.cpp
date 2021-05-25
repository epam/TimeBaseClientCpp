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

#include "list_streams_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


ListStreamsRequest::ListStreamsRequest(TickDbImpl &db) : XmlRequest(db, "listStreams", true)
{ }


using namespace XmlParse;

// Returns NULL if error or array is empty
void * ListStreamsRequest::getFirstElement()
{
    if (!executeAndParseResponse())
        return NULL;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "listStreamsResponse")) {
        return NULL;
    }

    XMLElement *levelElement = rootElement->FirstChildElement("streams");

    if (NULL == levelElement)
        return NULL;

    return levelElement->FirstChildElement("item");
}


vector<string> ListStreamsRequest::getStreamKeysAsVector()
{
    vector<string> result;

    for (XMLElement * item = (XMLElement *)getFirstElement();  NULL != item; item = item->NextSiblingElement()) {
        result.push_back(item->GetText());
    }

    return result;
}


set<string> ListStreamsRequest::getStreamKeysAsSet()
{
    set<string> result;
    
    for (XMLElement * item = (XMLElement *)getFirstElement(); NULL != item; item = item->NextSiblingElement()) {
        result.emplace(item->GetText());
    }

    return result;
}