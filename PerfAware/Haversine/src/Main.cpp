#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <random>

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

template <typename T>
struct DynamicArray
{
    u64 Capacity;
    u64 Size;
    T* Data;

    static constexpr int DefaultCapacity = 32;

    DynamicArray()
    {
        Capacity = DefaultCapacity;
        Size = 0;
        Data = new T[Capacity];
    }

    ~DynamicArray()
    {
        delete[] Data;
    }

    void Grow()
    {
        u64 OldCapacity = Capacity;
        T* OldData = Data;

        Capacity = OldCapacity * 2;
        Data = new T[Capacity];
        memcpy(Data, OldData, sizeof(T) * Size);

        delete[] OldData;
    }

    void Add(T NewItem)
    {
        if (Size == Capacity)
        {
            Grow();
        }

        Data[Size] = NewItem;
        Size++;
    }

    T& operator[](int Idx)
    {
        return Data[Idx];
    }
};

namespace Reference
{
    f64 Square(f64 A);
    f64 RadiansFromDegrees(f64 Degrees);
    f64 CalculateHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1);
} // namespace Reference

struct FileContentsT
{
    u64 Size;
    u8 *Contents;
};

FileContentsT ReadFileContents(const char* FileName, bool bAppendZero)
{
    FileContentsT Result = {};

    FILE* FileHandle = nullptr;
    fopen_s(&FileHandle, FileName, "rb");
    if (FileHandle)
    {
        fseek(FileHandle, 0, SEEK_END);
        Result.Size = (u64)ftell(FileHandle) + (bAppendZero ? 1 : 0);
        fseek(FileHandle, 0, SEEK_SET);

        if (Result.Size)
        {
            Result.Contents = new u8[Result.Size];
            fread_s(Result.Contents, Result.Size, Result.Size, 1, FileHandle);
            if (bAppendZero) { Result.Contents[Result.Size - 1] = '\0'; }
        }

        fclose(FileHandle);
    }

    return Result;
}

struct HaversinePair
{
    f64 X0;
    f64 Y0;
    f64 X1;
    f64 Y1;
};

struct GenerateHaversinePairsArgs
{
    bool bCluster;
    u32 Seed;
    u32 Num;
    std::mt19937 RNG;
};

HaversinePair *GenerateHaversinePairs(GenerateHaversinePairsArgs *Args)
{
    HaversinePair *Output = new HaversinePair[Args->Num];

    constexpr f64 MinX = -180.0;
    constexpr f64 MaxX = +180.0;
    constexpr f64 MinY = -90.0;
    constexpr f64 MaxY = +90.0;

    constexpr u32 NumClusters = 8;
    constexpr f64 ClusterOffsetRangeX = MaxX / 16.0;
    constexpr f64 ClusterOffsetRangeY = MaxY / 16.0;

    std::uniform_real_distribution<f64> X_Dist{MinX, MaxX};
    std::uniform_real_distribution<f64> Y_Dist{MinY, MaxY};

    std::uniform_int_distribution<u32> Cluster_Dist{0, NumClusters - 1};
    std::uniform_real_distribution<f64> ClusterOffsetX_Dist{-ClusterOffsetRangeX, +ClusterOffsetRangeX};
    std::uniform_real_distribution<f64> ClusterOffsetY_Dist{-ClusterOffsetRangeY, +ClusterOffsetRangeY};

    auto Clamp = [](f64 X, f64 A, f64 B) -> f64
    {
        if (X < A)
            return A;
        if (X > B)
            return B;
        return X;
    };

    if (Args->bCluster)
    {
        double ClusterXs[NumClusters] = {};
        double ClusterYs[NumClusters] = {};
        for (int ClusterIdx = 0; ClusterIdx < NumClusters; ClusterIdx++)
        {
            ClusterXs[ClusterIdx] = Clamp(X_Dist(Args->RNG), MinX + ClusterOffsetRangeX, MaxX - ClusterOffsetRangeX);
            ClusterYs[ClusterIdx] = Clamp(Y_Dist(Args->RNG), MinY + ClusterOffsetRangeY, MaxY - ClusterOffsetRangeY);
        }

        for (int PairIdx = 0; PairIdx < Args->Num; PairIdx++)
        {
            u32 ClusterIdx0 = Cluster_Dist(Args->RNG);
            u32 ClusterIdx1 = Cluster_Dist(Args->RNG);

            Output[PairIdx].X0 = ClusterXs[ClusterIdx0] + ClusterOffsetX_Dist(Args->RNG);
            Output[PairIdx].Y0 = ClusterYs[ClusterIdx0] + ClusterOffsetY_Dist(Args->RNG);

            Output[PairIdx].X1 = ClusterXs[ClusterIdx1] + ClusterOffsetX_Dist(Args->RNG);
            Output[PairIdx].Y1 = ClusterYs[ClusterIdx1] + ClusterOffsetY_Dist(Args->RNG);
        }
    }
    else
    {
        for (int PairIdx = 0; PairIdx < Args->Num; PairIdx++)
        {
            Output[PairIdx].X0 = X_Dist(Args->RNG);
            Output[PairIdx].Y0 = Y_Dist(Args->RNG);
            Output[PairIdx].X1 = X_Dist(Args->RNG);
            Output[PairIdx].Y1 = Y_Dist(Args->RNG);
        }
    }

    return Output;
}

