#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <conio.h>
#include <memory>


class Connection
{
    typedef DxApi::TickCursor TickCursor;
    typedef DxApi::TickLoader TickLoader;
    typedef DxApi::TickStream TickStream;
    typedef DxApi::TickDb TickDb;
public:

    enum class SubscribeToAll {
        NO,
        YES
    };

    DxApi::TickDb *db;
    //int numCursors, numStreamRequests;


    Connection(const char *address, bool listSteams = true)
    {
        db = DxApi::TickDb::createFromUrl(address, NULL, NULL);
        db->open(false);
        dx_assert(db->isOpen(), "Should be able to open Timebase connection");
        streams = db->listStreams();
    }
    

    size_t numCursors() const
    {
        return cursors.size();
    }


    size_t numLoaders() const
    {
        return loaders.size();
    }


    Connection& numCursors(size_t n)
    {
        cursors.resize(n, nullptr);
        return *this;
    }


    Connection& numLoaders(size_t n)
    {
        loaders.resize(n, nullptr);
        return *this;
    }


    DxApi::TickStream * stream(const char *streamName)
    {
        return db->getStream(streamName);
    }


    Connection& createCursorsFor(DxApi::TickStream *stream, SubscribeToAll all = SubscribeToAll::YES)
    {
        dx_assert(stream != NULL, "Stream not opened/not found");
        DxApi::SelectionOptions opt;

        forn(i, cursors.size()) {
            if (all == SubscribeToAll::YES) {
                // Subscribe to nothing
                cursors[i] = stream->createCursor(opt);
            }
            else {
                // Subscribe to all
                cursors[i] = stream->select(DxApi::TIMESTAMP_NULL, opt, NULL, NULL);
            }
        }

        return *this;
    }


    Connection& generateRandomRequests(DxApi::TickStream *stream, size_t numStreamRequests = 0)
    {
        forn(i, numStreamRequests)
        {
            DxApi::Interval interv;
            DxApi::TimestampMs range[2];
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

        return *this;
    }


    void concurrentStopAllCursors(bool shouldDelete, bool shouldYield);
    void concurrentStopAllLoaders(bool shouldDelete, bool shouldYield);


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

protected:
    std::vector<DxApi::TickCursor *> cursors;
    std::vector<DxApi::TickLoader *> loaders;
    std::vector<DxApi::TickStream *> streams;

    //size_t numCursors_, numloaders_;
};