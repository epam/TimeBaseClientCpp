#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <math.h>

#include "data_writer_impl.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;


#define LOGHDR "%s"
#define ID ""

#define THIS_IMPL impl(this)
#define LOADER_OBJ_EXPAND_HEADERS_IMPL expandHeadersImplBasic

#define DW_LOG (void)
//#define DW_LOG DBGLOG

static const double pow10dLUT[16] = {
    1.E0, 1.E1, 1.E2, 1.E3, 1.E4, 1.E5, 1.E6, 1.E7, 1.E8, 1.E9, 1.E10, 1.E11, 1.E12, 1.E13, 1.E14, 1.E15
};
static const uint64 lowbytemask64LUT[9] = { 0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF, 0xFFFFFFFFFF, 0xFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF };
static const uint32 lowbytemask32LUT[5] = { 0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF };
static const double signdLUT[2] = { 1.0, -1.0 };


template<typename T> static INLINE unsigned packedUInt22EncodedSize(T x)
{
    static_assert(sizeof(x) >= 4, "Type encoded to PUInt22 must be at least 32-bit");
    assert(x >= 0);
    assert(x < 0x400000);
    return (x < 0x80) ? 1 : (x < 0x4000) ? 2 : 3;
}


// Does not write NULL string
void DataWriter::writeUTF8(const char * data, uintptr_t length)
{
    assert(length == 0 || NULL != data);
    if (length >= 0xFFFF) {
        THROW_DBGLOG("putUTF8(char *): UTF8 string length is too big: %llu", (ulonglong)length);
    }

    putUInt16((uint16)length);
    if (0 != length) {
        putBytes((const byte *)data, length);
    }
}


// Does not write NULL string
void DataWriter::writeUTF8(const wchar_t * data, uintptr length)
{
    assert(length == 0 || NULL != data);
    if (length >= 0xFFFF) {
        THROW_DBGLOG("putUTF8(wchar_t *): UTF8 string length is too big: %llu", (ulonglong)length);
    }

    byte * header = dataPtr;
    putUInt16(0);
    if (0 == length) {
        setNotNull();
        return;
    }

    data += length;
    intptr i = -(intptr)length;
    unsigned c, cc;

    // TODO: Possible perf. improvement: estimate max. length and preallocate bytes

    do {
        c = data[i];
        if (c < 0x80) {
            putByte((byte)c);
            continue;
        }

        if (c < 0x800) {
            putByte((byte)(0xC0 | ((c >> 6) & 0x1F)));
            putByte((byte)(0x80 | (c & 0x3F)));
            continue;
        }

        if (0xD800 != (c & 0xF800)) {
            putByte((byte)(0xE0 | ((c >> 12) & 0x0F)));
            putByte((byte)(0x80 | ((c >> 6) & 0x3F)));
            putByte((byte)(0x80 | (c & 0x3F)));
            continue;
        }

        if (0x400 & c) {
            THROW_DBGLOG("putUTF8(wchar *): UTF-16 High surrogate pair encountered before low");
        }

        if (0 == ++i) {
            THROW_DBGLOG("putUTF8(wchar *): String ends on UTF-16 High surrogate");
        }

        cc = data[i];
        c = (c << 10) + (0x10000U - (0xD800 << 10) - 0xDC00);
        if (0xDC00 != (cc & 0xFC00)) {
            THROW_DBGLOG("putUTF8(wchar *): UTF-16 High surrogate is not followed by Low surrogate");
        }

        c += cc;

        // Encode 20-bit codepoint (which is guaranteed to be > 0xFFFF)
        putByte((byte)(0xF0 | ((c >> 18) & 0x07)));
        putByte((byte)(0x80 | ((c >> 12) & 0x3F)));
        putByte((byte)(0x80 | ((c >> 6) & 0x3F)));
        putByte((byte)(0x80 | (c & 0x3F)));

    } while (0 != ++i);

    length = (dataPtr - header - 2);
    if (length >= 0xFFFF) {
        THROW_DBGLOG("putUTF8(wchar *): after conversion to UTF8, length >= 0xFFFF: %llu", (ulonglong)length);
    }

    _storeBE<uint16>(header, (uint16)length);
    setNotNull();
}


// Does not write NULL string
void DataWriter::writeAscii(const char * data, uintptr_t length)
{
    assert(length == 0 || NULL != data);
    if (length >= 0xFFFF) {
        THROW_DBGLOG("putAscii(char *): UTF8 string length is too big: %llu", (ulonglong)length);
    }

    putUInt16((uint16)length);
    if (0 != length) {
        putBytes((const byte *)data, length);
    }
}


// Does not write NULL string
void DataWriter::writeAscii(const wchar_t * data, uintptr length)
{
    assert(length == 0 || NULL != data);
    if (length >= 0xFFFF) {
        THROW_DBGLOG("putAscii(wchar_t *): UTF8 string length is too big: %llu", (ulonglong)length);
    }

    putUInt16((uint16)length);
    if (0 != length) {
        for (int i = 0; i < length; ++i) {
            putByte((byte) data[i]);
        }
    }
}

