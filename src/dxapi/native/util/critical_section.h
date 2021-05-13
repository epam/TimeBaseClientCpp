#pragma once

#ifdef __cplusplus
#define EXTERNC extern "C"
extern "C" {
#endif  /* __cplusplus */

#include <stdint.h>

typedef uint32_t DX_CRITICAL_SECTION;

#ifdef _WIN32
#ifndef DX_PLATFORM_WINDOWS
#define DX_PLATFORM_WINDOWS 1
#endif
#include <intrin.h>

#ifndef INLINE
#if (_MSC_VER >= 1200)
#define INLINE __forceinline
#else
#define INLINE __inline
#endif /* #if (_MSC_VER >= 1200) */
#endif /* #ifndef INLINE */

#else
#ifndef DX_PLATFORM_UNIX
#define DX_PLATFORM_UNIX 1
#endif
#ifndef INLINE
#define INLINE inline __attribute__((always_inline))
#endif

    // TODO: include something for Unix/Linux
#endif


#if DX_PLATFORM_UNIX

#define DxCompilerBarierRead() do { __asm__ __volatile__ ( "" ::: "memory" ); } while(0)
#define DxCompilerBarierWrite() do { __asm__ __volatile__ ( "" ::: "memory" );  } while(0)
#define DxCompilerBarierReadWrite() do { __asm__ __volatile__ ( "" ::: "memory" ); } while(0)

#define DxCacheBarierRead() do {__builtin_ia32_lfence(); } while(0)
#define DxCacheBarierWrite() do {__builtin_ia32_sfence(); } while(0)
#define DxCacheBarierReadWrite() do {__builtin_ia32_mfence(); } while(0)

    static INLINE uint32_t
        DxAtomicExchangeUInt32(volatile uint32_t *ptr, uint32_t new_value)
    {
        return (uint32_t)__sync_lock_test_and_set(ptr, new_value);
    }

    static INLINE void *
        DxAtomicExchangePointer(void * volatile *ptrptr, void *new_value)
    {
        return (void *)__sync_lock_test_and_set((void * volatile *)ptrptr, (void *)new_value);
    }

    INLINE void DxThreadYield() { sched_yield(); }

#elif DX_PLATFORM_WINDOWS

EXTERNC __declspec(dllimport) int __stdcall SwitchToThread();

#define DxCompilerBarierRead() do { _ReadBarrier(); } while(0)
#define DxCompilerBarierWrite() do { _WriteBarrier(); } while(0)
#define DxCompilerBarierReadWrite() do { _ReadWriteBarrier(); } while(0)

#define DxCacheBarierRead() do {_mm_lfence(); } while(0)
#define DxCacheBarierWrite() do {_mm_sfence(); } while(0)
#define DxCacheBarierReadWrite() do {_mm_mfence(); } while(0)

    INLINE void DxThreadYield() { SwitchToThread(); }

    static INLINE uint32_t
        DxAtomicExchangeUInt32(volatile uint32_t * ptr, uint32_t new_value)
    {
        return (uint32_t)_InterlockedExchange((volatile long *)ptr, new_value);
    }

#if (defined(_M_X64) || defined(_M_ARM64))
    static INLINE uint64_t
        DxAtomicExchangeUInt64(volatile uint64_t * ptr, uint64_t new_value)
    {
        return (uint64_t)_InterlockedExchange64((volatile __int64 *)ptr, (int64_t)new_value);
    }
#endif

    static INLINE void *
        DxAtomicExchangePointer(void * volatile * ptrptr, void * new_value)
    {
#if (defined(_M_X64) || defined(_M_ARM64))        
        return (void *)_InterlockedExchangePointer((void * volatile *)ptrptr, (void *)new_value);
#else
        // Workaround for windows header bug (occured in v120 toolset for x86 C++/CLR target )
        static_assert(sizeof(long) == sizeof(void*), "This code expects 32-bit pointers");
        return (void *)_InterlockedExchange((long volatile *)ptrptr, (long)new_value);
#endif
    }

#endif

    static INLINE void
        DxCriticalSectionInit(DX_CRITICAL_SECTION * const section)
    {
        *section = 1;
    }


    static INLINE void
        DxCriticalSectionEnterYield(DX_CRITICAL_SECTION * const section)
    {
        while (DxAtomicExchangeUInt32(section, 0x00) == 0x00) {
            DxThreadYield();
        }
    }

    static INLINE void
        DxCriticalSectionEnterSpin(DX_CRITICAL_SECTION * const section)
    {
        while (DxAtomicExchangeUInt32(section, 0x00) == 0x00) {
        }
    }

    static INLINE void
        DxCriticalSectionLeave(DX_CRITICAL_SECTION * const section)
    {
        DxAtomicExchangeUInt32(section, 1);
    }


#ifdef __cplusplus
}

