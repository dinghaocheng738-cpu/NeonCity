// Direct Tidewater WaterQuery open-ocean composition, fixed revision in source manifest.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT: Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "NeonCityOceanQuery.h"
#include "NeonCityOceanFFT.h"
#include "NeonCityOceanWake.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "RHIStaticStates.h"
#include "DataDrivenShaderPlatformInfo.h"

class FNeonCityOceanWaterQueryCS:public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FNeonCityOceanWaterQueryCS);
    SHADER_USE_PARAMETER_STRUCT(FNeonCityOceanWaterQueryCS,FGlobalShader);
    class FCoastal:SHADER_PERMUTATION_BOOL("NC_COASTAL_QUERIES");
    class FWake:SHADER_PERMUTATION_BOOL("NC_WAKE_QUERIES");
    using FPermutationDomain=TShaderPermutationDomain<FCoastal,FWake>;
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_ARRAY(FVector4f,QuerySizes,[4])
        SHADER_PARAMETER(float,QueryAmplitude)
        SHADER_PARAMETER(float,QuerySeaLevel)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>,WakeDisplay)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>,WakeNear)
        SHADER_PARAMETER_SAMPLER(SamplerState,WakeSampler)
        SHADER_PARAMETER(FVector4f,WakeControls)
        SHADER_PARAMETER(FVector4f,WakeBoat)
        SHADER_PARAMETER(FVector2f,WakeHullScale)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>,TerrainDepth)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>,IslandHeight)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>,IslandShore)
        SHADER_PARAMETER(FVector4f,ShoreControls)
        SHADER_PARAMETER_SAMPLER(SamplerState,TerrainSampler)
        SHADER_PARAMETER(FVector4f,TerrainBounds)
        SHADER_PARAMETER(FVector2f,TerrainOrigin)
        SHADER_PARAMETER(float,TerrainGridSize)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<float4>,oceanDisplacement)
        SHADER_PARAMETER_SAMPLER(SamplerState,smpLinearRepeat)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>,queryInputs)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>,queryResults)
    END_SHADER_PARAMETER_STRUCT()
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P){return IsFeatureLevelSupported(P.Platform,ERHIFeatureLevel::SM5);}
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& P,FShaderCompilerEnvironment& E)
    {FGlobalShader::ModifyCompilationEnvironment(P,E);E.SetDefine(TEXT("NEONCITY_UE_SHADER"),1);}
};
IMPLEMENT_GLOBAL_SHADER(FNeonCityOceanWaterQueryCS,"/NeonCity/Private/OceanExperience/Wake/WaterQuery.usf","Main",SF_Compute);

struct FNeonCityOceanQuery::FState
{
    struct FSlot
    {
        TUniquePtr<FRHIGPUBufferReadback> Readback;
        FNeonCityOceanQueryRequest Request;
        uint64 Sequence=0;
        bool bInFlight=false;
    };
    FSlot Slots[3];
    TRefCountPtr<FRDGPooledBuffer> Results;
};
FNeonCityOceanQuery::FNeonCityOceanQuery():State(MakeUnique<FState>()){}
FNeonCityOceanQuery::~FNeonCityOceanQuery()=default;
void FNeonCityOceanQuery::Reset_RenderThread()
{
    check(IsInRenderingThread());State=MakeUnique<FState>();LastDelivered=NextSequence-1;
}