INLINE void DataWriterInternal::putInterval(uint32 value)
{
    if (INTERVAL_NULL == value) {
        putByte(INTERVAL_NULL);
        return;
    }

    if (value > 0x7FFFFFFF) {
        THROW_DBGLOG("putInterval() : Intervals greater then 0x7FFFFFFF are not supported. value = %u", value);
    }

    /*
    enum Granularity {
        SECONDS = 0,        // Interval length is divisible by 1000
        MINUTES = 1 << 6,   // Interval length is divisible by 60 * 1000 = 60000
        HOURS   = 2 << 6,   // Interval length is divisible by 60 * 60 * 1000 = 3600000
        OTHER   = 3 << 6    // Arbitrary length
    };
    */

    unsigned v, scale;

    if (0 == value % 60000) {  // 2**5 * 3 * 5**4
        if (0 == value % 3600000) { // 2**7 * 3**2 * 5**5
            v = value / 3600000;
            scale = INTERVAL_HEADER(HOURS);
        }
        else {
            v = value / 60000;
            scale = INTERVAL_HEADER(MINUTES);
        }
    }
    else if (0 == value % 1000) { // 2**3 * 5**3
        v = value / 1000;
        scale = INTERVAL_HEADER(SECONDS);
    }
    else {
        putByte(INTERVAL_HEADER(OTHER));
        putUInt32(value);
        setNotNull();
        return;
    }

    // It is impossible for v to be == 0 at this point
    assert(v > 0);

    // Fits into 6 bits?
    if (v < (1 << 6)) {
        putByte((byte)v);
    }
    else {
        putByte(INTERVAL_HEADER(OTHER));
        putUInt32(value);
    }

    setNotNull();
}


/*
// Replaced with template, see below
void DataWriterInternal::writeUInt30(uint32_t value)
{
assert(value < (1 << 30));

// Write compressed
byte * headerPtr = dataPtr;
putByte(0);
unsigned low6bits = value & 0x3F;

for (value >>= 6; value != 0; value >>= 8) {
putByte((byte)value);
}

uintptr numBytes = dataPtr - headerPtr - 1;
_storeBE(headerPtr, (byte)(low6bits | (numBytes << 6)));
}
*/


#if defined(DX_PLATFORM_64)
#define WRITE_VLI_VER 3
#else
#define WRITE_VLI_VER 2
#endif



#if WRITE_VLI_VER==1
// Returns number of nonzero bytes written (0..sizeof(T))
// "Classic" version
template <typename T> INLINE size_t DataWriterInternal::writeVariableLengthInt(byte *&headerPtr, T value)
{
    // This is 1st implementation, can be improved
    static_assert((T)-1 > (T)0, "Type must be unsigned");
    byte *wp = headerPtr = dataPtr;
    *wp = 0;
    for (; value != 0; value >>= 8) {
        *++wp = (byte)value;
    }

    if ((dataPtr = wp + 1) > dataEnd) {
        onOverflow();
    }

    return wp - headerPtr;
}

#elif WRITE_VLI_VER==2
// Returns number of nonzero bytes written (0..sizeof(T))
// Unaligned write version
template <typename T> INLINE size_t DataWriterInternal::writeVariableLengthInt(byte *&headerPtr, T value)
{
    static_assert((T)-1 > (T)0, "Type must be unsigned");
    headerPtr = dataPtr;
    *headerPtr = 0;
    _storeLE<T>(headerPtr + 1, value);
    
    size_t length = 0;
    while (0 != value) {
        value >>= 8;
        ++length;
    }

    if ((dataPtr = headerPtr + length + 1) > dataEnd) {
        onOverflow();
    }

    return length;
}
#elif WRITE_VLI_VER==3

// Returns number of nonzero bytes written (0..sizeof(T))
// Branchless version
template <typename T> INLINE size_t DataWriterInternal::writeVariableLengthInt(byte *&headerPtr, T value)
{
    typedef make_unsigned<T>::type U_T;
    headerPtr = dataPtr;
    *headerPtr = 0;
    _storeLE<U_T>(headerPtr + 1, value);

    size_t length = (_bsr((U_T)value) + 7) >> 3;

    if ((dataPtr = headerPtr + length + 1) > dataEnd) {
        onOverflow();
    }

    return length;
}
#else
#error define WRITE_VLI_VER properly, please
#endif


