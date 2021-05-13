#include "tickdb/common.h"
#include "util/critical_section.h"
#include <exception>
#include <ctime>

#if DX_PLATFORM_WINDOWS
#include "windows/resource.h"
#include "windows/winplat_impl.h"
#include "sockets_impl.h"
#endif


#if !defined(_MSC_VER)
#define _open open
#define _close close
#define _write write
#define _strdup strdup

#ifndef O_TEXT
#define O_TEXT 0
#endif

#endif


NOINLINE size_t strlen_ni(const char *s)
{
    return strlen(s);
}


NOINLINE int strcmp_ni(const char *str_a, const char * str_b)
{
    return strcmp(str_a, str_b);
}


NOINLINE void * memset_ni(void *data, int value, size_t size)
{
    return memset(data, value, size);
}

static bool strhasprefix_impl(const char *s, const char *prefix, size_t l1, size_t l2)
{
    if (NULL == s || NULL == prefix) {
        return false;
    }

    if (l1 < l2) {
        return false;
    }

    return s == prefix || 0 == memcmp(s, prefix, l2);
}


bool strhasprefix(const char *s, const char *prefix)
{
    return strhasprefix_impl(s, prefix, strlen(s), strlen(prefix));
}


bool strhasprefix(const char *s, size_t string_length, const char *prefix, size_t prefixlength)
{
    return strhasprefix_impl(s, prefix, string_length, prefixlength);
}


bool strhasprefix(const char *s, const char *prefix, size_t prefixlength)
{
    return strhasprefix_impl(s, prefix, strlen(s), prefixlength);
}


// Based on stbiw__crc32 from stb_image_write by Sean Barrett
uint32_t crc32(const uint8_t *buffer, size_t len)
{
    static uint32_t crc_table[256];

    if (0 == crc_table[1]) {
        forn (i, 256) {
            uint32_t crc = (uint32_t)i;
            forn (j, 8) {
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            }

            crc_table[i] = crc;
        }
    }

    uint32_t crc = ~0u;
    buffer += len;
    for_neg (i, -(intptr_t)len) {
        crc = (crc >> 8) ^ crc_table[buffer[i] ^ (crc & 0xFF)];
    }

    return ~crc;
}


// Split source string into up to n strings, using the specified separator character, by copying them into an array of std::string
// Returns the number of strings

size_t split(std::string strings[], size_t nstrings, const char *src, char separator)
{
    if (NULL == src || NULL == strings || nstrings <= 0)
        return 0;

    assert(strings->data() != src);

    intptr i;
    for(i = 1; i != nstrings; ++i) {
        const char * p = strchr(src, (unsigned char)separator);
        if (NULL == p) 
            break;

        (strings++)->assign(src, p);
        src = p + 1;
    }

    strings->assign(src);

    return i;
}


// In-place replace 
NOINLINE void replace_all(char *s, size_t n, const char *from, const char *to)
{
    if ('\0' != s[n]) {
        throw std::logic_error("replace_all - string must be 0-terminated!");
    }

    size_t len = strlen_ni(from);
    if (strlen_ni(to) != len) {
        throw std::invalid_argument("replace: length(from) != length(to)");
    }

    while (NULL != (s = strstr(s, from))) {
        memcpy(s, to, len);
        s += len;
    }
}


NOINLINE void replace_all(std::string * const dest, const char *src, const char *from, const char *to)
{
    if (NULL == dest || NULL == src || NULL == from || NULL == to) {
        throw std::invalid_argument("one of the arguments is NULL");
    }

    size_t from_len = strlen_ni(from);
    
    dest->clear();
    while ('\0' != (*src)) {
        if (0 == strncmp(src, from, from_len)) {
            dest->append(to);
            src += from_len;
        }
        else {
            dest->push_back(*src++);
        }
    }
}


bool dbg_log_enabled = DBG_LOG_ENABLED;

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
static int dbgfile__ = -1;
static char *log_file_base;

static QPCTimer log_qpc_timer;
static uint64_t log_timer_base_us;


