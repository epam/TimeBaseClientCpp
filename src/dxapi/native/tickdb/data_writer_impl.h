#pragma once

#include "tickdb/common.h"
#include <dxapi/data_writer.h>

#define LOADER_MAX_NESTED_OBJECTS 16

namespace DxApi {
    class TickLoader;
    class DataWriter;
}

namespace DxApiImpl {
    class TickLoaderImpl;

    class DataWriterInternal : public DxApi::DataWriter {
        friend class DxApi::DataWriter;
        friend class DxApi::TickLoader;
        friend class DxApiImpl::TickLoaderImpl;

    public:
        // 
        struct ExpansionPoint {
            uint32_t blockOffset;   // offset of the start of the data block to be moved to the right
            uint32_t headerLength;  // contents of the 3-byte length field to be inserted at offset - 1, top byte contains additional length = [1..2]
            INLINE ExpansionPoint(uint32_t o, uint32_t l) : blockOffset(o), headerLength(l) {}
        };

        INLINE size_t remaining()       const
        {
            return dataEnd - dataPtr;
        }

        INLINE byte* data()
        {
            return dataPtr;
        }
        
        INLINE const byte* dataStart()  const
        {
            return bufferStart;
        }

        INLINE size_t dataSize()        const
        {
            return dataPtr - bufferStart;
        }

        INLINE size_t nestingDepth()    const
        {
            return depth;
        }

        INLINE byte* resetPointer()
        {
            return (dataPtr = bufferStart);
        }

        // Reserve header field and save pointer to the byte following it, also reset other data structures
        INLINE byte* startMessage();

        // Low level writing access
        INLINE void advance(uintptr_t amount)
        {
            dataPtr += amount;
            if (dataPtr > dataEnd) {
                onOverflow();
            }
        }

        template <typename T> static INLINE void storeBE(uint8_t *p, const T &value)
        {
            _storeBE(p, value);
        }

    
        void putInterval(uint32_t value);

        // Write nanoseconds (only)
        void putCompressedNanosecondTime(int64_t value);
        // Write milliseconds
        void putCompressedMillisecondTime(int64_t value);

        // Write PUINT61 or PUINT30
        template <typename T> void writePackedUInt(T value);
        
        void putDecimal(double value, unsigned precision);
        void putDecimal(double value);

        void putAlphanumericNull(uint32_t size);
        
        template <typename T, typename P> void putAlphanumeric(uint32 fieldSize, const P *str, size_t stringLength);
        template <typename T> void putAlphanumericInt(uint32_t fieldSize, T value);

        //template<int n> void writeAlphanumeric(const char *str, uintptr stringLength, uintptr fieldSize);

        //void writeAlphanumeric32(const char *s, uintptr dataSize, uintptr fieldSize);
        //void writeAlphanumeric(const char *s, uintptr dataSize, uintptr fieldSize);

        void depth0ExpectedError();
        void expandedTooBigError(size_t size, size_t remaining);

        void writeArrayStart(size_t nItems);
        void writeArrayEnd();

        void writeObjectStart(uint8_t typeIndex);
        void writeObjectEnd();

        // Write binary array header, get write pointer, for given size
        byte* writeBinaryArrayEx(size_t size);
    
        byte* finishMessage(const byte *messageStart);
        byte* finishMessageCommon();


        void setBuffer(byte *allocPtr, byte *startPtr, byte *endPtr);
        DataWriterInternal();

    protected:
        bool writeSpecialDecimal(double value);

        // Write uint, returning the actual number of nonzero bytes used for storing the value.
        // Note: if the value is 0, the number of written bytes is also 0, if this behavior isn't desired, use an extra check
        template <typename T> uintptr writeVariableLengthInt(byte *&headerPtr, T value);
        void writePackedUInt22(unsigned value);

        void reserveLengthField();
        void openContainer(bool isArray);
        void closeContainer(bool isArray);
        static void expandHeaders(uint8_t * const pBufferStart, uint8_t *pEnd, intptr_t offset,
            const ExpansionPoint points[], size_t n_points, const uint8_t * const pMessageStart);

        template <typename T> INLINE void putLE(const T value, size_t size)
        {
            uint8_t *p;
            assert(size <= 8);
            _storeLE(p = dataPtr, value);
            if ((dataPtr = p + size) > dataEnd) {
                onOverflow();
            }
        }


        byte* onOverflow();

    protected:
        byte *bufferAllocPtr;
        byte *bufferStart;

    /**
     * Complex types support (Arrays/Objects)
     */
        size_t      depth;
        byte        *containerHeader[LOADER_MAX_NESTED_OBJECTS];
        byte        containerIsArray[LOADER_MAX_NESTED_OBJECTS];
        uint32_t    containerContentsExtraSize[LOADER_MAX_NESTED_OBJECTS + 1]; // Last level is written, never read

#if (LOADER_OBJ_IMPL == 2)
        // offsets are relative to bufferStart
        std::vector<ExpansionPoint> expansionPoints;
#endif

        DISALLOW_COPY_AND_ASSIGN(DataWriterInternal);
    };

    DEFINE_IMPL_CLASS(DxApi::DataWriter, DataWriterInternal);

    INLINE void DataWriterInternal::putAlphanumericNull(uint32 fieldSize)
    {
        // alphanumeric null is encoded by storing size + 1 in high bits
        uintptr nSizeBits = _bsr(++fieldSize);
        putBE<uint32>(fieldSize << (32 - nSizeBits), (nSizeBits + 7) >> 3);
    }


    INLINE byte* DataWriterInternal::finishMessageCommon()
    {
        if (0 != depth) {
            depth0ExpectedError();
        }

#if 1 == LOADER_TRIM_NULL_FIELDS
        return min(dataPtr, dataEndNotNull);
#else
        return dataPtr;
#endif
    }


#if LOADER_OBJ_IMPL==1
    INLINE byte * DataWriterInternal::finishMessage(const byte * messageStart)
    {
        return finishMessageCommon();
    }
#elif LOADER_OBJ_IMPL==2

    INLINE byte* DataWriterInternal::startMessage()
    {
        uint8_t *p;
        // Write placeholder for length
        _storeLE(p = dataPtr, 0);
        if ((dataPtr = p += 4) > dataEnd) {
            onOverflow();
        }
        dataEndNotNull = p + (uintptr_t)TDB::LOADER_MESSAGE_HEADER_SIZE; /* initialize with full header size including length */
        expansionPoints.clear();
        containerContentsExtraSize[0] = 0;
        depth = 0;
        return p;
    }

    INLINE byte* DataWriterInternal::finishMessage(const byte *messageStart)
    {
        byte *dataPtr = finishMessageCommon();
        unsigned extraSize = containerContentsExtraSize[0];
        assert((extraSize != 0) == (expansionPoints.size() != 0));

        if (0 == extraSize) {
            return dataPtr;
        }

        // This part only executes when there's at least one container object with content size >= 0x80,
        // So the performance cost of the following code is amortized

        if (dataPtr + extraSize > dataEnd) {
            expandedTooBigError(dataPtr + extraSize - messageStart, dataEnd - messageStart);
        }

        assert(expansionPoints.data() != NULL);
        expandHeaders(bufferStart, dataPtr, extraSize, expansionPoints.data(), expansionPoints.size(), messageStart);
        return (this->dataPtr = dataPtr + extraSize);
    }


#else
#error LOADER_OBJ_IMPL not defined
#endif

}

namespace DxApi {
    INLINE DataWriter::DataWriter()
    {}
}