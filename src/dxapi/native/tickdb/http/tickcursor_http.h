#pragma once

#include "tickdb_http.h"
#include "tickdb/data_reader_impl.h"
#include "tickdb/symbol_translator.h"
#include "tickdb/http/xml/selection_options_impl.h"
#include "util/static_array.h"

namespace DxApi {
    class TickDb;
    class TickCursor;
}

namespace DxApiImpl {
    struct CursorStateEnum {
        enum Enum {
            STARTED,
            READING,
            END /*,
            CLOSED*/
        };
    };

    ENUM_CLASS(uint8_t, CursorState);

    // isAtBeginning = START == state;
    // isAtEnd = state >= END;
    class TickCursorImpl : public DxApi::TickCursor {
        friend class DxApi::TickDb;
        friend class DxApi::TickCursor;

    public:
        // EXTERNALLY CALLED SPECIAL METHODS
        // Main methods
        // Receive the next message header. Not locked. Causes synchronization with the main
        bool          next(DxApi::MessageHeader * const msg, bool nonBlocking);

        void          close(bool shouldSendCloseRequest, bool noThrow);

        // send select request from created. Used by TickDb implementation. Not locked as the cursor is not yet shared
        TickCursor&   select(const std::vector<std::string> * types, const std::vector<std::string> * entities);

        uintptr_t     preload(uintptr_t nBytesToPreload, uintptr_t readBlockSize = 0x2000);

        // NON-SYNCHRONIZED READ External interface methods
        uint64_t      nBytesReceived() const;
        uint64_t      nBytesRead() const;
        uint64        nMessageBytesRead() const;

        // SYNCHRONIZED READ External interface methods
        unsigned numStreams(); // Return number of streams, excluding null stream for anonymous messages
        // Retrieve stream. streamIndex = [-1 .. numStreams() ). returns NULL for streamIndex = -1
        const DxApi::TickStream * stream(intptr_t streamIndex);

        unsigned registerUserMessageType(unsigned newTypeId, const std::string &newTypeName);
        const std::string* getInstrument(unsigned entityId);

        // retrieves name for remote type. Will only return NULL if the server error occured or the message is not server-originated 
        const std::string * getMessageTypeName(unsigned msgDescId);
        //const std::string * getMessageTypeName(unsigned remoteTypeId, unsigned streamId);
        const std::string * getMessageSchema(unsigned msgDescId);

        const std::string * getMessageStreamKey(intptr_t streamId);

        // SYNCHRONIZED WRITE External interface methods
        TickCursorImpl& setStreams(const std::vector<const DxApi::TickStream *> * streams);


        // SYNCHRONIZED WRITE External interface methods (Subscriptions)
        void reset(DxApi::TimestampMs dt);
        void modifyEntities(const std::string entities[], size_t numEntities, SubscrChangeAction action);
        void modifyEntities(const std::vector<std::string> &entities, SubscrChangeAction action);
        void modifyTypes(const std::string types[], size_t numTypes, SubscrChangeAction action);
        void modifyTypes(const std::vector<std::string> &types, SubscrChangeAction action);
        void modifyTypes(const char * types[], size_t numTypes, SubscrChangeAction action);
        void modifyStreams(const DxApi::TickStream * const streams[], size_t numStreams, SubscrChangeAction action);

        void setTimeForNewSubscriptions(DxApi::TimestampMs dt);

        // Set cursor reading state directly
        uint8_t setStateLocked(CursorState newState);       //    Take this lock when setting the value
        uint8_t setState(CursorState newState);

        // Lifecycle methods
        TickCursorImpl(TickDbImpl &db_, uint64_t uid);
        TickCursorImpl(TickCursorImpl &other, uint64_t uid);
        TickCursorImpl& operator=(const TickCursorImpl &other);
        ~TickCursorImpl();

        // Misc
        INLINE uint64_t id() const { return id_.id; }
        std::string textId() const;

        void dbgBreakConnection(bool input = true, bool output = true);
        // TODO: Remove on next cleanup
        /*bool onIOStreamDisconnected(const char * string);
        static bool onIOStreamDisconnectedCallback(TickCursorImpl *self, const char * string);*/

    public:
        // Public fields (for internal use)
        volatile /*CursorState*/ uint8_t state;          // Due to how managed C++ works(ignores inline when calling native from managed), its a field, not a getter
        SelectionOptionsImpl options;
        std::string extraTraitsText; // Additional traits encoded to short string to be displayed in log

    protected:
        struct SubscriptionState;
        void addSubscriptionCommand(SubscriptionState &s, int64_t cmdId, const char * requestName);

        uintptr_t numStreamsInternal() const;
        const DxApi::TickStream * streamInternal(intptr_t streamIndex) const;

