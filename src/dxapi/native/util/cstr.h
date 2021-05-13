#pragma once

#include <string>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "inline.h"


struct cstr {
    operator const char *() const;

    cstr(nullptr_t);
    cstr(const char * source);
    cstr(const std::string & source);

    static cstr null();
    static cstr move(const char * source);

    ~cstr();
protected:
    cstr() = delete;
    cstr &operator=(cstr &) = delete;
    cstr(const cstr &) = delete;

protected:
    //cstr(const char * src, bool dummy = false) : data_(src) {};

    char * data_;
};


// TODO:

#if 0
INLINE      cstr::operator const char *() const { return data_; }

NOINLINE    cstr::cstr(const char * source) : data_(_strdup(source)) {}

INLINE      cstr::cstr(const std::string & source) : cstr(source.c_str()) {}

INLINE      cstr::cstr(nullptr_t) : data_(NULL) {}

NOINLINE    cstr::~cstr() { ::free(data_); }

INLINE cstr cstr::null() { return cstr(nullptr);  }
#endif