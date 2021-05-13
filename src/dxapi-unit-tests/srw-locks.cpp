#include "tests-common.h"
#include "util/srw_lock.h"

using namespace std;
using namespace DxApi;
using namespace dx_thread;

#if (TEST_SRW_LOCK)

SCENARIO("SRW lock class should provide shared/exclusive locks in single threaded use case", "[integration][misc]") {
    GIVEN("An SRW lock") {
        srw_lock lock;
        WHEN("A shared lock is acquired via 'get'") {
            lock.get_shared();
            THEN("Another shared lock succeeds (1/2)") {
                REQUIRE(lock.try_shared());
                lock.release_shared();
            }

            THEN("Another shared lock succeeds (2/2)") {
                lock.get_shared();
                REQUIRE("Success");
                lock.release_shared();
            }

            THEN("Can grab many more shared locks") {
                int n_grabbed = 0;
                int N = 100;
                forn (i, N) {
                    if (!lock.try_shared())
                        break;
                    ++n_grabbed;
                }

                REQUIRE(n_grabbed == N);
                
                forn (i, n_grabbed) {
                    lock.release_shared();
                    --n_grabbed;
                }

                REQUIRE(0 == n_grabbed);
            }

            THEN("Exclusive lock fails") {
                REQUIRE(!lock.try_exclusive());
                //lock.release_exclusive(); <- will cause crash at this point
                
                // But shared still works, just in case
                REQUIRE(lock.try_shared());
                lock.release_shared();
            }

            lock.release_shared();
            REQUIRE("release_shared() suceeded");
        }

        WHEN("An exclusive lock is acquired via 'get'") {
            lock.get_exclusive();
            THEN("Shared lock fails") {
                REQUIRE(!lock.try_shared());
            }

            THEN("Another exclusive lock fails") {
                REQUIRE(!lock.try_exclusive());
            }

            lock.release_exclusive();
            REQUIRE("release_exclusive() suceeded");
        }

        WHEN("An exclusive lock is acquired via lock section") {
            // Section entered
            {
                srw_write section(lock);
                THEN("Shared lock fails") {
                    REQUIRE(!lock.try_shared());
                }

                THEN("Another exclusive lock fails") {
                    REQUIRE(!lock.try_exclusive());
                }
            }

            // Section exited
    
            REQUIRE("Able to exit lock section");

            THEN("After exit, shared lock works") {
                REQUIRE(lock.try_shared());
                lock.release_shared();
            }

            THEN("After exit, exclusive lock works") {
                REQUIRE(lock.try_exclusive());
                lock.release_exclusive();
            }
        }

        WHEN("A shared lock is acquired via lock section") {
            // Section entered
            {
                srw_read section(lock);
                //THEN("Shared lock suceeds")
                {
                    REQUIRE(lock.try_shared());
                    lock.release_shared();
                }

                //THEN("Exclusive lock fails")
                {
                    REQUIRE(!lock.try_exclusive());
                }
            }

            // Section exited
            REQUIRE("Able to exit lock section");

            THEN("After exit, shared lock works") {
                REQUIRE(lock.try_shared());
                lock.release_shared();
            }

            THEN("After exit, exclusive lock works") {
                REQUIRE(lock.try_exclusive());
                lock.release_exclusive();
            }
        }
    }
}


#endif // #if (TEST_SRW_LOCK)