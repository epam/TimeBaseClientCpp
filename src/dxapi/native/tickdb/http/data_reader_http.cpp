/*
 * Copyright 2021 EPAM Systems, Inc
 *
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
#include "tickdb/common.h"
#include <dxapi/dxapi.h>

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

typedef uint8_t u8;

/**
 * Input Stream data reading
 */

// Reads portion of data from the input stream object. Handles buffer layer and input stream layer
const byte* DataReaderBaseImpl::getFromStream(const byte *from, size_t sizeNeeded)
{
    assert((intptr)sizeNeeded > 0);

    const byte *dataEnd = streamDataEnd;
    const byte * const bufferEnd = this->bufferEnd; // cache read-only field
    size_t      nBytesRemaining = dataEnd - from;
    size_t      nFree;

    assert(from <= dataEnd);
    assert(dataEnd <= bufferEnd);
    assert(nBytesRemaining <= (size_t)(bufferEnd - bufferStart));
    assert(sizeNeeded <= (size_t)(bufferEnd - bufferStart) && " requested size is too big?");

    if (sizeNeeded <= nBytesRemaining)
        return from;

    sizeNeeded -= nBytesRemaining;

    // We take any chance to reset read pointer to the start of the buffer (unless debugging "buffer full" code)
    if (0 == nBytesRemaining && !GET_FROM_STREAM_DBG_VER) {
        nFree = bufferEnd - (dataPtr = dataEnd = from = bufferStart);
    } else {
        // We append to existing data only if there's some useful bytes left in the buffer
        nFree = bufferEnd - dataEnd; // can overflow but this is taken into account later
        if (sizeNeeded > nFree) {
            byte * const bufferStart = (byte *)this->bufferStart;  // cache read-only field
            // No space left, forced to shift the remaining data to the start of the buffer
            _move<uint64>(bufferStart, from); // Copying 8b even if nBytesRemaining = 0, because why not?
            if (nBytesRemaining > sizeof(uint64)) {
                memmove(bufferStart + sizeof(uint64), from + sizeof(uint64), nBytesRemaining - sizeof(uint64));
            }

            dataEnd = (dataPtr = from = bufferStart) + nBytesRemaining;
            nFree = bufferEnd - dataEnd;
            // Should be impossible, unless a bug is caught
            assert(dataPtr + sizeNeeded <= bufferEnd
                   && "after compacting, data pointer is still outside, probably the data element too big to fit into the buffer");
        }
    }

    size_t nBytesRead = s->read((u8 *)dataEnd, sizeNeeded, nFree);
    assert(nBytesRead >= sizeNeeded);
    // Save new end pointer and zero-terminate
    *(u8 *)(dataEnd += nBytesRead) = 0;
    streamDataEnd = dataEnd;
    return from;
}


// TODO: Optimize with inlineable part and return dataPtr

// SKIP means that size bytes after from are to be ignored, no matter if already in buffer or yet to be read from the input
void DataReaderBaseImpl::skipStream(const u8 *from, size_t size)
{
    assert((ssize_t)size >= 0);
    const byte *dataEnd = streamDataEnd;
    size_t      nBytesRemaining = dataEnd - from;
    assert(from <= dataEnd);
    assert(nBytesRemaining <= (size_t)(bufferEnd - bufferStart));
    if (size <= nBytesRemaining) {
        dataPtr = from + size;
        return;
    }

    // buffer is empty
    size -= nBytesRemaining;
    from = bufferStart;
    size_t bufferSize = (size_t)(bufferEnd - from);
    for (; size > bufferSize; size -= bufferSize) {
        size_t szRead = s->read((u8 *)from, bufferSize, bufferSize);
        assert(bufferSize == szRead);
    }

    assert(size > 0);
    size_t szRead = s->read((byte *)from, size, bufferSize);
    assert(szRead >= size);
    dataPtr = from + size;
    streamDataEnd = from += szRead;
    *(u8 *)from = 0; // Always zero-terminate data to avoid accidental overruns in string functions.
}


