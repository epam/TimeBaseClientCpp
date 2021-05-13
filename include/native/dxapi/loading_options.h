#pragma once

#include "dxcommon.h"


namespace DxApi {

    struct WriteModeEnum {
        enum Enum {
            APPEND,     /* Adds only new data into a stream without truncation */
            REPLACE,    /* Adds data into a stream and removes previous data older that new time */
            REWRITE,    /* Adds data into a stream and removes previous data by truncating using first new message time */
            TRUNCATE    /* Stream truncated every time when loader writes messages earlier that last */
        };
    };

    ENUM_CLASS(uint8_t, WriteMode);

    class LoadingOptions {
    public:
        WriteMode writeMode;
        bool raw;
        bool minLatency;

    public:
        LoadingOptions() : writeMode(WriteMode::REWRITE), raw(true), minLatency(false) {};
    };
}