// Tidewater runtime adapter. Source algorithms: 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT: Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "Water/CoastalOceanSurface.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Water/OceanSoundscapeComponent.h"
#include "Water/CoastalWaveData.h"
#include "Water/TidewaterOceanTexture.h"
#include "Water/TidewaterWaterSample.h"
#include "Water/CoastalOceanInstances.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "NeonCityOceanFFT.h"
#include "NeonCityOceanQuery.h"
#include "NeonCityOceanCDLOD.h"
#include "NeonCityOceanWake.h"
#include "NeonCityOceanCaustics.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/NeonCityMainlandSite.h"
#include "Components/DirectionalLightComponent.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RenderTargetPool.h"
#include "TextureResource.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include <atomic>

namespace
{
// The existing native island adapter uses this source origin and handedness.
// Distinct name: an anonymous-namespace `Origin` trips C4459 against engine headers
// that also declare one (for example TransformVectorized.h SetLocation).
const FVector SourceOrigin=NeonCityCoastalLayout::IslandOrigin;
FVector ToSource(FVector P){P=(P-SourceOrigin)/100;return FVector(P.X,P.Z,-P.Y);}
FVector ToWorld(FVector P){return SourceOrigin+FVector(P.X,-P.Z,P.Y)*100;}
struct FOceanRenderState
{
 FNeonCityOceanFFT FFT;
 FNeonCityOceanQuery Queries;
 FNeonCityOceanWake Wake;
 FVector4f WakeControls=FVector4f(0,0,0,0),WakeBoat=FVector4f(0,0,1,0);
 FVector2f WakeHullScale=FVector2f(1,1);
 TRefCountPtr<IPooledRenderTarget> OutputDisplacement,OutputDerivatives;
 FCriticalSection Mutex;
 FNeonCityOceanQueryResult Latest;
 std::atomic<bool> Busy{false},Published{false};
 bool HasResult=false;
};
struct FWaterLease:FTidewaterWaterSample
{
 TWeakObjectPtr<UPrimitiveComponent> Body;
 FVector Local=FVector::ZeroVector;
 int32 Index=INDEX_NONE;
};
}
struct FCoastalOceanRuntime
{
 TSharedPtr<FOceanRenderState,ESPMode::ThreadSafe> Render=MakeShared<FOceanRenderState,ESPMode::ThreadSafe>();
 FNeonCityOceanCDLOD LOD;
 FWaterLease Leases[64];
 TArray<FVector4f> Nodes;
 FCoastalOceanInstances Instances;
 uint64 Revision=1,Sequence=0;
 uint8 ActiveState=1;
 bool ResetSpectrum=true;
 double LastFrameTime=0,Latency=.04;
 double WakeSelectionTime=0;
 TWeakObjectPtr<ANeonCityMarineVehicleBase> WakeBoat;
};
ACoastalOceanSurface::ACoastalOceanSurface()
{
 CreateDefaultSubobject<UOceanSoundscapeComponent>(TEXT("OceanSoundscape"));
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
 Surface=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OceanRenderGrid"));SetRootComponent(Surface);
 Surface->SetMobility(EComponentMobility::Movable);Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Surface->SetCanEverAffectNavigation(false);Surface->SetCastShadow(false);Surface->bReceivesDecals=false;
 Surface->bAffectDistanceFieldLighting=false;Surface->bAffectDynamicIndirectLighting=false;
 Surface->NumCustomDataFloats=4;Surface->SetVisibility(false);
 static ConstructorHelpers::FObjectFinder<UCoastalWaveData> Waves(TEXT("/Game/NeonCity/Art/Immersion/Ocean/DA_CoastalWaves"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Grid(TEXT("/Game/NeonCity/Art/Immersion/Ocean/SM_TidewaterCDLOD"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Water(TEXT("/Game/NeonCity/Art/Immersion/Ocean/M_TidewaterOcean"));
 Field=Waves.Object;GridAsset=Grid.Object;WaterMaterial=Water.Object;
}
bool ACoastalOceanSurface::InitializeField(UCoastalWaveData* InField,UMaterialInterface* InMaterial)
{
 if(HasActorBegunPlay() || IsActorBeginningPlay() || Runtime.IsValid() || !IsValid(InField)
  || !InField->IsValidField() || !InField->IslandHeight || !InField->IslandShore || !InField->IslandNormal)return false;
 Field=InField;
 if(InMaterial)WaterMaterial=InMaterial;
 return true;
}
void ACoastalOceanSurface::BeginPlay()
{
 Super::BeginPlay();
 if(!Field || !Field->IsValidField() || !GridAsset || !WaterMaterial || !Field->IslandHeight || !Field->IslandShore || !Field->IslandNormal || GMaxRHIFeatureLevel<ERHIFeatureLevel::SM5)
 {UE_LOG(LogTemp,Error,TEXT("Tidewater runtime assets/RHI incomplete; existing water preserved."));SetActorTickEnabled(false);return;}
 Runtime=MakeShared<FCoastalOceanRuntime,ESPMode::ThreadSafe>();
 // Contained (inland / pool) water sits above the sea: never draw the open ocean beneath it.
 {
  TArray<FBox2D> Inland;
  for(TActorIterator<ANeonCityWaterVolume> It(GetWorld());It;++It)if(It->bContainedPoolWater)
  {
   FBox2D Box(ForceInit);
   for(int32 C=0;C<4;++C)
   {
    const FVector P=ToSource(It->GetActorTransform().TransformPosition(FVector((C&1)?It->Extent.X:-It->Extent.X,(C&2)?It->Extent.Y:-It->Extent.Y,0)));
    Box+=FVector2D(P.X,P.Z);
   }
   Inland.Add(Box);
  }
  Runtime->LOD.SetExclusions(Inland);
 }
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It){DayNight=*It;break;}
 const auto CausticTarget=[&](const TCHAR* Name,int32 Resolution)
 {
  auto* T=NewObject<UTextureRenderTarget2D>(this,Name);T->bCanCreateUAV=true;T->bAutoGenerateMips=true;
  T->ClearColor=FLinearColor::Transparent;T->AddressX=TA_Wrap;T->AddressY=TA_Wrap;
  T->InitCustomFormat(Resolution,Resolution,PF_FloatRGBA,true);T->UpdateResourceImmediate(true);return T;
 };
 CausticsFine=CausticTarget(TEXT("OceanCausticsFine"),512);CausticsBroad=CausticTarget(TEXT("OceanCausticsBroad"),256);
 Displacement=NewObject<UTidewaterOceanTexture>(this);Derivatives=NewObject<UTidewaterOceanTexture>(this);
 Displacement->UpdateResource();Derivatives->UpdateResource();
 Surface->SetStaticMesh(GridAsset);Material=UMaterialInstanceDynamic::Create(WaterMaterial,this);Surface->SetMaterial(0,Material);
 if(const auto* Site=ANeonCityMainlandSite::Find(GetWorld()))Site->ApplyOceanCoverage(*Material);
 Material->SetTextureParameterValue(TEXT("Displacement"),Displacement);Material->SetTextureParameterValue(TEXT("Derivatives"),Derivatives);
 Material->SetVectorParameterValue(TEXT("OceanOrigin"),FLinearColor(SourceOrigin.X,SourceOrigin.Y,SourceOrigin.Z,0));
 Material->SetTextureParameterValue(TEXT("Bathymetry"),Field->DepthTexture);
 Material->SetTextureParameterValue(TEXT("HeightField"),Field->IslandHeight);
 Material->SetTextureParameterValue(TEXT("ShoreField"),Field->IslandShore);
 Material->SetTextureParameterValue(TEXT("NormalRock"),Field->IslandNormal);
 Material->SetVectorParameterValue(TEXT("WaterBounds"),FLinearColor(Field->GridMinimum.X,Field->GridMinimum.Y,Field->GridMaximum.X,Field->GridMaximum.Y));
 Material->SetScalarParameterValue(TEXT("GridSize"),Field->GridSize);
 WakeHull=LoadObject<UTexture2D>(nullptr,TEXT("/Game/NeonCity/Art/Immersion/Ocean/T_WakeHull"));
 WakePiles=LoadObject<UTexture2D>(nullptr,TEXT("/Game/NeonCity/Art/Immersion/Ocean/T_WakePiles"));
 if(WakeHull && WakePiles)
 {
  auto Target=[&](const TCHAR* Name,int32 X,int32 Y)
  {auto* T=NewObject<UTextureRenderTarget2D>(this,Name);T->bCanCreateUAV=true;T->ClearColor=FLinearColor::Transparent;T->InitCustomFormat(X,Y,PF_FloatRGBA,true);T->UpdateResourceImmediate(true);return T;};
  WakeDisplay=Target(TEXT("OceanWakeDisplay"),512,512);WakeAeration=Target(TEXT("OceanWakeAeration"),512,512);WakeNear=Target(TEXT("OceanWakeNearField"),32,88);
  Material->SetTextureParameterValue(TEXT("WakeDisplay"),WakeDisplay);Material->SetTextureParameterValue(TEXT("WakeAeration"),WakeAeration);Material->SetTextureParameterValue(TEXT("WakeNear"),WakeNear);
 }
 for(int32 I=0;I<12;++I){const auto V=Runtime->LOD.GetMorphParameters()[I];Material->SetVectorParameterValue(*FString::Printf(TEXT("Morph%d"),I),FLinearColor(V.X,V.Y,V.Z,V.W));}
}
void ACoastalOceanSurface::SetSeaState(ECoastalSeaState State)
{if(uint8(State)<=uint8(ECoastalSeaState::Moderate))SeaState=State;}
bool ACoastalOceanSurface::RegisterSamples(UPrimitiveComponent* Body,const TArray<FVector>& Points)
{
 if(!Runtime || !IsValid(Body) || Points.IsEmpty() || Points.Num()>62 || Body->GetWorld()!=GetWorld())return false;
 for(const auto& P:Points)if(P.ContainsNaN())return false;
 int32 Free=0;
 for(int32 I=2;I<64;++I){if(Runtime->Leases[I].Body==Body)return true;if(!Runtime->Leases[I].Body.IsValid())++Free;}
 if(Free<Points.Num()){UE_LOG(LogTemp,Warning,TEXT("Ocean sample capacity reached; additional body retains mean-water fallback."));return false;}
 int32 N=0;
 for(int32 I=2;I<64 && N<Points.Num();++I)if(!Runtime->Leases[I].Body.IsValid())
 {auto& S=Runtime->Leases[I];S=FWaterLease();S.Body=Body;S.Local=Points[N];S.Index=N++;}
 ++Runtime->Revision;
 return true;
}
void ACoastalOceanSurface::UnregisterSamples(UPrimitiveComponent* Body)
{
 if(!Runtime)return;
 for(int32 I=2;I<64;++I)if(Runtime->Leases[I].Body==Body)Runtime->Leases[I]=FWaterLease();
 ++Runtime->Revision;
}
void ACoastalOceanSurface::Tick(float Delta)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(CoastalOceanTick);
 Super::Tick(Delta);if(!Runtime || !Material)return;
 // Renderer-free commandlets/PIE have no global compute shader map. Do not
 // dispatch FFT work through NullRHI or publish a fictitious ready field.
 // Existing not-ready query behavior remains authoritative in this mode.
 if(GUsingNullRHI)return;
 auto& R=*Runtime;WaveSeconds=GetWorld()->GetTimeSeconds();
 const float Step=FMath::Clamp(Delta,0.f,.1f);
 // Source spectra change only at zero displacement, avoiding a discontinuous sea/boat impulse.
 const bool Changing=R.ActiveState!=uint8(SeaState);
 Strength=FMath::FInterpConstantTo(Strength,Changing?0.f:1.f,Step,.3f);
 if(Changing && Strength<.001f){R.ActiveState=uint8(SeaState);R.ResetSpectrum=true;++R.Revision;for(auto& S:R.Leases)S.Valid=false;}
 for(auto& S:R.Leases)S.Advance(Step);
 {
  FNeonCityOceanQueryResult Result;bool New=false;
  {FScopeLock Lock(&R.Render->Mutex);if(R.Render->HasResult){Result=R.Render->Latest;R.Render->HasResult=false;New=true;}}
  if(New && Result.Sequence>R.Sequence && Result.Request.ClientRevision==R.Revision)
  {
   R.Sequence=Result.Sequence;R.Latency=FMath::Clamp(WaveSeconds-Result.Request.SimulationTime,0.,.12);
   for(int32 I=0;I<64;++I)
   {
    if(I>1 && !R.Leases[I].Body.IsValid())continue;
    auto& S=R.Leases[I];const auto V=Result.Values[I];const auto Q=Result.Request.Points[I];
    S.Accept(V,FVector2D(Q.X,Q.Y),Result.Request.SimulationTime);
   }
  }
 }
 if(!bReady && R.Render->Published && R.Sequence>0)
 {
  for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
   if(auto* M=It->GetStaticMeshComponent()->GetMaterial(0);M && M->GetPathName().Contains(TEXT("M_SB205_CoastalWater")) && !It->IsHidden())
   {HiddenLegacySurfaces.Add(*It);It->SetActorHiddenInGame(true);}
  bReady=true;Surface->SetVisibility(true);
 }
 FVector Eye=SourceOrigin+FVector(0,0,200);FRotator View=FRotator::ZeroRotator;
 auto* PC=GetWorld()->GetFirstPlayerController();if(PC)PC->GetPlayerViewPoint(Eye,View);
 const FVector Camera=ToSource(Eye);
 // Conservative bounded view volume; UE performs exact instance frustum culling.
 TStaticArray<FVector4d,6> Planes;
 Planes[0]=FVector4d(1,0,0,40000-Camera.X);Planes[1]=FVector4d(-1,0,0,40000+Camera.X);
 Planes[2]=FVector4d(0,0,1,40000-Camera.Z);Planes[3]=FVector4d(0,0,-1,40000+Camera.Z);
 Planes[4]=FVector4d(0,1,0,30000);Planes[5]=FVector4d(0,-1,0,30000);
 bool bSelected=false;
 {TRACE_CPUPROFILER_EVENT_SCOPE(CoastalOceanSelectLOD);bSelected=R.LOD.Select(Camera,Planes);}
 if(bSelected)
 {
  const auto& Nodes=R.LOD.GetNodes();
  if(Nodes!=R.Nodes)
  {
   R.Instances.Apply(*Surface,Nodes,SourceOrigin);R.Nodes=Nodes;
  }
 }
 Material->SetVectorParameterValue(TEXT("SourceCamera"),FLinearColor(Camera.X,Camera.Y,Camera.Z,0));
 {
  FScopeLock Lock(&R.Render->Mutex);const auto C=R.Render->WakeControls,B=R.Render->WakeBoat;
  Material->SetVectorParameterValue(TEXT("WakeControls"),FLinearColor(C.X,C.Y,C.Z,C.W));
  Material->SetVectorParameterValue(TEXT("WakeBoat"),FLinearColor(B.X,B.Y,B.Z,B.W));
  const auto S=R.Render->WakeHullScale;Material->SetVectorParameterValue(TEXT("WakeHullScale"),FLinearColor(S.X,S.Y,0,0));
 }
 // Render command owns snapshots only; no world/actor reads on the render thread.
 if(R.Render->Busy.exchange(true))return;
 FNeonCityOceanParameters Parameters;const auto& Bytes=Field->GetProfile(R.ActiveState);FMemory::Memcpy(&Parameters,Bytes.GetData(),sizeof(Parameters));
 CausticTileSizes=FVector2f(Parameters.Sizes[3].X,Parameters.Sizes[2].X);
 for(int32 I=0;I<4;++I){const auto V=Parameters.Sizes[I];Material->SetVectorParameterValue(*FString::Printf(TEXT("Size%d"),I),FLinearColor(V.X,V.Y,V.Z,V.W));}
 Material->SetScalarParameterValue(TEXT("Amplitude"),Strength);Material->SetScalarParameterValue(TEXT("FoamBias"),Parameters.FoamBias);
 Material->SetScalarParameterValue(TEXT("WindSpeed"),R.ActiveState==0?3.5f:R.ActiveState==1?7.f:12.f);
 const FVector4f ShoreControls(float(WaveSeconds),R.ActiveState==0?11.f:R.ActiveState==1?9.f:8.5f,
  R.ActiveState==0?.18f:R.ActiveState==1?.34f:.56f,Strength);
 Material->SetVectorParameterValue(TEXT("ShoreControls"),FLinearColor(ShoreControls.X,ShoreControls.Y,ShoreControls.Z,ShoreControls.W));
 FNeonCityOceanQueryRequest Request;Request.Count=64;Request.SeaLevel=0;Request.Amplitude=Strength;
 Request.SimulationTime=WaveSeconds;Request.ClientRevision=R.Revision;
 for(int32 I=0;I<64;++I)
 {
  FVector P=Eye;
  if(I==1 && PC && PC->GetPawn())P=PC->GetPawn()->GetActorLocation();
  if(I>1)if(auto* Body=R.Leases[I].Body.Get())P=Body->GetComponentTransform().TransformPosition(R.Leases[I].Local)+Body->GetPhysicsLinearVelocity()*R.Latency;
  const FVector S=ToSource(P);Request.Points[I]=FVector4f(S.X,S.Z,0,0);
 }
 const auto Shared=R.Render;auto* D=Displacement->GetResource();auto* G=Derivatives->GetResource();
 auto* Seabed=Field->DepthTexture->GetResource();
 auto* HeightField=Field->IslandHeight->GetResource();auto* ShoreField=Field->IslandShore->GetResource();
 const FVector4f DepthBounds(Field->GridMinimum.X,Field->GridMinimum.Y,Field->GridMaximum.X,Field->GridMaximum.Y);
 const float DepthResolution=Field->GridSize;
 const bool Reset=R.ResetSpectrum;R.ResetSpectrum=false;
 const float DT=FMath::Clamp(float(WaveSeconds-R.LastFrameTime),.001f,.1f);R.LastFrameTime=WaveSeconds;
 // One nearby active vessel owns the bounded 205m simulation window. Retain it
 // after stopping so the actual curved track can spread and decay for 40s.
 if(auto* Driven=PC?Cast<ANeonCityMarineVehicleBase>(PC->GetPawn()):nullptr)R.WakeBoat=Driven;
 else if(WaveSeconds>=R.WakeSelectionTime)
 {
  R.WakeSelectionTime=WaveSeconds+.5;double Best=FMath::Square(20000.f);
  TWeakObjectPtr<ANeonCityMarineVehicleBase> Candidate;
  for(TActorIterator<ANeonCityMarineVehicleBase> It(GetWorld());It;++It)
  {
   double Dist=FVector::DistSquared(Eye,It->GetActorLocation());if(Dist>FMath::Square(25000.f))continue;
   const auto T=It->GetMarineMovement()->GetTelemetry();if(T.Afloat && T.SpeedKmh>2)Dist*=.15;
   if(R.WakeBoat.Get()==*It)Dist*=.8; // Retain a fading track unless another moving boat is nearby.
   if(Dist<Best){Best=Dist;Candidate=*It;}
  }
  R.WakeBoat=Candidate;
 }
 FNeonCityWakeInput WakeInput;
 if(auto* Boat=R.WakeBoat.Get())
 {
  const FVector P=ToSource(Boat->GetActorLocation()),F=Boat->GetActorForwardVector();const auto* Movement=Boat->GetMarineMovement();const auto T=Movement->GetTelemetry();
  WakeInput.Position=FVector2f(P.X,P.Z);WakeInput.Forward=FVector2f(F.X,-F.Y).GetSafeNormal();WakeInput.BoatID=Boat->GetUniqueID();
  WakeInput.Speed=T.SpeedKmh/3.6f;WakeInput.Throttle=T.SignedThrust;WakeInput.RPM=Movement->GetWakeEngineRPM();WakeInput.Driven=T.EngineRunning && T.Afloat;
  if(const auto* Profile=Boat->GetProfile();Profile && !Profile->bReferenceDisplacementHull)
   WakeInput.HullScale=FVector2f(Profile->HullExtent.Y/125.f,Profile->HullExtent.X/390.f);
 }
 auto* HullField=WakeHull?WakeHull->GetResource():nullptr;auto* PileField=WakePiles?WakePiles->GetResource():nullptr;
 auto* WD=WakeDisplay?WakeDisplay->GameThread_GetRenderTargetResource():nullptr;auto* WA=WakeAeration?WakeAeration->GameThread_GetRenderTargetResource():nullptr;auto* WN=WakeNear?WakeNear->GameThread_GetRenderTargetResource():nullptr;
 auto* CF=CausticsFine?CausticsFine->GameThread_GetRenderTargetResource():nullptr;auto* CB=CausticsBroad?CausticsBroad->GameThread_GetRenderTargetResource():nullptr;
 const auto* Sun=DayNight.IsValid()?DayNight->GetSun():nullptr;
 const FVector SunWorld=Sun?-Sun->GetForwardVector():FVector::UpVector;
 const FVector3f CausticSun(SunWorld.X,SunWorld.Z,-SunWorld.Y);
 // No focus map work at night or when viewing from far above the water.
 const bool RenderCaustics=CF && CB && SunWorld.Z>.035 && (!Sun || Sun->Intensity>25) && Eye.Z<SourceOrigin.Z+1200;
 ENQUEUE_RENDER_COMMAND(NeonCityCoastalFFT)([Shared,D,G,Seabed,HeightField,ShoreField,ShoreControls,DepthBounds,DepthResolution,Parameters,Request,Reset,DT,WakeInput,HullField,PileField,WD,WA,WN,CF,CB,CausticSun,RenderCaustics](FRHICommandListImmediate& Cmd)
 {
  FNeonCityOceanQueryResult Result;
  if(Shared->Queries.TryConsumeLatest_RenderThread(Result)){FScopeLock Lock(&Shared->Mutex);Shared->Latest=Result;Shared->HasResult=true;}
  if(Reset)Shared->FFT.Reset_RenderThread();
  FRDGBuilder Graph(Cmd);const auto Frame=Shared->FFT.AddFrame_RenderThread(Graph,Parameters,float(Request.SimulationTime),DT);
  FNeonCityWakeFrame WakeFrame;
  if(Frame.Displacement && Frame.Derivatives)
  {
   if(RenderCaustics)
    AddNeonCityCaustics(Graph,Frame.Derivatives,
     Graph.RegisterExternalTexture(CreateRenderTarget(CF->GetRenderTargetTexture(),TEXT("Ocean.CausticsFine"))),
     Graph.RegisterExternalTexture(CreateRenderTarget(CB->GetRenderTargetTexture(),TEXT("Ocean.CausticsBroad"))),
     CausticSun,FVector2f(Parameters.Sizes[3].X,Parameters.Sizes[2].X),Request.Amplitude);
   FNeonCityOceanBathymetry Depth;
   if(Seabed && Seabed->TextureRHI)
   {
    Depth.Depth=Graph.RegisterExternalTexture(CreateRenderTarget(Seabed->TextureRHI,TEXT("NeonCity.OceanSeabed")));
    if(HeightField && HeightField->TextureRHI)Depth.IslandHeight=Graph.RegisterExternalTexture(CreateRenderTarget(HeightField->TextureRHI,TEXT("NeonCity.IslandHeight")));
    if(ShoreField && ShoreField->TextureRHI)Depth.IslandShore=Graph.RegisterExternalTexture(CreateRenderTarget(ShoreField->TextureRHI,TEXT("NeonCity.IslandRefraction")));
    Depth.ShoreControls=ShoreControls;
    Depth.Bounds=DepthBounds;Depth.Origin=FVector2f(SourceOrigin.X,SourceOrigin.Y);Depth.Resolution=DepthResolution;
   }
   if(Depth.IslandHeight && HullField && PileField && WD && WA && WN)
   {
    auto Hull=Graph.RegisterExternalTexture(CreateRenderTarget(HullField->TextureRHI,TEXT("Wake.Hull")));
    auto Piles=Graph.RegisterExternalTexture(CreateRenderTarget(PileField->TextureRHI,TEXT("Wake.Piles")));
    WakeFrame=Shared->Wake.AddFrame_RenderThread(Graph,WakeInput,float(Request.SimulationTime),DT,Depth.IslandHeight,Hull,Piles);
    if(WakeFrame.Stepped)
    {
     AddCopyTexturePass(Graph,WakeFrame.Display,Graph.RegisterExternalTexture(CreateRenderTarget(WD->GetRenderTargetTexture(),TEXT("Wake.MaterialDisplay"))));
     AddCopyTexturePass(Graph,WakeFrame.Aeration,Graph.RegisterExternalTexture(CreateRenderTarget(WA->GetRenderTargetTexture(),TEXT("Wake.MaterialAeration"))));
     AddCopyTexturePass(Graph,WakeFrame.Near,Graph.RegisterExternalTexture(CreateRenderTarget(WN->GetRenderTargetTexture(),TEXT("Wake.MaterialNear"))));
    }
   }
   Shared->Queries.AddQueries_RenderThread(Graph,Frame.Displacement,Parameters,Request,&Depth,&WakeFrame);
   Graph.QueueTextureExtraction(Frame.Displacement,&Shared->OutputDisplacement);
   Graph.QueueTextureExtraction(Frame.Derivatives,&Shared->OutputDerivatives);
  }
  Graph.Execute();
  {FScopeLock Lock(&Shared->Mutex);Shared->WakeControls=WakeFrame.Controls;Shared->WakeBoat=WakeFrame.Boat;Shared->WakeHullScale=WakeFrame.HullScale;}
  if(Shared->OutputDisplacement && Shared->OutputDerivatives)
  {
   UTidewaterOceanTexture::Publish_RenderThread(Cmd,D,Shared->OutputDisplacement->GetRHI());
   UTidewaterOceanTexture::Publish_RenderThread(Cmd,G,Shared->OutputDerivatives->GetRHI());Shared->Published=true;
  }
  Shared->Busy=false;
 });
}
#if WITH_DEV_AUTOMATION_TESTS
FNeonCityOceanLeaseDiagnostic ACoastalOceanSurface::GetBodyLeaseDiagnostic(UPrimitiveComponent* Body,int32 Index,FVector P)const
{
 FNeonCityOceanLeaseDiagnostic D;
 if(!Runtime || !bReady || P.ContainsNaN())return D;
 D.State=1;
 for(int32 I=2;I<64;++I)
 {
  const auto& S=Runtime->Leases[I];if(S.Body!=Body || S.Index!=Index)continue;
  const FVector Source=ToSource(P);D.IssuedAge=WaveSeconds-S.Time;D.FootprintMetres=(FVector2D(Source.X,Source.Z)-S.Point).Size();
  D.State=!S.Valid?2:D.FootprintMetres>20?3:D.IssuedAge>1?4:5;return D;
 }
 return D;
}
#endif
bool ACoastalOceanSurface::SampleBody(UPrimitiveComponent* Body,int32 Index,FVector P,float& Height,FVector& Normal,FVector& Velocity,FTidewaterWaterSample* Snapshot)const
{
 if(!Runtime || !bReady || P.ContainsNaN())return false;
 for(int32 I=2;I<64;++I)
 {
  const auto& S=Runtime->Leases[I];if(S.Body!=Body || S.Index!=Index || !S.Valid)continue;
  const FVector Source=ToSource(P);const FVector2D Offset=FVector2D(Source.X,Source.Z)-S.Point;
  if(Offset.SizeSquared()>400 || WaveSeconds-S.Time>1)return false;
  if(Snapshot)*Snapshot=S;
  // Bounded local tangent correction; no extrapolation beyond the named hull footprint.
  Height=SourceOrigin.Z+100*(S.Effective+FMath::Clamp(FVector2D::DotProduct(S.Slope,Offset),-.5,.5));
  Normal=FVector(-S.Slope.X,S.Slope.Y,1).GetSafeNormal();Velocity=FVector(0,0,S.Vertical*100);return true;
 }
 return false;
}
void ACoastalOceanSurface::Sample(FVector P,float& Height,FVector& Normal,FVector& Velocity)const
{
 Height=Field?Field->SeaLevel:float(SourceOrigin.Z);Normal=FVector::UpVector;Velocity=FVector::ZeroVector;
 if(!Runtime || !bReady || P.ContainsNaN())return;
 const FVector Source=ToSource(P);const FVector2D Q(Source.X,Source.Z);const FWaterLease* Best=nullptr;double Distance=25;
 for(int32 I=0;I<64;++I)
 {
  const auto& S=Runtime->Leases[I];if(!S.Valid || (I>1 && !S.Body.IsValid()) || WaveSeconds-S.Time>1)continue;
  const double D=(Q-S.Point).SizeSquared();if(D<Distance){Distance=D;Best=&S;}
 }
 if(Best)
 {
  Height=SourceOrigin.Z+100*(Best->Effective+FMath::Clamp(FVector2D::DotProduct(Best->Slope,Q-Best->Point),-.5,.5));
  Normal=FVector(-Best->Slope.X,Best->Slope.Y,1).GetSafeNormal();Velocity.Z=Best->Vertical*100;
 }
}
uint64 ACoastalOceanSurface::GetQuerySequence()const{return Runtime?Runtime->Sequence:0;}
int32 ACoastalOceanSurface::GetSurfaceNodeCount()const{return Runtime?Runtime->Nodes.Num():0;}
#if WITH_DEV_AUTOMATION_TESTS
FNeonCityOceanInstanceDiagnostic ACoastalOceanSurface::GetSurfaceInstanceDiagnostic()const
{
 FNeonCityOceanInstanceDiagnostic Result;
 if(!Runtime)return Result;
 const auto& State=Runtime->Instances;const auto& Stats=State.GetStatistics();
 Result.TrackedNodes=State.Num();Result.RenderedInstances=Surface?Surface->GetInstanceCount():0;
 Result.ChangedSelections=Stats.Selections;Result.OrderOnlySelections=Stats.OrderOnlySelections;
 Result.TopologyUpdates=Stats.TopologyUpdates;Result.AddedNodes=Stats.Added;Result.RemovedNodes=Stats.Removed;
 return Result;
}
#endif
bool ACoastalOceanSurface::HasPhysicalWakeFor(const AActor* Boat)const
{return bReady && WakeDisplay && Runtime && Runtime->WakeBoat.Get()==Boat;}
void ACoastalOceanSurface::EndPlay(EEndPlayReason::Type Reason)
{
 SetActorTickEnabled(false);bReady=false;Surface->SetVisibility(false);
 if(Reason==EEndPlayReason::Destroyed)for(auto& Old:HiddenLegacySurfaces)if(Old.IsValid())Old->SetActorHiddenInGame(false);
 HiddenLegacySurfaces.Reset();
 if(Runtime)
 {
  const auto Shared=Runtime->Render;
  ENQUEUE_RENDER_COMMAND(NeonCityCoastalRelease)([Shared](FRHICommandListImmediate&)
  {Shared->Queries.Reset_RenderThread();Shared->FFT.Reset_RenderThread();Shared->Wake.Reset_RenderThread();Shared->OutputDisplacement.SafeRelease();Shared->OutputDerivatives.SafeRelease();});
  Runtime.Reset();
 }
 Super::EndPlay(Reason);
}
