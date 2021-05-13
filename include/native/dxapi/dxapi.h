#pragma once

/*
 * Native C++ API prototype
 */

#include "dxcommon.h"
#include "dxconstants.h"
#include "data_reader.h"
#include "data_writer.h"

#include <string>
#include <vector>
#include <memory>


namespace DxApi {
    class TickDb;
    class SelectionOptions;
    class TickStream;
    struct MessageHeader;
    typedef int64_t TimestampMs;
    typedef int64_t TimestampNs;
}

// TODO: selection options should be wrapped in interface or split into 2 classes, public and private
#include "selection_options.h"
#include "loading_options.h"
#include "interval.h"
#include "stream_options.h"
#include "bg_proc_info.h"

namespace DxApi {
    struct InstrumentTypeEnum {
        enum Enum {
            EQUITY = 0,
            OPTION,
            FUTURE,
            BOND,
            FX,
            INDEX,                  // = 5
            ETF,
            CUSTOM,
            SIMPLE_OPTION,
            EXCHANGE,
            TRADING_SESSION,        // = 10
            STREAM,
            DATA_CONNECTOR,
            EXCHANGE_TRADED_SYNTHETIC,
            SYSTEM,
            CFD,                    // Contract-For-Difference-Synthetic = 15
            _COUNT_,
			UNKNOWN = 0xFF          // unknown/invalid
        };
    };

    ENUM_CLASS(uint8_t, InstrumentType);

    /**
     * true if instrument of this type can accumulate positions and appear in trade orders
     * Currently supported as a function, not a method. Use: isTradable(instrumentType)
     */
    INLINE bool isTradable(const InstrumentType &t)
    {
        auto ordinal = t.toInt();
        return ordinal <= (unsigned)InstrumentType::SIMPLE_OPTION || InstrumentType::EXCHANGE_TRADED_SYNTHETIC == ordinal;
    }


    struct InstrumentIdentity {
        std::string symbol;
        InstrumentType type;

        INLINE bool operator==(const InstrumentIdentity &other) const
        {
            return type == other.type && symbol == other.symbol; 
        }


        INLINE std::string toString() const
        {
            std::string s;
            s.append(this->type.toString()).append(":").append(this->symbol);
            return s;
        }


        InstrumentIdentity(const InstrumentIdentity &other)
            : symbol(other.symbol), type(other.type) { }

        InstrumentIdentity(InstrumentType type, const std::string &symbol)
            : symbol(symbol), type(type) { }

        InstrumentIdentity(InstrumentType type, const char * symbol)
            : symbol(symbol), type(type) { }
        
    protected:
        InstrumentIdentity() 
            : type(InstrumentType::UNKNOWN) { }
    };


    class TickCursorException : public std::runtime_error {
    public:
        TickCursorException(const std::string &s) : std::runtime_error(s) {}
        TickCursorException(const char * s) : std::runtime_error(s) {}
    };


    class TickCursorServerError : public TickCursorException {
    public:
        int code;
        TickCursorServerError(int c, const std::string &s) : TickCursorException(s), code(c) {}
        TickCursorServerError(int c, const char * s) : TickCursorException(s), code(c) {}

    };


    // Local interruption from close() etc.
    class TickCursorInterruptedException : public TickCursorException {
    public:
        TickCursorInterruptedException(const std::string &s) : TickCursorException(s) {}
        TickCursorInterruptedException(const char * s) : TickCursorException(s) {}

    };


    class TickCursorError : public TickCursorException {
    public:
        TickCursorError(const std::string &s) : TickCursorException(s) {}
        TickCursorError(const char * s) : TickCursorException(s) {}

    };


    class TickCursorClosedException : public TickCursorException {
    public:
        TickCursorClosedException() : TickCursorException("Tickloader is closed") {}
        TickCursorClosedException(const std::string &s) : TickCursorException(s) {}
        TickCursorClosedException(const char * s) : TickCursorException(s) {}
    };
    

    class TickCursor {
    public:
        bool isAtEnd() const;
        bool isClosed() const;

        _DXAPI bool      next(MessageHeader * const msg);
        bool            nextIfAvailable(MessageHeader* const msg);
        void            close();
        size_t          preload(size_t nBytesToPreload, size_t readBlockSize = 0x2000);