template <typename T> INLINE void DataWriterInternal::writePackedUInt(T value)
{
    assert(8 != sizeof(T) || value < (UINT64_C(1) << 61));
    assert(4 != sizeof(T) || value < (UINT32_C(1) << 30));
    static_assert((T)-1 > (T)0, "Type must be unsigned");

    // Write compressed
    // Not the best implementation, but small optimizations will be made only after benchmarks
    byte * headerPtr;
    
    unsigned lowBits = value & (0x100 / sizeof(T) - 1);

    // Remove lower bits encoded in the header (5 for uint64, 6 for uint32)
    value /= 0x100 / sizeof(T);

    //for (; value != 0; value >>= 8) {
        //putByte((byte)value);
    //}

    //uintptr numBytes = dataPtr - headerPtr - 1;

    uintptr numBytes = writeVariableLengthInt(headerPtr, value);
    _storeBE(headerPtr, (byte)(lowBits | (numBytes * (0x100 / sizeof(T)))));
}


INLINE void DataWriterInternal::writePackedUInt22(unsigned value)
{
    assert(value <= (1 << 22));

    if (value < 0x80) {
        putBE<uint8_t>(value);
        return;
    }

    byte * headerPtr;
    
    unsigned lowBits = value & 0x3F;

    uintptr numBytes = writeVariableLengthInt(headerPtr, value >> 6);
    assert(1 == numBytes || 2 == numBytes);
    _storeBE(headerPtr, (byte)(lowBits + 0x40 + (numBytes << 6)));
}


INLINE bool DataWriterInternal::writeSpecialDecimal(double value)
{
    if (value == 0) {
        putByte(0);
        setNotNull();
        return true;
    }
    else if (value != value) /* NaN */ {
        putByte(0x1F);
        return true;
    }
    else if (value == FLOAT64_NEGATIVE_INFINITY) {
        putByte(0x2F);
        setNotNull();
        return true;
    }
    else if (value == FLOAT64_POSITIVE_INFINITY) {
        putByte(0x3F);
        setNotNull();
        return true;
    }
    else
        return false;
}


INLINE void DataWriterInternal::putDecimal(double value)
{
    if (writeSpecialDecimal(value))
        return;

    unsigned isNegative = value < 0;
    double x = value * signdLUT[isNegative];
    unsigned exp = 0;
    int64_t lv;

    for (;;) {
        double scale = pow10dLUT[exp];
        lv = (int64_t)(x * scale);

        // If mantissa is bigger than 7 bytes, write uncompressed
        if ((lv & INT64_C(0xFF00000000000000)) != 0) {
            break;
        }

        if (x == lv / scale || x == (lv + 1) / scale) {
            lv = (int64_t)floor(x * scale + 0.5);
            if (lv / scale == x) {
                // Write compressed
                byte * headerPtr;
                uintptr numBytes = writeVariableLengthInt(headerPtr, (uint64)lv);
                assert(numBytes != 0);
                //putByte(0);

                //for (; lv != 0; lv = (uint64)lv >> 8) {
                    //putByte((byte)lv);
                //}

                isNegative <<= 7;
                //uintptr numBytes = dataPtr - headerPtr - 1;
                _storeBE(headerPtr, (byte)(isNegative + (numBytes << 4) + exp));
                setNotNull();
                return;
            }
        }

        exp++;

        if (exp == 15) {
            break;
        }
    }

    // Write uncompressed
    putByte(0x0F);
    putFloat64(value);

    setNotNull();
}


INLINE void DataWriterInternal::putDecimal(double value, unsigned precision)
{
    if (writeSpecialDecimal(value))
        return;

    unsigned isNegative = value < 0;
    double x = value * signdLUT[isNegative];
    unsigned exp = precision;
    uint64 lv;

    double scale = pow10dLUT[exp];
    lv = (uint64)floor(x * scale + 0.5); // Overflow check???

    while (exp > 1) { // TODO: != 0 ???
        if ((lv % 10U) != 0)
            break;

        lv = lv / 10U;
        exp--;
    }

    // If mantissa is bigger than 7 bytes, write uncompressed
    if ((lv & UINT64_C(0xFF00000000000000)) != 0) {
        // Write uncompressed
        putByte(0x0F);
        putFloat64(value);
    }

    // Write compressed
    /*byte * headerPtr = dataPtr;
    putByte(0);

    for (; lv != 0; lv >>= 8) {
        putByte((byte)lv);
    }*/
    byte * headerPtr;
    uintptr numBytes = writeVariableLengthInt(headerPtr, lv);
    assert(numBytes != 0);

    isNegative <<= 7;
    //uintptr numBytes = dataPtr - headerPtr - 1;
    _storeBE(headerPtr, (byte)(isNegative + (numBytes << 4) + exp));

    setNotNull();
}


void DataWriter::writeDecimal(double value, unsigned precision)
{
    THIS_IMPL->putDecimal(value, precision);
}


void DataWriter::writeDecimal(double value) {
    THIS_IMPL->putDecimal(value);
}


void DataWriter::writeInterval(uint32 value)
{
    THIS_IMPL->putInterval(value);
}


// This code is not 32-bit-friendly and should be modified if good performance is desired for 32-bit architecture

