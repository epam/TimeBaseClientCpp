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


namespace DxApi {

    struct WriteModeEnum {
        enum Enum {
            APPEND,     /* Adds only new data into a stream without truncation */
            REPLACE,    /* Adds data into a stream and removes previous data older that new time */
            REWRITE,    /* Adds data into a stream and removes previous data by truncating using first new message time */
            TRUNCATE    /* Stream truncated every time when loader writes messages earlier that last */
        };
    };

    ENUM_CLASS(uint8_t, WriteMode);

    class LoadingOptions {
    public:
        WriteMode writeMode;
        bool raw;
        bool minLatency;

    public:
        LoadingOptions() : writeMode(WriteMode::REWRITE), raw(true), minLatency(false) {};
    };
}