        TickDb&         getDB() const;
        DataReader&     getReader() const;

        const std::string* getInstrument(unsigned entityId);

        // retrieves name for remote type. Will only return NULL if the server error occured or the message is not server-originated 
        const std::string* getMessageTypeName(unsigned msgDescId);
        //const std::string * getMessageTypeName(unsigned msgDescId, unsigned streamId);

        const std::string* getMessageStreamKey(unsigned msgDescId);
        const std::string* getMessageSchema(unsigned msgDescId);
        
        // Registers user's message type. This is deprecated and will be removed soon
        unsigned    registerMessageType(unsigned newTypeId, const std::string &newTypeName);
        unsigned    registerMessageType(unsigned newTypeId, unsigned streamId, const std::string &newTypeName);


        // Subscription methods

        //  const std::vector<std::string> &types, const std::vector<std::string> &entities
        
        // [+]
        // Made unaccessible from .NET API (2016-02-03), deprecated?
        void reset(TimestampMs dt, const std::vector<std::string> &entities);
        
        /////
        

        // [+]
        void reset(TimestampMs dt);
        
        // [+]
        // Add entities to the subscription
        // Does nothing if the entities array in empty
        void addEntities(const std::vector<std::string> &entities);

        // [+]
        // Add entities to the subscription
        // NULL can be used to denote <ALL>, otherwise the request is ignored if n == 0
        void addEntities(std::string entities[], size_t n);
        
        // [+]
        // Add a single entity to the subscription
        void addEntity(const std::string &entity);
        
        // [+]
        // Remove entities from the subscription
        // Does nothing if the entities array in empty
        void removeEntities(const std::vector<std::string> &entities);

        // [+]
        // NULL can be used to denote <ALL>, otherwise the request is ignored if n == 0
        void removeEntities(std::string entities[], size_t n);
        
        // [+]
        // Remove a single entity from the subscription
        void removeEntity(const std::string &entity);

        // [+]
        // Remove all entities from the subscription
        void clearAllEntities();
        
        // [+]
        // Add <ALL> entities to the subscription
        void subscribeToAllEntities();
        
        // [+]
        // Add message types to the subscription
        // If the types array is empty, does nothing
        void addTypes(const std::vector<std::string> &types);
        
        // [+]
        // Add message types to the subscription
        // NULL can be used to denote <ALL>, otherwise the request is ignored if n == 0
        void addTypes(const char * types[], size_t n);
        
        // [+]
        // Remove types from the subscription
        // If the types array is empty, does nothing
        void removeTypes(const std::vector<std::string> &types);

        // [+]
        // Remove message types from the subscription
        // NULL can be used to denote <ALL>, otherwise the request is ignored if n == 0
        void removeTypes(const char * types[], size_t n);
        
        // [+]
        // Set types
        // If the types array is empty, will unsubscribe from all messages (except realtimestart?)
        void setTypes(const std::vector<std::string> &types);

        // [+]
        // Set types
        // If types is NULL, should subscribe to all types, otherwise if n == 0 will unsubscribe from all types
        void setTypes(const char * types[], size_t n);
        
        // [+]
        // Subscribe to all message types
        void subscribeToAllTypes();

        // [+]
        // Add entities and types to the subscription. A shortcut for addEntities() , addTypes()
        // Can't be used to subscribe to all entities or types
        void add(const std::vector<std::string> &entities, const std::vector<std::string> &types);
        
        // [+]
        // Remove entities and types from the subscription. A shortcut for removeEntities() , removeTypes()
        void remove(const std::vector<std::string> &entities, const std::vector<std::string> &types);

        // [+]
        // Add streams to the subscription
        // If the streams array is empty, does nothing
        void addStreams(const std::vector<TickStream *> &streams);

        // [+-]
        // Remove streams from the subscription
        // If the streams array is empty, does nothing
        void removeStreams(const std::vector<TickStream *> &streams);

        // [+-]
        // Remove all streams from the subscription. RealtimeStartMesage will still come through
        void removeAllStreams();

        // [+]
        // Submitted subscription changes will occur at the speciefied time
        void setTimeForNewSubscriptions(TimestampMs dt);


        // Statistics ans misc

        // Number of bytes received from the input stream
        uint64_t    nBytesReceived() const;

