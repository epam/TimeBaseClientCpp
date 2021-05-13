#pragma once
#include "dxplatform.h"

namespace DxApi {
    enum Constants {
        ENUM_NULL       =         -1,
        TIMEOFDAY_NULL  =         -1,
        INTERVAL_NULL   =          0,
        CHAR_NULL       =          0,
        BOOL_FALSE      =          0,
        BOOL_TRUE       =          1,
        BOOL_NULL       =       0xFF,
        INT8_NULL       =      -0x80,
        INT16_NULL      =  INT16_MIN,
        INT32_NULL      =  INT32_MIN,
        UINT30_NULL     = 0x7FFFFFFF,
    };


    // 64-bit constants
    static const int64_t  INT48_MIN         = INT64_C(-0x0000800000000000);
    static const int64_t  INT48_MAX         = INT64_C( 0x00007FFFFFFFFFFF);
    static const int64_t  INT48_NULL        = INT48_MIN;
    static const int64_t  INT64_NULL        = INT64_MIN; //INT64_C(-0x8000000000000000);
    static const uint64_t UINT61_NULL       = INT64_C( 0x7fffffffffffffff);
    static const int64_t  TIMESTAMP_NULL    = INT64_NULL;
    static const int64_t  ALPHANUMERIC_NULL = INT64_NULL;

    static const int64_t  TIMESTAMP_UNKNOWN = INT64_NULL;
    static const int64_t  USE_CURRENT_TIME  = TIMESTAMP_UNKNOWN + 1;
    static const int64_t  USE_CURSOR_TIME   = TIMESTAMP_UNKNOWN + 2;


    static const uint32_t EMPTY_MSG_TYPE_ID = UINT32_MAX;
    static const int      emptyStreamId  = -1;
    //static const unsigned emptyLocalTypeId  =  0;
    static const uint32_t emptyEntityId     = UINT32_MAX;

    typedef union _f64bits {
        uint64_t u;
        double f;
    } f64bits;

    typedef union _f32bits {
        uint32_t u;
        float f;
    } f32bits;


    static const f32bits float32null   = { UINT32_C(0x7fc00000) };
    static const f64bits float64null   = { UINT64_C(0x7ff8000000000000) };
    static const f64bits float64posinf = { UINT64_C(0x7ff0000000000000) };
    static const f64bits float64neginf = { UINT64_C(0xfff0000000000000) };


#define FLOAT32_NULL                float32null.f
#define FLOAT64_NULL                float64null.f
#define FLOAT64_POSITIVE_INFINITY   float64posinf.f
#define FLOAT64_NEGATIVE_INFINITY   float64neginf.f
#define DECIMAL_NULL FLOAT64_NULL


#define FLOAT32_NULL                float32null.f
#define FLOAT64_NULL                float64null.f
#define FLOAT64_POSITIVE_INFINITY   float64posinf.f
#define FLOAT64_NEGATIVE_INFINITY   float64neginf.f
#define DECIMAL_NULL FLOAT64_NULL

    struct FieldTypeEnum {
        enum Enum {
            UNKNOWN,    // This value is used as terminator
            INT8,
            INT16,
            INT32,
            INT48,
            INT64,
            BOOL,
            CHAR,
            TIME_OF_DAY,
            FLOAT32,
            FLOAT64,
            DECIMAL,        /* (0..14) */
            ALPHANUM,       /* (1..65535?)  */
            STRING,
            PUINT30,
            PUINT61,
            PINTERVAL,
            ENUM8,
            ENUM16,
            ENUM32,
            ENUM64,
            OBJECT,
            ARRAY,
            BINARY,
            PTIME,
            NULLABLE = 0x20,
            INVALID = 0xFF
        };
    };

}