#define CHECK_TS(NUMBYTES)  do { if (q >= (UINT64_C(1) << ((NUMBYTES) * 8 - 3))) { \
    THROW_DBGLOG("Time too big for " #NUMBYTES " byte encoding: %s", toString(q).c_str()); \
} } while (0)

#define PACK_TIME(SCALE, NUMBYTES) do {CHECK_TS(NUMBYTES); \
    putLE((q & 0x1F) + ((q ^ (q & 0x1F)) << 3) + TIMESTAMP_HEADER(SCALE), NUMBYTES); \
} while (0)


INLINE void DataWriterInternal::putCompressedMillisecondTime(int64 value)
{
    uint64 t = (uint64)(value - COMPRESSED_TIMESTAMP_BASE_MS);
    if ((int64_t)t < 0) {
        THROW_DBGLOG("Time too small: %lld", (long long)value);
    }

    // Binary decision tree for determining the scale
    if (t % 10000 == 0) {
        if (t % 3600000 == 0) {
            uint64 q = t / 3600000; // TODO: 32 is enough here, but should be careful about range check
            //CHECK_TS(21);
            //putLE((q & 0x1F) + ((q - (q & 0x1F)) << 3) + TIMESTAMP_HEADER(HOURS), 3);
            PACK_TIME(HOURS, 3);
        }
        else {
            uint64 q = t / 10000;
            PACK_TIME(TEN_SECONDS, 4);
        }
    }
    else {
        if (t % 1000 == 0) {
            uint64 q = t / 1000;
            PACK_TIME(SECONDS, 5);
        }
        else {
            uint64 q = t;
            PACK_TIME(MILLIS, 6);
        }
    }
}


INLINE void DataWriterInternal::putCompressedNanosecondTime(int64 value)
{
    value -= COMPRESSED_TIMESTAMP_BASE_MS; // Yes, in milliseconds. See Java implementation (writeNanos() in TimeCodec)

    if (value < 0) {
        THROW_DBGLOG("Time too small: %lld", value + COMPRESSED_TIMESTAMP_BASE_MS);
    }

    // TODO: write milliseconds if divisible by 1M

    bool isLong = (uint64)value >= (UINT64_C(1) << (64 - 3));
    putByte((isLong ? TIMESTAMP_HEADER(LONG_NANOS) : TIMESTAMP_HEADER(NANOS)) | ((uint64)value & 0x1F));
        // Another variant
        // assert((int)TimestampGranularity::NANOS + 1 == (int)TimestampGranularity::LONG_NANOS);
        // putByte((isLong + (unsigned)TimestampGranularity::NANOS) << 5 | ((uint64)value & 0x1F));
    putLE<uint64>((uint64)value >> 5, (uintptr)7 /* bytes */ + isLong);
}


void DataWriter::writeCompressedNanoTime(int64 value)
{
    if (TIMESTAMP_NULL == value) {
        // NULL value
        putByte(TIMESTAMP_HEADER(SPECIAL));
    }
    
    //else if (0 == value % TIME_NANOS_PER_MILLISECOND) {
    else if (fast_divisible_by(value, TIME_NANOS_PER_MILLISECOND)) {
        THIS_IMPL->putCompressedMillisecondTime(fast_divide_by(value, TIME_NANOS_PER_MILLISECOND));
    }
    else {
        THIS_IMPL->putCompressedNanosecondTime(value);
    }

    setNotNull();
}


void DataWriter::writeCompressedTime(int64 value)
{
    if (TIMESTAMP_NULL == value) {
        putByte(TIMESTAMP_HEADER(SPECIAL));
    }
    else {
        THIS_IMPL->putCompressedMillisecondTime(value);
        setNotNull();
    }
}


void DataWriter::writeAlphanumericNull(uint32 fieldSize)
{
    return THIS_IMPL->putAlphanumericNull(fieldSize);
}


template<typename T, typename CHARTYPE> INLINE void DataWriterInternal::putAlphanumeric(uint32 fieldSize, const CHARTYPE *str, size_t stringLength)
{
    if (stringLength > fieldSize) {
        THROW_DBGLOG("Alphanumeric field is too long");
    }

    size_t nSizeBits = _bsr(++fieldSize);
    if (NULL == str) {
        putBE<uint32>(fieldSize << (32 - nSizeBits), (nSizeBits + 7) >> 3);
        return;
    }

    if (stringLength > fieldSize) {
        THROW_DBGLOG("writeAlphanumeric(): String is too long!");
    }

    if (0 == stringLength) {
        putLE<uint32>(0, (nSizeBits + 7) >> 3);
        setNotNull();
        return;
    }

    assert(nSizeBits + 6 <= BITSIZEOF(T));
    // TODO: ensure T is unsigned
    T accumulator = stringLength;// << 6;
    int nBitsFree = (int)(BITSIZEOF(T) - nSizeBits);

    intptr nCharsRemaining = -(intptr)stringLength;
    str += stringLength;
    
    typedef typename make_unsigned<CHARTYPE>::type UCHARTYPE;

    do {
        unsigned ch;
        // Reserve space in the accumulator (6 more bits)
        if ((nBitsFree -= 6) < 0) {
            // We should never get here if the whole value fits into T (10 chars if T=uint64)
            // Write sizeof(T) - 1 bytes
#if 0
            // Version 1
            // we store (sizeof(T) - 1) completely filled bytes and adjust the number of free bits accordingly
            putBE<T>(accumulator << (nBitsFree + 6), sizeof(T) - 1);
            nBitsFree += (sizeof(T) - 1) * 8;
#else
            // Version 2
            putBE<T>((accumulator << (nBitsFree + 6)) | (ch >> -nBitsFree), sizeof(T));
            nBitsFree += BITSIZEOF(T); // possible values:
            accumulator = 0; // Redundant, actually 
#endif
        }

        ch = (UCHARTYPE)str[nCharsRemaining] - (unsigned)' ';
        if (ch >= 0x40 /*ch & -0x40 */) {
            THROW_DBGLOG("writeAlphanumeric(): Invalid character encountered: 0x%02x = '%c'", (unsigned)ch, (unsigned)ch);
        }

        accumulator = (accumulator << 6) | ch;
    } while (0 != ++nCharsRemaining);

    assert(nBitsFree < BITSIZEOF(T)); // At least 1 bit must be used
    // Finally write the number of bytes necessary to hold these remaining bits
    putBE<T>(accumulator << nBitsFree, (BITSIZEOF(T) + 7 - nBitsFree) >> 3);
    setNotNull();
}


void DataWriter::writeAlphanumeric(uint32 fieldSize, const char *str, size_t stringLength)
{
    THIS_IMPL->putAlphanumeric<uint64, byte>(fieldSize, (const byte *)str, stringLength);
}


void DataWriter::writeAlphanumeric(uint32 fieldSize, const wchar_t *str, size_t stringLength)
{
    THIS_IMPL->putAlphanumeric<uint64, wchar_t>(fieldSize, str, stringLength);
}


template <> void DataWriter::writeAlphanumericInt<int32>(uint32_t fieldSize, int32 value)
{
    uintptr nSizeBits = _bsr(++fieldSize);
    if (INT32_NULL == value) {
        putBE<uint32>(fieldSize << (32 - nSizeBits), (nSizeBits + 7) >> 3);
        return;
    }

    uintptr len = (uint32)value >> (32 - nSizeBits);
    // TODO: may need verification (Java code doesn't have it)?

    putBE<uint32>(value, (nSizeBits + 6 * len + 7) >> 3);
    setNotNull();
}


template <> void DataWriter::writeAlphanumericInt<int64>(uint32 fieldSize, int64 value)
{
    uintptr nSizeBits = _bsr(++fieldSize);
    if (INT64_NULL == value) {
        putBE<uint32>(fieldSize << (32 - nSizeBits), (nSizeBits + 7) >> 3);
        return;
    }

    uintptr len = (uintptr)((uint64)value >> (64 - nSizeBits));
    // TODO: may need verification (Java code doesn't have it)?

    putBE<uint64>(value, (nSizeBits + 6 * len + 7) >> 3);
    setNotNull();
}

//template <typename T> void DataWriter::writeAlphanumericInt<uint32_t>(uint32_t fieldSize, uint32_t value);
//template <typename T> void DataWriter::writeAlphanumericInt<uint64_t>(uint32_t fieldSize, uint64_t value);

// TODO: these 2 can be optimized via wrapping to 0

void DataWriter::writePUInt30(uint32_t value)
{
    if (UINT30_NULL == value) {
        putByte(0x00); // Hardcoded compressed value of 0
    }
    else {
        if (value > ((1 << 30) - 2)) {
            THROW_DBGLOG("putUInt30() : Number is too big; value = %u", value);
        }
        THIS_IMPL->writePackedUInt<uint32>(value + 1);
        setNotNull();
    }
}


void DataWriter::writePUInt61(uint64_t value)
{
    if (UINT61_NULL == value) {
        putByte(0x00); // Hardcoded compressed value of 0
    }
    else {
        if (value > ((UINT64_C(1) << 61) - 2)) {
            THROW_DBGLOG("putUInt61() : Number is too big; value = %llu", (ulonglong)value);
        }
        THIS_IMPL->writePackedUInt<uint64>(value + 1);
        setNotNull();
    }
}





