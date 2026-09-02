#ifndef HAVERSINE_COMMON_H
#define HAVERSINE_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <random>

#include <intrin.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using f32 = float;
using f64 = double;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

#if _DEBUG
#define Assert(Exp) if (!(Exp)) { __debugbreak(); }
#else
#define Assert(Exp) (void)0
#endif // _DEBUG

struct HaversinePair
{
    f64 X0;
    f64 Y0;
    f64 X1;
    f64 Y1;
};

struct HaversineArgs
{
    bool bCluster = false;
    u32 Seed = 0;
    u32 Num = 0;
    std::mt19937 RNG;

    f64 Average = 0.0;

    static constexpr int MaxFileNameLength = 128;
    char FileName_PairsJSON[MaxFileNameLength];
    char FileName_AnswersJSON[MaxFileNameLength];
    char FileName_AnswersBinary[MaxFileNameLength];

    void Init(bool _bCluster, u32 _Seed, u32 _Num);
    void Generate();
};

u64 GetOSTimerFreq();
u64 ReadOSTimer();
inline u64 ReadCPUTimer() { return __rdtsc(); }
u64 EstimateCPUTimerFreq();

struct BlockTiming
{
    const char* Name;
    u64 Begin;
    u64 End;
};

struct ScopedTiming
{
    u64 ID;

    ScopedTiming(const char* _Name);
    ~ScopedTiming();
};

struct Perf
{
    static constexpr u64 MaxBlockTimings = 1024;
    static BlockTiming Total;
    static BlockTiming Timings[MaxBlockTimings];
    static u64 Count;

    static void BeginProfiling();
    static void EndProfiling();
    static u64 BeginTiming(const char* Name);
    static void EndTiming(u64 ID);
    static void PrintTimings();
};

#define TimeBlock(BlockName) ScopedTiming _ScopedTiming_##BlockName(#BlockName)
#define TimeFunction() ScopedTiming _ScopedTiming_##__func__(__func__)

namespace Reference
{
f64 Square(f64 A);
f64 RadiansFromDegrees(f64 Degrees);
f64 CalculateHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1);
} // namespace Reference

#endif // HAVERSINE_COMMON_H

