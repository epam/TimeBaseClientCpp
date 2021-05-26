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
//#include <conio.h>

#define _CRT_SECURE_NO_WARNINGS

#include "../catch.hpp"
#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "tickdb/timebase_starter.h"
#include "config-tests.h"


#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
static int _mkdir(const char *dir)
{
    return mkdir(dir, (S_IFDIR | 0666));
}

#endif


#define DB_OPEN_CHECKED(DB, host, isReadOnly)                                                       \
    unique_ptr<DxApi::TickDb> p_##DB(DxApi::TickDb::createFromUrl(host, USER, PASS));               \
    DxApi::TickDb &DB = *p_##DB;                                                                    \
    REQUIRE(p_##DB != nullptr);                                                                     \
    { bool openSuccess = DB.open(isReadOnly); REQUIRE(openSuccess == true); }


#define DB_OPEN_INSTANCE(DB, host, subdir, isReadOnly)                                              \
    DxApiImpl::TimebaseInstance tbi(host, subdir);                                                             \
    REQUIRE(tbi.start());                                                                           \
    unique_ptr<DxApi::TickDb> p_##DB(DxApi::TickDb::createFromUrl(host, USER, PASS));               \
    DxApi::TickDb &DB = *p_##DB;                                                                    \
    REQUIRE(p_##DB != nullptr);                                                                     \
    { bool openSuccess = DB.open(isReadOnly); REQUIRE(openSuccess == true); }


#define forn(i, n) for (intptr_t i = 0, i##_count = (intptr_t)(n); i != i##_count; ++i)

#define RAND_RANGE64 ((uint64_t)(RAND_MAX + 1))

uint64_t randu64();

void randn(byte * to, size_t n);

void randn(std::vector<byte> &v, size_t n);

void delete_if_exists(DxApi::TickDb &db, const std::string &streamKey);

//DxApiImpl::TimebaseInstance start_instance(const char * host, const char * subdir);

DxApiImpl::TimebaseInstance get_tb_instance();

// If length of host == 0, assuming "external"
void set_timebase_home(const char *home);

void set_timebase_host(const char *host);

std::string stream_key_prefix();

void set_stream_key_prefix(const char *prefix);

bool stream_key_equals(const std::string &other);

bool stream_key_equals(const char *other);

std::string make_stream_key(const std::string &key);