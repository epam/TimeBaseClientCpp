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
#define _CRT_SECURE_NO_WARNINGS
#include "tests-common.h"
#include <dxapi/dxapi.h>
#include "util/srw_lock.h"

#define QSHOME_NAME "cpp_test_timebase_starter"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace dx_thread;


void printdirs(const std::string &tbdir, const std::string &tmphomedir)
{
    static bool printed;
    if (!printed) {
        printed = true;
        printf("\nDirs:\n%s\n%s\n", tbdir.c_str(), tmphomedir.c_str());
    }
}


#if TEST_TB_STARTER
SCENARIO(" Should be able to start Timebase instances in DELTIX_HOME/temp", "[integration][timebase][priority][priority2][timebase-starter]") {
    WHEN("Checking DELTIX_HOME and other dirs") {
        const char * dh = getenv("DELTIX_HOME");
        dh = dh ? dh : "<NULL>";
        printf("\nDELTIX_HOME='%s'\n", dh);
        REQUIRE("DH" != dh);
        char dir[0x2000];
        dh = _getcwd(dir, sizeof(dir));
        REQUIRE("CWD" != dh);
        REQUIRE("CWD" != dir);
    }

    WHEN("Getting working directories") {
        auto tbdir = TimebaseRemoteStarter::getDeltixHome();
        CHECK("" != tbdir);
        auto tmphomedir = TimebaseRemoteStarter::getTmpHome();
        REQUIRE("" != tmphomedir);
        printdirs(tbdir, tmphomedir);
    }

    WHEN("Trying to stop orphaned TB starter") {
        TimebaseRemoteStarter::stop(HOST);
        THEN("Timebase starter is not running") {
            REQUIRE(!TimebaseRemoteStarter::isRunning(HOST));
        }
    }

    WHEN("Starting timebase instance") {
        auto tbi = start_instance(HOST, QSHOME_NAME);
        THEN("Is running") {
            REQUIRE(tbi.isRunning());
            WHEN("Stopping") {
                tbi.stop();
                THEN("Should stop") {
                    REQUIRE(!tbi.isRunning());
                }
            }
        }
    }

    WHEN("Starting timebase instance w/o protocol name") {
        auto tbi = start_instance(HOST_NO_PROTO, QSHOME_NAME);
        THEN("Is running") {
            REQUIRE(tbi.isRunning());
            WHEN("Stopping") {
                tbi.stop();
                THEN("Should stop") {
                    REQUIRE(!tbi.isRunning());
                }
            }
        }
    }
}

#endif // #if TEST_TB_STARTER