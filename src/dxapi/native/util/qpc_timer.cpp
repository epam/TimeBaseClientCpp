/*
  Standalone performance counter header. Wraps and abstracts Windows QPC, converts to different units
  Probably needs rewrite due to being replaced by std::chrono or rdtsc
*/

#include <stdint.h>
#include <assert.h>
#include "qpc_timer.h"


QPCTimer g_qpc_timer;

#define NANOS_PER_SEC  UINT64_C(1000000000)
#define TICKS_PER_SEC  UINT64_C(10000000)
#define MICROS_PER_SEC UINT64_C(1000000)
#define MILLIS_PER_SEC UINT64_C(1000)
#define DOTNET_TICKS_TO_UNIX_EPOCH INT64_C(621355968000000000)


#ifdef _WIN32
// Windows includes

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define INLINE inline __forceinline

#define GET_QPC() \
uint64_t t0; \
register uint64_t t; \
QueryPerformanceCounter((LARGE_INTEGER *)&t0); \
t = t0 - timebase.base;

// Y = X * TO_SCALE / FROM_SCALE      while avoiding overflow if possible
// (FROM_SCALE*TO_SCALE) <= UINT64_MAX!!
// if (FROM_SCALE*TO_SCALE) > UINT64_MAX, long div version should be implemented
#define SCALE(X, FROM_SCALE, TO_SCALE) ((TO_SCALE) * ((X) / (FROM_SCALE)) + (((X) % (FROM_SCALE)) * (TO_SCALE)) / (FROM_SCALE))
#define GET_SCALED(TGT_FREQ, BASE) GET_QPC(); return SCALE(t, timebase.freq, TGT_FREQ) + (BASE);

#elif 1
#include <chrono>
// Unix includes
#define INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__ ((noinline))

#define GET_QPC() uint64_t t = chrono::duration_cast<chrono::nanoseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() - timebase.base;

#define GET_SCALED(TGT_FREQ, BASE) GET_QPC(); return (t) / (NANOS_PER_SEC / TGT_FREQ) + (BASE);

using namespace std;

#else
#error QPCTimer class is not implemented for non-windows systems
#endif


INLINE int64_t QPCTimer::getTicks()
{
    GET_SCALED(TICKS_PER_SEC, baseDotNetTicks);
}


INLINE int64_t QPCTimer::getTicksElapsed()
{
    GET_SCALED(TICKS_PER_SEC, 0);
}


INLINE int64_t QPCTimer::getNanos()
{
    GET_SCALED(NANOS_PER_SEC, baseNanos);
}


int64_t QPCTimer::getMicros()
{
    GET_SCALED(MICROS_PER_SEC, baseMicros);
}


int64_t QPCTimer::getMillis()
{
    GET_SCALED(MILLIS_PER_SEC, baseMillis);
}


int64_t QPCTimer::ticks()
{
    return g_qpc_timer.getTicks();
}


int64_t QPCTimer::ticksElapsed()
{
    return g_qpc_timer.getTicksElapsed();
}


int64_t QPCTimer::nanos()
{
    return g_qpc_timer.getNanos();
}


int64_t QPCTimer::micros()
{
    return g_qpc_timer.getMicros();
}


int64_t QPCTimer::millis()
{
    return g_qpc_timer.getMillis();
}


void QPCTimer::reset(int64_t baseNanos)
{
    return g_qpc_timer.init(baseNanos);
}


void QPCTimer::init(int64_t baseNanos)
{
    // TODO: maybe convert part of the baseNanos value into qpcBase for greater precision
    this->timebase.base = 0;
    GET_QPC();
    this->timebase.base = t;
    this->baseNanos = baseNanos;
    this->baseMillis = (this->baseMicros = baseNanos / 1000) / 1000;
    // Translate into the middle of unsigned range, scale(divide), then translate back
    // If scale as signed, can round in the wrong direction
    this->baseDotNetTicks = (baseNanos + (INT64_MAX / UINT64_C(100)) * 100) / 100U + (DOTNET_TICKS_TO_UNIX_EPOCH - (INT64_MAX / 100));

#ifdef _WIN32
    uint64_t f;
    QueryPerformanceFrequency((LARGE_INTEGER*)&f);
    this->timebase.freq = f;
    // Check range. Otherwise conversion math will fail
    assert((double)(int64_t)timebase.freq * (int64_t)NANOS_PER_SEC < UINT64_MAX);
#else
    this->timebase.freq = NANOS_PER_SEC;
#endif

}
