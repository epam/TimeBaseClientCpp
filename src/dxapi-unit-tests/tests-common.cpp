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
#include "tests-common.h" 

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;

typedef uint8_t u8;

string g_stream_key_prefix, g_timebase_home, g_timebase_host;


uint64_t randu64()
{
    return RAND_RANGE64 * (RAND_RANGE64 * (RAND_RANGE64 * (RAND_RANGE64 * rand() + rand()) + rand()) + rand()) + rand();
}


void randn(u8 *to, size_t n)
{
    forn(i, n) to[i] = (u8)(rand() >> 4);
}


void randn(std::vector<u8> &v, size_t n)
{
    v.resize(n);
    return randn(v.data(), n);
}


size_t rand_between(size_t from, size_t to)
{
    to = to - from + 1;
    return from + randu64() % to;
}

void delete_if_exists(TickDb &db, const string &key)
{
    auto s = db.getStream(key);
    if (NULL != s) {
        s->deleteStream();
    }
}


TimebaseInstance get_tb_instance()
{
    TimebaseInstance tbi(g_timebase_host.c_str(), g_timebase_home.c_str(), 0 != g_timebase_home.length() ? false : true);
    tbi.start();
    return move(tbi);
}


TimebaseInstance start_instance(const char *host, const char *subdir)
{
    string dir = TimebaseRemoteStarter::getTmpHome();
    _mkdir(dir.c_str());
    dir.append("/").append(subdir);
    puts(dir.c_str());
    _mkdir(dir.c_str());
    TimebaseInstance tbi(host, dir.c_str());
    tbi.start();
    return move(tbi);
}


void set_timebase_host(const char *host)
{
    g_timebase_host = host;
}


void set_timebase_home(const char *home)
{
    g_timebase_home = home;
}


void set_stream_key_prefix(const char *prefix)
{
    g_stream_key_prefix = prefix;
}


bool stream_key_equals(const string& this_stream, const string& other_stream)
{
    auto &prefix = g_stream_key_prefix;

    if (this_stream == other_stream) {
        return this_stream.substr(0, prefix.length()) == prefix;
    }

    string tmp(prefix);
    tmp.append(other_stream);

    return this_stream == other_stream;
}


string make_stream_key(const string &key)
{
    string out(g_stream_key_prefix);
    out.append(key);
    return out;
}
