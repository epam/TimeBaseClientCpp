/*
  Copyright 2021 EPAM Systems, Inc

  See the NOTICE file distributed with this work for additional information
  regarding copyright ownership. Licensed under the Apache License,
  Version 2.0 (the "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
  License for the specific language governing permissions and limitations under
  the License.
 */
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