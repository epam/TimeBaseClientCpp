#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "dxapi/schema.h"
#include "data_reader_impl.h"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace Schema;


#define THIS_IMPL (static_cast<DataReaderBaseImpl *>(this))
#define THIS_IMPL_CONST (static_cast<const DataReaderBaseImpl *>(this))

typedef DataReaderBaseImpl DataReaderImpl;


static const double pow10dLUT[16] = {
    1.E0, 1.E1, 1.E2, 1.E3, 1.E4, 1.E5, 1.E6, 1.E7, 1.E8, 1.E9, 1.E10, 1.E11, 1.E12, 1.E13, 1.E14, 1.E15
};

static const double signdLUT[2] = { 1.0, -1.0 };

// TODO: make platform-agnostic
static const uint64  lowbytemask64LUT[9] = {
    UINT64_C(0x0000000000000000),
    UINT64_C(0x00000000000000FF),
    UINT64_C(0x000000000000FFFF),
    UINT64_C(0x0000000000FFFFFF),
    UINT64_C(0x00000000FFFFFFFF),
    UINT64_C(0x000000FFFFFFFFFF),
    UINT64_C(0x0000FFFFFFFFFFFF),
    UINT64_C(0x00FFFFFFFFFFFFFF),
    UINT64_C(0xFFFFFFFFFFFFFFFF)
};

static const uint64 highbytemask64LUT[9] = {
    UINT64_C(0x0000000000000000),
    UINT64_C(0xFF00000000000000),
    UINT64_C(0xFFFF000000000000),
    UINT64_C(0xFFFFFF0000000000),
    UINT64_C(0xFFFFFFFF00000000),
    UINT64_C(0xFFFFFFFFFF000000),
    UINT64_C(0xFFFFFFFFFFFF0000),
    UINT64_C(0xFFFFFFFFFFFFFF00),
    UINT64_C(0xFFFFFFFFFFFFFFFF)
};

static const uint32  lowbytemask32LUT[5] = {
    UINT32_C(0x00000000),
    UINT32_C(0x000000FF),
    UINT32_C(0x0000FFFF),
    UINT32_C(0x00FFFFFF),
    UINT32_C(0xFFFFFFFF)
};

static const uint32 highbytemask32LUT[5] = {
    UINT32_C(0x00000000),
    UINT32_C(0xFF000000),
    UINT32_C(0xFFFF0000),
    UINT32_C(0xFFFFFF00),
    UINT32_C(0xFFFFFFFF)
};


// Note: buffer is expected to actually be bigger than buffersize. At least 8 bytes of guard area is required after the end and at least 8 - 1 before the bufferStart due to optimizations
DataReaderBaseImpl::DataReaderBaseImpl(byte * buffer, uintptr bufferSize, uintptr dataSize, InputStream * source) :
bufferStart(buffer), bufferEnd(buffer + bufferSize), s(source)
{
    dataPtr = buffer;
    streamDataEnd = dataEnd = dataEnd0 = const_cast<const byte *>(buffer + dataSize);
    contentEnd = (const byte *)(UINTPTR_MAX - READ_BUFFER_ALLOC_SIZE);
    depth = 0;
}


// TODO:
/*
static INLINE unsigned packedIntFieldSize(int x) {
    if (x < 0)
        return 0; // error

    if (x < 0x80)
        return 1;

    if (x < 0x4000) 
        return 2;

    if (x < 0x400000) 
        return 3;

    return 0; // error 
}
*/


// TODO: Remove soon
#if 0
INLINE bool DataReaderImpl::getAlphanumeric32(std::string& out, uintptr size)
{
    const byte * readPtr;
    byte * writePtr;
    unsigned sizeBits = _bsr((uint32)size + 1);      // number of bits to store |[0..size]| = size + 1 values
    unsigned numBits  = sizeBits + 6 * (unsigned)size;
    unsigned numBytes = (numBits + 7) >> 3;             // total number of bytes to store these bits
    register unsigned accumulator;
    assert(size < READ_BUFFER_SIZE);
    
    readPtr = bytes(numBytes); // In this version we are fetching all the bytes immediately
    accumulator = _loadBE<uint32>(readPtr); // read 4 bytes in BE order
    readPtr += 4;
    intptr len = accumulator >> (32 - sizeBits);
    int bitIndex = 32 - sizeBits - 6;
    if (size + 1 == len) {
        return false;
    }
    out.resize(len);
    writePtr = (byte *)&out[len];
    intptr i = -len;
    for (;0 != i; ++i) {
        writePtr[i] = ((accumulator >> bitIndex) & 0x3F) + ' ';
        bitIndex -= 6;
        if (bitIndex < 0) {
            // This block should be executed in exactly 1 case out of 4, branch predictor will work well
            // Never executed for string lengths < 5
            bitIndex += 24;
            // shift in 3 more bytes (in BE order)
            accumulator = (accumulator << 24) | (_loadBE<uint32>(readPtr) >> 8);
            readPtr += 3;
        }
    }

    return true;
}


