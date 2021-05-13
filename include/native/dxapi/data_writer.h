#pragma once

#include "dxcommon.h"
#include "dxconstants.h"

// PUBLIC API

namespace DxApi {
    class DataWriter {
    public:
       /**********************************************************************
        * Non-nullable types, have special value if actually null
        */

        // Integers
        void    writeByte(uint8_t);
        void    writeInt8(int8_t);
        void    writeUInt16(uint16_t);
        void    writeInt16(int16_t);
        void    writeUInt32(uint32_t);
        void    writeInt32(int32_t);
        void    writeUInt40(uint64_t);
        void    writeInt48(int64_t);
        void    writeUInt64(uint64_t);
        void    writeInt64(int64_t);

        // Compressed unsigned integers
        void    writePUInt30(uint32_t);
        void    writePUInt61(uint64_t);
        void    writeInterval(uint32_t);
        void    writeCompressedTime(int64_t);         // Milliseconds
        void    writeCompressedNanoTime(int64_t);     // Nanoseconds

        // Floating point types
        void    writeFloat64(double);
        void    writeFloat32(float);

        // Compressed floating point
        void    writeDecimal(double);
        void    writeDecimal(double value, unsigned precision);

        // Timestamp
        void    writeTimestamp(int64_t);

        // Timestamp
        void    writeTimeOfDay(int32_t);

        // Enum types
        void    writeEnum8(int);
        void    writeEnum16(int);
        void    writeEnum32(int);
        void    writeEnum64(int64_t);

        // Other types
        void    writeBoolean(bool);
        void    writeWChar(wchar_t);

        // Binary array, data pointer can't be null, use writeBinaryArrayNull to write NULL
        void     writeBinaryArray(const uint8_t *data, size_t size);
        void     writeBinaryArrayNull();

        // Arrays

        // Start writing array. Parameter is the number of items array will contain. We can write less items than specified here.
        void    writeArrayStart(size_t nItems);

        // Stop writing array
        void    writeArrayEnd();

        // Write NULL array
        void    writeArrayNull();

        // Objects

        // Start writing nested object. Parameter is the number of items
        void    writeObjectStart(uint8_t typeIndex);

        // Stop writing nested object
        void    writeObjectEnd();

        // Write NULL object
        void    writeObjectNull();
                
        // Not nullable (s != NULL)
        void    writeUTF8(const char * s, size_t length);
        
        // Not nullable (s != NULL)
        void    writeUTF8(const wchar_t * s, size_t length);

        // Not nullable (s != NULL)
        void    writeAscii(const char * s, size_t length);

        // Not nullable (s != NULL)
        void    writeAscii(const wchar_t * s, size_t length);

        void    writeAlphanumericNull(uint32_t size);
        void    writeAlphanumeric(uint32_t fieldSize, const char *str, size_t stringLength);
        void    writeAlphanumeric(uint32_t fieldSize, const wchar_t *str, size_t stringLength);
        void    writeAlphanumeric(uint32_t fieldSize, const std::string &value);
        void    writeAlphanumeric(uint32_t fieldSize, const std::string *value);
        // bool writeAlphanumeric(char value[], size_t size); // TODO: Later
        
        template <typename T>
                void writeAlphanumericInt(uint32_t fieldSize, T value);

        INLINE void writeUTF8(const std::string &s)     { writeUTF8(s.data(), s.length()); }
        INLINE void writeUTF8(const std::wstring &s)    { writeUTF8(s.data(), s.length()); }

        template<class T> INLINE void writeUTF8(const T *s)
        {
            if (NULL == s) {
                putUInt16(0xFFFF);
            }
            else {
                writeUTF8(*s);
            }
        }

        INLINE void writeAscii(const std::string &s) { writeAscii(s.data(), s.length()); }
        INLINE void writeAscii(const std::wstring &s) { writeAscii(s.data(), s.length()); }

