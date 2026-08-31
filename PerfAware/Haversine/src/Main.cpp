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

#define Assert(Exp) if (!(Exp)) { __debugbreak(); }

template <typename T>
struct DynamicArray
{
    u64 Capacity;
    u64 Size;
    T* Data;

    static constexpr u64 DefaultCapacity = 16;

    DynamicArray()
    {
        Capacity = DefaultCapacity;
        Size = 0;
        Data = new T[Capacity];
    }

    ~DynamicArray()
    {
        if (Data) { delete[] Data; }
    }

    T& operator[](int Idx)
    {
        return Data[Idx];
    }

    void Grow()
    {
        u64 OldCapacity = Capacity;
        T* OldData = Data;

        Capacity = Capacity * 2;
        Data = new T[Capacity];
        memcpy(Data, OldData, sizeof(T) * Size);
        delete[] OldData;
    }

    void Add(T Item)
    {
        if (Size == Capacity) { Grow(); }

        Data[Size] = Item;
        Size++;
    }

    T& Last()
    {
        return Data[Size - 1];
    }
};

struct FileContentsT
{
    u64 Size;
    u8* Contents;
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

enum JSONType
{
    JSONType_Unspecified,
    JSONType_Object,
    JSONType_Array,
    JSONType_String,
    JSONType_NumberInt,
    JSONType_NumberFloat,
    JSONType_Boolean,
    JSONType_Null,
    JSONType_Error
};

struct JSONObject;

struct JSONValue
{
    JSONType Type;
    union
    {
        DynamicArray<JSONObject*>* List;
        char* String;
        s64 NumberInt;
        f64 NumberFloat;
        u64 Boolean;
    };
};

struct JSONObject
{
    char* Key = nullptr;
    JSONValue Value;
    
    JSONObject* GetProperty(const char* Key);
    JSONObject* GetItem(int Idx);

    DynamicArray<JSONObject*>& GetList();
    char*& GetString();
    s64& GetInt();
    f64& GetFloat();
    u64& GetBool();
    bool IsNull();
};

JSONObject* JSONObject::GetProperty(const char* Key)
{
    Assert(Value.Type == JSONType_Object && Value.List && Key);

    JSONObject* Result = nullptr;

    for (int Idx = 0; Key && Idx < Value.List->Size; Idx++)
    {
        if (strcmp(Key, Value.List->Data[Idx]->Key) == 0) { Result = Value.List->Data[Idx]; break; }
    }

    return Result;
}

JSONObject* JSONObject::GetItem(int Idx)
{
    Assert(Value.Type == JSONType_Array && Value.List && Idx < Value.List->Size);

    JSONObject* Result = nullptr;
    if (Idx < Value.List->Size) { Result = Value.List->Data[Idx]; }
    return Result;
}

DynamicArray<JSONObject*>& JSONObject::GetList()
{
    Assert(Value.Type == JSONType_Object || Value.Type == JSONType_Array);
    return *Value.List;
}

char*& JSONObject::GetString()
{
    Assert(Value.Type == JSONType_String);
    return Value.String;
}

s64& JSONObject::GetInt()
{
    Assert(Value.Type == JSONType_NumberInt);
    return Value.NumberInt;
}

f64& JSONObject::GetFloat()
{
    Assert(Value.Type == JSONType_NumberFloat);
    return Value.NumberFloat;
}

u64& JSONObject::GetBool()
{
    Assert(Value.Type == JSONType_Boolean);
    return Value.Boolean;
}

bool JSONObject::IsNull()
{
    return Value.Type == JSONType_Null;
}

enum JSONToken
{
    JSONToken_Unspecified,
    JSONToken_LeftCurly,
    JSONToken_RightCurly,
    JSONToken_LeftSquare,
    JSONToken_RightSquare,
    JSONToken_Colon,
    JSONToken_Comma,
    JSONToken_String,
    JSONToken_Number,
    JSONToken_LiteralBoolean,
    JSONToken_LiteralNull,
    JSONToken_Error,
    JSONToken_End
};

struct JSONObjectStack
{
    DynamicArray<JSONObject*> Stack;

    bool IsEmpty() { return Stack.Size == 0; }
    void Push(JSONObject* NewObject) { Stack.Add(NewObject); }
    void Pop() { Assert(!IsEmpty()); if (!IsEmpty()) { Stack[Stack.Size - 1] = {}; Stack.Size--; } }
    JSONObject* Top() { JSONObject* Result = nullptr; if (!IsEmpty()) { Result = Stack[Stack.Size - 1]; } return Result; }
};

struct JSONParseContext
{
    FileContentsT JSONContents;
    u64 ReadIdx;