INLINE bool DataReaderImpl::getAlphanumeric64(std::string& out, uintptr size)
{
    const byte * readPtr;
    byte * writePtr;
    unsigned sizeBits = _bsr((uint32)size + 1);      // number of bits to store |[0..size]| = size + 1 values
    unsigned numBits  = sizeBits + 6 * (unsigned)size;
    unsigned numBytes = (numBits + 7) >> 3;             // total number of bytes to store these bits
    register uint64 accumulator;
    assert(size < READ_BUFFER_SIZE);

    readPtr = bytes(numBytes); // In this version we are fetching all the bytes immediately
    accumulator = _loadBE<uint64>(readPtr); // read 8 bytes in BE order
    readPtr += 8;
    
#ifdef DX_PLATFORM_64
    uintptr len = accumulator >> (64 - sizeBits);
#else
    uintptr len = (accumulator >> 32) >> (32 - sizeBits);
#endif

    int bitIndex = 64 - sizeBits - 6;
    if (size + 1 == len) {
        return false;
    }
    out.resize(len);
    writePtr = (byte *)&out[len];
    intptr i = -(intptr)len;
    for (;0 != i; ++i) {
        writePtr[i] = ((accumulator >> bitIndex) & 0x3F) + ' ';
        bitIndex -= 6;
        if (bitIndex < 0) {
            // This block should be executed in exactly 1 case out of 4, branch predictor will work well
            // Never executed for string lengths < 11
            bitIndex += 56;
            // shift in 7 more bytes (in BE order)
            accumulator = (accumulator << 56) | (_loadBE<uint64>(readPtr) >> 8);
            readPtr += 7;
        }
    }

    return true;
}
#endif


// Template parameter is the read size (uint32/uint64)


// TODO: Implement reading in fixed size buffer, or maybe use "placement new" strings?
#if 0
template<typename T, typename Q> INLINE bool DataReaderImpl::getAlphanumeric(Q * dest, uintptr_t destLength, uint32 maxFieldLength)
{
    const byte * readPtr;
    byte * writePtr;
    unsigned sizeBits = _bsr((uint32)++maxFieldLength);               // number of bits to store |[0..size]| = size + 1 values
    unsigned numHeaderBytes;
    register T accumulator;
    assert(maxFieldLength <= READ_BUFFER_SIZE);

    readPtr = peek(numHeaderBytes = (sizeBits + 7) >> 3);           // Fetch the amount of bytes necessary to store size
    accumulator = _loadBE<T>(readPtr);                              // read 4/8 bytes in BE order
    intptr len = accumulator >> (BITSIZEOF(T)- sizeBits);
    if (maxFieldLength == len) {
        // == NULL
        DataReader::skip(numHeaderBytes);
        return false;
    }

    readPtr = bytes((sizeBits + len * 6 + 7) >> 3); // Fetch the full actual number of bytes
    accumulator = _loadBE<T>(readPtr); // read 4/8 bytes in BE order
    readPtr += sizeof(T);
    int bitIndex = BITSIZEOF(T) - sizeBits - 6;

    out.resize(len);
    writePtr = (byte *)&out[len];
    intptr i = -len;
    for (; 0 != i; ++i) {
        writePtr[i] = ((accumulator >> bitIndex) & 0x3F) + ' ';
        bitIndex -= 6;
        if (bitIndex < 0) {
            // This block should be executed in exactly 1 case out of 4/8, branch predictor will work well
            // Never executed for string lengths < 5/11
            bitIndex += (sizeof(T)-1) * 8;
            // shift in 7 more bytes (in BE order)
            accumulator = (accumulator << (sizeof(T)-1) * 8) | (_loadBE<T>(readPtr) >> 8);
            readPtr += sizeof(T)-1;
        }
    }

    return true;
}
#endif

// Template parameter is the read size (uint32/uint64)
// TODO: Not 100% correct, small problem exists, will be rewritten to more general but more complex implementation
template<typename T, typename Q> INLINE bool DataReaderInternal::getAlphanumeric(std::basic_string<Q, char_traits<Q>, allocator<Q>>& out, uint32 size)
{
    typedef typename make_unsigned<Q>::type U_Q;
    const byte * readPtr;
    U_Q * writePtr;
    unsigned sizeBits = _bsr((uint32)++size);               // number of bits to store |[0..size]| = size + 1 values
    unsigned numHeaderBytes;
    T accumulator;
    assert(size <= READ_BUFFER_SIZE);

    readPtr = peek(numHeaderBytes = (sizeBits + 7) >> 3);   // Fetch the amount of bytes necessary to store size
    accumulator = _loadBE<T>(readPtr); // read 4/8 bytes in BE order
    //readPtr += sizeof(T);
    intptr len = accumulator >> (BITSIZEOF(T) - sizeBits);
    if (size == len) {
        // == NULL
        DataReader::skip(numHeaderBytes); // TODO: Kludge
        return false;
    }

    readPtr = bytes((sizeBits + len * 6 + 7) >> 3); // Fetch the full actual number of bytes
    accumulator = _loadBE<T>(readPtr); // read 4/8 bytes in BE order
    readPtr += sizeof(T);
    int bitIndex = BITSIZEOF(T) - sizeBits - 6;

    out.resize(len);

    writePtr = (U_Q *)&out[len];
    intptr i = -len;
    for (; 0 != i; ++i) {
        writePtr[i] = ((accumulator >> bitIndex) & 0x3F) + ' ';
        bitIndex -= 6;
        if (bitIndex < 0) {
            // This block should be executed in exactly 1 case out of 4/8, branch predictor will work well
            // Never executed for string lengths <= 4/10
            bitIndex += (sizeof(T) - 1) * 8;
            // shift in 3/7 more bytes (in BE order)
            accumulator = (accumulator << (sizeof(T) - 1) * 8) | (_loadBE<T>(readPtr) >> 8);
            readPtr += sizeof(T) - 1;
        }
    }

    return true;
}


