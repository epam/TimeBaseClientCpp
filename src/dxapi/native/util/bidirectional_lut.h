#pragma once

#include "platform/platform.h"
#include <string>
#include <vector>
#include <unordered_map>

// Bidirectional map, stores unique <string/other hashable type, uint> pairs with exactly 1 to 1 correspondence
// Mapping from integer id to string is done via simple array access

// TI - type that can be used as array index (supports conversion to/from uintptr_t)
// TS - type that can be used as unordered_map index and constructed via new()
// TI_BASE = first valid value for TI, used as array offset

#define TEMPLATE template<typename TS, typename TI, intptr_t TI_BASE>

TEMPLATE class BiLUT {
    typedef TI ID;
    typedef std::unordered_map<TS, TI> TS2TI;
    typedef std::vector<TS *> TI2TS;
    
public:

    // 'Invalid value' for fast id
    ID invalidId() const;

    // Get the number of valid registered pairs
    uintptr_t size() const;

    // Get content from a valid integer id, may throw exception if out of range, or return NULL if not valid
    const TS * operator[](uintptr_t indexId) const;

    // Get integer Id from a string, return invalidId(), if not found
    ID operator[](const TS & slowId) const;
    ID operator[](const TS * const slowId) const;

    // Return true if integer id belongs to one of the stored pairs
    bool contains(uintptr_t indexId) const;

    // Return true if string id belongs to one of the stored pairs
    bool contains(const TS &slowId) const;

    // return true if new pair was added, false if the pair is already registered, exception if one of the values is duplicate
    bool add(ID integerId, const TS &slowId);

    // Remove all pairs
    void clear();

    BiLUT();

private:
    TI2TS i2s_;                 // Maps TI -> TS  Some entries may be null, if no correspondence
    uintptr i2sSize_, size_;    // i2sSize >= size_  size_ == s3i_.size() == number of actual pairs stored
    TS2TI s2i_;                 // Maps TS -> TI
};

#define CLASS BiLUT<TS,TI,TI_BASE>
#define METHOD(return_values, name) TEMPLATE return_values CLASS::name


METHOD(INLINE typename CLASS::ID, invalidId)() const
{
    return (CLASS::ID)(TI_BASE - 1);
}


METHOD(INLINE const TS *, operator[])(uintptr_t integerId) const
{
    return i2s_[integerId - TI_BASE];
}


METHOD(INLINE typename CLASS::ID, operator[])(const TS * const stringId) const
{
    assert(NULL != stringId);
    return operator[](*stringId);
}


METHOD(INLINE typename CLASS::ID, operator[])(const TS &stringId) const
{
    auto i = s2i_.find(stringId);
    return s2i_.cend() == i ? invalidId() : i->second;
}


METHOD(INLINE uintptr_t, size)() const
{
    assert(size_ == s2i_.size());
    return size_;
}


METHOD(INLINE bool, contains)(uintptr_t integerId) const
{
    return (uintptr_t)(integerId - TI_BASE) < i2sSize_ &&  NULL != operator[](integerId);
}


METHOD(INLINE bool, contains)(const TS &key) const
{
    return invalidId() != operator[](key);
}


METHOD(bool, add)(ID integerId, const TS &key)
{
    uintptr_t index = integerId - TI_BASE;
    if (index > INTPTR_MAX) {
        throw std::out_of_range("integerId field is out of range");
    }

    if (index >= i2sSize_) {
        i2s_.resize(index + 1, NULL);
        i2sSize_ = index + 1;
    }

    auto s = operator[](integerId);
    auto i = operator[](key);
    if (s != NULL || invalidId() != i) {
        if (i == integerId && key == *s) {
            return false;
        }

        throw std::runtime_error("Attempt to register BiLUT pair with non-unique values");
    }

    i2s_[index] = new TS(key);
    ++size_;
    s2i_[key] = (ID)(index + TI_BASE);
    return true;
}


METHOD(INLINE void, clear)()
{
    s2i_.clear();
    // TODO: Mem leak? should research why deallocation code wasn't added before
    i2s_.clear();
    i2sSize_ = size_ = 0;
}


METHOD(, BiLUT)() : i2s_(128), i2sSize_(0), size_(0)
{
}


#undef TEMPLATE
#undef CLASS
#undef METHOD