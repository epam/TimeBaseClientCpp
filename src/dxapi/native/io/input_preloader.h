#pragma once

#include "tickdb/common.h"
#include "streams.h"

namespace DxApiImpl {
    class InputStreamPreloader : public InputStream {
    public:
        virtual size_t read(byte *to, size_t minSize, size_t maxSize) override;
        virtual size_t tryRead(byte *to /* can be null */, size_t maxSize /* can be 0 */) override;
        virtual size_t tryRead(byte *to, size_t maxSize, uint64_t timeout_us) override;

        virtual size_t nBytesAvailable() const override;
        virtual uint64 nBytesRead() const override;

        virtual bool isInputClosed() const override;

        virtual void closeInput() override;
        size_t preload(size_t nBytesToPreload);
        INLINE InputStream * source() const { return sourceStream_; }

        // Dump input buffer to disk
        void dbg_dump(size_t startOffset, size_t nBytes, bool dumphex, bool dumpUnchunked);

        // Set the buffer from external source
        void set(size_t startOffset, const void *srcData, size_t nBytes);

        // Set the buffer from external source
        template<typename T> INLINE void set(size_t startOffset, std::vector<T> &v) { set(startOffset, (const void  *)v.data(), v.size() * sizeof(T)); }

        // Load file from disk and insert it into the input buffer at specified offset
        size_t dbg_subst(size_t startOffset, size_t nBytes, const char * fileName);

        // Preload input buffer from file
        size_t dbg_preload_from_file(const char *fileName, size_t nBytes = 0);

        // Note: preloader does not own the source stream
        InputStreamPreloader(InputStream *source, size_t readBlockSize = 0x2000);
        virtual ~InputStreamPreloader();
        
    protected:
        void cleanup();

    protected:
        std::vector<byte>   data_;
        size_t              readPtr_;       // current read Pointer
        size_t              dataSize_;
        const size_t        readBlockSize_;
        InputStream         *sourceStream_;
        uint64              nBytesRead_;
        bool                closed_;
    };
}