template <typename T> INLINE T DataReaderInternal::getAlphanumericAs(uint32_t size)
{
    //  deltix/qsrv/hf/pub/codec/AlphanumericCodec.java:75
    assert(size < READ_BUFFER_SIZE);
    uintptr nSizeBits = _bsr(++size);     // number of bits to store |[0..size]| = size + 1 values
    if (size > 10 + 1) {
        throw runtime_error("Size is too big for AlphanumericInt");
    }

    uintptr len = (uintptr)(_loadBE<byte>(peek(1)) >> (8 - nSizeBits));
    if (len >= size) {
        if (len == size) {
            DataReader::skip(1); // TODO: Kludge
            // ALPHANUMERIC_NULL == INT64_NULL == INT64_MIN == INT8_MIN << 56(for 64-bit T)
            return (T)(INT64_C(-0x80) << (sizeof(T) - 1) * 8);
        }
        throw runtime_error("Alphanumeric field length is too big to fit into integer type");
    }

    uintptr fullSize = ((nSizeBits + 7) + 6 * len) >> 3;
    return highbytemask64LUT[fullSize] & _loadBE<T>(bytes(fullSize));
}


INLINE void DataReaderInternal::skipAlphanumeric(uint32_t size)
{
    uintptr nSizeBits = _bsr(++size);     // number of bits to store |[0..size]| = size + 1 values
    uintptr nSizeBytes = (nSizeBits + 7) >> 3;
    // Get the number of bytes nececcary to hold length, get actual length
    uintptr len = _loadBE<uint32>(peek(nSizeBytes)) >> (32 - nSizeBits);
    // Do nothing if length == size + 1 (null value), otherwise skip the remaining bytes
    DataReader::skip(len != size ? ((nSizeBits + 7) + 6 * len) >> 3 : nSizeBytes);
}


void DataReader::skipAlphanumeric(uint32_t size)
{
    if (!endOfContent()) {
        THIS_IMPL->skipAlphanumeric(size);
    }
}


template <typename Q> bool DataReader::readAlphanumeric(basic_string<Q, char_traits<Q>, allocator<Q>>& value, uint32 size)
{
    SR_LOG_READ(bool, Alphanumeric);
    return endOfContent() ? false : THIS_IMPL->getAlphanumeric<uint32, Q>(value, size);
}

template bool DataReader::readAlphanumeric<char>(std::string& value, uint32 size);
template bool DataReader::readAlphanumeric<wchar_t>(std::wstring& value, uint32 size);


template <typename T> NOINLINE T DxApi::DataReader::readAlphanumericAs(uint32_t maxFieldLength)
{
    SR_LOG_READ(T, AlphanumericAs);
    return endOfContent() ?
        (T)(INT64_C(-0x80) << (sizeof(T)-1) * 8)
        :
        THIS_IMPL->getAlphanumericAs<T>(maxFieldLength);
}


// Instantiate two versions of the template. uint16 also possible but pointless
template NOINLINE uint32 DataReader::readAlphanumericAs<uint32>(uint32 maxFieldLength);
template NOINLINE uint64 DataReader::readAlphanumericAs<uint64>(uint32 maxFieldLength);


   // It is possible that utility/data conversion methods will be moved from DataReader later and only buffer operations will remain

// Verify string characters for being in the valid range for alphanumeric type. Empty or null strings are valid
template <typename Q> NOINLINE bool DataReaderInternal::isValidAlphanumeric(const Q * str, uint32 length)
{
    typedef typename make_unsigned<Q>::type U_Q;
    intptr i = length;
    if (0 == i) {
        return true;
    }

    if (NULL == str || i > 0xFFFE /* Max alphanumeric length that will supposedly go through all current codecs */) {
        return false;
    }

    str += i;
    i = -i;

    do {
        if (((U_Q)str[i] - (unsigned)' ') >= 0x40) {
            return false;
        }
    } while (0 != ++i);

    return true;
}


template NOINLINE bool DataReaderInternal::isValidAlphanumeric<wchar_t>(const wchar_t * str, uint32 length);
template NOINLINE bool DataReaderInternal::isValidAlphanumeric<uint8_t>(const uint8_t * str, uint32 length);
template NOINLINE bool DataReaderInternal::isValidAlphanumeric<char>(const char * str, uint32 length);


    // Verify packed alphanumeric
    // Will only verify length field, as there are no invalid packed alphanumeric characters
    // could also verify that bits beyond length are 0
