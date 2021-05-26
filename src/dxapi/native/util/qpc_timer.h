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

#include <stdint.h>

class QPCTimer {
public:
    int64_t getTicks();         // .NET 100ns ticks with base
    int64_t getTicksElapsed();  // .NET 100ns ticks without base (since start of init() call)
    int64_t getMillis();        // Unix epoch millis (with base)
    int64_t getMicros();        // Unix epoch micros
    int64_t getNanos();         // Unix epoch nanos
    void init(int64_t baseNanos = 0);

    // These methods do the same thing, but applied to the singleton static instance
    static int64_t ticks();         // .NET 100ns ticks with base
    static int64_t ticksElapsed();  // .NET 100ns ticks without base (since start of init() call)
    static int64_t millis();        // Unix epoch millis (with base)
    static int64_t micros();        // Unix epoch micros
    static int64_t nanos();         // Unix epoch nanos
    static void reset(int64_t baseNanos = 0);

public:
    int64_t baseDotNetTicks;
    int64_t baseMillis;         
    int64_t baseMicros;         
    int64_t baseNanos;
    
protected:
    /* not _that_ timebase */
    struct Timebase {
        uint64_t base;
        uint64_t freq;
    } timebase;
};

extern QPCTimer g_qpc_timer;