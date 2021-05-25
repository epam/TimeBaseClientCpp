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
#define _ALLOW_KEYWORD_MACROS

#include <string>
#include "tests-common.h"

#define private public
#define protected public

#include "tickdb/http/tickdb_http.h"
#include "tickdb/session_handler.h"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;

#define QSHOME_NAME "cpp_test_session_handler"

#if TEST_SESSION_HANDLER

SCENARIO("Session Handler object supports a set of methods for reading updated stream data from Timebase server", "[integration][session]") {
    GIVEN("Timebase connection and a stream") {
        auto tbi = start_instance(HOST, QSHOME_NAME);
        DB_OPEN_CHECKED(db, HOST, false);
        string key = "sh_stream1_test";
        string name = "Test - Session Handler Stream 1";

        delete_if_exists(db, key);
        TickStream *stream = db.createStream(key, name, name, 0);
        REQUIRE(nullptr != stream);
        
        WHEN("name property is requested") {
            impl(db).sessionHandler_.getPropertySynchronous(key, TickStreamPropertyId::NAME);
            THEN("original value is obtained") {
                REQUIRE(stream->name() == name);
            }
        }
        
// Does not work
#if 0
        WHEN("schema property is requested") {
            auto metadata = stream->metadata();
            auto ispoly = stream->polymorphic();
            impl(db).sessionHandler_.getPropertySynchronous(key, TickStreamPropertyId::SCHEMA);
            THEN("contents are still the same") {
                REQUIRE(stream->metadata() == metadata);
                REQUIRE(stream->polymorphic() == ispoly);
            }
        }
#endif

        delete_if_exists(db, key);
    }
}

#endif