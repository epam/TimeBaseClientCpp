#define _CRT_SECURE_NO_WARNINGS

#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <conio.h>
#include <memory>
#include "util/connection.h"

using namespace std;
using namespace DxApi;

//using SelectionOptions = Deltix.Timebase.Api.Communication.SelectionOptions;

#if 0
class Connection
{
public:
    static const int N = 8;

    TickDb *db;
    TickCursor *cursors[N];
    TickLoader *loaders[N];
    TickStream *streams[N * 2];
    int numCursors, numStreamRequests;

    Connection(const char *address, const char *streamName, int numStreamRequests = 0, int numCursors = 0)
    {
        memset(cursors, 0, sizeof(cursors));
        memset(loaders, 0, sizeof(loaders));
        memset(streams, 0, sizeof(streams));

        db = TickDb::createFromUrl(address, NULL, NULL);
        db->open(false);
        assert(db->isOpen());
        auto stream = streams[0] = db->getStream(streamName);
        assert(stream != NULL);

        SelectionOptions opt;
        cursors[0] = stream->createCursor(opt);
        db->listStreams();
        for (int i = 0; i < numStreamRequests; ++i)
        {
            Interval interv;
            TimestampMs range[2];

            switch (i % 4)
            {
            case 0:
                stream->getPeriodicity(&interv);
                break;
            case 1:
                stream->listEntities(NULL);
                break;
            case 2:
            case 3:
                stream->getTimeRange(range);
                break;
            }

            //Thread.Sleep(1);
        }
    }





    ~Connection()
    {
        for (auto i : cursors) {
            if (NULL != i) delete i;
        }

        for (auto i : loaders) {
            if (NULL != i) delete i;
        }

        for (auto i : streams) {
            if (NULL != i) delete i;
        }

        if (NULL != db) {
            delete db;
        }
    }
};

#endif

class LeakTest
{
    string hostAddress;
    string outputStream;
    string inputStream;

    vector<Connection> connections;
    int connectionLimit, newConnectionIndex;                // Limits the number of active connections
    long total;                                             // Total amount created during test
    int numStreamRequests;
    int numCursors;

    LeakTest(const char * host, const char * stream, const char * tempstream)
    {
        int connectionLimit = 0, numStreamRequests = 0, numCursors = 0;

        this->hostAddress = host;
        this->inputStream = stream;
        this->outputStream = tempstream;
        this->connectionLimit = connectionLimit;
        this->numCursors = numCursors;
        this->numStreamRequests = numStreamRequests;
        this->total = 0;

        newConnectionIndex = 0;
    }


    void run()
    {

        while (true)
        {
            if (0 != connectionLimit && connections.size() >= connectionLimit)
            {
                newConnectionIndex %= connectionLimit;
                connections.emplace(connections.begin() + newConnectionIndex, hostAddress.c_str());
            }
            else {
                connections.emplace_back(hostAddress.c_str());
            }

            auto & conn = connections[newConnectionIndex++];
            auto stream = conn.stream(inputStream.c_str());
            conn
                .numCursors(numCursors)
                .numLoaders(0)
                .generateRandomRequests(stream, numStreamRequests)
                .createCursorsFor(stream)
                ;

            ++total;
            printf("%llu/%llu", (ulonglong)connections.size(), (ulonglong)total);
            sleep_ns(1000000);
        }
    }

    void stop()
    {

    }
};

