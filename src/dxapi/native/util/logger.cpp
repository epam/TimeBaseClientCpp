// For win32 warnings
#define _CRT_SECURE_NO_DEPRECATE

#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <io.h>
#include <stdio.h>

#include "logger.h"
#include "critical_section.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

    // TODO: Remove dependency later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


struct Logger {
    FILE * file;
    size_t max_log_size;
    DX_CRITICAL_SECTION crit_section;
    uint8_t mode;
    uint8_t * log_level;
    unsigned log_n_level;
    char filename[512];
};

// buffer for file copying
#define DEBUG_COPY_BUFFER_SIZE 0x20000

struct Logger g_debug;
struct Logger * dbg = &g_debug;


static void FormatDateTime(char * to, const SYSTEMTIME * const st)
{
    sprintf(to, "%04d-%02d-%02d %02d:%02d:%02d",
        st->wYear, st->wMonth, st->wDay,
        st->wHour, st->wMinute, st->wSecond);
}

static void GetDateTime(char * to)
{
    // TODO: This code is windows - dependent
    SYSTEMTIME st;
    GetLocalTime(&st);
    FormatDateTime(to, &st);
}

void log_open(struct Logger * const self, const char * name, size_t maxsize,
    uint8_t * log_level, unsigned log_n_level)
{

    char stime[128];
    if (NULL == self) return;
    memset(self, 0, sizeof(struct Logger));
    self->log_level = log_level;
    self->log_n_level = log_n_level;

    self->file = fopen(name, "a+t");
    if (NULL == self->file) return;
    strncpy(self->filename, name, sizeof(self->filename));
    self->mode = 1;
    GetDateTime(stime);
    fprintf(self->file, "%s --- Logging started -----\n", stime);
    // we supply loaded and prepared log level array
    //memset( debug->log_level, 0, sizeof(debug->log_level[0])*debug->log_n_level );
    self->max_log_size = maxsize > 2047 ? maxsize : INT32_MAX;
    DxCriticalSectionInit(&self->crit_section);
}


void log_close(struct Logger * const self)
{
    if (NULL == self) return;
    if (self->file) {
        fprintf(self->file, "\n\n");
        fclose(self->file);
    }
    self->mode = 0;
}

// Check log file size, shorten if needed
#ifdef LOG_SIZE_LIMIT
static void sizecheck(struct Logger * const self)
{
    if (NULL == self) return;
    char tempbuffer[DEBUG_COPY_BUFFER_SIZE];
    unsigned long fsz, rd_pos, wr_pos;		// file size
    long left;		// number of bytes to copy
    int t;			// temporary char
    int n;			// number of bytes currently being copied
    int file;

    if ((fsz = ftell(self->file)) > self->max_log_size) {

        fseek(self->file, fsz - self->max_log_size / 2, SEEK_SET);
        do {
            t = fgetc(self->file);
        } while (t != EOF && t != '\n' && t != '\r');
        // code for CR or LF
        while (t == '\n' || t == '\r') {
            t = fgetc(self->file);
        }
        left = fsz - (rd_pos = ftell(self->file) - 1);
        wr_pos = 0;
        fclose(self->file);
        if (t != EOF && left) {
            file = _open(self->filename, _O_BINARY | _O_RDWR, _S_IREAD | _S_IWRITE);
            if (file)
                do {
                    n = min(left, DEBUG_COPY_BUFFER_SIZE);
                    _lseek(file, rd_pos, SEEK_SET);
                    rd_pos += _read(file, tempbuffer, n);
                    _lseek(file, wr_pos, SEEK_SET);
                    wr_pos += _write(file, tempbuffer, n);
                    left -= n;
                } while (left);
                _chsize(file, wr_pos);
                _close(file);
        }
        self->file = fopen(self->filename, "a+t");
        fseek(self->file, wr_pos, SEEK_SET);
    }
}
#endif // LOG_SIZE_LIMIT


