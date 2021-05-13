#pragma once

namespace DxApiImpl {
    namespace TDB {
        enum Constants {
            TYPE_BLOCK_ID                   = 1,
            INSTRUMENT_BLOCK_ID             = 2,
            MESSAGE_BLOCK_ID                = 3,
            ERROR_BLOCK_ID                  = 4,
            TERMINATOR_BLOCK_ID             = 5,
            PING_BLOCK_ID                   = 6,
            CURSOR_BLOCK_ID                 = 7,
            COMMAND_BLOCK_ID                = 8,
            STREAM_BLOCK_ID                 = 9,

            REQ_UPLOAD_DATA                 = 0x01, // Open loader
            REQ_CREATE_CURSOR               = 0x02,
            REQ_CREATE_SESSION              = 0x03, // Open TickDb

                
            PROTOCOL_INIT                   = 0x18,
            LOADER_KEEPALIVE_ID             = 10,
            LOADER_RESPONSE_BLOCK_ID        = 11,

            CURSOR_MESSAGE_HEADER_SIZE      = 8 + 2 + 1 + 1,    // timestamp + instrument_index + type_index + stream_index (does not include 32-bit length prefix)
            LOADER_MESSAGE_HEADER_SIZE      = 8 + 2 + 1,        // timestamp + instrument_index + type_index (does not include 32-bit length prefix)
            MESSAGE_BLOCK_TERMINATOR_RECORD = 0xFFFFFFFF,       // pseudo-length that terminates message block
            ERROR_BLOCK_ID_WIDE             = 0xFFFFFFFE,       // ???
            MAX_MESSAGE_SIZE                = 0x400000,         // 4 MB, does not include header size, so actual max length field value = MAX_MESSAGE_SIZE+HEADER_SIZE - 1, according to java code

            ERR_INVALID_AGRUMENTS           = 1,
            ERR_PROCESSING                  = 2,

            RESP_ERROR               = -1,
            RESP_OK                  = 0,

            PROTOCOL_VERSION                = 6,
            DEFAULT_PORT                    = 8011,


            // Session sub-protocol
            REQ_GET_STREAMS                 = 1,
            REQ_GET_STREAM_PROPERTY         = 2,
            REQ_CLOSE_SESSION               = 3,

            STREAM_PROPERTY_CHANGED         = 11,
            STREAM_DELETED                  = 12,
            STREAM_CREATED                  = 13,
            STREAM_RENAMED                  = 14,
            STREAMS_DEFINITION              = 15,
            STREAM_PROPERTY                 = 16,
            SESSION_CLOSED                  = 17,
            SESSION_STARTED                 = 18,
            STREAMS_CHANGED                 = 19
        };


        // Interval holds milliseconds
        enum class IntervalGranularity {
            SECONDS     = 0,                // Interval length is divisible by 1000
            MINUTES     = 1,                // Interval length is divisible by 60 * 1000 = 60000
            HOURS       = 2,                // Interval length is divisible by 60 * 60 * 1000 = 3600000
            OTHER       = 3                 // Arbitrary length
        };


        // Timestamp holds nanoseconds
        enum class TimestampGranularity {
            INVALID     = 0,
            MILLIS      = 1,
            SECONDS     = 2,
            TEN_SECONDS = 3,
            HOURS       = 4,
            SPECIAL     = 5,                // NULL value/unknown
            NANOS       = 6,
            LONG_NANOS  = 7
        };

#define INTERVAL_HEADER(x)  ((unsigned)IntervalGranularity::x << 6)
#define TIMESTAMP_HEADER(x) ((unsigned)TimestampGranularity::x << 5)


#define TIME_NANOS_PER_MILLISECOND INT64_C(1000000)
#define TIME_NANOS_PER_SECOND (TIME_NANOS_PER_MILLISECOND * 1000)
#define TIME_NANOS_PER_HOUR (TIME_NANOS_PER_SECOND * 3600)


        // almost -70 years IN NANOSECONDS
#define COMPRESSED_TIMESTAMP_BASE_NS  (INT64_C(-70) * 366 * 24 * 3600000 * 1000000)
#define COMPRESSED_TIMESTAMP_BASE_MS  (COMPRESSED_TIMESTAMP_BASE_NS / 1000000)

        // Amount of bytes (except header), taken by compressed timestamps
        static const byte compressed_timestamp_size[8] = {
            0,
            5,
            4,
            3,
            2,
            0,
            7,
            8
        };
    } // namespace TDB
} // namespace DxApiImpl
