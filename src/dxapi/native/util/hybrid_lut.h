#pragma once

#include "dynamic_lut.h"

// TODO: Work in progress
// 2-layer LUT for storing state for elements from a bigger set whose total size is unknown
// Assume some data set can be accessed either by integer index via array, either by "slow" hasheable index via map

// We assume that the mapping  (slow -> integer) exists somewhere else and when such mapping is assigned, assign() is called.
// After executing assign(), slow index is never used anymore and all further accesses will be done via integer index


#define TEMPLATE template<typename TS, typename T, intptr_t BASE, uintptr_t NMAX>
#define PARENT ArrayLUT<T, BASE, NMAX>
TEMPLATE class DualLayerLUT : PARENT {
    typedef std::unordered_map<TS, T> TS2T;

    bool contains(const TS &s) const
    {
        auto i = s2t_.find(s);
        return s2t_.cend() != i;
    }

    // Get data from slow layer
    const T operator[](const TS &s) const
    {
        return s2t_[s];
    }

    // Get data from fast layer
    const T operator[](uintptr_t index) const
    {
        return PARENT::operator[](index);
    }

    // Assign index to the key
    bool assign(const TS &s, uintptr_t index)
    {
        auto i = s2t_.find(s);
        if (s2t_.cend() != i) {
            // Propagate to the "fast layer"
            PARENT::set(index, i->second);
            s2t_.erase(i);
        }
    }

    void clear()
    {
        PARENT::clear();
        s2t_.clear();
    }

protected:
    TS2T s2t_; // This layer precedes DynaLUT layer
};


#undef TEMPLATE
#undef CLASS
#undef PARENT
#undef METHOD