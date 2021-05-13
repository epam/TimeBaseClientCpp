#pragma once

#include "tickdb/common.h"
#include <dxapi/data_reader.h>
#include <algorithm>
#include "tickdb_base.h"

#define READER_MAX_NESTED_OBJECTS 16
#define SAFE_GETBYTES 1

// TODO: Actually this is not the best architecture, but an incremental improvement from a worse one.
// This inheritance chain is probably not necessary and maybe the actual implementation of the internals should be done using C, not C++ OOP style
// (instead of virtual method, function pointer returning new structure, maybe even packed into delta uint64, instead of what we now have)
// Basically, in the next implementation we shouldn't inherit anything from DataReader class that is exposed to user, it should be only connected to implementation via a function pointer


/****
 DataReader is API user-visible class that implements minimum functionality inline, delegating the rest to DataReaderBaseImpl
 
 DataReaderBaseImpl implement all non-inline codec functions, and contains the data fields not visible to the user, but does not implement a method to fill the buffer with new input data
 These functions are delegated to a subclass via virtual method calls (methods get() and skip() ). Still it houses data members needed for those classes

 

 DataReaderHTTP & DataReaderHTTPchunked fill buffer with data
 DataReaderMsgHTTP & DataReaderMsgHTTPchunked implement message end barrier, delegating the rest of work to previous classes


 */


namespace DxApiImpl {

    // String reader internal methods (not included into public APIs)
    class DataReaderInternal : public DxApi::DataReader {

        // It is possible that utility/data conversion methods will be moved from DataReader later and only buffer operations will remain
    public:
        // Number of contiguous unread bytes in the buffer
        INLINE uintptr_t    nBytesAvailable() const;

        INLINE uintptr_t    nBytesAvailableInStream() const;

        // Internal functions for reading data fields
        // Don't contain message end check
        uint8_t     getByte();
        uint16_t    getUInt16();
        int16_t     getInt16();
        uint32_t    getUInt32();
        uint64_t    getUInt64();
        int64_t     getInt64();
        double      getFloat64();
        float       getFloat32();

        // Redefined as pubic for internal use in tests
        INLINE const uint8_t * bytes(uintptr_t size) { return DataReader::bytes(size); }

        // Use with caution, internal use only
        const uint8_t * peek(uintptr_t size);

        // Compressed unsigned integers (internal use)
        uint32_t getPackedUInt22();
        uint32_t getPackedUInt30();
        uint64_t getPackedUInt61();
        void skipPackedUInt22();

        double getDecimal();
        uint32 getInterval();
        template<int64_t T> int64_t getCompressedNanoTimeDividedBy();

        //bool getAlphanumeric32(std::string& out, uintptr size);
        //bool getAlphanumeric64(std::string& out, uintptr size);

        // Copy from the source stream, NOT NULLABLE
        void getBytes(uint8_t * destinationBuffer, uintptr_t size);

        // Copy N data words from the source stream, NOT NULLABLE
        template <typename T> void getWords(T * destinationBuffer, uintptr_t n);

        // Read length part of a string field, converting 0xFFFF into -1
        // If conversion is not needed, use getUInt16()
        intptr_t    getUTF8length();
        void getUTF8chars(wchar_t * destinationBuffer, uint32_t size); // TODO: Not implemented 
        void getUTF8chars(char * destinationBuffer, uintptr_t size);
        void getUTF8chars(uint8_t * destinationBuffer, uintptr_t size);
        bool getUTF8_inl(std::string &value);
        bool getUTF8(std::string &value);

        intptr_t    getAsciilength();
        void getAsciichars(wchar_t * destinationBuffer, uint32_t size); // TODO: Not implemented 
        void getAsciichars(char * destinationBuffer, uintptr_t size);
        void getAsciichars(uint8_t * destinationBuffer, uintptr_t size);
        bool getAscii_inl(std::string &value);
        bool getAscii(std::string &value);

        template <typename T, typename Q> bool getAlphanumeric(std::basic_string<Q, std::char_traits<Q>, std::allocator<Q>>& out, uint32_t size);
        template <typename T> T getAlphanumericAs(uint32_t maxFieldLength);
        void skipAlphanumeric(uint32 size);

        // Utility methods for converting alphanumerics

        // Verify if string chars can be packed into alphanumeric
        template <typename T> static bool isValidAlphanumeric(const T * stringChars, uint32_t length);
        // Verify packed alphanumeric
        template <unsigned SIZE> static bool isValidAlphanumeric(uint64_t value);
        
        // Returns actual length of the string read. Value of SIZE + 1 denotes null, length > SIZE + 1 means invalid data in value
        template <unsigned SIZE, typename T> static unsigned fromAlphanumericInt64(T * dest, uint64_t value);
        
        template <unsigned SIZE, typename T> static uint64_t toAlphanumericInt64(const T * stringChars, uint32_t length);

    protected:

        // Protected empty default constructor, no copying support
        INLINE DataReaderInternal() {}
        DISALLOW_COPY_AND_ASSIGN(DataReaderInternal);
    };

