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

#include "dxcommon.h"
#include <stdint.h>
#include <sstream>
#include <string>

namespace DxApi {
    class SelectionOptions {
    public:
        int64_t             from;
        int64_t             to;
        bool                useCompression;
        bool                live;
        bool                reverse;
        bool                isBigEndian;
        bool                allowLateOutOfOrder;
        bool                realTimeNotification;
        bool                minLatency;
        Nullable<std::vector<std::string>> spaces;

    public:
        SelectionOptions() :
            from(INT64_MIN),
            to(INT64_MAX),
            useCompression(false),
            live(false),
            reverse(false),
            isBigEndian(true),
            allowLateOutOfOrder(false),
            realTimeNotification(false),
            minLatency(false),
            spaces()
        { };

        void withSpaces(const std::vector<std::string> *spaces) {
            if (spaces == nullptr) {
                this->spaces.reset();
            } else {
                this->spaces = *spaces;
            }
        }
    };
}