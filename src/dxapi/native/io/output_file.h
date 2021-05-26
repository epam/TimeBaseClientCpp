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

#include "streams.h"

namespace DxApiImpl {
    class OutputStreamFileLogger : public OutputStream {
    public:
        virtual size_t write(const byte *data, size_t data_size) override;
        virtual uint64 nBytesWritten() const override;
        bool open(const char * name);

        OutputStreamFileLogger();
        OutputStreamFileLogger(OutputStream *outStream);
        virtual ~OutputStreamFileLogger();

    private:
        FILE            *outputFile;        // We log to this file
        OutputStream    *outputStream;      // We pass calls to this stream
    };
}