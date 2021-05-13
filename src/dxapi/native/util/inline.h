#pragma once

#if defined(_MSC_VER)
// Visual C
#ifndef INLINE
#define NOINLINE __declspec(noinline)
#if (_MSC_VER >= 1200)
// For newer Visual C
#define INLINE __forceinline
#else
// For older Visual C
#define INLINE __inline
#endif /* #if (_MSC_VER >= 1200) */
#endif /* #ifndef INLINE */

#else
// GCC/Clang
#ifndef INLINE
#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__ ((noinline))
#endif
#endif