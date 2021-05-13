#include "tests-common.h"
using namespace DxApiImpl;

using namespace std;
using namespace DxApi;

// TODO: Microsoft-specific

//#define TRY {bool crashed = false; __try
//#define CATCH_CRASH __except(1) { crashed = true; REQUIRE(!"crashed")}


#ifdef _WIN32
#define TRY __try {
#define RETURN_HAS_CRASHED }        __except(1) {return true; } return false;
#define RETURN_HAS_NOT_CRASHED }    __except(1) {return false; } return true;

#else
#define TRY try {
#define RETURN_HAS_CRASHED }        catch(...) {return true; } return false;
#define RETURN_HAS_NOT_CRASHED }    catch(...) {return false; } return true;
#endif


bool generic_null_ptr_dereference_fails()
{
    TRY {
        volatile auto x = *(uint8_t volatile *)0;
    } RETURN_HAS_CRASHED;
}


// call 'delete' on TickStream* multiple times
bool stream_obj_multi_delete_works(TickStream * stream, int n)
{
    TRY {
        forn(i, n) {
            delete stream;
        }
    } RETURN_HAS_NOT_CRASHED;
}


// just call delete on TickStream
bool stream_obj_delete_works(TickStream * stream)
{
    TRY {
        delete stream;
    } RETURN_HAS_NOT_CRASHED;
}


// call delete on TickStream array
bool stream_obj_delete_works(TickStream * streams[], size_t n)
{
    TRY {
        forn(i, n) {
            delete streams[i];
        }
    } RETURN_HAS_NOT_CRASHED;
}


// call delete on TickStream array
bool stream_obj_delete_works(vector<TickStream *> streams)
{
    return stream_obj_delete_works((TickStream **)streams.data(), streams.size());
}

#if TEST_TICKSTREAMS
SCENARIO("TickStreams are created and managed by TickDb, 'TickStream *' ignores C++ delete()", "[integration][timebase][timebase-streams]") {
    auto tbi = get_tb_instance(); //start_instance(HOST, QSHOME_NAME);
    //puts("TickStreams are created and managed..");
    //GIVEN("NULL pointer") {
    //    //puts("GIVEN(\"NULL pointer\")");
    //    WHEN("Accessing NULL pointer") {
    //        THEN("Crash") {
    //            REQUIRE(generic_null_ptr_dereference_fails());
    //        }
    //    }
    //}

    GIVEN("Timebase connection") {
        DB_OPEN_CHECKED(db, HOST, false);
        //puts("GIVEN(\"Timebase connection\")");
        GIVEN("NULL stream") {
            TickStream * stream = nullptr;
            WHEN("NULL stream view object is deleted many times") {
                THEN("Nothing happens (no crash)") {
                    REQUIRE(stream_obj_multi_delete_works(stream, 10000));
                }
            }
        }


        GIVEN("A valid stream") {
            TickStream * stream = db.listStreams()[0];
            REQUIRE(nullptr != stream);
            REQUIRE(stream->key().length() != 0);

            WHEN("Valid stream view object deleted many times") {
                THEN("Nothing happens (no crash)") {
                    REQUIRE(stream_obj_multi_delete_works(stream, 10000));
                }
            }
        }

        GIVEN("A list of existing streams") {
            auto streams = db.listStreams();
            REQUIRE(streams.size() > 0);
            WHEN("Valid stream view object is deleted many times") {
                THEN("Nothing happens (no crash)") {
                    forn(j, 4) {
                        REQUIRE(stream_obj_delete_works(streams));
                    }
                }
            }

            WHEN("We make several getStream(key) calls with the same key") {
                TickStream * duplicates[10];
                THEN("All returned 'TickStream *' point to the same stream view object") {
                    forn(j, COUNTOF(duplicates)) {
                        duplicates[j] = db.getStream(streams[0]->key());
                    }

                    forn(j, COUNTOF(duplicates)) {
                        REQUIRE((void *)duplicates[j] == (void *)streams[0]);
                    }
                }
            }

            WHEN("We call ListStreams a few times") {
                TickStream * duplicates[10];
                db.listStreams();
                db.listStreams();
                auto streams2 = db.listStreams();
                THEN("All returned 'TickStream *' point to the same stream view object") {
                    forn(j, COUNTOF(duplicates)) {
                        duplicates[j] = db.getStream(streams[0]->key());
                        REQUIRE(duplicates[j] != nullptr);
                    }

                    forn(j, COUNTOF(duplicates)) {
                        REQUIRE((void *)duplicates[j] == (void *)streams[0]);
                    }
                }

                THEN("Streams in the new list match streams in the old list") {
                    TickStream * match = nullptr;
                    duplicates[0] = db.getStream(streams[0]->key());
                    forn(j, streams2.size()) {
                        REQUIRE(streams2[j] != nullptr);
                        if (duplicates[0]->key() == streams2[j]->key()) {
                            match = streams2[j];
                            break;
                        }
                    }

                    REQUIRE(nullptr != match);
                    REQUIRE((void *)match == (void *)duplicates[0]);
                }
            }
        }

        GIVEN("Some stream object pointers, pointing to random memory") {
            TickStream * streams[10000];
            forn(i, COUNTOF(streams)) streams[i] = (TickStream *)randu64();

            WHEN("A garbage pointer is deleted many times") {
                THEN("Nothing happens (no crash)") {
                    forn(j, 4) {
                        REQUIRE(stream_obj_delete_works(streams, COUNTOF(streams)));
                    }
                }
            }
        }

        db.close();
    }
}

#endif