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
#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "tickstream_properties.h"

using namespace DxApiImpl;
using namespace DxApiImpl::TDB;

const char * infoTickStreamPropertyId[] = {
    "!!UNKNOWN!!",
    "KEY",
    "NAME",
    "DESCRIPTION",
    "PERIODICITY",
    "-SKIPPED-",
    "SCHEMA",
    "TIME_RANGE",
    "ENTITIES",
    "HIGH_AVAILABILITY",
    "UNIQUE",
    "BUFFER_OPTIONS",
    "VERSIONING",
    "DATA_VERSION",
    "REPLICA_VERSION",
    "BG_PROCESS",
    "WRITER_CREATED",
    "WRITER_CLOSED",
    "-SKIPPED-",
    "-SKIPPED-",
    "SCOPE",
    "DF",
    "OWNER",

    // number of properties
    "_COUNT_"
};


IMPLEMENT_ENUM(byte, TickStreamPropertyId, false)
{}