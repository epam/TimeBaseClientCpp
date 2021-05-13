#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "tickdb/http/tickdb_http.h"
#include "tickdb/symbol_translator.h"
#include "tickdb/data_writer_impl.h"

// #include <atomic>

/*
The current implementation implies chunked http encoding and non-realtime mode
messages are packed into chunks which are send only if there is data for at least one full chunk,
like in the original implementation
*/

namespace DxApiImpl {
    class LoaderManager;

    namespace TickLoaderNs {
        struct InterruptionEnum {
            enum Enum {
                WORKING = 0,
                STOPPING,
                CLIENT_FINISHED,
                SERVER_FINISHED_OK,
                ERROR,
                SERVER_FINISHED_ERROR,
                SERVER_DISCONNECTED,
                IO_ERROR,
                _COUNT_,
                UNKNOWN = 0xFF
            };
        };

        struct ErrorEnum {
            enum Enum {
                NO_ERROR = 0,
                ERROR,
                HANDSHAKE_ERROR,
                DISCONNECTION_ERROR, // A subset of IO error
                IO_ERROR,
                _COUNT_,
                UNKNOWN = 0xFF
            };
        };

        ENUM_CLASS(uint8_t, Interruption);
        ENUM_CLASS(uint8_t, Error);
    };

    class TickLoaderImpl : public DxApi::TickLoader {
        friend class LoaderManager;
        friend class DxApi::TickCursor;
        typedef DxApi::TimestampMs TimestampMs;
        typedef DxApi::DataWriter DataWriter;
        typedef DxApi::LoadingOptions LoadingOptions;
        typedef DxApi::InstrumentType InstrumentType;

        // Two magic numbers..
        enum WriterFinishedSignal {
            WRITER_FINISHED_UNSET=0x9CF4B817,
            WRITER_FINISHED_SET=0xDEADBEEF,
        };

    public:
        bool isClosed() const;
        bool isWriterFinished() const;
        const DxApi::TickStream * stream() const;

        TickLoaderImpl * open(const DxApi::TickStream * stream, const DxApi::LoadingOptions &opt);

        // Finish writing data, close stream
        void finishInternal(bool calledFromMainThread);
        void finish();

        // Deallocate resources. Stop writing if needed.
        void close();

        //DataWriter& next();

        // Start writing next message (and fill header fields)
        DataWriter& next(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp = DxApi::TIMESTAMP_NULL);

        // Start writing next message (and fill header fields)
        DataWriter& next(unsigned messageTypeId, const std::string &symbolName, TimestampMs timestamp = DxApi::TIMESTAMP_NULL);

