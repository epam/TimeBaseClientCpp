#define _CRT_SECURE_NO_WARNINGS

#include "tickdb/common.h"

#include "input_preloader.h"
#include "output_preloader.h"
#include "output_file.h"
#include "streams.h"

// TODO: Maybe move to platform-dependent file
#include "platform/sockets_impl.h"

using namespace std;
using namespace DxApiImpl;


size_t InputStreamPreloader::read(byte *to, size_t minSize, size_t maxSize)
{
    if (closed_) {
        THROW_DBGLOG_IO(IOStreamException, ENOTCONN,  "InputStreamPreloader::read(): ERR: closed");
    }

    assert(NULL != to);
    assert(minSize <= maxSize);

    size_t remaining = dataSize_ - readPtr_;
    size_t sizeRead = std::min(remaining, maxSize);
    if (sizeRead) {
        memcpy(to, data_.data() + readPtr_, sizeRead);
    }

    readPtr_ += sizeRead;
    if (minSize > sizeRead) {
        InputStream * sourceStream = sourceStream_;
        if (NULL == sourceStream) {
            // Preloader without source stream throws disconnection exception when read is attempted beyond the end of the buffer
            throw IOStreamDisconnectException();
        }
        sizeRead += sourceStream->read(to + sizeRead, minSize - sizeRead, maxSize - sizeRead);
    }

    nBytesRead_ += sizeRead;
    return sizeRead;
}


size_t InputStreamPreloader::tryRead(byte *to, size_t maxSize, uint64_t timeout_us)
{
    return tryRead(to, maxSize);
}


size_t InputStreamPreloader::tryRead(byte *to, size_t maxSize)
{
    size_t remaining = dataSize_ - readPtr_;

    if (closed_) {
        THROW_DBGLOG_IO(IOStreamException, ENOTCONN, "InputStreamPreloader::tryRead(): ERR: closed");
    }

    if (0 != remaining) {
        if (0 == maxSize) {
            return  maxSize;
        }

        if (NULL == to) {
            THROW_DBGLOG("InputStreamPreloader::tryRead(): 'to' parameter is NULL with non-zero maxSize = %llu", (ulonglong)maxSize);
        }

        size_t sizeRead = std::min(remaining, maxSize);

        memcpy(to, data_.data() + readPtr_, sizeRead);
        readPtr_ += sizeRead;
        return sizeRead;
    }

    // No data remaining in the buffer
    InputStream *sourceStream = sourceStream_;
    if (NULL == sourceStream) {
        // Preloader without source stream throws disconnection exception when read is attempted beyond the end of the buffer
        throw IOStreamDisconnectException();
    } else {
        THROW_DBGLOG("InputStreamPreloader::tryRead(): 'to' parameter is NULL with non-zero maxSize = %llu", (ulonglong)maxSize);
    }

    // No return here, unreachable
}


size_t InputStreamPreloader::nBytesAvailable() const
{
    size_t remaining = dataSize_ - readPtr_;
    InputStream *sourceStream = sourceStream_;
    return 0 != remaining ? remaining : !closed_ && NULL != sourceStream ? sourceStream->nBytesAvailable() : 0;
}


uint64 InputStreamPreloader::nBytesRead() const
{
    return nBytesRead_;
}


InputStreamPreloader::InputStreamPreloader(InputStream *source, size_t readBlockSize)
: readPtr_(0), dataSize_(0), readBlockSize_(readBlockSize ? readBlockSize : 0x2000), sourceStream_(source), nBytesRead_(NULL != source ? source->nBytesRead() : 0), closed_(false) 
{
}


void InputStreamPreloader::cleanup()
{
    if (0 != readPtr_) {
        byte *p = data_.data();
        // Normalize buffer
        size_t newSize = data_.size() - readPtr_;
        memmove(p, p + readPtr_, newSize);
        readPtr_ = 0;
        data_.resize(dataSize_ = newSize);
    }
}


InputStreamPreloader::~InputStreamPreloader()
{
    DBGLOG("Preloader deleted, buffer size was: %llu", (ulonglong)data_.size());
}


void InputStreamPreloader::set(size_t startOffset, const void *srcData, size_t nBytes)
{
    if (NULL == srcData) {
        THROW_DBGLOG("Source buffer is NULL");
    }

    data_.resize(dataSize_ = startOffset + nBytes);
    memcpy(&data_[startOffset], srcData, nBytes);
}


size_t InputStreamPreloader::dbg_subst(size_t startOffset, size_t nBytes, const char *fileName)
{
    FILE *f = fopen(fileName, "rb");
    if (NULL == f) {
        THROW_DBGLOG("Unable to open substitution file");
    }

    if (0 == nBytes) {
        fseek(f, 0, SEEK_END);
        nBytes = (size_t)(unsigned long)ftell(f);
        fseek(f, 0, SEEK_SET);
    }

    data_.resize(dataSize_ = startOffset + nBytes);
    if (nBytes != fread(&data_[startOffset], 1, nBytes, f)) {
        THROW_DBGLOG("Unable to read substitution file completely");
    }

    fclose(f);
    dbg_log("Input data substitution done");
    return  nBytes;
}


size_t InputStreamPreloader::dbg_preload_from_file(const char *fileName, size_t nBytes)
{
    cleanup();
    size_t sizeBefore = data_.size(); // size before appending the new data
    return dbg_subst(sizeBefore, nBytes, fileName);

}


