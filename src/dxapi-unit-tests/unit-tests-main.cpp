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
#define CATCH_CONFIG_RUNNER
#include "../catch.hpp"

#include "tests-common.h"
#include <dxapi/dxapi.h>


using namespace std;
using namespace DxApi;

//#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
//#include "catch.hpp"



int main( int argc, char* argv[] )
{
    Catch::Session session;

    set_stream_key_prefix("dxapi-cxx-tests-");
    set_timebase_host("dxtick://localhost:8022");
    set_timebase_home("dxapi_cxx_tests");

    if (argc > 2 && 0 == strcmp(argv[1], "--host")) {
        set_timebase_host(argv[2]);
        set_timebase_home("");

        forn (i, argc - 3) {
            argv[i + 1] = argv[i + 3];
        }

        argc -= 2;
    }

    int returnCode = session.applyCommandLine( argc, argv );
    if( returnCode != 0 )
        return returnCode;

    int numFailed = session.run();

    // Note that on unices only the lower 8 bits are usually used, clamping
    // the return value to 255 prevents false negative when some multiple
    // of 256 tests has failed
    return ( numFailed < 0xff ? numFailed : 0xff );
}