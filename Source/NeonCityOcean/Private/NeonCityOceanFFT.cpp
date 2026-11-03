// Direct algorithm adaptation: Tidewater 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT notice: Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "NeonCityOceanFFT.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "DataDrivenShaderPlatformInfo.h"

BEGIN_SHADER_PARAMETER_STRUCT(FOceanCommonParameters,)
    SHADER_PARAMETER_ARRAY(FVector4f,OceanSizes,[4])
    SHADER_PARAMETER_ARRAY(FVector4f,OceanCuts,[4])
    SHADER_PARAMETER_ARRAY(FVector4f,OceanSysA,[2])
    SHADER_PARAMETER_ARRAY(FVector4f,OceanSysB,[2])
    SHADER_PARAMETER(float,OceanChoppiness)
    SHADER_PARAMETER(float,OceanFoamBias)
    SHADER_PARAMETER(float,OceanFoamGain)
    SHADER_PARAMETER(float,OceanFoamDecay)
    SHADER_PARAMETER(float,OceanFoamAdd)
    SHADER_PARAMETER(float,OceanTime)
    SHADER_PARAMETER(float,OceanDepth)
    SHADER_PARAMETER(uint32,OceanSeed)
    SHADER_PARAMETER(float,FrameDeltaSeconds)
END_SHADER_PARAMETER_STRUCT()

#include "TidewaterFFTShaders.generated.h"

struct FNeonCityOceanFFT::FState
{
    TRefCountPtr<FRDGPooledBuffer> H0,Wave,Foam;
    TRefCountPtr<IPooledRenderTarget> Displacement,Derivatives;
};

FNeonCityOceanFFT::FNeonCityOceanFFT():State(MakeUnique<FState>()){}
FNeonCityOceanFFT::~FNeonCityOceanFFT()=default;

void FNeonCityOceanFFT::Reset_RenderThread()
{
    check(IsInRenderingThread());
    State=MakeUnique<FState>();
}

FNeonCityOceanFrame FNeonCityOceanFFT::AddFrame_RenderThread(FRDGBuilder& Graph,
    const FNeonCityOceanParameters& P,float Time,float DeltaSeconds,const FNeonCityOceanAudit& Audit)
{
    check(IsInRenderingThread());
    FNeonCityOceanFrame R;
    if(GMaxRHIFeatureLevel<ERHIFeatureLevel::SM5 || !FMath::IsFinite(Time) ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0 || P.Depth<=0) return R;
    for(const auto& Size:P.Sizes) if(!FMath::IsFinite(Size.X) || Size.X<=0) return R;
    const bool bInitialize=!State->H0.IsValid();
    constexpr uint32 Samples=256*256*4;
    const auto Buffer=[&](const TCHAR* Name,uint32 Count,uint32 Stride)
    {
        auto Desc=FRDGBufferDesc::CreateStructuredDesc(Stride,Count);
        Desc.Usage|=EBufferUsageFlags::SourceCopy;
        return Graph.CreateBuffer(Desc,Name);
    };
    R.H0=bInitialize?Buffer(TEXT("NeonCityOcean.H0"),Samples,16):Graph.RegisterExternalBuffer(State->H0);
    R.Wave=bInitialize?Buffer(TEXT("NeonCityOcean.Wave"),Samples,16):Graph.RegisterExternalBuffer(State->Wave);
    R.Foam=bInitialize?Buffer(TEXT("NeonCityOcean.Foam"),Samples,4):Graph.RegisterExternalBuffer(State->Foam);
    R.Tmp=Buffer(TEXT("NeonCityOcean.Tmp"),Samples*2,16);
    R.MipSrc=Buffer(TEXT("NeonCityOcean.MipSrc"),Samples*2,16);
    R.MipMid=Buffer(TEXT("NeonCityOcean.MipMid"),8*8*4*2,16);
    const auto Desc=FRDGTextureDesc::Create2DArray(FIntPoint(256,256),PF_FloatRGBA,FClearValueBinding::None,
        ETextureCreateFlags::ShaderResource|ETextureCreateFlags::UAV,4,9);
    R.Displacement=bInitialize?Graph.CreateTexture(Desc,TEXT("NeonCityOcean.Displacement")):Graph.RegisterExternalTexture(State->Displacement);
    R.Derivatives=bInitialize?Graph.CreateTexture(Desc,TEXT("NeonCityOcean.Derivatives")):Graph.RegisterExternalTexture(State->Derivatives);
    FOceanCommonParameters Common{};
    for(int32 I=0;I<4;++I){Common.OceanSizes[I]=P.Sizes[I];Common.OceanCuts[I]=P.Cuts[I];}
    for(int32 I=0;I<2;++I){Common.OceanSysA[I]=P.SysA[I];Common.OceanSysB[I]=P.SysB[I];}
    Common.OceanChoppiness=P.Choppiness;Common.OceanFoamBias=P.FoamBias;
    Common.OceanFoamGain=P.FoamGain;Common.OceanFoamDecay=P.FoamDecay;Common.OceanFoamAdd=P.FoamAdd;
    Common.OceanTime=Time;Common.OceanDepth=P.Depth;Common.OceanSeed=P.Seed;Common.FrameDeltaSeconds=DeltaSeconds;
    if(bInitialize)
    {
        AddInitSpectrumPass(Graph,Common,R);
        if(Audit)Audit(Graph,TEXT("init"),R);
        AddConjugatePass(Graph,Common,R);AddCopyH0Pass(Graph,Common,R);
    }
    AddRowsPass(Graph,Common,R);
    if(Audit)Audit(Graph,TEXT("rows"),R);
    AddColumnsPass(Graph,Common,R);
    AddDisplacementMipsAPass(Graph,Common,R);AddDerivativeMipsAPass(Graph,Common,R);
    AddDisplacementMipsBPass(Graph,Common,R);AddDerivativeMipsBPass(Graph,Common,R);
    if(Audit)Audit(Graph,TEXT("surface"),R);
    Graph.QueueBufferExtraction(R.H0,&State->H0);
    Graph.QueueBufferExtraction(R.Wave,&State->Wave);
    Graph.QueueBufferExtraction(R.Foam,&State->Foam);
    Graph.QueueTextureExtraction(R.Displacement,&State->Displacement);
    Graph.QueueTextureExtraction(R.Derivatives,&State->Derivatives);
    return R;
}