// High-performance unix time in microseconds
static uint64_t timestamp_us()
{
    static dx_thread::critical_section section;

    if (0 == log_timer_base_us) {
        dx_thread::yield_lock lock(section);
        log_qpc_timer.init(0);
        log_timer_base_us = time_us() - log_qpc_timer.getMicros();
    }

    return log_qpc_timer.getMicros() + log_timer_base_us;
}


static struct tm* localtime(uint64_t timestampSeconds)
{
    time_t timeint = timestampSeconds;
    return localtime(&timeint);
}


static struct tm* gmtime(uint64_t timestampSeconds)
{
    time_t timeint = timestampSeconds;
    return gmtime(&timeint);
}


INLINE static struct tm* localtime_from_us(uint64_t timestampMks)
{
    return localtime(timestampMks / 1000000U);
}


INLINE static struct tm* gmtime_from_us(uint64_t timestampMks)
{
    return gmtime(timestampMks / 1000000U);
}


INLINE static struct tm * gmtime_from_ms(uint64_t timestampMs)
{
    return gmtime(timestampMs / 1000U);
}


INLINE static struct tm * gmtime_from_ns(uint64_t timestampNs)
{
    return gmtime(timestampNs / 1000000000U);
}


static unsigned dbg_get_pid()
{
    return (unsigned)
#if DX_PLATFORM_WINDOWS
    GetCurrentProcessId();
#else
    getpid();
#endif
}


NOINLINE std::string dbg_logfile_path(const char * name, const char * ext)
{
    std::string out;
    static dx_thread::critical_section section;

    if (NULL == log_file_base) {
        dx_thread::yield_lock lock(section);
        char filename[0x800];
        const char *root;
        unsigned pid = dbg_get_pid();

#if DX_PLATFORM_WINDOWS
        root = getenv("DXAPI_LOG_PATH");
        if (NULL == root) {
            root = getenv("TEMP");
        }
#else
        root = ".";
#endif
        if (NULL != root && strlen_ni(root) < sizeof(filename) - 0x20) {
            auto t = localtime_from_us(timestamp_us());
            if (NULL != t) {
                snprintf(filename, sizeof(filename), "%s/" LOG_FILE_NAME_PREFIX "%02d%02d_%02d%02d%02d_%05u_", root,
                t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
                pid);
            }
            else {
                snprintf(filename, sizeof(filename), "%s/" LOG_FILE_NAME_PREFIX "%05u_", root, pid);
            }

            log_file_base = _strdup(filename);
        }

        if (NULL == log_file_base) {
            dbg_log_enabled = false;
        }
    }

    // Do not remove this comparison
    if (NULL != log_file_base) {
        out.append(log_file_base);
        out.append(name);
        out.append(".");
        out.append(ext);
    }

    return out;
}

#define HLINE "------------------------------------------------------------------------------"

NOINLINE static bool dbg_open_logfile(const char * fname, int flags)
{
    if (-1 != dbgfile__) {
        _close(dbgfile__);
    }
    dbgfile__ = _open(fname, O_APPEND | O_TEXT | O_WRONLY | O_TRUNC | flags, 00666);
    if (-1 != dbgfile__) {
#if DX_PLATFORM_WINDOWS 
        char exename[0x1000];
        char libname[0x1000];
        GetModuleFileNameA(NULL, exename, sizeof(exename));
        exename[sizeof(exename) - 1] = '\0';
        libname[0] = '\0';
        GetModuleFileNameA(GetModuleHandleA(VER_INTERNALNAME_STR), libname, sizeof(libname));
        libname[sizeof(libname) - 1] = '\0';
        
        dbg_log(LOG_START_MSG "\n%s\n%s\n%s\n"
            "Version  : %s\n"
            "Built    : " __DATE__ " " __TIME__ "\n"
//            "CLR ver.: " __CLR_VER "\n"
            "MSC ver. : %u\n"
            "PID      : %u\n" HLINE, exename, GetCommandLineA(), libname,
            VER_FILEVERSION_STR " " VER_BUILD_CONFIGURATION_STR, _MSC_FULL_VER,
            dbg_get_pid());

#else 
        dbg_log("Logging started \n" HLINE);
#endif
    }
    return -1 != dbgfile__;
}