#if 0
void DataWriterInternal::writeAlphanumeric(const char *str, uintptr stringLength, uintptr fieldSize)
{
    assert(fieldSize < 0xFFFF);
    unsigned sizeBits = bsr32((unsigned)fieldSize + 2);
    unsigned numBits = sizeBits + 6 * (unsigned)fieldSize;
    unsigned numBytes = (numBits + 7) >> 3;
    unsigned len;
    bool isNull = NULL == str;
    if (isNull) {
        len = (unsigned)(fieldSize + 1);
    } else {
        if (stringLength > fieldSize) {
            throw std::invalid_argument(string("Alphanumeric value should fit into ").append(::toString(fieldSize)).append(" characters, got ").append(::toString(stringLength)));
        }
    }

    int32_t bitIdx = 0;
    byte v;
    if (sizeBits < 8) {
        v = (len << (8 - sizeBits));
        bitIdx = sizeBits;
    }
    else {
        putByte(len >> (sizeBits - 8));
        v = ((len & 0xff) << (16 - sizeBits));
        bitIdx = sizeBits - 8;
    }
    if (isNull || 0 == len) {
        putByte(v);
        return;
    }

    int8_t v1;
    int charIdx = 0;
    while ((unsigned)charIdx < len) {
        byte ch = str[charIdx];
        if (ch < 0x20 || ch > 0x5F) { // CHAR_MIN, CHAR_MAX
            throw std::invalid_argument(string("Character #").append(toString(ch)).append(" is not in allowed range. String: ").append(string(str, stringLength)));
        }

        unsigned chVal = ch - 0x20; // CHAR_MIN
        int sizeValue = 6;
        if (8 - bitIdx < sizeValue) {
            // split
            // size  - step (8 - bitIdx)
            v |= (int8_t)((uint32_t)chVal >> (sizeValue - (8 - bitIdx))); // shift left to extract upper bits
            // 8 - (size  - step)
            v1 = (int8_t)((chVal << (16 - sizeValue - bitIdx)) & 0xff); // shift right to extract lower bits
        }
        else {
            // append
            v |= (int8_t)(chVal << (8 - sizeValue - bitIdx));
            v1 = 0;
        }
        bitIdx += sizeValue;
        charIdx++;


        if (bitIdx >= 8) {
            putByte(v);
            bitIdx -= 8;
            v = v1;
        }

        if (charIdx == len && v != 0) {
            putByte(v);
            bitIdx = 0;
        }
    }
    if (bitIdx > 0)
        putByte(v);
}
#endif


#if 0
template<int NBITS> INLINE void DataWriterInternal::writeAlphanumeric(const char *str, uintptr stringLength, uintptr fieldSize)
{
    assert(fieldSize < 0xFFFF);
    unsigned sizeBits = bsr32((unsigned)fieldSize + 1);
    unsigned numBits = sizeBits + 6 * (unsigned)fieldSize;
    unsigned numBytes = (numBits + 7) >> 3;
    unsigned len;
    bool isNull = NULL == str;
    if (isNull) {
        len = (unsigned)(fieldSize + 1);
    }
    else {
        if (stringLength > fieldSize) {
            throw std::invalid_argument(string("Alphanumeric value should fit into ").append(::toString(fieldSize)).append(" characters, got ").append(::toString(stringLength)));
        }
    }

    register unsigned accumulator, bitIndex, ch;

    accumulator = (accumulator << 6) | ((ch - ' ') & 0x3F);
    if (bitIndex >= 8) {
        putByte((byte)(accumulator >> (bitIndex - 8)));
        bitIndex -= 8;
        accumulator >>= 8;
    }

    // TODO: unfinished
}
#endif

/**
 * BinaryArray
 */

// Internal method for writing binary array header
byte* DataWriterInternal::writeBinaryArrayEx(size_t size)
{
    putByte(0);
    writePackedUInt<uint32_t>((uint32_t)size);

    static_assert((2 << 22) >= MAX_MESSAGE_SIZE, "following check assumes max binaryarray size >= max message size");
    byte *pStart = dataPtr;
    if ((dataPtr = pStart + size) > dataEnd) {
        if (size >= (2 << 22)) {
            THROW_DBGLOG("Binary array has illegal size: %u bytes", size);
        }
        else {
            THROW_DBGLOG("No space remaining in the message buffer to write binary array with size: %u bytes, total size: %u", size, (unsigned)(dataPtr - bufferStart));
        }
    }

    return pStart;
}


void DataWriter::writeBinaryArray(const uint8_t *data, size_t size)
{
    byte *w = THIS_IMPL->writeBinaryArrayEx(size);
    memcpy(w, data, size);
}


/**
 * Objects and arrays
 */

const char *ct_names[2] = { "Object", "Array" };
static const char depthstr[LOADER_MAX_NESTED_OBJECTS + 1] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

NOINLINE static void depthOverflowError(bool isArray)
{
    THROW_DBGLOG("Write%sStart(): nesting level can't be more than %d!", ct_names[isArray], (int)LOADER_MAX_NESTED_OBJECTS);
}

NOINLINE void DataWriterInternal::depth0ExpectedError()
{
    THROW_DBGLOG(LOGHDR ".finalizeMessage(): not finished writing nested objects. remaining depth = %llu", ID, (ulonglong)depth);
}