    // TODO: DataReaderInternal ?
    class DataReaderBaseImpl : public DataReaderInternal {
        friend class DataReaderInternal;
        friend class DxApi::DataReader;

    public:

        void        resetMessageSize();
        void        setMessageSize(uintptr size);
        void        nextMessage();

        // Note: buffer is expected to actually be bigger than buffersize. At least 8 bytes of guard area is required after the end and at least 8-1 before the bufferStart due to optimizations
        DataReaderBaseImpl(byte *buffer, size_t bufferSize, size_t dataSize, InputStream *source);

        INLINE void setInputStream(InputStream * source) { s = source; }

        // Some defensive programming never hurts
        INLINE void invalidate() { dataPtr = dataEnd = NULL; s = NULL; }

        template <typename T> bool isValidAlphanumeric(const T *chars, uint32_t size);
        bool isValidAlphanumeric10(uint64_t);

    protected:
        const uint8_t* onPeekBarrier(const uint8_t *from, size_t size);
        void        skipUnreadMessageBody();
        void        skipContainer(bool isArray);
        bool        openContainer(bool isArray);
        void        closeContainer(bool isArray);

        //INLINE const byte * messageEnd() const { return end[0]; }

        // Called when pre-incremented dataPtr goes beyond dataEnd pointer. Parameter is previous dataPtr value
        // returns new value of dataPtr

        // Make sure that the required amount of bytes is available in the buffer
        // Taked and returns pointer to the start of the data (previous dataPtr).
        // Assumes that prevPtr =< dataEnd < dataPtr
        // dataPtr before and after call points at the next byte after the piece being read,
        // [prevPtr, dataPtr) region is invalid before the call, but becomes valid as result.
        virtual const byte * get(const byte * from, size_t size);
        virtual void skip(const byte * from, size_t size);

        const byte * get_inl(const byte * from, size_t size);
        void skip_inl(const byte * from, size_t size);
        
        // Handles buffer layer and input stream layer

        template<bool SKIPEXISTINGDATA> const byte* getFromStream(const byte *from, size_t size);

    protected:
        // Move Constructor implemented as shallow copy, 
        DataReaderBaseImpl(DataReaderBaseImpl&& other) :
            bufferStart(other.bufferStart),
            bufferEnd(other.bufferEnd),
            streamDataEnd(other.streamDataEnd),
            s(other.s),
            dataEnd0(other.dataEnd0),
            depth(other.depth)
        {
            dataPtr = other.dataPtr;
            dataEnd = other.dataEnd;
            contentEnd = other.contentEnd;
            schemaPtr = other.schemaPtr;

            memcpy(eOfs, other.eOfs, sizeof(eOfs));
            memcpy(containerIsArray, other.containerIsArray, sizeof(containerIsArray));
            other.invalidate();
        }

    protected:
        const byte  *bufferStart;         // Buffer start pointer
        const byte  *bufferEnd;           // Buffer end pointer
        const byte  *streamDataEnd;       // Source data end pointer
        InputStream *s;

        // The code that uses these fields is in superclasses.
        // This base class by itself is not aware of HTTP chunks and TB messages, but still hosts the required data fields.
        // This is necessary because of inheritance fork.

        const byte *dataEnd0;
        uintptr_t depth;
        uintptr_t eOfs[READER_MAX_NESTED_OBJECTS];
        byte containerIsArray[READER_MAX_NESTED_OBJECTS];


        DISALLOW_DEFAULT_CONSTRUCTORS(DataReaderBaseImpl);
    };


    class DataReaderHTTPchunked : public DataReaderBaseImpl {
        // Note, buffer must be able to fit the biggest data element AND the chunk header (CRLF + length + CRLF)
    public:
        DataReaderHTTPchunked(byte * buffer, uintptr bufferSize, uintptr dataSize, InputStream * s) :
            DataReaderBaseImpl(buffer, bufferSize, dataSize, s)
        {
            chunkEnd = dataEnd; // We assume that the chunk content ends at dataSize
        }

        DataReaderHTTPchunked(DataReaderBaseImpl &&other) :
            DataReaderBaseImpl(std::move(other))
        {
            dataPtr -= 2;
            assert(dataPtr >= bufferStart);
            if (dataPtr >= bufferStart) {
                _storeLE((char *)dataPtr, (uint16_t)0x0A0D); // TODO: debug code
            }

            chunkEnd = dataEnd = dataPtr; // We assume that chunk ends at the current point
        }


        DataReaderHTTPchunked(DataReaderHTTPchunked &&other) :
            DataReaderBaseImpl(std::move(other)), chunkEnd(other.chunkEnd)
        {
        }


    public:
        virtual const byte * get(const byte *from, size_t size);
        virtual void skip(const byte *from, size_t size);
        template <bool SKIP> const byte* getChunked(const byte *from, size_t size);
        
    protected:
        const byte * chunkEnd;