        template<class T> INLINE void writeAscii(const T *s)
        {
            if (NULL == s) {
                putUInt16(0xFFFF);
            }
            else {
                writeAscii(*s);
            }
        }

       /**********************************************************************
        * Nullable types, write null if bool or pointer parameter is false.
        */

        // Integers
        void         writeInt8(const int8_t value, bool isNull);
        void         writeInt16(const int16_t value, bool isNull);
        void         writeInt32(const int32_t value, bool isNull);
        void         writeInt48(const int64_t value, bool isNull);
        void         writeInt64(const int64_t value, bool isNull);

        // Compressed unsigned integers
        void         writePUInt30(uint32_t value, bool isNull);
        void         writePUInt61(uint64_t value, bool isNull);
        void         writeInterval(uint32_t value, bool isNull);

        // Floating point types
        void         writeFloat64(double value, bool isNull);
        void         writeFloat32(float value, bool isNull);

        // Compressed floating point
        void         writeDecimal(double value, bool isNull);

        // Timestamp
        void         writeTimestamp(int64_t value, bool isNull);

        // Enum types
        void         writeEnum8(int value, bool isNull);
        void         writeEnum16(int value, bool isNull);
        void         writeEnum32(int value, bool isNull);
        void         writeEnum64(int64_t value, bool isNull);

        // Other types
        void         writeNullableBoolean(bool value, bool isNull);
        void         writeWChar(wchar_t value, bool isNull);

        void        writeNullableUTF8(const char *srcString, size_t length);
        void        writeNullableUTF8(const wchar_t *srcString, size_t length);

        // Misc
    protected:
        void putByte(uint8_t value);
        void putUInt16(uint16_t value);
        void putUInt32(uint32_t value);
        void putUInt64(uint64_t value);
        void putFloat64(double value);
        void putFloat32(float value);

        void checkNull(bool isNull);
        void setNotNull();
        void wroteNull();

        // Arbitrary sequence of bytes (RAW data), can not be NULL
        void    putBytes(const uint8_t *srcData, size_t size);

        template <typename P, typename Q> static const P& as(const Q &x);
        template <typename T> static void storeBE(void * ptr, T x);
        template <typename T> void putBE(const T x, size_t size);
        template <typename T> void putBE(const T x);

        DataWriter();
        DISALLOW_COPY_AND_ASSIGN(DataWriter);

    protected:
        uint8_t         *dataPtr;               // Current write pointer
        const uint8_t   *dataEnd;               // Written data end pointer (including written NULL values)
        uint8_t         *dataEndNotNull;        // Written data end pointer (ignoring trailing NULL values)
        
    protected:
        uint8_t* onOverflow();
    };


   /*************************************************************************
    * Internal inline methods implementation
    */

    INLINE void DataWriter::putByte(uint8_t value)
    {
        putBE<uint8_t>(value);
    }


    INLINE void DataWriter::putUInt16(uint16_t value)
    {
        putBE<uint16_t>(value);
    }


    INLINE void DataWriter::putUInt32(uint32_t value)
    {
        putBE<uint32_t>(value);
    }


    INLINE void DataWriter::putUInt64(uint64_t value)
    {
        putBE<uint64_t>(value);
    }


    INLINE void DataWriter::putFloat64(double value)
    {
        putBE<double>(value);
    }


    INLINE void DataWriter::putFloat32(float value)
    {
        putBE<float>(value);
    }


    INLINE void DataWriter::checkNull(bool isNull)
    {
        // I'm deliberately using these temp vars. Don't remove, if you don't know why
        uint8_t *dataEndNotNullCopy     = dataEndNotNull;
        uint8_t *dataPtrCopy            = dataPtr;

        dataEndNotNull = isNull ? dataEndNotNullCopy : dataPtrCopy;
    }


    INLINE void DataWriter::setNotNull()
    {
        dataEndNotNull = dataPtr;
    }

