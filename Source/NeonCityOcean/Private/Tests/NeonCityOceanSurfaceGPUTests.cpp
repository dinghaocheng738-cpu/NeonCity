#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "NeonCityOceanCDLOD.h"
#include "NeonCityOceanFFT.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "RHIStaticStates.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include <atomic>

class FNeonCityOceanSurfaceAuditCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FNeonCityOceanSurfaceAuditCS);
    SHADER_USE_PARAMETER_STRUCT(FNeonCityOceanSurfaceAuditCS, FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_ARRAY(FVector4f, SurfaceMorph, [12])
        SHADER_PARAMETER_ARRAY(FVector4f, SurfaceSizes, [4])
        SHADER_PARAMETER(FVector3f, SurfaceCamera)
        SHADER_PARAMETER(float, SurfaceSeaLevel)
        SHADER_PARAMETER(float, SurfaceAmplitude)
        SHADER_PARAMETER(uint32, SurfaceAuditCount)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float4>, oceanDisplacement)
        SHADER_PARAMETER_SAMPLER(SamplerState, smpLinearRepeat)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SurfaceAuditInputs)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, SurfaceAuditOutput)
    END_SHADER_PARAMETER_STRUCT()
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
    { return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FNeonCityOceanSurfaceAuditCS, "/NeonCity/Private/Tidewater/Ocean/SurfaceVertexAudit.usf", "Main", SF_Compute);

