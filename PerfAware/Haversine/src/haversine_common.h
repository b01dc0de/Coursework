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

#define Assert(Exp) if (!(Exp)) { __debugbreak(); }

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

namespace Reference
{
f64 Square(f64 A);
f64 RadiansFromDegrees(f64 Degrees);
f64 CalculateHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1);
} // namespace Reference

#endif // HAVERSINE_COMMON_H

