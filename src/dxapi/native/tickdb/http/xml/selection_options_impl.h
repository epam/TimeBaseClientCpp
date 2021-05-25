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

#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "xml_request.h"

namespace DxApiImpl {

    struct TypeTransmissionEnum {
        enum Enum {
            GUID,
            NAME,
            DEFINITION
        };
    };

    ENUM_CLASS(uint8_t, TypeTransmission);


    class SelectionOptionsImpl : public DxApi::SelectionOptions {
    public:
        TypeTransmission    typeTransmissionMode;
        DxApi::Nullable<std::string> qql;
        std::vector<DxApi::QueryParameter> qqlParameters;

    public:
        SelectionOptionsImpl() : SelectionOptions(),
            typeTransmissionMode(TypeTransmission::DEFINITION)
        {}

        SelectionOptionsImpl(const SelectionOptions &other) : SelectionOptions(other),
            typeTransmissionMode(TypeTransmission::DEFINITION)
        {}

        SelectionOptionsImpl(const SelectionOptionsImpl &other) : SelectionOptions(other),
            typeTransmissionMode(TypeTransmission::DEFINITION), qql(other.qql),
            qqlParameters(other.qqlParameters)
        {}
    };

    template<> DxApiImpl::XmlGen::XmlOutput& operator<<(DxApiImpl::XmlGen::XmlOutput& xml, const SelectionOptionsImpl &v);
}