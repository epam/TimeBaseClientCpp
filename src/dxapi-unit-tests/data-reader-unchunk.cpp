
#include <string>
#include "tests-common.h"

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

// TODO: Microsoft-specific

//#define TRY {bool crashed = false; __try
//#define CATCH_CRASH __except(1) { crashed = true; REQUIRE(!"crashed")}
#if 0
#define TRY __try {
#define RETURN_CRASHED } __except(1) {return false; } return true;
#define RETURN_NOT_CRASHED } __except(1) {return true; } return false;
#else
#define TRY {
#define RETURN_CRASHED } return false;
#define RETURN_NOT_CRASHED } return true;
#endif


// TODO: merge copy-pasted methods into one


// Return random timebase bool
int8_t rndbool()
{
    auto x = rand() % 3;
    return x == 2 ? 0xFF : x;
}


struct MsgDesc {
    uint32_t length;

    uint32_t nSuccess;
    vector<FieldTypeEnum::Enum> fields;

    // Encode an array of types
    void enc(DataWriter &w, const byte * p, size_t n)
    {
        auto end = p + n;
        nSuccess = 0;
        for (auto t : fields) {
            assert((unsigned)t <= n);
            switch (t) {
#define C(ETYPE, TYPE, CTYPE, SZ) case FieldTypeEnum::ETYPE:                                                \
                w.write##TYPE(*(CTYPE *)p << 8 * (sizeof(CTYPE) - SZ) >> 8 * (sizeof(CTYPE) - SZ));         \
                assert(p + SZ <= end); p += SZ; break;

                C(INT8, Int8, int8_t, 1);
                C(INT16, Int16, int16_t, 2);
                C(INT32, Int32, int32_t, 4);
                C(INT48, Int48, int64_t, 6);
                C(INT64, Int64, int64_t, 8);

#undef C
#define C(ETYPE, TYPE, CTYPE, SZ) case FieldTypeEnum::ETYPE:                                                \
                w.write##TYPE(*(CTYPE *)p);                                                                 \
                assert(p + SZ <= end); p += SZ; break;

                C(BOOL, Int8, int8_t, 1);
                C(CHAR, Int16, int16_t, 2);
                C(FLOAT32, Float32, float, 4);
                C(FLOAT64, Float64, double, 8);
#undef C
            default:
                throw runtime_error("This type is not supported");
            }

            ++nSuccess;
        }
    }


    void dec(DataReader &r, std::vector<byte> &v)
    {
        nSuccess = 0;
        for (auto t : fields) {
            size_t sz = v.size();
            v.resize(sz + 16);
            size_t m = 0;
            byte * p = v.data() + sz;

            switch (t) {
#define C(ETYPE, TYPE, CTYPE, SZ) case FieldTypeEnum::ETYPE:            \
                *(CTYPE *)p = r.read##TYPE();                           \
                 m = SZ; break;

                C(INT8, Int8, int8_t, 1);
                C(INT16, Int16, int16_t, 2);
                C(INT32, Int32, int32_t, 4);
                C(INT48, Int48, int64_t, 6);
                C(INT64, Int64, int64_t, 8);

                C(BOOL, NullableBooleanInt8, int8_t, 1);
                C(CHAR, Int16, int16_t, 2);
                C(FLOAT32, Float32, float, 4);
                C(FLOAT64, Float64, double, 8);
#undef C
                default:
                    throw runtime_error("This type is not supported");
            }

            v.resize(sz + m);
            ++nSuccess;
        }
    }

    void genBasic(std::vector<byte> &v, size_t n)
    {
        FieldTypeEnum::Enum types[9] = {
            FieldTypeEnum::INT8, FieldTypeEnum::INT16, FieldTypeEnum::INT32, FieldTypeEnum::INT48, FieldTypeEnum::INT64,
            FieldTypeEnum::BOOL, FieldTypeEnum::CHAR, FieldTypeEnum::FLOAT32, FieldTypeEnum::FLOAT64
        };
        FieldTypeEnum::Enum t;

        this->fields.clear();
        forn(i, n) {
            t = (FieldTypeEnum::Enum)(types[rand() % COUNTOF(types)]);
            size_t sz = v.size();
            this->fields.push_back(t);
            v.resize(sz + 16);
            size_t m = 0;
            byte * p = v.data() + sz;
            switch (t) {
#define C(ETYPE, TYPE, CTYPE, SZ, F) case FieldTypeEnum::ETYPE:            \
                *(CTYPE *)p = (CTYPE)F;                         \
                m = SZ; break;

                C(INT8, Int8, int8_t, 1, randu64());
                C(INT16, Int16, int16_t, 2, randu64());
                C(INT32, Int32, int32_t, 4, randu64());
                C(INT48, Int48, int64_t, 6, randu64());
                C(INT64, Int64, int64_t, 8, randu64());

                C(BOOL, NullableBooleanInt8, int8_t, 1, rndbool());
                C(CHAR, Int16, int16_t, 2, randu64());
                C(FLOAT32, Float32, float, 4, randu64());
                C(FLOAT64, Float64, double, 8, randu64());
#undef C
                default:
                    throw runtime_error("This type is not supported");
            }

            v.resize(sz + m);
        }

        length = (uint32_t)v.size();
    }
#undef W

