#pragma once
#ifndef __DX_PLATFORM_H__
#define __DX_PLATFORM_H__


// Determine target CPU endianness
#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define DX_PLATFORM_LITTLE_ENDIAN 1
    #else
        #define DX_PLATFORM_LITTLE_ENDIAN 0
    #endif
#elif defined(__ENDIAN_LITTLE__)
    #if __ENDIAN_LITTLE__ == 1 
        #define DX_PLATFORM_LITTLE_ENDIAN 1
    #else
        #define DX_PLATFORM_LITTLE_ENDIAN 0
    #endif
#elif defined(__BIG_ENDIAN__) || defined(__BIG_ENDIAN) || defined(_BIG_ENDIAN) || defined(_ARCH_PPC) || defined(__PPC) || defined(__PPC__)
    #define DX_PLATFORM_LITTLE_ENDIAN 0
#else
    // LE system by default
    #define DX_PLATFORM_LITTLE_ENDIAN 1
#endif


// Determine target operating system
#if defined(_WIN32)
/* Windows platform */
#define DX_PLATFORM_WINDOWS 1
#if (_MANAGED == 1) || (_M_CEE == 1)
// Managed windows environment
#define DX_PLATFORM_MANAGED
#define DX_PLATFORM_WINDOWS_MANAGED
#else
// Native windows environment
#define DX_PLATFORM_NATIVE 1
#define DX_PLATFORM_WINDOWS_NATIVE 1
#endif // (_MANAGED == 1) || (_M_CEE == 1)

#if defined(_MSC_VER) && ( _MSC_VER < 1800)
#define strtoull _strtoui64
#define strtoll _strtoi64
//#define isnan _isnan
#endif

#include <intrin.h>

#else // defined(_WIN32)
#define DX_PLATFORM_UNIX 1
#define DX_PLATFORM_NATIVE 1
#endif

#if !defined(_M_X64) /* || TODO: (more platform arch checks) */
#define DX_PLATFORM_32 1
#else
#define DX_PLATFORM_64 1
// const assert for word size
#endif

#if defined(_MSC_VER)
#ifndef INLINE
#define NOINLINE __declspec(noinline)
#if (_MSC_VER >= 1200)
#define INLINE __forceinline
#else
#define INLINE __inline
#endif /* #if (_MSC_VER >= 1200) */
#endif /* #ifndef INLINE */

#else
#ifndef INLINE
#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__ ((noinline))
#endif
#endif


#if DX_PLATFORM_WINDOWS
#pragma comment (lib, "WS2_32.lib")

#define _bswap64(x) _byteswap_uint64(x)
#define _bswap32(x) _byteswap_ulong(x)
#define _bswap16(x) _byteswap_ushort(x)

#else
// !defined(DX_PLATFORM_WINDOWS)
#define _bswap64(x)  __builtin_bswap64(x)
#define _bswap32(x)  __builtin_bswap32(x)
#define _bswap16(x)  __builtin_bswap16(x)

#include <unistd.h>

#endif


// Any platform

#include <cmath>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <vector>

namespace DxApiImpl {
    template <typename P, typename Q> static INLINE const P& _as(const Q &x)
    {
        static_assert(sizeof(P) <= sizeof(Q), "Type sizes must match");
        return *(const P *)(const char *)&x;
    }

    /*
     * Byteswap operation
     */

    template<typename T> INLINE T    _bswap(T x);
    template<> INLINE uint8_t        _bswap<uint8_t>(uint8_t x)     { return x; }
    template<> INLINE int8_t         _bswap<int8_t>(int8_t x)       { return x; }
    template<> INLINE uint16_t       _bswap<uint16_t>(uint16_t x)   { return _bswap16(x); }
    template<> INLINE int16_t        _bswap<int16_t>(int16_t  x)    { return (int16_t)_bswap16((uint16_t)x); }
    template<> INLINE uint32_t       _bswap<uint32_t>(uint32_t x)   { return _bswap32(x); }
    template<> INLINE int32_t        _bswap<int32_t>(int32_t  x)    { return (int32_t)_bswap32((uint32_t)x); }
    template<> INLINE uint64_t       _bswap<uint64_t>(uint64_t x)   { return _bswap64(x); }
    template<> INLINE int64_t        _bswap<int64_t>(int64_t  x)    { return (int64_t)_bswap64((uint64_t)x); }


    /*
     * Unaligned load/store operations
     */

    template<typename T> static INLINE T    _load(const void * ptr)         { return *(const T *)ptr; }
    template<typename T> static INLINE void _store(void * ptr, T x)         { *(T *)ptr = x; }

    // Move value of type T between possibly overlapping unaligned memory locations
    template<typename T> static INLINE void _move(void * dest, const void * src)         { _store<T>(dest, _load<T>(src)); }
    //template<typename T> static INLINE void _move(void * dest, const void * src)         { T tmp = *(const T *)src; *(T *)dest = tmp; }

#if DX_PLATFORM_LITTLE_ENDIAN
    template<typename T> static INLINE T    _toBE(T x)                      { return _bswap<T>(x); }
    template<typename T> static INLINE T    _fromBE(T x)                    { return _bswap<T>(x); }
    template<typename T> static INLINE T    _toLE(T x)                      { return x; } // Does nothing at all
    template<typename T> static INLINE T    _fromLE(T x)                    { return x; }
#else
#error "This library only supports Little-Endian systems (for now)"
    // While it could be defined for big-endian systems, it is mostly pointless, because known BE systems have poor support for misaligned loads/stores
#endif
    template<typename T> static INLINE T    _loadBE(const void * ptr)       { return _fromBE(_load<T>(ptr)); }
    template<typename T> static INLINE void _storeBE(void * ptr, T x)       { _store<T>(ptr, _toBE<T>(x)); }
    template<typename T> static INLINE T    _loadLE(const void * ptr)       { return _fromLE(_load<T>(ptr)); }
    template<typename T> static INLINE void _storeLE(void * const ptr, T x) { _store<T>(ptr, _toLE<T>(x)); }

    template<> INLINE double         _loadBE<double>(const void * ptr)       { uint64_t tmp = _loadBE<uint64_t>(ptr); return _as<double>(tmp); }
    template<> INLINE float          _loadBE<float>(const void * ptr)        { uint32_t tmp = _loadBE<uint32_t>(ptr); return _as<float>(tmp); }
    template<> INLINE void           _storeBE<double>(void * ptr, double x)  { _storeBE<uint64_t>(ptr, _as<uint64_t>(x)); }
    template<> INLINE void           _storeBE<float>(void * ptr, float x)    { _storeBE<uint32_t>(ptr, _as<uint32_t>(x)); }
} // namespace DxApiImpl

#endif // #if defined(__DX_PLATFORM_H__)

#ifndef _DXAPI
#define _DXAPI

// TODO: For debug only, remove later?
#ifdef _DEBUG
//#include "tickdb/common.h"
#endif

#endif