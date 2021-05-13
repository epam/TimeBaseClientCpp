#pragma once

#include "platform/platform.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_map>


#define TEMPLATE template<typename T, typename ARRAY, intptr_t BASE, uintptr_t NMAX>

// ARRAY<T> must support operator[], resize(), clear(). No need to support size(), no need to initialize entries

TEMPLATE class AbstractLUT {
public:
    /* -- Field accessor methods
    */

    // Read the default value
    size_t maxSize() const;

    // Read the currently allocated number of entries
    size_t size() const;

    // Read the default value
    T defaultValue() const;

    // Change the default value
    AbstractLUT& setDefaultValue(T value);

    /* -- Const methods
    */

    // Get value from valid index (guaranteed to be in range, does not check range, may crash if invalid)
    T operator[](size_t index) const;

    // Get value from possibly invalid index (returns defaultValue if out of range)
    T get(size_t index) const;


    /* -- Non-const methods
    */

    // Clear all entries, set length to 0
    void clear();

    void clearBitmask(T mask);

    // Fill all entries with specific value
    void fill(T value);
    void fill();

    void fillBitmask(T mask, T value);
    void fillBitmask(T mask);

    // Set an element to specified or default value
    void set(size_t index, T value);
    void set(size_t index);

    AbstractLUT();

protected:
    size_t dataSize_;    // Cached data_.size();
    T defaultValue_;
    ARRAY data_;
    
};


#define CLASS AbstractLUT<T, ARRAY, BASE, NMAX>
#define METHOD(return_values, name) TEMPLATE return_values CLASS::name

METHOD(INLINE T, operator[])(size_t index) const
{
    assert(index - BASE < dataSize_);
    return data_[index - BASE];
}

// get value from possibly invalid index (return default if out of range)
METHOD(INLINE T, get)(size_t index) const
{
    return (index -= BASE) < dataSize_ ? data_[index] : defaultValue_;

    // TODO: possible optimization: additional entry for the default Value, so we can have branch-free accessor
    // Like this:
    // return data_[((index -= BASE) < dataSize_ ? index : -1) + 1];
}


METHOD(void, set)(size_t index, T value)
{
    index -= BASE;
    if (index < dataSize_) {
        data_[index] = value;
    }
    else {
        assert(index >= dataSize_);
        if (index < NMAX) {
            data_.resize(index + 1);
            T *ep = &data_[index];
            auto dv = defaultValue_;
            for_neg(i, (intptr_t)(dataSize_ - index)) {
                ep[i] = dv;
            }

            dataSize_ = index + 1;
            data_[index] = value;
        }
        else {
            throw std::out_of_range("LUT::set() : index > max. allowed size or is negative");
        }
    }
}


METHOD(void, set)(size_t index)
{
    set(index, defaultValue_);
}


METHOD(void, clear)()
{
    data_.clear();
    dataSize_ = 0;
}


METHOD(void, fill)(T value)
{
    T * wp = &data_[0];
    forn (i , dataSize_) wp[i] = value;
}


METHOD(void, fill)()
{
    fill(defaultValue_);
}


// Common case (will set or clear bits within mask)
METHOD(void, fillBitmask)(T mask, T value)
{
    T * wp = &data_[0];
    value &= mask;
    forn(i, dataSize_) wp[i] = (wp[i] & mask) | value;
}


METHOD(void, fillBitmask)(T mask)
{
    fillBitmask(mask, defaultValue_);
}


METHOD(INLINE CLASS&, setDefaultValue)(T value)
{
    defaultValue_ = value;
    return *this;
}


// TODO: may add preallocation option
METHOD(,AbstractLUT)() : dataSize_(0), defaultValue_((T)(BASE - 1)) {}

METHOD(INLINE T, defaultValue)() const { return defaultValue_; }

METHOD(INLINE size_t, maxSize)() const { return NMAX; }

METHOD(INLINE size_t, size)() const    { return dataSize_; }


#undef TEMPLATE
#undef CLASS
#undef METHOD


//#define CLASS DynaLUT<T, BASE, NMAX>
//#define METHOD(return_values, name) TEMPLATE return_values CLASS::name
//
//
//#undef TEMPLATE
//#undef CLASS
//#undef METHOD

template<typename T, size_t SIZE> class _LUT_ArrayWrapper_ {
public:
    const T& operator[](size_t index) const { return data_[index]; }
    T& operator[](size_t index) { return data_[index]; }

    void clear() {}

    size_t capacity() const { return SIZE; }
    size_t max_size() const { return SIZE; }

    void resize(size_t n) const
    {
        if (n > SIZE) {
            throw std::out_of_range("array::resize(): can't be resized beyond preallocated size");
        }
    }

protected:
    T data_[SIZE];
};


template<typename T, intptr_t BASE, size_t NMAX> class VectorLUT : public AbstractLUT<T, std::vector<T>, BASE, NMAX> {};


template<typename T, intptr_t BASE, size_t NMAX> class ArrayLUT : public AbstractLUT<T, _LUT_ArrayWrapper_<T, NMAX>, BASE, NMAX> {};