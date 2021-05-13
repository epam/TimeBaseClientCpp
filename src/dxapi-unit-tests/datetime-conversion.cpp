#include "../catch.hpp"

#if TEST_DATETIME_SCALING && !_DEBUG
SCENARIO("timestamp conversion done via double precision FP multiplication and integer division should match") {
    GIVEN("Sufficiently big unix nano timestamp range") {
        // 2200-01-01
        int64_t year1970 = 0;
        int64_t year2200 = 7258107600000LL * 1000000;

        WHEN("We are iterating through all nanoseconds in a first minute of 1970-01-01") {
            int64_t range_length = 2 * 1000000000LL;
            int64_t range_start = year1970;
            int64_t range_start_ticks = range_start / 100;
            puts("\n1970:");
            THEN("converted tick timestamp should match for two conversion methods") {
                for (auto i = range_start, end = range_start + range_length; i < end; ++i) {
                    int64_t intTicks = i / 100;
                    int64_t doubleTicks = i * 0.01;

                    //printf("\r%lld          ", i);
                    if (0 == (i & 0xFFFFFF)) {
                        printf("\r%lld        ", i);
                    }

                    // Remove the constant part
                    intTicks -= range_start_ticks;
                    doubleTicks -= range_start_ticks;
                    if (!(intTicks == doubleTicks)) {
                        REQUIRE(intTicks == doubleTicks);
                    }
                }
            }
        }

        WHEN("We are iterating through all nanoseconds in a first minute of 2200-01-01") {
            int64_t range_length = 2 * 1000000000LL;
            int64_t range_start = year2200;
            int64_t range_start_ticks = range_start / 100;
            puts("\n2200:");
            THEN("converted tick timestamp should match for two conversion methods") {
                for (auto i = range_start, end = range_start + range_length; i < end; ++i) {
                    int64_t intTicks = i / 100;
                    int64_t doubleTicks = i * 0.01;

                    printf("\r%lld          ", i);
                    if (0 == (i & 0xFFFFFF)) {
                        printf("\r%lld", i);
                    }

                    // Remove the constant part
                    intTicks -= range_start_ticks;
                    doubleTicks -= range_start_ticks;
                    if (!(intTicks == doubleTicks)) {
                        REQUIRE(intTicks == doubleTicks);
                    }
                }
            }
        }
    }
}

#endif