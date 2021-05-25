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
#include "../xml_request.h"
#include "bg_proc_info_impl.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;

const char * infoExecutionStatus[] = {
    "None",
    "Running",
    "Completed",
    "Aborted",
    "Failed"
};


IMPLEMENT_ENUM(uint8_t, ExecutionStatus, false)
{}

#define ADD(x) xml.add(#x, v.x).add('\n')

//#define ADD(x) do { ss << tag(#x, x).append("\n"); } while (0)


using namespace XmlGen;
namespace DxApiImpl {
    template<> XmlOutput& operator<<(XmlOutput& xml, const BackgroundProcessInfoImpl &v)
    {
        xml.add('\n');

        ADD(name);
        ADD(status);
            // ADD(affectedStreams); // TODO:
        ADD(progress);
        ADD(startTime);
        ADD(endTime);

        return xml;
    }
}


using namespace XmlParse;

#define GET(FIELDNAME) tmp = getText(root, #FIELDNAME); if(NULL != tmp) if (!parse(this->FIELDNAME, tmp)) break;


// TODO: Add == ?
//bool BackgroundProcessInfoImpl::operator==(const BackgroundProcessInfoImpl &other) const
//{
//    return name == other.name
//        && status == other.status
//        && affectedStreams == other.affectedStreams
//        && progress == other.progress
//        && startTime == other.startTime
//        && endTime == other.endTime;
//}

template<> bool XmlParse::parse(ExecutionStatus &type, const char *from)
{
    return NULL == from ? false : (type = ExecutionStatus(from), true);
}

     
bool BackgroundProcessInfoImpl::fromXML(tinyxml2::XMLElement *root)
{
    const char *tmp;

    while (1) {
        GET(name);
        GET(status);
        // TODO:
        //GET(affectedStreams);
        GET(progress);
        GET(startTime);
        GET(endTime);
        return true;
    }

    return false;
}