template <unsigned SIZE> NOINLINE bool DataReaderInternal::isValidAlphanumeric(uint64 value)
{
    // Value of ALPHANUMERIC_NULL will also return true without additional check for any values of SIZE
    static_assert(SIZE <= 10, "Alphanumeric size should be <= 10 here");
    static_assert(((uint64_t)(ALPHANUMERIC_NULL) >> (64 - 4)) == 8, "ALPHANUMERIC_NULL value changed? Recheck this code, please");

    uintptr nSizeBits = _bsr(SIZE + 1);           // number of bits to store |[0..size]| = size + 1 values
                                                // TODO: Check if _bsr with constant parameter actually replaced with constant
    unsigned len = (unsigned)(value >> (BITSIZEOF(value) - nSizeBits));
    return len <= SIZE + 1; // valid lengths are: 0..n, n + 1
}

// We only define this methods for alphanumeric(10) type, other sizes should cause linking error
template NOINLINE bool DataReaderInternal::isValidAlphanumeric<10>(uint64 value);


// if length >= SIZE + 1 returns length without converting any chars, without distinguishing between NULL and invalid
template <unsigned SIZE, typename T> unsigned DataReaderInternal::fromAlphanumericInt64(T * dest, uint64_t value)
{
    static_assert(SIZE <= 10, "Alphanumeric size should be <= 10 here");
    
    if (ALPHANUMERIC_NULL == value) {
        return SIZE + 1;
    }

    const intptr nSizeBits = _bsr(SIZE + 1);           // number of bits to store |[0..size]| = size + 1 values
    unsigned len = (unsigned)(value >> (BITSIZEOF(value) - nSizeBits));

    value <<= nSizeBits;
    switch (len) {
#define DUFF(i) case i: dest[i - 1] = (T)(((value >> (64 - 6 * (i))) & 0x3F) + 0x20);

            DUFF(10)
            DUFF(9)
            DUFF(7)
            DUFF(8)
            DUFF(6)
            DUFF(5)
            DUFF(4)
            DUFF(3)
            DUFF(2)
            DUFF(1)
        case 0:
        default:
            break;
    }
    
    return len;
}
#undef DUFF

template unsigned DataReaderInternal::fromAlphanumericInt64<10, wchar_t>(wchar_t * chars, uint64_t value);


template <unsigned SIZE, typename T> uint64_t DataReaderInternal::toAlphanumericInt64(const T *chars, uint32_t length)
{
    static_assert(SIZE <= 10, "Alphanumeric size should be <= 10 here");
    const intptr nSizeBits = _bsr(SIZE + 1);

    if(length > SIZE) {
        return ALPHANUMERIC_NULL;
    }

    if (NULL == chars) {
        return ALPHANUMERIC_NULL;
    }
    
    uint64 acc = 0;
    
    // No char verification. All verification should be made with "IsValid" functions, if necessary
    switch (length) {
#define DUFF(i) case i: acc |= (uint64)((chars[i - 1] - 0x20) & 0x3F) << (10 - i) * 6;

            DUFF(10)
            DUFF(9)
            DUFF(8)
            DUFF(7)
            DUFF(6)
            DUFF(5)
            DUFF(4)
            DUFF(3)
            DUFF(2)
            DUFF(1)
        case 0:
        default:
            break;
    }
    
    return (length << (64 - nSizeBits)) + (acc << (4 - nSizeBits));
}
#undef DUFF

template uint64_t DataReaderInternal::toAlphanumericInt64<10>(const wchar_t * chars, uint32_t length);


INLINE double DataReaderInternal::getDecimal()
{
    // Compressed decimal has the following format:
    // 7..0:SLLLEEEE
    // E is negative power of 10 (0->1, 1->1/10, 2->1/100)
    // S is the sign
    // L is length of mantissa, in bytes.
    // mantissa is read in LE format, not BE!

    unsigned header = getByte();

    switch (header) {
    case 0x1F:      return DECIMAL_NULL;
    case 0x2F:      return FLOAT64_NEGATIVE_INFINITY; // -std::numeric_limits<double>::infinity()
    case 0x3F:      return FLOAT64_POSITIVE_INFINITY; //std::numeric_limits<double>::infinity()
    case 0x0F:      return getFloat64();
    }

    // Mantissa length == 0 ?
    uintptr numBytes = (header >> 4) & 0x7;
    if (0 == numBytes)
        return 0.0;

    const uint64 x = *(uint64 *)bytes(numBytes);
    return (int64)(x & lowbytemask64LUT[numBytes]) / pow10dLUT[header & 0x0F] * signdLUT[header >> 7];
}


void DataReader::skipDecimal()
{
    SR_LOG_SKIP(Decimal);
    if (endOfContent()) {
        return;
    }

    unsigned header = getByte();

    switch (header) {
    case 0x1F: 
    case 0x2F:  
    case 0x3F: 
        return;
    case 0x0F:
        return skip(8);
    }

    return skip((header >> 4) & 0x7);
}


INLINE uint64 DataReaderInternal::getPackedUInt61()
{
    unsigned header  = getByte();
    uintptr numBytes = header >> 5;
    uint64     value = header & 0x1F;

    const uint64 x = *(uint64 *)bytes(numBytes);
    return value | ((x & lowbytemask64LUT[numBytes]) << 5);
}