// CRLF + 1 + digit + CRLF
#define CHUNK_HEADER_SIZE_MIN (2 + 1 + 2)
#define CHUNK_HEADER_SIZE_MAX (2 + 7 + 2)
#define CHUNK_SIZE_MAX 0xFFFFFFFU

static const u8 * parseChunkHeader(const u8 *pStart, const u8 *p, int *chunkSize)
{
    *chunkSize = -1;
    if ((size_t)(p - pStart) < CHUNK_HEADER_SIZE_MIN || ((pStart[0] - 0xD) | (pStart[1] - 0xA)))
        return NULL;

    ptrdiff_t i = pStart - p + 2;
    unsigned ch;
    unsigned x = 1U << (8 * sizeof(unsigned) - 1);
    while (1) {
        ch = p[i] - '0';
        int t = ch < 10 ? ch : ch - 0x11 < 6 ? ch - 7 : ch - 0x31 < 6 ? ch - 0x27 : -1;
        if (t < 0) {
            if (ch != 0xDU - '0')
                return NULL;

            // Success only if there's CRLF present
            if (++i != 0) {
                if ((ch = p[i]) == 0xA) {
                    *chunkSize = x;
                    assert(x <= CHUNK_SIZE_MAX);
                    return p + i + 1;
                }

                return NULL;
            }

            *chunkSize = x;
            return NULL;
        }

        x = (x << 4) | t;
        if (++i == 0 || x > CHUNK_SIZE_MAX) {
            *chunkSize = x;
            return NULL;
        }
    }
}