    // Dummy function. Called after writing NULL value
    INLINE void DataWriter::wroteNull()
    {
    }

   /*************************************************************************
    * DataReader inline methods implementation
    */


    INLINE void DataWriter::writeByte(uint8_t value)
    {
        putByte(value);
    }

    INLINE void DataWriter::writeInt8(int8_t value)
    {
        putByte(value);
        checkNull(INT8_NULL == value);
    }

    INLINE void DataWriter::writeUInt16(uint16_t value)
    {
        putUInt16(value);
    }

    INLINE void DataWriter::writeInt16(int16_t value)
    {
        putUInt16(value);
        checkNull(INT16_NULL == value);
    }

    INLINE void DataWriter::writeUInt32(uint32_t value)
    {
        putUInt32(value);
    }

    INLINE void DataWriter::writeInt32(int32_t value)
    {
        putUInt32(value);
        checkNull(INT32_NULL == value);
    }

    INLINE void DataWriter::writeUInt40(uint64_t value)
    {
        assert(value < (1ULL << 40));
        putBE<uint64_t>(value << 24, 5);
    }

    INLINE void DataWriter::writeInt48(int64_t value)
    {
        assert(value < (1LL << 47) && value >= (-1LL << 47));
        putBE<uint64_t>(value << 16, 6);
        checkNull(INT48_NULL == value);
    }

    INLINE void DataWriter::writeUInt64(uint64_t value)
    {
        putUInt64(value);
    }

    INLINE void DataWriter::writeInt64(int64_t value)
    {
        putUInt64(value);
        checkNull(INT64_NULL == value);
    }

    INLINE void DataWriter::writeFloat64(double value)
    {
        putFloat64(value);
        checkNull(std::isnan(value));
    }

    INLINE void DataWriter::writeFloat32(float value)
    {
        putFloat32(value);
        checkNull(std::isnan(value));
    }

    INLINE void DataWriter::writeTimestamp(int64_t value)
    {
        putUInt64(value);
        checkNull(TIMESTAMP_NULL == value);
    }

    INLINE void DataWriter::writeTimeOfDay(int32_t value)
    {
        putUInt32(value);
        checkNull(TIMEOFDAY_NULL == value);
    }

    INLINE void DataWriter::writeEnum8(int value)
    {
        putByte((uint8_t)value);
        checkNull(ENUM_NULL == value);
    }

    INLINE void DataWriter::writeEnum16(int value)
    {
        putUInt16((uint16_t)value);
        checkNull(ENUM_NULL == value);
    }

    INLINE void DataWriter::writeEnum32(int value)
    {
        putUInt32((uint32_t)value);
        checkNull(ENUM_NULL == value);
    }

    INLINE void DataWriter::writeEnum64(int64_t value)
    {
        putUInt64(value);
        checkNull(ENUM_NULL == value);
    }

    INLINE void DataWriter::writeBoolean(bool value)
    {
        putByte(value);
        setNotNull();
    }

    INLINE void DataWriter::writeWChar(wchar_t value)
    {
        putUInt16(value);
        checkNull(CHAR_NULL == value);
    }

    INLINE void DataWriter::writeBinaryArrayNull()
    {
        putByte(1);
        wroteNull();
    }


    INLINE void DataWriter::writeArrayNull()
    {
        putByte(0);
        wroteNull();
    }


    INLINE void DataWriter::writeObjectNull()
    {
        putByte(0);
        wroteNull();
    }

   /**********************************************************************
    * Nullable types, return false if null
    */

