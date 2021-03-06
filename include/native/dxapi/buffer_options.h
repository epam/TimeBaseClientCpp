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


namespace DxApi {
    class BufferOptions {
    public:
        /**
         *  Initial size of the write buffer in bytes.
         */
        uint32_t initialBufferSize;

        /**
         *  The limit on buffer growth in bytes. Default is 64K.
         */
        uint32_t maxBufferSize;
        
        /**
         * The limit on buffer growth as difference between first
         * and last message time. Default is INT64_MAX.
         */
        uint64_t maxBufferTimeDepth;


        /**
         * Applicable to transient streams only. When set to <tt>true</tt>,
         * the loader will be delayed until all currently open cursors
         * have read enough messages to free up space in
         * the buffer. When set to <tt>false</tt>,
         * older messages will be discarded after the buffer is filled up regardless
         * of whether there are open cursors that have not yet read such messages.
         * Default is <tt>false</tt>. Durable streams are always lossless.
         */
        bool lossless;

        //bool operator==(const BufferOptions &other) const;
        bool operator==(const BufferOptions &other) const
        {
            return initialBufferSize == other.initialBufferSize
                && maxBufferSize == other.maxBufferSize
                && maxBufferTimeDepth == other.maxBufferTimeDepth
                && lossless == other.lossless;
        }

        BufferOptions() :
            initialBufferSize(0x2000),
            maxBufferSize(0x10000),
            maxBufferTimeDepth(INT64_MAX),
            lossless(false)
        { }
    };


    
}