#pragma once

#include "dxcommon.h"
#include "dxconstants.h"

// PUBLIC API


// Version of implementation. Temporary defines for internal use
#define SR_IMPL 1
//#define FAKESKIP 1

// SR_VERIFY 1 turns on schema verification
//#define SR_VERIFY 1

// SR_VERIFY 2 turns on read method logging, only compiles in debug mode
#ifdef _DEBUG
//#define SR_VERIFY 2
#endif

namespace DxApi {
    class DataReader {
    public:
        /**********************************************************************
        * Uncompressed integer types
        */

        // Integers
        uint8_t             readUInt8();
        int8_t              readInt8();
        int16_t             readInt16();
        int32_t             readInt32();
        uint64_t            readUInt40();
        int64_t             readInt48();
        int64_t             readInt64();

        void                skipInt8();
        void                skipInt16();
        void                skipInt32();
        void                skipInt48();
        void                skipInt64();

        // Compressed integers, all are nullable
        uint32_t            readPUInt30();
        uint64_t            readPUInt61();
        uint32_t            readInterval();
        int64_t             readDateTimeMs();
        int64_t             readDateTimeNs();
        void                skipPUInt30();
        void                skipPUInt61();
        void                skipInterval();
        void                skipDateTime();

        // Floating point types
        double       readFloat64();
        float        readFloat32();

        // Compressed floating point
        double              readDecimal();
        void                skipDecimal();

        // Timestamp (uncompressed int64)
        int64_t      readTimestamp();

        // TimeOfDay
        int32_t      readTimeOfDay();

        // Enum types
        int          readEnum8();
        int          readEnum16();
        int          readEnum32();
        int64_t      readEnum64();

        // Other types
        bool         readBoolean();
        int8_t       readNullableBooleanInt8();
        wchar_t      readWChar();

        /**********************************************************************
        * Nullable types, return false if null
        */

        // Integers
        bool         readInt8(int8_t &value);
        bool         readInt16(int16_t &value);
        bool         readInt32(int32_t &value);
        bool         readInt48(int64_t &value);
        bool         readInt64(int64_t &value);

        // Compressed unsigned integers
        bool                readPUInt30(uint32_t &value);
        bool                readPUInt61(uint64_t &value);
        bool                readPUInt30(int32_t &value);
        bool                readPUInt61(int64_t &value);
        bool         readInterval(int32_t &value);
        bool         readInterval(uint32_t &value);

        // Floating point types
        bool         readFloat64(double &value);
        bool         readFloat32(float &value);

        // Compressed floating point
        bool                readDecimal(double &value);
        
        // Timestamp
        bool         readTimestamp(int64_t &value);

        // TimeOfDay
        //INLINE bool         getTimeOfDay(int32_t &value);

        // Enum types
        bool         readEnum8(int &value);
        bool         readEnum16(int &value);
        bool         readEnum32(int &value);
        bool         readEnum64(int64_t &value);

        // Other types
        bool         readNullableBoolean(bool &value);
        bool         readWChar(wchar_t &value);

    public:

        // Strings and raw data
        template <typename Q>  bool readAlphanumeric(std::basic_string<Q, std::char_traits<Q>, std::allocator<Q>> &value, uint32_t maxFieldLength);
        bool readAlphanumeric(char value[], uint32_t maxFieldLength);
        template <typename T> T readAlphanumericAs(uint32_t maxFieldLength);
        void skipAlphanumeric(uint32_t maxFieldLength);

        // returns true if not NULL
        bool        readUTF8(std::string &value);
        void        skipUTF8();
        
        bool        readAscii(std::string &value);
        void        skipAscii();

        bool        readBinary(std::vector<uint8_t> &value);
        void        skipBinary();

        // Array

        // Returns array length(number of items, starting from 0). Returns INT32_NULL if NULL
        int32_t     readArrayStart();
        void        readArrayEnd();
        void        skipArray();
        
        // Reads object header. Returns object type index (0 .. 0xFF) Returns INT32_NULL if NULL
        int32_t     readObjectStart(); 
        void        readObjectEnd();
        void        skipObject();

        // true if read pointer is at the end of the message
        bool        endOfContent() const;