        SubscriptionState * other(SubscriptionState *) const;
        void resetStreamsArray(SubscriptionState &s);

        void closeConnection();

        void freeResources();

        // Print final statistics
        void finalStats();

        // Logs debug data from select
        void log_select(const std::string &xml, int what, bool isError);

        void statsCheckpoint();
        std::string getStatsUpdate(bool reset);

        // Returns pointer to usable part of the input buffer
        byte*       inputBuffer() const;

        // Return false on allocation failure
        bool        allocateInputBuffer();
        static size_t   inputBufferSize();

        static void modifyFilterSet(
            ArrayLUT<uint8_t, 0, 0x100> &filter,
            const_static_array_view<SymbolTranslator::MessageDescriptor *, 0x100> descriptors,
            size_t index, uint8_t value);

        /*****************************************************************************************
         *  Protocol implementation private methods
         */

        void readMessageType();
        void readEntity();
        void readStreamKey();

        void skipMessageBlock();
        bool readNextMessageInBlock(DxApi::MessageHeader &next);
        bool readNextRecord();
        bool readNextMessage(DxApi::MessageHeader &msg);
        bool readNextMessageNonBlock(DxApi::MessageHeader& msg);
        void skipUntilEOF();
        bool readNext(DxApi::MessageHeader &msg);

            // 'false' means the cursor is stopped, 'true' means that at least 1 record id is processed
       

        // Should only be called once from select
        bool readUntilCursorId();

        void dbg_dumpMessage(const DxApi::MessageHeader &msg, const char * text);


        void synchronizeCheck();
        void synchronizeDo();
        SubscriptionState& getWriteShadowState();   // Should be used within write lock
        SubscriptionState& getReadShadowState();    // Should be used within read lock

        // SYNC WRITE?
        void registerNewInstrument(SubscriptionState &s, unsigned newEntityId, const std::string &instrument);

        // SYNC WRITE?
        void registerNewMessageType(SubscriptionState &s, unsigned newTypeId, const std::string &data);

        void registerNewStream(SubscriptionState &s, unsigned newId, const std::string &streamKey);

        // Returns true if GUID belongs to a system message that will not be filtered by subscription filters
        bool isSystemMessageGuid(const char * guid);
        // Skip a sequence of messages within message block until message block ends, without doing any work
        //void skipMessageBlock();

        

        /*****************************************************************************************
        *  Private methods
        */

        void readRemainderAndClose(bool fromMainThread);
        void doClose();
        void sendCloseRequest();

        /*****************************************************************************************
        *  Private fields
        */

     protected:
        TickDbImpl &db_;                // reference to the parent TickDb object, set once at init

        byte *bufferStart_;            // network input buffer user data start pointer, set once at init
        void *bufferAllocPtr_;         // network input buffer allocation pointer, ste once at init


        InputStream         *activeInputStream_;           // set 1-2 times at connect
        InputStream         *preloaderInputStream_;        // set 1-2 times at connect
        DataReaderBaseImpl  *reader_;                // set once at connect

        uint64_t        nNonMessageBytesRead_;

        // Non-protected state variables

        // State flags
        bool isWithinMessageBlock_;                     // Internal to next()
        bool isIdle_;
        bool readMessageDone_;

        // If this flag is false, reads are possible and the reader field is valid. When the cursor is closed or not yet completely opened, isAtEnd is true
//        bool isAtEnd_;
        volatile bool isInterrupted_;
        volatile bool interruptedTimeout_;
        volatile bool isInNext_;

        // If this flag is true, the cursor was first opened, then closed
        bool isClosed_;

        
        volatile bool isOpen_;                        // True until closed and resources are freed. This only changes in predictable fashion, so it is not put into ReaderState
        volatile bool shadowCopyUpdated_;               // Signals to the 'next()'-calling thread that the state is updated
        dx_thread::srw_lock thisLock_;                // main lock, used for data modifications
        dx_thread::critical_section closeLock_;           // Prevents simultaneous call to close from several sources
        dx_thread::critical_section disconnectionLock_;           // Prevents simultaneous call to close from several sources

        int64_t   cursorId_;                // Unique ID assigned to this cursor by the server (protocol version 3+). Set once at connect
        int64_t   lastReceivedCommandId_;   // Id of the last received modification command, mostly for debuging

        DxApi::TimestampMs subscriptionTimeMs_;    // This timestamp is passed to new subscription requests

        DxApi::TimestampNs lastTimestamp_;
        DxApi::TimestampNs periodStartTimestamp_;

        IdentityInfo id_;

        enum FILTERS {
            TYPES,
            STREAMS
        };

        // Mirrored internal state
        struct SubscriptionState {