// used by readBinary to get length
INLINE uint32 DataReaderInternal::getPackedUInt30()
{
    unsigned   header = getByte();
    uintptr  numBytes = header >> 6;
    unsigned    value = header & 0x3F;

    const uint32 x = *(uint32 *)bytes(numBytes);

    return value | ((x & lowbytemask32LUT[numBytes]) << 6);
}


// Used by array readers to get both array length and item count
INLINE uint32 DataReaderInternal::getPackedUInt22()
{
    // Main diference between this and UInt30 version is that this one encodes 7-bit uint as one byte, not 2,
    // this only leaves 2 possible extension lengths (1 and 2 extra bytes)

    unsigned header   = getByte();
    uintptr numBytes  = (header >> 6) - 1;
    if (0 == (header & 0x80)) {
        return header;
    }

    // At this point, header is either 10XXXXXX or 11XXXXXX (value of 2 bits = either 2 or 3)
    // so, numBytes = 1 or 2

    const uint32 x = *(uint32 *)bytes(numBytes); // We fetch 1 or 2 extra bytes
    return (header & 0x3F) | ((x & lowbytemask32LUT[numBytes]) << 6);
}


INLINE void DataReaderInternal::skipPackedUInt22()
{
    uintptr numBytes = (getByte() >> 6) - 1;
    DataReader::skip(numBytes);
}


INLINE uint32 DataReaderInternal::getInterval()
{
    unsigned    header = getByte();
    unsigned    x = header & 0x3F;

    enum Granularity {
        SECONDS = 0,        // Interval length is divisible by 1000
        MINUTES = 1 << 6,   // Interval length is divisible by 60 * 1000 = 60000
        HOURS   = 2 << 6,   // Interval length is divisible by 60 * 60 * 1000 = 3600000
        OTHER   = 3 << 6    // Arbitrary length
    };

    switch ((IntervalGranularity)((unsigned)header >> 6) /* scale */) {
    case IntervalGranularity::SECONDS:  return x * 1000U;
    case IntervalGranularity::MINUTES:  return x * 60000U;
    case IntervalGranularity::HOURS:    return x * 3600000U;
    case IntervalGranularity::OTHER:    return getUInt32();
    default:
        DBGLOG("getInterval() : This code should NEVER be called ");
        throw logic_error("getInterval() : This code should NEVER be called ");
    }
}


uint32 DataReader::readInterval()
{
    return endOfContent() ? INTERVAL_NULL : THIS_IMPL->getInterval();
}


#if 0
template <> INLINE int64 DataReaderImpl::getCompressedNanoTimeDividedBy<1>()
{
    byte    header = getByte();
    int64   lowBits = header & 0x1F;

    switch (header >> 5) {
    case TimestampGranularity::MILLIS:
        return (((_loadLE<uint64>(bytes(5))  & BYTEMASK_L(5)) << 5) + lowBits) * TIME_NANOS_PER_MILLISECOND + COMPRESSED_TIMESTAMP_BASE;
    case TimestampGranularity::SECONDS:
        return ((_loadLE<uint32>(bytes(4)) << 5) + lowBits) * TIME_NANOS_PER_SECOND + COMPRESSED_TIMESTAMP_BASE;
    case TimestampGranularity::TEN_SECONDS:
        return (((_loadLE<uint32>(bytes(3))  & BYTEMASK_L(3)) << 5) + lowBits) * (TIME_NANOS_PER_SECOND * 10) + COMPRESSED_TIMESTAMP_BASE;
    case TimestampGranularity::HOURS:
        return ((_loadLE<uint16>(bytes(2)) << 5) + lowBits) * TIME_NANOS_PER_HOUR + COMPRESSED_TIMESTAMP_BASE;
    case TimestampGranularity::NANOS:
        return (((_loadLE<uint64>(bytes(7)) & BYTEMASK_L(7)) << 5) + lowBits) + COMPRESSED_TIMESTAMP_BASE;
    case TimestampGranularity::LONG_NANOS:
        return ((_loadLE<uint64>(bytes(8)) << 5) + lowBits) + COMPRESSED_TIMESTAMP_BASE;

    case TimestampGranularity::SPECIAL:
        if (0 == lowBits) {
            return TIMESTAMP_NULL;
        }
        // fall through
    default:
    case TimestampGranularity::INVALID:
        THROW_DBGLOG("getCompressedTimestamp() : Invalid timestamp data");
    }
}
#endif