        // Skip the specified amount of compressed data bytes. 
        void        skip(size_t numBytes);

    protected:
        uint8_t      getByte();
        uint16_t     getUInt16();
        uint32_t     getUInt32();
        uint64_t     getUInt64();

        // copy bytes from string into buffer, must hold at least dataLength + 1 bytes
        void getUTF8chars(uint8_t *dstBuffer, size_t size);
        void getUTF8chars(char *dstBuffer, size_t size);

        // Currently conversion is done elsewhere, so this version is not supported.
        // TODO: cleanup
        // void getUTF8chars(wchar_t *dstBuffer, size_t size);

            // Stubs that redirects to virtual member call, preferably without inlining.
        const uint8_t*  onReadBarrier(const uint8_t *from, size_t size);
        void            onSkipBarrier(const uint8_t *from, size_t size);
        void            onReadTypeMismatch(uint8_t t);
        void            onSkipTypeMismatch(uint8_t t);

        // Get constant pointer to the specified number of bytes from the input stream and advance the read pointer by that amount
        // bytes are guaranteed to be available for reading until the next getXX/bytes() call
        const uint8_t* bytes(size_t size);

        // Should not be called directly by any code visible through this header file
        virtual const uint8_t* get(const uint8_t * from, size_t size) = 0;
        virtual void skip(const uint8_t * from, size_t size) = 0;

        // Low-level data access functions
        template <typename P, typename Q> static INLINE const P& as(const Q &x) { return DxApiImpl::_as<P, Q>(x); }
        template <typename T> INLINE T loadBE() { return DxApiImpl::_loadBE<T>(bytes(sizeof(T))); }
        template <typename T, int NBITS> INLINE T loadBE() { return DxApiImpl::_loadBE<T>(bytes((NBITS + 7) >> 3)) >> ((8 * sizeof(T)) - NBITS); }

        DataReader();
        DISALLOW_COPY_AND_ASSIGN(DataReader);

    protected:
        const uint8_t   *dataPtr;           // Read pointer
        const uint8_t   *dataEnd;           // Valid data end barrier
        const uint8_t   *contentEnd;        // End of the current object/array/message
        const uint32_t  *schemaPtr;         // 
    };

    // TODO: All this debug/verification code should be moved into separate header that is set to empty for release build

#if !defined(SR_VERIFY) || SR_VERIFY == 0
#define SR_LOG_READ(OUT_TYPE, TYPE_NAME)
#define SR_LOG_SKIP(TYPE_NAME)
#define SR_LOG (void)

#define SR_READ(OUT_TYPE, TYPE_NAME, ENUM_TYPE_NAME) OUT_TYPE DataReader::read##TYPE_NAME() {
#define SR_READ_NULLABLE(OUT_TYPE, TYPE_NAME, ENUM_TYPE_NAME) bool DataReader::read##TYPE_NAME(OUT_TYPE & value) {
#define SR_SKIP(TYPE_NAME, ENUM_TYPE_NAME) void DataReader::skip##TYPE_NAME() { if (endOfContent()) return;
#elif SR_VERIFY == 1
#define SR_LOG_READ(OUT_TYPE, TYPE_NAME)
#define SR_LOG_SKIP(TYPE_NAME)
#define SR_LOG (void)

#error Update verification code to use nullable flag and sizes
#define SR_READ(OUT_TYPE, TYPE_NAME, ENUM_TYPE_NAME) OUT_TYPE DataReader::read##TYPE_NAME() { \
    if ((*(uint8_t *)(schemaPtr++) & 0x1F) != (uint8_t)FieldType::ENUM_TYPE_NAME) onReadTypeMismatch((uint8_t)FieldType::ENUM_TYPE_NAME);

#define SR_READ_NULLABLE(OUT_TYPE, TYPE_NAME, ENUM_TYPE_NAME) bool DataReader::read##TYPE_NAME(OUT_TYPE & value) { \
    if ((*(uint8_t *)(schemaPtr++) & 0x1F) != (uint8_t)FieldType::ENUM_TYPE_NAME) onReadTypeMismatch((uint8_t)FieldType::ENUM_TYPE_NAME);

#define SR_SKIP(TYPE_NAME, ENUM_TYPE_NAME) void DataReader::skip##TYPE_NAME() { \
    if ((*(uint8_t *)(schemaPtr++) & 0x1F) != (uint8_t)FieldType::ENUM_TYPE_NAME) onSkipTypeMismatch((uint8_t)FieldType::ENUM_TYPE_NAME); \
    if (endOfContent()) return;

#else
#define SR_LOG_READ(OUT_TYPE, TYPE_NAME) \
    DBGLOG("    " #OUT_TYPE " read" #TYPE_NAME "()");

#define SR_LOG_SKIP(TYPE_NAME) \
    DBGLOG("    skip" #TYPE_NAME "()");

#define SR_LOG DBGLOG

#define SR_READ(OUT_TYPE, TYPE_NAME, ENUM_TYPE_NAME) OUT_TYPE DataReader::read##TYPE_NAME() { \
    SR_LOG_READ(OUT_TYPE, TYPE_NAME);

#define SR_READ_NULLABLE(OUT_TYPE, TYPE_NAME, ENUM_TYPE_NAME) bool DataReader::read##TYPE_NAME(OUT_TYPE & value) { \
    SR_LOG_READ(OUT_TYPE, TYPE_NAME);

#define SR_SKIP(TYPE_NAME, ENUM_TYPE_NAME) void DataReader::skip##TYPE_NAME() { \
    SR_LOG_SKIP(TYPE_NAME); \
    if (endOfContent()) return;

#endif

    /*************************************************************************
    * DataReader public inline methods implementation
    */


    INLINE bool DataReader::endOfContent() const
    { 
        return dataPtr == contentEnd;
    }
    

    INLINE SR_READ(uint8_t, UInt8, INT8)
        return endOfContent() ? INT8_NULL : getByte();
    }


    INLINE SR_READ(int8_t, Int8, INT8)
        return endOfContent() ? INT8_NULL : (int8_t)getByte();
    }


