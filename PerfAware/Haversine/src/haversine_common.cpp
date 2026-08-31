#include "haversine_common.h"

void HaversineArgs::Init(bool _bCluster, u32 _Seed, u32 _Num)
{
    bCluster = _bCluster;
    Seed = _Seed;
    Num = _Num;
    RNG = std::mt19937{ Seed };

#define HvArgs_BaseFileNameFmt "output/%s_Seed_%d_%s_%d.%s"
#define HvArgs_PairsPrefix "output_pairs"
#define HvArgs_AnswersPrefix "answers"
#define HvArgs_ExtensionJSON "json"
#define HvArgs_ExtensionBinary "f64"
    sprintf_s(FileName_PairsJSON, MaxFileNameLength, HvArgs_BaseFileNameFmt, HvArgs_PairsPrefix, Seed, bCluster ? "clustered" : "uniform", Num, HvArgs_ExtensionJSON);
    sprintf_s(FileName_AnswersJSON, MaxFileNameLength, HvArgs_BaseFileNameFmt, HvArgs_AnswersPrefix, Seed, bCluster ? "clustered" : "uniform", Num, HvArgs_ExtensionJSON);
    sprintf_s(FileName_AnswersBinary, MaxFileNameLength, HvArgs_BaseFileNameFmt, HvArgs_AnswersPrefix, Seed, bCluster ? "clustered" : "uniform", Num, HvArgs_ExtensionBinary);
}

void WriteHaversineOutputToFile(HaversineArgs* Args, HaversinePair* HvPairs, f64* HvAnswers)
{
    FILE* JSONPairsFile = nullptr;
    fopen_s(&JSONPairsFile, Args->FileName_PairsJSON, "w+b");
    fprintf(stdout, "Wrote haversine output to:\n");
    if (JSONPairsFile)
    {
        fprintf(JSONPairsFile, "{\"pairs\":[\n");
        for (int PairIdx = 0; PairIdx < Args->Num; PairIdx++)
        {
            fprintf(JSONPairsFile, "\t{\"X0\":%.16f, \"Y0\":%.16f, \"X1\":%.16f, \"Y1\":%.16f}%s\n", HvPairs[PairIdx].X0,
                HvPairs[PairIdx].Y0, HvPairs[PairIdx].X1, HvPairs[PairIdx].Y1, PairIdx < Args->Num - 1 ? "," : "");
        }
        fprintf(JSONPairsFile, "]}");
        fclose(JSONPairsFile);
        fprintf(stdout, "\t%s\n", Args->FileName_PairsJSON);
    }

    FILE* JSONAnswersFile = nullptr;
    fopen_s(&JSONAnswersFile, Args->FileName_AnswersJSON, "w+b");
    if (JSONAnswersFile)
    {
        fprintf(JSONAnswersFile, "{\"pairs\":[\n");
        for (int PairIdx = 0; PairIdx < Args->Num; PairIdx++)
        {
            fprintf(JSONAnswersFile, "\t{\"answer\":%.16f}%s\n", HvAnswers[PairIdx], PairIdx < Args->Num - 1 ? "," : "");
        }
        fprintf(JSONAnswersFile, "]}");
        fclose(JSONAnswersFile);
        fprintf(stdout, "\t%s\n", Args->FileName_AnswersJSON);
    }

    FILE* BinaryAnswersFile = nullptr;
    fopen_s(&BinaryAnswersFile, Args->FileName_AnswersBinary, "w+b");
    if (BinaryAnswersFile)
    {
        fwrite(HvAnswers, sizeof(f64), Args->Num, BinaryAnswersFile);
        fclose(BinaryAnswersFile);
        fprintf(stdout, "\t%s\n", Args->FileName_AnswersBinary);
    }
}

HaversinePair* GenerateHaversinePairs(HaversineArgs* Args)
{
    HaversinePair* Output = new HaversinePair[Args->Num];

    constexpr f64 MinX = -180.0;
    constexpr f64 MaxX = +180.0;
    constexpr f64 MinY = -90.0;
    constexpr f64 MaxY = +90.0;

    constexpr u32 NumClusters = 8;
    constexpr f64 ClusterOffsetRangeX = MaxX / 16.0;
    constexpr f64 ClusterOffsetRangeY = MaxY / 16.0;

    std::uniform_real_distribution<f64> X_Dist{ MinX, MaxX };
    std::uniform_real_distribution<f64> Y_Dist{ MinY, MaxY };

    std::uniform_int_distribution<u32> Cluster_Dist{ 0, NumClusters - 1 };
    std::uniform_real_distribution<f64> ClusterOffsetX_Dist{ -ClusterOffsetRangeX, +ClusterOffsetRangeX };
    std::uniform_real_distribution<f64> ClusterOffsetY_Dist{ -ClusterOffsetRangeY, +ClusterOffsetRangeY };

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

void HaversineArgs::Generate()
{
    HaversinePair* HvPairs = GenerateHaversinePairs(this);
    f64* HvAnswers = new f64[Num];

    double HaversineSum = 0.0;
    for (int PairIdx = 0; PairIdx < Num; PairIdx++)
    {
        f64 PairHaversine = Reference::CalculateHaversine(HvPairs[PairIdx].X0, HvPairs[PairIdx].Y0, HvPairs[PairIdx].X1, HvPairs[PairIdx].Y1);
        HaversineSum += PairHaversine;
        HvAnswers[PairIdx] = PairHaversine;
    }
    double HaversineAvg = HaversineSum / Num;
    Average = HaversineAvg;

    fprintf(stdout, "Generation:\n");
    fprintf(stdout, "\tSeed: %d\n\t# Pairs: %d (%s)\n", Seed, Num, bCluster ? "clustered" : "uniform");
    fprintf(stdout, "\tReference Sum: %f\n", HaversineSum);
    fprintf(stdout, "\tReference average: %f\n", HaversineAvg);
    WriteHaversineOutputToFile(this, HvPairs, HvAnswers);

    delete[] HvPairs;
    delete[] HvAnswers;
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