// Note: divisions for nanos don't look right, but are closest to what I see in Java implementation
template <int64 N> INLINE int64 DataReaderInternal::getCompressedNanoTimeDividedBy()
{
    byte    header = getByte();
    int64   lowBits = header & 0x1F;

    switch ((TimestampGranularity)(header >> 5) /* scale */) {
    case TimestampGranularity::MILLIS:
        return (((_loadLE<uint64>(bytes(5))  & BYTEMASK_L(5)) << 5) + lowBits) * (TIME_NANOS_PER_MILLISECOND / N) + COMPRESSED_TIMESTAMP_BASE_NS / N;

    case TimestampGranularity::SECONDS:
        return ((_loadLE<uint32>(bytes(4)) << 5) + lowBits) * (TIME_NANOS_PER_SECOND / N) + COMPRESSED_TIMESTAMP_BASE_NS / N;

    case TimestampGranularity::TEN_SECONDS:
        return (((_loadLE<uint32>(bytes(3))  & BYTEMASK_L(3)) << 5) + lowBits) * ((TIME_NANOS_PER_SECOND * 10) / N) + COMPRESSED_TIMESTAMP_BASE_NS / N;

    case TimestampGranularity::HOURS:
        return ((_loadLE<uint16>(bytes(2)) << 5) + lowBits) * (TIME_NANOS_PER_HOUR / N) + COMPRESSED_TIMESTAMP_BASE_NS / N;

    case TimestampGranularity::NANOS:
        return ((((_loadLE<uint64>(bytes(7)) & BYTEMASK_L(7)) << 5) + lowBits) + COMPRESSED_TIMESTAMP_BASE_MS) / N;

    case TimestampGranularity::LONG_NANOS:
        return (((_loadLE<uint64>(bytes(8)) << 5) + lowBits) + COMPRESSED_TIMESTAMP_BASE_MS ) / N;

    case TimestampGranularity::SPECIAL:
        if (0 == lowBits) {
            return TIMESTAMP_NULL;
        }
        // fall through
    default:
    case TimestampGranularity::INVALID:
        THROW_DBGLOG("getCompressedTimestamp() : Invalid timestamp data");
    }
}


NOINLINE SR_READ(int64, DateTimeNs, PTIME)
    return endOfContent() ? TIMESTAMP_NULL : THIS_IMPL->getCompressedNanoTimeDividedBy<1>();
}


NOINLINE SR_READ(int64, DateTimeMs, PTIME)
    return endOfContent() ? TIMESTAMP_NULL : THIS_IMPL->getCompressedNanoTimeDividedBy<TIME_NANOS_PER_MILLISECOND>();
}


NOINLINE SR_SKIP(DateTime, PTIME)
    unsigned header = getByte();

    skip(compressed_timestamp_size[header >> 5]);
    if ((unsigned)TimestampGranularity::INVALID == (header >> 5) ||
        ((unsigned)TimestampGranularity::SPECIAL == (header >> 5) && header != TIMESTAMP_HEADER(SPECIAL))
    ) {
        THROW_DBGLOG("getCompressedTimestamp() : Invalid timestamp data");
    }
}


NOINLINE SR_READ(double, Decimal, DECIMAL)
    return endOfContent() ? DECIMAL_NULL : THIS_IMPL->getDecimal();
}


NOINLINE SR_READ_NULLABLE(double, Decimal, DECIMAL)
    if (endOfContent()) {
        value = DECIMAL_NULL;
        return false;
    }
    value = THIS_IMPL->getDecimal();
    return !std::isnan((double)value);
}


void DataReaderInternal::getBytes(uint8_t *destinationBuffer, uintptr_t dataLength)
{
#if defined(SAFE_GETBYTES)
    while (0 != dataLength--) *destinationBuffer++ = getByte();
#else
    uintptr_t n;

    while (1) {
        n = nBytesAvailable();
        // Do we already have enough data in the buffer?
        if (n >= dataLength) {
            memcpy(destinationBuffer, dataPtr, dataLength);
            dataPtr += dataLength;
            return;
        }

        // n < dataLength
        memcpy(destinationBuffer, dataPtr, n);
        destinationBuffer += n;
        dataLength -= n;
        dataPtr += n;

        // Try to fetch at least one more byte and repeat
        peek(1);
    }


    // should later make different copy operation, using reads <= 8 bytes each? with duff device maybe?
    //memcpy(destinationBuffer, bytes(dataLength), dataLength);
#endif

}

template <typename T> void DataReaderInternal::getWords(T * destinationBuffer, uintptr_t n)
{
    THROW_DBGLOG("getWords not implemented!");
    // Not implemented.
}


NOINLINE SR_READ(uint32, PUInt30, PUINT30)
    if (endOfContent()) {
        return UINT30_NULL;
    }

    uint32 v = THIS_IMPL->getPackedUInt30();
    assert(v < (1 << 30));
    // return 0 == v ? UINT30_NULL : v - 1;
    static_assert(UINT30_NULL == INT32_MAX, "UINT30_NULL == INT32_MAX");
    // We assume a specific value of UINT30_NULL, this assert is to ensure that
    return INT32_MAX & (v - 1);
}


NOINLINE SR_READ(uint64, PUInt61, PUINT61)
    if (endOfContent()) {
        return UINT61_NULL;
    }

    uint64 v = THIS_IMPL->getPackedUInt61();
    assert(v < (UINT64_C(1) << 61));
    // return 0 == v ? UINT61_NULL : v - 1;

    assert(UINT61_NULL == INT64_MAX); // We assume a specific value of UINT61_NULL, this assert is to ensure that
    return INT64_MAX & (v - 1);
}


//void DataReader::getUTF8chars(std::string &s, uintptr_t dataLength)
//{
//    s.resize(dataLength);
//
//    for (uintptr_t i = 0; i != dataLength; ++i) {
//        s[i] = getByte();
//    }
//}


