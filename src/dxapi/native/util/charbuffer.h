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

