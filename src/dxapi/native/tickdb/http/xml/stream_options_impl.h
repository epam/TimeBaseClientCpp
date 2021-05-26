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

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "xml_request.h"


namespace DxApiImpl {

    class BufferOptionsImpl : public DxApi::BufferOptions {
    public:
        BufferOptionsImpl() : BufferOptions()
        { };

    public:
        std::string toXML() const;
        
        // Should be called on new streamoptions object! Return true on success.
        bool fromXML(tinyxml2::XMLElement * root);
    };

    DEFINE_IMPL_CLASS(DxApi::BufferOptions, BufferOptionsImpl)

    class StreamOptionsImpl : public DxApi::StreamOptions {
    public:
        StreamOptionsImpl() : StreamOptions()
        {}
        

    public:        
        // Should be called on new streamoptions object! Return true on success.
        bool fromXML(tinyxml2::XMLElement * root);
    };
    

    template<> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput& xml, const StreamOptionsImpl &value);

    DEFINE_IMPL_CLASS(DxApi::StreamOptions, StreamOptionsImpl)
}