    bool verify(const byte * a, const byte * b)
    {
        return false; // TODO:
    }
};


struct EncDec {
    MsgDesc msg;
    DataWriter &writer;
    DataReader &reader;
    vector<byte> src;
    vector<byte> dest;
    InputStreamPreloader inputStream;

    size_t nFields;
    size_t nFieldsSkipMax;

    bool execute();
    void check();

    EncDec(size_t nFields__, size_t nFieldsSkipMax__, DataWriter &writer__, DataReader &reader__ /*, InputStreamPreloader &inputStream__*/) : 
        writer(writer__), reader(reader__), inputStream(NULL),
        nFields(nFields__), nFieldsSkipMax(nFieldsSkipMax__)
    {
    }

};

bool EncDec::execute()
{
    msg.genBasic(src, nFields);

    printf("[%u] %u", (unsigned)msg.fields.size(), (unsigned)src.size());

    // Encode message into writer
    impl(writer).resetPointer();
    msg.enc(writer, src.data(), src.size());
    // Move message from writer directly into preloader
    ((DataReaderBaseImpl&)(reader)).setInputStream(&inputStream);
    inputStream.set(0, impl(writer).dataStart(), impl(writer).dataSize());
    // decode the message (should access preloader)
    msg.dec(reader, dest);


    return true;
}

void EncDec::check()
{
    THEN("Encoding and decoding should execute") {
        REQUIRE(execute());
        THEN("Encoded and Decoded data should match") {
            REQUIRE(msg.nSuccess == msg.fields.size());
            REQUIRE(0 == memcmp(src, dest));
        }
    }
}


//bool encode_decode(MsgDesc &msg, DataWriter &writer, DataReader &reader, vector<byte> &src, vector<byte> &dest)
//{
//    
//}


bool encode_decode_does_not_crash(MsgDesc &msg, DataWriter &writer, DataReader &reader, vector<byte> &src, vector<byte> &dest, InputStreamPreloader &inputStream)
{
    TRY {
        msg.genBasic(src, 0x100000);

        printf("[%u] %u", (unsigned)msg.fields.size(), (unsigned)src.size());


        // Encode message into writer
        impl(writer).resetPointer();
        msg.enc(writer, src.data(), src.size());
        // Move message from writer directly into preloader
        inputStream.set(0, impl(writer).dataStart(), impl(writer).dataSize());
        // decode the message (should access preloader)
        msg.dec(reader, dest);
        return true;

        //return encode_decode_works(msg, writer, reader, src, dest);
    } RETURN_CRASHED;
}


bool encode_decode_does_not_crash(EncDec &e)
{
    TRY {
        e.check();
        return true;
    } RETURN_CRASHED;
}


bool encode_decode_works_correctly(MsgDesc &msg, DataWriter &writer, DataReader &reader, vector<byte> &src, vector<byte> &dest, InputStreamPreloader &inputStream)
{
    REQUIRE(encode_decode_does_not_crash(msg, writer, reader, src, dest, inputStream));

    REQUIRE(msg.nSuccess == msg.fields.size());
    REQUIRE(0 == memcmp(src, dest));
    return true;
}


#if TEST_DATA_RW

SCENARIO("Data Reader offers and API for reading binary messages wrapped in HTTP chunks, directly from a socket read buffer", "[integration][dataio]") {
    GIVEN("A DataWriter and a write buffer") {
        vector<byte> writeBuffer;
        srand((unsigned)time_us());
        writeBuffer.resize(0x500000); // 5MB max
        DataWriterInternal writerImpl;
        writerImpl.setBuffer(writeBuffer.data(), writeBuffer.data(), writeBuffer.data() + writeBuffer.size());
        DataWriter &writer = writerImpl;

        GIVEN("A Basic DataReader, initially empty, & preloader") {
            InputStreamPreloader inputStream(NULL);
            byte readerBuffer[0x1000];
            DataReaderBaseImpl reader(readerBuffer, sizeof(readerBuffer) - 0x10, 0, &inputStream);

            GIVEN("A vector of random basic values") {
                EncDec test(0x100000, 0, writer, reader);
                //MsgDesc msg;
                //vector<byte> src, dest;

                WHEN("encoded&decoded") {
                    //TODO:
                    REQUIRE(encode_decode_does_not_crash(test));
                    //REQUIRE(encode_decode_works_correctly(msg, writer, reader, src, dest, inputStream));
                }
            }
        }
        

        /*GIVEN("A HTTP DataReader") {
            InputStreamPreloader inputStream(NULL);
            byte readerBuffer[0x1000];
            DataReaderHTTPchunked reader(readerBuffer, sizeof(readerBuffer) - 0x10, 0, &inputStream);

            GIVEN("A vector of random basic values") {
                MsgDesc msg;
                vector<byte> src, dest;

                WHEN("The test data is encoded and decoded") {
                    REQUIRE(encode_decode_works_correctly(msg, writer, reader, src, dest, inputStream));
                }
            }
        }*/
    }
}

#endif