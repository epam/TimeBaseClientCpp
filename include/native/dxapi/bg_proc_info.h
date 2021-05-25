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
#include "dxconstants.h"


namespace DxApi {

    struct ExecutionStatusEnum {
        enum Enum {
            NONE,
            RUNNING,
            COMPLETED,
            ABORTED,
            FAILED
        };
    };

    ENUM_CLASS(uint8_t, ExecutionStatus);

    class BackgroundProcessInfo {
    public:
        std::string name;
        ExecutionStatus status;

        // keys of affected streams
        // DxTickStream instead of string? 
        std::vector<std::string> affectedStreams;
        double progress;
        TimestampMs startTime;
        TimestampMs endTime;

        bool isFinished() const
        {
            return status == ExecutionStatus::COMPLETED     ||
                   status == ExecutionStatus::ABORTED       ||
                   status == ExecutionStatus::FAILED;
        }

    public:
        BackgroundProcessInfo() :
            status(ExecutionStatus::NONE), progress(0.0), startTime(TIMESTAMP_NULL), endTime(TIMESTAMP_NULL)
        {}
    };
}