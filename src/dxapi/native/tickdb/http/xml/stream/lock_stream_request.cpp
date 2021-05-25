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

#include "lock_stream_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


// This class is probably redundant. Created due to wrong assumptions that both requests return similar XML response. Or maybe will do that later

INLINE LockUnlockRequest::LockUnlockRequest(const DxApi::TickStream * stream, const char *name, bool write)
    : StreamRequest(stream, name), write_(write)
{
    add("write", write);
}


LockStreamRequest::LockStreamRequest(const DxApi::TickStream * stream, const std::string &sid, bool write, int64_t timeout)
    : LockUnlockRequest(stream, "lockStream", write)
{
    add("timeout", timeout);
    add("sid", sid);
}


UnlockStreamRequest::UnlockStreamRequest(const DxApi::TickStream * stream, const std::string &id, bool write)
    : LockUnlockRequest(stream, "unlockStream", write)
{
    add("id", id);
    id_ = id;
}


using namespace XmlParse;

bool LockStreamRequest::execute()
{
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "lockStreamsResponse")) {
        return false;
    }

    const char * s = getText(rootElement, "id");
    if (NULL == s) {
        return false;
    }

    id_ = s;

    if (!parse(write_, getText(rootElement, "write"))) {
        return false;
    }

    return true;
}


bool UnlockStreamRequest::execute()
{
    return executeWithTextResponse();
}