void InputStreamPreloader::dbg_dump(size_t startOffset, size_t nBytes, bool dumpHex, bool dumpUnchunked)
{
    char basename[200];
    char name[200];
    FILE *f;
    const byte *data    = &data_[startOffset];
    const byte *end     = data + nBytes;
    uint64 time_ms      = time_ns() / TIME_NANOS_PER_MILLISECOND;

    sprintf(basename, "preloaded@%llu", (ulonglong)time_ms);
    sprintf(name, "%s.bin", basename);

    f = fopen(name, "wb");
    fwrite(data, 1, nBytes, f);
    fclose(f);
    dbg_log("Saved dump of preload buffer");

    if (dumpUnchunked) {
        // Use external unchunking code for now
#if 0
        vector<byte> out;

        sprintf(name, "%s_unc.bin", basename);
        f = fopen(name, "wb");
        //puts(name);

        const byte * p = data;
        while (p < end - 1) {
            char * endptr;
            
            ulonglong length = strtoull((const char *)p, &endptr, 0x10);
            assert(NULL != endptr);
            assert(endptr[0] == '\xD');
            assert(endptr[1] == '\xA');
            if (0 == length) break;
            p = (byte *)endptr + 2;

            size_t inext = p - data_.data() + length;
            if (inext >= i) {
                inext = i;
                length = inext - (p - data_.data());
                out.resize(out.size() + length);
                memcpy(&out[out.size() - length], p, length);
                break;
            }
            else {
                out.resize(out.size() + length);
                memcpy(&out[out.size() - length], p, length);
            }

            p += length;
            assert(p[0] == '\xD');
            assert(p[1] == '\xA');
            p += 2;
            i = p - data_.data();
        }

        fwrite(out.data(), out.size(), 1, f);

        fclose(f);
        dbg_log("Saved dump of preload buffer without HTTP chunks");
#endif
    }

    if (dumpHex) {
        sprintf(name, "%s.txt", basename);
        f = fopen(name, "wt");
        forn(i, end - data) {
            // Printf a hex value with optional EOL character after every 16 values
            fprintf(f, "%02X%c", data[i], " \n"[0xF == (i & 0xF)]);
        }

        fclose(f);
        dbg_log("Saved dump of preload buffer in hex text format");
    }
}


// Append n bytes from the source stream to the buffer
size_t InputStreamPreloader::preload(size_t nBytesToPreload)
{
    cleanup();
    size_t sizeBefore = data_.size(); // size before appending the new data

    // Injecting input data stream that causes bug to occur, recorded on Konstantin's PC.
#if 0 && defined(_DEBUG)
    
    return dbg_subst(0, 0, "preloaded@13091649379172.bin");
#endif

    if (NULL == sourceStream_)
        return 0;

    data_.resize(dataSize_ = sizeBefore + nBytesToPreload);
    byte *writePtr = &data_[sizeBefore];

    size_t nBytesLeft = nBytesToPreload;
    size_t readBlockSize = readBlockSize_;

    try {
        while (0 != nBytesLeft) {
            size_t n = std::min(nBytesLeft, readBlockSize);
            // We read anything from 1 byte up, this is important
            /*if (0 == sourceStream_->nBytesAvailable()) {
                break;
            }*/

            n = sourceStream_->read(writePtr, 1, n);

            writePtr += n;
            nBytesLeft -= n;
            //printf(" %d ", (int)n);
        }
    }
    catch (IOStreamDisconnectException &) {
        data_.resize(dataSize_ = writePtr - &data_[sizeBefore]);
        // Do nothing
    }

    // Uncomment to dump preloaded data to disk
#if 0 && defined(_DEBUG)
    dbg_dump(sizeBefore, nBytesToPreload - nBytesLeft, false, true);
#endif

    return nBytesToPreload - nBytesLeft;
}


void InputStreamPreloader::closeInput()
{
    closed_ = true;
    readPtr_ = dataSize_;
    if (NULL != sourceStream_) {
        sourceStream_->closeInput();
    }
}


bool InputStreamPreloader::isInputClosed() const
{
    return closed_;
}


size_t OutputStreamFileLogger::write(const byte *data, size_t dataSize)
{
    if (NULL != outputFile) {
        fwrite(data, 1, dataSize, outputFile);
    }
    return NULL != outputStream ? outputStream->write(data, dataSize) : dataSize;
}


uint64 OutputStreamFileLogger::nBytesWritten() const
{
    return outputStream->nBytesWritten();
};


OutputStreamFileLogger::OutputStreamFileLogger(OutputStream *outStream) :  outputFile(NULL), outputStream(outStream)
{ }


bool OutputStreamFileLogger::open(const char *name)
{
    outputFile = fopen(name, "wb");
    return NULL == outputFile;
}


OutputStreamFileLogger::OutputStreamFileLogger() : outputFile(NULL), outputStream(NULL)
{ }


OutputStreamFileLogger::~OutputStreamFileLogger()
{
    if (NULL != outputFile) {
        fclose(outputFile);
    }
    DBGLOG("Logger stream deleted");
}



void OutputStreamPreloader::closeOutput()
{

}


// Convert to errno
int IOStreamException::convertErrorCode(int errn)
{
#ifdef DX_PLATFORM_WINDOWS
    // some WSA error codes -10000 match unix errno, but don't match windows errno. So, 1 by 1 conversion needed
    switch (errn) {
        case WSAETIMEDOUT:      return ETIMEDOUT;
        case WSAECONNREFUSED:   return ECONNREFUSED;
        case WSAEHOSTDOWN:      return ECONNREFUSED; // No EHOSTDOWN on Windows
        case WSAEHOSTUNREACH:   return EHOSTUNREACH;
    default:
        return errn;
    }

    /*  TODO: convert other values (but this is currently enough for our purposes)
        Also this will become outdated if we switch to the different socket library
    */
#else
    return errn;
#endif
}