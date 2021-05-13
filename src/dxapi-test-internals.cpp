 #define _CRT_SECURE_NO_WARNINGS

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include <memory>

#include "tickdb/http/tickcursor_http.h"
#include "io/input_preloader.h"
#include "util/base64.h"

using namespace std;
using namespace DxApi;
using namespace DxApiImpl;

#define _rdtsc()  __rdtsc()
#define _CDECL __cdecl

static double timetodouble(uint64_t t)
{
    return 1E-9 * t;
}



template<typename T>
static inline std::string toString(const T &x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}


// Effective read buffer size slightly smaller than 4 pages so the last read doesn't go into a new one
#define READ_BUFFER_SIZE (0x4000 - 8 * 2)
// Buffer size to actually allocate 
#define READ_BUFFER_ALLOC_SIZE (READ_BUFFER_SIZE + 8 * 2/*front&back guard areas*/ + 0xFFF/* page alignment */ + 128 /* cache line - sized guard area*/)



// Returns offset of the first difference
uintptr_t test_unchunk(const char * sourceFile, const char * destFile, uintptr minReadSize, uintptr maxReadSize, int method)
{
    // 
    // Allocate buffer (currently using constant size)
    void * bufAllocPtr = malloc(READ_BUFFER_ALLOC_SIZE);
    assert(NULL != bufAllocPtr);

    // Align to 4k page boundary and add 8-byte front guard area
    byte * bufPtr = (byte *)DX_POINTER_ALIGN_UP(bufAllocPtr, 0x1000) + 8;
    InputStreamPreloader preloader(NULL, 0);

    preloader.dbg_preload_from_file(sourceFile, 2);
    preloader.dbg_preload_from_file(sourceFile);
    DataReaderBaseImpl * reader0 = new DataReaderMsg(bufPtr, READ_BUFFER_SIZE, 0, &preloader);
    reader0->getInt16();
    DataReaderBaseImpl * reader = new DataReaderMsgHTTPchunked(std::move(*reader0));
    delete reader0;
    
    vector<byte> dest;
    load_file(dest, destFile);
    intptr_t sz = dest.size();

    vector<byte> tmp;
    tmp.resize(maxReadSize);
    intptr_t i;

    maxReadSize -= minReadSize;
    for (i = 0; i < sz - 20000;) {
        intptr_t readsize = maxReadSize > 0 ? rand() % maxReadSize : 0;
        readsize += minReadSize;
        if (readsize > sz - i) readsize = sz - i;
        if (0x25510 == i) {
            puts("break here");
        }
        switch (method) {
        case 0:
            reader->getBytes(&tmp[0], readsize);
            break;
        case 1:
            memcpy(&tmp[0], reader->bytes(readsize), readsize);
            break;
        }
        
        if (0 != memcmp(&tmp[0], &dest[i], readsize)) {
            fprintf(stderr, "Mismatch at: %lld\n", (longlong)i);
            DBG_BREAK();
        }
        i += readsize;
    }

    delete reader0;
    delete reader;
    return i;
}



bool  test_internals()
{
    try {
        //test_unchunk("D:\\preloaded@13091649361411.bin", "D:\\preloaded@13091649361411u.bin", 8, 8, 1);
        puts(Base64MIME::base64_encode("Aladdin:OpenSesame").c_str());
        assert(std::string("QWxhZGRpbjpPcGVuU2VzYW1l") == Base64MIME::base64_encode("Aladdin:OpenSesame"));
        puts("Internal tests done");
    } 
    catch (exception &e) {
        printf("\nEXCEPTION: %s\n", e.what());
    }

    return true;

}
