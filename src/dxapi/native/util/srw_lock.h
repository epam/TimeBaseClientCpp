#pragma once


#if defined(_MSC_VER)
#ifndef INLINE
#if (_MSC_VER >= 1200)
#define INLINE __forceinline
#else
#define INLINE __inline
#endif /* #if (_MSC_VER >= 1200) */
#endif /* #ifndef INLINE */

#else
// For GC and CLang
#ifndef INLINE
#define INLINE inline __attribute__((always_inline))
#endif
#endif

#ifdef _WIN32
#ifndef DX_PLATFORM_WINDOWS
#define DX_PLATFORM_WINDOWS 1
#endif

#include <intrin.h>

#if defined(RTL_SRWLOCK_INIT)
typedef SRWLOCK DX_SRWLOCK;
#else
// Windows SRW lock
struct _WIN_RTL_SRWLOCK {
    void * Ptr;
};                            

typedef struct _WIN_RTL_SRWLOCK DX_SRWLOCK;

extern "C" {
    void __stdcall ReleaseSRWLockShared(volatile DX_SRWLOCK *);
    void __stdcall ReleaseSRWLockExclusive(volatile DX_SRWLOCK *);
    void __stdcall AcquireSRWLockShared(volatile  DX_SRWLOCK *);
    void __stdcall AcquireSRWLockExclusive(volatile DX_SRWLOCK *);
    unsigned char __stdcall TryAcquireSRWLockShared(volatile DX_SRWLOCK *);
    unsigned char __stdcall TryAcquireSRWLockExclusive(volatile DX_SRWLOCK *);
}

#endif // #ifndef RTL_SRWLOCK_INIT

#elif defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#ifndef DX_PLATFORM_UNIX
#define DX_PLATFORM_UNIX 1
#endif

extern "C" {
#include <pthread.h>
}

typedef pthread_rwlock_t DX_SRWLOCK;

#else
#error This platform is not supported
#endif

namespace dx_thread {
    class srw_lock {
    public:
        bool try_shared();
        bool try_exclusive();

        void get_shared();
        void get_exclusive();

        void release_shared();
        void release_exclusive();

        srw_lock();
        ~srw_lock();
    protected:
        void operator=(const srw_lock &) = delete;
        srw_lock(const srw_lock &) = delete;
        DX_SRWLOCK lock_;
    };


    template <bool shared> class srw_lock_section {
    public:
        srw_lock_section(srw_lock &lock);
        ~srw_lock_section();

    protected:
        srw_lock_section() = delete;
        srw_lock_section(const srw_lock_section &) = delete;
        void operator=(const srw_lock_section &) = delete;
        void * operator new (std::size_t size) = delete;    // Can't accidentally create on heap

        srw_lock &lock_;
    };

    typedef srw_lock_section<true> srw_shared, srw_read;
    typedef srw_lock_section<false> srw_exclusive, srw_write;

#if DX_PLATFORM_WINDOWS

    INLINE srw_lock::srw_lock() : lock_({0})
    {
    }


    INLINE srw_lock::~srw_lock()
    {
    }


    INLINE bool srw_lock::try_shared()
    {
        return 0 != TryAcquireSRWLockShared(&lock_);
    }


    INLINE bool srw_lock::try_exclusive()
    {
        return 0 != TryAcquireSRWLockExclusive(&lock_);
    }


    INLINE void srw_lock::get_shared()
    {
        AcquireSRWLockShared(&lock_);
    }


    INLINE void srw_lock::get_exclusive()
    {
        AcquireSRWLockExclusive(&lock_);
    }


    INLINE void srw_lock::release_shared()
    {
        ReleaseSRWLockShared(&lock_);
    }


    INLINE void srw_lock::release_exclusive()
    {
        ReleaseSRWLockExclusive(&lock_);
    }

#elif DX_PLATFORM_UNIX
    INLINE srw_lock::srw_lock()
    {
        if (0 != pthread_rwlock_init(&lock_, NULL)) {
            throw std::runtime_error(strerror(errno));
        }
    }


    INLINE srw_lock::~srw_lock()
    {
        pthread_rwlock_destroy(&lock_);
    }


    INLINE bool srw_lock::try_shared()
    {
        int result = pthread_rwlock_tryrdlock(&lock_);
        if (0 == result)
            return true;

        if (EBUSY == result)
            return false;
        else
            throw std::runtime_error(strerror(errno));
    }


    INLINE bool srw_lock::try_exclusive()
    {
        int result = pthread_rwlock_trywrlock(&lock_);
        if (0 == result)
            return true;

        if (EBUSY == result)
            return false;
        else
            throw std::runtime_error(strerror(errno));
    }


    INLINE void srw_lock::get_shared()
    {
        if (0 != pthread_rwlock_rdlock(&lock_)) {
            throw std::runtime_error(strerror(errno));
        }
    }


    INLINE void srw_lock::get_exclusive()
    {
        if (0 != pthread_rwlock_wrlock(&lock_)) {
            throw std::runtime_error(strerror(errno));
        }
    }


    INLINE void srw_lock::release_shared()
    {
        if (0 != pthread_rwlock_unlock(&lock_)) {
            throw std::runtime_error(strerror(errno));
        }
    }


    INLINE void srw_lock::release_exclusive()
    {
        if (0 != pthread_rwlock_unlock(&lock_)) {
            throw std::runtime_error(strerror(errno));
        }
    }

#else

#endif


    template<> INLINE srw_lock_section<true>::srw_lock_section(srw_lock &lock) : lock_(lock)
    {
        lock.get_shared();
    }


    template<> INLINE srw_lock_section<true>::~srw_lock_section()
    {
        lock_.release_shared();
    }


    template<> INLINE srw_lock_section<false>::srw_lock_section(srw_lock &lock) : lock_(lock)
    {
        lock.get_exclusive();
    }


    template<> INLINE srw_lock_section<false>::~srw_lock_section()
    {
        lock_.release_exclusive();
    }

}

