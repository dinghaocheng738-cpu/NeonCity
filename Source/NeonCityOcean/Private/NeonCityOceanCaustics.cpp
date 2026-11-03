// Adapted from Tidewater src/ocean/Caustics.js at 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT: Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "NeonCityOceanCaustics.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderUtils.h"
#include "CommonRenderResources.h"
#include "GenerateMips.h"

BEGIN_SHADER_PARAMETER_STRUCT(FCausticParameters,)
 SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray,OceanDerivatives)
 SHADER_PARAMETER_SAMPLER(SamplerState,OceanSampler)
 SHADER_PARAMETER(FVector3f,SunDirection)
 SHADER_PARAMETER(float,TileSize)
 SHADER_PARAMETER(float,FocalDepth)
 SHADER_PARAMETER(float,SlopeLevel)
 SHADER_PARAMETER(float,Amplitude)
 SHADER_PARAMETER(uint32,Cascade)
 SHADER_PARAMETER(uint32,Grid)
 SHADER_PARAMETER(uint32,Channel)
 RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
class FCausticVS:public FGlobalShader
{
public:
 DECLARE_GLOBAL_SHADER(FCausticVS);SHADER_USE_PARAMETER_STRUCT(FCausticVS,FGlobalShader);
 using FParameters=FCausticParameters;
};
class FCausticPS:public FGlobalShader
{
public:
 DECLARE_GLOBAL_SHADER(FCausticPS);SHADER_USE_PARAMETER_STRUCT(FCausticPS,FGlobalShader);
 using FParameters=FCausticParameters;
};
IMPLEMENT_GLOBAL_SHADER(FCausticVS,"/NeonCity/Private/OceanExperience/Caustics.usf","MainVS",SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FCausticPS,"/NeonCity/Private/OceanExperience/Caustics.usf","MainPS",SF_Pixel);

void AddNeonCityCaustics(FRDGBuilder& Graph,FRDGTextureRef Derivatives,FRDGTextureRef Fine,
 FRDGTextureRef Broad,FVector3f Sun,FVector2f Tiles,float Strength)
{
 if(!Derivatives || !Fine || !Broad || Tiles.X<=0 || Tiles.Y<=0)return;
 TShaderMapRef<FCausticVS> VS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
 TShaderMapRef<FCausticPS> PS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
 for(uint32 Layer=0;Layer<2;++Layer)
 {
  FRDGTextureRef Target=Layer==0?Fine:Broad;
  // Half of the source's grid density, at its original texture resolutions.
  // Bounded to two focal planes/layer; no per-object or CPU photon simulation.
  const uint32 Grid=Layer==0?218:109;
  for(uint32 Channel=0;Channel<2;++Channel)
  {
   auto* P=Graph.AllocParameters<FCausticParameters>();
   P->OceanDerivatives=Derivatives;P->OceanSampler=TStaticSamplerState<SF_Trilinear,AM_Wrap,AM_Wrap>::GetRHI();
   P->SunDirection=Sun;P->TileSize=Layer==0?Tiles.X:Tiles.Y;
   P->FocalDepth=Layer==0?(Channel==0?1.2f:4.f):(Channel==0?3.f:9.f);
   P->SlopeLevel=Layer==0?1.f:.5f;P->Amplitude=Strength;P->Cascade=Layer==0?3:2;P->Grid=Grid;P->Channel=Channel;
   P->RenderTargets[0]=FRenderTargetBinding(Target,Channel==0?ERenderTargetLoadAction::EClear:ERenderTargetLoadAction::ELoad);
   const int32 Resolution=Target->Desc.Extent.X;
   Graph.AddPass(RDG_EVENT_NAME("OceanCaustics L%u F%u",Layer,Channel),P,ERDGPassFlags::Raster,
    [P,VS,PS,Grid,Resolution](FRHICommandList& Cmd)
   {
    Cmd.SetViewport(0,0,0,Resolution,Resolution,1);
    FGraphicsPipelineStateInitializer State;Cmd.ApplyCachedRenderTargets(State);
    State.BlendState=TStaticBlendState<CW_RGBA,BO_Add,BF_One,BF_One,BO_Add,BF_One,BF_One>::GetRHI();
    State.RasterizerState=TStaticRasterizerState<FM_Solid,CM_None>::GetRHI();
    State.DepthStencilState=TStaticDepthStencilState<false,CF_Always>::GetRHI();
    State.PrimitiveType=PT_TriangleList;State.BoundShaderState.VertexDeclarationRHI=GEmptyVertexDeclaration.VertexDeclarationRHI;
    State.BoundShaderState.VertexShaderRHI=VS.GetVertexShader();State.BoundShaderState.PixelShaderRHI=PS.GetPixelShader();
    SetGraphicsPipelineState(Cmd,State,0);SetShaderParameters(Cmd,VS,VS.GetVertexShader(),*P);SetShaderParameters(Cmd,PS,PS.GetPixelShader(),*P);
    Cmd.DrawPrimitive(0,Grid*Grid*2,1);
   });
  }
  FGenerateMipsParams Mips;Mips.AddressU=Mips.AddressV=AM_Wrap;
  FGenerateMips::Execute(Graph,GMaxRHIFeatureLevel,Target,Mips,EGenerateMipsPass::Compute);
 }
}