NOINLINE static bool auto_configure_logger()
{
    // TODO: remove
#if DX_PLATFORM_WINDOWS
    char path[0x1000], *p;
    size_t len;

    GetModuleFileNameA(NULL, path, sizeof(path) - 0x100);
    len = strlen_ni(path);
    p = path + len - 1;
    // Decrement pointer until start of path or start of the string
    for (; p >= path && *p != '\\' && *p != '/'; --p)
    if (p < path) {
        return false;
    }
    memcpy(++p, LOG_FILE_NAME_PREFIX LOG_FILE_NAME_SUFFIX "." LOG_FILE_EXT, strlen(LOG_FILE_NAME_PREFIX LOG_FILE_NAME_SUFFIX "." LOG_FILE_EXT) + 1);
    return dbg_open_logfile(path, dbg_log_enabled ? O_CREAT : 0);
#else
    return false;
#endif
};


INLINE static void dbg_log(const char * text, uintptr length)
{
    static dx_thread::critical_section section;
    assert(-1 != dbgfile__);
    {
        dx_thread::yield_lock lock(section);
        char timebuf[0x80];
        static bool first_record = true;

        uint64_t microtime = timestamp_us();
        struct tm * tstruct = localtime_from_us(microtime);

        if (NULL != tstruct) {
            if (first_record) {
                first_record = false;
                snprintf(timebuf, sizeof(timebuf), /*"%04d-" */ "%02d-%02d ", /*1900 + tstruct->tm_year, */ tstruct->tm_mon + 1, tstruct->tm_mday);
                _write(dbgfile__, timebuf, 11 - 5);
            }

            snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d.%06llu ", tstruct->tm_hour, tstruct->tm_min, tstruct->tm_sec, (ulonglong)(microtime % 1000000U));
            _write(dbgfile__, timebuf, 16);

        }

        _write(dbgfile__, text, (unsigned)length);
        _write(dbgfile__, "\n", 1);
    }
}

static class Helper123123 {
public:
    Helper123123()
    {
        // First, attempt to open the log file in local directory
        //if (auto_configure_logger()) {
//            dbg_log_enabled = true;
//            return;
//        }

        if (dbg_log_enabled) {
            dbg_log_enabled = dbg_open_logfile(dbg_logfile_path(LOG_FILE_NAME_SUFFIX, LOG_FILE_EXT).c_str(), O_CREAT);
        }
    }

    ~Helper123123()
    {
        if (-1 != dbgfile__) {
            dbg_log("\n--closed--");
            _close(dbgfile__);
        }

        if (log_file_base != NULL) {
            ::free(log_file_base);
            log_file_base = NULL;
        }
    }

} hlp3123;


static INLINE size_t dbg_va_format(char buffer[], const size_t bufferSize, const char *text, va_list p)
{
    int ret;
    // TODO: Check and use the return value, considering the MSVC incompatibilities
    // refer to tinyxml2 implemenation
    ret = vsnprintf(buffer, bufferSize, text, p);
    buffer[bufferSize - 1] = '\0';
    size_t len = strlen(buffer);
    return len;
}


static INLINE size_t dbg_va_format(std::string *out, char buffer[], const size_t buffer_size, const char *text, va_list p)
{
    size_t len = dbg_va_format(buffer, buffer_size, text, p);
    if (NULL != out) {
        out->resize(len);
        memcpy(&(*out)[0], buffer, len);
    }

    return len;
}


static INLINE size_t dbg_va_format(std::string *out, const char *text, va_list p)
{
    char buffer[0x10000];
    return dbg_va_format(out, buffer, sizeof(buffer), text, p);
}


static INLINE void dbg_va_log(std::string *out, const char *text, va_list p)
{
    char buffer[0x10000];
    size_t len = dbg_va_format(out, buffer, sizeof(buffer), text, p);

    dbg_log(buffer, len);
}