    INLINE SR_READ(int16_t, Int16, INT16)
        return endOfContent() ? INT16_NULL : (int16_t)getUInt16();
    }


    INLINE SR_READ(int32_t, Int32, INT32)
        return endOfContent() ? INT32_NULL : (int32_t)getUInt32();
    }


    // I dont have null value for this type. 
    INLINE SR_READ(uint64_t, UInt40, UNKNOWN)
        return loadBE<uint64_t, 40>();
        //return endOfContent() ? INT40_NULL : loadBE<uint64_t, 40>();
    }


    INLINE SR_READ(int64_t, Int48, INT48)
        return endOfContent() ? INT48_NULL : loadBE<int64_t, 48>();
    }


    INLINE SR_READ(int64_t, Int64, INT64)
        return endOfContent() ? INT64_NULL : getUInt64();
    }


    INLINE SR_READ(double, Float64, FLOAT64)
        return endOfContent() ? FLOAT64_NULL : loadBE<double>();
    }


    INLINE SR_READ(float, Float32, FLOAT32)
        return endOfContent() ? FLOAT32_NULL : loadBE<float>();
    }


    INLINE SR_READ(int64_t, Timestamp, INT64)
        return endOfContent() ? TIMESTAMP_NULL : getUInt64();
    }


    INLINE SR_READ(int32_t, TimeOfDay, TIME_OF_DAY)
        return endOfContent() ? TIMEOFDAY_NULL : getUInt32();
    }


    INLINE SR_READ(int, Enum8, ENUM8)
        if (endOfContent()) {
            return ENUM_NULL;
        }
        return loadBE<int8_t>();
    }


    INLINE SR_READ(int, Enum16, ENUM16)
        if (endOfContent()) {
            return ENUM_NULL;
        }
        return loadBE<int16_t>();
    }


    INLINE SR_READ(int, Enum32, ENUM32)
        if (endOfContent()) {
            return ENUM_NULL;
        }
        return loadBE<int32_t>();
    }


    INLINE SR_READ(int64_t, Enum64, ENUM64)
        if (endOfContent()) {
            return ENUM_NULL;
        }
        return loadBE<int64_t>();
    }


    INLINE SR_READ(bool, Boolean, BOOL)
        return 1 == getByte();
    }


    INLINE SR_READ(int8_t, NullableBooleanInt8, BOOL)
        return endOfContent() ? (int8_t)BOOL_NULL : (int8_t)getByte();
    }


    INLINE SR_READ(wchar_t, WChar, CHAR)
        return endOfContent() ? CHAR_NULL : getUInt16();
    }


    INLINE SR_SKIP(Int8, INT8)
        skip(1);
    }


    INLINE SR_SKIP(Int16, INT16)
        skip(2);
    }


    INLINE SR_SKIP(Int32, INT32)
        skip(4);
    }


    INLINE SR_SKIP(Int48, INT48)
        skip(6);
    }


    INLINE SR_SKIP(Int64, INT64)
        skip(8);
    }


    INLINE SR_SKIP(PUInt61, PUINT61)
        skip(getByte() >> 5);
    }


    INLINE SR_SKIP(PUInt30, PUINT30)
        skip(getByte() >> 6);
    }


    INLINE SR_SKIP(Interval, PINTERVAL)
        skip(0xC0 == (0xC0 & getByte()) ? 4 : 0);
    }


    INLINE SR_SKIP(UTF8, STRING)
        unsigned l = getUInt16();
        skip(0xFFFF == l ? 0 : l);
    }

    INLINE SR_SKIP(Ascii, STRING)
        unsigned l = getUInt16();
        skip(0xFFFF == l ? 0 : l);
    }

    /**********************************************************************
    * Nullable types, return false if null.
    */

