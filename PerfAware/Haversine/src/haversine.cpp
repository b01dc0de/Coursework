#include "haversine_common.h"

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

void FreeFileContents(FileContentsT* InFileContents)
{
    if (InFileContents)
    {
        delete[] InFileContents->Contents;
        *InFileContents = {};
    }
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

    bool IsCharValidNumber(char X);

    JSONToken PeekNextToken();
    char* ParseString();
    JSONValue ParseNumber();
    JSONValue ParseLiteral();

    void ParseToken();
};

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

            default: {} break;
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
        } break;

        case JSONToken_End: { bEnd = true; } break;

        case JSONToken_Error:
        case JSONToken_Unspecified:
        default: { bError = true; } break;
    }

    bColon = Token == JSONToken_Colon;
    bComma = Token == JSONToken_Comma;
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

u64 Prof_Begin = 0;
u64 Prof_ReadFile = 0;
u64 Prof_Parse = 0;
u64 Prof_Sum = 0;
u64 Prof_Free = 0;
u64 Prof_End = 0;

double CalculateHaversineAverageFromJSON(const char* JSONFileName)
{
    Prof_Begin = ReadCPUTimer();

    Prof_ReadFile = ReadCPUTimer();
    FileContentsT HvPairsFileText = ReadFileContents(JSONFileName, true);

    Prof_Parse = ReadCPUTimer();
    JSONObject Root = JSONParse(HvPairsFileText);

    double HaversineSum = 0.0;
    JSONObject* Pairs = Root.GetProperty("pairs");
    Assert(Pairs && Pairs->Value.Type == JSONType_Array);

    Prof_Sum = ReadCPUTimer();
    for (int PairIdx = 0; PairIdx < Pairs->Value.List->Size; PairIdx++)
    {
        JSONObject* Pair = Pairs->GetItem(PairIdx);

        JSONObject* X0 = Pair->GetProperty("X0");
        JSONObject* Y0 = Pair->GetProperty("Y0");
        JSONObject* X1 = Pair->GetProperty("X1");
        JSONObject* Y1 = Pair->GetProperty("Y1");

        Assert(X0 && Y0 && X1 && Y1);
        Assert(X0->Value.Type == JSONType_NumberFloat && Y0->Value.Type == JSONType_NumberFloat &&
            X1->Value.Type == JSONType_NumberFloat && Y1->Value.Type == JSONType_NumberFloat);

        HaversineSum += Reference::CalculateHaversine(X0->GetFloat(), Y0->GetFloat(), X1->GetFloat(), Y1->GetFloat());
    }
    double HaversineAvg = HaversineSum / Pairs->Value.List->Size;

    Prof_Free = ReadCPUTimer();
    FreeFileContents(&HvPairsFileText);
    FreeJSONRoot(&Root);
    Prof_End = ReadCPUTimer();

    return HaversineAvg;
}

void PrintTimings()
{
    // NOTE(CKA): Code for PrintTimeElapsed is taken from
    //      listing_0075_timed_haversine_main.cpp from PerfAware coursework
    auto PrintTimeElapsed = [](char const* Label, u64 TotalTSCElapsed, u64 Begin, u64 End)
    {
        u64 Elapsed = End - Begin;
        f64 Percent = 100.0 * ((f64)Elapsed / (f64)TotalTSCElapsed);
        printf("  %s: %llu (%.2f%%)\n", Label, Elapsed, Percent);
    };

    u64 TotalCPUElapsed = Prof_End - Prof_Begin;
        
    u64 CPUFreq = EstimateCPUTimerFreq();
    if (CPUFreq)
    {
        printf("\nTotal time: %0.4fms (CPU freq %llu)\n", 1000.0 * (f64)TotalCPUElapsed / (f64)CPUFreq, CPUFreq);
    }

    PrintTimeElapsed("Startup", TotalCPUElapsed, Prof_Begin, Prof_ReadFile);
    PrintTimeElapsed("ReadFile", TotalCPUElapsed, Prof_ReadFile, Prof_Parse);
    PrintTimeElapsed("Parse", TotalCPUElapsed, Prof_Parse, Prof_Sum);
    PrintTimeElapsed("Sum", TotalCPUElapsed, Prof_Sum, Prof_Free);
    PrintTimeElapsed("Free", TotalCPUElapsed, Prof_Free, Prof_End);
}

int main(int ArgCount, const char** ArgValues)
{
    (void)ArgCount; (void)ArgValues;

    //EstimateCPUTimerFreq();

    u32 TestSeeds[] = { 19854, 54285, 43745, 63179, 5897 };
    bool bCluster = true;
    u32 Seed = TestSeeds[0];
    u32 Num = 1000000;
    HaversineArgs Args = {};
    Args.Init(bCluster, Seed, Num);
    constexpr bool bGenerate = true;
    if (bGenerate) { Args.Generate(); }

    double HaversineAvg = CalculateHaversineAverageFromJSON(Args.FileName_PairsJSON);
    double Difference = HaversineAvg - Args.Average;
    fprintf(stdout, "Validation:\n\tCalculated average: %.16f\n\tDifference: %.16f\n", HaversineAvg, Difference);

    PrintTimings();

    return 0;
}