NOINLINE static void depthUnderflowError(bool isArray)
{
    THROW_DBGLOG("Write%sFinish(): nesting depth underflow. depth of 0 is unexpected!", ct_names[isArray]);
}

NOINLINE static void wrongTypeError(bool isArray)
{
    THROW_DBGLOG("Write%sFinish(): Container type mismatch. Write%sFinish() is expected!", ct_names[isArray], ct_names[isArray ^ 1]);
}

NOINLINE void DataWriterInternal::expandedTooBigError(uintptr_t size, uintptr_t remaining)
{
    THROW_DBGLOG(LOGHDR "finalizeMessage(): buffer overflow while expanding complex message. Too big: %llu bytes", ID, 
        (ulonglong)size);
}


// Start writing array containing n items
INLINE void DataWriterInternal::writeArrayStart(size_t nItems)
{
    openContainer(true);
    writePackedUInt22((unsigned)nItems);
}


// Start writing nested object. Parameter is the number of items
INLINE void DataWriterInternal::writeObjectStart(uint8_t typeIndex)
{
    openContainer(false);
    putByte(typeIndex);
}


// Stop writing array
INLINE void DataWriterInternal::writeArrayEnd()
{
    closeContainer(true);
}


// Stop writing nested object
INLINE void DataWriterInternal::writeObjectEnd()
{
    closeContainer(false);
}


// Start writing array. Parameter is the number of items
void DataWriter::writeArrayStart(size_t nItems)
{
    THIS_IMPL->writeArrayStart(nItems);
    setNotNull();
}

// Stop writing array
void DataWriter::writeArrayEnd()
{
    THIS_IMPL->writeArrayEnd();
    setNotNull();
}

// Start writing nested object. Parameter is type Index of the object class
void DataWriter::writeObjectStart(uint8_t typeIndex)
{
    THIS_IMPL->writeObjectStart(typeIndex);
    setNotNull();
}

// Stop writing nested object
void DataWriter::writeObjectEnd()
{
    THIS_IMPL->writeObjectEnd();
    setNotNull();
}


#if LOADER_OBJ_IMPL==1

// Initial impl. Reserve space for max. length
INLINE void DataWriterInternal::reserveLengthField()
{
    putBE<uint32>(0, 3);
}


INLINE void putUncompressedUInt22Special(byte * p, unsigned value)
{
    assert(value < (1 << 22));
    value = (value & 0x3F) + ((value & ~0x3F) << 2) + 0xC0;
    _storeLE<uint32_t>(p, (uint32_t)value | _loadLE<uint32_t>(p));
}


INLINE void DataWriterInternal::writeContainerHeader(bool isArray)
{
    size_t depth = this->depth;

    if ((intptr_t)depth >= LOADER_MAX_NESTED_OBJECTS) {
        depthOverflowError(isArray);
    }

    this->depth = depth + 1;
    containerHeader[depth] = dataPtr;
    containerIsArray[depth] = isArray;

    reserveLengthField();
}


void DataWriterInternal::writeContainerEnd(bool isArray)
{
    size_t depth = this->depth;
    if (0 == depth) {
        depthUnderflowError(isArray);
    }

    this->depth = --depth;

    if (containerIsArray[depth] != isArray) {
        wrongTypeError(isArray);
    }

    // NOTE: This is a temporary implementation
    byte *headerPtr = containerHeader[depth];
    uintptr_t length = dataPtr - headerPtr - 3;
    putUncompressedUInt22Special(headerPtr, (unsigned)length);
}


#elif LOADER_OBJ_IMPL==2

INLINE void DataWriterInternal::openContainer(bool isArray)
{
    size_t depth = this->depth;

    if ((intptr_t)depth >= LOADER_MAX_NESTED_OBJECTS) {
        // TODO: CHECK: typecast necessary? can initial depth be -1?
        depthOverflowError(isArray);
    }

    ++depth;
    DW_LOG("%sopen%s() @ depth %d", &depthstr[LOADER_MAX_NESTED_OBJECTS - depth], ct_names[isArray], depth);
    this->depth = depth;
    containerContentsExtraSize[depth] = 0;
    containerHeader[depth - 1] = dataPtr;
    containerIsArray[depth - 1] = isArray;

    reserveLengthField();
}


void DataWriterInternal::closeContainer(bool isArray)
{
    size_t depth = this->depth;
    if (0 == depth) {
        depthUnderflowError(isArray);
    }

    DW_LOG("%sclose%s() @ depth %d", &depthstr[LOADER_MAX_NESTED_OBJECTS - depth], ct_names[isArray], depth);

    this->depth = --depth;

    if (containerIsArray[depth] != (byte)isArray) {
        wrongTypeError(isArray);
    }

    byte *contentPtr = containerHeader[depth] + 1;
    uintptr_t length = dataPtr - contentPtr + containerContentsExtraSize[depth + 1];
    contentPtr[-1] = (byte)length;
    if (length < 0x80)
        return;

    assert(length < 0x400000);
    uint32_t extraHeaderBytes = 1 + (length >= 0x4000);
    // Length get special encoding here, slightly different from the usual
    uint32_t packedLength = ((uint32_t)length & 0x3F) + (((uint32_t)length & ~0x3F) << 2) + (extraHeaderBytes << 24) + (extraHeaderBytes << 6) + 0x40;

    containerContentsExtraSize[depth] += extraHeaderBytes;
    expansionPoints.emplace_back((uint32_t)(contentPtr - bufferStart), packedLength);
}