    INLINE void DataWriter::writeInt8(int8_t value, bool isNull)
    {
        putByte(isNull ? INT8_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeInt16(int16_t value, bool isNull)
    {
        putUInt16(isNull ? INT16_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeInt32(int32_t value, bool isNull)
    {
        putUInt32(isNull ? INT32_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeInt48(int64_t value, bool isNull)
    {
        //put(isNull ? 0x80 : _byteswap_uint64(value) >> 16, 6);
        putBE<uint64_t>(isNull ? INT48_NULL << 16 : value << 16, 6);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeInt64(int64_t value, bool isNull)
    {
        //put(isNull ? 0x80 : _byteswap_uint64(value), 8);
        putBE<uint64_t>(isNull ? INT64_NULL : value);
        checkNull(isNull);
    }

    // Compressed integers

    INLINE void DataWriter::writePUInt30(uint32_t value, bool isNull)
    {
        writePUInt30(isNull ? UINT30_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writePUInt61(uint64_t value, bool isNull)
    {
        writePUInt61(isNull ? UINT61_NULL : value);
        checkNull(isNull);
    }
    
    INLINE void DataWriter::writeInterval(uint32_t value, bool isNull)
    {
        writeInterval(isNull ? INTERVAL_NULL : value);
        checkNull(isNull);
    }

    // Floating point types
    INLINE void DataWriter::writeFloat64(double value, bool isNull)
    {
        putBE<uint64_t>(isNull ? float64null.u : as<uint64_t>(value)); // Should be done better
        checkNull(isNull);
    }

    INLINE void DataWriter::writeFloat32(float value, bool isNull)
    {

        putBE<uint32_t>(isNull ? float32null.u : as<uint32_t>(value)); // Should be done better
        checkNull(isNull);
    }

    // Enum types
    INLINE void DataWriter::writeEnum8(int value, bool isNull)
    {
        putByte(isNull ? ENUM_NULL : (uint8_t)value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeEnum16(int value, bool isNull)
    {
        putUInt16(isNull ? ENUM_NULL : (uint16_t)value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeEnum32(int value, bool isNull)
    {
        putUInt32(isNull ? ENUM_NULL : (uint32_t)value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeEnum64(int64_t value, bool isNull)
    {
        putUInt64(isNull ? ENUM_NULL : (uint64_t)value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeTimestamp(int64_t value, bool isNull)
    {
        putUInt64(isNull ? TIMESTAMP_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeNullableBoolean(bool value, bool isNull)
    {
        putByte(isNull ? BOOL_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeWChar(wchar_t value, bool isNull)
    {
        putUInt16(isNull ? CHAR_NULL : value);
        checkNull(isNull);
    }

    INLINE void DataWriter::writeAlphanumeric(uint32_t size, const std::string &value)
    {
        writeAlphanumeric(size, value.c_str(), value.size());
        setNotNull();
    }


    INLINE void DataWriter::writeAlphanumeric(uint32_t size, const std::string *value)
    {
        if (NULL == value) {
            writeAlphanumericNull(size);
        }
        else {
            writeAlphanumeric(size, value->c_str(), value->size());
            setNotNull();
        }
    }


   /*********************************************************************
    * Private functions
    */
    template <typename P, typename Q> INLINE const P& DataWriter::as(const Q &x)
    { 
        return DxApiImpl::_as<P, Q>(x);
    }


    template <typename T> INLINE void DataWriter::storeBE(void *ptr, T x)
    {
        return DxApiImpl::_storeBE<T>(ptr, x);
    }


    template <typename T> INLINE void DataWriter::putBE(const T value, size_t size)
    {
        uint8_t * p;
        assert(size <= 8);
        storeBE(p = dataPtr, value);
        if ((dataPtr = p + size) > dataEnd) {
            onOverflow();
        }
    }

    INLINE void DataWriter::putBytes(const uint8_t *data, size_t size)
    {
        assert(NULL != data);
        uint8_t * p = dataPtr;
        // TODO: Check correctness
        if ((dataPtr = p + size) > dataEnd) {
            memcpy(p, data, dataEnd - p);
            onOverflow();
        }
        else {
            memcpy(p, data, size);
        }
    }

    template <typename T> INLINE void DataWriter::putBE(const T value)
    {
        putBE(value, sizeof(T));
    }

} // namespace DxApi
