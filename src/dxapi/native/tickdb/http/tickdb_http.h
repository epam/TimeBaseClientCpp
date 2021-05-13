#pragma once

#include "tickdb/common.h"
#include "util/critical_section.h"
#include <dxapi/dxapi.h>
// TODO: cleanup
#include <dxapi/schema_change_task.h> 
#include "xml/tinyxml2/tinyxml2.h"
#include <set>

#include "tickdb/tickdb_base.h"
#include "tickdb/data_reader_impl.h"
#include "tickdb/tickstream_cache.h"
#include "tickdb/loader_manager.h"
#include "tickdb/session_handler.h"
#include "tickdb/qqlstate.h"
#include "uri.h"



namespace DxApiImpl {
    class IOStream;
    class SelectionOptionsImpl;
    class LoaderManager;

    typedef ::Uri Uri; // Prevents namespace collisions later

    class TCP {
    public:
        enum NoDelayMode {
            NORMAL = 0,
            NO_DELAY = 1
        };
    };

    class HTTP {
    public:
        enum class ContentType {
            NONE = 0,
            XML = 1,
            BINARY = 2
        };

        enum class ContentLength {
            NONE = 0,
            FIXED = 1,
            CHUNKED = 2
        };

        // Structure that contains partially parsed HTTP response string
        class ResponseHeader : public std::string {
        public:
            unsigned code;
            const unsigned responseStartOffset = 9;
            const unsigned responseStringStartOffset = 13;
            unsigned responseStringEndOffset;
            std::string responseText() const;

            // Returns valid HTTP response code or throws exception
            unsigned parse();
            intptr_t parseContentLength();
            void limitLengthTo(unsigned limit);
            ResponseHeader();

        protected:
            // Throws exception, instead of actually returning a value
            unsigned parseException(const char *fmt);
        };
        
        // Read response header. Returns valid HTTP response code, or parsing exception, or IOStreamDisconnectException
        // responseHeader guaranteed to contain CRLFCRLF at the end
        static unsigned readResponseHeader(ResponseHeader& responseHeader, DataReaderInternal& streamReader);

        // Throws exception if verification fails
        static unsigned parseResponseHeader(ResponseHeader& responseHeader);

        // returns contentLength, or -1 for chunked encoding
        // returns 0 if not able to find information about content length at all
        static intptr_t parseContentLength(const std::string& responseHeader);

        // contentLength < 0 means chunked encoding
        static bool readResponseBody(std::string& responseBody, DataReaderInternal& reader, intptr contentLength);

        static bool tryExtractTomcatResponseMessage(std::string& responseBody);
    };

    class XmlRequest;
    class TickLoaderImpl;
    class TickCursorImpl;
    class TickStreamImpl;


    struct IdentityInfo {
        char textId[0x100];
        uint64_t id;
        IdentityInfo(uint64_t n) : id(n) { textId[0] = '\0'; }
    };


    // Will live here for a while
    struct SubscrChangeActionEnum {
        enum Enum {
            SET, ADD, REMOVE
        };
    };

    ENUM_CLASS(uint8_t, SubscrChangeAction);

    std::string subscriptionToString(const std::string s[], size_t n);
    std::string subscriptionToString(const DxApi::TickStream * const s[], size_t n);
    //std::string subscriptionToString(const DxApi::InstrumentIdentity s[], size_t n);

    template<typename T> std::string subscriptionToString(const std::vector<T> * x)
    {
        if (NULL == x) {
            return subscriptionToString((T *)NULL, (size_t)0);
        }

        size_t l = x->size();
        return 0 == l ? subscriptionToString((const T *)0x64, l) : subscriptionToString(x->data(), l);
    }


    class TickDbImpl : /*TickDbBase,*/ public DxApi::TickDb {
        friend class TDB::SessionHandler;
    public:
        typedef void(DXAPI_CALLBACK * OnStreamDeletedCallback)(TickDbImpl * db, DxApi::TickStream * stream);
        typedef void(DXAPI_CALLBACK * OnDisconnectedCallback)(TickDbImpl * db);

        // TODO: Currently we assume that there's only one owner/listener (Which is sufficient for current use case)
        OnStreamDeletedCallback onStreamDeletedCallback; 
        OnDisconnectedCallback onDisconnectedCallback;

    public:
        virtual bool isReadOnly() const;
        virtual bool isOpen() const;

        virtual bool open(bool readOnlyMode);
        virtual void close();
        virtual std::vector<DxApi::TickStream *> listStreams();
        //virtual TickStream getStream(const std::string &key);
        virtual DxApi::TickStream* getStream(const std::string &key);
        virtual DxApi::TickStream* createStream(const std::string &key, const std::string &name, const std::string &description, int distributionFactor);
        virtual DxApi::TickStream* createStream(const std::string &key, const DxApi::StreamOptions &options);
        virtual DxApi::TickStream* createFileStream(const std::string &key, const std::string &dataFile);
        
