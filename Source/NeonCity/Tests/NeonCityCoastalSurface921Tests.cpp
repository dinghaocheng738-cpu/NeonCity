#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"
#include "ShowFlags.h"
#include "StaticMeshResources.h"
#include "PrimitiveSceneProxy.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace CoastalSurface921
{
const TCHAR* Files[]={TEXT("C921-NightSurface-00-Baseline.jpg"),TEXT("C921-NightSurface-01-LightingOff.jpg"),
 TEXT("C921-NightSurface-02-DynamicShadowsOff.jpg"),TEXT("C921-NightSurface-03-NaniteFallback.jpg")};
const TCHAR* FacadeFiles[]={TEXT("C921-NightFacade-00-Baseline.jpg"),TEXT("C921-NightFacade-01-MarinaFallback.jpg"),TEXT("C921-NightFacade-02-RestoredBaseline.jpg")};

class FDiagnostic final : public IAutomationLatentCommand
{
 struct FMeshGuard
 {
  TWeakObjectPtr<AStaticMeshActor> Actor;
  TWeakObjectPtr<UStaticMesh> Mesh;
  FTransform Pose;
  TArray<TObjectPtr<UMaterialInterface>> Overrides;
  bool DisallowNanite=false,ForceDisableNanite=false;
 };
 FAutomationTestBase* Test;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<UGameViewportClient> Viewport;
 TWeakObjectPtr<ACameraActor> Camera;
 TWeakObjectPtr<AActor> OldView;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 FEngineShowFlags OldFlags{ESFIM_Game};
 IConsoleVariable* Nanite=nullptr;
 EConsoleVariableFlags OldNaniteFlags=ECVF_Default;
 TArray<FMeshGuard> MeshGuards;
 TArray<TSharedPtr<FJsonValue>> MeshFacts,Captures;
 TMap<FString,FString> Quality;
 FString Directory,ImageDirectory,OldFlagsText;
 double Began=FPlatformTime::Seconds(),AtWall=0,ClockSetWall=0;
 float AtGame=0,OldHour=12,ClockSetGame=0;
 int32 Stage=0,Frames=0,OldNanite=1,FinalNanite=1,OldViewMode=VMI_Lit,ShaderWaitFrames=0;
 EConsoleVariableFlags FinalNaniteFlags=ECVF_Default;
 const FName NaniteTag=TEXT("NeonCity.NightSurface921.TemporaryNanite");
 bool FacadeOnly=false,NaniteTemporary=false,Ready=false,ShaderSettled=false,ClockCaptured=false,FlagsCaptured=false,NaniteCaptured=false,OldCycle=true,Cleaned=false;
 bool FlagsRestored=true,NaniteRestored=true,ViewRestored=true,ClockRestored=true,CameraRemoved=true;

 int32 CaptureCount()const{return FacadeOnly?UE_ARRAY_COUNT(FacadeFiles):UE_ARRAY_COUNT(Files);}
 const TCHAR* CaptureFile(int32 Index)const{return FacadeOnly?FacadeFiles[Index]:Files[Index];}
 void RestoreMarina()
 {
  if(!FacadeOnly || MeshGuards.IsEmpty() || !MeshGuards[0].Actor.IsValid())return;
  auto* Component=MeshGuards[0].Actor->GetStaticMeshComponent();
  if(Component && Component->bDisallowNanite!=MeshGuards[0].DisallowNanite)
  {Component->bDisallowNanite=MeshGuards[0].DisallowNanite;Component->MarkRenderStateDirty();}
 }

 void Cleanup()
 {
  if(Cleaned)return;
  Cleaned=true;
  RestoreMarina();
  if(FlagsCaptured)
  {
   FlagsRestored=Viewport.IsValid();
   if(Viewport.IsValid())
   {Viewport->EngineShowFlags=OldFlags;Viewport->ViewModeIndex=OldViewMode;FlagsRestored=Viewport->EngineShowFlags.ToString()==OldFlagsText && Viewport->ViewModeIndex==OldViewMode;}
  }
  if(NaniteCaptured)
  {
   if(NaniteTemporary){Nanite->Unset(ECVF_SetByTemp,NaniteTag);NaniteTemporary=false;}
   FinalNanite=Nanite->GetInt();FinalNaniteFlags=Nanite->GetFlags();
   NaniteRestored=FinalNanite==OldNanite && FinalNaniteFlags==OldNaniteFlags;
  }
  if(Controller.IsValid() && OldView.IsValid())
  {Controller->SetViewTarget(OldView.Get());ViewRestored=Controller->GetViewTarget()==OldView.Get();}
  else if(Ready)ViewRestored=false;
  if(Camera.IsValid())CameraRemoved=Camera->Destroy();
  if(ClockCaptured)
  {
   ClockRestored=Clock.IsValid();
   if(Clock.IsValid())
   {Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);ClockRestored=FMath::IsNearlyEqual(Clock->GetTimeOfDay(),OldHour,.001f) && Clock->bCycleEnabled==OldCycle;}
  }
 }
 bool End(FString Error=FString())
 {
  bool BindingsUnchanged=true,QualityUnchanged=true;
  Cleanup();
  for(const auto& Guard:MeshGuards)
  {
   auto* Actor=Guard.Actor.Get();auto* Component=Actor?Actor->GetStaticMeshComponent():nullptr;
   BindingsUnchanged&=Component && Component->GetStaticMesh()==Guard.Mesh.Get()
    && Actor->GetActorTransform().Equals(Guard.Pose,0.f) && Component->OverrideMaterials==Guard.Overrides
    && Component->bDisallowNanite==Guard.DisallowNanite && Component->bForceDisableNanite==Guard.ForceDisableNanite;
  }
  for(const auto& Item:Quality)
  {const auto* Variable=IConsoleManager::Get().FindConsoleVariable(*Item.Key);QualityUnchanged&=Variable && Variable->GetString()==Item.Value;}
  if(!BindingsUnchanged)Error+=TEXT(" Protected facade mesh/override/pose changed during diagnostic.");
  if(!QualityUnchanged)Error+=TEXT(" A quality setting outside the diagnostic changed.");
  if(!FlagsRestored || !NaniteRestored || !ViewRestored || !ClockRestored || !CameraRemoved)
   Error+=TEXT(" Transient viewport/Nanite/camera/clock state failed exact restoration.");
  if(!NaniteRestored)Error+=FString::Printf(TEXT(" Nanite restore expected value %d flags %u; observed value %d flags %u."),OldNanite,uint32(OldNaniteFlags),FinalNanite,uint32(FinalNaniteFlags));
  if(Error.IsEmpty() && Captures.Num()!=CaptureCount())Error=TEXT(" Missing diagnostic variant captures.");
  if(!Error.IsEmpty())Test->AddError(Error);
  auto Receipt=MakeShared<FJsonObject>();Receipt->SetBoolField(TEXT("passed"),Error.IsEmpty());Receipt->SetStringField(TEXT("failure"),Error);
  Receipt->SetArrayField(TEXT("captures"),Captures);Receipt->SetArrayField(TEXT("protected_facades"),MeshFacts);
  Receipt->SetStringField(TEXT("eye_cm"),TEXT("X=-270.000 Y=-1250.000 Z=640.000"));Receipt->SetStringField(TEXT("aim_cm"),TEXT("X=-680.000 Y=-1180.000 Z=565.000"));Receipt->SetNumberField(TEXT("fov"),65);
  Receipt->SetNumberField(TEXT("original_nanite"),OldNanite);Receipt->SetNumberField(TEXT("original_nanite_flags"),uint32(OldNaniteFlags));
  Receipt->SetNumberField(TEXT("restored_nanite"),FinalNanite);Receipt->SetNumberField(TEXT("restored_nanite_flags"),uint32(FinalNaniteFlags));
  Receipt->SetNumberField(TEXT("original_view_mode_index"),OldViewMode);
  Receipt->SetBoolField(TEXT("viewport_showflags_restored_exactly"),FlagsRestored);Receipt->SetBoolField(TEXT("nanite_value_and_priority_restored_exactly"),NaniteRestored);
  Receipt->SetBoolField(TEXT("view_target_restored"),ViewRestored);Receipt->SetBoolField(TEXT("temporary_camera_removed"),CameraRemoved);Receipt->SetBoolField(TEXT("clock_restored"),ClockRestored);
  Receipt->SetBoolField(TEXT("facade_bindings_overrides_poses_unchanged"),BindingsUnchanged);Receipt->SetBoolField(TEXT("other_quality_unchanged"),QualityUnchanged);
  Receipt->SetNumberField(TEXT("player_pose_writes"),0);Receipt->SetNumberField(TEXT("asset_or_map_writes"),0);Receipt->SetNumberField(TEXT("shader_wait_frames"),ShaderWaitFrames);
  Receipt->SetBoolField(TEXT("certifies_visual_acceptance"),false);Receipt->SetBoolField(TEXT("performance_sample"),false);
  Receipt->SetBoolField(TEXT("single_facade_component_comparison"),FacadeOnly);Receipt->SetNumberField(TEXT("global_cvar_writes"),FacadeOnly?0:2);
  Receipt->SetStringField(TEXT("scope"),TEXT("Exact C920View15 interior camera at 22h. Four independent diagnostic variants restore the original viewport flags, view mode and Nanite value between captures: baseline, Lighting=false with VMI_Unlit, viewport DynamicShadows=false, and r.Nanite=0 using the already-authored full-triangle fallback meshes. VMI_Lit forces rendered Lighting=true, so the lighting-off variant requires VMI_Unlit; its normal engine override also disables light functions, dynamic shadows, fog and atmosphere. Effective view-family flags are recorded. Showflags affect the entire game viewport; r.Nanite affects all Nanite geometry during that variant. Each capture follows at least 3.25 wall seconds, 3 game seconds and 24 frames with no shader jobs pending. No materials, mesh settings, assets, maps, player pose or persistent render policy change. Captures identify the black-grid cause; successful capture is neither a production fix nor visual acceptance or performance evidence."));
  if(FacadeOnly)Receipt->SetStringField(TEXT("scope"),TEXT("Exact C920View15 interior camera at 22h, A/B/A single-component comparison: untouched baseline after at least 20 wall/game seconds, only Marina StaticMeshActor_1662 bDisallowNanite=true with MarkRenderStateDirty, then its original flag restored before the third capture. Marina's existing built fallback contains the exact 198502 source triangles. Actual scene-proxy Nanite state and wall/game age since the clock changed to 22h are recorded. The longer untouched baseline distinguishes clock/GI settling from representation changes; it is diagnostic only. Courtyard/Gallery flags, all mesh bindings/material overrides/poses, viewport showflags/view mode, all global CVars and quality settings stay unchanged. Each image follows at least 3.25 wall seconds, 3 game seconds and 24 frames after shader jobs reach zero. No source, asset, map, player pose or persistent setting writes. Camera and clock restore on every exit. This is causal diagnosis, not visual acceptance or performance evidence."));
  auto Policy=MakeShared<FJsonObject>();for(const auto& Item:Quality)Policy->SetStringField(Item.Key,Item.Value);Receipt->SetObjectField(TEXT("unchanged_quality"),Policy);
  FString Text;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text));
  if(!Directory.IsEmpty())
  {const FString File=Directory/TEXT("receipt.json");if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Text,*File))Test->AddError(TEXT("Night surface receipt exists or cannot be written; previous evidence is immutable"));}
  return true;
 }
 void RestartWait(){AtWall=FPlatformTime::Seconds();AtGame=World->GetTimeSeconds();Frames=0;}
 bool ApplyVariant()
 {
  Viewport->EngineShowFlags=OldFlags;
  Viewport->ViewModeIndex=!FacadeOnly && Stage==1?VMI_Unlit:OldViewMode;
  if(FacadeOnly)
  {
   auto* Component=MeshGuards[0].Actor->GetStaticMeshComponent();
   const bool DesiredDisallow=Stage==1?true:MeshGuards[0].DisallowNanite;
   if(Component->bDisallowNanite!=DesiredDisallow){Component->bDisallowNanite=DesiredDisallow;Component->MarkRenderStateDirty();}
  }
  else
  {
   if(Stage==3){Nanite->Set(0,ECVF_SetByTemp,NaniteTag);NaniteTemporary=true;}
   else if(NaniteTemporary){Nanite->Unset(ECVF_SetByTemp,NaniteTag);NaniteTemporary=false;}
   if(Stage==1)Viewport->EngineShowFlags.SetLighting(false);
   if(Stage==2)Viewport->EngineShowFlags.SetDynamicShadows(false);
  }
  ShaderSettled=false;RestartWait();
  return Nanite->GetInt()==(!FacadeOnly && Stage==3?0:OldNanite);
 }
