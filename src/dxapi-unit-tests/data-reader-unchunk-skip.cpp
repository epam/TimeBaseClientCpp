
#include <string>
#include <math.h>
#include "tests-common.h"

// Gain access to private members of the classes being tested
#define private public
#define protected public

#include "tickdb/http/tickdb_http.h"
#include "tickdb/session_handler.h"
#include "tickdb/data_writer_impl.h"
#include "io/streams.h"
#include "io/input_preloader.h"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;

typedef uint8_t u8;

#include <math.h>


class IOReadTestStream : public InputStreamPreloader {
public:
    static const int MAX_READ_SIZE = 0x5000;
    virtual size_t read(u8 *to, size_t minSize, size_t maxSize) override;
    virtual size_t tryRead(u8 *to /* can be null */, size_t maxSize /* can be 0 */) override;
    virtual size_t tryRead(u8 *to, size_t maxSize, uint64_t timeout_us) override;
    IOReadTestStream& add(const vector<u8> &src)
    {
        this->set(dataSize_, &src[0], src.size());
        return *this;
    }

    IOReadTestStream(const vector<u8> &src) : InputStreamPreloader(NULL) {
        add(src);
    }

protected:
    static size_t readSize(size_t minSz, size_t maxSz)
    {
        int r = rand() % 8;
        size_t sz = 3 == r ? 1 : 5 == r ? MAX_READ_SIZE : rand() % (MAX_READ_SIZE + 1);
        return min(max(sz, minSz), maxSz);
    }
};


size_t IOReadTestStream::read(u8 *to, size_t minSize, size_t maxSize)
{
    return InputStreamPreloader::read(to, minSize, readSize(minSize, maxSize));
}

size_t IOReadTestStream::tryRead(u8 *to, size_t maxSize)
{
    return InputStreamPreloader::tryRead(to, readSize(1, maxSize));
}

size_t IOReadTestStream::tryRead(u8 *to, size_t maxSize, uint64_t timeout_us)
{
    return InputStreamPreloader::tryRead(to, readSize(1, maxSize), timeout_us);
}

void append(vector<u8> &a, const vector<u8> &b, size_t ofs, size_t n)
{
    size_t sz = a.size();
    a.resize(sz + n);
    memcpy(&a[sz], &b[ofs], n);
}

void append(vector<u8> &a, const u8 *buffer, size_t n)
{
    size_t sz = a.size();
    a.resize(sz + n);
    memcpy(&a[sz], buffer, n);
}

void append(vector<u8> &a, InputStream &readStream, size_t minSz, size_t maxSz)
{
    u8 buffer[0x10000];
    size_t sz = a.size();
    size_t szRead = readStream.read(buffer, minSz, min(maxSz, sizeof(buffer)));
    a.resize(sz + szRead);
    memcpy(&a[sz], buffer, szRead);
}

void append(vector<u8> &a, InputStream &readStream, size_t n)
{
    append(a, readStream, n, n);
}

void append_until(vector<u8> &a, InputStream &readStream, size_t n)
{
    while (a.size() < n) {
        append(a, readStream, 1, n - a.size());
    }
}

u8* append(vector<u8> &a, size_t n)
{
    size_t sz = a.size();
    a.resize(sz + n);
    return &a[sz];
}

size_t rand_quadratic(size_t from, size_t to)
{
    if (to / 3 * 4 < from)
        return rand_between(from, to);

    size_t x;
    do {
        x = (randu64() % (to + 1)) * (randu64() % (to + 1)) / (to + 1);
    } while (x < from);

    return x;
}

vector<u8> chunkify(const vector<u8> &in, size_t minSz, size_t maxSz)
{
    size_t rp = 0, n = in.size();
    vector<u8> out;
    while (rp < n) {
        size_t remaining = n - rp;
        size_t sz = min(remaining, rand_quadratic(minSz, maxSz));
        char buf[200];
        sprintf(buf, "\xD\xA%llx\xD\xA", (ulonglong)sz);
        string s(buf);
        for (char ch : s) {
            out.push_back(ch);
        }

        append(out, in, rp, sz);
        rp += sz;
    }

    return out;
}

void checkEqual(const vector<u8> &a, const vector<u8> &b)
{
    if (a.size() != b.size()) {
//        DBG_BREAK();
        REQUIRE(a.size() == b.size());
    }

    if (0 != memcmp(&a[0], &b[0], a.size())) {
//        DBG_BREAK();
        FAIL("memcmp(&a[0], &b[0], a.size())");
    }
}



