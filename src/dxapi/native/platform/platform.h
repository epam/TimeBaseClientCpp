#pragma once

// We inherit all definitions from the public platform header
#include "dxapi/dxplatform.h"


/* Windows platform includes */

#if DX_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "windows/winplat.h"
#include <io.h>
#include <intrin.h>

#define _rdtsc()  __rdtsc()
#define _CDECL __cdecl

#define DBG_IS_DEBUGGER_PRESENT() IsDebuggerPresent()
#define DBG_BREAK() __debugbreak()
#define DBG_DEBUG_IF_DEBUGGER_PRESENT0() (void)(DBG_IS_DEBUGGER_PRESENT() ? (DBG_BREAK(), 0) : 0)
#define DBG_DEBUG_IF_DEBUGGER_PRESENT() (void)(DBG_DEBUG_IF_DEBUGGER_PRESENT0())

#else
typedef int SOCKET;
#define INVALID_SOCKET (~(SOCKET)0)
#define SOCKET_ERROR   (-1)

#define DBG_IS_DEBUGGER_PRESENT() 0
#define DBG_BREAK() __builtin_trap()
#define DBG_DEBUG_IF_DEBUGGER_PRESENT() (void)(0)
#endif

/* OSX */
#if defined(__APPLE__)
#include <sys/time.h>
#endif

/* GNU CC */
#ifdef __GNUC__
#ifndef _strcmpi
#define _strcmpi strcasecmp
#endif
#define _CDECL __attribute__((cdecl))
#define _rdtsc() 1/0
#endif

typedef uintptr_t   uintptr;
typedef intptr_t    intptr;
typedef uint8_t     byte;
typedef int8_t      int8;
typedef uint16_t    uint16;
typedef int16_t     int16;
typedef uint32_t    uint32;
typedef int32_t     int32;
typedef uint64_t    uint64;
typedef int64_t     int64;
typedef signed long long longlong;
typedef unsigned long long ulonglong;


/*
* Search the mask data from LSB to the MSB for a set bit. Returns the bit position of the first
* bit set (starting from 1) or 0 if the mask is zero.
*/

#if DX_PLATFORM_64
static INLINE unsigned _bsf64(uint64_t x)
{
#if DX_PLATFORM_WINDOWS
    DWORD index;
    return _BitScanForward64(&index, x) ? (index + 1) : 0;
#else   /* #if DX_PLATFORM_WINDOWS */
    return __builtin_ffsll(Mask);
#endif  /* #if DX_PLATFORM_WINDOWS */
}
#endif

static INLINE unsigned _bsf32(uint32_t x)
{
#if DX_PLATFORM_WINDOWS
    DWORD index;
    return _BitScanForward(&index, x) ? (index + 1) : 0;
#else   /* #if DX_PLATFORM_WINDOWS */
    return __builtin_ffs(x); // TODO: verify
#endif  /* #if DX_PLATFORM_WINDOWS */
}


/*
* Search the mask data from MSB to the LSB for a set bit. Returns the bit position of the first
* bit set (starting from 1) or 0 if the mask is zero.
*/

#if DX_PLATFORM_64
static INLINE unsigned _bsr64(uint64_t x)
{
#if DX_PLATFORM_WINDOWS
    DWORD index;
    return _BitScanReverse64(&index, x) ? (index + 1) : 0;
#else   /* #if DX_PLATFORM_WINDOWS */
    return 0 == x ? 0 : 64 - __builtin_clzll(Mask);
#endif  /* #if DX_PLATFORM_WINDOWS */
}
#endif

static INLINE unsigned _bsr32(uint32_t x)
{
#if DX_PLATFORM_WINDOWS
    DWORD index;
    return _BitScanReverse(&index, x) ? (index + 1) : 0;
#else   /* #if DX_PLATFORM_WINDOWS */
    return 0 == x ? 0 : 32 - __builtin_clz(x); // TODO: verify
#endif  /* #if DX_PLATFORM_WINDOWS */
}

namespace DxApiImpl {

    template<typename T> static INLINE unsigned _bsr(T x);
    template<typename T> static INLINE unsigned _bsf(T x);
    
    template<> INLINE unsigned           _bsr<uint32_t>(uint32_t x) { return _bsr32(x); }
    template<> INLINE unsigned           _bsf<uint32_t>(uint32_t x) { return _bsf32(x); }

#ifdef DX_PLATFORM_64
    template<> INLINE unsigned           _bsf<uint64_t>(uint64_t x) { return _bsf64(x); }
    template<> INLINE unsigned           _bsr<uint64_t>(uint64_t x) { return _bsr64(x); }
#endif
}


/**
 * Format error message to a string buffer from system-dependent eror code.
 * For non-windows systems errno is expected
 * will always 0-terminate (last character written is '\0', but not included in the returned length)
 * so, the returned value is [0 .. dst_size-1]
 */
size_t format_error(char *dst, size_t dst_size, int error);

/**
 * Same as previous, max_append_size must be a sane temporary buffer size
 * out can never be NULL, pointer is only specified for readability
 */
size_t format_error(std::string *out, size_t max_append_size, int error);

/**
 * Format socket error from the specified error code
 * source - optionally prepend this text to the error message
 */
std::string format_socket_error(int error, const char *source = NULL);

/**
 * Format socket error
 * errorcode - optionally store error code to here
 * source - optionally prepend this text to the error message
 */
std::string last_socket_error(int *errorcode = NULL, const char *source = NULL);


void sleep_ns(int64_t sleep_ns);

// Return 64-bit timestamp, relative to unspecified reference point, for measuring time periods with more or less decent precision
uint64_t time_ns();

uint64_t time_us();

static void spin_wait_ns(uint64_t wait_ns)
{
    wait_ns += time_ns();
    while ((time_ns() - wait_ns) > UINT64_C(1000000000000))
    {
    }
}

static void spin_wait_ms(uint64_t wait_ms) { spin_wait_ns(wait_ms * 1000000); }


INLINE static double time_s()
{
    return 1E-9 * time_ns();
}