            // Clear message filters, set to "allow all"  
            void clearMessageFilters(bool isSkippingAll = false);
            void clearStreamsArray();
            INLINE bool isExecutingSubscriptions() const { return lastExpectedCommandId_ != -1; }

            INLINE size_t nStreams() const { return tickdbStreams_.size() - 1; }
            /*void parseStreamGuids(const DxApi::TickStream * stream);
            void parseStreamGuids();*/

            bool isResetInProgress_;
            bool readingRestarted_;

            int64_t   lastExpectedCommandId_;    // Id of the last modification command, returned by the server (protocol version 3+)
            int64_t   firstExpectedCommandId_;    // Id of the last modification command, returned by the server (protocol version 3+)
            int64_t   lastResetCommandId_;    // Id of the last modification command, returned by the server (protocol version 3+)

            // Symbol name/type cache, filtering tables
            SymbolTranslator    tables_;
            VectorLUT<uint8_t, 0, 0x10000> isSymbolSkipped_;
            ArrayLUT<uint8_t, 0, 0x100>    isMessageDescriptorSkipped_;
            ArrayLUT<uint8_t, 0, 0x100>    isStreamSkipped_;

            // List of filtered Instruments not yet encountered in the data stream
            std::unordered_map<std::string, bool> isUnregisteredSymbolSkipped_;

            // List of filtered MsgTypeNames not yet encountered in the data stream
            std::unordered_map<std::string, bool> isUnregisteredMessageTypeSkipped_;

            // List of filtered stream keys not yet encountered in the stream.
            // We need this because filter uses incremental tickstream list received from the data stream, not the transient tickstream list in cursor
            std::unordered_map<std::string, bool> isUnregisteredStreamSkipped_;

            //std::unordered_map<std::string, std::string> guid2streamKey_;

            // These streams are currently subscribed + 1. 0th element is always NULL;
            std::vector <const DxApi::TickStream *> tickdbStreams_;

        } subState_[2];

        SubscriptionState * mainState_, *shadowState_;

        // List of flags, marking system messages
        // System messages are always allowed through filters, no matter what
        uint8_t    isSystemMessage_[0x100];

       
        struct Stats {

            // Statistics (currently unused)
#if 0
            /// Number of bytes received from socked after the last request
            uintptr_t nBytesRead;
            /// Number of bytes actually processed
            uintptr_t nBytesProcessed;
#endif

            /// Number of data messages processed, total
            uintptr_t nMessagesProcessed;

            /// Number of data messages processed, total
            uintptr_t nMessagesSkipped;

            /// Number of service messages processed
            uintptr_t nServiceMessagesProcessed;

            /// Number of HTTP chunk headers decoded (incremented as soon as the chunk header is read)
            uintptr_t nHTTPChunksProcessed;
            // TODO: move it!!!

        } stats, prev;
    };

    
    DEFINE_IMPL_CLASS(DxApi::TickCursor, DxApiImpl::TickCursorImpl)

    /*****************************************************************************************
    *  Inline method implementation
    */


    INLINE uintptr_t TickCursorImpl::numStreamsInternal() const
    {
        return mainState_->nStreams();
    }


    INLINE const DxApi::TickStream * TickCursorImpl::streamInternal(intptr_t streamIndex) const
    {
        assert((uintptr_t)(streamIndex + 1) < numStreamsInternal());
        return mainState_->tickdbStreams_[streamIndex + 1];
    }


    INLINE unsigned TickCursorImpl::numStreams()
    {
        dx_thread::srw_read section(thisLock_);
        return (unsigned)mainState_->nStreams();
    }


    INLINE const DxApi::TickStream* TickCursorImpl::stream(intptr_t streamIndex) 
    {
        dx_thread::srw_read section(thisLock_);
        assert((uintptr_t)(streamIndex + 1) < numStreamsInternal());
        return mainState_->tickdbStreams_[streamIndex + 1];
    }


    INLINE byte* TickCursorImpl::inputBuffer() const
    {
        assert(bufferStart_);
        return bufferStart_;
    }


    INLINE size_t TickCursorImpl::inputBufferSize() { return READ_BUFFER_SIZE; }


    INLINE TickCursorImpl::SubscriptionState * TickCursorImpl::other(TickCursorImpl::SubscriptionState * state) const
    {
        if (&subState_[0] == state) return state + 1;
        if (&subState_[1] == state) return state - 1;
        THROW_DBGLOG("Logic error, state pointer has unknown value:" FMT_HEX64, (ulonglong)state);
    }


    INLINE uint8_t TickCursorImpl::setState(CursorState newState)
    {
        return (volatile uint8_t &)state = newState;
    }
}