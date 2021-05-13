#pragma once

#include <stdexcept>
#include <vector>
#include <stdlib.h>
#include <assert.h>
#ifndef INLINE
#include "inline.h"
#endif

template<typename T> class const_array_view {
public:
    INLINE const T* data() const   { return data_; }
    INLINE const T& operator[](size_t i) const { return data_[i]; }
    INLINE size_t size()        const { return n_; }
    INLINE size_t size_max()    const { return n_; }

    INLINE const_array_view(const T *data, size_t data_size) : data_(data), n_(data_size)
    {}

    INLINE const_array_view(std::vector<T> &v) : data_(v.data()), n_(v.size())
    {}

protected:
    const T *data_;
    size_t n_;
};


template<typename T, size_t SIZE> class const_static_array_view {
public:
    INLINE const T* data() const   { return data_; }
    INLINE const T& operator[](size_t i) const { return data_[i]; }

    static INLINE size_t size()     { return SIZE; }
    static INLINE size_t size_max() { return SIZE; }

    static INLINE bool fits(size_t i) { return i < SIZE; }

    INLINE const_static_array_view(const T *data) : data_(data)
    {}

    INLINE const_static_array_view(std::vector<T> &v) : data_(v.data())
    {
        assert(v.size() >= SIZE);
    }

protected:
    const T *data_;
};


template<typename T, size_t SIZE, T FILLER> class static_array {
public:
    INLINE const T* data() const   { return data_; }
    INLINE       T* data()         { return data_; }

    INLINE const T& operator[](size_t i) const { return data_[i]; }
    INLINE       T& operator[](size_t i)       { return data_[i]; }

    INLINE size_t size() const { return n_; }
    INLINE static size_t size_max() { return SIZE; }
    INLINE bool fits(size_t i) const { return i < SIZE; }


    INLINE void clear()
    {
        n_ = 0;
    }


    void resize(size_t n)
    {
        if (n <= SIZE) {
            for (size_t i = n_; i < n; ++i) {
                data_[i] = FILLER;
            }

            n_ = n;
        }
        else {
            throw std::out_of_range("new static_array size is out of range");
        }
    }


    INLINE void push_back(const T& value) {
        auto n = n_;

        if (fits(n)) {
            data_[n] = value;
            n_ = n + 1;
        }
        else {
            error_add_size_limit();
        }
    }


    operator const_static_array_view<T, SIZE>() const
    {
        return const_static_array_view<T, SIZE>(data_);
    }


    operator const const_array_view<T>() const
    {
        return const_array_view<T>(data_, n_);
    }


    static_array(size_t n = 0) : n_(0)
    {
        resize(n);
    }
        
protected:
    static void error_add_size_limit()
    {
        throw std::out_of_range("static_array.add(): size limit reached");
    }
 
protected:
    size_t n_;
    T data_[SIZE];
};