        // Number of bytes read(decoded). Does not include bytes still left in the input buffer
        uint64_t    nBytesRead() const;

        // Number of message bytes read (including 11 byte header and 4-byte length field)
        uint64_t    nMessageBytesRead() const;

        // Number of messages read (number of succesful next calls)
        uint64_t    nMessagesRead() const;

        // Number of messages read (number of succesful next calls)
        uint64_t    nMessagesSkipped() const;
        
        // Number of messages read+skipped
        uint64_t    nMessagesReceived() const;

        //static bool isValidLocalTypeId(unsigned x);
        //static bool isValidRemoteTypeId(unsigned x);

        //bool isRegisteredRemoteTypeId(size_t typeId) const;
        //bool isRegisteredLocalTypeId(size_t typeId) const;
        //bool isRegisteredRemoteEntityId(size_t entityId) const;
        ~TickCursor();
        static void operator delete(void* ptr, size_t sz);
    protected:
        TickCursor();
        DISALLOW_COPY_AND_ASSIGN(TickCursor);
    }; // class TickCursor



    class TickLoaderException : public std::runtime_error {
    public:
        TickLoaderException(const std::string &s) : std::runtime_error(s) {}
        TickLoaderException(const char * s) : std::runtime_error(s) {}
    };

    class TickLoaderInterruptedException : public TickLoaderException {
    public:
        TickLoaderInterruptedException(const std::string &s) : TickLoaderException(s) {}
        TickLoaderInterruptedException(const char * s) : TickLoaderException(s) {}
        TickLoaderInterruptedException() : TickLoaderException("") {}
    };

    class TickLoaderClosedException : public TickLoaderException {
    public:
        TickLoaderClosedException() : TickLoaderException("Tickloader is interrupted/closed") {}
        TickLoaderClosedException(const std::string &s) : TickLoaderException(s) {}
        TickLoaderClosedException(const char * s) : TickLoaderException(s) {}
    };


    class TickLoader {
    public:
        class ErrorListener {
        public:
            virtual void onError(const std::string &errorClass, const std::string &errorMsg) = 0;
            virtual ~ErrorListener() = default;
        };

        class SubscriptionListener {
        public:
            virtual void typesAdded(const std::vector<std::string> &types) = 0;
            virtual void typesRemoved(const std::vector<std::string> &types) = 0;
            virtual void allTypesAdded() = 0;
            virtual void allTypesRemoved() = 0;

            virtual void entitiesAdded(const std::vector<std::string> &entities) = 0;
            virtual void entitiesRemoved(const std::vector<std::string> &entities) = 0;
            virtual void allEnititesAdded() = 0;
            virtual void allEnititesRemoved() = 0;

        protected:
            ~SubscriptionListener() = default;
        };

        //MessageHeader make(const std::string &typeName, const std::string &symbolName);
        //MessageLoader & next(const MessageHeader &header);

        bool isClosed() const;

        DataWriter& getWriter();

        // Start writing next message (finalize previous one automatically, if necessary) - REMOVED
        //DataWriter& next();

        // Start writing next message (and fill header fields)
        DataWriter& next(const std::string &typeName, const std::string &symbolName, TimestampMs timestamp = TIMESTAMP_NULL);

        // Start writing next message (and fill header fields)
        DataWriter& next(unsigned messageTypeId, const std::string &symbolName, TimestampMs timestamp = TIMESTAMP_NULL);

