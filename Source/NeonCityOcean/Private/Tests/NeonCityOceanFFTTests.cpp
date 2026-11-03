#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "NeonCityOceanFFT.h"
#include "NeonCityOceanQuery.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include <atomic>
#include <limits>

class FNeonCityOceanAuditTextureCS:public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FNeonCityOceanAuditTextureCS);
    SHADER_USE_PARAMETER_STRUCT(FNeonCityOceanAuditTextureCS,FGlobalShader);
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float4>,AuditTexture)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>,AuditBuffer)
        SHADER_PARAMETER(uint32,AuditSize)
        SHADER_PARAMETER(uint32,AuditMip)
    END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FNeonCityOceanAuditTextureCS,"/NeonCity/Private/Tidewater/Ocean/AuditTexture.usf","Main",SF_Compute);

namespace
{
struct FOceanAuditCase
{
    FString Profile,SourceProfile;
    FNeonCityOceanParameters Parameters;
    int32 Frame=0;
    double Time=0,Delta=0;
    bool bFoamProbe=false;
    FNeonCityOceanQueryRequest Query;
};
struct FOceanAuditReadback
{
    FString Name;
    uint32 Bytes=0;
    TUniquePtr<FRHIGPUBufferReadback> Readback;
};
struct FOceanAuditResult { FString Name;TArray<uint8> Data; };
struct FOceanAuditState
{
    // Simulation and staging resources are touched only by render commands.
    TUniquePtr<FNeonCityOceanFFT> Simulation;
    TUniquePtr<FNeonCityOceanQuery> Queries;
    TUniquePtr<FNeonCityOceanQuery> StressQueries;
    FNeonCityOceanQueryResult QueryResult;
    bool bQueryReady=false,bStressReady=true;
    uint64 StressExpectedRevision=0;
    TArray<FOceanAuditReadback> Readbacks;
    TArray<FOceanAuditResult> Results;
    std::atomic<bool> Submitted{false},Polling{false},Ready{false};
    FString Error;
};

void AddBufferCapture(FRDGBuilder& Graph,const FString& Name,FRDGBufferRef Buffer,FOceanAuditState& State)
{
    FOceanAuditReadback R;R.Name=Name;R.Bytes=Buffer->Desc.GetSize();
    R.Readback=MakeUnique<FRHIGPUBufferReadback>(FName(*Name));
    AddEnqueueCopyPass(Graph,R.Readback.Get(),Buffer,R.Bytes);State.Readbacks.Add(MoveTemp(R));
}

void AddTextureCapture(FRDGBuilder& Graph,const FString& Name,FRDGTextureRef Texture,FOceanAuditState& State)
{
    for(uint32 Mip=0;Mip<9;++Mip)
    {
        const uint32 Size=256>>Mip;
        auto Desc=FRDGBufferDesc::CreateStructuredDesc(16,Size*Size*4);Desc.Usage|=EBufferUsageFlags::SourceCopy;
        auto* Buffer=Graph.CreateBuffer(Desc,TEXT("NeonCityOcean.AuditTexture"));
        auto* P=Graph.AllocParameters<FNeonCityOceanAuditTextureCS::FParameters>();
        P->AuditTexture=Texture;P->AuditBuffer=Graph.CreateUAV(Buffer);P->AuditSize=Size;P->AuditMip=Mip;
        TShaderMapRef<FNeonCityOceanAuditTextureCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FComputeShaderUtils::AddPass(Graph,RDG_EVENT_NAME("NeonCityOcean.AuditMip"),Shader,P,
            FIntVector(FMath::DivideAndRoundUp(Size,8u),FMath::DivideAndRoundUp(Size,8u),4));
        AddBufferCapture(Graph,Name+FString::Printf(TEXT("-mip%u"),Mip),Buffer,State);
    }
}

class FOceanAuditCommand:public IAutomationLatentCommand
{
public:
    FOceanAuditCommand(FAutomationTestBase* InTest,TArray<FOceanAuditCase> InCases,FString InPath)
        :Test(InTest),Cases(MoveTemp(InCases)),Path(MoveTemp(InPath)),State(MakeShared<FOceanAuditState,ESPMode::ThreadSafe>()),Started(FPlatformTime::Seconds()){}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180)
        {
            Test->AddError(TEXT("Ocean native GPU capture timed out; incomplete evidence retained."));Cleanup(true);return true;
        }
        if(!bWaiting)
        {
            const auto Case=Cases[Index];State->Ready=false;State->Submitted=false;State->Polling=false;bWaiting=true;
            ENQUEUE_RENDER_COMMAND(NeonCityOceanAuditSubmit)([State=State,Case](FRHICommandListImmediate& RHICmdList)
            {
                State->Results.Reset();State->Readbacks.Reset();State->Error.Reset();
                State->bQueryReady=false;State->bStressReady=true;
                if(!State->Simulation) State->Simulation=MakeUnique<FNeonCityOceanFFT>();
                if(!State->Queries) State->Queries=MakeUnique<FNeonCityOceanQuery>();
                if(Case.Frame==0){State->Simulation->Reset_RenderThread();State->Queries->Reset_RenderThread();}
                FRDGBuilder Graph(RHICmdList);
                const FString Prefix=Case.Profile+FString::Printf(TEXT("--frame%d"),Case.Frame);
                FNeonCityOceanAudit Audit=[&](FRDGBuilder& G,const TCHAR* Stage,const FNeonCityOceanFrame& R)
                {
                    if(FCString::Strcmp(Stage,TEXT("init"))==0)
                    {
                        AddBufferCapture(G,Case.Profile+TEXT("--init--fftH0"),R.H0,*State);
                        AddBufferCapture(G,Case.Profile+TEXT("--init--fftWave"),R.Wave,*State);
                    }
                    else if(FCString::Strcmp(Stage,TEXT("rows"))==0) AddBufferCapture(G,Prefix+TEXT("-rows--fftTmp"),R.Tmp,*State);
                    else
                    {
                        AddBufferCapture(G,Prefix+TEXT("-surface--fftMipSrc"),R.MipSrc,*State);
                        AddBufferCapture(G,Prefix+TEXT("-mip5--fftMipMid"),R.MipMid,*State);
                        AddTextureCapture(G,Prefix+TEXT("--oceanDisplacement"),R.Displacement,*State);
                        AddTextureCapture(G,Prefix+TEXT("--oceanDerivatives"),R.Derivatives,*State);
                    }
                };
                const auto Frame=State->Simulation->AddFrame_RenderThread(Graph,Case.Parameters,float(Case.Time),float(Case.Delta),Audit);
                if(!Frame.Displacement) State->Error=TEXT("Native spectrum parameters or RHI unsupported");
                const auto Query=State->Queries->AddQueries_RenderThread(Graph,Frame.Displacement,Case.Parameters,Case.Query);
                if(!Query.Results || !Query.bReadbackQueued) State->Error=TEXT("Native bounded query submission failed");
                if(Case.Profile==TEXT("default") && Case.Frame<2)
                {
                    if(!State->StressQueries)State->StressQueries=MakeUnique<FNeonCityOceanQuery>();
                    auto Request=Case.Query;Request.ClientRevision+=1000;
                    if(Case.Frame==0)
                    {
                        // Real render passes: no fourth staging allocation when the ring is full.
                        for(int32 I=0;I<4;++I)
                        {
                            Request.ClientRevision=Case.Query.ClientRevision+1000+I;
                            const auto Sent=State->StressQueries->AddQueries_RenderThread(Graph,Frame.Displacement,Case.Parameters,Request);
                            if(!Sent.Results || Sent.bReadbackQueued!=(I<3))State->Error=TEXT("Query ring exceeded three slots or stopped GPU camera output");
                            if(I==2)State->StressExpectedRevision=Request.ClientRevision;
                        }
                        Request.Count=65;
                        if(State->StressQueries->AddQueries_RenderThread(Graph,Frame.Displacement,Case.Parameters,Request).Results)State->Error=TEXT("Oversized query was not rejected");
                        Request.Count=0;
                        if(State->StressQueries->AddQueries_RenderThread(Graph,Frame.Displacement,Case.Parameters,Request).Results)State->Error=TEXT("Missing camera query was not rejected");
                        Request.Count=64;Request.Points[0].X=std::numeric_limits<float>::quiet_NaN();
                        if(State->StressQueries->AddQueries_RenderThread(Graph,Frame.Displacement,Case.Parameters,Request).Results)State->Error=TEXT("Nonfinite query was not rejected");
                    }
                    else
                    {
                        const auto Sent=State->StressQueries->AddQueries_RenderThread(Graph,Frame.Displacement,Case.Parameters,Request);
                        if(!Sent.Results || !Sent.bReadbackQueued)State->Error=TEXT("Drained query ring did not accept a new frame");
                        State->StressExpectedRevision=Request.ClientRevision;
                    }
                    State->bStressReady=false;
                }
                Graph.Execute();State->Submitted=true;
            });
            return false;
        }
        if(State->Ready)
        {
            if(!State->Error.IsEmpty()){Test->AddError(State->Error);Cleanup(false);return true;}
            const auto& Case=Cases[Index];
            for(const auto& Result:State->Results)
            {
                const auto* Values=reinterpret_cast<const float*>(Result.Data.GetData());
                bool bFinite=true;float Max=0;
                for(int32 I=0;I<Result.Data.Num()/4;++I){bFinite&=FMath::IsFinite(Values[I]);Max=FMath::Max(Max,FMath::Abs(Values[I]));}
                Test->TestTrue(*FString::Printf(TEXT("Finite native field %s"),*Result.Name),bFinite);
                if(Case.Profile==TEXT("calm") && !Result.Name.Contains(TEXT("fftWave")))
                    Test->TestEqual(*FString::Printf(TEXT("Zero wind/swell field %s"),*Result.Name),Max,0.f);
                if(Result.Name.EndsWith(TEXT("-surface--fftMipSrc")))
                {
                    double Sum=0;float MaxFoam=0;
                    for(int32 I=0;I<4*256*256;++I){Sum+=FMath::Square(double(Values[I*8+1]));MaxFoam=FMath::Max(MaxFoam,Values[I*8+3]);}
                    const double RMS=FMath::Sqrt(Sum/(4*256*256));
                    if(Case.Profile!=TEXT("calm")) Test->TestTrue(TEXT("Non-flat spectrum with bounded metre-scale height"),RMS>.01 && RMS<5);
                    if(Case.bFoamProbe) Test->TestTrue(TEXT("Native nonzero foam generation and retention"),MaxFoam>.01 && MaxFoam<=1.5f);
                    Test->AddInfo(FString::Printf(TEXT("Native ocean %s frame %d: height RMS %.8fm; max foam %.8f"),*Case.Profile,Case.Frame,RMS,MaxFoam));
                }
                const FString File=Result.Name+TEXT(".bin");
                Test->TestTrue(TEXT("Save new native audit field"),FFileHelper::SaveArrayToFile(Result.Data,*(Path/File)));
                FileCounts->SetNumberField(File,Result.Data.Num());
            }
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("profile"),Case.Profile);Row->SetStringField(TEXT("source_profile"),Case.SourceProfile);
            Row->SetNumberField(TEXT("frame"),Case.Frame);Row->SetNumberField(TEXT("time"),Case.Time);Row->SetNumberField(TEXT("dt"),Case.Delta);
            auto Overrides=MakeShared<FJsonObject>();if(Case.bFoamProbe)Overrides->SetNumberField(TEXT("foamBias"),Case.Parameters.FoamBias);
            Row->SetObjectField(TEXT("overrides"),Overrides);
            const auto& Query=State->QueryResult;
            Test->TestEqual(TEXT("Async query retains request revision"),Query.Request.ClientRevision,Case.Query.ClientRevision);
            Test->TestEqual(TEXT("Async query retains simulation time"),Query.Request.SimulationTime,Case.Time);
            Test->TestEqual(TEXT("Bounded query count"),Query.Request.Count,64u);
            if(Case.Profile==TEXT("default") && Case.Frame<2) Test->TestTrue(TEXT("Three-slot overflow, drain, latest metadata and reuse verified"),State->bStressReady);
            TArray<TSharedPtr<FJsonValue>> Points,Values;
            for(uint32 I=0;I<Query.Request.Count;++I)
            {
                const auto& V=Query.Values[I];const auto& P=Query.Request.Points[I];
                Test->TestTrue(TEXT("Finite queried surface and upward unit normal"),!V.ContainsNaN() && V.Y*V.Y+V.Z*V.Z<=1.00001f);
                Test->TestEqual(TEXT("Source open-ocean sea floor"),V.W,-500.f);
                Test->TestTrue(TEXT("Readback carries issued sample position"),P==Case.Query.Points[I]);
                TArray<TSharedPtr<FJsonValue>> PP,VV;
                PP.Add(MakeShared<FJsonValueNumber>(P.X));PP.Add(MakeShared<FJsonValueNumber>(P.Y));
                for(float X:{V.X,V.Y,V.Z,V.W}) VV.Add(MakeShared<FJsonValueNumber>(X));
                Points.Add(MakeShared<FJsonValueArray>(PP));Values.Add(MakeShared<FJsonValueArray>(VV));
            }
            Row->SetArrayField(TEXT("query_points"),Points);Row->SetArrayField(TEXT("query_results"),Values);
            Row->SetNumberField(TEXT("query_sea_level"),Query.Request.SeaLevel);Row->SetNumberField(TEXT("query_amplitude"),Query.Request.Amplitude);
            Row->SetNumberField(TEXT("query_sequence"),double(Query.Sequence));Rows.Add(MakeShared<FJsonValueObject>(Row));
            ++Index;bWaiting=false;
            if(Index<Cases.Num())return false;
            auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("source_commit"),TEXT("4811ba48d795197de5621985f404e765c0b7c0ef"));
            Report->SetStringField(TEXT("runner"),TEXT("Unreal Engine RDG/D3D native shader execution; all captures little-endian float32"));
            Report->SetStringField(TEXT("adapter"),GRHIAdapterName);Report->SetArrayField(TEXT("cases"),Rows);Report->SetObjectField(TEXT("files"),FileCounts);
            FString Json;auto Writer=TJsonWriterFactory<>::Create(&Json);FJsonSerializer::Serialize(Report,Writer);
            Test->TestTrue(TEXT("Write native audit report"),FFileHelper::SaveStringToFile(Json,*(Path/TEXT("Report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
            Test->TestEqual(TEXT("All original audit fields captured"),FileCounts->Values.Num(),243);
            Test->AddInfo(TEXT("NEONCITY_NATIVE_OCEAN_GPU_CAPTURE ")+Path);Cleanup(false);return true;
        }
        if(State->Submitted && !State->Polling.exchange(true))
        {
            ENQUEUE_RENDER_COMMAND(NeonCityOceanAuditPoll)([State=State](FRHICommandListImmediate& RHICmdList)
            {
                for(const auto& R:State->Readbacks)if(!R.Readback->IsReady()){State->Polling=false;return;}
                if(State->Error.IsEmpty())
                {
                    if(!State->bQueryReady)State->bQueryReady=State->Queries->TryConsumeLatest_RenderThread(State->QueryResult);
                    if(!State->bStressReady)
                    {
                        FNeonCityOceanQueryResult Latest;
                        if(State->StressQueries->TryConsumeLatest_RenderThread(Latest))
                            State->bStressReady=Latest.Request.ClientRevision==State->StressExpectedRevision;
                    }
                    if(!State->bQueryReady || !State->bStressReady){State->Polling=false;return;}
                }
                for(auto& R:State->Readbacks)
                {
                    const auto* Data=R.Readback->Lock(R.Bytes);
                    if(!Data){State->Error=TEXT("GPU audit staging map returned no data");break;}
                    FOceanAuditResult Result;Result.Name=R.Name;Result.Data.Append(static_cast<const uint8*>(Data),R.Bytes);
                    R.Readback->Unlock();State->Results.Add(MoveTemp(Result));
                }
                State->Readbacks.Reset();State->Ready=true;
            });
        }
        return false;
    }
private:
    void Cleanup(bool bWait)
    {
        ENQUEUE_RENDER_COMMAND(NeonCityOceanAuditRelease)([State=State,bWait](FRHICommandListImmediate& RHICmdList)
        {
            if(bWait) RHICmdList.SubmitAndBlockUntilGPUIdle(); // Failed offline audit only; never a runtime stall.
            State->Readbacks.Reset();State->StressQueries.Reset();State->Queries.Reset();State->Simulation.Reset();
        });
    }
    FAutomationTestBase* Test;
    TArray<FOceanAuditCase> Cases;
    FString Path;
    TSharedRef<FOceanAuditState,ESPMode::ThreadSafe> State;
    double Started;
    int32 Index=0;bool bWaiting=false;
    TArray<TSharedPtr<FJsonValue>> Rows;
    TSharedRef<FJsonObject> FileCounts=MakeShared<FJsonObject>();
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityOceanFFTGPU,"NeonCity.Tidewater.OceanFFTGPU",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityOceanFFTGPU::RunTest(const FString&)
{
    if(!TestTrue(TEXT("Real shader-capable RHI required"),GDynamicRHI && GMaxRHIFeatureLevel>=ERHIFeatureLevel::SM5))return false;
    FString Text;TArray<TSharedPtr<FJsonValue>> Profiles;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/Ocean/UniformProfiles.json"))))return false;
    auto Reader=TJsonReaderFactory<>::Create(Text);if(!FJsonSerializer::Deserialize(Reader,Profiles))return false;
    if(!TestEqual(TEXT("Five immutable source profiles"),Profiles.Num(),5))return false;
    FString QueryText;TSharedPtr<FJsonObject> QueryInputs;
    if(!FFileHelper::LoadFileToString(QueryText,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/WaterQuery/TestInputs.json"))))return false;
    auto QueryReader=TJsonReaderFactory<>::Create(QueryText);if(!FJsonSerializer::Deserialize(QueryReader,QueryInputs))return false;
    const auto& Points=QueryInputs->GetArrayField(TEXT("points"));
    if(!TestEqual(TEXT("Canonical camera/gameplay query points"),Points.Num(),64))return false;
    TArray<FOceanAuditCase> Cases;
    for(int32 P=0;P<=Profiles.Num();++P)
    {
        const bool bProbe=P==Profiles.Num();const auto Profile=Profiles[bProbe?0:P]->AsObject();
        const FString Hex=Profile->GetStringField(TEXT("packed_hex"));
        if(!TestEqual(TEXT("Original 224-byte source uniform block"),Hex.Len(),448))return false;
        FNeonCityOceanParameters Parameters;auto* Bytes=reinterpret_cast<uint8*>(&Parameters);
        for(int32 I=0;I<224;++I)
        {
            if(!FChar::IsHexDigit(Hex[I*2]) || !FChar::IsHexDigit(Hex[I*2+1])){AddError(TEXT("Invalid source parameter byte"));return false;}
            Bytes[I]=(FParse::HexDigit(Hex[I*2])<<4)|FParse::HexDigit(Hex[I*2+1]);
        }
        const TArray<double> Times=bProbe?TArray<double>{1./60,.5,.75,1.}:P==0?TArray<double>{1./60,.05,1.}:TArray<double>{1./60};
        double Previous=0;
        for(int32 F=0;F<Times.Num();++F)
        {
            FOceanAuditCase C;C.SourceProfile=Profile->GetStringField(TEXT("name"));C.Profile=bProbe?TEXT("foam_probe"):C.SourceProfile;
            C.Parameters=Parameters;C.Frame=F;C.Time=Times[F];C.Delta=C.Time-Previous;Previous=C.Time;C.bFoamProbe=bProbe;
            if(bProbe)C.Parameters.FoamBias=F<2?1.1f:0.f;
            C.Query.Count=64;C.Query.SimulationTime=C.Time;C.Query.ClientRevision=0xA170u+Cases.Num();
            C.Query.SeaLevel=P==1?2.75f:bProbe?-.8f:0.f;C.Query.Amplitude=P==2?.65f:P==3?1.35f:1.f;
            for(int32 I=0;I<64;++I){const auto& Point=Points[I]->AsArray();C.Query.Points[I]=FVector4f(float(Point[0]->AsNumber()),float(Point[1]->AsNumber()),0,0);}
            Cases.Add(C);
        }
    }
    FString Label;
    if(!FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label))Label=TEXT("OceanAudit_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    for(TCHAR C:Label)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-')){AddError(TEXT("Invalid evidence label"));return false;}
    const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VerticalSlice")/Label/TEXT("OceanNative"));
    if(IFileManager::Get().DirectoryExists(*Path)){AddError(TEXT("Native capture directory already exists; preserve evidence"));return false;}
    if(!IFileManager::Get().MakeDirectory(*Path,true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FOceanAuditCommand(this,MoveTemp(Cases),Path));return true;
}
#endif
