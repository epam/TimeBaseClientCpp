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
#pragma once

#include "stream_request.h"
#include <dxapi/schema_change_task.h>

namespace DxApiImpl {

    class BasicStreamRequest : public StreamRequest {
    public:
        BasicStreamRequest(const DxApi::TickStream * stream, const char * requestName);

        bool execute();
    };

#define BASIC_STREAM_REQUEST(NAME, REQNAME, ...) \
    class NAME : public BasicStreamRequest { \
    public: \
        NAME(const DxApi::TickStream * stream, ##__VA_ARGS__ ); \
        }; \
    INLINE NAME::NAME(const DxApi::TickStream * stream, ##__VA_ARGS__ ) \
        : BasicStreamRequest(stream, #REQNAME)


    BASIC_STREAM_REQUEST(ClearStreamRequest, clearStream, const std::vector<std::string> * const instruments)
    {
        addItemArray("identities", instruments);
    }

    BASIC_STREAM_REQUEST(DeleteStreamRequest, deleteStream)
    {
    }


    BASIC_STREAM_REQUEST(PurgeStreamRequest, purgeStream, DxApi::TimestampMs time)
    {
        add("time", time);
    }


    BASIC_STREAM_REQUEST(TruncateStreamRequest, truncateStream, DxApi::TimestampMs time, const std::vector<std::string> * const instruments)
    {
        add("time", time).add('\n').addItemArray("identities", instruments);
    }


    BASIC_STREAM_REQUEST(RenameStreamRequest, renameStream, const std::string &newKey)
    {
        add("key", newKey);
    }

    BASIC_STREAM_REQUEST(AbortBGProcessRequest, abortBgProcess)
    {
    }


    BASIC_STREAM_REQUEST(SetSchemaRequest, setSchema, bool isPolymorphic, const DxApi::Nullable<std::string> &schema)
    {
        // TODO: check bool serialization
        add("polymorphic", isPolymorphic).add("type", "XML").add('\n').add("schema", schema);
    }


    BASIC_STREAM_REQUEST(ChangeSchemaRequest, changeSchema, const DxApi::SchemaChangeTask &task)
    {
        add("polymorphic", task.polymorphic).add("background", task.background).add('\n')
            .add("schema", task.schema).add('\n')
            .addItemMap("defaults", "name", "value", task.defaults).add('\n')
            .addItemMap("mappings", "name", "value", task.mappings);
    }


    INLINE BasicStreamRequest::BasicStreamRequest(const DxApi::TickStream * stream, const char * requestName)
        : StreamRequest(stream, requestName)
    {
    }


    INLINE bool BasicStreamRequest::execute()
    {
        return executeWithTextResponse();
    }
}