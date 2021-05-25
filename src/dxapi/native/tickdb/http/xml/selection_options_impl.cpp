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

#include "xml_request.h"
#include "selection_options_impl.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;


const char * infoTypeTransmission[] = {
    "GUID",
    "NAME",
    "DEFINITION"
};

IMPLEMENT_ENUM(uint8_t, TypeTransmission, false)
{}


#define ADD(x) xml.add(#x, v.x).add('\n')

using namespace XmlGen;
namespace DxApiImpl {

    template<> XmlOutput& operator<<(XmlOutput& xml, const DxApi::QueryParameter &v)
    {
        ADD(name);
        ADD(type);
        if (v.value.is_set()) {
            ADD(value);
        }

        return xml;
    }

    template<> XmlOutput& operator<<(XmlOutput& xml, const SelectionOptionsImpl &v)
    {

        if (v.useCompression || !v.isBigEndian) {
            DBGLOG("WARNING: Non-default values for those options is not supported. useCompression=%i, isBigEndian=%i",
                (int)v.useCompression, (int)v.isBigEndian);
        }

        ADD(from);
        ADD(to);
        xml.add("typeTransmission", v.typeTransmissionMode).add('\n');
        //ss << "<compression>" << CompressionNames[compression] << "</compression>" << std::endl;
        ADD(useCompression);
        ADD(reverse);
        ADD(isBigEndian); // Endianness is only for message header fields, as the body(RawMessage) is read/written as blob anyway
        ADD(live);
        ADD(minLatency);
        ADD(allowLateOutOfOrder);
#if 1 == REALTIME_NOTIFICATION_DISABLED
        xml.add("realTimeNotification", false).add('\n');
#else
        ADD(realTimeNotification);
#endif

        if (v.qql.has_value())
        {
            ADD(qql);
            if (v.qqlParameters.size() != 0) {
                xml.addItemArray("parameters", v.qqlParameters).add('\n');
            }
        }

        return xml;
    }
}