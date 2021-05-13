 #define _CRT_SECURE_NO_WARNINGS

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "tickdb/http/xml/stream/schema_request.h"

#include <conio.h>
#include <memory>
//#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>

using namespace std;
using namespace DxApi;


//#define HOST "dxtick://10.10.81.15:8122"
//#define HOST "dxtick://localhost:36743"
//#define HOST "dxtick://localhost:8200"
#define HOST "dxtick://localhost:8022"

// Run some test for internal implementation classes
//#define INTERNAL_TESTS

//#define LIST_TYPES
//#define TEST_STREAM_LIST
//#define TEST_XML
//#define TEST_MULTI
//#define TEST_BARS
//#define TEST_READ_TYPES
#define TEST_L2
//#define TEST_BARS_WRITE

#define PRINT if(0)
//#define PRINT puts

#define PRELOAD

#define _rdtsc()  __rdtsc()
#define _CDECL __cdecl

extern bool test_internals();

static double timetodouble(uint64_t t)
{
    return 1E-9 * t;
}

#define USER "admin"
#define PASS "AdminAdmin"


#define DB_OPEN_CHECKED(host, isReadOnly)                               \
    unique_ptr<DxApi::TickDb> pDb(DxApi::TickDb::createFromUrl(host, USER, PASS));            \
    DxApi::TickDb &db = *pDb;                                                \
    if (!db.open(isReadOnly)) { string s("Unable to open DxTickDB at "); s.append(host);  throw std::runtime_error(s); }; \
    printf("Connected to: %s isReadOnly: %d\n", host, (int)isReadOnly);


extern void schema_dump_types();
void list_types()
{
    try {
        DB_OPEN_CHECKED(HOST, true);
        auto streams = db.listStreams();

        for (auto stream : streams) {
            DxApiImpl::SchemaRequest s(stream);
            s.getClassDescriptors();
        }
    }
    catch (exception &e) {
        printf("\nEXCEPTION: %s\n", e.what());
    }
    //schema_dump_types();
}



bool list_test()
{
    try {
        DB_OPEN_CHECKED(HOST, true);
        auto streams = db.listStreams();

        for (auto const &i : streams) {
            TickStream &stream = *i;
            printf("STREAM: %s\n\t", stream.key().c_str());
            auto entities = stream.listEntities();
            Interval interval;
            stream.getPeriodicity(&interval);

            printf("\tPERIODICITY: %lld %s", (long long)interval.numUnits, interval.unitType.toString());

            int64_t range[2];
            stream.getTimeRange(range);
            printf("\tRANGE: %lld %lld", (long long)range[0], (long long)range[1]);
            
            stream.getTimeRange(range, entities);
            printf("\tRANGE(ALL): %lld %lld", (long long)range[0], (long long)range[1]);

            if (0 != entities.size()) {
                vector<InstrumentIdentity> entities2;
                entities2.push_back(entities[0]);
                stream.getTimeRange(range, entities2);
                printf("\tRANGE(%s): %lld %lld", entities2[0].symbol.c_str(), (long long)range[0], (long long)range[1]);
            }

            for (auto const &entity : entities) {
                printf(" %s", entity.symbol.c_str());
            }
            puts("");

        }
        puts("<end>");
    }
    catch (exception &e) {
        printf("\nEXCEPTION: %s\n", e.what());
    }
    return true;
}

template<typename T>
static inline std::string toString(const T &x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}


#define MY_BAR_ID 1

class Bar : public MarketMessage {
public:
    //char exchangeCode[16];
    std::string exchangeCode;
    double close;
    double open;
    double high;
    double low;
    double volume;


    bool hasCurrencyCode;
    bool hasClose;
    bool hasOpen;
    bool hasHigh;
    bool hasLow;
    bool hasVolume;

    virtual const std::string toString() const {
        std::stringstream ss;
        ss << MarketMessage::toString() << " o:" << open << " h:" << high << " l:" << low << " c:" << close << " v:" << volume;
        return ss.str();
    }
};

