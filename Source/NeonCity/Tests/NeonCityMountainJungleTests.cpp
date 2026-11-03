#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityForestOcclusionProbe.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/Foliage/NeonCityFoliageInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "AssetCompilingManager.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMountainJungleData,"NeonCity.Marine.MountainJungleData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMountainJungleData::RunTest(const FString&)
{
 auto* Data=LoadObject<UNeonCityReferenceVegetationData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/DA_NCTW_MountainJungle"));
 if(!TestNotNull(TEXT("Persisted authored mountain density layer"),Data))return false;
 TestNotNull(TEXT("Source distance representation retained for added crowns"),Data->CanopyImpostor.LoadSynchronous());
 const TMap<FName,int32> Expected={{TEXT("trees"),20510},{TEXT("shrubs"),6254},{TEXT("ferns"),1852},{TEXT("monsteras"),419}};
 TMap<FName,int32> Counts;bool Upright=true,Aligned=true,Bounded=true;
 for(const auto& G:Data->Groups)
 {
  Counts.FindOrAdd(G.SourceFamily)+=G.Instances.Num();
  auto* Mesh=G.Mesh.LoadSynchronous();
  if(TestNotNull(TEXT("Density uses existing reviewed source geometry"),Mesh))
  {
   TestTrue(TEXT("No substitute primitive meshes"),Mesh->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_")));
   if(G.SourceFamily==TEXT("trees") || G.SourceFamily==TEXT("shrubs"))
   {
    FAssetCompilingManager::Get().FinishAllCompilation();
    TestTrue(TEXT("Dense near crowns have built Nanite resources"),Mesh->HasValidNaniteData());
    TestTrue(TEXT("Thin leaf area and packed vertex attributes are preserved"),
     Mesh->GetNaniteSettings().ShapePreservation==ENaniteShapePreservation::PreserveArea && !Mesh->GetNaniteSettings().bLerpUVs);
   }
  }
  for(const auto& P:G.Instances)
  {
   const FVector Pos=P.Transform.GetLocation();const auto B=P.ShaderData1;
   Upright&=!P.Transform.ContainsNaN() && FVector::DotProduct(P.Transform.GetRotation().GetUpVector(),FVector::UpVector)>.99999;
   Aligned&=Pos.Equals(FVector(B.G,-B.A,B.B)*100,.05);
   const bool Understory=G.SourceFamily==TEXT("ferns") || G.SourceFamily==TEXT("monsteras");
   // Scatter bounds describe grid-cell origins; its last 5.2m shrub cell
   // (3.65m for trees) can extend beyond the nominal Z=-142m edge.
   Bounded&=FMath::Abs(Pos.X)<55000 && Pos.Y>(Understory?13200:13600) && Pos.Y<82000 && Pos.Z>500 && Pos.Z<25000;
   if(G.SourceFamily==TEXT("trees"))Bounded&=P.Transform.GetScale3D().GetMax()<1.8;
  }
 }
 for(const auto& E:Expected)TestEqual(*FString::Printf(TEXT("Authored %s count"),*E.Key.ToString()),Counts.FindRef(E.Key),E.Value);
 TestTrue(TEXT("Added plant rotations remain upright and finite"),Upright);
 TestTrue(TEXT("Added shader roots and transforms remain aligned"),Aligned);
 TestTrue(TEXT("Canopy and trail understory retain their respective authored bounds"),Bounded);
 return true;
}

namespace
{
// Authored source hiking path, not a straight line through the forest. Once
// placed at the initial fixture every centimetre is normal template movement.
class FMountainJungleWalk : public IAutomationLatentCommand
{
public:
 explicit FMountainJungleWalk(FAutomationTestBase* In):Test(In) {}
 ~FMountainJungleWalk() override { RestoreLightingProbe();RestoreTransportProbe();RestoreSkyCaptureProbe(); }
 void RestoreSkyCaptureProbe()
 {
  if(SkyCaptureStage==0)return;
  if(SkyCaptureComponent.IsValid())SkyCaptureComponent->SetRealTimeCaptureEnabled(SkyCaptureRealtime);
  if(auto* W=GEditor?GEditor->PlayWorld.Get():nullptr)
   if(auto* PC=W->GetFirstPlayerController())PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));
 }
 void RestoreTransportProbe()
 {
  if(TransportStage==0)return;
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing.LightingMode")))V->Set(TransportMode,ECVF_SetByConsole);
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing.MaxIterations")))V->Set(TransportIterations,ECVF_SetByConsole);
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.MaxRayIntensity")))V->Set(TransportScreenIntensity,ECVF_SetByConsole);
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.Radiosity.MaxRayIntensity")))V->Set(TransportRadiosityIntensity,ECVF_SetByConsole);
  if(TransportVolume.IsValid())TransportVolume->DestroyComponent();TransportVolume.Reset();
  if(auto* W=GEditor?GEditor->PlayWorld.Get():nullptr)
   if(auto* PC=W->GetFirstPlayerController())PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));
 }
 void RestoreLightingProbe()
 {
  if(LightingStage==0)return;
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing")))V->Set(LightingHW,ECVF_SetByConsole);
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow")))V->Set(LightingGI,ECVF_SetByConsole);
  if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Visualize")))V->Set(LightingVisualize,ECVF_SetByConsole);
  for(const auto& Item:LightingVisibility)if(Item.Key.IsValid())Item.Key->SetVisibility(Item.Value);
  LightingVisibility.Reset();
  if(auto* W=GEditor?GEditor->PlayWorld.Get():nullptr)
   if(auto* PC=W->GetFirstPlayerController())
   {PC->SetControlRotation(LightingRotation);PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));}
 }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  const bool TransportProbe=FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestTransportProbe"));
  const bool OcclusionProbe=FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestOcclusionProbe"));
  const bool ExposureTransportProbe=FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestExposureTransportProbe"));
  if(FPlatformTime::Seconds()-Started>((TransportProbe || OcclusionProbe || ExposureTransportProbe)?240:165)){Test->AddError(TEXT("Mountain trail timed out"));return true;}
  UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || World->GetTimeSeconds()<.2f)return false;
  auto* Move=Player->GetCharacterMovement();
  if(!Ready)
  {
   int32 Original=0,Added=0,LayerCount=0,Probes=0;double MinEmbed=1e9,MaxEmbed=-1e9;
   for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)
   {
    if(It->ActorHasTag(TEXT("NeonCity.ReferenceVegetation")))Original+=It->GetPlantCount();
    if(!It->ActorHasTag(TEXT("NeonCity.MountainJungle")))continue;
    ++LayerCount;Added+=It->GetPlantCount();
    Test->TestEqual(TEXT("Every added tree and shrub retains its far view"),It->GetImpostorCount(),26764);
    if(!Test->TestNotNull(TEXT("Saved density actor has its authored data"),It->Data.Get()))return true;
    for(const auto& G:It->Data->Groups)for(int32 I=0;I<G.Instances.Num();I+=277)
    {
     const FVector Root=It->GetActorTransform().TransformPosition(G.Instances[I].Transform.GetLocation());
     FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(JungleRootSupport),false,Player);
     // This audit measures terrain grounding, not the new wood collision above it.
     for(TActorIterator<ANeonCityReferenceVegetation> Ignore(World);Ignore;++Ignore)Params.AddIgnoredActor(*Ignore);
     if(!World->LineTraceSingleByChannel(Hit,Root+FVector(0,0,1200),Root-FVector(0,0,1200),ECC_Visibility,Params))
     {Test->AddError(TEXT("Density plant has no native terrain support"));return true;}
     const double Embed=Hit.ImpactPoint.Z-Root.Z;MinEmbed=FMath::Min(MinEmbed,Embed);MaxEmbed=FMath::Max(MaxEmbed,Embed);++Probes;
    }
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(*It);
    for(auto* C:Components)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName()==TEXT("SM_NCTW_CanopyImpostor"))
    {
     Test->TestFalse(TEXT("Additional distant canopy cannot flood dynamic shadow allocation"),bool(C->CastShadow));
     Test->TestTrue(TEXT("Far foliage cannot duplicate real tree GI/ray occlusion"),
      !C->bVisibleInRayTracing && !C->bAffectDynamicIndirectLighting && !C->bAffectDistanceFieldLighting);
    }
   }
   Test->TestEqual(TEXT("Canonical source vegetation is untouched"),Original,65383);
   Test->TestEqual(TEXT("Single reversible authored density layer"),LayerCount,1);
   Test->TestEqual(TEXT("All authored canopy and understory exist in the actual level"),Added,29035);
   Test->TestTrue(TEXT("Added roots contact native triangle terrain without floating"),Probes>70 && MinEmbed>=-1. && MaxEmbed<200.);
   Test->AddInfo(FString::Printf(TEXT("Native root probes=%d embed min=%.3f max=%.3f cm"),Probes,MinEmbed,MaxEmbed));
   FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(JungleTrailFixture),false,Player);
   for(TActorIterator<ANeonCityReferenceVegetation> Ignore(World);Ignore;++Ignore)Params.AddIgnoredActor(*Ignore);
   if(!World->LineTraceSingleByChannel(Hit,FVector(4900,15000,45000),FVector(4900,15000,-10000),ECC_Visibility,Params))
   {Test->AddError(TEXT("Missing actual hiking trail floor"));return true;}
   Move->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(Hit.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StopMovementImmediately();PC->SetControlRotation(FRotator(-8,90,0));
   auto* Clock=World->SpawnActor<ANeonCityDayNightCycle>();Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(11);
   World->SpawnActor<ASkyAtmosphere>();Ready=true;WaitUntil=World->GetTimeSeconds()+3;LastProgress=WaitUntil;
   StartLocation=Hit.ImpactPoint;return false;
  }
  Evidence.Record(World,PC,Point);
  int32 Wood=0,Candidates=0;
  for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)if(auto* Contact=It->GetFoliageInteraction())
  {
   Wood+=Contact->GetActiveWoodCount();Candidates+=Contact->GetLastCandidateCount();
   CapacityMisses+=Contact->GetCapacityMissCount();
  }
  PeakWood=FMath::Max(PeakWood,Wood);PeakCandidates=FMath::Max(PeakCandidates,Candidates);
  if(World->GetTimeSeconds()<WaitUntil)return false;
  const FVector2D Points[]={FVector2D(4950,16200),FVector2D(4700,17500),FVector2D(4100,18800),FVector2D(3600,19900),
   FVector2D(3100,21200),FVector2D(3300,22400),FVector2D(2600,23600),FVector2D(2000,24700),FVector2D(2400,25800),
   FVector2D(1600,27000),FVector2D(1100,28300)};
  const FVector2D To=Points[Point]-FVector2D(Player->GetActorLocation());
  if(World->GetTimeSeconds()-LastProgress>18)
  {Test->AddError(FString::Printf(TEXT("Actual authored mountain trail blocked at %d: %s"),Point,*Player->GetActorLocation().ToString()));return true;}
  if(To.Size()>55)
  {
   PC->SetControlRotation(FRotator(-8,FMath::RadiansToDegrees(FMath::Atan2(To.Y,To.X)),0));
   Player->DoMove(0,FMath::Clamp(To.Size()/150.,.25,1.));return false;
  }
  if(Point==6 && ExposureTransportProbe && !ExposureTransportDone)
  {
   LastProgress=World->GetTimeSeconds();ExposureTransportDone=ExposureTransport.Update(World,PC,Player,Test);
   if(!ExposureTransportDone)return false;
  }
  if(Point==6 && OcclusionProbe && !OcclusionDone)
  {
   LastProgress=World->GetTimeSeconds();OcclusionDone=Occlusion.Update(World,PC,Player,Test);
   if(!OcclusionDone)return false;
  }
  // Optional cost diagnosis in the same normally walked forest, after the
  // player naturally stops. It never replaces the route or its assertions.
  if(Point==6 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestProfile")) && ProfileStage<2)
  {
   if(ProfileStage==0){ProfileAt=World->GetTimeSeconds()+3;ProfileStage=1;return false;}
   if(World->GetTimeSeconds()<ProfileAt)return false;
   PC->ConsoleCommand(TEXT("r.ProfileGPU.ShowUI 0"));PC->ConsoleCommand(TEXT("ProfileGPU"));
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("JungleTrail-06-Settled.png"));
   ProfileStage=2;return false;
  }
  if(Point==6 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestDiagnostics")) && DiagnosticStage<3)
  {
   const float Now=World->GetTimeSeconds();LastProgress=Now;
   if(DiagnosticStage==0)
   {
    int32 Clocks=0;
    for(TActorIterator<ANeonCityDayNightCycle> It(World);It;++It)
    {
     ++Clocks;auto* Sky=It->FindComponentByClass<USkyLightComponent>();auto* Sun=It->GetSun();
     Test->AddInfo(FString::Printf(TEXT("Forest clock %.2fh sun %.3f lux rotation %s sky %.3f realtime=%d"),It->GetTimeOfDay(),Sun->Intensity,*Sun->GetComponentRotation().ToString(),Sky?Sky->Intensity:-1,Sky && Sky->IsRealTimeCaptureEnabled()));
    }
    Test->TestEqual(TEXT("Forest diagnostic has exactly one daylight owner"),Clocks,1);
    PC->ConsoleCommand(TEXT("viewmode VisualizeBuffer"));PC->ConsoleCommand(TEXT("r.BufferVisualizationTarget BaseColor"));
    Test->TestTrue(TEXT("Diagnostic base-colour view really enabled"),World->GetGameViewport()->EngineShowFlags.VisualizeBuffer);
    DiagnosticAt=Now+3;DiagnosticStage=1;return false;
   }
   if(Now<DiagnosticAt)return false;
   if(DiagnosticStage==1)
   {
    FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Jungle-Diagnostic-BaseColor.png"));
    PC->ConsoleCommand(TEXT("viewmode lit"));PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));
    DiagnosticAt=Now+3;DiagnosticStage=2;return false;
   }
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Jungle-Diagnostic-Exposure.png"));
   PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));DiagnosticStage=3;return false;
  }
  if(Point==6 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestLightingProbe")) && LightingStage<10)
  {
   const float Now=World->GetTimeSeconds();LastProgress=Now;
   if(LightingStage==0)
   {
    LightingHW=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"))->GetInt();
    LightingGI=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DiffuseIndirect.Allow"))->GetInt();
    LightingVisualize=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Visualize"))->GetInt();
    LightingRotation=PC->GetControlRotation();
    LightingAt=Now+3;LightingStage=1;return false;
   }
   if(Now<LightingAt)return false;
   const TCHAR* Labels[]={TEXT(""),TEXT("Lighting-Lit.png"),TEXT("Lighting-Cache.png"),TEXT("Lighting-Indirect.png"),TEXT("Lighting-SoftwareGI.png"),TEXT("Lighting-SkyWithoutGI.png"),TEXT("Lighting-Restored.png"),TEXT("Lighting-OpenHDR.png"),TEXT("Lighting-SkyHDR.png"),TEXT("Lighting-Final.png")};
   FNeonCityVisualEvidence::CaptureHUD(World,Labels[LightingStage]);
   if(LightingStage==1)PC->ConsoleCommand(TEXT("r.Lumen.Visualize 5"));
   if(LightingStage==2)PC->ConsoleCommand(TEXT("r.Lumen.Visualize 13"));
   if(LightingStage==3){PC->ConsoleCommand(TEXT("r.Lumen.Visualize 0"));PC->ConsoleCommand(TEXT("r.Lumen.HardwareRayTracing 0"));}
   if(LightingStage==4)
   {PC->ConsoleCommand(FString::Printf(TEXT("r.Lumen.HardwareRayTracing %d"),LightingHW));PC->ConsoleCommand(TEXT("r.Lumen.DiffuseIndirect.Allow 0"));}
   if(LightingStage==5)PC->ConsoleCommand(FString::Printf(TEXT("r.Lumen.DiffuseIndirect.Allow %d"),LightingGI));
   if(LightingStage==6)
   {
    // Isolate atmosphere/ground light from the canopy in this diagnostic only.
    // Original visibility is restored on completion, timeout and teardown.
    for(TActorIterator<ANeonCityReferenceVegetation> It(World);It;++It)
    {
     TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(*It);
     for(auto* C:Components){LightingVisibility.Add(C,C->IsVisible());C->SetVisibility(false);}
    }
    PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));
   }
   if(LightingStage==7)PC->SetControlRotation(FRotator(60,LightingRotation.Yaw,0));
   if(LightingStage==8)RestoreLightingProbe();
   ++LightingStage;LightingAt=Now+5;return false;
  }
  if(Point==6 && FParse::Param(FCommandLine::Get(),TEXT("NeonCityForestSkyCaptureProbe")) && SkyCaptureStage<6)
  {
   const float Now=World->GetTimeSeconds();LastProgress=Now;
   if(SkyCaptureStage==0)
   {
    for(TActorIterator<ANeonCityDayNightCycle> It(World);It;++It)
     if(auto* Sky=It->FindComponentByClass<USkyLightComponent>()){SkyCaptureComponent=Sky;break;}
    if(!Test->TestTrue(TEXT("Sky capture probe has the actual world sky component"),SkyCaptureComponent.IsValid()))return true;
    SkyCaptureRealtime=SkyCaptureComponent->IsRealTimeCaptureEnabled();
    Test->AddInfo(FString::Printf(TEXT("Sky probe source=%d intensity=%.4f location=%s distance=%.1f"),
     int32(SkyCaptureComponent->SourceType),SkyCaptureComponent->Intensity,*SkyCaptureComponent->GetComponentLocation().ToString(),SkyCaptureComponent->SkyDistanceThreshold));
    for(TActorIterator<AActor> It(World);It;++It)if(auto* Atmosphere=It->FindComponentByClass<USkyAtmosphereComponent>())
     Test->AddInfo(FString::Printf(TEXT("Atmosphere probe %s location=%s"),*It->GetName(),*Atmosphere->GetComponentLocation().ToString()));
    SkyCaptureAt=Now+3;SkyCaptureStage=1;return false;
   }
   if(Now<SkyCaptureAt)return false;
   const TCHAR* Labels[]={TEXT(""),TEXT("SkyCapture-Realtime.png"),TEXT("SkyCapture-RealtimeHDR.png"),
    TEXT("SkyCapture-Recaptured.png"),TEXT("SkyCapture-RecapturedHDR.png"),TEXT("SkyCapture-Restored.png")};
   FNeonCityVisualEvidence::CaptureHUD(World,Labels[SkyCaptureStage]);
   if(SkyCaptureStage==1 || SkyCaptureStage==3)PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));
   if(SkyCaptureStage==2)
   {
    PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));
    SkyCaptureComponent->SetRealTimeCaptureEnabled(false);SkyCaptureComponent->RecaptureSky();
    Test->AddInfo(TEXT("Diagnostic only: compare a settled one-shot sky capture; no saved light/settings changes"));
   }
   if(SkyCaptureStage==4)
   {
    // This CPU SH belongs to the settled one-shot capture, not the GPU-only
    // real-time map. Read it after both eight-second capture intervals.
    const auto SH=SkyCaptureComponent->GetIrradianceEnvironmentMap();
    Test->AddInfo(FString::Printf(TEXT("Settled one-shot sky SH0 RGB %.9g %.9g %.9g; texture=%d"),
     SH.R.V[0],SH.G.V[0],SH.B.V[0],SkyCaptureComponent->GetProcessedSkyTexture()!=nullptr));
    RestoreSkyCaptureProbe();
   }
   ++SkyCaptureStage;SkyCaptureAt=Now+8;return false;
  }
  if(Point==6 && TransportProbe && TransportStage<11)
  {
   const float Now=World->GetTimeSeconds();LastProgress=Now;
   if(TransportStage==0)
   {
    TransportMode=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing.LightingMode"))->GetInt();
    TransportIterations=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing.MaxIterations"))->GetInt();
    TransportScreenIntensity=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.MaxRayIntensity"))->GetFloat();
    TransportRadiosityIntensity=IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.Radiosity.MaxRayIntensity"))->GetFloat();
    auto* Volume=NewObject<UPostProcessComponent>(Player);Volume->bUnbound=true;Volume->Priority=1000;
    Player->AddInstanceComponent(Volume);Volume->RegisterComponent();TransportVolume=Volume;
    TransportAt=Now+3;TransportStage=1;return false;
   }
   if(Now<TransportAt)return false;
   const TCHAR* Labels[]={TEXT(""),TEXT("Transport-Baseline.png"),TEXT("Transport-HitLighting.png"),
    TEXT("Transport-TraversalBudget.png"),TEXT("Transport-SceneDetail.png"),TEXT("Transport-Sky005.png"),TEXT("Transport-Sky020.png"),TEXT("Transport-RayBudget.png"),TEXT("Transport-FixedEV9.png"),TEXT("Transport-FixedEV9HDR.png"),TEXT("Transport-Restored.png")};
   FNeonCityVisualEvidence::CaptureHUD(World,Labels[TransportStage]);
   if(TransportStage==1)PC->ConsoleCommand(TEXT("r.Lumen.HardwareRayTracing.LightingMode 1"));
   if(TransportStage==2)
   {PC->ConsoleCommand(FString::Printf(TEXT("r.Lumen.HardwareRayTracing.LightingMode %d"),TransportMode));PC->ConsoleCommand(TEXT("r.Lumen.HardwareRayTracing.MaxIterations 16384"));}
   if(TransportStage==3)
   {
    PC->ConsoleCommand(FString::Printf(TEXT("r.Lumen.HardwareRayTracing.MaxIterations %d"),TransportIterations));
    TransportVolume->Settings.bOverride_LumenSceneDetail=true;TransportVolume->Settings.LumenSceneDetail=4;
   }
   if(TransportStage==4)
   {
    auto& S=TransportVolume->Settings;S.bOverride_LumenSceneDetail=false;
    S.bOverride_LumenSkylightLeaking=true;S.LumenSkylightLeaking=.005f;
    S.bOverride_LumenFullSkylightLeakingDistance=true;S.LumenFullSkylightLeakingDistance=2000;
   }
   if(TransportStage==5)TransportVolume->Settings.LumenSkylightLeaking=.02f;
   if(TransportStage==6)
   {
    RestoreTransportProbe();
    PC->ConsoleCommand(TEXT("r.Lumen.ScreenProbeGather.MaxRayIntensity 100000"));
    PC->ConsoleCommand(TEXT("r.LumenScene.Radiosity.MaxRayIntensity 100000"));
   }
   if(TransportStage==7)
   {
    RestoreTransportProbe();
    auto* Volume=NewObject<UPostProcessComponent>(Player);Volume->bUnbound=true;Volume->Priority=1000;
    auto& S=Volume->Settings;S.bOverride_AutoExposureMinBrightness=true;S.AutoExposureMinBrightness=9;
    S.bOverride_AutoExposureMaxBrightness=true;S.AutoExposureMaxBrightness=9;
    Player->AddInstanceComponent(Volume);Volume->RegisterComponent();TransportVolume=Volume;
   }
   if(TransportStage==8)PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 1"));
   if(TransportStage==9){RestoreTransportProbe();PC->ConsoleCommand(TEXT("ShowFlag.VisualizeHDR 0"));}
   ++TransportStage;TransportAt=Now+8;return false;
  }
  Test->TestTrue(TEXT("Mountain hiking retains normal walkable floor"),Move->IsMovingOnGround());
  Test->TestEqual(TEXT("Hiking does not damage the player"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
  if(Point%3==0 || Point==10)FNeonCityVisualEvidence::CaptureHUD(World,FString::Printf(TEXT("JungleTrail-%02d.png"),Point));
  Test->AddInfo(FString::Printf(TEXT("Normal trail waypoint %d %s"),Point,*Player->GetActorLocation().ToString()));
  ++Point;LastProgress=World->GetTimeSeconds();
  if(Point!=UE_ARRAY_COUNT(Points))return false;
  Test->TestTrue(TEXT("Continuous trail climbs through the mountain forest"),Player->GetActorLocation().Z-StartLocation.Z>4000);
  Test->TestTrue(TEXT("Dense authored trail has active physical trees without exceeding the local budget"),PeakWood>10 && PeakWood<=1024 && CapacityMisses==0);
  Test->TestTrue(TEXT("Runtime spatial lookup never scans the whole forest"),PeakCandidates<10000);
  Test->AddInfo(FString::Printf(TEXT("Physical forest peak wood bodies=%d candidates=%d capacity misses=%d"),PeakWood,PeakCandidates,CapacityMisses));
  return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;float WaitUntil=0,LastProgress=0;int32 Point=0;bool Ready=false;
 FVector StartLocation=FVector::ZeroVector;
 int32 PeakWood=0,PeakCandidates=0,CapacityMisses=0;
 int32 ProfileStage=0;float ProfileAt=0;
 int32 DiagnosticStage=0;float DiagnosticAt=0;
 int32 LightingStage=0,LightingHW=1,LightingGI=1,LightingVisualize=0;float LightingAt=0;
 FRotator LightingRotation=FRotator::ZeroRotator;
 TMap<TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>,bool> LightingVisibility;
 int32 TransportStage=0,TransportMode=0,TransportIterations=8192;float TransportAt=0;
 float TransportScreenIntensity=10,TransportRadiosityIntensity=40;
 TWeakObjectPtr<UPostProcessComponent> TransportVolume;
 int32 SkyCaptureStage=0;float SkyCaptureAt=0;bool SkyCaptureRealtime=false;
 TWeakObjectPtr<USkyLightComponent> SkyCaptureComponent;
 FNeonCityVisualEvidence Evidence{TEXT("MountainJungleTrail")};
 FNeonCityForestOcclusionProbe Occlusion;bool OcclusionDone=false;
 FNeonCityForestExposureTransportProbe ExposureTransport;bool ExposureTransportDone=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMountainJungleWalkTest,"NeonCity.Marine.MountainJungleTrailPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMountainJungleWalkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMountainJungleWalk>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