INLINE void DataWriterInternal::reserveLengthField()
{
    putByte(0);
}


INLINE void expandHeadersImplBasic(uint8_t * const pBufferStart, uint8_t *pEnd, intptr_t offset,
    const DataWriterInternal::ExpansionPoint points[], size_t n_points, const uint8_t * const pMessageStart)
{
    static const uint32_t MASK_ARRAY[3] = { 0, 0xFFFF0000, 0xFF000000 };
    assert((0 != offset) && (0 != n_points));

    --points;
    size_t i = n_points;
    do {
        auto &point = points[i];
        byte * pStart = pBufferStart + point.blockOffset;
        memmove(pStart + offset, pStart, pEnd - pStart);

        uint32_t length = point.headerLength;
        intptr_t diff = (int64_t)(length >> 24);
        assert(diff == 1 || diff == 2);

        pEnd = pStart - 1;
        offset -= diff;
        byte *pHdr = pEnd + offset;
        length = (length & 0xFFFFFF) | (_loadLE<uint32_t>(pHdr) & (MASK_ARRAY[diff]));
        _storeLE<uint32_t>(pHdr, length);
        assert(offset >= 0);
    } while (0 != --i);

    if (0 != offset || pEnd <= pMessageStart) {
        THROW_DBGLOG("Logic error in DataWriter complex object implementation, end offset = %p", (void *)offset);
    }
}


// Version 1
INLINE void expandHeadersImplV1(uint8_t * const pBufferStart, uint8_t *pEnd, intptr_t offset,
    const DataWriterInternal::ExpansionPoint points[], size_t n_points, const uint8_t * const pMessageStart)
{
    assert((0 != offset) && (0 != n_points));

    --points;
    size_t i = n_points;
    uint64_t tail = _load<uint64_t>(pEnd + offset);
    do {
        auto &point = points[i];
        byte *pStart = pBufferStart + point.blockOffset;
        uint32_t length = point.headerLength;
        uint64_t first = _load<uint64_t>(pStart);
        byte *p = pEnd;
        while ((p -= 8) > pStart) {
            _move<int64_t>(p + offset, p);
        }

        /*for (intptr i = (pEnd - pStart + 7) >> 3; 0 != i; --i) {
            _move<int64_t>(pStart + offset + i * 8, pStart + i * 8);
        }*/

        intptr_t diff = -(int64_t)(length >> 24);
        assert(diff == -1 || diff == -2);
        _storeLE<uint32_t>(pStart + offset - 1 + diff, length);
        // Will overwrite 1 or more bytes of the previous value
        _store<uint64_t>(pStart + offset, first);
        _store<uint64_t>(pEnd + offset, tail);
        pEnd = pStart - 1;
        offset += diff;
        tail = _load<uint64_t>(pEnd + offset);
        assert(offset >= 0);
    } while (0 != --i);

    if (0 != offset || pEnd <= pMessageStart) {
        THROW_DBGLOG("Logic error in DataWriter complex object implementation, end offset = %p", (void *)offset);
    }
}


void DataWriterInternal::expandHeaders(uint8_t * const pBufferStart, uint8_t *pEnd, intptr_t offset,
    const ExpansionPoint points[], size_t n_points, const uint8_t * const pMessageStart)
{
    assert((0 == offset) == (0 == n_points));
    if (0 == n_points)
        return;

    // The LAST copied segment should be big enough
    // The algorithms used is some possible implementations would corrupt the data if this condition is not met
    // The condition should be always true because Expansion Points are only inserted before blocks bigger than 127 bytes
    assert(pEnd - (pBufferStart + points[n_points - 1].blockOffset) > 127);
    LOADER_OBJ_EXPAND_HEADERS_IMPL(pBufferStart, pEnd, offset, points, n_points, pMessageStart);
}

#else // #elif LOADER_OBJ_IMPL==2
#error LOADER_OBJ_IMPL not defined or has illegal value
#endif // #if LOADER_OBJ_IMPL==...


void DataWriterInternal::setBuffer(byte *allocPtr, byte *startPtr, byte *endPtr)
{
    assert(NULL != allocPtr);
    assert(NULL != startPtr);
    assert(NULL != endPtr);
    assert(allocPtr <= startPtr);
    assert(startPtr < endPtr);

    bufferAllocPtr = allocPtr;
    bufferStart = startPtr;
    dataEnd = endPtr;
}