NOINLINE void dbg_log(const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    dbg_va_log(NULL, fmt, p);
    va_end(p);
}




NOINLINE void dbg_log(std::string *out, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    dbg_va_log(out, fmt, p);
    va_end(p);
}


NOINLINE void format_string(std::string *out, const char *fmt, ...)
{
    va_list p;
    va_start(p, fmt);
    dbg_va_format(out, fmt, p);
    va_end(p);
}


NOINLINE void dbg_dump(const char *basename, void *data, size_t data_size)
{
    static dx_thread::critical_section section;
    static uint64_t counter;
    char name[0x400];

    {
        dx_thread::yield_lock lock(section);
        snprintf(name, sizeof(name), "%s-%06llu", basename, (ulonglong)counter++);
        name[sizeof(name) - 1] = '\0';
        std::string path = dbg_logfile_path(name, "bin");
        FILE *f = fopen(path.c_str(), "wb");
        if (NULL != f) {
            fwrite(data, 1, data_size, f);
            fclose(f);
        }
    }
}


// TODO: move to utils or something

bool load_file(std::vector<uint8_t> &out, const char *filename)
{
    FILE *f = fopen((const char*)filename, "rb");
    if (NULL == f) {
        dbg_log("ERROR: Unable to open file: %s\n", filename);
        return false;
    }

    /* Read the whole file contents into buffer */
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    out.resize(size);

    if (size >= SIZE_MAX) {
        fclose(f);
        dbg_log("ERROR: File %s is too big : %llu bytes\n", filename, (ulonglong)size);
        return false;
    }

    rewind(f);
    size_t sizeread = fread(&out[0], 1, size, f);
    fclose(f);

    if (size != sizeread) {
        out.resize(sizeread);
        dbg_log("ERROR: Unable to read file: %s, %llu/%llu read\n", filename,
            (ulonglong)sizeread, (ulonglong)size);

        return false;
    }

    return true;
}


/**
 * Format error message to a string buffer from system-dependent eror code.
 * For non-windows systems errno is expected
 * will always 0-terminate (last character written is '\0', but not included in the returned length)
 * so, the returned value is [0 .. dst_size-1]
 */
size_t format_error(char *dst, size_t dst_size, int error)
{
    size_t n_appended;

    // dst_size expected to be 1..0xFFFFFFFF
    if (dst_size - 1 >= UINT32_MAX)
        return 0;

#ifdef _WIN32
    n_appended = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        dst, (DWORD)(dst_size - 1), NULL);

    /* Can return 0 on error, but that's ok, we return empty line then, instead of trying
       to format error message for the function that formats error message
    */

#else
    const char *strerr = strerror(error);
    n_appended = strlen(strerr);
    if (n_appended >= dst_size) {
        n_appended = dst_size - 1;
    }

    memcpy(dst, strerr, n_appended);
#endif

    assert(n_appended < dst_size);
    dst[n_appended] = '\0';
    return n_appended;
}


size_t format_error(std::string *out, size_t max_append_size, int error)
{
    assert(out != NULL);
    size_t start = out->size();
    out->resize(start + max_append_size);
    size_t n_appended = format_error(&(*out)[start], max_append_size, error);
    out->resize(start + n_appended);

    return n_appended;
}


std::string format_socket_error(int error, const char *source)
{
    std::string out;
    char txt[200];

    if (NULL != source) {
        out.append(source).append(": ");
    }

    snprintf(txt, sizeof(txt), "Socket error: #%d - ", error);
    out.append(txt);

    format_error(&out, 0x400, error);

    return out;
}


std::string last_socket_error(int *errorcode, const char *source)
{
#ifdef _WIN32
    int error = WSAGetLastError();
#else
    int error = errno;
#endif
    if (NULL != errorcode) {
        *errorcode = error;
    }

    return format_socket_error(error, source);
}



// TODO: move this
#define TIMESTAMP_NULL INT64_MIN
#define USE_CURRENT_TIME (TIMESTAMP_NULL + 1)
#define USE_CURSOR_TIME (TIMESTAMP_NULL + 2)

