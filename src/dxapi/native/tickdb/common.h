#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "config.h"

#ifndef DBG_LOG_ENABLED
#define DBG_LOG_ENABLED 1
#endif
#ifndef LOG_FILENAME
#define LOG_FILENAME "dxapi_log.txt"
#endif

// TODO: MAYBE: support for old Visual Studio versions with broken stdint?
#include "dxapi/dxcommon.h"

#include "platform/platform.h"
#include "util/qpc_timer.h"
#include "util/charbuffer.h"

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include <sstream>
#include <algorithm>
#include <vector>
#include <exception>
// C++11!
#include <type_traits>

namespace DxApiImpl {
    template<typename T> INLINE std::string toString(const T &x);
    template<typename T> std::string toString(const std::vector<T> &x) {
        std::string out;
        const char * sep = "";
        for (auto &i : x) {
            out.append(sep).append(toString(i));
            sep = ", ";
        }

        return out;
    }

    template<> std::string INLINE toString<bool>(const bool &x)                         { return x ? "true" : "false"; }
    template<> std::string INLINE toString<std::string>(const std::string &x)           { return x; }
    template<> std::string INLINE toString<const char *>(const char * const &x)         { return std::string(x); }


    
    

    /*template<> std::string INLINE toString<DxApi::Nullable<std::string>>(const DxApi::Nullable<std::string> &x)
    {
        return x.is_null() ? std::string("<NULL>") : x.get(); 
    }*/

    template<typename T> std::string  INLINE toString(const T &x)
    {
        std::stringstream ss;
        ss << x;
        return ss.str();
    }

//#include "util/logger.h"
}


#ifdef _countof
#define COUNTOF(arr) _countof(arr)
#else
#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

// number of bits used to hold a type, assuming byte is 8 bits
#define BITSIZEOF(x) (8 * sizeof(x))

// Fill POD structure with 0
#define DX_CLEAR(What) memset(&(What), 0, sizeof(What) + 0 * 1/(int)(sizeof(What) != sizeof(void *)))
// Fill POD structure with 0, using non-inlined memset call
#define DX_CLEAR_NI(What) memset_ni(&(What), 0, sizeof(What) + 0 * 1/(int)(sizeof(What) != sizeof(void *)))

    /**
    * @brief Rounds down the given value to the alignment boundary.
    */
#define DX_ALIGN(Value, Alignment)                                                    \
    ((uintptr_t)(Value) & ~((uintptr_t)(Alignment) - 1))

    /**
    * @brief Rounds down the given value to the sizeof of specific type
    */
#define DX_ALIGN_TO(Value, Type)                                                      \
    ((uintptr_t)(Value) & ~((uintptr_t)sizeof(Type) - 1))

    /**
    * @brief Rounds up the given value to the alignment boundary.
    */
#define DX_ALIGN_UP(Value, Alignment)                                                      \
    (((uintptr_t)(Value) + ((uintptr_t)(Alignment) - 1)) & ~((uintptr_t)(Alignment) - 1))

    /**
    * @brief Rounds down the given pointer to the alignment boundary.
    */
#define DX_POINTER_ALIGN(Pointer, Alignment)                                          \
    ((void *)DX_ALIGN(Pointer, Alignment))

    /**
    * @brief Rounds up the given pointer to the alignment boundary.
    */
#define DX_POINTER_ALIGN_UP(Pointer, Alignment)                                            \
    ((void *)DX_ALIGN_UP(Pointer, Alignment))

// generate bitmask with N low bits set (0..01..1)
#define BITMASK_L(num_low_bits) ((uint64_t)(-1) >> (64 - (num_low_bits)))

// generate bitmask with N high bits set (1..10..0)
#define BITMASK_H(num_high_bits) ((uint64_t)(-1) << (64 - (num_high_bits)))

// generate bitmask with N * 8 low bits set (0x00..00FF..FF)
#define BYTEMASK_L(num_low_bytes) BITMASK_L((num_low_bytes) * 8)

// generate bitmask with N * 8 high bits set (0xFF..FF00..00)
#define BYTEMASK_H(num_high_bytes) BITMASK_H((num_high_bytes) * 8)


    // Effective read buffer size slightly smaller than 4 pages so the last read doesn't go into a new one