public:
 explicit FDiagnostic(FAutomationTestBase* InTest,bool InFacadeOnly=false):Test(InTest),FacadeOnly(InFacadeOnly){}
 virtual ~FDiagnostic(){Cleanup();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Began>180)return End(TEXT("Night surface diagnostic timed out, including shader settling"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  auto* PC=W->GetFirstPlayerController();auto* Client=W->GetGameViewport();if(!PC || !Client || !Client->Viewport)return false;
  if(!Ready)
  {
   if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling()){++ShaderWaitFrames;return false;}
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);Run=FPaths::MakeValidFileName(Run);
   if(Run.IsEmpty())return End(TEXT("Night surface diagnostic requires a unique evidence label"));
   const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/(FacadeOnly?TEXT("NightFacadeFallbackComparison921"):TEXT("NightSurfaceDiagnostic921"));
   const FString Images=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("UI");
   if(IFileManager::Get().DirectoryExists(*Folder))return End(TEXT("Never reuse a night surface evidence directory"));
   for(int32 Index=0;Index<CaptureCount();++Index)if(IFileManager::Get().FileExists(*(Images/CaptureFile(Index))))return End(TEXT("Never overwrite an existing night surface image"));
   Nanite=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
   const auto* ProxyMode=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.ProxyRenderMode"));
   if(!Nanite || Nanite->GetInt()!=1 || !ProxyMode || ProxyMode->GetInt()!=0 || Client->ViewModeIndex!=VMI_Lit || !Client->EngineShowFlags.Lighting || !Client->EngineShowFlags.DynamicShadows)
    return End(TEXT("Normal lit/shadowed Nanite baseline and visible non-Nanite fallback policy required"));
   if(!FacadeOnly && (Nanite->GetFlags()&ECVF_SetByMask)>=ECVF_SetByTemp)return End(TEXT("Global Nanite diagnostic requires priority below the tagged temporary layer"));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)if(It->GetLevel()==W->PersistentLevel)
   {if(Clock.IsValid())return End(TEXT("Ambiguous persistent-level clock"));Clock=*It;}
   const TCHAR* ActorNames[]={TEXT("StaticMeshActor_1662"),TEXT("StaticMeshActor_1664"),TEXT("StaticMeshActor_1666")};
   const TCHAR* MeshNames[]={TEXT("SM_C921_Marina24_ContinuousPlaster1"),TEXT("SM_C921_Courtyard21_ContinuousPlaster1"),TEXT("SM_C921_Gallery27_ContinuousPlaster1")};
   // Exact actual-FBX readback counts in FacadeWallSeams/Revision1/wall-seams.json.
   const int32 SourceTriangles[]={198502,176718,134094};
   for(int32 Index=0;Index<UE_ARRAY_COUNT(ActorNames);++Index)
   {
    AStaticMeshActor* Found=nullptr;
    for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetLevel()==W->PersistentLevel && It->GetFName()==ActorNames[Index])
    {if(Found)return End(TEXT("Ambiguous exact facade actor"));Found=*It;}
    auto* Component=Found?Found->GetStaticMeshComponent():nullptr;auto* Mesh=Component?Component->GetStaticMesh().Get():nullptr;
    const FString Package=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC921/")+FString(MeshNames[Index]);
    if(!Mesh || Mesh->GetPathName()!=Package+TEXT(".")+MeshNames[Index])return End(TEXT("Expected current continuous-plaster facade binding missing"));
    const auto& Settings=Mesh->GetNaniteSettings();
    if(!Settings.bEnabled || Settings.FallbackTarget!=ENaniteFallbackTarget::RelativeError || Settings.FallbackRelativeError!=0.f || Settings.FallbackPercentTriangles!=1.f)
     return End(TEXT("Exact authored full-triangle Nanite fallback policy missing; do not rebuild assets in a diagnostic"));
    const auto* RenderData=Mesh->GetRenderData();
    if(!RenderData || !RenderData->IsInitialized() || !RenderData->HasValidNaniteData() || RenderData->LODResources.Num()==0)
     return End(TEXT("Actual built Nanite/full-triangle fallback render data is unavailable"));
    const int32 FallbackTriangles=RenderData->LODResources[0].GetNumTriangles();
    if(FallbackTriangles!=SourceTriangles[Index])return End(FString::Printf(TEXT("Exact native fallback triangle proof failed for %s: source %d fallback %d"),MeshNames[Index],SourceTriangles[Index],FallbackTriangles));
    MeshGuards.Add({Found,Mesh,Found->GetActorTransform(),Component->OverrideMaterials,bool(Component->bDisallowNanite),bool(Component->bForceDisableNanite)});
    if(FacadeOnly && Index==0 && (Component->bDisallowNanite || Component->bForceDisableNanite || !Component->GetSceneProxy() || !Component->GetSceneProxy()->IsNaniteMesh()))return End(TEXT("Marina comparison requires the actual Nanite baseline scene proxy"));
    auto Fact=MakeShared<FJsonObject>();Fact->SetStringField(TEXT("actor"),Found->GetPathName());Fact->SetStringField(TEXT("mesh"),Mesh->GetPathName());
    Fact->SetNumberField(TEXT("fallback_relative_error"),Settings.FallbackRelativeError);Fact->SetNumberField(TEXT("fallback_percent_triangles"),Settings.FallbackPercentTriangles);
    Fact->SetNumberField(TEXT("source_fbx_triangles"),SourceTriangles[Index]);Fact->SetNumberField(TEXT("actual_built_fallback_triangles"),FallbackTriangles);
    Fact->SetBoolField(TEXT("original_disallow_nanite"),Component->bDisallowNanite);Fact->SetBoolField(TEXT("original_force_disable_nanite"),Component->bForceDisableNanite);
    MeshFacts.Add(MakeShared<FJsonValueObject>(Fact));
   }
   if(!Clock.IsValid() || !PC->GetViewTarget())return End(TEXT("Original clock/view target missing"));
   for(const TCHAR* Name:{TEXT("r.AntiAliasingMethod"),TEXT("r.ScreenPercentage"),TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("r.Nanite.ProxyRenderMode")})
    if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(Name))Quality.Add(Name,Variable->GetString());
   if(FacadeOnly)Quality.Add(TEXT("r.Nanite"),Nanite->GetString());
   Directory=Folder;ImageDirectory=Images;IFileManager::Get().MakeDirectory(*Directory,true);
   World=W;Controller=PC;Viewport=Client;OldView=PC->GetViewTarget();
   OldFlags=Client->EngineShowFlags;OldFlagsText=OldFlags.ToString();OldViewMode=Client->ViewModeIndex;FlagsCaptured=true;
   OldNanite=Nanite->GetInt();OldNaniteFlags=Nanite->GetFlags();NaniteCaptured=true;
   OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;ClockCaptured=true;
   Camera=W->SpawnActor<ACameraActor>();if(!Camera.IsValid())return End(TEXT("Night surface comparison camera could not spawn"));
   const FVector Eye(-270,-1250,640),Aim(-680,-1180,565);Camera->GetCameraComponent()->FieldOfView=65;
   Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(22);PC->SetViewTarget(Camera.Get());
   ClockSetWall=FPlatformTime::Seconds();ClockSetGame=W->GetTimeSeconds();
   Ready=true;if(!ApplyVariant())return End(TEXT("Night surface baseline policy could not apply"));return false;
  }
  if(World.Get()!=W || Controller.Get()!=PC || Viewport.Get()!=Client || !Camera.IsValid() || !Clock.IsValid())return End(TEXT("Diagnostic world/view fixture changed"));
  const bool Compiling=GShaderCompilingManager && (GShaderCompilingManager->IsCompiling() || GShaderCompilingManager->GetNumRemainingJobs()!=0);
  if(Compiling){++ShaderWaitFrames;ShaderSettled=false;return false;}
  if(!ShaderSettled){ShaderSettled=true;RestartWait();return false;}
  ++Frames;const double WallAge=FPlatformTime::Seconds()-AtWall;const float GameAge=W->GetTimeSeconds()-AtGame;
  if(WallAge<3.25 || GameAge<3.f || Frames<24)return false;
  if(FacadeOnly && Stage==0 && (WallAge<20. || GameAge<20.f))return false;
  FEngineShowFlags Expected=OldFlags;if(!FacadeOnly && Stage==1)Expected.SetLighting(false);if(!FacadeOnly && Stage==2)Expected.SetDynamicShadows(false);
  if(Client->EngineShowFlags.ToString()!=Expected.ToString() || Client->ViewModeIndex!=(!FacadeOnly && Stage==1?VMI_Unlit:OldViewMode) || Nanite->GetInt()!=(!FacadeOnly && Stage==3?0:OldNanite))return End(TEXT("Diagnostic variant render state changed before capture"));
  FEngineShowFlags Effective=Client->EngineShowFlags;EngineShowFlagOverride(ESFIM_Game,EViewModeIndex(Client->ViewModeIndex),Effective,false);
  if(Effective.Lighting!=(FacadeOnly || Stage!=1) || (!FacadeOnly && Stage==2 && Effective.DynamicShadows))return End(TEXT("Effective rendered view-family flags did not apply the requested diagnostic"));
  TArray<TSharedPtr<FJsonValue>> FacadeStates;
  for(int32 Index=0;Index<MeshGuards.Num();++Index)
  {
   const auto& Guard=MeshGuards[Index];auto* Component=Guard.Actor.IsValid()?Guard.Actor->GetStaticMeshComponent():nullptr;
   if(!Component || !Component->GetSceneProxy())return End(TEXT("Protected facade scene proxy missing during comparison"));
   const bool ExpectedDisallow=FacadeOnly && Stage==1 && Index==0?true:Guard.DisallowNanite;
   if(Component->bDisallowNanite!=ExpectedDisallow || Component->bForceDisableNanite!=Guard.ForceDisableNanite)return End(TEXT("Unexpected facade Nanite component override during comparison"));
   if(FacadeOnly && Index==0 && Component->GetSceneProxy()->IsNaniteMesh()!=(Stage!=1))return End(TEXT("Marina actual scene proxy did not switch to the expected representation"));
   auto State=MakeShared<FJsonObject>();State->SetStringField(TEXT("actor"),Guard.Actor->GetPathName());State->SetBoolField(TEXT("disallow_nanite"),Component->bDisallowNanite);
   State->SetBoolField(TEXT("force_disable_nanite"),Component->bForceDisableNanite);State->SetBoolField(TEXT("actual_scene_proxy_is_nanite"),Component->GetSceneProxy()->IsNaniteMesh());FacadeStates.Add(MakeShared<FJsonValueObject>(State));
  }
  if(PC->GetViewTarget()!=Camera.Get() || !FMath::IsNearlyEqual(Clock->GetTimeOfDay(),22.f,.001f) || Clock->bCycleEnabled)return End(TEXT("Exact camera/22h fixture changed before capture"));
  const FString File=CaptureFile(Stage);if(IFileManager::Get().FileExists(*(ImageDirectory/File)))return End(TEXT("Diagnostic image already exists"));
  if(!FNeonCityVisualEvidence::CaptureRenderedJPEG(W,File))return End(TEXT("Rendered night surface capture failed: ")+File);
  if(GShaderCompilingManager && (GShaderCompilingManager->IsCompiling() || GShaderCompilingManager->GetNumRemainingJobs()!=0))return End(TEXT("Shader work began during the diagnostic screenshot; capture cannot certify settled state"));
  auto Capture=MakeShared<FJsonObject>();Capture->SetStringField(TEXT("file"),File);Capture->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());
  Capture->SetBoolField(TEXT("viewport_lighting"),Client->EngineShowFlags.Lighting);Capture->SetBoolField(TEXT("viewport_dynamic_shadows"),Client->EngineShowFlags.DynamicShadows);Capture->SetNumberField(TEXT("r.Nanite"),Nanite->GetInt());
  Capture->SetNumberField(TEXT("r.Nanite_flags"),uint32(Nanite->GetFlags()));Capture->SetArrayField(TEXT("facade_component_states"),FacadeStates);
  Capture->SetNumberField(TEXT("view_mode_index"),Client->ViewModeIndex);Capture->SetBoolField(TEXT("effective_render_family_lighting"),Effective.Lighting);
  Capture->SetBoolField(TEXT("effective_render_family_dynamic_shadows"),Effective.DynamicShadows);Capture->SetBoolField(TEXT("effective_render_family_fog"),Effective.Fog);Capture->SetBoolField(TEXT("effective_render_family_atmosphere"),Effective.Atmosphere);
  Capture->SetNumberField(TEXT("stable_wall_seconds"),WallAge);Capture->SetNumberField(TEXT("stable_game_seconds"),GameAge);Capture->SetNumberField(TEXT("stable_frames"),Frames);
  Capture->SetNumberField(TEXT("wall_seconds_since_clock_set_22"),FPlatformTime::Seconds()-ClockSetWall);Capture->SetNumberField(TEXT("game_seconds_since_clock_set_22"),W->GetTimeSeconds()-ClockSetGame);
  Capture->SetBoolField(TEXT("shader_compiling"),false);Capture->SetNumberField(TEXT("remaining_shader_jobs"),GShaderCompilingManager?GShaderCompilingManager->GetNumRemainingJobs():0);
  Captures.Add(MakeShared<FJsonValueObject>(Capture));
  if(++Stage==CaptureCount())return End();
  if(!ApplyVariant())return End(TEXT("Night surface variant Nanite switch could not apply"));return false;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalNightSurfaceDiagnostic921,"NeonCity.Phase205c.MainlandC921.Standalone.NightSurfaceDiagnostic",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalNightSurfaceDiagnostic921::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("Night surface diagnostic requires rendered stills and disabled automatic persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(CoastalSurface921::FDiagnostic(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalNightFacadeFallbackComparison921,"NeonCity.Phase205c.MainlandC921.Standalone.NightFacadeFallbackComparison",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalNightFacadeFallbackComparison921::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("Night facade comparison requires rendered stills and disabled automatic persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(CoastalSurface921::FDiagnostic(this,true));return true;
}
#endif