//    INLINE bool DataReader::readInt8(int8_t &value)

    INLINE SR_READ_NULLABLE(int8_t, Int8, INT8)
        return !endOfContent() && INT8_NULL != (value = getByte());
    }

    //INLINE bool DataReader::readInt16(int16_t &value)
    INLINE SR_READ_NULLABLE(int16_t, Int16, INT16)
        if (endOfContent()) return false;
        int16_t x = loadBE<int16_t>();
        return INT16_NULL != (value = x);
    }

    //INLINE bool DataReader::readInt32(int32_t &value)
    INLINE SR_READ_NULLABLE(int32_t, Int32, INT32)
        if (endOfContent()) return false;
        int32_t x = loadBE<int32_t>();
        return INT32_NULL != (value = x);
    }


    //INLINE bool DataReader::readInt48(int64_t &value)
    INLINE SR_READ_NULLABLE(int64_t, Int48, INT48)
        if (endOfContent()) return false;
        int64_t x = loadBE<int64_t, 48>();
        return INT48_NULL != (value = x);
    }

    //INLINE bool DataReader::readInt64(int64_t &value)
    INLINE SR_READ_NULLABLE(int64_t, Int64, INT64)
        if (endOfContent()) return false;
        int64_t x = loadBE<int64_t>();
        return INT64_NULL != (value = x);
    }

    //INLINE bool DataReader::readUInt30(uint32_t &value)
    INLINE SR_READ_NULLABLE(uint32_t, PUInt30, PUINT30)
        if (endOfContent()) return false;
        uint32_t x = readPUInt30();
        return UINT30_NULL != (value = x);
    }

    //INLINE bool DataReader::readUInt30(int32_t &value)
    INLINE SR_READ_NULLABLE(int32_t, PUInt30, PUINT30)
        if (endOfContent()) return false;
        uint32_t x = readPUInt30();
        return UINT30_NULL != (value = x);
    }

    //INLINE bool DataReader::readUInt61(uint64_t &value)
    // TODO: Double check here etc.
    INLINE SR_READ_NULLABLE(uint64_t, PUInt61, PUINT61)
        if (endOfContent()) return false;
        uint64_t x = readPUInt61();
        return UINT61_NULL != (value = x);
    }

    //INLINE bool DataReader::readUInt61(int64_t &value)
    INLINE SR_READ_NULLABLE(int64_t, PUInt61, PUINT61)
        if (endOfContent()) return false;
        uint64_t x = readPUInt61();
        return UINT61_NULL != (value = x);
    }

    //INLINE bool DataReader::readInterval(uint32_t &value)
    INLINE SR_READ_NULLABLE(uint32_t, Interval, PINTERVAL)
        if (endOfContent()) return false;
        uint32_t x = readInterval();
        return INTERVAL_NULL != (value = x);
    }

    //INLINE bool DataReader::readInterval(int32_t &value)
    INLINE SR_READ_NULLABLE(int32_t, Interval, PINTERVAL)
        if (endOfContent()) return false;
        uint32_t x = readInterval();
        return INTERVAL_NULL != (value = x);
    }

    // Floating point types
    //INLINE bool DataReader::readFloat64(double &value)
    INLINE SR_READ_NULLABLE(double, Float64, FLOAT64)
        if (endOfContent()) return false;
        uint64_t x = getUInt64();
        value = as<double>(x);
        return float64null.u == x;
    }

    //INLINE bool DataReader::readFloat32(float &value)
    INLINE SR_READ_NULLABLE(float, Float32, FLOAT32)
        if (endOfContent()) return false;
        uint32_t x = getUInt32();
        value = as<float>(x);
        return float32null.u == x;
    }

    /*INLINE bool getDecimal(double &value) {
    x = getDecimal();
    return DECIMAL_NULL == (value = x);
    }*/

    // Enum types
    //INLINE bool DataReader::readEnum8(int &value)
    INLINE SR_READ_NULLABLE(int, Enum8, ENUM8)
        if (endOfContent()) return false;
        return (value = loadBE<int8_t>()) != ENUM_NULL;
    }

    //INLINE bool DataReader::readEnum16(int &value)
    INLINE SR_READ_NULLABLE(int, Enum16, ENUM16)
        if (endOfContent()) return false;
        return (value = loadBE<int16_t>()) != ENUM_NULL;
    }

    //INLINE bool DataReader::readEnum32(int &value)
    INLINE SR_READ_NULLABLE(int, Enum32, ENUM32)
        if (endOfContent()) return false;
        return (value = loadBE<int32_t>()) != ENUM_NULL;
    }

    //INLINE bool DataReader::readEnum64(int64_t &value)
    INLINE SR_READ_NULLABLE(int64_t, Enum64, ENUM64)
        if (endOfContent()) return false;
        return (value = loadBE<int64_t>()) != ENUM_NULL;
    }

    //INLINE bool DataReader::readTimestamp(int64_t &value)
    INLINE SR_READ_NULLABLE(int64_t, Timestamp, INT64)
        if (endOfContent()) return false;
        int64_t x = readInt64();
        return TIMESTAMP_NULL != (value = x);
    }

    //INLINE bool DataReader::readNullableBoolean(bool &value)
    INLINE SR_READ_NULLABLE(bool, NullableBoolean, BOOL)
        if (endOfContent()) return false;
        uint8_t x = getByte();
        value = 1 == x;
        return BOOL_NULL != x;
    }

    //INLINE bool DataReader::readWChar(wchar_t &value)
    INLINE SR_READ_NULLABLE(wchar_t, WChar, CHAR)
        if (endOfContent()) return false;
        return CHAR_NULL != (value = getUInt16());
    }


    /*********************************************************************
    * Private methods
    */

    INLINE uint8_t DataReader::getByte()
    {
        return loadBE<uint8_t>();
    }


    INLINE uint16_t DataReader::getUInt16()
    {
        return loadBE<uint16_t>();
    }


    INLINE uint32_t DataReader::getUInt32()
    {
        return loadBE<uint32_t>();
    }


    INLINE uint64_t DataReader::getUInt64()
    {
        return loadBE<uint64_t>();
    }


    INLINE void DataReader::skip(size_t size)
    {
#if FAKESKIP == 1
        while (size--) getByte();
#else        
        const uint8_t * p = dataPtr;
        if ((dataPtr = (p + size)) > dataEnd) {
            onSkipBarrier(p, size);
        }
#endif
    }

    // Stub that redirects to virtual member call, preferably without inlining. See fetchData definition. 

#if 1 == SR_IMPL
    INLINE const uint8_t* DataReader::bytes(size_t size)
    {
        const uint8_t *p = dataPtr;
        if ((dataPtr = (p + size)) > dataEnd) {
            p = onReadBarrier(p, size);
        }
        return p;
    }
#elif 2==SR_IMPL
#error Removed
#elif 3==SR_IMPL
#error Removed    
#endif

} // namespace DxApi