        DxApi::TickCursor* select(DxApi::TimestampMs time, SelectionOptionsImpl &options, const std::vector<const DxApi::TickStream *> *streams, const std::vector<std::string> *types,
            const std::vector<std::string> *entities, const std::string *qql = NULL, const std::vector<DxApi::QueryParameter> * const param = NULL);

        DxApi::TickCursor* select(DxApi::TimestampMs time, const DxApi::SelectionOptions &options, const std::vector<const DxApi::TickStream *> *streams, const std::vector<std::string> *types,
            const std::vector<std::string> *entities, const std::string *qql = NULL, const std::vector<DxApi::QueryParameter> * const param = NULL);

        virtual DxApi::TickCursor* select(DxApi::TimestampMs time,
            const std::vector<const DxApi::TickStream *> * streams, const DxApi::SelectionOptions &options,
            const std::vector<std::string> * types, const std::vector<std::string> * entities);

        virtual DxApi::TickCursor* select(DxApi::TimestampMs time,
            const std::vector<const DxApi::TickStream *> &streams, const DxApi::SelectionOptions &options,
            const std::vector<std::string> * types, const std::vector<std::string> * entities);

        virtual DxApi::TickCursor* select(DxApi::TimestampMs time,
            const std::vector<DxApi::TickStream *> * streams, const DxApi::SelectionOptions &options,
            const std::vector<std::string> * types, const std::vector<std::string> * entities);

        virtual DxApi::TickCursor* select(DxApi::TimestampMs time,
            const std::vector<DxApi::TickStream *> &streams, const DxApi::SelectionOptions &options,
            const std::vector<std::string> * types, const std::vector<std::string> * entities);
        

        virtual DxApi::TickCursor* executeQuery(const std::string &qql, const DxApi::SelectionOptions &options, DxApi::TimestampMs time,
            const std::vector<std::string> *instruments, const std::vector<DxApi::QueryParameter> &params);

        virtual DxApi::TickCursor* executeQuery(const std::string &qql, const DxApi::SelectionOptions &options, DxApi::TimestampMs time, const std::vector<DxApi::QueryParameter> &params);

        virtual DxApi::TickCursor* executeQuery(const std::string &qql, const DxApi::SelectionOptions &options, const std::vector<DxApi::QueryParameter> &params);

        virtual DxApi::TickCursor* executeQuery(const std::string &qql, const std::vector<DxApi::QueryParameter> &params);


        virtual DxApi::TickCursor* createCursor(const DxApi::TickStream * stream, const DxApi::SelectionOptions &options);
        virtual DxApi::TickLoader* createLoader(const DxApi::TickStream * stream, const DxApi::LoadingOptions &options);

        // These methods are mostly used for running tests withput creating conflicts
        virtual void setStreamNamespacePrefix(const std::string &prefix);
        virtual void setStreamNamespacePrefix(const char *prefix);

        virtual bool format();

        virtual bool validateQql(QqlState &out, const std::string &qql);

        virtual ~TickDbImpl();

        TickDbImpl();

    public: // Internal
        Uri uri;

        uint64_t id() const { return id_.id; }
        const char* textId() const { return id_.textId; }

        INLINE bool isClosed() const { return isClosed_; }

        LoaderManager &loaderManager() { return loaderManager_; }

        IOStream* sendRequest(  const char *request,                                // GET, PUT, POST,...
                                const char *path,                                   // Resource path
                                const std::string *requestBody              = NULL, // Out, optional
                                HTTP::ContentType contentType               = HTTP::ContentType::XML,
                                HTTP::ContentLength contentLengthEncoding   = HTTP::ContentLength::FIXED,
                                TCP::NoDelayMode noDelayMode                = TCP::NoDelayMode::NORMAL,
                                bool filteredExceptions                     = false) const;

        IOStream* sendTbPostRequest(   const std::string *requestBody,
                                        HTTP::ContentType contentType               = HTTP::ContentType::XML,
                                        HTTP::ContentLength contentLengthEncoding   = HTTP::ContentLength::FIXED,
                                        TCP::NoDelayMode noDelayMode                = TCP::NoDelayMode::NORMAL,
                                        bool filteredExceptions                     = false) const;

        tinyxml2::XMLError xmlRequest(tinyxml2::XMLDocument &doc, const std::string &requestBody);
        tinyxml2::XMLError xmlRequest(tinyxml2::XMLDocument &doc, const std::stringstream &requestBody);

        // Send request and receive response
        // Returns http response code, or 0, if there's an error that precludes reading said response code
        int executeTbPostRequest(std::string& responseBody, const std::string &requestBody, const char *requestName = NULL, bool allowUnopenedDb = false);

        int executeRequest( std::string& responseBody,
                            const char *request,
                            const char *path,
                            const std::string *requestBody  = NULL,
                            const char *requestName         = NULL,
                            bool allowUnopenedDb            = false);

        bool testConnection() const;

        // Disconnect session
        void dbgBreakConnection();


        void disposeStream(DxApi::TickStream *stream);

        TickStreamImpl* allocateStream(const std::string &key, const DxApi::StreamOptions &options);
        TickCursorImpl* allocateCursor();
        TickLoaderImpl* allocateLoader();


