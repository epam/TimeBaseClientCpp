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
#pragma once

#include <string>
#include "inline.h"

class CharBufferImpl {
    template<size_t N> friend struct CharBuffer;
    static void copy_shortened(char dest[], const std::string &s, size_t dest_max_length);
    static void copy_clipped(char dest[], const std::string &s, size_t dest_max_length);
    static void copy_shortened(char dest[], const char *s, size_t dest_max_length);
    static void copy_clipped(char dest[], const char *s, size_t dest_max_length);
};

template<size_t N> struct CharBuffer {
    //friend class CharBufferImpl;
    const char &operator[](intptr_t i) const { return data_[i]; }
    char &operator[](intptr_t i) { return data_[i]; }
    const char* c_str() const { return data_; }
    operator const char*() const { return data_; }
    operator char*() const { return data_; }

    INLINE static CharBuffer shortened(const char *s)          { CharBuffer<N> b; CharBufferImpl::copy_shortened(b.data_, s, N - 1); return b; }
    INLINE static CharBuffer clipped(const char *s)            { CharBuffer<N> b; CharBufferImpl::copy_clipped(b.data_, s, N - 1); return b; }

    INLINE static CharBuffer shortened(const std::string &s)   { CharBuffer<N> b; CharBufferImpl::copy_shortened(b.data_, s, N - 1); return b; }
    INLINE static CharBuffer clipped(const std::string &s)     { CharBuffer<N> b; CharBufferImpl::copy_clipped(b.data_, s, N - 1); return b; }

    /*CharBuffer(const char * s);
    CharBuffer(const std::string & s);*/

protected:
    CharBuffer() {}

    //static void copy_shortened(char dest[], const std::string &s, size_t dest_max_length);
    //static void copy_clipped(char dest[], const std::string &s, size_t dest_max_length);
    //static void copy_shortened(char dest[], const char *s, size_t dest_max_length);
    //static void copy_clipped(char dest[], const char *s, size_t dest_max_length);


    /*void copy_shortened(CharBuffer<N> &cb, const std::string &s);
    void copy_clipped(CharBuffer<N> &cb, const std::string &s);
    void copy_shortened(CharBuffer<N> &cb, const char *s);
    void copy_clipped(CharBuffer<N> &cb, const char *s);*/

protected:
    char data_[N];
};