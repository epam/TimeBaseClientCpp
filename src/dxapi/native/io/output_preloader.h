#pragma once

#include "streams.h"

namespace DxApiImpl {
    class OutputStreamPreloader : public OutputStream {
    public:
        virtual size_t      write(const byte *srcData, size_t dataSize) override;
        virtual uint64_t    nBytesWritten() const override;

        bool            open(const char *name);
        virtual void    closeOutput() override;
        INLINE void     close() override { return closeOutput(); }

        OutputStreamPreloader(OutputStream *outStream);
        virtual ~OutputStreamPreloader();

    private:
        FILE            *outputFile;
        OutputStream    *outputStream;
    };
}