bool DataReaderInternal::getUTF8_inl(std::string &s)
{
    uint32_t len = getUInt16();

    // Check for NULL
    if (0xFFFF == len) {
        return false;
    }

    s.resize(len);  // NOTE: Memory allocation exception possible here
    getUTF8chars(&s[0], len);    
    return true;
}


bool DataReaderInternal::getUTF8(std::string &s)
{
    return getUTF8_inl(s);
}

bool DataReaderInternal::getAscii_inl(std::string &s)
{
    uint32_t len = getUInt16();

    // Check for NULL
    if (0xFFFF == len) {
        return false;
    }

    s.resize(len);  // NOTE: Memory allocation exception possible here
    getAsciichars(&s[0], len);
    return true;
}


bool DataReaderInternal::getAscii(std::string &s)
{
    return getAscii_inl(s);
}


NOINLINE SR_READ_NULLABLE(string, UTF8, STRING)
    return endOfContent() ? false : THIS_IMPL->getUTF8_inl(value);
}

NOINLINE SR_READ_NULLABLE(string, Ascii, STRING)
return endOfContent() ? false : THIS_IMPL->getAscii_inl(value);
}

NOINLINE SR_SKIP(Binary, BINARY)
    if (getByte() != 0) {
        return;
    }

    skip(THIS_IMPL->getPackedUInt30());
}


NOINLINE SR_READ_NULLABLE(std::vector<uint8_t>, Binary, BINARY)

    // Check for NULL
    if (getByte() != 0) {
        return false;
    }

    uintptr_t len = THIS_IMPL->getPackedUInt30();
    value.resize(len);  // NOTE: Memory allocation exception possible here
    THIS_IMPL->getBytes(&value[0], len);

    return true;
}


static const char * ct_names[2] = { "Object", "Array" };
static const char depthstr[READER_MAX_NESTED_OBJECTS + 1] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

NOINLINE static void depthOverflowError(bool isArray)
{
    THROW_DBGLOG("DataReader.Open%s(): nesting level can't be more than %d!", ct_names[isArray], (int)READER_MAX_NESTED_OBJECTS);
}


NOINLINE static void depthUnderflowError(bool isArray)
{
    THROW_DBGLOG("DataReader.Close%s(): nesting depth underflow. depth of 0 is unexpected!", ct_names[isArray]);
}


NOINLINE static void wrongTypeError(bool isArray)
{
    THROW_DBGLOG("DataReader.Close%s(): Container type mismatch. Close%s() is expected!", ct_names[isArray], ct_names[isArray ^ 1]);
}


INLINE bool DataReaderImpl::openContainer(bool isArray)
{
    // numBytes == 0 means NULL  deltix/qsrv/hf/pub/codec/intp/ArrayFieldDecoder.java:20
    // deltix/qsrv/hf/pub/codec/intp/ClassFieldDecoder.java:96
    uintptr size = getPackedUInt22();
    uintptr remaining = contentEnd - dataPtr;
    assert(size < MAX_MESSAGE_SIZE);
    assert(remaining < MAX_MESSAGE_SIZE);

    if (0 == size) {
        SR_LOG("open%s() = NULL", ct_names[isArray]);
        return false;
    }

    //printf("numBytes=%u\n", size);

    uintptr depth = this->depth;

    if (depth >= READER_MAX_NESTED_OBJECTS) {
        depthOverflowError(isArray);
    }

    SR_LOG("%sopen%s() @ depth %d", &depthstr[READER_MAX_NESTED_OBJECTS - (depth + 1)], ct_names[isArray], depth + 1);
    this->depth = depth + 1;
    size = size <= remaining ? size : remaining;
    containerIsArray[depth] = isArray;
    auto e = contentEnd -= (eOfs[depth] = remaining - size);
    dataEnd = std::min(e, dataEnd0);
    
    return true;
}


INLINE void DataReaderImpl::closeContainer(bool isArray)
{
    uintptr_t depth = this->depth;
    if (0 == depth) {
        depthUnderflowError(isArray);
    }

    SR_LOG("%sclose%s() @ depth %d", &depthstr[READER_MAX_NESTED_OBJECTS - depth], ct_names[isArray], depth);
    this->depth = --depth;

    if (containerIsArray[depth] != (byte)isArray) {
        wrongTypeError(isArray);
    }

    assert(dataPtr <= contentEnd);
    DataReader::skip(contentEnd - dataPtr); // TODO: minor optimization opportunity. Currently w edon't cache contentend, because skip() may change it
    auto e = contentEnd += eOfs[depth];
    dataEnd = std::min(e, dataEnd0);
}


INLINE void DataReaderImpl::skipContainer(bool isArray)
{
    uintptr size = getPackedUInt22();
    uintptr remaining = contentEnd - dataPtr;
    assert(size < MAX_MESSAGE_SIZE);
    assert(remaining < MAX_MESSAGE_SIZE);

    // we can have incomplete message, skip minimum of 2 sizes (remaining object/message size and the size of this object)
    // deltix/qsrv/hf/pub/codec/intp/ClassFieldDecoder.java:55
    // deltix/qsrv/hf/pub/codec/intp/ArrayFieldDecoder.java:68
    DataReader::skip(size <= remaining ? size : remaining);
}


