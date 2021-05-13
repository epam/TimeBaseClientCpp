#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <math.h>

#include "dxapi/schema.h"
#include "tickdb/data_reader_impl.h"

#include <assert.h>


#ifdef _DEBUG
#define GET_FROM_STREAM_DBG_VER true
#else
#define GET_FROM_STREAM_DBG_VER false
#endif


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;


/**
 * Input Stream data reading
 */

// Reads portion of data from the input stream object. Handles buffer layer and input stream layer
// SKIP means that size bytes after from are to be ignored, no matter if already in buffer or yet to be read from the input
template<bool SKIP> const byte* DataReaderBaseImpl::getFromStream(const byte *from, size_t size)
{
    assert((intptr)size > 0);

    const byte *dataEnd = streamDataEnd;
    const byte * const bufferEnd = this->bufferEnd; // cache read-only field
    size_t      nBytesRemaining = dataEnd - from;
    size_t      nFree;

    assert(from <= dataEnd);
    assert(nBytesRemaining <= (size_t)(bufferEnd - bufferStart));
    assert(size <= (size_t)(bufferEnd - bufferStart) && " requested size is too big?");

    if (size <= nBytesRemaining) {
        return from;
    }

    size -= nBytesRemaining;

    assert(dataEnd <= bufferEnd);

    // We take any chance to reset read pointer to the start of the buffer (unless debugging "buffer full" code)
    if (SKIP || (0 == nBytesRemaining && !GET_FROM_STREAM_DBG_VER)) {
        nFree = bufferEnd - (dataPtr = dataEnd = bufferStart);
        from = dataEnd - nBytesRemaining; // Can go < bufferStart in skip mode
    }
    else {
        // We append to existing data only if there's some useful bytes left in the buffer
        nFree = bufferEnd - dataEnd; // can overflow but this is taken into account later
        if (size > nFree) {
            byte * const bufferStart = const_cast<byte *>(this->bufferStart);  // cache read-only field
            // No space left, forced to shift the remaining data to the start of the buffer
            _move<uint64>(bufferStart, from); // Copying 8b even if nBytesRemaining = 0, because why not?
            if (nBytesRemaining > sizeof(uint64)) {
                memmove(bufferStart + sizeof(uint64), from + sizeof(uint64), nBytesRemaining - sizeof(uint64));
            }

            dataEnd = (dataPtr = from = bufferStart) + nBytesRemaining;
            nFree = bufferEnd - dataEnd;
            assert(dataPtr + size <= bufferEnd && "after compacting, data pointer is still outside, probably the data element too big to fit into the buffer");
        }
    }

    size_t nBytesRead = s->read(const_cast<byte *>(dataEnd), size, nFree);
    assert(nBytesRead >= size);
    // Save new end pointer and zero-terminate
    *const_cast<byte *>(dataEnd += nBytesRead) = '\0';
    streamDataEnd = dataEnd;
    return from;
}


template const byte* DataReaderBaseImpl::getFromStream<false>(const byte *from, size_t size);
template const byte* DataReaderBaseImpl::getFromStream<true>(const byte *from, size_t size);


