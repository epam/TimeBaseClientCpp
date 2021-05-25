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

#include "http/tickdb_http.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;


TickDb * TickDb::createFromUrl(const char * url, const char * username, const char * password)
{
    TickDbImpl *db = new TickDbImpl();

    // Does not check if the uri is connectable at this moment
    if (!db->setUri(url)) {
        delete db;
        THROW_DBGLOG_EX(TickDbException, "Unable to parse URI: %s\n", url);
        return NULL;
    }

    if (NULL != username) {
        if (!db->setAuth(username, password)) {
            delete db;
            THROW_DBGLOG_EX(TickDbException, "Unable to set authentication information for URI: %s\n", url);
            return NULL;
        }
    }

    DBGLOG("%s: Will connect to: %s\n", db->textId(), url);

    return db;
}

TickDb * TickDb::createFromUrl(const char * url)
{
    return createFromUrl(url, NULL, NULL);
}


TickDb::~TickDb()
{
    // Nothing here for now
}


std::string MarketMessage::toString() const {
    std::stringstream ss;
    //TickCursor * cursor = reinterpret_cast<TickCursor*>(parent);
    ss << "<" << *cursor->getInstrument(entityId) << "|" << typeId << ">";
    return ss.str();
}


const std::string * InstrumentMessage::getTypeName() const {
    return cursor->getMessageTypeName(typeId);
}

