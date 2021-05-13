#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "util/srw_lock.h"
#include "util/critical_section.h"
#include "util/concurrent_object_set.h"
#include "io/streams.h"
#include "data_writer_impl.h"
#include "tickstream_properties.h"

#if defined(ERROR)
#error Remove Windows.h!!!
#endif

#if defined(NO_DATA)
#undef NO_DATA
#endif


#include <thread>

namespace DxApiImpl {
    class TickDbImpl;
    class TickStreamCache;

    namespace TDB {
        class SessionHandler {
        public:
            // Returns true if this instance is running
            bool isRunning() const;

            void stop();

            void start();

            void getStreamsSynchronous();
            void getStreamSynchronous(const char * key);
            void getStreamSynchronous(const std::string &key);
            void getStreamsSynchronous(const std::vector<std::string> &keys);
            void getStreams(const std::vector<std::string> &keys);

            // Forcibly request to update a certain stream property
            uint64_t getProperty(const std::string &key, TickStreamPropertyId id);
            void getPropertySynchronous(const std::string &key, TickStreamPropertyId id);

            SessionHandler(TickDbImpl &db);
            ~SessionHandler();
            void dbgBreakConnection();

            // Verify if the master connection still active
            bool verifyConnection();

        protected:
            enum Result {
                NO_DATA,
                OK,
                FINISHED
            };


            static void threadProcStatic(SessionHandler &);
            void threadProcImpl();
            void threadProcExceptionWrapper();
            void threadProc();

            SessionHandler::Result processInput();
            void flushOutput();
            void closeConnection();
            void cleanup();
            void close();

            void stopped(); // Called when thread is stopped and all cursors and loaders are disconnected

            // false, if the request did not complete yet
            bool propertyRequestCompleted(uint64_t requestId);
            
            // false, if the request did not complete yet
            bool streamDefRequestCompleted(uint64_t requestId);

            // true if running and ready to execute new commands (not shutting down or interrupted)
            bool isWorking();

            // These may be called from a different thread via public methods
            uint64_t requestGetStream(const std::string &key);
            uint64_t requestGetStreams();
            uint64_t requestGetStreams(const std::vector<std::string> &keys);
            uint64_t requestGetProperty(const std::string &key, TickStreamPropertyId id);
            // Request data for a stream that does not exist (basically, a ping)
            uint64_t requestGetDummyStream();

            void requestClose();

            TickStreamCache& cache();

            template<typename T> T read();
            TickStreamPropertyId readPropertyId();
            bool readUTF8(std::string &s);


            Result receive_STREAM_PROPERTY_CHANGED();

            Result receive_STREAM_DELETED();

            Result receive_STREAM_CREATED();

            Result receive_STREAM_RENAMED();

            Result receive_STREAMS_DEFINITION();

            Result receive_STREAM_PROPERTY();

            Result receive_SESSION_CLOSED();

            Result receive_STREAMS_CHANGED();

        protected:
            TickDbImpl &db_;
            dx_thread::critical_section writerSection_;
            dx_thread::critical_section threadObjSection_; // After thread's creation, its pointer and state are modified within this section
            std::thread * thread_;

            volatile bool isRunning_, isStopped_, shouldStop_, interrupted_;
            volatile bool getStreamsReady_;

            // Those two are only accessed under lock, therefore already atomic
            uint64_t nStreamDefRequestsSent_;
            uint64_t nPropertyRequestsSent_;

            // There are only updated from a single place, but can be read/waited from a different thread
            volatile uint64_t nStreamDefRequestsReceived_;
            volatile uint64_t nPropertyRequestsReceived_;

            concurrent_obj_set<std::string> neededStreamDefs;
            concurrent_obj_set<std::string> knownStreamDefs;

            DataWriterInternal writer_;
            byte * bufferAllocPtr_;

            IOStream * ioStream_;
            std::string errorText_;
        };
    }
};