// CRLF + 1 + digit + CRLF
#define CHUNK_HDR_SIZE (2 + 1 + 2)
template <bool SKIP> const byte* DataReaderHTTPchunked::getChunked(const byte * from, size_t size)
{
    const byte  *pHeaderStart, *pHeaderEnd;
    const byte  *pEnd;
    size_t      chunkSize;
    size_t      chunkHeaderLength;

    // In this mode, dataEnd is either the end of the data actually read, or the end of chunk
    assert(from <= dataEnd);
    assert(from + size > dataEnd);
    assert(dataEnd <= bufferEnd);

    size_t tailSize = chunkEnd - from; // Number of bytes remaining before the end of the chunk, Now we can forget about chunkend pointer for a while

    if (tailSize >= size) {
        // Not reached the end of the chunk yet
        dx_assert(streamDataEnd == dataEnd, "");
        dx_assert(streamDataEnd <= chunkEnd, "");
        from = DataReaderBaseImpl::getFromStream<SKIP>(from, size);
        chunkEnd = from + tailSize;
        dataEnd = std::min(chunkEnd, streamDataEnd);
        return from;
    }

    do {
        assert(tailSize < size);
        chunkHeaderLength = 2 + 1 + 2;

        // Okay, we need to read the chunk header within our data block
    again:
        from = DataReaderBaseImpl::getFromStream<false>(from, size + chunkHeaderLength);
        pHeaderStart = from + tailSize;
        pEnd = NULL;
        chunkSize = strtoul((const char *)pHeaderStart + 2, (char **)&pEnd, 0x10); // Size of the next chunk
        pHeaderEnd = pEnd + 2;
        chunkHeaderLength = (pHeaderEnd - pHeaderStart);
        if (((pHeaderStart >= bufferStart) && ('\r' != pHeaderStart[0] || '\n' != pHeaderStart[1])) || NULL == pEnd || pHeaderEnd <= pHeaderStart || chunkHeaderLength > 7 + 4) {
            // Maximum chunk size we support: 0xFFFFFFF
            THROW_DBGLOG_EX(TickCursorError, "ERR: Malformed HTTP chunk header detected");
        }

        // Check, if chunk length is terminated properly
        if ('\r' != pHeaderEnd[-2]) {
            if ('\0' == pHeaderEnd[-2]) {
                // We must reparse chunk length again, probably more digits will be added
                goto again;
            }
            THROW_DBGLOG_EX(TickCursorError, "ERR: Malformed input detected while reading next HTTP chunk length (by the way, chunk extension field is not supported)!");
        }

        // we could miss a byte from the chunk header, so reread remaining data to be sure
        from = DataReaderBaseImpl::getFromStream<false>(from, size + chunkHeaderLength);
        pHeaderStart = from + tailSize;
        pHeaderEnd = pHeaderStart + chunkHeaderLength;

        // Do we already have more than 8 bytes of data read from the next chunk in the buffer?
        if (streamDataEnd > pHeaderEnd + sizeof(uint64)) {
            // If yes, we will move previously unconsumed data forward, replacing chunk header bytes
            // We are moving the data forward, not back, because we assume that the size of the remaining data is small and the amount of data we fetched from the buffer is bigger
            assert(tailSize >= 0);
            if (0 != tailSize && !SKIP) {
                if (tailSize <= sizeof(uint64)) {
                    _move<uint64>((void *)(pHeaderEnd - sizeof(uint64)), pHeaderStart - sizeof(uint64));
                }
                else {
                    memmove(const_cast<byte *>(pHeaderEnd - tailSize), from, tailSize);
                }
            }
            from += chunkHeaderLength;
        }
        else {
            assert(streamDataEnd >= from + size + chunkHeaderLength);
            // If no (<=8 bytes of new data is read), we simply won't increment dataPtr to skip the chunk header. We'll overwrite the chunk header instead
            _move<uint64>((void*)pHeaderStart, pHeaderEnd);
            pHeaderEnd -= chunkHeaderLength;
            streamDataEnd -= chunkHeaderLength;
        }
        chunkEnd = pHeaderEnd + chunkSize;

    } while ((tailSize = chunkEnd - from) < size);

    dataEnd = std::min(streamDataEnd, chunkEnd);
    return from;
}


const byte* DataReaderHTTPchunked::get(const byte *from, size_t size)
{
    return getChunked<false>(from, size);
}


void DataReaderHTTPchunked::skip(const byte *from, size_t size)
{
    dataPtr = getChunked<true>(from, size) + size;
}


// TODO: replace with do<OP>()
// enum class Operation {
//      SKIP,
//      GET,
//      PEEK

const byte* DataReaderMsgHTTPchunked::get(const byte *pData, size_t size)
{
    if (pData + size <= contentEnd) {
        const byte *pDataNew = DataReaderHTTPchunked::getChunked<false>(pData, size);
        contentEnd += pDataNew - pData;
        dataEnd0 = dataEnd;
        dataEnd = std::min(contentEnd, dataEnd);
        return pDataNew;
    }
    else {
        THROW_DBGLOG("Read beyond end of a message by %llu bytes", (ulonglong)(pData + size - contentEnd));
    }
}


void DataReaderMsgHTTPchunked::skip(const byte *pData, size_t size)
{
    if (pData + size <= contentEnd) {
        size_t remaining = contentEnd - pData - size;
        contentEnd = (dataPtr = DataReaderHTTPchunked::getChunked<true>(pData, size) + size) + remaining;
        dataEnd0 = dataEnd;
        dataEnd = std::min(contentEnd, dataEnd);
    }
    else {
        THROW_DBGLOG("Read beyond end of a message by %llu bytes", (ulonglong)(pData + size - contentEnd));
    }
}