        // Start writing next message (and fill header fields)
        DataWriter& next(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp = TIMESTAMP_NULL);

        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp = DxApi::TIMESTAMP_NULL);

        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, const std::string &symbolName, TimestampMs timestamp = TIMESTAMP_NULL);


        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, unsigned entityId,
            TimestampMs timestamp, bool commitPrevious);

        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, const std::string &symbolName, 
            TimestampMs timestamp, bool commitPrevious);

        // Send the current message
        void send();
        void commitMessage();

        void flush();

        // Send the current message (and fill header fields before sending) - REMOVED
        //void send(const std::string &typeName, const std::string &symbolName, Timestamp timestamp = DxApi::TIMESTAMP_NULL);

        void finish();

        void close();

        // register message type Id you can use later with this instance of TickLoader.
        // Not mandatory in this version, if you know message descriptor order in advance. Id is small uint, can start from 0
        void    registerMessageType(unsigned newTypeId, const std::string &newTypeName);

        // retrieve an ID you can later use instead of instrument name when calling functions for this instance of TickLoader. Value of 0 is reserved
        //unsigned    getInstrumentId(const std::string &symbolName);

        unsigned    getInstrumentId(const std::string &instrument);

        void addListener(ErrorListener * listener);
        void addListener(SubscriptionListener *listener);

        void removeListener(ErrorListener *listener);
        void removeListener(SubscriptionListener *listener);

        size_t nErrorListeners();
        size_t nSubscriptionListeners();

        uint64_t    nBytesWritten() const;
        uint64_t    nMsgWritten() const;
        uint64_t    nMsgCancelled() const;
        void preload(size_t nBytesToPreload, size_t writeBlockSize = 0x4000);

        const TickStream * stream() const;

        ~TickLoader();

        void operator delete(void* ptr, size_t sz);
    protected:
        TickLoader();
        DISALLOW_COPY_AND_ASSIGN(TickLoader);
    }; // Class TickLoader


    // This definition must match the definition for managed interface
#pragma pack(push, 1)
    struct MessageHeader {
        int64_t  timestamp;
        uint32_t length;
        uint32_t entityId;
        uint32_t typeId;
        int32_t  streamId;
        uint32_t typeIdLocal;       // Deprecated.
        uint8_t  cursorState = 0;  // Contains isAtEnd and isAtBeginning flags etc.
        uint8_t  z0, z1, z2;        // Padding
        //int64_t pt;
        /*struct {
            uint64_t nanos;
            uint64_t ticks;
        } pt;*/

        /*union {
            void * parent;
            uint64_t __dummy;
        };*/
    };