        // Start writing next message (and fill header fields)
        DataWriter& next(const std::string &typeName, const std::string &symbolName, TimestampMs timestamp);

        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, unsigned entityId, TimestampMs timestamp = DxApi::TIMESTAMP_NULL);

        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, const std::string &symbolName, TimestampMs timestamp = DxApi::TIMESTAMP_NULL);


        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, unsigned entityId,
            TimestampMs timestamp, bool commitPrevious);

        // Start writing next message (and fill header fields). The previous message (if any) is cancelled
        DataWriter& beginMessage(unsigned messageTypeId, const std::string &symbolName, 
            TimestampMs timestamp, bool commitPrevious);
       
        void send();

        // Send the current message
        void commitMessage();

        void flush();

        //void send(const std::string &typeName, const std::string &symbolName, InstrumentType instrumentType, Timestamp timestamp);

        void    registerMessageType(unsigned localTypeId, const std::string &typeName);

        // Returns allocated entityID
        /*unsigned    getInstrumentId(const std::string &symbolName, InstrumentType instrumentType);*/

        void preload(uintptr nBytesToPreload, uintptr writeBlockSize = 0x4000);

        TickLoaderImpl(TickDbImpl &dbRef, uint64_t uid);
        // Automatic desctructor would also work in out case, but..
        ~TickLoaderImpl();

        // Misc
        const char * textId() const { return id_.textId; }
        uint64_t id() const { return id_.id; }

        void addListener(ErrorListener * listener);
        void addListener(SubscriptionListener * listener);

        void removeListener(ErrorListener * listener);
        void removeListener(SubscriptionListener * listener);

        size_t nErrorListeners();
        size_t nSubscriptionListeners();

        uint64_t nBytesWritten() const;
        uint64_t nMsgWritten() const;
        uint64_t nMsgCancelled() const;
        DataWriter& getWriter();

        unsigned getInstrumentId(const std::string &instrument);

        void dbgBreakConnection(bool input = true, bool output = true);
        void setShouldLogMessages(bool shouldLog);
        void setWriterFinished();

    protected:
        enum State {
            NOT_INITIALIZED = 0,
            FINISHED_WRITING,
            WRITING,
            NOT_IN_MESSAGE_BLOCK = WRITING,
            MESSAGE_START,  // We only open message block if we have a message to write
            MESSAGE_BODY,
        };

        INLINE bool isWritingMessageBody() const    { return MESSAGE_BODY == state_; }
        INLINE bool isWritingMessage() const        { return state_ >= MESSAGE_START; }
        INLINE bool isInMessageBlock() const        { return state_ >= MESSAGE_START; }
        INLINE bool isWriting() const               { return state_ >= WRITING; }

        //void writeChunk(uintptr size);

        bool getInterruptedException(DxApi::TickLoaderInterruptedException &e);

    protected:
        struct ErrorInfo {
            volatile TickLoaderNs::Error::Enum code;
            std::string className;
            std::string msg;
            ErrorInfo() : code(TickLoaderNs::Error::Enum::NO_ERROR) {}
        };
        
        unsigned getMessageDescriptorId(const std::string &messageTypeName);
        unsigned getMessageDescriptorId(unsigned localTypeId);

        void onInterruption();
        // Check if the main writer is interrupted
        void checkWriterInterruption();
        

        // Send the output buffer
        void transmitBuffer();

        void resetPointers();

        // Flush non-message(protocol) data. (Handshake/initialization data and the terminator marker)
        void flushOutput();

        // Flush message data specifically
        void flushOutputMsg();

        template<bool ISMSGDATA> void flushOutputInternal();

        void endMessage();
        void flushOutputMessageSynchronized();
        void writeMessageBlockEnd();
        void writeMessageBlockStart();
        void endMessageIfNeeded();
        void startMessageBlockIfNeeded();
        void endMessageBlockIfNeeded();
        void writeInstrumentBlock(const std::string &symbol);
        void endWriting();

        // Cancel message currently being written
        void cancelMessage();

        // Internal implementation of next does not contain validity checks. Starts new message, optionally cancelling previous
        template<bool SHOULD_RESET> DataWriter& nextInternal(unsigned messageTypeID, unsigned entityID, TimestampMs timestamp);

        // Reading data from input channel
        template<typename T> T read();
        std::string readUTF8();
        bool readUTF8(std::string &s);
        void readErrorResponse(ErrorInfo &error);

        void readErrorResponse();
        void readEntitiesSubscriptionChange();
        void readTypesSubscriptionChange();



        void dispatchError(int code, const std::string &s);

        void dispatchTypesSubscriptionChange(bool all, bool added, const std::vector<std::string> &types);
        void dispatchEntitiesSubscriptionChange(bool all, bool added, const std::vector<std::string> &entities);
        // Read notifications and responses from server
        // Should be only called from external manager thread
        bool processResponse(); 

        // Close output channel
        void closeOutput();

        // Close and invalidate I/O connection
        void closeConnection();

        // Close connection. Put into stopped, unusable state. Deregister from LoaderManager
        void cleanup(bool deregister = true);

        //
        std::string serverErrorText();

        bool dbgProcessingServerResponse_;

    protected:
        TickDbImpl &db_;
        LoaderManager &manager_;

        volatile uint8_t state_;
        volatile bool clientIsActive_;

#if LOADER_FLUSH_TIMER_ENABLED
        // TODO: This is not a working implementation yet
        std::atomic_flag flushable_;
        std::atomic_flag willBeFlushedByManager_;
        std::atomic_flag beingFlushed_;
#endif

        bool writingMessage_;
        bool shouldLogMessages_;
        bool shouldLogDataBlocks_;

        DataWriterInternal messageWriter_;
        byte * messageStart_;
        byte * writePtr_;
        
        IOStream * ioStream_;
        OutputStream * preloaderOutputStream_;

        uint64_t nMsgWritten_;
        uint64_t nMsgCancelled_;

        uintptr_t minChunkSize_; // Data is not written until this amount is stored in the buffer. We don't guarantee that some chunks won't go below this size
        uintptr_t maxChunkSize_; // Chunks are never written bigger than this size

        const DxApi::TickStream * stream_;

        IdentityInfo id_;

        SymbolTranslator tables_;

        // last names read from instrument identity and type blocks
        std::string lastSymbolName, lastMessageTypeName;
        uint8_t lastSymbolType_;
        uintptr_t lastRemoteMessageTypeId_;

        // set if the loader is interrupted by the server or unrecoverable error, will not read/write any more data
        // can only be set once
        
        //volatile uint8_t interruption_;
        // Signal for interrupting the writer thread
        volatile TickLoaderNs::Interruption::Enum interruption_;

        volatile uint32_t writerFinishedSignal_;

        // true if the loader stopped with error    hasError => interrupted
        // can only be set once

        // NOTE: will later decide what to do about 'volatile' modifier applied to class
        ErrorInfo error_;

        dx_thread::srw_lock errorListenersLock_;
        dx_thread::srw_lock subscriptionListenersLock_;

        std::unordered_set<ErrorListener *> errorListeners_;
        std::unordered_set<SubscriptionListener *> subscriptionListeners_;
    };

    DEFINE_IMPL_CLASS(DxApi::TickLoader, TickLoaderImpl);

    INLINE bool TickLoaderImpl::isClosed() const { return state_ < WRITING; }
    INLINE bool TickLoaderImpl::isWriterFinished() const { return WRITER_FINISHED_SET == writerFinishedSignal_; }
    INLINE const DxApi::TickStream * TickLoaderImpl::stream() const { return stream_; }
}