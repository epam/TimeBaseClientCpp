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
#include <dxapi/dxapi.h>


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;


#define ENUM_NAME TimeUnit

const char * infoTimeUnit[] = {
    // Fixed length
    "X-MILLISECOND",
    "S-SECOND",
    "I-MINUTE",
    "H-HOUR",
    "D-DAY",
    "W-WEEK",

    // Variable length
    "M-MONTH",
    "Q-QUARTER",
    "Y-YEAR",
};

#define ENUM_HELPER_VRFY(NAME) assert(ENUM_NAME::NAME    == fromChar(info##ENUM_NAME[ENUM_NAME::NAME]));

#define VRFY ENUM_HELPER_VRFY

IMPLEMENT_ENUM(uint8_t, TimeUnit, true)
{
    // Verify that format did not change
    assert(TimeUnit::MILLISECOND    == fromChar('X'));
    assert(TimeUnit::SECOND         == fromChar('S'));
    assert(TimeUnit::MINUTE         == fromChar('I'));
    assert(TimeUnit::HOUR           == fromChar('H'));
    assert(TimeUnit::DAY            == fromChar('D'));
    assert(TimeUnit::WEEK           == fromChar('W'));
    assert(TimeUnit::MONTH          == fromChar('M'));
    assert(TimeUnit::QUARTER        == fromChar('Q'));
    assert(TimeUnit::YEAR           == fromChar('Y'));
}

/*

TODO:remove

TimeUnit::EnumClass(const char s[]) : value_(helper.fromString(s)) {}


TimeUnit TimeUnit::fromChar(int x)
{
    return helper.fromChar(x);
}


char TimeUnit::toChar() const
{
    return helper.toChar(*this);
}


const char * TimeUnit::toString() const
{
    return helper.toString(*this);
}

*/