constexpr int MaxFileNameLength = 128;

void ConstructHvPairsFileName(GenerateHaversinePairsArgs *Args, char *OutFileName)
{
    sprintf_s(OutFileName, MaxFileNameLength, "output_pairs_Seed_%d_%s_%d.json", Args->Seed, Args->bCluster ? "clustered" : "uniform",
            Args->Num);
}

void WriteHaversinePairsToFileJSON(GenerateHaversinePairsArgs *Args, HaversinePair *HvPairs)
{
    char OutputFileName[MaxFileNameLength];
    ConstructHvPairsFileName(Args, OutputFileName);
    printf("Writing haversine pairs to %s\n", OutputFileName);

    FILE* OutputFile = nullptr;
    fopen_s(&OutputFile, OutputFileName, "w+");
    if (OutputFile)
    {
        fprintf(OutputFile, "{\"pairs\":[\n");
        for (int PairIdx = 0; PairIdx < Args->Num; PairIdx++)
        {
            fprintf(OutputFile, "\t{\"X0\":%.16f, \"Y0\":%.16f, \"X1\":%.16f, \"Y1\":%.16f}%s\n", HvPairs[PairIdx].X0,
                    HvPairs[PairIdx].Y0, HvPairs[PairIdx].X1, HvPairs[PairIdx].Y1, PairIdx < Args->Num - 1 ? "," : "");
        }
        fprintf(OutputFile, "]}");
        fclose(OutputFile);
    }
}

void GenerateHaversineOutput(GenerateHaversinePairsArgs *Args)
{
    printf("Generating %d %s haversine pairs (seed: %d)\n", Args->Num, Args->bCluster ? "clustered" : "uniform",
           Args->Seed);
    HaversinePair *HvPairs = GenerateHaversinePairs(Args);

    double HaversineSum = 0.0;
    for (int PairIdx = 0; PairIdx < Args->Num; PairIdx++)
    {
        HaversineSum += Reference::CalculateHaversine(HvPairs[PairIdx].X0, HvPairs[PairIdx].Y0, HvPairs[PairIdx].X1,
                                                      HvPairs[PairIdx].Y1);
    }
    double HaversineAvg = HaversineSum / Args->Num;
    printf("Calculated Haversine Sum: %f\n", HaversineAvg);

    WriteHaversinePairsToFileJSON(Args, HvPairs);

    delete[] HvPairs;
}

int main(int ArgCount, const char **ArgValues)
{
    u32 TestSeeds[] = {19854, 54285, 43745, 63179, 5897};
    bool bCluster = true;
    u32 Seed = TestSeeds[0];
    u32 Num = 10;//1000000;
    GenerateHaversinePairsArgs Args{bCluster, Seed, Num, std::mt19937{Seed}};
    GenerateHaversineOutput(&Args);

    char HvPairsFileName[MaxFileNameLength];
    ConstructHvPairsFileName(&Args, HvPairsFileName);
    FileContentsT HvPairsFile = ReadFileContents(HvPairsFileName, true);

    __debugbreak();

    return 0;
}

namespace Reference
{
    /*
     * NOTE(CKA): The following code was taken as reference code from
     *      https://github.com/cmuratori/computer_enhance/blob/main/perfaware/part2/listing_0065_haversine_formula.cpp
     *      as part of the Performance-Aware Programming Coursework available at https://www.computerenhance.com
     */

    f64 Square(f64 A)
    {
        f64 Result = (A * A);
        return Result;
    }

    f64 RadiansFromDegrees(f64 Degrees)
    {
        f64 Result = 0.01745329251994329577 * Degrees;
        return Result;
    }

    f64 CalculateHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1)
    {
        constexpr double EarthRadius = 6372.8;

        f64 lat1 = Y0;
        f64 lat2 = Y1;
        f64 lon1 = X0;
        f64 lon2 = X1;

        f64 dLat = RadiansFromDegrees(lat2 - lat1);
        f64 dLon = RadiansFromDegrees(lon2 - lon1);
        lat1 = RadiansFromDegrees(lat1);
        lat2 = RadiansFromDegrees(lat2);

        f64 a = Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2));
        f64 c = 2.0 * asin(sqrt(a));

        f64 Result = EarthRadius * c;

        return Result;
    }
} // namespace Reference

