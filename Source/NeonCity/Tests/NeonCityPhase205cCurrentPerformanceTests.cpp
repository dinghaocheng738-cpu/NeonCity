#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Adventure/IslandActivityJob.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Adventure/IslandLandResidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "RenderTimer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace CurrentLandPerformance
{
struct FView
{
 FString Name,Source;
 FVector Eye,Aim;
 float Hour=12,FOV=70;
};

/** Fixed-camera performance only. New measured residence cameras replace the
 * old ground-floor interior camera. No input, pawn/transport pose, save,
 * screenshot, movie, GPU readback or gameplay completion fixture is used.
 * This explicitly gated run is held until the final native geometry is frozen.
 */
class FFlow : public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* InTest,bool InFinalGeometry=true,bool InDiagnostic=false):Test(InTest),Metrics(TEXT("CurrentSixScene")),FinalGeometry(InFinalGeometry),Diagnostic(InDiagnostic) {}
 ~FFlow(){Restore();}

 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();
  if(!Started)Started=Now;
  W=NeonCityValidationWorld();
  PC=W?W->GetFirstPlayerController():nullptr;
  if(Now-Started>600)return End(TEXT("Current six-scene performance finite timeout"));
  if(!W || !PC || !W->HasBegunPlay() || !W->GetGameViewport() || !W->GetGameViewport()->Viewport)return false;

  if(Directory.IsEmpty())
  {
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);
   const FString Existing=Directory/TEXT("Metrics/CurrentSixScene-allframes.csv");
   if(IFileManager::Get().FileExists(*Existing) || IFileManager::Get().FileExists(*(Directory/TEXT("Metrics/CurrentSixScene-cpu.csv"))))
   {
    Test->AddError(TEXT("Retained performance evidence exists; use a fresh run label"));Directory.Reset();return true;
   }
   IFileManager::Get().MakeDirectory(*(Directory/TEXT("Metrics")),true);
   CPUFile=Directory/TEXT("Metrics/CurrentSixScene-cpu.csv");
   if(!FFileHelper::SaveStringToFile(TEXT("sample_index,engine_frame,wall_seconds,game_seconds,stage,scene_seconds,game_thread_ms,render_thread_ms,rhi_thread_ms,game_critical_path_ms,render_critical_path_ms,game_wait_ms,render_wait_ms,present_ms\n"),*CPUFile))
    return End(TEXT("Dedicated raw CPU timing header write failed"));
   auto* Viewport=static_cast<FSceneViewport*>(W->GetGameViewport()->Viewport);
   Viewport->SetFixedViewportSize(1600,900);
   for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.ScreenPercentage")})
   {
    auto* C=IConsoleManager::Get().FindConsoleVariable(Name);
    if(!C)return End(FString(TEXT("Required rendering policy variable absent: "))+Name);
    OldPolicy.Add(Name,C->GetFloat());
    const float Target=FCString::Strcmp(Name,TEXT("r.ScreenPercentage"))==0?100.f:3.f;
    // Reapplying identical scalability values recreates foliage/render state
    // and flushes async loading. Retain real cold frames without causing an
    // avoidable artificial hitch at the beginning of the measurement.
    if(!FMath::IsNearlyEqual(C->GetFloat(),Target))C->Set(Target,ECVF_SetByConsole);
   }
  }

  if(!Initialized)
  {
   Sample(TEXT("Startup_Cold"),Now-Started);
   if(W->GetTimeSeconds()<4)return false;
   Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(Region->GetActorLocation()+FVector(-1400,51200,30610)))return false;
   FString Failure;
   if(!BuildViews(Failure))return End(Failure);
   if(Diagnostic)Views.SetNum(1); // One unchanged Village view; never a six-scene acceptance gate.
   if(FParse::Value(FCommandLine::Get(),TEXT("NeonCityPerformanceScene="),FocusedScene))
   {
    if(Diagnostic || (FocusedScene!=TEXT("Night") && FocusedScene!=TEXT("VillageAndAerial") && FocusedScene!=TEXT("HelicopterAerial")))return End(TEXT("Only explicitly affected views may use this bounded repair"));
    Views.RemoveAll([&](const auto& V){return FocusedScene==TEXT("VillageAndAerial")?(V.Name!=TEXT("Village") && V.Name!=TEXT("HelicopterAerial")):V.Name!=FocusedScene;});
    if(Views.Num()!=(FocusedScene==TEXT("VillageAndAerial")?2:1))return End(TEXT("Requested performance repair scene is absent"));
   }
   OldView=PC->GetViewTarget();
   OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;
   Clock->SetCycleEnabled(false);ClockChanged=true;
   Camera=W->SpawnActor<ACameraActor>();
   if(!Camera)return End(TEXT("Dedicated performance camera creation failed"));
   PC->SetViewTarget(Camera);
   Initialized=true;
   Place(Now);StreamingAt=Now;
   return false;
  }

  const FView& View=Views[ViewIndex];
  if(Now-StreamingAt>=1)
  {
   if(!Region->EnsureDestinationReady(View.Eye))return End(TEXT("Current performance view lost native island streaming"));
   StreamingAt=Now;
  }
  if(Now-PolicyAt>=5)
  {
   if(!PolicyIsValid())return End(TEXT("Current performance policy must remain 1600x900 Epic/100%"));
   PolicyAt=Now;
  }
  const double Elapsed=Now-ViewStarted;
  if(Diagnostic && !Profiled && Elapsed>=25)
  {PC->ConsoleCommand(TEXT("r.ProfileGPU.ShowUI 0"));PC->ConsoleCommand(TEXT("ProfileGPU"));Profiled=true;}
  const FString Stage=View.Name+(Elapsed<2?TEXT("_ColdWarmup"):Elapsed<20?TEXT("_Warmup"):TEXT("_Sample"));
  Sample(Stage,Elapsed);
  if(Elapsed<50)return false;
  auto Row=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("name"),View.Name);Row->SetStringField(TEXT("camera_source"),View.Source);
  Row->SetStringField(TEXT("eye_cm"),View.Eye.ToString());Row->SetStringField(TEXT("aim_cm"),View.Aim.ToString());
  Row->SetNumberField(TEXT("hour"),View.Hour);Row->SetNumberField(TEXT("fov"),View.FOV);
  Row->SetNumberField(TEXT("warmup_seconds"),20);Row->SetNumberField(TEXT("sample_seconds"),Elapsed-20);
  Row->SetNumberField(TEXT("sample_frames"),SceneSampleFrames);
  Row->SetNumberField(TEXT("positive_gpu_sample_frames"),ScenePositiveGPUFrames);
  CompletedViews.Add(MakeShared<FJsonValueObject>(Row));
  if(SceneSampleFrames<100 || ScenePositiveGPUFrames<100)return End(TEXT("Performance timing collection is incomplete; no positive GPU/CPU evidence inferred"));
  Test->AddInfo(FString::Printf(TEXT("Current %s:20s retained cold/warm frames + %.3fs sample, %d frames, no readback"),*View.Name,Elapsed-20,SceneSampleFrames));
  if(++ViewIndex==Views.Num())return End(TEXT(""));
  Place(Now);return false;
 }

