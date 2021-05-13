#include "tests-common.h"

using namespace std;
using namespace DxApi;

SCENARIO("split() function will split const char * into up to n std::string objects", "[integration][misc]") {
    GIVEN("Given NULL dest array") {
        string *out = NULL;
        THEN("0 is always returned") {
            REQUIRE(split(out, 0, NULL, 'Z') == 0);
            REQUIRE(split(out, 0, "Abcdef", '\0') == 0);
            REQUIRE(split(out, 123213213, "Abcdef", '\0') == 0);
        }
    }

    GIVEN("Given dest array of size 0") {
        string out[1];
        THEN("0 is always returned") {
            REQUIRE(split(out, 0, NULL, 'Z') == 0);
            REQUIRE(split(out, 0, "Abcdef", '\0') == 0);
            REQUIRE(split(out, 0, "Abcdef", '\0') == 0);
        }
    }

    GIVEN("Given destination array of size 1") {
        string out[1];

        WHEN("Source string is empty") {
            THEN("1 empty string is returned") {
                REQUIRE(1 == split(out, COUNTOF(out), "", 'Z'));
                REQUIRE(0 == out[0].size());
            }
        }

        WHEN("Source string is not empty") {
            const char * src = "All your base are belong to us!";

            WHEN("Separator does not present in the string") {
                REQUIRE(nullptr == strchr(src, 'Z'));
                THEN("the same string is returned") {
                    REQUIRE(1 == split(out, COUNTOF(out), src, 'Z'));
                    REQUIRE(0 == strcmp(src, out[0].c_str()));
                }
            }

            WHEN("Separator does present in the string") {
                REQUIRE(src + 3 == strchr(src, ' '));
                THEN("the same string is returned") {
                    REQUIRE(1 == split(out, COUNTOF(out), src, ' '));
                    REQUIRE(out[0] == src);
                }
            }
        }
    }

    GIVEN("Given destination array of size 2") {
        string out[2];

        WHEN("Source string is empty") {
            THEN("1 empty string is returned") {
                REQUIRE(1 == split(out, COUNTOF(out), "", 'Z'));
                REQUIRE(out[0] == "");
            }
        }

        WHEN("Source string is not empty") {
            const char * src = "All your base are belong to us!";

            WHEN("Separator does not present in the string") {
                REQUIRE(nullptr == strchr(src, 'Z'));
                THEN("the same string is returned") {
                    REQUIRE(1 == split(out, COUNTOF(out), src, 'Z'));
                    REQUIRE(out[0] == src);
                }
            }

            WHEN("Separator does present in the string") {
                char sep = ' ';
                REQUIRE(src + 3 == strchr(src, sep));

                THEN("2 strings are returned") {
                    REQUIRE(2 == split(out, COUNTOF(out), src, sep));
                    REQUIRE(out[0] == "All");
                    REQUIRE(out[1] == "your base are belong to us!");
                }
            }
        }
    }

    GIVEN("Given destination array of sufficiently big size") {
        string out[100];

        WHEN("Source string is not empty") {
            const char * src = "All your base are belong to us!";

            WHEN("Separator does present in the string 6 times") {
                char sep = ' ';
                REQUIRE(src + 3 == strchr(src, sep));

                THEN("7 strings are returned") {
                    REQUIRE(7 == split(out, COUNTOF(out), src, sep));
                    REQUIRE(out[0] == "All");
                    REQUIRE(out[1] == "your");
                    REQUIRE(out[5] == "to");
                    REQUIRE(out[6] == "us!");
                }
            }

            WHEN("String ends with separator") {
                char sep = '!';
                REQUIRE(nullptr != strchr(src, sep));
                REQUIRE(strchr(src, sep) == src + strlen(src) - 1);

                THEN("2 strings are returned, and the 2nd one is empty") {
                    REQUIRE(2 == split(out, COUNTOF(out), src, sep));
                    REQUIRE(out[0] == string(src, src + (strlen(src) - 1)));
                    REQUIRE(out[1] == "");
                }
            }
        }

    }
}