namespace
{
struct FSurfaceCase
{
    FString Name, Profile, CameraName;
    FNeonCityOceanParameters Ocean;
    FVector3f Camera;
    TArray<FVector4f> Inputs;
    float SeaLevel = 0, Amplitude = 1, Time = 1.f/60;
};
struct FSurfaceState
{
    TUniquePtr<FNeonCityOceanFFT> Simulation;
    TUniquePtr<FRHIGPUBufferReadback> Readback;
    TArray<uint8> Bytes;
    FString Error;
    std::atomic<bool> Submitted{false}, Polling{false}, Ready{false};
};

class FSurfaceCommand : public IAutomationLatentCommand
{
public:
    FSurfaceCommand(FAutomationTestBase* InTest, TArray<FSurfaceCase> InCases, FString InPath)
        : Test(InTest), Cases(MoveTemp(InCases)), Path(MoveTemp(InPath)), State(MakeShared<FSurfaceState,ESPMode::ThreadSafe>()), Start(FPlatformTime::Seconds()) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Start > 180)
        { Test->AddError(TEXT("Surface GPU audit timed out; evidence retained")); Release(true); return true; }
        if (!bWaiting)
        {
            const auto C = Cases[Index]; bWaiting = true; State->Submitted = false; State->Ready = false;
            ENQUEUE_RENDER_COMMAND(NeonCitySurfaceAuditSubmit)([S=State,C](FRHICommandListImmediate& RHICmdList)
            {
                S->Bytes.Reset(); S->Error.Reset(); S->Simulation = MakeUnique<FNeonCityOceanFFT>();
                S->Readback = MakeUnique<FRHIGPUBufferReadback>(TEXT("NeonCityOcean.SurfaceAudit"));
                FRDGBuilder Graph(RHICmdList);
                const auto Frame = S->Simulation->AddFrame_RenderThread(Graph,C.Ocean,C.Time,C.Time);
                if (!Frame.Displacement) { S->Error=TEXT("Surface audit FFT unavailable"); S->Ready=true; return; }
                const uint32 Count=C.Inputs.Num()/2;
                auto* Inputs = CreateStructuredBuffer(Graph,TEXT("SurfaceAudit.Inputs"),16,C.Inputs.Num(),C.Inputs.GetData(),C.Inputs.Num()*16);
                auto Desc=FRDGBufferDesc::CreateStructuredDesc(16,Count*3); Desc.Usage|=EBufferUsageFlags::SourceCopy;
                auto* Output=Graph.CreateBuffer(Desc,TEXT("SurfaceAudit.Output"));
                auto* P=Graph.AllocParameters<FNeonCityOceanSurfaceAuditCS::FParameters>();
                FNeonCityOceanCDLOD LOD;
                for (int32 I=0;I<12;++I) P->SurfaceMorph[I]=LOD.GetMorphParameters()[I];
                for (int32 I=0;I<4;++I) P->SurfaceSizes[I]=C.Ocean.Sizes[I];
                P->SurfaceCamera=C.Camera; P->SurfaceSeaLevel=C.SeaLevel; P->SurfaceAmplitude=C.Amplitude;
                P->SurfaceAuditCount=Count; P->oceanDisplacement=Frame.Displacement;
                // Vertex LOD is fractional: all nine original mips must blend, unlike integer-LOD queries.
                P->smpLinearRepeat=TStaticSamplerState<SF_Trilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI();
                P->SurfaceAuditInputs=Graph.CreateSRV(Inputs); P->SurfaceAuditOutput=Graph.CreateUAV(Output);
                TShaderMapRef<FNeonCityOceanSurfaceAuditCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
                FComputeShaderUtils::AddPass(Graph,RDG_EVENT_NAME("NeonCityOcean.SurfaceVertexAudit"),Shader,P,FIntVector(FMath::DivideAndRoundUp(Count,64u),1,1));
                AddEnqueueCopyPass(Graph,S->Readback.Get(),Output,Count*48);
                Graph.Execute(); S->Submitted=true;
            });
            return false;
        }
        if (State->Ready)
        {
            if (!State->Error.IsEmpty()) { Test->AddError(State->Error); Release(false); return true; }
            const auto& C=Cases[Index]; const uint32 Count=C.Inputs.Num()/2;
            const auto* Floats=reinterpret_cast<const float*>(State->Bytes.GetData()); bool bFinite=true;
            for (int32 I=0;I<State->Bytes.Num()/4;++I) bFinite&=FMath::IsFinite(Floats[I]);
            Test->TestTrue(C.Name+TEXT(" all GPU outputs finite"),bFinite);
            for (uint32 I=0;I<Count;++I)
            {
                Test->TestEqual(TEXT("Original deep-water vertex depth"),Floats[I*12+7],500.f);
                if (C.Profile==TEXT("calm"))
                {
                    Test->TestEqual(TEXT("Calm vertex sea level"),Floats[I*12+1],C.SeaLevel);
                    Test->TestEqual(TEXT("Calm zero displacement height"),Floats[I*12+6],0.f);
                }
            }
            const FString File=C.Name+TEXT(".bin");
            Test->TestTrue(TEXT("Save native surface field"),FFileHelper::SaveArrayToFile(State->Bytes,*(Path/File)));
            auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("profile"),C.Profile); Row->SetStringField(TEXT("camera"),C.CameraName);
            Row->SetStringField(TEXT("file"),File); Row->SetNumberField(TEXT("count"),Count); Row->SetNumberField(TEXT("bytes"),State->Bytes.Num());
            Row->SetNumberField(TEXT("amplitude"),C.Amplitude); Row->SetNumberField(TEXT("sea_level"),C.SeaLevel); Row->SetNumberField(TEXT("time"),C.Time);
            Rows.Add(MakeShared<FJsonValueObject>(Row)); ++Index; bWaiting=false;
            if (Index<Cases.Num()) return false;
            auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("source_commit"),TEXT("4811ba48d795197de5621985f404e765c0b7c0ef"));
            Report->SetStringField(TEXT("runner"),TEXT("Unreal Engine RDG / original vertex functions; float32 numeric audit, not rendered game"));
            Report->SetStringField(TEXT("adapter"),GRHIAdapterName); Report->SetArrayField(TEXT("cases"),Rows);
            FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
            Test->TestTrue(TEXT("Write surface report"),FFileHelper::SaveStringToFile(Json,*(Path/TEXT("Report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
            Release(false); return true;
        }
        if (State->Submitted && !State->Polling.exchange(true))
        {
            const uint32 Bytes=Cases[Index].Inputs.Num()/2*48;
            ENQUEUE_RENDER_COMMAND(NeonCitySurfaceAuditPoll)([S=State,Bytes](FRHICommandListImmediate& RHICmdList)
            {
                if (!S->Readback->IsReady()) { S->Polling=false; return; }
                const void* Data=S->Readback->Lock(Bytes);
                if (!Data) S->Error=TEXT("Surface staging map unavailable");
                else { S->Bytes.Append(static_cast<const uint8*>(Data),Bytes); S->Readback->Unlock(); }
                S->Readback.Reset(); S->Simulation.Reset(); S->Polling=false; S->Ready=true;
            });
        }
        return false;
    }
private:
    void Release(bool bWait)
    {
        ENQUEUE_RENDER_COMMAND(NeonCitySurfaceAuditRelease)([S=State,bWait](FRHICommandListImmediate& RHICmdList)
        { if (bWait) RHICmdList.SubmitAndBlockUntilGPUIdle(); S->Readback.Reset(); S->Simulation.Reset(); });
    }
    FAutomationTestBase* Test;
    TArray<FSurfaceCase> Cases;
    FString Path;
    TSharedRef<FSurfaceState,ESPMode::ThreadSafe> State;
    double Start;
    int32 Index=0;
    bool bWaiting=false;
    TArray<TSharedPtr<FJsonValue>> Rows;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOceanSurfaceGPUTest,"NeonCity.Tidewater.OceanSurfaceGPU",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityOceanSurfaceGPUTest::RunTest(const FString&)
{
    if (!TestTrue(TEXT("Real shader-capable RHI"),GDynamicRHI && GMaxRHIFeatureLevel>=ERHIFeatureLevel::SM5)) return false;
    auto Read=[](const TCHAR* Relative,TSharedPtr<FJsonValue>& Value)
    { FString Text; return FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/Relative)) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Value); };
    TSharedPtr<FJsonValue> Profiles,Oracle,Fixture;
    if (!Read(TEXT("ArtSource/VerticalSlice/Tidewater/Ocean/UniformProfiles.json"),Profiles)
        || !Read(TEXT("ArtSource/VerticalSlice/Tidewater/OceanSurface/LODOracle.json"),Oracle)
        || !Read(TEXT("ArtSource/VerticalSlice/Tidewater/OceanSurface/VertexFixture.json"),Fixture)) return false;
    TArray<FSurfaceCase> Cases; const auto F=Fixture->AsObject();
    for (const auto& ProfileName:F->GetArrayField(TEXT("profiles")))
    {
        FNeonCityOceanParameters Params; bool bFound=false;
        for (const auto& Profile:Profiles->AsArray()) if (Profile->AsObject()->GetStringField(TEXT("name"))==ProfileName->AsString())
        {
            const FString Hex=Profile->AsObject()->GetStringField(TEXT("packed_hex"));
            if (Hex.Len()!=448) return false;
            HexToBytes(Hex,reinterpret_cast<uint8*>(&Params)); bFound=true; break;
        }
        if (!bFound) return false;
        const auto Controls=F->GetObjectField(TEXT("controls"))->GetObjectField(ProfileName->AsString());
        for (const auto& Camera:Oracle->AsArray())
        {
            const auto O=Camera->AsObject(); FSurfaceCase C; C.Profile=ProfileName->AsString(); C.CameraName=O->GetStringField(TEXT("name"));
            C.Name=C.Profile+TEXT("--")+C.CameraName; C.Ocean=Params;
            C.Amplitude=Controls->GetNumberField(TEXT("amplitude")); C.SeaLevel=Controls->GetNumberField(TEXT("sea_level")); C.Time=F->GetNumberField(TEXT("time"));
            const auto& Pos=O->GetArrayField(TEXT("position")); C.Camera=FVector3f(Pos[0]->AsNumber(),Pos[1]->AsNumber(),Pos[2]->AsNumber());
            const auto& Nodes=O->GetArrayField(TEXT("nodes"));
            for (int32 I=0;I<Nodes.Num();I+=4) for (const auto& G:F->GetArrayField(TEXT("grid_points")))
            {
                C.Inputs.Emplace(Nodes[I]->AsNumber(),Nodes[I+1]->AsNumber(),Nodes[I+2]->AsNumber(),Nodes[I+3]->AsNumber());
                C.Inputs.Emplace(G->AsArray()[0]->AsNumber(),G->AsArray()[1]->AsNumber(),0,0);
            }
            Cases.Add(MoveTemp(C));
        }
    }
    if (!TestEqual(TEXT("Three spectra by ten camera selections"),Cases.Num(),30)) return false;
    FString Label; FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
    if (Label.IsEmpty() || Label.Contains(TEXT("/")) || Label.Contains(TEXT("\\")) || Label.Contains(TEXT(".."))) return false;
    const FString Path=FPaths::ProjectSavedDir()/TEXT("VerticalSlice")/Label/TEXT("OceanSurfaceNative");
    if (IFileManager::Get().DirectoryExists(*Path)) { AddError(TEXT("Preserve prior surface capture; use a unique run label")); return false; }
    IFileManager::Get().MakeDirectory(*Path,true);
    ADD_LATENT_AUTOMATION_COMMAND(FSurfaceCommand(this,MoveTemp(Cases),Path)); return true;
}
#endif