private:
 static FVector VectorField(const TSharedPtr<FJsonObject>& Row,const TCHAR* Key)
 {
  const auto& V=Row->GetArrayField(Key);return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());
 }
 FVector HousePoint(FVector M) const {return HouseTransform.TransformPosition(FVector(M.X*100,-M.Y*100,M.Z*100));}

 bool Surface(FVector Point,FHitResult& Hit) const
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentPerformanceNativeSupport),false,PC->GetPawn());
  if(Camera)Q.AddIgnoredActor(Camera);
  return W->LineTraceSingleByChannel(Hit,FVector(Point.X,Point.Y,80000),FVector(Point.X,Point.Y,-20000),ECC_Pawn,Q);
 }
 bool ClearEye(FVector Eye) const
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentPerformanceCameraClearance),false,PC->GetPawn());
  if(Camera)Q.AddIgnoredActor(Camera);
  return !W->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(12),Q);
 }
 bool BuildViews(FString& Failure)
 {
  AStaticMeshActor* FloorActor=nullptr;USplineComponent* Road=nullptr;
  for(TActorIterator<AStaticMeshActor> I(W);I;++I)
   if(I->GetName()==TEXT("StaticMeshActor_433"))FloorActor=*I;
  for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
   if(I->RoadID==TEXT("Island.Mountain.Main"))Road=I->GetRoadSpline();
  for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)
  {
   if(Clock){Failure=TEXT("Shared clock is ambiguous");return false;}Clock=*I;
  }
  if(!FloorActor || !Clock || !Road || Road->GetNumberOfSplinePoints()!=639)
  {Failure=TEXT("Actual The One floor/shared clock/639-point road absent");return false;}
  HouseTransform=FloorActor->GetActorTransform();HouseOrigin=HouseTransform.GetLocation();
  //786 retains the measured703 floor geometry and supplies the current room
  //finishes. Reject obsolete assets without rejecting the installed derivative.
  if(!FloorActor->GetStaticMeshComponent()->GetStaticMesh() || FloorActor->GetStaticMeshComponent()->GetStaticMesh()->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/TheOne786/SM_TheOne786_Ground.SM_TheOne786_Ground"))
  {Failure=TEXT("Current room/site geometry must be installed before performance collection");return false;}
  const FVector InteriorEye=HousePoint(FVector(-10,27.8,4.648)),InteriorAim=HousePoint(FVector(-12.5,23,7.2));
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentPerformanceTheOneFloor),false,PC->GetPawn());FHitResult Floor;
  const FVector FloorPoint=HousePoint(FVector(-10,27.8,3.048));
  if(!W->LineTraceSingleByChannel(Floor,FloorPoint+FVector(0,0,45),FloorPoint-FVector(0,0,25),ECC_Pawn,Q)
   || Floor.ImpactNormal.Z<.7 || FMath::Abs(Floor.ImpactPoint.Z-FloorPoint.Z)>15 || !ClearEye(InteriorEye))
  {Failure=TEXT("Current The One foyer camera needs actual Main floor and eye clearance");return false;}
  FVector RoadEye,RoadAim;
  for(int32 Index=424;Index<=450;++Index)
  {
   const FVector Expected=Road->GetLocationAtSplinePoint(Index,ESplineCoordinateSpace::World);FHitResult Hit;
   if(!W->LineTraceSingleByChannel(Hit,Expected+FVector(0,0,120),Expected-FVector(0,0,200),ECC_Pawn,Q)
    || Hit.ImpactNormal.Z<.5 || FMath::Abs(Hit.ImpactPoint.Z-Expected.Z)>80)
   {Failure=FString::Printf(TEXT("Current road%d support mismatch"),Index);return false;}
   Survey+=FString::Printf(TEXT("road,%d,%.3f,%.3f,%.3f,%s,%s\n"),Index,Hit.ImpactPoint.X,Hit.ImpactPoint.Y,Hit.ImpactPoint.Z,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
   if(Index==430)RoadEye=Hit.ImpactPoint+FVector(0,0,165);
   if(Index==438)RoadAim=Hit.ImpactPoint+FVector(0,0,100);
  }
  const FVector ExteriorEye=HousePoint(FVector(92,-84,25)),ExteriorAim=HousePoint(FVector(20,-12,6));
  const FVector AerialEye=HousePoint(FVector(95,95,90));
  if(!ClearEye(RoadEye) || !ClearEye(ExteriorEye) || !ClearEye(AerialEye))
  {Failure=TEXT("A current camera intersects blocking geometry");return false;}
  Views={
   {TEXT("Village"),TEXT("Retained unchanged Village view"),NeonCityCoastalLayout::FromLegacyIsland(FVector(57100,-157800,630)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57200,-158950,500)),12,70},
   {TEXT("MountainRoad"),TEXT("Current639 actual support430/438"),RoadEye,RoadAim,12,70},
   {TEXT("MansionExterior"),TEXT("Native638 frame; full new leisure/guest/site bounds"),ExteriorEye,ExteriorAim,12,70},
   {TEXT("MansionInterior"),TEXT("Current Main3.048 foyer/duplex, native support checked"),InteriorEye,InteriorAim,12,70},
   {TEXT("Night"),TEXT("Same current foyer, shared clock22"),InteriorEye,InteriorAim,22,70},
   {TEXT("HelicopterAerial"),TEXT("Current estate overview; fixed camera does not certify flight"),AerialEye,HousePoint(FVector(-10,-10,3)),12,70}
  };
  return true;
 }

 void Place(double Now)
 {
  const auto& View=Views[ViewIndex];
  Camera->GetCameraComponent()->FieldOfView=View.FOV;
  Camera->SetActorLocationAndRotation(View.Eye,(View.Aim-View.Eye).Rotation());
  Clock->SetTimeOfDay(View.Hour);PC->SetViewTarget(Camera);
  ViewStarted=Now;SceneSampleFrames=ScenePositiveGPUFrames=0;
 }
 bool PolicyIsValid() const
 {
  if(W->GetGameViewport()->Viewport->GetSizeXY()!=FIntPoint(1600,900))return false;
  for(const auto& Entry:OldPolicy)
  {
   const auto* C=IConsoleManager::Get().FindConsoleVariable(*Entry.Key);
   const float Target=Entry.Key==TEXT("r.ScreenPercentage")?100.f:3.f;
   if(!C || !FMath::IsNearlyEqual(C->GetFloat(),Target))return false;
  }
  return true;
 }
 void Sample(const FString& Stage,double SceneSeconds)
 {
  if(LastEngineFrame==GFrameCounter)return;
  LastEngineFrame=GFrameCounter;
  Metrics.Sample(W,Stage);
  const double Now=FPlatformTime::Seconds();
  // These are the exported UE5.8 raw counters used by FStatUnitData::DrawStat,
  // without its smoothed averaging or a stat overlay. Render/RHI may lag.
  CPURows+=FString::Printf(TEXT("%d,%llu,%.6f,%.6f,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
   SampleIndex++,static_cast<unsigned long long>(GFrameCounter),Now-Started,W->GetTimeSeconds(),*Stage,SceneSeconds,
   FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(GRHIThreadTime),
   FPlatformTime::ToMilliseconds(GGameThreadTimeCriticalPath),FPlatformTime::ToMilliseconds(GRenderThreadTimeCriticalPath),
   FPlatformTime::ToMilliseconds(GGameThreadWaitTime),FPlatformTime::ToMilliseconds(GRenderThreadWaitTime),FPlatformTime::ToMilliseconds(GSwapBufferTime));
  if(Stage.EndsWith(TEXT("_Sample")))
  {
   ++SceneSampleFrames;if(RHIGetGPUFrameCycles()>0)++ScenePositiveGPUFrames;
  }
 }
 void Restore()
 {
  if(IsValid(PC) && OldView.IsValid())PC->SetViewTarget(OldView.Get());
  if(IsValid(Camera)){Camera->Destroy();Camera=nullptr;}
  if(ClockChanged && IsValid(Clock)){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);ClockChanged=false;}
  for(const auto& Entry:OldPolicy)
   if(auto* C=IConsoleManager::Get().FindConsoleVariable(*Entry.Key))
    if(!FMath::IsNearlyEqual(C->GetFloat(),Entry.Value))C->Set(Entry.Value,ECVF_SetByConsole);
  OldPolicy.Reset();
 }
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);
  if(!Metrics.Flush())Test->AddError(TEXT("Final native metric buffer could not be written; no collection PASS"));
  if(!CPUFile.IsEmpty() && !CPURows.IsEmpty())
   if(!FFileHelper::SaveStringToFile(CPURows,*CPUFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append))Test->AddError(TEXT("Raw CPU timing supplement write failed"));
  CPURows.Reset();
  if(!Directory.IsEmpty())
  {
   auto J=MakeShared<FJsonObject>();
   J->SetBoolField(TEXT("technical_collection_passed"),Error.IsEmpty() && !Test->HasAnyErrors() && CompletedViews.Num()==(FocusedScene.IsEmpty()?6:FocusedScene==TEXT("VillageAndAerial")?2:1) && !Diagnostic);
   J->SetStringField(TEXT("focused_scene_repair"),FocusedScene);
   J->SetBoolField(TEXT("single_scene_diagnostic_only"),Diagnostic);
   J->SetStringField(TEXT("world_mode"),W && W->WorldType==EWorldType::PIE?TEXT("PIE"):TEXT("NATIVE_STANDALONE_GAME"));
   J->SetStringField(TEXT("scope"),TEXT("Current six fixed-camera performance scopes in the actual rendered gameplay world. No continuous driving/flight/facility/save/quality completion claim; USER subjective review remains separate."));
   J->SetBoolField(TEXT("computer_control"),false);J->SetBoolField(TEXT("image_readback"),false);J->SetBoolField(TEXT("motion_recording"),false);
   J->SetBoolField(TEXT("final_geometry_frozen_opt_in"),FinalGeometry);
   J->SetStringField(TEXT("validation_purpose"),FinalGeometry?TEXT("FINAL_GEOMETRY_PERFORMANCE_COLLECTION"):TEXT("PRE_CHANGE_BASELINE_DIAGNOSTIC_ONLY; not final acceptance"));
   J->SetNumberField(TEXT("width"),1600);J->SetNumberField(TEXT("height"),900);J->SetNumberField(TEXT("epic_quality"),3);J->SetNumberField(TEXT("screen_percentage"),100);
   J->SetNumberField(TEXT("warmup_seconds_per_scene"),20);J->SetNumberField(TEXT("sample_seconds_per_scene"),30);
   J->SetStringField(TEXT("warmup_policy"),TEXT("Startup cold and every scene cold/warm frame are retained, never removed from raw evidence. Warm sample percentiles and all-frame spikes are reported separately; no stutter trimming."));
   J->SetStringField(TEXT("cpu_api"),TEXT("Actual UE5.8 RenderCore/Public/RenderTimer.h exported raw cycles, same conversions as Engine/Private/UnrealClient.cpp FStatUnitData::DrawStat. Game/render/RHI busy, critical path, wait and present times; render/RHI counters may lag and are process-wide, not exclusive PIE thread timings."));
   J->SetStringField(TEXT("cpu_join"),TEXT("CPU supplement and existing metrics each emit once per engine frame, in identical row order/stage. Analysis must reject count/order mismatch."));
   J->SetStringField(TEXT("gpu_timing"),TEXT("Existing all-frame helper RHIGetGPUFrameCycles latest completed frame, may lag CPU; zero values remain raw and are explicitly unavailable in GPU percentiles."));
   J->SetStringField(TEXT("vram"),TEXT("Existing process-local RHIGetMemoryStats UsedLocal/BudgetLocal/DemotedLocal1Hz with validity and sample age; not whole-system memory."));
   J->SetStringField(TEXT("disk_policy"),TEXT("Existing common helper120-frame buffered CSV flush is retained; CPU supplement buffers until end. All flush-related spikes remain, no per-frame image writes."));
   J->SetStringField(TEXT("night_scope"),TEXT("Current The One foyer Night22 performance only; site lighting visuals remain separate."));
   J->SetStringField(TEXT("actual_residence_origin_cm"),HouseOrigin.ToString());J->SetStringField(TEXT("failure"),Error);J->SetArrayField(TEXT("scenes"),CompletedViews);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
   if(!FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("current-six-scene-scope.json"))))Test->AddError(TEXT("Current performance manifest write failed"));
   FFileHelper::SaveStringToFile(Survey,*(Directory/TEXT("Metrics/CurrentSixScene-native-camera-survey.csv")));
  }
  Restore();return true;
 }

 FAutomationTestBase* Test=nullptr;UWorld* W=nullptr;APlayerController* PC=nullptr;
 ANeonCityCoastalRegion* Region=nullptr;ANeonCityDayNightCycle* Clock=nullptr;ACameraActor* Camera=nullptr;
 TWeakObjectPtr<AActor> OldView;FVector HouseOrigin=FVector::ZeroVector;FTransform HouseTransform=FTransform::Identity;TMap<FString,float> OldPolicy;
 FNeonCityLandFrameMetrics Metrics;TArray<FView> Views;TArray<TSharedPtr<FJsonValue>> CompletedViews;
 FString Directory,CPUFile,CPURows,Survey=TEXT("scope,index,x,y,z,actor,component\n");
 double Started=0,ViewStarted=0,StreamingAt=0,PolicyAt=0;
 uint64 LastEngineFrame=MAX_uint64;int32 ViewIndex=0,SampleIndex=0,SceneSampleFrames=0,ScenePositiveGPUFrames=0;
 FString FocusedScene;
 float OldHour=0;bool OldCycle=false,Initialized=false,ClockChanged=false,FinalGeometry=true,Diagnostic=false,Profiled=false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentSixScenePerformancePIE,"NeonCity.Phase205c.Performance.CurrentSixScenePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentSixScenePerformancePIE::RunTest(const FString&)
{
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"))
  || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityFinalGeometryFrozen"))
  || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || Label.IsEmpty())
 {AddError(TEXT("Explicit metrics/final-geometry-frozen/persistence-disabled flags and a fresh labelled run are required"));return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
 {AddError(TEXT("Current performance scope cannot run with synchronous motion readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(CurrentLandPerformance::FFlow(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneSixScenePerformance,"NeonCity.Phase205c.Estate.Standalone.SixScenePerformance",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FStandaloneSixScenePerformance::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityFinalGeometryFrozen")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"))){AddError(TEXT("Frozen native standalone metrics require unchanged quality, disabled persistence and no motion readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(CurrentLandPerformance::FFlow(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneVillageGPUProfile,"NeonCity.Phase205c.Estate.Standalone.VillageGPUProfile",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FStandaloneVillageGPUProfile::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
 {AddError(TEXT("Single-scene diagnosis requires metrics, disabled persistence and no readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(CurrentLandPerformance::FFlow(this,false,true));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaselineSixScenePerformancePIE,"NeonCity.Phase205c.Performance.BaselineSixScenePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBaselineSixScenePerformancePIE::RunTest(const FString&)
{
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"))
  || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || Label.IsEmpty()
  || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
 {AddError(TEXT("Baseline requires labelled metrics, disabled persistence and no motion readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(CurrentLandPerformance::FFlow(this,false));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