const char * timestamp_const_str(int64_t x)
{
    if (TIMESTAMP_NULL == x) {
        return "<*>";
    }
    else if (USE_CURSOR_TIME == x) {
        return "<CURSOR_TIME>";
    }
    else if (USE_CURRENT_TIME == x) {
        return "<CURRENT_TIME>";
    }
    else {
        return NULL;
    }
}

std::string timestamp_cursor2str(int64_t x)
{
    std::string out;
    char timebuf[80];
    const char *cs = timestamp_const_str(x);
    if (NULL != cs) {
        out.assign(cs);
    } 
    else {
        struct tm *tstruct = gmtime_from_us(x * 1000);
        if (NULL != tstruct) {
            snprintf(timebuf, sizeof(timebuf),
                /*"%04d-" */ "%02d-%02d "
                "%02d:%02d:%02d.%03llu",
                /*1900 + tstruct->tm_year, */ tstruct->tm_mon + 1, tstruct->tm_mday,
                tstruct->tm_hour, tstruct->tm_min, tstruct->tm_sec, (ulonglong)(x % 1000U));
            out.assign(timebuf);
        }
        else {
            out.assign("<ERROR>");
        }
    }

    return out;
}


std::string timestamp_ns2str(int64_t x, bool withSeparator)
{
    std::string out;
    char timebuf[80];
    const char *cs = timestamp_const_str(x);
    if (NULL != cs) {
        out.assign(cs);
    }
    else {
        struct tm *tstruct = gmtime_from_ns(x);
        if (NULL != tstruct) {
            static const char *fmtStrings[] = { "%02d%02d%02d'%06llu", "%02d:%02d:%02d.%06llu" };
            snprintf(timebuf, sizeof(timebuf), fmtStrings[withSeparator], tstruct->tm_hour, tstruct->tm_min, tstruct->tm_sec, (ulonglong)(x / 1000U % 1000000U));
            out.assign(timebuf);
        }
        else {
            out.assign("<ERROR>");
        }
    }

    return out;
}


void sleep_ns(int64_t sleep_ns)
{
    if (sleep_ns < 0) {
        sleep_ns = 0;
    }

#if DX_PLATFORM_WINDOWS
    Sleep((int32_t)(sleep_ns / 1000000));
#else 
    struct timespec req;
    req.tv_nsec = sleep_ns % UINT64_C(1000000000);
    req.tv_sec = (time_t)(sleep_ns / UINT64_C(1000000000));
    nanosleep(&req, NULL);
#endif /* #if DX_PLATFORM_WINDOWS */
}


uint64_t time_ns()
{
#if DX_PLATFORM_WINDOWS
    union
    {
        ::FILETIME ft;
        uint64_t ft64;
    } ft;
    GetSystemTimeAsFileTime(&ft.ft);
    return ft.ft64 * UINT64_C(100);
#elif defined(__APPLE__)
    struct timeval system_time;
    if (0 != gettimeofday(&system_time, NULL))
        return 0;

    return system_time.tv_sec * UINT64_C(1000000000) + system_time.tv_usec * UINT64_C(1000);
#else
    // Linux, etc.
    struct timespec system_time;

    clock_gettime(CLOCK_REALTIME, &system_time);
    return system_time.tv_sec * UINT64_C(1000000000) + system_time.tv_nsec;

#endif
}


uint64_t time_us()
{
#if DX_PLATFORM_WINDOWS
    union
    {
        FILETIME ft;
        uint64_t ft64;
    } ft;
    GetSystemTimeAsFileTime(&ft.ft);
    return ft.ft64 / 10;
#elif defined(__APPLE__)
    struct timeval system_time;
    if (0 != gettimeofday(&system_time, NULL))
        return 0;

    return system_time.tv_sec * UINT64_C(1000000) + system_time.tv_usec;
#else
    // Linux, etc.
    struct timespec system_time;

    clock_gettime(CLOCK_REALTIME, &system_time);
    return system_time.tv_sec * UINT64_C(1000000) + system_time.tv_nsec / 1000U;

#endif
}