#define READ_BUFFER_SIZE (0x4000 - 8 * 2)
    // Buffer size to actually allocate 
#define READ_BUFFER_ALLOC_SIZE (READ_BUFFER_SIZE + 8 * 2/*front&back guard areas*/ + 0xFFF/* page alignment */ + 128 /* cache line - sized guard area*/)

#define WRITE_BUFFER_ALLOC_SIZE (MAX_MESSAGE_SIZE + 0xFFF + 8 * 2 /* for chunk header and guard area */ + 128 /* extra cache line */)

#define CRLF "\r\n"
#define CRLFCRLF "\r\n\r\n"

#define forn(i, n) for (intptr_t i = 0, i##_count = (intptr_t)(n); i != i##_count; ++i)

// Iterate through range [n .. 0), n assumed to be <=0
#define for_neg(i, n) assert(n <= 0); for (intptr_t i = (n); 0 != i; ++i)

template<typename T> T INLINE align(T x, uintptr_t y) { return (T)DX_ALIGN(x, y); }

// Non-inlined library functions, to be used for code size savings where necessary
size_t strlen_ni(const char * str);
int strcmp_ni(const char * str0, const char * str1);
void * memset_ni(void * data, int value, size_t size);


#if defined(_MSC_VER) && (_MSC_VER < 1900 )
// Workaround for snprintf on VS2013 or less. _MSC_VER 1900 = Toolset 14
// NOTE:  snprintf and _snprintf are not fully compatible though this doesn't matter in our case
#define snprintf _snprintf
#endif

#if !defined(_MSC_VER) && !DX_PLATFORM_64
#define DXAPI_CALLBACK
#else
#define DXAPI_CALLBACK __stdcall
#endif


static int memcmp_impl(const void * a, const void * b, size_t asize, size_t bsize)
{
    return asize < bsize ? -1 : asize > bsize ? 1 : 0 == asize ? 0 : memcmp(a, b, asize);
}

/**
 * Compare 2 strings by directly comparing bytes of the content
 */
template<typename T> INLINE static int memcmp(const std::basic_string<T> &a, const std::basic_string<T> &b)
{
    return memcmp_impl(a.data(), b.data(), a.size() * sizeof(T), b.size() * sizeof(T));
}

/**
 * Compare 2 arrays by directly comparing bytes of the content
 */
template<typename T> INLINE static int memcmp(const std::vector<T> &a, const std::vector<T> &b)
{
    return memcmp_impl(a.data(), b.data(), a.size() * sizeof(T), b.size() * sizeof(T));
}


/**
 * Calculate CRC32 of a buffer
 */
uint32_t crc32(const uint8_t * buffer, uintptr_t len);

INLINE uint32_t crc32(const std::vector<uint8_t> &buffer)   { return crc32(buffer.data(), buffer.size()); }
INLINE uint32_t crc32(const std::string &buffer)            { return crc32((const uint8_t *)buffer.data(), buffer.size()); }

/**
 * Return true if string s has specified prefix
 */

bool strhasprefix(const char * s, const char * prefix);
bool strhasprefix(const char * s, const char * prefix, size_t prefixlength);
bool strhasprefix(const char * s, size_t string_length, const char * prefix, size_t prefixlength);

/**
 * Split source string into up to n strings, using the specified separator character, by copying them into an array of std::string
 * Returns the number of strings created
 */

size_t split(std::string strings[], size_t nstrings, const char * src, char separator);

extern bool dbg_log_enabled;

std::string dbg_logfile_path(const char * name, const char * ext);

void dbg_log(const char * fmt, ...);

// Log data to file, and, optionally, put it into out std::string. 'out' parameter is nullable
void dbg_log(std::string * out, const char * fmt, ...);

void dbg_dump(const char * basename, void * data, size_t nBytes);

void format_string(std::string * out, const char * fmt, ...);

void dbg_save(const char * filename);

bool load_file(std::vector<uint8_t> &out, const char * filename);
bool save_file(const std::vector<uint8_t> &in, const char * filename);