static void binlog(struct Logger * const self, const void * data, unsigned datalen)
{
    static const char sep[17] = "    -   -   -   ";
    int j, linelen;
    unsigned tmp;
    if (NULL == self) return;
    DxCriticalSectionEnterSpin(&self->crit_section);
    while (datalen) {
        linelen = datalen  <16 ? datalen : 16;
        for (j = 0; j < linelen; j++) {
            fprintf(self->file, "%c%02X", sep[j], ((uint8_t *)data)[j]);
        }
        for (j = 0; j < 16 - linelen; j++)
            fprintf(self->file, "   ");
        fprintf(self->file, " |"); // print separator
        for (j = 0; j < linelen; j++) {
            tmp = ((uint8_t *)data)[j];
            fprintf(self->file, "%1c", tmp >= 0x20 ? tmp : '.');
        }
        fprintf(self->file, "\n");
        data = (uint8_t *)data + linelen;
        datalen -= linelen;
    }
    DxCriticalSectionLeave(&self->crit_section);
}


void dbg_logdata_full(struct Logger * const self, const _LogDesc_ * const l, va_list arg, const void * const data, unsigned datalen)
{
    SYSTEMTIME st;
    char hms[32];
    char msg[512];

    if (NULL == self) {
        return;
    }

    if (self->mode) {
        DxCriticalSectionEnterSpin(&self->crit_section);
        GetLocalTime(&st);
        /*fprintf(hb_debugfile, "[%02d:%02d:%02d.%03d] ",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );*/
        sprintf(hms, "%02d-%02d %02d:%02d:%02d", st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
        fprintf(self->file, "[%s.%03d] ",
            hms, st.wMilliseconds);
        if (l->level >= _LOG_ERROR)
            fprintf(self->file, "ERROR in ");

        vsnprintf(msg, sizeof(msg), l->fmt, arg);
        msg[sizeof(msg) - 1] = '\0';

        if (l->file) {
            fprintf(self->file, "%s:%d\n\t%s\n", l->file, l->line, msg);
        }
        else {
            fprintf(self->file, "\n\t%s\n", msg);
        }

        if (datalen) {
            binlog(self, data, datalen);
        }

        fflush(self->file);
        // TODO: move into debug object property!!!! (or not)
#ifdef LOG_SIZE_LIMIT
        sizecheck(debug);
#endif // LOG_SIZE_LIMIT
        DxCriticalSectionLeave(&self->crit_section);
    }
}


void __cdecl logdata(struct Logger * const self, const _LogDesc_ * const l, ...)
{
    register va_list arg;
    va_start(arg, l);
    dbg_logdata_full(self, l, arg, NULL, 0);
    va_end(arg);
}


void __cdecl binlogdata(struct Logger * const self, const _LogDesc_ * const l, const void * const data, unsigned datalen, ...)
{
    register va_list arg;
    va_start(arg, datalen);
    dbg_logdata_full(self, l, arg, data, datalen);
    va_end(arg);
}


//#pragma mark Global log

void __cdecl g_logdata(const _LogDesc_ * const l, ...)
{
    register va_list arg;
    va_start(arg, l);
    dbg_logdata_full(dbg, l, arg, NULL, 0);
    va_end(arg);
}


void __cdecl g_binlogdata(const _LogDesc_ * const l, const void * const data, unsigned datalen, ...)
{
    register va_list arg;
    va_start(arg, datalen);
    dbg_logdata_full(dbg, l, arg, data, datalen);
    va_end(arg);
}


void g_log_set(struct Logger * const logger)
{
    if (NULL != dbg)
    {
        g_log_close();
    }

    if (NULL == (dbg = logger)) {
        // Will do nothing at this point
        //dbg = &g_debug;
    }
}


void g_log_close(void)
{
    if (NULL != dbg) {
        if (&g_debug != dbg) {
            dbg = NULL;
        }
        else {

        }
    }
}


void log_idle(struct Logger * const self)
{
 // Was only used for database logging
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */