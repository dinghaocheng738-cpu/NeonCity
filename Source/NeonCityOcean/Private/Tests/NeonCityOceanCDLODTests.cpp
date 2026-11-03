#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "NeonCityOceanCDLOD.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOceanCDLODTest, "NeonCity.Tidewater.OceanCDLOD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNeonCityOceanCDLODTest::RunTest(const FString&)
{
    const FString Root = FPaths::ProjectDir() / TEXT("ArtSource/VerticalSlice/Tidewater/OceanSurface");
    auto ReadJson = [this, &Root](const TCHAR* File, TSharedPtr<FJsonValue>& Out)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *(Root / File))) { AddError(FString(TEXT("Missing source fixture: ")) + File); return false; }
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out)) { AddError(TEXT("Invalid source JSON")); return false; }
        return true;
    };
    TSharedPtr<FJsonValue> Oracle;
    if (!ReadJson(TEXT("LODOracle.json"), Oracle)) return false;
    FNeonCityOceanCDLOD LOD;
    TStaticArray<FVector4d, 6> Planes;
    for (const auto& Value : Oracle->AsArray())
    {
        const auto Case = Value->AsObject();
        const FString Name = Case->GetStringField(TEXT("name"));
        const auto& P = Case->GetArrayField(TEXT("position"));
        const FVector3d Camera(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
        const auto& Frustum = Case->GetArrayField(TEXT("planes"));
        for (int32 I = 0; I < 6; ++I)
        {
            const auto& Plane = Frustum[I]->AsArray();
            Planes[I] = FVector4d(Plane[0]->AsNumber(), Plane[1]->AsNumber(), Plane[2]->AsNumber(), Plane[3]->AsNumber());
        }
        TestTrue(Name + TEXT(" valid bounded selection"), LOD.Select(Camera, Planes));
        const auto& Expected = Case->GetArrayField(TEXT("nodes"));
        const auto& Nodes = LOD.GetNodes();
        TestEqual(Name + TEXT(" node count"), Nodes.Num(), int32(Case->GetNumberField(TEXT("node_count"))));
        if (Expected.Num() != Nodes.Num() * 4) return false;
        int32 Different = 0;
        for (int32 I = 0; I < Nodes.Num(); ++I)
            for (int32 K = 0; K < 4; ++K)
                Different += Nodes[I][K] != float(Expected[I * 4 + K]->AsNumber()) ? 1 : 0;
        TestEqual(Name + TEXT(" every node and stable source order"), Different, 0);
        const auto& Counts = Case->GetArrayField(TEXT("lod_counts"));
        for (int32 I = 0; I < FNeonCityOceanCDLOD::LevelCount; ++I)
            TestEqual(Name + FString::Printf(TEXT(" level %d"), I), LOD.GetLevelCounts()[I], uint32(Counts[I]->AsNumber()));
    }

    TArray<FVector3f> Vertices; TArray<uint32> Indices;
    FNeonCityOceanCDLOD::BuildGrid(Vertices, Indices);
    TArray<uint8> SourceVertices, SourceIndices;
    if (!FFileHelper::LoadFileToArray(SourceVertices, *(Root / TEXT("GridVertices.f32")))
        || !FFileHelper::LoadFileToArray(SourceIndices, *(Root / TEXT("GridIndices.u32")))) return false;
    TestEqual(TEXT("Grid vertex byte count"), SourceVertices.Num(), Vertices.Num() * int32(sizeof(FVector3f)));
    TestEqual(TEXT("Grid index byte count"), SourceIndices.Num(), Indices.Num() * int32(sizeof(uint32)));
    if (SourceVertices.Num() != Vertices.Num() * sizeof(FVector3f) || SourceIndices.Num() != Indices.Num() * sizeof(uint32)) return false;
    TestEqual(TEXT("Every source grid vertex bit"), FMemory::Memcmp(SourceVertices.GetData(), Vertices.GetData(), SourceVertices.Num()), 0);
    TestEqual(TEXT("Every source triangle and winding bit"), FMemory::Memcmp(SourceIndices.GetData(), Indices.GetData(), SourceIndices.Num()), 0);

    TSharedPtr<FJsonValue> Contract;
    if (!ReadJson(TEXT("Contract.json"), Contract)) return false;
    const FString ExpectedHex = Contract->AsObject()->GetStringField(TEXT("lod_uniform_hex"));
    const auto& Morph = LOD.GetMorphParameters();
    const FString ActualHex = BytesToHex(reinterpret_cast<const uint8*>(Morph.GetData()), sizeof(FVector4f) * FNeonCityOceanCDLOD::LevelCount);
    TestEqual(TEXT("Every source float32 morph uniform bit"), ActualHex.ToLower(), ExpectedHex);

    TestFalse(TEXT("Invalid camera rejected"), LOD.Select(FVector3d(std::numeric_limits<double>::quiet_NaN(), 0, 0), Planes));
    TestEqual(TEXT("Invalid input clears previous visible mesh"), LOD.GetNodes().Num(), 0);
    Planes[0] = FVector4d(0, 0, 0, 0);
    TestFalse(TEXT("Degenerate frustum rejected"), LOD.Select(FVector3d::ZeroVector, Planes));
    return !HasAnyErrors();
}
#endif