FNeonCityOceanQuerySubmission FNeonCityOceanQuery::AddQueries_RenderThread(FRDGBuilder& Graph,
    FRDGTextureRef Displacement,const FNeonCityOceanParameters& Ocean,const FNeonCityOceanQueryRequest& Request,
    const FNeonCityOceanBathymetry* Bathymetry,const FNeonCityWakeFrame* Wake)
{
    check(IsInRenderingThread());
    FNeonCityOceanQuerySubmission Out;
    if(!Displacement || Request.Count==0 || Request.Count>Request.Capacity ||
        !FMath::IsFinite(Request.SimulationTime) || !FMath::IsFinite(Request.SeaLevel) ||
        !FMath::IsFinite(Request.Amplitude) || Request.Amplitude<0) return Out;
    for(const auto& P:Request.Points)if(P.ContainsNaN())return Out;
    for(const auto& S:Ocean.Sizes)if(!FMath::IsFinite(S.X) || S.X<=0)return Out;
    auto* Inputs=CreateStructuredBuffer(Graph,TEXT("NeonCityOcean.QueryInputs"),sizeof(FVector4f),Request.Capacity,
        Request.Points,sizeof(Request.Points),ERDGInitialDataFlags::None);
    if(State->Results)Out.Results=Graph.RegisterExternalBuffer(State->Results);
    else
    {
        auto Desc=FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f),Request.Capacity);Desc.Usage|=EBufferUsageFlags::SourceCopy;
        Out.Results=Graph.CreateBuffer(Desc,TEXT("NeonCityOcean.QueryResults"));
    }
    auto* P=Graph.AllocParameters<FNeonCityOceanWaterQueryCS::FParameters>();
    for(int32 I=0;I<4;++I)P->QuerySizes[I]=Ocean.Sizes[I];
    P->QueryAmplitude=Request.Amplitude;P->QuerySeaLevel=Request.SeaLevel;P->oceanDisplacement=Displacement;
    P->smpLinearRepeat=TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI();
    P->queryInputs=Graph.CreateSRV(Inputs);P->queryResults=Graph.CreateUAV(Out.Results);
    const bool Coastal=Bathymetry && Bathymetry->Depth && Bathymetry->IslandHeight && Bathymetry->IslandShore && Bathymetry->Resolution>1;
    if(Coastal)
    {
        P->TerrainDepth=Bathymetry->Depth;P->TerrainBounds=Bathymetry->Bounds;
        P->IslandHeight=Bathymetry->IslandHeight;P->IslandShore=Bathymetry->IslandShore;P->ShoreControls=Bathymetry->ShoreControls;
        P->TerrainOrigin=Bathymetry->Origin;P->TerrainGridSize=Bathymetry->Resolution;
        P->TerrainSampler=TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
    }
    FNeonCityOceanWaterQueryCS::FPermutationDomain Permutation;Permutation.Set<FNeonCityOceanWaterQueryCS::FCoastal>(Coastal);
    const bool HasWake=Coastal && Wake && Wake->Display && Wake->Near && Wake->Controls.Z>0;
    Permutation.Set<FNeonCityOceanWaterQueryCS::FWake>(HasWake);
    if(HasWake){P->WakeDisplay=Wake->Display;P->WakeNear=Wake->Near;P->WakeControls=Wake->Controls;P->WakeBoat=Wake->Boat;P->WakeHullScale=Wake->HullScale;P->WakeSampler=P->smpLinearRepeat;}
    TShaderMapRef<FNeonCityOceanWaterQueryCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel),Permutation);
    ClearUnusedGraphResources(Shader,P);
    FComputeShaderUtils::AddPass(Graph,RDG_EVENT_NAME("NeonCityOcean.WaterQueries"),Shader,P,FIntVector(1,1,1));
    for(auto& Slot:State->Slots)
    {
        if(Slot.bInFlight)continue;
        if(!Slot.Readback)Slot.Readback=MakeUnique<FRHIGPUBufferReadback>(TEXT("NeonCityOcean.QueryReadback"));
        Slot.Readback->ResetReady();Slot.Request=Request;Slot.Sequence=NextSequence++;Slot.bInFlight=true;
        AddEnqueueCopyPass(Graph,Slot.Readback.Get(),Out.Results,sizeof(FVector4f)*Request.Capacity);
        Out.bReadbackQueued=true;break;
    }
    Graph.QueueBufferExtraction(Out.Results,&State->Results);
    return Out;
}

bool FNeonCityOceanQuery::TryConsumeLatest_RenderThread(FNeonCityOceanQueryResult& Result)
{
    check(IsInRenderingThread());bool bNew=false;
    for(auto& Slot:State->Slots)
    {
        if(!Slot.bInFlight || !Slot.Readback->IsReady())continue;
        if(Slot.Sequence>LastDelivered)
        {
            const auto* Data=Slot.Readback->Lock(sizeof(Result.Values));
            if(!Data)continue;
            FMemory::Memcpy(Result.Values,Data,sizeof(Result.Values));Slot.Readback->Unlock();
            Result.Request=Slot.Request;Result.Sequence=Slot.Sequence;LastDelivered=Slot.Sequence;bNew=true;
        }
        Slot.bInFlight=false;
    }
    return bNew;
}
