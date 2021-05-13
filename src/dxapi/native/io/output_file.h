#pragma once

#include "streams.h"

namespace DxApiImpl {
    class OutputStreamFileLogger : public OutputStream {
    public:
        virtual size_t write(const byte *data, size_t data_size) override;
        virtual uint64 nBytesWritten() const override;
        bool open(const char * name);

        OutputStreamFileLogger();
        OutputStreamFileLogger(OutputStream *outStream);
        virtual ~OutputStreamFileLogger();

    private:
        FILE            *outputFile;        // We log to this file
        OutputStream    *outputStream;      // We pass calls to this stream
    };
}