typedef vector<unsigned> seg_list;
vector<unsigned> randSegments(size_t totalSize, size_t n,  ...) {
    va_list p;
    vector<size_t> lengths;
    unsigned type_present = 1;
    if (0 == n) {
        ++n;
        type_present = 0;
    }

    va_start(p, n);
    forn(i, n * 2) {
        lengths.push_back(va_arg(p, size_t));
    }

    va_end(p);

    vector<unsigned> out;
    while(0 != totalSize) {
        unsigned t = (unsigned)rand_between(0, n - 1);
        if (type_present) {
            out.push_back(t);
        }

        size_t sz = min(totalSize, rand_quadratic(lengths[t * 2], lengths[t * 2 + 1]));
        out.push_back((unsigned)sz);
        totalSize -= sz;
    }

//    std::swap(out[0], out[out.size() - 2]);
//    std::swap(out[1], out.back());
    return out;
}


enum class ReadMethod {
    getBytes,
    bytes
};

void readTo(ReadMethod rm, vector<u8> &a, DataReaderInternal &dr, size_t size)
{
    switch (rm) {
        case ReadMethod::getBytes: dr.getBytes(append(a, size), size); break;
        case ReadMethod::bytes: append(a, dr.bytes(size), size); break;
    }
}


void readBlocks(ReadMethod rm, const vector<u8> &a, DataReaderInternal &dr, const seg_list &segs)
{
    vector<u8> b;
    forn (i, segs.size()) {
        size_t sz = segs[i];
        readTo(rm, b, dr, segs[i]);
    }

    checkEqual(a, b);
}



void readOrSkipBlocks(ReadMethod rm, const vector<u8> &a, DataReaderInternal &dr, const seg_list &segs)
{
    vector<u8> b, c;
    size_t rdptr = 0;
    for (size_t i = 0, n = segs.size(); i < n; i += 2) {
        size_t sz = segs[i];
        if (0 == segs[i + 1]) {
            dr.skip(sz);
        } else {
            readTo(rm, b, dr, segs[i]);
            append(c, a, rdptr, sz);
        }
        rdptr += sz;
//        checkEqual(c, b); // Uncomment for debugging
    }
//    printf(" %d", (int)segs.size());
    checkEqual(c, b);
}


#define INIT_TEST(rdStream, a, buffer) \
    IOReadTestStream rdStream(a); \
    u8 buffer[READ_BUFFER_SIZE+200]; \
    memset(buffer,0, sizeof(buffer));

static struct RandomArrayProvider {
    vector<u8> get() {
        return data[n++ & 1];
    }

    RandomArrayProvider(size_t m) : n(0)
    {
        forn(i, 2) {
            randn(data[i], m);
        }
    }

    ~RandomArrayProvider()
    {
        printf("DBG: Data Reader test: Random array was requested %llu times.\n", (ulonglong)n);
    }

    vector<u8> data[2];
    size_t n;
} rndArray(0x200000);

void test_read_methods(vector<u8> &a, DataReaderBaseImpl &dr, size_t i);
void test_read_methods(const char *testName, vector<u8> &a, DataReaderBaseImpl &dr);

#define NUM_ITERATIONS 40

#if TEST_STREAM_READERS

// TODO: Test invalid chunks
// TODO: Test chunks with big amount of \0,CR,LF,CRLF

