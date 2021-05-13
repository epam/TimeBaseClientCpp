#pragma once

#include "streams.h"

namespace DxApiImpl {
    class IOStreamTCP : public IOStream {
    public:
        virtual size_t read(byte *to, size_t minSize, size_t maxSize) override;
        virtual size_t tryRead(byte *to, size_t maxSize) override;
        virtual size_t tryRead(byte *to, size_t maxSize, uint64_t timeout_us) override;

        virtual size_t write(const byte *data, size_t data_size) override;
        INLINE  size_t write(const char *data, size_t data_size)   { return write((const byte *)data, data_size); }
        INLINE  size_t write(const std::string &s)                 { return write((const byte *)s.data(), s.size()); }
        INLINE  size_t write(const char *str)                      { return write((const byte *)str, strlen(str)); }
        virtual void flush() override;

        virtual size_t nBytesAvailable() const override;
        virtual uint64 nBytesRead() const override;
        virtual uint64 nBytesWritten() const override;

        virtual bool isInputClosed() const override;
        virtual bool isOutputClosed() const override;
        virtual bool isClosed() const override;

        virtual void closeInput() override;
        virtual void closeOutput() override;

        virtual void close() override;
        virtual void abort() override;
        virtual void setDisconnectionCallback(DisconnectionCallback callback, void *arg);

        void closesocket();

        IOStreamTCP(SOCKET s);
        virtual ~IOStreamTCP();

    private:
        size_t tryReadImpl(byte *to, size_t maxSize, timeval &timeout);
        bool invokeDisconnectionCallback(const char * text, bool isRead);
        bool invokeDisconnectionCallback(const std::string &text, bool isRead);

    private:
        SOCKET socket_;
        uint64 nBytesRead_;
        uint64 nBytesWritten_;
        DisconnectionCallback disconnectionCallback_;
        void * disconnectionCallbackArg_;
        bool closed_, inputClosed_, outputClosed_;
    };
}