#define START_TEST(x) \
    uint64_t nMessagesRead = 0; \
    uint64_t start = time_ns(); \
    puts("Reading " #x);

#define START_TEST_W(x) \
    uint64_t nMessagesRead = 0; \
    uint64_t start = time_ns(); \
    puts("Writing " #x);


#define END_READING(name) do { end_read_test(#name, nMessagesRead, cursor.nBytesReceived(), cursor.nBytesRead(), start, time_ns()); } while (0)
#define END_WRITING(name) do { end_write_test(#name, nMessagesWritten, loader.nBytesWritten(), start, time_ns()); } while (0)

static void end_read_test(const char *testname, uint64_t nMessages, uint64_t nRecv, uint64_t nRead, uint64_t start, uint64_t end)
{
    printf("Bytes received : %llu\n", (long long unsigned)nRecv);
    printf("Bytes read     : %llu, avg msg size: %.1lf bytes\n", (long long unsigned)nRead, (double)nRead / nMessages);
    printf("Messages read  : %.0lf\n", 1.0 * nMessages);
    printf("Decoding speed : %.1lf bytes/s\n", (double)nRead / timetodouble(end - start));
    printf("Elapsed time   : %lfs, %lf msg/s\n", timetodouble(end - start), nMessages / timetodouble(end - start));
    puts("");
}

static void end_write_test(const char *testname, uint64_t nMessages, uint64_t nWritten,  uint64_t start, uint64_t end)
{
    printf("Bytes written    : %llu, avg msg size: %.1lf bytes\n", (long long unsigned)nWritten, (double)nWritten / nMessages);
    printf("Messages written : %.0lf\n", 1.0 * nMessages);
    printf("Encoding speed   : %.1lf bytes/s\n", (double)nWritten / timetodouble(end - start));
    printf("Elapsed time     : %lfs, %lf msg/s\n", timetodouble(end - start), nMessages / timetodouble(end - start));
    puts("");
}


void preload(TickCursor & cursor, uintptr_t preloadSize, uintptr_t blockSize = 0x2000)
{
    if (0 != preloadSize) {
        printf("Preloading %llu bytes, blocksize=%llu ...", (long long unsigned) preloadSize, (long long unsigned) blockSize);
        START_TEST(...);
        uintptr_t preloaded = cursor.preload(preloadSize, blockSize);
        double td = timetodouble(time_ns() - start);
        printf("Done in %.3lf s, %.0lf bytes/s\n", td, preloaded / td);
        fflush(stdout);
    }
}

/* This test stream uses decimal(4) */
bool test_bars_fmt1(TickDb& db) {
    const char *messageNames[] = { "Unknown", "deltix.qsrv.hf.pub.BarMessage" };
    static const uintptr_t preloadSize = 300000000;
    auto stream = db.getStream("test");

    SelectionOptions opt;
    /*vector<string> types;
    vector<InstrumentIdentity> symbols;*/
    //opt.setTypeTransmission(DxApi::DEFINITION);
    unique_ptr<TickCursor> pCursor(stream->select(TIMESTAMP_NULL, opt, NULL, NULL));
    TickCursor &cursor = *pCursor;

    Bar msg;

    for (intptr_t i = 0; i < COUNTOF(messageNames); ++i)
        cursor.registerMessageType((unsigned)i, messageNames[i]);

#ifdef PRELOAD
    preload(cursor, preloadSize, 0x400);
#endif

    DataReader &reader = cursor.getReader();
    START_TEST(test bars);
    msg.cursor = &cursor;
    while (cursor.next(&msg) && nMessagesRead < 5000000) {

        switch (msg.typeIdLocal) {
        case MY_BAR_ID:
            msg.hasCurrencyCode = INT16_NULL != (msg.currencyCode = reader.readInt16());

            msg.hasClose =
                reader.readDecimal(msg.close);
            msg.hasOpen =
                reader.readDecimal(msg.open);
            msg.hasHigh =
                reader.readDecimal(msg.high);
            msg.hasLow =
                reader.readDecimal(msg.low);
            msg.hasVolume =
                reader.readDecimal(msg.volume);

            msg.open += msg.close;
            msg.high += msg.close;
            msg.low += msg.close;

            // Print contents
            //puts(msg.toString().c_str());
            break;
        default:
            printf("Unknown message. id: %d -> localId: %d", (int)msg.typeId, (int)msg.typeIdLocal);
            //puts(msg.MarketMessage::toString().c_str());
        }
        ++nMessagesRead;
    }

    //uint64_t end = time_ns();
    //printf("Bytes received: %llu\n", (long long unsigned)cursor.getNumBytesReceived());
    //printf("Bytes read: %llu, avg msg size: %.1lf bytes\n", (long long unsigned)cursor.getNumBytesRead(), (double)cursor.getNumBytesRead() / nMessagesRead);
    //printf("Decoding speed: %.1lf bytes/s\n", (double)cursor.getNumBytesRead() / timetodouble(end - start));

    END_READING(test bars);
    return true;
}


bool test_read_dynamic_types(TickDb &db, const char *streamName, uint64_t nMax, uintptr_t preloadSize)
{
    vector<string> messageNames;
    vector<string> types;
    vector<uint64_t> numMsg;
    auto stream = db.getStream(streamName);
    SelectionOptions opt;
    /*vector<InstrumentIdentity> symbols;*/
    unique_ptr<TickCursor> pCursor(stream->select(TIMESTAMP_NULL, opt, NULL, NULL));
    TickCursor &cursor = *pCursor;

    InstrumentMessage msg;

    //messageNames.push_back("<unknown>");
    //numMsg.push_back(0);
    //cursor.registerMessageType(0, messageNames[0]);
    
#ifdef PRELOAD
    preload(cursor, preloadSize, 0x400);
#endif

    DataReader &reader = cursor.getReader();
    START_TEST(test dynamic);
    msg.cursor = &cursor;
    while (cursor.next(&msg) && nMessagesRead < nMax) {
        if (numMsg.size() > msg.typeId) {
            // Do nothing
            ++numMsg[msg.typeId];
        }
        else {
            printf("%s\n", msg.getTypeName()->c_str());
            //cursor.registerMessageType((unsigned)messageNames.size(), msg.getTypeName()->c_str());
            messageNames.resize(msg.typeId + 1);
            numMsg.resize(msg.typeId + 1);
            numMsg[msg.typeId] = 1;
            messageNames[msg.typeId] = *msg.getTypeName();
        }
        ++nMessagesRead;
    }

    //uint64_t end = time_ns();
    //printf("Bytes received: %llu\n", (long long unsigned)cursor.getNumBytesReceived());
    //printf("Bytes read: %llu, avg msg size: %.1lf bytes\n", (long long unsigned)cursor.getNumBytesRead(), (double)cursor.getNumBytesRead() / nMessagesRead);
    //printf("Decoding speed: %.1lf bytes/s\n", (double)cursor.getNumBytesRead() / timetodouble(end - start));

    END_READING(test bars);
    printf("Number of message types: %u\n", (unsigned)messageNames.size());
    puts("Number of messages read:");
    for (int i = 0; i < (int)messageNames.size(); ++i) {
        printf("%llu\t%s\n", numMsg[i], messageNames[i].c_str());
    }
    puts("");
    cursor.close();

    return true;
}


/* This test stream uses uncompressed doubles for decimal fields */
bool test_bars_fmt2(TickDb &db) {
    const char *messageNames[] = { "Unknown", "deltix.qsrv.hf.pub.BarMessage" };
    static const uintptr_t preloadSize = 200000000;

    auto stream = db.getStream("bars1min");

    SelectionOptions opt;
    /*vector<string> types;
    vector<InstrumentIdentity> symbols;*/

    unique_ptr<TickCursor> pCursor(stream->select(TIMESTAMP_NULL, opt, NULL, NULL));
    TickCursor &cursor = *pCursor;

    Bar msg;

    for (intptr_t i = 0; i < COUNTOF(messageNames); ++i)
        cursor.registerMessageType((unsigned)i, messageNames[i]);

#ifdef PRELOAD
    preload(cursor, preloadSize, 0x400);
#endif

    START_TEST(bars1min);
    DataReader &reader = cursor.getReader();

    static volatile double y = 0.0; // these vars are for fighting compiler optimizations
    double x = 0;

    msg.volume = 0.0;
    msg.cursor = &cursor;
    while (cursor.next(&msg) && nMessagesRead < 5000000) {
        switch (msg.typeIdLocal) {
        case MY_BAR_ID:
            //msg.originalTimestamp = cursor.getInt64();
            //msg.currencyCode = cursor.getInt16();
            //msg.sequenceNumber = cursor.getInt64();

            //msg.hasExchangeCode = 
            //cursor.getAlphanumeric(msg.exchangeCode, 10);

            /*
            msg.hasClose =
                cursor.getDecimal(msg.close);
            msg.hasOpen =
                cursor.getDecimal(msg.open);
            msg.hasHigh =
                cursor.getDecimal(msg.high);
            msg.hasLow =
                cursor.getDecimal(msg.low);
            msg.hasVolume =
                cursor.getDecimal(msg.volume);
                */

            msg.hasClose =
                reader.readDecimal(msg.close);
            msg.hasOpen =
                reader.readDecimal(msg.open);
            msg.hasHigh =
                reader.readDecimal(msg.high);
            //msg.low = 0;
            msg.hasLow = reader.readDecimal(msg.low);
            msg.hasVolume = reader.readDecimal(msg.volume);

            //msg.hasVolume = reader->getDecimal(msg.volume);

            msg.open += msg.close;
            msg.high += msg.close;
            msg.low += msg.close;

            x += msg.open; // This is to prevent some extreme compiler optimizations
            // Print contents
            //puts(msg.toString().c_str());
            break;
        default:
            printf("Unknown message. id: %d -> localId: %d", (int)msg.typeId, (int)msg.typeIdLocal);
            puts(msg.MarketMessage::toString().c_str());
        }
        ++nMessagesRead;

        //if (9210617 == nMessagesRead) {
        //    nMessagesRead &= 0xFFFFFFFFF;
        //}
        //if (nMessagesRead > 9210591 /*&& nMessagesRead % 32 == 0*/)
            //printf("%llu ", (long long unsigned)nMessagesRead);
    }

    END_READING(bars1min);
    y = x;
    return true;
}


bool test_bars_multi1(TickDb& db) {
    const char *messageNames[] = { "Unknown", "deltix.qsrv.hf.pub.BarMessage" };
    static const uintptr_t preloadSize = 300000000;
    //vector<unique_ptr<TickStream>> streams;
    vector<TickStream *> streams;
    //streams.push_back(db->getStream("bars1min_w"));
    streams.push_back(db.getStream("bars"));
    streams.push_back(db.getStream("1123"));

    SelectionOptions opt;
    /*vector<string> types;
    vector<InstrumentIdentity> symbols;*/
    
    TickCursor * pCursor = db.select(INT64_MIN, streams, opt, NULL, NULL), &cursor = *pCursor;

    Bar msg;

    for (intptr_t i = 0; i < 1 /*COUNTOF(messageNames)*/; ++i)
        cursor.registerMessageType((unsigned)i, messageNames[i]);

#ifdef PRELOAD
    preload(cursor, preloadSize, 0x400);
#endif

    DataReader &reader = cursor.getReader();
    START_TEST(test bars);
    msg.cursor = &cursor;
    while (cursor.next(&msg) && nMessagesRead < 200000) {

        switch (msg.typeIdLocal) {
        case MY_BAR_ID:
            /*msg.currencyCode = reader.getInt16();

            msg.hasClose =
                reader.getDecimal(msg.close);
            msg.hasOpen =
                reader.getDecimal(msg.open);
            msg.hasHigh =
                reader.getDecimal(msg.high);
            msg.hasLow =
                reader.getDecimal(msg.low);
            msg.hasVolume =
                reader.getDecimal(msg.volume);

            msg.open += msg.close;
            msg.high += msg.close;
            msg.low += msg.close;*/

            // Print contents
            //puts(msg.toString().c_str());
            break;
        default:
            printf("Unknown message. id: %d -> localId: %d\n", (int)msg.typeId, (int)msg.typeIdLocal);
            //puts(msg.MarketMessage::toString().c_str());
            ;
        }
        ++nMessagesRead;
    }

    puts(::toString(msg.typeId).c_str());

    //uint64_t end = time_ns();
    //printf("Bytes received: %llu\n", (long long unsigned)cursor.getNumBytesReceived());
    //printf("Bytes read: %llu, avg msg size: %.1lf bytes\n", (long long unsigned)cursor.getNumBytesRead(), (double)cursor.getNumBytesRead() / nMessagesRead);
    //printf("Decoding speed: %.1lf bytes/s\n", (double)cursor.getNumBytesRead() / timetodouble(end - start));

    END_READING(test bars);
    delete pCursor;
    return true;
}



#define READ(x, type) x = READER.read##type()
#define READ_NULLABLE(x, type) has_##x = READER.read##type(x)
#define READ_NULLABLE2(x, type, parm0) has_##x = READER.read##type(x, parm0)


#ifdef TEST_L2
bool test_l2msg_fmt1(TickDb& db) {
    enum MsgType { UNKNOWN = 0, LEVEL2ACTION, CONNECTION_STATUS_CHANGE, L2MESSAGE, MF_RAW_MESSAGE, LEVEL2MESSAGE };
    const char *messageNames[] = { "Unknown", "deltix.qsrv.hf.pub.Level2Action", "deltix.qsrv.hf.spi.data.ConnectionStatusChangeMessage",
        "deltix.qsrv.hf.pub.L2Message", "deltix.qsrv.hf.plugins.data.mf.MarketFactoryRawMessage", "deltix.qsrv.hf.pub.Level2Message"
    };
    //TickStream stream = db->getStream("l2msg");
    unique_ptr<TickStream>pstream(db.getStream("l2msg2"));
    TickStream &stream = *pstream;
    class Level2Action {
        int    level;     // int16, non-nullable
        bool   isAsk;         // boolean, non-nullable
        int    action;    // enum 0..2
        double price;       // Float 64, nullable
        double size;        // Float 64, nullable
        int    numOfOrders;    // int32, nullable
        bool   has_action, has_price, has_size, has_numOfOrders;

    public:
        inline void readFrom(DataReader& reader)
        {
#define READER reader
            READ(level, Int16);
            READ(isAsk, Boolean);
            READ_NULLABLE(action, Enum8); // TODO:
            READ_NULLABLE(price, Float64);
            READ_NULLABLE(size, Float64);
            READ_NULLABLE(numOfOrders, Int32);
            assert(level < 21);
            assert(action < 20);
        }
        virtual std::string toString() const
        {
            char tmp[0x1000];
            sprintf(tmp, "\n\t%1d %c %c %lf %lf %d", level, "BA??"[isAsk], "_+*-???"[action + 1], price, size, has_numOfOrders ? numOfOrders : -1);
            return std::string(tmp);
        }
    };

    class ConnectionStatusChange : public MarketMessage {
        int status;
        std::string cause;
        bool has_cause, has_status;
    public:
        inline void readFrom(DataReader &reader)
        {
            READ_NULLABLE(status, Enum8);
            has_cause = reader.readUTF8(cause);
        }
        virtual std::string toString() const { return MarketMessage::toString().append(" cause:").append(cause); }
    };

    class L2Message : public MarketMessage {
        vector<Level2Action> actions;
        std::string exchangeCode;
        bool isImplied, isSnapshot;
        int64_t sequenceId;
        bool has_actions, has_exchangeCode, has_sequenceId;

    public:
        //void copyFrom(const MessageHeader &m) { *static_cast<MessageHeader *>(this) = m; }
        inline void readFrom(DataReader &reader)
        {
            READ_NULLABLE(originalTimestamp, Timestamp);
            READ_NULLABLE(currencyCode, Int16);
            READ_NULLABLE(sequenceNumber, Int64);

            intptr_t count;
            //{ reader.skipArray(); return; }
            count = reader.readArrayStart();
            has_actions = false;
            if (count >= 0) {
                has_actions = true;
                actions.resize(count);
                for (intptr_t i = 0; i < count; ++i) {
                    //reader.readInt16(); // Skip first 2 bytes for now
                    if (reader.readObjectStart() >= 0) {
                        Level2Action act;
                        act.readFrom(reader);
                        actions[i] = act;
                        reader.readObjectEnd();
                    }
                }
                READ_NULLABLE2(exchangeCode, Alphanumeric, 10);
                READ(isImplied, Boolean);
                READ(isSnapshot, Boolean);
                reader.readArrayEnd();
            }
            //READ_NULLABLE(sequenceId, Int64); // TODO: Sorry, will get to it later
        }

        virtual std::string toString() const
        {
            char tmp[0x1000];
            sprintf(tmp, " currency: %d, actions(%d): ", (int)currencyCode, (int)actions.size());
            std::string s = MarketMessage::toString().append(tmp);
            for (intptr_t i = 0, count = actions.size(); i < count; ++i) {
                s/*.append(i ? "," : "")*/.append(actions[i].toString());
            }
            return s;
        }
    };

    SelectionOptions opt;
    vector<string> types;
    vector<InstrumentIdentity> symbols;

    unique_ptr<TickCursor> pCursor(stream.select(TIMESTAMP_NULL, opt, NULL, NULL));
    TickCursor &cursor = *pCursor;

    for (intptr_t i = 0; i < COUNTOF(messageNames); ++i)
        cursor.registerMessageType((unsigned)i, messageNames[i]);

    //MessageHeader msg;
    MarketMessage marketMessage; // We are using this as a default type
    Level2Action l2Action;
    L2Message l2Message;
    ConnectionStatusChange connectionStatusChange;
    marketMessage.cursor = &cursor;
    START_TEST(L2Msg1);
    DataReader &reader = cursor.getReader();
    // Note: C++ API is not a priority, it will be made better-looking, but that code is not useful for Managed API, so it is not written yet.

    while (cursor.next(&marketMessage) && nMessagesRead < 5000000) {
        //MarketMessage *msg = &marketMessage;
        switch (marketMessage.typeIdLocal) {
        //case LEVEL2ACTION:
        case CONNECTION_STATUS_CHANGE:
            connectionStatusChange.copyHeaderFrom(marketMessage);
            connectionStatusChange.readFrom(reader);
            //msg = &connectionStatusChange;
            //puts(connectionStatusChange.toString().c_str());
            break;
        case L2MESSAGE:
            l2Message.copyHeaderFrom(marketMessage);
            l2Message.readFrom(reader);
            PRINT(l2Message.toString().c_str());
            break;
            break;
        case MF_RAW_MESSAGE:
        case LEVEL2MESSAGE:
            //puts(marketMessage.toString().append(*marketMessage.getTypeName()).c_str());
            break;
        default:
            printf("Unknown message <%s>. id: %d -> localId: %d", marketMessage.getTypeName()->c_str(), (int)marketMessage.typeId, (int)marketMessage.typeIdLocal);
            //puts(marketMessage.toString().c_str());
        }
        // Print contents
        //puts(msg->toString().c_str());
         ++nMessagesRead;
    }

    END_READING(L2Msg1);
    return true;
}

bool test_l2msg_fmt2(TickDb& db) {
    //static const uintptr_t preloadSize = 35000000;
    static const uintptr_t preloadSize = 24000000;
    enum MsgType { UNKNOWN = 0, LEVEL2ACTION, L2MESSAGE, LEVEL2MESSAGE };
    const char *messageNames[] = { "Unknown", "deltix.qsrv.hf.pub.Level2Action", 
        "deltix.qsrv.hf.pub.L2Message", "deltix.qsrv.hf.pub.Level2Message"
    };
    class Level2Action {
        int    level;                   // int8, non-nullable
        bool   isAsk;                   // boolean, non-nullable
        int    action;                  // enum 0..2
        double price;                   // Float 64, nullable
        double size;                    // Float 64, nullable
        int    numOfOrders;             // int32, nullable
        bool   has_price, has_size, has_numOfOrders;


    public:
        inline void readFrom(DataReader& reader)
        {
#define READER reader
            READ(level, Int8);
            READ(isAsk, Boolean);
            READ(action, Enum8); // TODO:
            READ_NULLABLE(price, Float64);
            READ_NULLABLE(size, Float64);
            READ_NULLABLE(numOfOrders, Int32);
        }
        virtual std::string toString() const
        {
            char tmp[0x1000];
            sprintf(tmp, "\n\t%1d %c %c %lf %lf %d", level, "BA??"[isAsk], "_+*-???"[action + 1], price, size, has_numOfOrders ? numOfOrders : -1);
            return std::string(tmp);
        }
    };

    class L2Message : public MarketMessage {
        vector<Level2Action> actions;
        std::string exchangeCode;
        bool isImplied, isSnapshot;
        int64_t sequenceId;
        bool has_actions, has_exchangeCode, has_sequenceId;

    public:
        inline void readFrom(DataReader &reader)
        {
            READ_NULLABLE(originalTimestamp, Timestamp);
            READ_NULLABLE(currencyCode, Int16);
            READ_NULLABLE(sequenceNumber, Int64);

            intptr_t count;
            //{ reader.skipArray(); return; }
            count = reader.readArrayStart();
            has_actions = false;
            if (count >= 0) {
                has_actions = true;
                actions.resize(count);
                for (intptr_t i = 0; i < count; ++i) {
                    if (reader.readObjectStart() >= 0) {
                        Level2Action act;
                        act.readFrom(reader);
                        actions[i] = act;
                        reader.readObjectEnd();
                    }
                }

                READ_NULLABLE2(exchangeCode, Alphanumeric, 10);
                READ(isImplied, Boolean);
                READ(isSnapshot, Boolean);
                READ_NULLABLE(sequenceId, Int64);
                reader.readArrayEnd();
            }
        }

        virtual std::string toString() const
        {
            char tmp[0x1000];
            sprintf(tmp, " currency: %d, actions(%d): ", (int)currencyCode, (int)actions.size());
            std::string s = MarketMessage::toString().append(tmp);
            for (intptr_t i = 0, count = actions.size(); i < count; ++i) {
                s/*.append(i ? "," : "")*/.append(actions[i].toString());
            }
            return s;
        }
    };

    class Level2Message : public MarketMessage {
        double price;                   // Float 64, nullable
        double size;                    // Float 64, nullable
        std::string exchangeCode;       // Alpha(10), nullable
        int8_t depth;                   // int8, nullable
        bool   isAsk;                   // boolean, non-nullable
        int    action;                  // enum 0..2
        bool   _isLast;                 // boolean, non-nullable
        int    numOfOrders;             // int32, nullable

        bool has_exchangeCode, has_price, has_size, has_depth, has_action, has_numOfOrders;

    public:
        inline void readFrom(DataReader &reader)
        {
            READ_NULLABLE(originalTimestamp, Timestamp);
            READ_NULLABLE(currencyCode, Int16);
            READ_NULLABLE(sequenceNumber, Int64);

            READ_NULLABLE(price, Float64);
            READ_NULLABLE(size, Float64);
            READ_NULLABLE2(exchangeCode, Alphanumeric, 10);
            READ_NULLABLE(depth, Int8);
            READ(isAsk, Boolean);
            READ(action, Enum8);
            READ(_isLast, Boolean);
            READ_NULLABLE(numOfOrders, Int32);
        }

        virtual std::string toString() const
        {
            return MarketMessage::toString().append("Price: ").append(::toString(price));
        }
    };

    SelectionOptions opt;

    //TickStream stream = db.getStream("l2msg");
    unique_ptr<TickStream>pstream(db.getStream("l2msg4"));
    TickStream &stream = *pstream;
    unique_ptr<TickCursor> pCursor(stream.select(TIMESTAMP_NULL, opt, NULL, NULL));
    TickCursor &cursor = *pCursor;

    for (intptr_t i = 0; i < COUNTOF(messageNames); ++i)
        cursor.registerMessageType((unsigned)i, messageNames[i]);

#ifdef PRELOAD
    preload(cursor, preloadSize, 0x400);
#endif

    //MessageHeader msg;
    MarketMessage marketMessage; // We are using this as a default type
    Level2Action l2Action;
    L2Message l2Message;
    Level2Message level2Message;

    START_TEST(L2Msg2);
    DataReader &reader = cursor.getReader();
    marketMessage.cursor = &cursor;
    // Note: C++ API is not a priority, it will be made better-looking, but that code is not useful for Managed API, so it is not written yet.

    while (cursor.next(&marketMessage) && nMessagesRead < 5000000) {
        switch (marketMessage.typeIdLocal) {
        case L2MESSAGE:
            l2Message.copyHeaderFrom(marketMessage);
            l2Message.readFrom(reader);
            PRINT(l2Message.toString().c_str());
            break;
        case LEVEL2MESSAGE:
            level2Message.copyHeaderFrom(marketMessage);
            level2Message.readFrom(reader);
            PRINT(level2Message.toString().c_str());
            break;
        default:
            printf("Unknown message <%s>. id: %d -> localId: %d", marketMessage.getTypeName()->c_str(), (int)marketMessage.typeId, (int)marketMessage.typeIdLocal);
            //puts(marketMessage.toString().c_str());
        }
        // Print contents
        //puts(msg->toString().c_str());
        ++nMessagesRead;
    }

    END_READING(L2Msg2);
    return true;
}


bool test_l2msg_fmt3(TickDb& db) {
    //static const uintptr_t preloadSize = 1000000000;
    static const uintptr_t preloadSize = 307000000;
    //static const uintptr_t preloadSize = 35000000;
    //static const uintptr_t preloadSize = 24000000;
    enum MsgType { UNKNOWN = 0, LEVEL2ACTION, L2MESSAGE, LEVEL2MESSAGE };
    const char *messageNames[] = { "Unknown", "deltix.qsrv.hf.pub.Level2Action",
        "deltix.qsrv.hf.pub.L2Message", "deltix.qsrv.hf.pub.Level2Message"
    };
    class Level2Action {
        int    level;                   // int8, non-nullable
        bool   isAsk;                   // boolean, non-nullable
        int    action;                  // enum 0..2
        double price;                   // Float 64, nullable
        double size;                    // Float 64, nullable
        int    numOfOrders;             // int32, nullable
        bool   is_null, has_price, has_size, has_numOfOrders;


    public:
        inline void readFrom(DataReader& reader)
        {
#define READER reader
            READ(level, Int8);
            READ(isAsk, Boolean);
            READ(action, Enum8); // TODO:
            READ_NULLABLE(price, Float64);
            READ_NULLABLE(size, Float64);
            READ_NULLABLE(numOfOrders, Int32);
        }
        virtual std::string toString() const
        {
            char tmp[0x1000];
            sprintf(tmp, "\n\t%1d %c %c %lf %lf %d", level, "BA??"[isAsk], "_+*-???"[action + 1], price, size, has_numOfOrders ? numOfOrders : -1);
            return std::string(tmp);
        }
    };

    class L2Message : public MarketMessage {
        vector<Nullable<Level2Action>> actions;
        std::string exchangeCode;
        bool isImplied, isSnapshot;
        int64_t sequenceId;
        bool has_actions, has_exchangeCode, has_sequenceId;

    public:
        inline void readFrom(DataReader &reader)
        {
            READ_NULLABLE(originalTimestamp, Timestamp);
            READ_NULLABLE(currencyCode, Int16);
            READ_NULLABLE(sequenceNumber, Int64);

            intptr_t count;
            //{ reader.skipArray(); return; }
            count = reader.readArrayStart();
            has_actions = false;
            if (count >= 0) {
                has_actions = true;
                actions.resize(count);
                for (intptr_t i = 0; i < count; ++i) {
                    if (reader.readObjectStart() >= 0) {
                        Level2Action act;
                        act.readFrom(reader);
                        actions[i] = act;
                        reader.readObjectEnd();
                    }
                    else {
                        DBG_BREAK();
                    }
                }

                reader.readArrayEnd();
            }

            READ_NULLABLE2(exchangeCode, Alphanumeric, 10);
            READ(isImplied, Boolean);
            READ(isSnapshot, Boolean);
            READ_NULLABLE(sequenceId, Int64);
        }

        virtual std::string toString() const
        {
            char tmp[0x1000];
            sprintf(tmp, " currency: %d, actions(%d): ", (int)currencyCode, (int)actions.size());
            std::string s = MarketMessage::toString().append(tmp);
            for (intptr_t i = 0, count = actions.size(); i < count; ++i) {
                s/*.append(i ? "," : "")*/.append(actions[i].has_value() ? actions[i].get().toString() : "<NULL>");
            }

            return s;
        }
    };

    class Level2Message : public MarketMessage {
        double price;                   // Float 64, nullable
        double size;                    // Float 64, nullable
        std::string exchangeCode;       // Alpha(10), nullable
        int8_t depth;                   // int8, nullable
        bool   isAsk;                   // boolean, non-nullable
        int    action;                  // enum 0..2
        bool   _isLast;                 // boolean, non-nullable
        int    numOfOrders;             // int32, nullable

        bool has_exchangeCode, has_price, has_size, has_depth, has_action, has_numOfOrders;

    public:
        inline void readFrom(DataReader &reader)
        {
            READ_NULLABLE(originalTimestamp, Timestamp);
            READ_NULLABLE(currencyCode, Int16);
            READ_NULLABLE(sequenceNumber, Int64);

            READ_NULLABLE(price, Float64);
            READ_NULLABLE(size, Float64);
            READ_NULLABLE2(exchangeCode, Alphanumeric, 10);
            READ_NULLABLE(depth, Int8);
            READ(isAsk, Boolean);
            READ(action, Enum8);
            READ(_isLast, Boolean);
            READ_NULLABLE(numOfOrders, Int32);
        }

        virtual std::string toString() const
        {
            return MarketMessage::toString().append("Price: ").append(::toString(price));
        }
    };

    SelectionOptions opt;

    //TickStream stream = db.getStream("l2msg");
    unique_ptr<TickStream>pstream(db.getStream("mftest"));
    TickStream &stream = *pstream;
    assert(&stream != NULL);
    unique_ptr<TickCursor> pCursor(stream.select(TIMESTAMP_NULL, opt, NULL, NULL));
    TickCursor &cursor = *pCursor;

    /*for (intptr_t i = 0; i < COUNTOF(messageNames); ++i)
        cursor.registerMessageType((unsigned)i, messageNames[i]);*/

#ifdef PRELOAD
    preload(cursor, preloadSize, 0x400);
#endif

    //MessageHeader msg;
    MarketMessage marketMessage; // We are using this as a default type
    Level2Action l2Action;
    L2Message l2Message;
    Level2Message level2Message;

    START_TEST(mftest);
    DataReader &reader = cursor.getReader();
    marketMessage.cursor = &cursor;
    // Note: C++ API is not a priority, it will be made better-looking, but that code is not useful for Managed API, so it is not written yet.

    int nSkip = 6;
    while (cursor.next(&marketMessage) && nMessagesRead < 5000000) {
        const char * name = marketMessage.getTypeName()->c_str();

        if (nSkip) {
            --nSkip;
        }
        else {
            if (strstr(name, "L2Message")) {
                l2Message.copyHeaderFrom(marketMessage);
                l2Message.readFrom(reader);
                PRINT(l2Message.toString().c_str());
            }
            else if (strstr(name, "Level2Message")) {
                level2Message.copyHeaderFrom(marketMessage);
                level2Message.readFrom(reader);
                PRINT(level2Message.toString().c_str());
            }
            else {
                printf("Unknown message <%s>. id: %d -> localId: %d", marketMessage.getTypeName()->c_str(), (int)marketMessage.typeId, (int)marketMessage.typeIdLocal);
                //puts(marketMessage.toString().c_str());
            }
            //_getch();
        }
        // Print contents
        //puts(msg->toString().c_str());
        ++nMessagesRead;
    }

    END_READING(mftest);
    return true;
}


#endif // #if defined(TEST_L2)


bool xml_tests()
{
    try {
        DB_OPEN_CHECKED(HOST, true);

        string response;
        (static_cast<DxApiImpl::TickDbImpl&>(db)).executeTbPostRequest(response,
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<getSchema version = \"3\">\n"
            "<stream>bars1min_wZXCVV</stream>\n"
            "</getSchema>\n",
            "GetSchema",
            false
            );
        printf("Response:\n%s\n", response.c_str());

        //db->executeRequest(response, "Hello, world");
        //printf("Response:\n%s\n", response.c_str());
    }
    catch (exception &e) {
        printf("\nEXCEPTION: %s\n", e.what());
    }

    return true;
}

bool read_tests()
{
    //auto x = db->getStream("bars").listEntities();

    try {
        DB_OPEN_CHECKED(HOST, true);

#ifdef TEST_MULTI
        test_bars_multi1(db);
#endif
#ifdef TEST_BARS
        test_bars_fmt1(db);
        test_bars_fmt2(db);
#endif
#ifdef TEST_L2
        //test_l2msg_fmt1(db);
        //test_l2msg_fmt2(db);
        test_l2msg_fmt3(db);
#endif
#ifdef TEST_READ_TYPES
        test_read_dynamic_types(db, "bars1min", 5000000, 100000);
        test_read_dynamic_types(db, "bars1min", 5000000, 100000);
#endif

    }
    catch (exception &e) {
        printf("\nEXCEPTION: %s\n", e.what());
    }

    return true;
}

/* This test stream uses uncompressed doubles for decimal fields */
bool test_bar_writer_fmt2(TickDb& db) {
    auto stream = db.getStream("bars1min_w");
    stream->clear();
    //TickStream stream = db->getStream("eurusd_delete");
    LoadingOptions opt;
    intptr_t nMessagesWritten;
    unique_ptr<TickLoader> pLoader(stream->createLoader(opt));
    TickLoader &loader = *pLoader;

    START_TEST_W(bars1min_w);

    //MessageHeader barHeader = loader->make();
#ifndef _DEBUG
#define NLDRMSG 5000000
#else
#define NLDRMSG 500000
#endif

    DataWriter &writer = loader.getWriter();

    for (nMessagesWritten = 0; nMessagesWritten < NLDRMSG; ++nMessagesWritten) {
        //DataWriter &writer =
        loader.next("deltix.qsrv.hf.pub.BarMessage", "DLTX", InstrumentType::EQUITY, DxApi::TIMESTAMP_NULL);
        //DataWriter &writer = loader.next();

        /*writer.putDecimal(DECIMAL_NULL);
        writer.putDecimal(DECIMAL_NULL);
        writer.putDecimal(DECIMAL_NULL);
        writer.putDecimal(DECIMAL_NULL);
        writer.putDecimal(DECIMAL_NULL);*/
        writer.writeDecimal(11.23);
        writer.writeDecimal(-0.5);
        writer.writeDecimal(1.22);
        writer.writeDecimal(-1);
        writer.writeDecimal(123456789.12345);

        //loader.send("deltix.qsrv.hf.pub.BarMessage", "DLTX", DxApi::EQUITY, DxApi::TIMESTAMP_NULL);
    }

    loader.finish();
    END_WRITING(bars1min_w);
    loader.close();
    return true;

}

bool write_tests()
{
    try {
        DB_OPEN_CHECKED(HOST, false);
#if defined(TEST_BARS_WRITE)
        test_bar_writer_fmt2(db);
#endif
    }
    catch (exception &e) {
        printf("\nEXCEPTION: %s\n", e.what());
    }

    return true;
}


int _CDECL main(int argc, char* argv[])
{

#ifdef INTERNAL_TESTS
    test_internals();
#endif

#ifdef LIST_TYPES
    list_types();
#endif

#ifdef TEST_STREAM_LIST
    list_test();
#endif

#ifdef TEST_XML
    xml_tests();
#endif

    read_tests();
    write_tests();
    
	return 0;
}