SCENARIO("DataReader subclasses can read data from sockets and HTTP streams", "[integration][dataio]") {
    GIVEN("A buffer of random data as read input") {
        vector<u8> a = rndArray.get(); // Alternating between 2 random arrays to make tests faster and allow more iterations

        WHEN("Read data via from IOStream simulator returning data in random portions") {
            printf("test IOStream/Preloader(): [");
            forn(i, NUM_ITERATIONS) {
                IOReadTestStream rdStream(a);
                vector<u8> b;
                append_until(b, rdStream, a.size());
                checkEqual(a, b);
                putchar('.');
            }

            puts("]");
        }


        GIVEN("DataReaderBaseImpl class") {
            INIT_TEST(rdStream, a, buffer);
            DataReaderBaseImpl dr(buffer + 100, sizeof(buffer) - 200, 0, &rdStream); // TODO: Check buffer guard areas

            test_read_methods("DataReaderBaseImpl", a, dr);

//            forn(i, NUM_ITERATIONS) {
//                char i_str[200]; sprintf(i_str, "# %llu", (ulonglong)i);
//                WHEN("Start") printf("DataReaderBaseImpl  getByte(),getBytes(),skip() tests: [");
//                test_read_methods(a, dr, i_str);
//                if (i_count - 1 == i) WHEN("End") puts("]");
//            }
        }

        GIVEN("DataReaderHTTPchunked class") {
//            WHEN("Begin") printf("DataReaderHTTPchunked getByte(),getBytes(),skip() with HTTP chunk stream..");
            GIVEN("Random sized HTTP chunks 1..33000") {
                INIT_TEST(rdStream, chunkify(a, 1, 33000), buffer);
                DataReaderHTTPchunked dr(buffer + 100, sizeof(buffer) - 200, 0, &rdStream);
                test_read_methods("DataReaderHTTPchunked(random chunks)", a, dr);
            }

            GIVEN("Tiny sized HTTP chunks (1..100)") {
                INIT_TEST(rdStream, chunkify(a, 1, 100), buffer);
                DataReaderHTTPchunked dr(buffer + 100, sizeof(buffer) - 200, 0, &rdStream);
                test_read_methods("DataReaderHTTPchunked(tiny random chunks)", a, dr);
            }

            GIVEN("Buffer-sized HTTP chunks (16200..16400)") {
                INIT_TEST(rdStream, chunkify(a, 16200, 16400), buffer);
                DataReaderHTTPchunked dr(buffer + 100, sizeof(buffer) - 200, 0, &rdStream);
                test_read_methods("DataReaderHTTPchunked(Buffer-sized random chunks)", a, dr);
            }

            GIVEN("Big HTTP chunks (1..0xFFFFFFF)") {
                INIT_TEST(rdStream, chunkify(a, 1, 0xFFFFFFF), buffer);
                DataReaderHTTPchunked dr(buffer + 100, sizeof(buffer) - 200, 0, &rdStream);
                test_read_methods("DataReaderHTTPchunked(Big random chunks)", a, dr);
            }
        }

//        GIVEN("DataReaderHTTPchunked class") {
//            WHEN("Begin") printf("DataReaderHTTPchunked getByte(),getBytes(),skip() tests with malformed buffer..");
//            GIVEN("Not chunked input buffer") {
//                INIT_TEST(rdStream, a, buffer);
//                DataReaderHTTPchunked dr(buffer + 100, sizeof(buffer) - 200, 0, &rdStream);
//                REQUIRE_THROWS(test_read_methods(a, dr, 0));
//            }
//
//            WHEN("End") printf("Ok\n");
//        }
    }
}

void test_read_methods(const char *testName, vector<u8> &a, DataReaderBaseImpl &dr)
{
    WHEN("Start") printf("%s  getByte(),getBytes(),skip() tests: [", testName);
    forn(i, NUM_ITERATIONS) {
        test_read_methods(a, dr, i);
        // if (i_count - 1 == i) WHEN("End") puts("]");
    }
    WHEN("End") puts("]");
}

void test_read_methods(vector<u8> &a, DataReaderBaseImpl &dr, size_t i)
{
    char i_str[200]; sprintf(i_str, " (iteration #%llu)", (ulonglong)i);
    vector<u8> b;

    WHEN(".getByte()" + i_str) {
        forn(i, a.size()) b.push_back(dr.getByte());
        checkEqual(a, b);
    }

    // Note, current getBytes impl. only generates get(1) calls
    WHEN(".getBytes(byte *, n)" + i_str) {
        b.resize(a.size());
        dr.getBytes(&b[0], b.size());
        checkEqual(a, b);
    }

    WHEN(".getBytes(1..20000 bytes)" + i_str) {
        readBlocks(ReadMethod::getBytes, a, dr, randSegments(a.size(), 0, 1, 20000));
    }
    WHEN(".getBytes(1..12 bytes)" + i_str) {
        readBlocks(ReadMethod::getBytes, a, dr, randSegments(a.size(), 0, 1, 12));
    }

    WHEN(".get(1..8 bytes)" + i_str) {
        readBlocks(ReadMethod::bytes, a, dr, randSegments(a.size(), 0, 1, 8));
    }

    WHEN(".get(1..8 bytes)" + i_str) {
        readBlocks(ReadMethod::bytes, a, dr, randSegments(a.size(), 0, 1, 8));
    }

//    WHEN(".get(1..17000 bytes throws exception)" + i_str) {
//        REQUIRE_THROWS(readBlocks(ReadMethod::bytes, a, dr, randSegments(a.size(), 0, 1, 17000)));
//    }

    WHEN(".get(1..16000 bytes)" + i_str) {
        readBlocks(ReadMethod::bytes, a, dr, randSegments(a.size(), 0, 1, 16000));
    }

    WHEN(".getBytes(byte *, n) or .skip(n), small blocks" + i_str) {
        readOrSkipBlocks(ReadMethod::getBytes, a, dr, randSegments(a.size(), 2, 1, 1000, 1, 1000));
    }
    WHEN(".getBytes(byte *, n) or .skip(n), big blocks only" + i_str) {
        readOrSkipBlocks(ReadMethod::getBytes, a, dr, randSegments(a.size(), 2, 10000, 16000, 20000, 200000));
    }
    WHEN(".getBytes(byte *, n) or .skip(n), medium size blocks" + i_str) {
        readOrSkipBlocks(ReadMethod::getBytes, a, dr, randSegments(a.size(), 2, 1, 0x4000, 1, 0x4000));
        putchar('.');
    }
}

#endif