// TODO: move
std::string timestamp_cursor2str(int64_t x);
std::string timestamp_ns2str(int64_t x, bool withSeparator);
#define TS_CURSOR2STR(x) (timestamp_cursor2str(x).c_str())
#define TS_NS2STR(x) (timestamp_ns2str(x, true).c_str())

// Return vector.data() for non-empty vectors or dummy non-NULL pointer for empty vectors
template<typename T> INLINE static const T * data_ptr(const std::vector<T> &v) { auto p = v.data();  return NULL != p ? p : (const T *)(0x10); }

static_assert(sizeof(long long unsigned) == sizeof(uint64_t), "ulonglong != uint64");
static_assert(sizeof(unsigned) == sizeof(uint32_t), "uint != uint32");

#define FMT_HEX64 "0x%016llX"
#define FMT_HEX32 "0x%08X"

#if defined(DX_PLATFORM_64)
#define FMT_PTR FMT_HEX64
#else
#define FMT_PTR FMT_HEX32
#endif



// In Visual Studio __FUNCTION__ gives function name with enclosing namespaces
// __func__ is char[] identifier

#ifdef _MSC_VER
#define _FUNCTION_ __FUNCTION__

#define COMMA_VA_ARGS(...) , __VA_ARGS__
#else
// gcc, clang, etc.
#define _FUNCTION_ __PRETTY_FUNCTION__
//#define _FUNCTION_ __func__
#define COMMA_VA_ARGS(...) , ##__VA_ARGS__

#endif


#if DBG_LOG_ENABLED != 1
#define DBGLOG (void)

// Still format te mesasage into outpput string, but do not log this string.
#define DBGLOGERR(OUT, FMT, ...) format_string(OUT, FMT " at %s() in " __FILE__ ":%u" COMMA_VA_ARGS(__VA_ARGS__), _FUNCTION_, __LINE__)

//#define dx_assert(_EXPR, ...) assert(_EXPR)