NOINLINE SR_READ(int32_t, ArrayStart, ARRAY)
    if (endOfContent()) {
        return INT32_NULL;
    }

    if (!THIS_IMPL->openContainer(true)) {
        return INT32_NULL;
    }
    
    unsigned numItems = THIS_IMPL->getPackedUInt22();
    return (int32_t)numItems;
}


int32_t DataReader::readObjectStart()
{
    SR_LOG_READ(bool, Object);
    if (endOfContent()) {
        return INT32_NULL;
    }

    if (!THIS_IMPL->openContainer(false)) {
        return INT32_NULL;
    }

    // We are ignoring length if the object is not null and returning object type index
    return THIS_IMPL->getByte();
}


void DataReader::readObjectEnd()
{
    THIS_IMPL->closeContainer(false);
}


void DataReader::readArrayEnd()
{
    THIS_IMPL->closeContainer(true);
}


NOINLINE SR_SKIP(Object, OBJECT)
    THIS_IMPL->skipContainer(false);
}


NOINLINE SR_SKIP(Array, ARRAY)
    THIS_IMPL->skipContainer(true);
}

/**
* Input Stream data reading
*/


INLINE void DataReaderBaseImpl::skip_inl(const byte * from, uintptr_t size)
{
    assert(from + size > dataEnd);
    dataPtr = getFromStream<true>(from, size) + size;
    dataEnd = streamDataEnd;
}


INLINE const byte * DataReaderBaseImpl::get_inl(const byte * from, uintptr size)
{
    assert(from + size > dataEnd);
    const byte * p = getFromStream<false>(from, size);
    dataEnd = streamDataEnd;
    return p;
}


void DataReaderBaseImpl::skip(const byte * from, uintptr_t size)
{
#if SR_VERIFY == 2
    DBGLOG("Skipped: %llu bytes", (ulonglong)size);
#endif
    return skip_inl(from, size);
}


const byte * DataReaderBaseImpl::get(const byte * from, uintptr size)
{
    return get_inl(from, size);
}


const byte * DataReaderMsg::get(const byte * pData, uintptr size)
{
    if (pData + size <= contentEnd) {
        const byte * pDataNew = DataReaderBaseImpl::get_inl(pData, size);
        contentEnd += pDataNew - pData;
        dataEnd0 = dataEnd;
        dataEnd = std::min(contentEnd, dataEnd);
        return pDataNew;
    }
    else {
        THROW_DBGLOG("Read beyond end of a content block by %llu bytes", (ulonglong)(pData + size - contentEnd));
    }
}


void DataReaderMsg::skip(const byte * pData, uintptr size)
{
    if (pData + size <= contentEnd) {
        intptr remaining = contentEnd - pData - size;
        DataReaderBaseImpl::skip_inl(pData, size);
        contentEnd = dataPtr + remaining;
        dataEnd0 = dataEnd;
        dataEnd = std::min(contentEnd, dataEnd);
    }
    else {
        // TODO: Switch to error callbacks
        THROW_DBGLOG("Attempt to skip data beyond the end of a content block by %llu bytes", (ulonglong)(pData + size - contentEnd));
    }
}


void DataReaderBaseImpl::skipUnreadMessageBody()
{
    if (0 != depth) {
        // Unwind stack to restore message end pointer
        do {
            assert(depth < READER_MAX_NESTED_OBJECTS + 1);
            assert(eOfs[depth] < MAX_MESSAGE_SIZE);
            contentEnd += eOfs[depth];
        } while (--depth);

        dx_assert(false, "skipUnreadMessageBody() depth=%llu,  should be 0.", (ulonglong)depth);
    }

    uintptr n = contentEnd - dataPtr;
    resetMessageSize();
    DataReader::skip(n);
}


NOINLINE const uint8_t * DataReader::onReadBarrier(const uint8_t * from, uintptr_t size)
{
    from = get(const_cast<uint8_t *>(from), size);
    dataPtr = from + size;
    return from;
}


NOINLINE void DataReader::onSkipBarrier(const uint8_t * from, uintptr_t size)
{
    // Skip implementation always updates dataPtr itself
    skip(const_cast<uint8_t *>(from), size);
}


NOINLINE const uint8_t * DataReaderBaseImpl::onPeekBarrier(const uint8_t * from, size_t size)
{
    // Same as read barrier, except we don't increment dataPtr
    return dataPtr = get(const_cast<uint8_t *>(from), size);
}


NOINLINE void DataReader::onReadTypeMismatch(uint8_t wrong)
{
    DBGLOG("Type mismatch - trying to read %s instead of %s",
        Schema::FieldType::fromInt(wrong).toString(),
        Schema::FieldType::fromInt(as<uint8_t>(schemaPtr[-1])).toString()
        );
}


NOINLINE void DataReader::onSkipTypeMismatch(uint8_t wrong)
{
    DBGLOG("Type mismatch - trying to skip %s instead of %s",
        Schema::FieldType::fromInt(wrong).toString(),
        Schema::FieldType::fromInt(as<uint8_t>(schemaPtr[-1])).toString()
        );
}