        TickStreamImpl* addStream(TickStreamImpl *stream);

        // Internal stream - handling methods. Use special naming convention, match TickStream methods, but with stream_ prefix

        // Send delete request and, if succesful, delete the stream from cache
        bool stream_delete(DxApi::TickStream *stream);

        bool stream_clear(const DxApi::TickStream *stream, const std::vector<std::string> * const entities);

        bool stream_truncate(const DxApi::TickStream *stream, DxApi::TimestampMs millisecondTime,
            const std::vector<std::string> * const entities) const;

        bool stream_purge(const DxApi::TickStream *stream, DxApi::TimestampMs millisecondTime) const;

        bool stream_getBackgroundProcessInfo(const DxApi::TickStream *stream, DxApi::BackgroundProcessInfo *to) const;

        bool stream_abortBackgroundProcess(const DxApi::TickStream *stream) const;

        bool stream_listEntities(const DxApi::TickStream* stream, std::vector <std::string> & out) const;

        bool stream_getPeriodicity(const DxApi::TickStream *stream, DxApi::Interval *interval) const;

        bool stream_setSchema(const DxApi::TickStream *stream, bool isPolymorphic, const DxApi::Nullable<std::string> &schema);

        bool stream_changeSchema(const DxApi::TickStream *stream, const DxApi::SchemaChangeTask &task);

        bool stream_getTimeRange(const DxApi::TickStream *stream, int64_t range[],
            const std::vector<std::string> * const entities) const;


        bool stream_lockStream(const DxApi::TickStream *stream, bool write, int64_t timeoutMs, std::string * const id)  const;

        bool stream_unlockStream(const DxApi::TickStream *stream, bool write, const std::string &id)  const;

        bool setUri(const std::string &uri);
        bool setAuth(const char *username, const char *password);

        INLINE const secure_string& username()      const { return username_; }
        INLINE const secure_string& encodedPass()   const { return encodedPass_; }

        // TODO: Temporarily I allow external code to forcibly update stream cache.
        void updateCache();

        // Create I/O steam connected to db
        IOStream* createConnectionStream(enum TCP::NoDelayMode noDelayMode, bool filteredExceptions) const;

        bool pollLoader();

        // Add synchronous callback for handling loader events.
        void addLoaderCallback();

        // Return true if all cursors and loaders seem to be disconnected from timebase
        bool allCursorsAndLoadersDisconnected();

        // Called by SessionHandler when a stream with the specified key is deleted/renamed
        void onStreamDeleted(const std::string &key);

        // Called by SessionHandler when a session thread is finishing due to unrecoverable error or disconnection
        void onSessionStopping();

        // Called by SessionHandler when a session thread is finished and all cursors/loaders disconnected, just before exiting SH thread
        // IMPORTANT NOTE: The API client is allowed to delete the TickDb class from the onDisconnected() handler.
        void onDisconnected();

        void flushExceptions();

        // deregister cursor from deallocation pool
        void remove(TickCursorImpl *cursor);
        void add(TickCursorImpl *cursor);

    protected:

        /*std::vector<std::string> listStreamsAsStringVector();
        std::set<std::string> listStreamsAsStringSet();*/

       
        //static DxApi::TickStream * getStream(DxApi::TickDb * tickdb, const std::string &key);

        // Create socket_, estabilish transport connection
        SOCKET createConnection(enum TCP::NoDelayMode noDelayMode, bool nothrowOnAddr) const;
        SOCKET createConnection(enum TCP::NoDelayMode noDelayMode) const;
        SOCKET createConnection() const;

        bool onIOStreamDisconnected(IOStream *sender, const char *string);
        static bool onIOStreamDisconnectedCallback(IOStream *sender, TickDbImpl *self, const char *string);

        static bool isValidSocket(SOCKET s) { return INVALID_SOCKET != s; }

        DISALLOW_COPY_AND_ASSIGN(TickDbImpl);

    protected:
        // TODO: make pool of sockets later, this one only exists to check if the connection to the database can be estabilished
        //SOCKET s;
        IdentityInfo id_;
        bool readOnlyMode_;
        bool isOpened_;
        bool isClosed_;
        bool socketsInitialized_;

        volatile bool exceptionCork_;

        dx_thread::atomic_counter<uint64> cursor_id_counter_;
        dx_thread::atomic_counter<uint64> loader_id_counter_;
        dx_thread::atomic_counter<uint64> stream_id_counter_;
        dx_thread::srw_lock thisLock_;
        dx_thread::waitable_atomic_uint nBlockedExceptionThreads_;


        LoaderManager       loaderManager_;
        TDB::SessionHandler sessionHandler_;
        std::string         sessionId_;
        concurrent_ptr_set  cursors_;
        TickStreamCache     streamCache_;

        std::string         streamNamespacePrefix_;

        secure_string       basicAuth_;
        secure_string       username_;
        secure_string       encodedPass_;

    }; // class TickDbImpl

    DEFINE_IMPL_CLASS(DxApi::TickDb, TickDbImpl);
}

