#pragma once
#ifndef __DEBUG_LOGGER_H__
#define __DEBUG_LOGGER_H__

/*
 logger.h
 header file for debug data logging module
 (c) By Boris Chuprin 2007-2015
*/

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <stdint.h>
#include <stdarg.h>

enum LogErrorLevel {
    _LOG_FULL = 2,
    _LOG_DEBUGMSG = 4, _LOG_NOTIFY = 8, _LOG_WARNING = 12,
    _LOG_ERROR = 16, _LOG_CRITICAL = 20
};


typedef struct tag_LogDesc {
    const char * file;
    const char * fmt;
    unsigned line : 24;
    unsigned level : 8;
    //unsigned binlog:1;
    //unsigned unused:1;
} _LogDesc_;


struct Logger;
extern struct Logger g_logger;

void log_idle(struct Logger * const logger);
void g_log_idle(void);

void log_open(struct Logger * const logger, const char * name, size_t maxsize,
    uint8_t * log_level, unsigned log_n_level);

void g_log_open(const char * name, long maxsize,
    uint8_t * log_level, unsigned log_n_level);

void g_log_set(struct Logger * const logger);
void log_close(struct Logger * const logger);
void g_log_close(void);

// For internal usage and for overloading LOG macros, not for direct use
void dbg_logdata_full(
    struct Logger * const logger, const _LogDesc_ * const l, va_list arg,
    const void * const data, unsigned datalen);

//int __cdecl logdata( /*const char *where,*/ const char *file, int line, const char *fmt, ...);
void __cdecl logdata(struct Logger * const logger, const _LogDesc_ * const l, ...);
void __cdecl g_logdata(const _LogDesc_ * const l, ...);

void __cdecl binlogdata(struct Logger * const logger, const _LogDesc_ * const l, const void * const data, unsigned datalen, ...);
void __cdecl g_binlogdata(const _LogDesc_ * const l, const void * const data, unsigned datalen, ...);


#define LOGSTART( fmt, level, loglevel) {if( level >= loglevel ) { \
static const t_LogDesc l={ __FILE__, fmt, __LINE__, level};	g_logdata( &l

#define BINLOGSTART( fmt, level, loglevel, data, length) {if( level >= loglevel ) { \
	static const t_LogDesc l={ __FILE__, fmt, __LINE__, level}; \
	g_binlogdata( &l, (const void *)(data), (unsigned)(length)

#define LOGEND );} }

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif //__DEBUG_LOGGER_H__