#define dx_assert(_EXPR, FMT, ...) (void)((!!(_EXPR)) \
    || (DBG_IS_DEBUGGER_PRESENT() ? (DBG_BREAK(), 1) : 0) \
    || (_wassert(_CRT_WIDE(#_EXPR), _CRT_WIDE(__FILE__), __LINE__), 0))

#else
#define DBGLOG if (dbg_log_enabled) dbg_log
#define DBGLOGERR(OUT, FMT, ...) if (dbg_log_enabled) dbg_log(OUT, FMT " at %s() in " __FILE__ ":%u" COMMA_VA_ARGS(__VA_ARGS__), _FUNCTION_, __LINE__)

#ifdef _MSC_VER
#define dx_assert(_EXPR, FMT, ...) (void)((!!(_EXPR)) \
    || (dbg_log_enabled ? (dbg_log(FMT " ASSERT:(%s) at %s() in " __FILE__ ":%u" COMMA_VA_ARGS(__VA_ARGS__), #_EXPR, _FUNCTION_, __LINE__), 0) : 0 ) \
    || (DBG_IS_DEBUGGER_PRESENT() ? (DBG_BREAK(), 1) : 0) \
    || (_wassert(_CRT_WIDE(#_EXPR), _CRT_WIDE(__FILE__), __LINE__), 0))
#else
#define dx_assert(_EXPR, FMT, ...) (void)((!!(_EXPR)) \
    || (dbg_log_enabled ? (dbg_log(FMT " ASSERT:(%s) at %s() in " __FILE__ ":%u" COMMA_VA_ARGS(__VA_ARGS__), #_EXPR, _FUNCTION_, __LINE__), 0) : 0 ) \
    || (DBG_IS_DEBUGGER_PRESENT() ? (DBG_BREAK(), 1) : 0) \
    || (assert(_EXPR), 0))
#endif

#endif

#define THROW_DBGLOG_EX(EXC, FMT, ...) do { \
    std::string out; DBGLOGERR(&out, FMT, __VA_ARGS__); \
    throw EXC(out); \
} while (0)

#define THROW_DBGLOG_IO(EXC, ERR, FMT, ...) do { \
    std::string out; DBGLOGERR(&out, FMT, __VA_ARGS__); \
    throw EXC(ERR, out); \
} while (0)

#define THROW_DBGLOG(FMT, ...) THROW_DBGLOG_EX(std::runtime_error, FMT, __VA_ARGS__)

#ifdef NDEBUG
#define _wassert (void)
#endif


/**
 * Common macro to dispatch event handler callback from native impl. Callbacks may throw exceptions which will be caught and logged
 */

#define DXAPIIMPL_DISPATCH_THROW() throw;
// #define DXAPIIMPL_DISPATCH_THROW 

#define DXAPIIMPL_DISPATCH_LOG() do { DBGLOG("%s", errmsg.c_str()); puts(errmsg.c_str()); } while(0)


// Dispatch event handler callback, with appropriate logging etc.
#define DXAPIIMPL_DISPATCH(from, callback, parms, textid) do { try {                                        \
        auto cb = callback;                                                                                 \
        if (NULL != cb) {                                                                                   \
            DBGLOG_VERBOSE(LOGHDR "." #from "(): dispatch " #callback " STARTED", textid);                  \
            cb parms;                                                                                       \
            DBGLOG_VERBOSE(LOGHDR "." #from "(): dispatch " #callback " FINISHED", textid);                 \
                        }                                                                                   \
    }                                                                                                       \
catch (const std::exception &e) {                                                                           \
    std::string errmsg;                                                                                     \
    format_string(&errmsg, LOGHDR "." #from "(): dispatch " #callback " ERROR: std::exception: %s", textid, e.what());                         \
    DXAPIIMPL_DISPATCH_LOG();                                                                               \
    DXAPIIMPL_DISPATCH_THROW();                                                                             \
}                                                                                                           \
catch (...) {                                                                                               \
    std::string errmsg;                                                                                     \
    format_string(&errmsg, LOGHDR "." #from "(): dispatch " #callback " ERROR: system exception!", textid); \
    DXAPIIMPL_DISPATCH_LOG();                                                                               \
    DXAPIIMPL_DISPATCH_THROW();                                                                             \
}                                                                                                           \
} while (0)


/**
 * In-place substring replacement. Replace all occurences of <from> with <to>, length should match. Case sensitive.
 */
void replace_all(char * s, size_t s_size, const char * from, const char * to);
INLINE void replace_all(std::string * const s, const char * from, const char * to)
{ 
    if (NULL == s) throw std::invalid_argument("replace destination is NULL");
    return replace_all(&(*s)[0], s->size(), from, to);
} // This is very hacky, but works

/**
 * Copy src to dest while replacing one substring with another. Case sensitive.
 */
void replace_all(std::string * const dest, const char * src, const char * from, const char * to);
INLINE void replace_all(std::string * const dest, const std::string &src, const char * from, const char * to)
{
    return replace_all(dest, src.c_str(), from, to);
}


/**
* Fill contents of stl array with 0 without resizing it 
*/
template <typename T> INLINE void clear(std::vector<T> &v)
{
    size_t l = v.size();
    if (0 != l) {
        memset(&v[0], 0, l);
    }
}

INLINE void clear(std::string &v)
{
    size_t l = v.size();
    if (0 != l) {
        memset(&v[0], 0, l);
    }
}

INLINE void safe_clear(std::string &v)
{
    for (auto &i : v) (volatile char &)i = 0;
}

/**
* Scramble contents of a buffer by XOR-ing it with simple LCG
*/
template <typename T> INLINE void scramble(T * v, size_t size)
{
    uint32_t x = (uint32_t)(size * 9 + 103);
    forn (i, size) {
        x = x * 1664525U + 1013904223U;
        v[i] ^= (T)(x >> 16);
    }
}

template <typename T> INLINE void scramble(std::vector<T> &v)
{
    size_t l = v.size();
    if (0 != l) scramble(&v[0], l);
}


INLINE void scramble(std::string &v)
{
    size_t l = v.size();
    if (0 != l) scramble(&v[0], l);
}


// Clears itself before deletion
class secure_string : public std::string {
public:
    INLINE secure_string() : std::string() {}
    INLINE secure_string(const std::string &s) : std::string(s) {}
    INLINE ~secure_string() { safe_clear(*this); resize(0); }
};



/* Range checking helpers, for doing explicit unsigned checks */
static INLINE bool below(unsigned a, unsigned b) { return a < b; }
static INLINE bool below(uint64_t a, uint64_t b) { return a < b; }


/* true if a <= x < b */
static INLINE bool in_range(uint32_t x, uint32_t a, uint32_t b) { return below((x - a), (b - a)); }
static INLINE bool in_range(int32_t x, int32_t a, int32_t b)    { return below((uint32_t)(x - a), (uint32_t)(b - a)); }
static INLINE bool in_range(uint64_t x, uint64_t a, uint64_t b) { return below((x - a), (b - a)); }
static INLINE bool in_range(int64_t x, int64_t a, int64_t b)    { return below((uint64_t)(x - a), (uint64_t)(b - a)); }


// Static cast to unsigned version of type T
//template<class T> INLINE make_unsigned<T>::type unsigned_cast(T x) { typedef make_unsigned<T>::type Q; return static_cast<Q>(x); }
//template<class T> INLINE make_signed<T>::type signed_cast(T x) { return static_cast<make_signed<T>::type>(x); }

// check for divisibility of signed integer by unsigned constant
template<typename P, typename Q> static INLINE bool fast_divisible_by(P divided, Q divisor)
{
    typedef typename std::make_unsigned<P>::type U_P;
    typedef typename std::make_unsigned<Q>::type U_Q;
    assert(divisor > 0 && "divisor must be > 0");
    return 0 == (divided + (((U_P)(-1) / 2) / U_Q(divisor)) * divisor) % (U_Q)divisor;
}

// divide signed integer by unsigned constant with flooring, instead of truncating towards 0
template<typename P, typename Q> static INLINE P fast_divide_by(P divided, Q divisor)
{
    typedef typename std::make_unsigned<P>::type U_P;
    typedef typename std::make_unsigned<Q>::type U_Q;
    assert(divisor > 0 && "divisor must be > 0");
    return (P)((divided + (((U_P)(-1) / 2) / U_Q(divisor)) * divisor) / (U_Q)divisor - ((U_P)(-1) / 2) / U_Q(divisor));
}


template<typename INTTYPE /* underlying type */, class ENUM /* 'enum' class type */, unsigned N, const char ** typeInfo, bool HAS_FROM_CHAR> struct EnumHelper {
    INLINE ENUM fromChar(int x) const
    {
        return HAS_FROM_CHAR ? 
            ((ENUM)(0x40 == (x & ~0x3F) ? fromAsciiChar64[(uintptr)(unsigned)x - 0x40] : ~(INTTYPE)0))
            : (ENUM)x;
    }

    INLINE char toChar(const ENUM &x) const
    {
        return HAS_FROM_CHAR ?
            (x < N ? *typeInfo[(unsigned)x] : '\0')
            : x;
    }

    INLINE const char * toString(ENUM x) const
    {
        return x < N ? typeInfo[(unsigned)x] + 2 * (int)HAS_FROM_CHAR : NULL;
    }

    INTTYPE fromString(const char s[]) const
    {
        for (intptr i = 0; i < N; ++i) {
            if (0 == strcmp(typeInfo[i] + 2 * (int)HAS_FROM_CHAR, s)) {
                return (INTTYPE)i;
            }
        }

        return ~(INTTYPE)0;
    }

    EnumHelper()
    {
        if (HAS_FROM_CHAR) {
            memset_ni(fromAsciiChar64, ~0, sizeof(fromAsciiChar64));
            for (intptr i = N - 1; i >= 0; --i) {
                fromAsciiChar64[*typeInfo[i] - 0x40] = (INTTYPE)i;
            }
        }
    }

private:
    INTTYPE fromAsciiChar64[1 + 63 * (int)HAS_FROM_CHAR];
};


#define DX_ENUM_HELPER(INTTYPE, NS, ENUM, HAS_FROM_CHAR) EnumHelper<INTTYPE, NS ENUM::Enum, COUNTOF(info##ENUM), info##ENUM, HAS_FROM_CHAR>

#if 0
#define DX_ENUM_HELPER_IMPL(INTTYPE, NS, CLASS, HAS_FROM_CHAR)                                                    \
    { CLASS##EnumHelper(); } ___g_enumHelper##CLASS;                                                            \
                                                                                                                \
    template<> NS CLASS::EnumClass(const char s[]) :      value_(___g_enumHelper##CLASS.fromString(s)) {}       \
    template<> NS CLASS       NS CLASS::fromChar(int x)   { return ___g_enumHelper##CLASS.fromChar(x); }        \
    template<> char    NS CLASS::toChar() const           { return ___g_enumHelper##CLASS.toChar(*this); }      \
    template<> const char* NS CLASS::toString() const     { return ___g_enumHelper##CLASS.toString(*this); }    \
    /* Constructor */ CLASS##EnumHelper::CLASS##EnumHelper()
#endif

#if 0
#define DX_ENUM_HELPER_IMPL(INTTYPE, NS, CLASS, HAS_FROM_CHAR)                                                    \
    { CLASS##EnumHelper(); } ___g_enumHelper##CLASS;                                                            \
                                                                                                                \
    NS CLASS::EnumClass(const char s[]) :      value_(___g_enumHelper##CLASS.fromString(s)) {}       \
    NS CLASS       NS CLASS::fromChar(int x)   { return ___g_enumHelper##CLASS.fromChar(x); }        \
    char    NS CLASS::toChar() const           { return ___g_enumHelper##CLASS.toChar(*this); }      \
    const char* NS CLASS::toString() const     { return ___g_enumHelper##CLASS.toString(*this); }    \
    /* Constructor */ CLASS##EnumHelper::CLASS##EnumHelper()

#else
#define DX_ENUM_HELPER_IMPL(INTTYPE, NS, ENUM, HAS_FROM_CHAR)                                                   \
    { ENUM##EnumHelper(); } ___g_enumHelper##ENUM;                                                              \
                                                                                                                \
    NS ENUM::ENUM(const char s[]) : EnumClass((Enum)___g_enumHelper##ENUM.fromString(s)) {}                     \
    NS ENUM       NS ENUM::fromChar(int x)   { return ___g_enumHelper##ENUM.fromChar(x); }                      \
    char    NS ENUM::toChar() const           { return ___g_enumHelper##ENUM.toChar(*this); }                   \
    const char* NS ENUM::toString() const     { return ___g_enumHelper##ENUM.toString(*this); }                 \
                                                                                                                \
    /* Constructor for the helper */ ENUM##EnumHelper::ENUM##EnumHelper()

#endif


// This macro takes enum underlying type, enum name, boolean flag if support for toChar/fromChar is necessary (changes infoXX format)
#define IMPLEMENT_ENUM(INTTYPE, ENUM, HAS_FROM_CHAR)                                                     \
static struct ENUM##EnumHelper : DX_ENUM_HELPER(INTTYPE,, ENUM, HAS_FROM_CHAR)                           \
DX_ENUM_HELPER_IMPL(INTTYPE,, ENUM, HAS_FROM_CHAR)


#define IMPLEMENT_CLASS_ENUM(INTTYPE, NS, ENUM, HAS_FROM_CHAR)                                           \
static struct ENUM##EnumHelper : DX_ENUM_HELPER(INTTYPE, NS::, ENUM, HAS_FROM_CHAR)                     \
DX_ENUM_HELPER_IMPL(INNER, NS::, ENUM, HAS_FROM_CHAR)


// Defines a set of functions for accessing internal implementation class
#define DEFINE_IMPL_CLASS(EXTERNAL, INTERNAL)                                                               \
    static INLINE const INTERNAL* impl(const EXTERNAL *x) { return static_cast<const INTERNAL*>(x); }    \
    static INLINE const INTERNAL& impl(const EXTERNAL &x) { return static_cast<const INTERNAL&>(x); }       \
    static INLINE INTERNAL* impl(EXTERNAL *x) { return static_cast<INTERNAL*>(x); }                      \
    static INLINE INTERNAL& impl(EXTERNAL &x) { return static_cast<INTERNAL&>(x); }


#include "protocol.h"

#define _DXAPI