#pragma pack(pop)


    class InstrumentMessage : public MessageHeader {
    public:
        TickCursor * cursor;
        void copyHeaderFrom(const InstrumentMessage &m) { *this = m; }
        const std::string * getTypeName() const;
    };


    class MarketMessage : public InstrumentMessage {
    public:
        int64_t originalTimestamp;
        int64_t sequenceNumber;
        int16_t currencyCode;
        bool has_originalTimestamp, has_currencyCode, has_sequenceNumber;
        std::string toString() const;
    };

    typedef std::vector<TickStream *> TickStreamVector;


    struct QueryParameter {
        std::string name;                   // Name of the parameter
        std::string type;                   // Base type as text
        Nullable<std::string> value;        // nullable string value

        QueryParameter(const std::string &name_, const std::string &type_, const std::string value_)
            : name(name_), type(type_), value(value_) {}

        QueryParameter(const std::string &name_, const std::string &type_, const Nullable<std::string> &value_)
            : name(name_), type(type_), value(value_) {}

        QueryParameter(const std::string &name_, const std::string &type_)
            : name(name_), type(type_) {}
    };


    class TickDbException : public std::runtime_error {
    public:
        TickDbException(const std::string &s) : std::runtime_error(s) {}
        TickDbException(const char * s) : std::runtime_error(s) {}
    };


    class TickDb {
    public:
        _DXAPI static TickDb * createFromUrl(const char * url);
        _DXAPI static TickDb * createFromUrl(const char * url, const char * username, const char * password);

        virtual bool isReadOnly() const = 0;
        virtual bool isOpen() const = 0;

        virtual bool open(bool readOnlyMode) = 0;
        virtual void close() = 0;
        virtual bool format() = 0;

        virtual std::vector<TickStream *> listStreams() = 0;
        virtual TickStream * getStream(const std::string &key) = 0;
        virtual TickStream * createStream(const std::string &key, const std::string &name, const std::string &description, int distributionFactor) = 0;
        virtual TickStream * createStream(const std::string &key, const StreamOptions &options) = 0;
        virtual TickStream * createFileStream(const std::string &key, const std::string &dataFile) = 0;

        virtual TickCursor * select(TimestampMs time, const std::vector<const TickStream *> *streams, const SelectionOptions &options,
            const std::vector<std::string> *types, const std::vector<std::string> *entities) = 0;

        virtual TickCursor * select(TimestampMs time, const std::vector<const TickStream *> &streams, const SelectionOptions &options,
            const std::vector<std::string> *types, const std::vector<std::string> *entities) = 0;

        virtual TickCursor * select(TimestampMs time, const std::vector<TickStream *> *streams, const SelectionOptions &options,
            const std::vector<std::string> *types, const std::vector<std::string> *entities) = 0;

        virtual TickCursor * select(TimestampMs time, const std::vector<TickStream *> &streams, const SelectionOptions &options,
            const std::vector<std::string> *types, const std::vector<std::string> *entities) = 0;

        virtual TickCursor* executeQuery(const std::string &qql, const SelectionOptions &options, TimestampMs time,
            const std::vector<std::string> *instruments, const std::vector<QueryParameter> &params) = 0;

        virtual TickCursor * executeQuery(const std::string &qql, const SelectionOptions &options, TimestampMs time, const std::vector<QueryParameter> &params) = 0;

        virtual TickCursor * executeQuery(const std::string &qql, const SelectionOptions &options, const std::vector<QueryParameter> &params) = 0;

        virtual TickCursor * executeQuery(const std::string &qql, const std::vector<QueryParameter> &params) = 0;


        virtual TickCursor * createCursor(const TickStream *stream, const SelectionOptions &options) = 0;
        virtual TickLoader * createLoader(const TickStream *stream, const LoadingOptions &options) = 0;

        virtual ~TickDb() = 0;

        // Not supposed to be called by the end user of the API
        // This should only be called before open()
        virtual void setStreamNamespacePrefix(const std::string &prefix) = 0;
        virtual void setStreamNamespacePrefix(const char *prefix) = 0;

    protected:
        TickDb();
        DISALLOW_COPY_AND_ASSIGN(TickDb);
    }; // class TickDb


    // Timebase Tick Stream view class
    // Should not be used after deletion of the said database object
    // TODO: Verify with signature that is set to magic value only if the object is actually alive? So we always crash or print error when referencing dead object
    
    struct LockTypeEnum {
        enum Enum {
            NO_LOCK,
            READ_LOCK,
            WRITE_LOCK
        };
    };

    ENUM_CLASS(uint8_t, LockType);

    class TickStream {
    public:
        const std::string& key() const;
        int32_t distributionFactor() const;
        const Nullable<std::string>& name() const;
        const Nullable<std::string>& description() const;
        const Nullable<std::string>& owner() const;
        const Nullable<std::string>& location() const;

        const Nullable<std::string>& distributionRuleName() const;

        StreamScope  scope() const;

        bool duplicatesAllowed() const;
        bool highAvailability() const;
        bool unique() const;
        bool polymorphic() const;
        // TODO: Change representation
        const std::string& periodicity() const;
        const Nullable<std::string>& metadata() const;
        const StreamOptions&  options() const;
        bool setSchema(const DxApi::StreamOptions & options);

        TickCursor* select(TimestampMs millisecondTime, const SelectionOptions &options, const std::vector<std::string> * types, const std::vector<std::string> * entities) const;
        TickCursor* createCursor(const SelectionOptions &options) const;
        TickLoader* createLoader(const LoadingOptions &options) const;
        std::vector<std::string> listEntities() const;

        bool getTimeRange(TimestampMs range[], const std::vector<std::string> * const entities = NULL) const;
        bool getTimeRange(TimestampMs range[], const std::vector<std::string> &entities) const;

        bool truncate(TimestampMs millisecondTime, const std::vector<std::string> * const entities = NULL) const;
        bool truncate(TimestampMs millisecondTime, const std::vector<std::string> &entities) const;

        bool clear(const std::vector<std::string> * const entities = NULL) const;
        bool clear(const std::vector<std::string> &entities) const;

        bool purge(TimestampMs millisecondTime) const;
        bool deleteStream();
        bool abortBackgroundProcess() const;

        bool getPeriodicity(Interval * interval) const;

        static void operator delete(void* ptr, size_t sz);
        ~TickStream();

    protected:
        TickStream();
        DISALLOW_COPY_AND_ASSIGN(TickStream);
    }; // class TickStream
} // namespace DxApi

namespace std {
template<> struct hash<DxApi::InstrumentIdentity> {
public:
    size_t operator()(const DxApi::InstrumentIdentity &x) const
    {
        size_t h = hash<string>()(x.symbol) ^ hash<uint8_t>()(x.type.toInt());
        return  h;
    }
};
}