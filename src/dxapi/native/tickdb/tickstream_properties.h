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

#include <dxapi/dxapi.h>

namespace DxApiImpl {
    namespace TDB {
        struct TickStreamPropertyIdEnum {
            enum Enum {
                UNKNOWN = 0,
                KEY = 1,
                NAME = 2,
                DESCRIPTION = 3,
                PERIODICITY = 4,
                SCHEMA = 6,
                TIME_RANGE = 7,
                ENTITIES = 8,
                HIGH_AVAILABILITY = 9,
                UNIQUE = 10,
                BUFFER_OPTIONS = 11,

                VERSIONING = 12,
                DATA_VERSION = 13,
                REPLICA_VERSION = 14,

                BG_PROCESS = 15,
                WRITER_CREATED = 16,
                WRITER_CLOSED = 17,

                SCOPE = 20,
                DF = 21,

                OWNER = 22,

                // number of properties
                _COUNT_ = 23,
                INVALID = 0xFF
            };
        };

        ENUM_CLASS(uint8_t, TickStreamPropertyId);

        typedef uint32_t TickStreamPropertyMask;

        static_assert(TickStreamPropertyIdEnum::_COUNT_ < BITSIZEOF(TickStreamPropertyMask), "Enum members don't fit into the bitset");
    }
}