        DISALLOW_DEFAULT_CONSTRUCTORS(DataReaderHTTPchunked);
    };


    class DataReaderMsgHTTPchunked : public DataReaderHTTPchunked {
    public:
        DataReaderMsgHTTPchunked(DataReaderBaseImpl &&other) :
            DataReaderHTTPchunked(std::move(other))
        {
            dataEnd0 = dataEnd;
        }
    protected:
        virtual const byte * get(const byte *pData, size_t size);
        virtual void skip(const byte *pData, size_t size);

        DISALLOW_DEFAULT_CONSTRUCTORS(DataReaderMsgHTTPchunked);
    };


    class DataReaderMsg : public DataReaderBaseImpl {
    public:
        DataReaderMsg(byte * buffer, size_t bufferSize, size_t dataSize, InputStream * source) :
            DataReaderBaseImpl(buffer, bufferSize, dataSize, source)
        {
            dataEnd0 = dataEnd;
        }

    protected:
        virtual const byte * get(const byte *pData, size_t size);
        virtual void skip(const byte *pData, size_t size);

        DISALLOW_DEFAULT_CONSTRUCTORS(DataReaderMsg);
    };


    INLINE uintptr_t DataReaderInternal::nBytesAvailable() const
    {
        return dataEnd - dataPtr;
    }

    INLINE uintptr_t DataReaderInternal::nBytesAvailableInStream() const
    {
        return static_cast<const DataReaderBaseImpl*>(this)->streamDataEnd - dataPtr - 2; // ???
    }

    INLINE uint8_t DataReaderInternal::getByte()
    {
        return loadBE<uint8_t>();
    }


    INLINE uint16_t DataReaderInternal::getUInt16()
    {
        return loadBE<uint16_t>();
    }


    INLINE int16_t DataReaderInternal::getInt16()
    {
        return loadBE<uint16_t>();
    }


    INLINE uint32_t DataReaderInternal::getUInt32()
    {
        return loadBE<uint32_t>();
    }


    INLINE uint64_t DataReaderInternal::getUInt64()
    {
        return loadBE<uint64_t>();
    }


    INLINE int64_t DataReaderInternal::getInt64()
    {
        return getUInt64();
    }


    INLINE double DataReaderInternal::getFloat64()
    {
        return loadBE<double>();
    }


    INLINE float DataReaderInternal::getFloat32()
    {
        return loadBE<float>();
    }

    // Read length part of a string field, converting 0xFFFF into -1
    // If this conversion is not needed, use getUInt16()
    INLINE intptr_t DataReaderInternal::getUTF8length()
    {
        return (intptr_t)((getUInt16() + 1) & 0xFFFF) - 1;
    }
    INLINE intptr_t DataReaderInternal::getAsciilength()
    {
        return (intptr_t)((getUInt16() + 1) & 0xFFFF) - 1;
    }


    INLINE void DataReaderInternal::getUTF8chars(char * destinationBuffer, uintptr_t dataLength)
    {
        getBytes((uint8_t *)destinationBuffer, dataLength);
    }


    INLINE void DataReaderInternal::getUTF8chars(uint8_t * destinationBuffer, uintptr_t dataLength)
    {
        getBytes((uint8_t *)destinationBuffer, dataLength);
    }


    INLINE void DataReaderInternal::getAsciichars(char * destinationBuffer, uintptr_t dataLength)
    {
        getBytes((uint8_t *)destinationBuffer, dataLength);
    }

    INLINE void DataReaderInternal::getAsciichars(uint8_t * destinationBuffer, uintptr_t dataLength)
    {
        getBytes((uint8_t *)destinationBuffer, dataLength);
    }

    INLINE void DataReaderBaseImpl::resetMessageSize()
    {
        assert(0 == depth); // In places that make calls to this, depth should be 0
        contentEnd = (byte *)(UINTPTR_MAX - READ_BUFFER_ALLOC_SIZE);
        dataEnd = dataEnd0;
    }


    INLINE void DataReaderBaseImpl::setMessageSize(uintptr size)
    {
        assert(contentEnd > (byte *)(UINTPTR_MAX / 2));
        auto e = contentEnd = dataPtr + size;
        dataEnd0 = dataEnd;
        dataEnd = std::min(dataEnd, e);
        depth = 0;
    }


    INLINE void DataReaderBaseImpl::nextMessage()
    {
        assert(dataPtr <= contentEnd);
        // Quick test if the previous message is closed in a clean way. '|' is intended. 
        if (0 != ((contentEnd - dataPtr) | depth)) {
            skipUnreadMessageBody();
        }

        resetMessageSize();
    }


    INLINE const uint8_t * DataReaderInternal::peek(uintptr_t size)
    {
        const uint8_t * p = dataPtr;
        if (p + size > dataEnd) {
            p = static_cast<DataReaderBaseImpl*>(this)->onPeekBarrier(p, size);
        }
        return p;
    }
}

namespace DxApi {
    INLINE DataReader::DataReader() {}
}