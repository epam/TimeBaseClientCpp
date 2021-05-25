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

#include "dxcommon.h"
#include <stdint.h>
#include <sstream>
#include <string>

namespace DxApi {

    struct TimeUnitEnum {
        enum Enum {
            MILLISECOND = 0,
            SECOND,
            MINUTE,
            HOUR,
            DAY,
            /* Weeks have fixed day order, Monday through Sunday */
            WEEK,
            MONTH,
            /* Quarters are calendar quarters, starting in January, April, July and */
            /* October. This is different from fiscal quarters, which can start in */
            /* any calendar month. */
            QUARTER,
            YEAR,
            UNKNOWN = 0xFF
        };
    };

    ENUM_CLASS(uint8_t, TimeUnit);


    class Interval {
    public:
        bool isZero() const;
        bool isNegative() const;
        bool isPositive() const;

        int64_t  numUnits;
        TimeUnit unitType;

        // TODO (not important):
        // public static Int64 ToMilliseconds(Interval interval)
        // public static FixedInterval Parse(Int64 size)
    public:
        Interval(int64_t numUnits = 0, TimeUnit unitType = TimeUnit::MILLISECOND);
    };

    INLINE bool Interval::isZero()     const { return 0 == numUnits; }
    INLINE bool Interval::isNegative() const { return numUnits < 0; }
    INLINE bool Interval::isPositive() const { return numUnits > 0; }

    INLINE Interval::Interval(int64_t numUnits_, TimeUnit unitType_)
        : numUnits(numUnits_), unitType(unitType_)
    {}
}