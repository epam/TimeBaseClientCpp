#pragma once

#include "tickdb/common.h"
#include <string>

namespace DxApiImpl {
    // Those classes are not owned by their users, can only be deleted via their implementation class interface

    class InputStream {
    public:
        virtual size_t read(byte *to, size_t minSize, size_t maxSize) = 0;
        virtual size_t tryRead(byte *to /* can be null */, size_t maxSize /*can be 0*/) = 0;
        virtual size_t tryRead(byte *to, size_t maxSize, uint64_t timeout_us) = 0;
        INLINE  size_t read(char *to, size_t minSize, size_t maxSize) { return read((byte *)to, minSize, maxSize); }
        
        virtual size_t nBytesAvailable() const = 0;
        virtual uint64  nBytesRead() const = 0;

        virtual bool isInputClosed() const = 0;
        virtual bool isClosed() const { return isInputClosed(); }

        // Will not read anything from the stream, resources can be freed
        virtual void closeInput() = 0;
        virtual void close() { return closeInput(); }

        virtual ~InputStream() {}
    };

    class OutputStream {
    public:
        virtual size_t write(const byte *from, size_t size) = 0;
        INLINE  size_t write(const char *from, size_t size)
        {
            assert(NULL != from);
            return write((const byte *)from, size); 
        }

        INLINE  size_t write(const std::string &from)
        {
            return write((const byte *)from.data(), from.size());
        }

        INLINE  size_t write(const char *from)
        {
            return write((const byte *)from, strlen(from)); 
        }

        virtual void flush() = 0;

        virtual uint64 nBytesWritten() const = 0;

        virtual bool isOutputClosed() const = 0;
        virtual bool isClosed() const { return isOutputClosed(); }

        // Will not write anything to the stream, resources can be freed
        virtual void closeOutput() = 0;
        virtual void close() { return closeOutput(); }
        
        virtual ~OutputStream() {}
    };

    class IOStream : public InputStream, public OutputStream {
        static const size_t ILLEGAL_SIZE = ~(size_t)0; // UNUSED for now

    public:
        /**
         * Check size_t return value for error code. Currently UNUSED.
         * @return true if the call that returned size_t was succesful
         * For methods that return size_t, this gives us a way to check returned value for error flag, when we get rid of exceptions.
         */
        static INLINE bool success(size_t returnedSize) { return ILLEGAL_SIZE != returnedSize; }

        typedef bool(*DisconnectionCallback)(IOStream *sender, void *argument, const char *text);

        bool isClosed() const override = 0;

        void close() override = 0;
        virtual void abort() = 0;  // Dirtier version of close. kills connection as fast as possible
        virtual void setDisconnectionCallback(DisconnectionCallback callback, void *arg) = 0;
        virtual ~IOStream() {}
    };


    class IOStreamException : public std::runtime_error {
        
    public:
        INLINE int errorCode() const { return errno_; }
        INLINE IOStreamException(int errn, const char *s) : runtime_error(s), errno_(errn) {}
        INLINE IOStreamException(int errn, const std::string &s) : runtime_error(s), errno_(convertErrorCode(errn)) {}

    private:
        static int convertErrorCode(int errn);
        int errno_;
    };

    class IOStreamDisconnectException : public IOStreamException {
    public:
        IOStreamDisconnectException() : IOStreamException(0, "Input Stream disconnected by server") {}
    };
}