namespace dx_thread {

INLINE void yield() { DxThreadYield(); }

template<typename T> T atomic_exchange(T &value, T to);


class waitable_atomic_uint {
public:
    INLINE uint32_t exchange(uint32_t to)
    {
        return DxAtomicExchangeUInt32((uint32_t *)&value_, to);
    }

    // Spins while there's a succesful change to the specified value from any different value
    INLINE uint32_t exchange_spin(uint32_t to)
    {
        uint32_t tmp;
        while (to == (tmp = exchange(to))) {
        }

        return tmp;
    }

    // Yields while there's a succesful change to the specified value from any different value
    INLINE uint32_t exchange_yield(uint32_t to)
    {
        uint32_t tmp;
        while (to == (tmp = exchange(to))) {
            yield();
        }

        return tmp;
    }

    // Yields while there's a succesful change to the specified value from any different value
    INLINE void add_yield(int32_t delta)
    {
        uint32_t tmp;
        while (1) {
            tmp = value_;
            if (tmp == exchange(tmp + delta))
                return;
            yield();
        }
    }

    operator uint32_t() const { return value_; }

public:
    waitable_atomic_uint(uint32_t x = 0) : value_(x) {}

protected:
    uint32_t value_;
};

class critical_section : public waitable_atomic_uint {
public:
    INLINE void enter_spin()        { exchange_spin(0); }
    INLINE void enter_yield()       { exchange_yield(0); }
    INLINE void leave()             { exchange(1); }

    INLINE critical_section() : waitable_atomic_uint(1) {}

private:
    critical_section(const critical_section&) = delete;
    void operator=(const critical_section&) = delete;
};

template<bool spin> class lock {
public:
    INLINE ~lock() {
        section_.leave();
    }

    INLINE lock(critical_section &section) : section_(section)
    {
        if (spin) {
            section.enter_spin();
        }
        else {
            section.enter_yield();
        }
    }

protected:
    lock() = delete;
    lock(const lock&) = delete;
    void operator=(const lock&) = delete;
    void * operator new(std::size_t size) = delete;    // Can't accidentally create on heap

    critical_section &section_;
};

typedef lock<false> yield_lock;
typedef lock<true> spin_lock;


template<typename T> class atomic_counter {
public:
    T get()
    {
        T retval;
        cs_.enter_spin();
        retval = ++id_;
        cs_.leave();
        return retval;
    }

    atomic_counter() : id_(0) { }

protected:
    dx_thread::critical_section cs_;
    T id_;

    atomic_counter(const atomic_counter&) = delete;
    void operator=(const atomic_counter&) = delete;
};


//template<> static INLINE uint32_t atomic_exchange(volatile uint32_t &value, uint32_t to)
//{
//    return DxAtomicExchangeUInt32(&value, to);
//}


} /* namespace dx_thread */

template<> INLINE int32_t dx_thread::atomic_exchange(int32_t &value, int32_t to)
{
    return dx_thread::atomic_exchange((uint32_t &)value, (uint32_t)to);
}

#if (defined(_M_X64) || defined(_M_ARM64))
template<> INLINE uint64_t dx_thread::atomic_exchange(uint64_t &value, uint64_t to)
{
    return DxAtomicExchangeUInt64(&value, to);
}
#endif

template<> INLINE int64_t dx_thread::atomic_exchange(int64_t &value, int64_t to)
{
    return dx_thread::atomic_exchange((uint64_t &)value, (uint64_t)to);
}


template<> INLINE void * dx_thread::atomic_exchange(void * &value, void * to)
{
    return DxAtomicExchangePointer(&value, to);
}

#endif  /* __cplusplus */

#undef EXTERNC 

/* some code borrowed from _DX_FAST_SYNC_H_ */
