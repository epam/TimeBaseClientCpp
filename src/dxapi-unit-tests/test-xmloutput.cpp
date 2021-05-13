#include "tests-common.h"
#include "tickdb/http/xml/xml_request.h"
#include <string>

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::XmlGen;

#if (TEST_XMLOUTPUT)

SCENARIO("XmlOutput class generates XML request text via set of overloaded methods", "[integration][misc]") {

    const char *testString = "Abcd¿·‚„‰&\"'<>0123456789 ";
    const char *testStringEscaped = "Abcd¿·‚„‰&amp;&quot;&apos;&lt;&gt;0123456789 ";
    const char *tagName = "tag";

    GIVEN("XmlOutput and a C string, containing disallowed XML characters") {
        XmlOutput o;

        string testStdString(testString);
        string testStdStringEscaped(testStringEscaped);

        string tagNameStdString(tagName);
        string testStdStringEscapedInsideTag = string("<") + tagName + ">" + testStringEscaped + string("</") + tagName + ">";
        const char *testStringEscapedInsideTag =  testStdStringEscapedInsideTag.c_str();

        WHEN(".put(const char *)") {
            o.put(testString);
            THEN("output is unescaped") {
                REQUIRE(o.str() == testString);
            }
        }

        WHEN(".put(const char *, length)") {
            o.put(testString, strlen(testString));
            THEN("output is unescaped") {
                REQUIRE(o.str() == testString);
            }
        }

        WHEN(".ostream() << (const char *)") {
            o.ostream() << testString;
            THEN("output is unescaped") {
                REQUIRE(o.str() == testString);
            }
        }

        WHEN(".addChars(const char *, length)") {
            o.addChars(testString, strlen(testString));
            THEN("output is escaped") {
                REQUIRE(o.str() == testStringEscaped);
            }
        }

        WHEN("<< (const char *)") {
            o << testString;
            THEN("output is escaped") {
                REQUIRE(o.str() == testStringEscaped);
            }
        }

        WHEN(".add(const char *)") {
            o.add(testString);

            THEN("output is escaped") {
                REQUIRE(o.str() == testStringEscaped);
            }
        }

        WHEN(".add(tag, const char *)") {
            o.add(tagName, testString);

            THEN("output is escaped") {
                REQUIRE(o.str() == testStdStringEscapedInsideTag);
            }
        }
    }


    GIVEN("XmlOutput and std::string, containing disallowed XML characters") {
        XmlOutput o;
        
        string testStdString(testString);
        string testStdStringEscaped(testStringEscaped);

        string tagNameStdString(tagName);
        string testStdStringEscapedInsideTag = string("<") + tagName + ">" + testStringEscaped + string("</") + tagName + ">";
        const char *testStringEscapedInsideTag =  testStdStringEscapedInsideTag.c_str();

        WHEN(".ostream() << (std::string&)") {
            o.ostream() << testStdString;
            THEN("output is unescaped") {
                REQUIRE(o.str() == testString);
            }
        }

        WHEN("<< (std::string&)") {
            o << testStdString;
            THEN("output is escaped") {
                REQUIRE(o.str() == testStringEscaped);
            }
        }

        WHEN(".add(std::string&)") {
            o.add(testStdString);

            THEN("output is escaped") {
                REQUIRE(o.str() == testStringEscaped);
            }
        }

        WHEN(".add(tag, std::string&)") {
            o.add(tagName, testStdString);

            THEN("output is escaped") {
                REQUIRE(o.str() == testStdStringEscapedInsideTag);
            }
        }
    }


    GIVEN("XmlOutput and empty Nullable<std::string>") {
        XmlOutput o;
        string testStdStringEscapedInsideTag = string("<") + tagName + "></" + tagName + ">";

        WHEN(".add(Nullable<string>&(nullptr))") {
            o.add(Nullable<string>());

            THEN("We get nothing") {
                REQUIRE(o.str() == "");
            }
        }

        WHEN(".add(Nullable<string>&(nullptr))") {
            o.add(tagName, Nullable<string>());

            THEN("We get XML element with empty content") {
                REQUIRE(o.str() == testStdStringEscapedInsideTag);
            }
        }
    }


    GIVEN("XmlOutput and Nullable<std::string>, containing disallowed XML characters") {
        XmlOutput o;

        string testStdString(testString);
        string testStdStringEscaped(testStringEscaped);

        string tagNameStdString(tagName);
        string testStdStringEscapedInsideTag = string("<") + tagName + ">" + testStringEscaped + string("</") + tagName + ">";

        WHEN(".add(Nullable<string>&)") {
            o.add(Nullable<string>(testStdString));

            THEN("output is escaped") {
                REQUIRE(o.str() == testStdStringEscaped);
            }
        }

        WHEN("<< (Nullable<string>&)") {
            o << Nullable<string>(testStdString);

            THEN("output is escaped") {
                REQUIRE(o.str() == testStdStringEscaped);
            }
        }

        WHEN(".add(tag, Nullable<string>&)") {
            o.add(tagName, Nullable<string>(testStdString));

            THEN("output is escaped") {
                REQUIRE(o.str() == testStdStringEscapedInsideTag);
            }
        }
    }
}

#endif // #if (TEST_XMLOUTPUT)