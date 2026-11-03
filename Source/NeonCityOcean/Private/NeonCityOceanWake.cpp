// Tidewater 4811ba48d795197de5621985f404e765c0b7c0ef; MIT notice in Docs/ThirdParty.
#include "NeonCityOceanWake.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHIStaticStates.h"
#include "TidewaterWakeShaders.generated.h"

struct FNeonCityOceanWake::FState
{
 TRefCountPtr<FRDGPooledBuffer> Wave,Aer,NearBuffer;
 TRefCountPtr<IPooledRenderTarget> Display,Aeration,Near;
 FVector2f Center=FVector2f::ZeroVector,Origin=FVector2f::ZeroVector;
 float Idle=40;
 uint32 BoatID=0;
 bool Sleeping=true;
};
FNeonCityOceanWake::FNeonCityOceanWake():State(MakeUnique<FState>()){}
FNeonCityOceanWake::~FNeonCityOceanWake()=default;
void FNeonCityOceanWake::Reset_RenderThread(){check(IsInRenderingThread());State=MakeUnique<FState>();}
FNeonCityWakeFrame FNeonCityOceanWake::AddFrame_RenderThread(FRDGBuilder& Graph,const FNeonCityWakeInput& B,
 float Time,float Delta,FRDGTextureRef Island,FRDGTextureRef Hull,FRDGTextureRef Piles)
{
 check(IsInRenderingThread());FNeonCityWakeFrame R;
 if(!Island || !Hull || !Piles || Delta<=0 || !FMath::IsFinite(Time) || !FMath::IsFinite(B.Speed))return R;
 auto& S=*State;const bool Init=!S.Wave;
 const bool Moving=B.BoatID && (B.Speed>.5f || (B.Driven && FMath::Abs(B.Throttle)>.04f));
 S.Idle=Moving?0:S.Idle+Delta;
 const bool Active=S.Idle<40;
 const bool Changed=B.BoatID!=S.BoatID;
 bool Reset=Init || Changed || (S.Sleeping && Active);
 S.BoatID=B.BoatID;S.Sleeping=!Active;
 auto Buffer=[&](const TCHAR* Name,uint32 Count,uint32 Stride){return Graph.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(Stride,Count),Name);};
 auto Texture=[&](const TCHAR* Name,int32 X,int32 Y){return Graph.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(X,Y),PF_FloatRGBA,FClearValueBinding::None,ETextureCreateFlags::ShaderResource|ETextureCreateFlags::UAV),Name);};
 R.State=Init?Buffer(TEXT("Wake.State"),512*512,16):Graph.RegisterExternalBuffer(S.Wave);
 R.AerA=Init?Buffer(TEXT("Wake.AerationState"),512*512,4):Graph.RegisterExternalBuffer(S.Aer);
 R.NearBuffer=Init?Buffer(TEXT("Wake.NearMean"),32*88,4):Graph.RegisterExternalBuffer(S.NearBuffer);
 R.Display=Init?Texture(TEXT("Wake.Display"),512,512):Graph.RegisterExternalTexture(S.Display);
 R.Aeration=Init?Texture(TEXT("Wake.Aeration"),512,512):Graph.RegisterExternalTexture(S.Aeration);
 R.Near=Init?Texture(TEXT("Wake.Near"),32,88):Graph.RegisterExternalTexture(S.Near);
 R.Controls=FVector4f(S.Center.X*.4f,S.Center.Y*.4f,Active?FMath::Min(1.f,(40-S.Idle)/4):0,Time);
 R.Boat=FVector4f(B.Position.X,B.Position.Y,B.Forward.Y,B.Forward.X);
 R.HullScale=FVector2f(FMath::Clamp(B.HullScale.X,.3f,6.f),FMath::Clamp(B.HullScale.Y,.3f,6.f));
 if(!Active && !Init)return R;
 if(Init){AddClearUAVPass(Graph,Graph.CreateUAV(R.State),0);AddClearUAVPass(Graph,Graph.CreateUAV(R.AerA),0);AddClearUAVPass(Graph,Graph.CreateUAV(R.NearBuffer),0);}
 R.Scratch=Buffer(TEXT("Wake.Scratch"),512*512,16);R.Spec=Buffer(TEXT("Wake.Spectrum"),512*512,16);R.AerB=Buffer(TEXT("Wake.AerationNext"),512*512,4);
 const FVector2f Cells=B.Position/.4f;
 if(Reset || FMath::Abs(Cells.X-S.Center.X)>512 || FMath::Abs(Cells.Y-S.Center.Y)>512)
 {S.Center=FVector2f(FMath::RoundToFloat(Cells.X),FMath::RoundToFloat(Cells.Y));Reset=true;}
 for(int32 I=0;I<2;++I){if(Cells[I]-S.Center[I]>170)S.Center[I]=FMath::CeilToFloat(Cells[I]-170);if(S.Center[I]-Cells[I]>170)S.Center[I]=FMath::FloorToFloat(Cells[I]+170);}
 const FVector2f Previous=S.Origin;S.Origin=S.Center-FVector2f(256,256);
 auto Smooth=[](float V,float A,float C){const float T=FMath::Clamp((V-A)/(C-A),0.f,1.f);return T*T*(3-2*T);};
 const float DT=FMath::Clamp(Delta,1.f/240,1.f/30),Speed=FMath::Max(0.f,B.Speed),Plane=Smooth(Speed,2,9),Trim=Smooth(Speed,3,10);
 FWakeCommonParameters P{};
 P.WakeHullScale=R.HullScale;
 P.WakeOrigin=S.Origin;P.WakePrev=Reset?S.Origin:Previous;P.WakeCenter=S.Center*.4f;P.WakeBoatPos=B.Position;
 P.WakeBoatRot=FVector2f(B.Forward.Y,B.Forward.X);P.WakeDead=FVector2f(.03f,.08f);P.WakeTrim=FVector3f(-.08f*Trim,0,-.035f*Trim);
 P.WakeReset=Reset?1:0;P.WakePileBox=FVector4f(47.1066433f,-61.7777161f,15.7742622f,102.708622f);
 P.WakeDt=DT;P.WakeAmount=R.Controls.Z;P.WakeSource=.72f*(1+.2f*Plane);P.WakeWash=(B.Driven?FMath::Abs(B.Throttle)*B.RPM:0)+Smooth(Speed,1.5f,7)*.5f;
 // Disable only boat-generated bow whitening. The spectral displacement,
 // transom depression, propeller wash and environmental surf remain active.
 P.WakeBow=0;P.WakeSpeed=Speed;P.WakeWashW=.7f+.6f*Plane;P.WakeBoil=1.2f;P.WakeVisc=.006f;
 P.WakeDyn=.35f*Speed*Speed/(2*9.81f);P.WakeHollow=.35f+.015f*Speed*Speed;P.WakeHollowK=.3f*Plane;
 P.WakeAmplitude=1;P.WakeFoamGain=.35f;P.WakeAerGain=1;P.WakeAerOut=.3f;P.WakeNearRate=Reset?1:1-FMath::Exp(-DT/.25f);P.WakeTime=Time;
 AddWakeRows(Graph,P,R,Island,Hull,Piles);AddWakeColumns(Graph,P,R,Island,Hull,Piles);AddWakeInverse(Graph,P,R,Island,Hull,Piles);
 Graph.QueueBufferExtraction(R.State,&S.Wave);Graph.QueueBufferExtraction(R.AerA,&S.Aer);Graph.QueueBufferExtraction(R.NearBuffer,&S.NearBuffer);
 Graph.QueueTextureExtraction(R.Display,&S.Display);Graph.QueueTextureExtraction(R.Aeration,&S.Aeration);Graph.QueueTextureExtraction(R.Near,&S.Near);
 R.Controls=FVector4f(P.WakeCenter.X,P.WakeCenter.Y,P.WakeAmount,Time);R.Stepped=true;return R;
}
