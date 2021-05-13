#pragma once

#include <stdint.h>

class QPCTimer {
public:
    int64_t getTicks();         // .NET 100ns ticks with base
    int64_t getTicksElapsed();  // .NET 100ns ticks without base (since start of init() call)
    int64_t getMillis();        // Unix epoch millis (with base)
    int64_t getMicros();        // Unix epoch micros
    int64_t getNanos();         // Unix epoch nanos
    void init(int64_t baseNanos = 0);

    // These methods do the same thing, but applied to the singleton static instance
    static int64_t ticks();         // .NET 100ns ticks with base
    static int64_t ticksElapsed();  // .NET 100ns ticks without base (since start of init() call)
    static int64_t millis();        // Unix epoch millis (with base)
    static int64_t micros();        // Unix epoch micros
    static int64_t nanos();         // Unix epoch nanos
    static void reset(int64_t baseNanos = 0);

public:
    int64_t baseDotNetTicks;
    int64_t baseMillis;         
    int64_t baseMicros;         
    int64_t baseNanos;
    
protected:
    /* not _that_ timebase */
    struct Timebase {
        uint64_t base;
        uint64_t freq;
    } timebase;
};

extern QPCTimer g_qpc_timer;