    JSONObject Root;
    JSONObjectStack Stack;

    bool bError;
    bool bEnd;

    bool bColon;
    bool bComma;

#define ENABLE_DEBUG_STREAM() (_DEBUG && 0)
#if ENABLE_DEBUG_STREAM()
    DynamicArray<JSONToken> Debug_TokenStream;
    DynamicArray<JSONValue> Debug_ValueStream;

    void Debug_PrintToken(int& TokenIdx, int& ValueIdx);
    void Debug_PrintStream();
#endif // ENABLE_DEBUG_STREAM()

    bool IsCharValidNumber(char X);

    JSONToken PeekNextToken();
    char* ParseString();
    JSONValue ParseNumber();
    JSONValue ParseLiteral();

    void ParseToken();
};

#if ENABLE_DEBUG_STREAM()
void JSONParseContext::Debug_PrintToken(int& TokenIdx, int& ValueIdx)
{
    if (TokenIdx < Debug_TokenStream.Size)
    {
        JSONToken Token = Debug_TokenStream[TokenIdx];
        switch (Token)
        {
            case JSONToken_LeftCurly: { printf("{\n"); } break;
            case JSONToken_RightCurly: { printf("}\n"); } break;
            case JSONToken_LeftSquare: { printf("[\n"); } break;
            case JSONToken_RightSquare: { printf("]\n"); } break;
            case JSONToken_Colon: { printf(":\n"); } break;
            case JSONToken_Comma: { printf(",\n"); } break;

            case JSONToken_String:
            {
                Assert(ValueIdx < Debug_ValueStream.Size);
                Assert(JSONType_String == Debug_ValueStream[ValueIdx].Type);
                char* ValueString = Debug_ValueStream[ValueIdx].String;
                printf("String -> \"%s\"\n", ValueString);
                ValueIdx++;
            } break;
            case JSONToken_Number:
            {
                Assert(ValueIdx < Debug_ValueStream.Size);
                JSONType Type = Debug_ValueStream[ValueIdx].Type;
                if (Type == JSONType_NumberInt)
                {
                    s64 ValueNumberInt = Debug_ValueStream[ValueIdx].NumberInt;
                    printf("NumberInt -> %lld\n", ValueNumberInt);
                }
                else if (Type == JSONType_NumberFloat)
                {
                    f64 ValueNumberFloat = Debug_ValueStream[ValueIdx].NumberFloat;
                    printf("NumberFloat -> %f\n", ValueNumberFloat);
                }
                else { Assert(false); }
                ValueIdx++;
            } break;
            case JSONToken_LiteralBoolean:
            {
                Assert(ValueIdx < Debug_ValueStream.Size);
                Assert(Debug_ValueStream[ValueIdx].Type == JSONType_Boolean);
                printf("Boolean -> %s\n", Debug_ValueStream[ValueIdx].Boolean ? "true" : "false");
                ValueIdx++;
            } break;
            case JSONToken_LiteralNull:
            {
                Assert(ValueIdx < Debug_ValueStream.Size);
                Assert(Debug_ValueStream[ValueIdx].Type == JSONType_Null);
                printf("Null\n");
                ValueIdx++;
            } break;

            case JSONToken_Error: { printf("Token_Error\n"); } break;
            case JSONToken_End: { printf("Token_End\n"); } break;

            case JSONToken_Unspecified:
            default:
            {
                Assert(false);
            } break;
        }

        TokenIdx++;
    }
}
void JSONParseContext::Debug_PrintStream()
{
    int TokenIdx = 0;
    int ValueIdx = 0;
    while (TokenIdx < Debug_TokenStream.Size)
    {
        Debug_PrintToken(TokenIdx, ValueIdx);
    }
}
#endif // ENABLE_DEBUG_STREAM()

bool JSONParseContext::IsCharValidNumber(char X)
{
    // NOTE(CKA): This doesn't support scientific notation as-is
    return ('0' <= X && X <= '9') || X == '-' || X == '.';
}

JSONToken JSONParseContext::PeekNextToken()
{
    if (bError) { return JSONToken_Error; }
    if (bEnd || ReadIdx >= JSONContents.Size) { return JSONToken_End; }

    int PeekIdx = 0;
    JSONToken Token = JSONToken_Unspecified;
    while ((ReadIdx + PeekIdx) < JSONContents.Size)
    {
        u8 CurrChar = JSONContents.Contents[ReadIdx + PeekIdx];
        switch (CurrChar)
        {
            case '{': { Token = JSONToken_LeftCurly; } break;
            case '}': { Token = JSONToken_RightCurly; } break;
            case '[': { Token = JSONToken_LeftSquare; } break;
            case ']': { Token = JSONToken_RightSquare; } break;
            case ':': { Token = JSONToken_Colon; } break;
            case ',': { Token = JSONToken_Comma; } break;
            case '"': { Token = JSONToken_String; } break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
            case '-': { Token = JSONToken_Number; } break;

            case 't':
            case 'f': { Token = JSONToken_LiteralBoolean; } break;

            case 'n': { Token = JSONToken_LiteralNull; } break;

            case '\0': { Token = JSONToken_End; } break;

            default: { } break;
        }

        if (Token != JSONToken_Unspecified) { break; }
        else { PeekIdx++; }
    }

    if ((ReadIdx + PeekIdx) >= JSONContents.Size) { Token = JSONToken_End; }
    else { ReadIdx += PeekIdx; }

    return Token;
}

char* JSONParseContext::ParseString()
{
    Assert(JSONContents.Contents[ReadIdx] == '"');
    u64 BeginQuoteIdx = ReadIdx;

    ReadIdx++;
    while (ReadIdx < JSONContents.Size && JSONContents.Contents[ReadIdx] != '"') { ReadIdx++; }

    Assert(JSONContents.Contents[ReadIdx] == '"');
    u64 EndQuoteIdx = ReadIdx;

    char* Result = nullptr;
    if (ReadIdx < JSONContents.Size && 
        BeginQuoteIdx < EndQuoteIdx &&
        JSONContents.Contents[BeginQuoteIdx] == '"' &&
        JSONContents.Contents[EndQuoteIdx] == '"')
    {
        u64 BeginStringIdx = BeginQuoteIdx + 1;
        u64 EndStringIdx = EndQuoteIdx - 1;
        u64 StringLength = EndStringIdx - BeginStringIdx + 1;

        Result = new char[StringLength + 1];
        for (int StringIdx = 0; StringIdx < StringLength; StringIdx++)
        {
            Result[StringIdx] = JSONContents.Contents[BeginStringIdx + StringIdx];
        }
        Result[StringLength] = '\0';

        ReadIdx++;
    }

    return Result;
}

JSONValue JSONParseContext::ParseNumber()
{
    JSONValue Result{ JSONType_Error };

    Assert(IsCharValidNumber(JSONContents.Contents[ReadIdx]));
    u64 BeginNumberIdx = ReadIdx;
    bool bDecimal = false;

    ReadIdx++;
    bool bValid = true;
    while (bValid && ReadIdx < JSONContents.Size && IsCharValidNumber(JSONContents.Contents[ReadIdx]))
    {
        if (JSONContents.Contents[ReadIdx] == '.')
        {
            if (bDecimal) { bValid = false; }
            else { bDecimal = true; }
        }
        ReadIdx++;
    }

    Assert(bValid);
    if (bValid && IsCharValidNumber(JSONContents.Contents[BeginNumberIdx]))
    {
        if (bDecimal)
        {
            char* FirstCharAfterNumber = nullptr;
            Result.Type = JSONType_NumberFloat;
            Result.NumberFloat = strtod((const char*)&JSONContents.Contents[BeginNumberIdx], &FirstCharAfterNumber);
            Assert((u8*)FirstCharAfterNumber == (JSONContents.Contents + ReadIdx));
        }
        else
        {
            char* FirstCharAfterNumber = nullptr;
            Result.Type = JSONType_NumberInt;
            Result.NumberInt = strtoll((const char*)&JSONContents.Contents[BeginNumberIdx], &FirstCharAfterNumber, 10);
            Assert((u8*)FirstCharAfterNumber == (JSONContents.Contents + ReadIdx));
        }
    }

    return Result;
}

JSONValue JSONParseContext::ParseLiteral()
{
    JSONValue Result{ JSONType_Error };

    auto IsValidLiteralString = [&](const char* Literal, int LiteralLength) -> bool
    {
        if ((ReadIdx + LiteralLength - 1) < JSONContents.Size)
        {
            int Idx = 0;
            while (Idx < LiteralLength && JSONContents.Contents[ReadIdx + Idx] == Literal[Idx]) { Idx++; }
            return Idx == LiteralLength;
        }
        return false;
    };

    constexpr const char* LiteralTrue = "true";
    constexpr size_t LiteralTrueLength = sizeof("true") - 1;
    constexpr const char* LiteralFalse = "false";
    constexpr size_t LiteralFalseLength = sizeof("false") - 1;
    constexpr const char* LiteralNull = "null";
    constexpr size_t LiteralNullLength = sizeof("null") - 1;

    switch (JSONContents.Contents[ReadIdx])
    {
        case 't':
        {
            if (IsValidLiteralString(LiteralTrue, LiteralTrueLength))
            {
                Result.Type = JSONType_Boolean;
                Result.Boolean = true;
                ReadIdx += LiteralTrueLength;
            }
            else { bError = true; }
        } break;
        case 'f':
        {
            if (IsValidLiteralString(LiteralFalse, LiteralFalseLength))
            {
                Result.Type = JSONType_Boolean;
                Result.Boolean = false;
                ReadIdx += LiteralFalseLength;
            }
            else { bError = true; }
        } break;
        case 'n':
        {
            if (IsValidLiteralString(LiteralNull, LiteralNullLength))
            {
                Result.Type = JSONType_Null;
                ReadIdx += LiteralNullLength;
            }
            else { bError = true; }
        } break;
        default:
        {
            bError = true;
        } break;
    }
    return Result;
}

void JSONParseContext::ParseToken()
{
    JSONToken Token = PeekNextToken();
    // TODO(CKA): Check for bComma when adding to a list (_besides_ the first object)
    switch (Token)
    {
        case JSONToken_LeftCurly:
        case JSONToken_LeftSquare:
        {
            if (Stack.IsEmpty() && Token == JSONToken_LeftCurly) { Stack.Push(&Root); }
            else if (Stack.Top() && Stack.Top()->Value.Type == JSONType_Object)
            {
                JSONObject* Last = Stack.Top()->Value.List->Last();
                if (Last != nullptr && bColon && Last->Value.Type == JSONType_Unspecified)
                {
                    Last->Value.Type = Token == JSONToken_LeftCurly ? JSONType_Object : JSONType_Array;
                    Last->Value.List = new DynamicArray<JSONObject*>{};
                    Stack.Push(Last);
                }
                else { Assert(false); bError = true; }
            }
            else if (Stack.Top() && Stack.Top()->Value.Type == JSONType_Array)
            {
                if (!bColon && (Stack.Top()->Value.List->Size == 0 || bComma))
                {
                    JSONObject* NewObject = new JSONObject{ };
                    NewObject->Value.Type = Token == JSONToken_LeftCurly ? JSONType_Object : JSONType_Array;
                    NewObject->Value.List = new DynamicArray<JSONObject*>{};
                    Stack.Top()->Value.List->Add(NewObject);
                    Stack.Push(NewObject);
                }
                else { Assert(false); bError = true; }
            }
            else { Assert(false); bError = true; }
            ReadIdx++;
        } break;

        case JSONToken_RightCurly:
        {
            if (!Stack.IsEmpty() && Stack.Top()->Value.Type == JSONType_Object) { Stack.Pop(); }
            else { Assert(false); bError = true; }
            ReadIdx++;
            if (Stack.IsEmpty()) { bEnd = true; } // Mark end if root object is closed
        } break;

        case JSONToken_RightSquare:
        {
            if (!Stack.IsEmpty() && Stack.Top()->Value.Type == JSONType_Array) { Stack.Pop(); }
            else { Assert(false); bError = true; }
            ReadIdx++;
        } break;

        case JSONToken_Colon:
        {
            if (Stack.Top()->Value.Type != JSONType_Object || bColon || bComma) { Assert(false); bError = true; }
            ReadIdx++;
        } break;

        case JSONToken_Comma:
        {
            if (bColon || bComma) { Assert(false); bError = true; }
            ReadIdx++;
        } break;

        case JSONToken_String:
        {
            int StringLength = 0;
            char* NewString = ParseString();
            if (NewString)
            {
                if (bColon)
                {
                    JSONObject* Top = Stack.Top();
                    JSONObject* Last = Top->Value.List->Last();
                    if (Top && Top->Value.Type == JSONType_Object && Top->Value.List->Size != 0 &&
                        Last && Last->Key && Last->Value.Type == JSONType_Unspecified)
                    {
                        Last->Value.Type = JSONType_String;
                        Last->Value.String = NewString;
                    }
                    else { Assert(false); bError = true; }
                }
                else
                {
                    JSONObject* NewObject = new JSONObject{};
                    JSONObject* Top = Stack.Top();
                    if (Top && Top->Value.Type == JSONType_Object)
                    {
                        NewObject->Key = NewString;
                        NewObject->Value.Type = JSONType_Unspecified;
                        Top->Value.List->Add(NewObject);
                    }
                    else if (Top && Top->Value.Type == JSONType_Array)
                    {
                        NewObject->Value.Type = JSONType_String;
                        NewObject->Value.String = NewString;
                        Top->Value.List->Add(NewObject);
                    }
                    else { Assert(false); bError = true; }
                }
            }
            else { Assert(false); bError = true; }
            if (bError) { delete[] NewString; }
        #if ENABLE_DEBUG_STREAM()
            JSONValue Debug_JSONString = { JSONType_String };
            Debug_JSONString.String = NewString;
            Debug_ValueStream.Add(Debug_JSONString);
        #endif ENABLE_DEBUG_STREAM()
        } break;

        case JSONToken_Number:
        case JSONToken_LiteralBoolean:
        case JSONToken_LiteralNull:
        {
            JSONValue NewValue = {};
            if (Token == JSONToken_Number) { NewValue = ParseNumber(); }
            else if (Token == JSONToken_LiteralBoolean || Token == JSONToken_LiteralNull) { NewValue = ParseLiteral(); }

            if (NewValue.Type != JSONType_Unspecified && NewValue.Type != JSONType_Error)
            {
                JSONObject* Top = Stack.Top();
                if (Top && Top->Value.Type == JSONType_Object)
                {
                    JSONObject* Last = Top->Value.List->Last();
                    if (Last && Last->Key && Last->Value.Type == JSONType_Unspecified) { Last->Value = NewValue; }
                    else { Assert(false); bError = true; }
                }
                else if (Top && Top->Value.Type == JSONType_Array)
                {
                    JSONObject* NewObject = new JSONObject{ nullptr, NewValue };
                    Top->Value.List->Add(NewObject);
                }
                else { Assert(false); bError = true; }
            }
            else { Assert(false); bError = true; }
        #if ENABLE_DEBUG_STREAM()
            Debug_ValueStream.Add(NewValue);
        #endif ENABLE_DEBUG_STREAM()
        } break;

        case JSONToken_End: { bEnd = true; } break;

        case JSONToken_Error:
        case JSONToken_Unspecified:
        default: { bError = true; } break;
    }

    bColon = Token == JSONToken_Colon;
    bComma = Token == JSONToken_Comma;

#if ENABLE_DEBUG_STREAM()
    Debug_TokenStream.Add(Token);
    if (bError) { Debug_TokenStream.Add(JSONToken_Error); }
#endif // ENABLE_DEBUG_PARSE_STREAM()
}

JSONObject JSONParse(FileContentsT JSONContents)
{
    if (JSONContents.Size == 0 || !JSONContents.Contents) { return JSONObject{}; }

    JSONParseContext Context;
    Context.JSONContents = JSONContents;
    Context.ReadIdx = 0;
    Context.Root = {};
    Context.Root.Value.Type = JSONType_Object;
    Context.Root.Value.List = new DynamicArray<JSONObject*>;
    Context.bError = false;
    Context.bEnd = false;

    while (!Context.bError && !Context.bEnd)
    {
        Context.ParseToken();
    }

#if ENABLE_DEBUG_STREAM()
    Context.Debug_PrintStream();
#endif // ENABLE_DEBUG_STREAM()

    return Context.Root;
}

void FreeJSONObject(JSONObject* Object)
{
    if (!Object) { return; }

    if (Object->Key) { delete[] Object->Key; }
    switch (Object->Value.Type)
    {
        case JSONType_Object:
        case JSONType_Array:
        {
            for (int Idx = 0; Idx < Object->Value.List->Size; Idx++) { FreeJSONObject(Object->Value.List->Data[Idx]); }
            delete Object->Value.List;
        } break;

        case JSONType_String:
        {
            if (Object->Value.String) { delete[] Object->Value.String; }
        } break;

        case JSONType_NumberInt:
        case JSONType_NumberFloat:
        case JSONType_Boolean:
        case JSONType_Null:
        {
        } break;

        case JSONType_Unspecified:
        case JSONType_Error:
        default:
        {
            Assert(false);
        } break;
    }
    delete Object;
}

void FreeJSONRoot(JSONObject* Root)
{
    if (!Root) { return; }

    for (int Idx = 0; Root->Value.List && Idx < Root->Value.List->Size; Idx++)
    {
        FreeJSONObject(Root->Value.List->Data[Idx]);
    }
    delete Root->Value.List;
    *Root = {};
}

namespace Reference
{
    f64 Square(f64 A);
    f64 RadiansFromDegrees(f64 Degrees);
    f64 CalculateHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1);
} // namespace Reference

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
    FileContentsT HvPairsFileText = ReadFileContents(HvPairsFileName, true);
    JSONObject Root = JSONParse(HvPairsFileText);

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