template<bool SKIP> const byte* DataReaderHTTPchunked::getOrSkipChunked(const byte * from, size_t size)
{
    // In this mode, dataEnd is either the end of the data actually read, or the end of chunk
    assert(from <= dataEnd);
    assert(from + size > dataEnd);
    assert(dataEnd <= bufferEnd);

    assert(dataEnd == streamDataEnd || dataEnd == chunkEnd);

    // Number of bytes remaining before the end of the chunk, Now we can forget about chunkEnd pointer for a while
    size_t remainingSize = chunkEnd - from;
    if (remainingSize >= size) {
        // Not reached the end of the chunk yet
        dx_assert(streamDataEnd == dataEnd, "");
        dx_assert(streamDataEnd <= chunkEnd, "");
        if (SKIP) {
            DataReaderBaseImpl::skipStream(from, size);
            chunkEnd = (from = dataPtr) + (remainingSize - size);
        } else {
            chunkEnd = (from = DataReaderBaseImpl::getFromStream(from, size)) + remainingSize;
        }

        assert(chunkEnd >= dataPtr);
        dataEnd = std::min(chunkEnd, streamDataEnd);
        return from;
    }

    do {
        assert(remainingSize < size);
        if (SKIP) {
            DataReaderBaseImpl::skipStream(from, remainingSize);
            size -= remainingSize;
            from = dataPtr;
            remainingSize = 0;
        }

        size_t   chunkHeaderLength = CHUNK_HEADER_SIZE_MIN;
        const u8 *headerStart, *headerEnd;
        int nextSize;
        while (1) {
            if (!SKIP) {
                assert(size + CHUNK_HEADER_SIZE_MAX < (size_t)(bufferEnd - bufferStart) && " requested size is too big?");
                // Read the chunk header within our data block
                from = DataReaderBaseImpl::getFromStream(from, size + chunkHeaderLength);
                assert(streamDataEnd >= from + size + chunkHeaderLength);
            } else {
                assert(from < bufferEnd - CHUNK_HEADER_SIZE_MAX - 1);
                from = DataReaderBaseImpl::getFromStream(from, std::min(size + chunkHeaderLength, (size_t)0x400)); // TODO: Constant
            }

            // Try read size of the next chunk and the data bytes afterwards
            headerEnd = parseChunkHeader(headerStart = from + remainingSize, streamDataEnd, &nextSize);
            if (NULL != headerEnd)
                break;

            if ((unsigned)nextSize > CHUNK_SIZE_MAX)
                THROW_DBGLOG_EX(TickCursorError, "ERR: Malformed input detected while reading next HTTP chunk length"
                                " (by the way, chunk extension field is not supported)!");

            // In rare cases, we must reparse chunk length again, probably more digits or CRLF characters will be added
            // Assuming we need at least 1 more byte of chunk header and 1 byte of requested data after it, can increment by 2
            chunkHeaderLength += 2;
        }

        assert((unsigned)nextSize <= CHUNK_SIZE_MAX);
        chunkHeaderLength = headerEnd - headerStart;
        if (!SKIP) {
            // NOTE: We can end up with several payload bytes less in the buffer than expected. Fixed later, before return.
            assert(from >= bufferStart && streamDataEnd - from >= CHUNK_HEADER_SIZE_MIN + size);
            // Do we already have more than 8 bytes of data read from the next chunk in the buffer?
            if (streamDataEnd > headerEnd + sizeof(uint64)) {
                // If yes, we will move previously unconsumed data forward, replacing chunk header bytes
                // We are moving the data forward, not back, because we assume that the size of the remaining data is small
                // and the amount of data we fetched from the buffer is bigger
                if (0 != remainingSize) {
                    if (remainingSize <= sizeof(uint64)) {
                        _move<uint64>((u8 *)(headerEnd - sizeof(uint64)), headerStart - sizeof(uint64));
                    } else {
                        memmove((u8 *)(headerEnd - remainingSize), from, remainingSize);
                    }
                }

                from += chunkHeaderLength;
            }
            else {
                // TODO: Can instead skip the header by adjusting skip amount!!!!!!
                // If no (<=8 bytes of new data is read), we simply won't increment dataPtr to skip the chunk header.
                // We'll overwrite the chunk header instead
                _move<uint64>((u8 *)headerStart, headerEnd);
                streamDataEnd -= chunkHeaderLength;
            }
        } else { // SKIP
            assert(from >= bufferStart);
            from += chunkHeaderLength;
        }

        assert(from <= streamDataEnd);
        remainingSize += (unsigned)nextSize; // add chunk size
    } while (remainingSize < size);
    if (SKIP) {
        DataReaderBaseImpl::skipStream(from, size);
        from = dataPtr;
        remainingSize -= size;
    } else {
        // Retry to possibly read the remaining bytes after header growth. (we could have few bytes less in the buffer than what size expects)
        from = DataReaderBaseImpl::getFromStream(from, size);
    }

    dataEnd = std::min(streamDataEnd, chunkEnd = from + remainingSize);
    assert(SKIP || from + size <= dataEnd);
    return from;
}


const byte* DataReaderHTTPchunked::get(const byte *from, size_t size)
{
    return getOrSkipChunked<false>(from, size);
}


void DataReaderHTTPchunked::skip(const byte *from, size_t size)
{
    getOrSkipChunked<true>(from, size);
}


// TODO: replace with do<OP>()
// enum class Operation {
//      SKIP,
//      GET,
//      PEEK

const byte* DataReaderMsgHTTPchunked::get(const byte *pData, size_t size)
{
    if (pData + size <= contentEnd) {
        const byte *pDataNew = DataReaderHTTPchunked::getOrSkipChunked<false>(pData, size);
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
        DataReaderHTTPchunked::getOrSkipChunked<true>(pData, size);
        contentEnd += dataPtr - (pData + size);
        dataEnd0 = dataEnd;
        dataEnd = std::min(contentEnd, dataEnd);
    }
    else {
        THROW_DBGLOG("Read/skip beyond end of a message by %llu bytes", (ulonglong)(pData + size - contentEnd));
    }
}
