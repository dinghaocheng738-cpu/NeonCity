#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "SceneView.h"
#include "ShaderCompiler.h"
#include "RHI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HeadlandLighting923
{
// Exact comparison20 geometry; only this transient camera's exposure bias and
// the diagnostic viewport Fog flag vary. Production settings remain intact.
const FVector Eye(15000,-6500,6500),Aim(18700,4600,1200);
const TCHAR* Files[]={TEXT("C923-Lighting-00-Baseline-Lit.jpg"),TEXT("C923-Lighting-01-CameraBias0.jpg"),TEXT("C923-Lighting-02-CameraBias0-NoFog.jpg")};
class FDiagnosis final : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<UGameViewportClient> Viewport;
 TWeakObjectPtr<AActor> OldView;
 TWeakObjectPtr<ACameraActor> Camera;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 TWeakObjectPtr<UStaticMeshComponent> Terrain;
 TObjectPtr<UStaticMesh> OldMesh=nullptr;
 TArray<TObjectPtr<UMaterialInterface>> OldOverrides;
 FEngineShowFlags OldFlags{ESFIM_Game};
 FPostProcessSettings OldCameraSettings;
 FString Directory,OldFlagsText;
 FName OldBufferMode=NAME_None;
 int32 OldMode=VMI_Lit,Stage=0,Frames=0;
 float OldHour=12,OldCameraWeight=1,AtGame=0,BaselineBias=0;
 double Started=FPlatformTime::Seconds(),AtWall=0;
 bool Ready=false,SavedState=false,SavedCamera=false,OldCycle=true,Cleaned=false,CameraSettingsRestored=true;
 TArray<TSharedPtr<FJsonValue>> Shots;

 void Cleanup()
 {
  if(Cleaned)return;Cleaned=true;
  if(SavedCamera)
  {
   CameraSettingsRestored=false;
   if(Camera.IsValid())
   {
    auto* C=Camera->GetCameraComponent();C->PostProcessSettings=OldCameraSettings;C->PostProcessBlendWeight=OldCameraWeight;
    CameraSettingsRestored=C->PostProcessSettings.bOverride_AutoExposureBias==OldCameraSettings.bOverride_AutoExposureBias
     && FMath::IsNearlyEqual(C->PostProcessSettings.AutoExposureBias,OldCameraSettings.AutoExposureBias,.0001f)
     && FMath::IsNearlyEqual(C->PostProcessBlendWeight,OldCameraWeight,.0001f);
   }
  }
  if(SavedState && Viewport.IsValid())
  {Viewport->EngineShowFlags=OldFlags;Viewport->ViewModeIndex=OldMode;Viewport->SetCurrentBufferVisualizationMode(OldBufferMode);}
  if(SavedState && Controller.IsValid() && OldView.IsValid())Controller->SetViewTarget(OldView.Get());
  if(Camera.IsValid())Camera->Destroy();
  if(SavedState && Clock.IsValid()){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
 }

 bool End(FString Error=FString())
 {
  Cleanup();bool Restored=CameraSettingsRestored;
  if(SavedState)
  {
   const bool ViewportRestored=Viewport.IsValid() && Viewport->EngineShowFlags.ToString()==OldFlagsText
    && Viewport->ViewModeIndex==OldMode && Viewport->GetCurrentBufferVisualizationMode()==OldBufferMode;
   const bool ClockRestored=Clock.IsValid() && FMath::IsNearlyEqual(Clock->GetTimeOfDay(),OldHour,.001f) && Clock->bCycleEnabled==OldCycle;
   const bool ViewRestored=Controller.IsValid() && Controller->GetViewTarget()==OldView.Get();
   const bool BindingUnchanged=Terrain.IsValid() && Terrain->GetStaticMesh()==OldMesh && Terrain->OverrideMaterials==OldOverrides;
   Restored&=ViewportRestored && ClockRestored && ViewRestored;
   if(!ViewportRestored)Error+=TEXT(" Diagnostic viewport failed exact restoration.");
   if(!ClockRestored)Error+=TEXT(" Clock failed restoration.");
   if(!ViewRestored)Error+=TEXT(" Original camera target failed restoration.");
   if(!BindingUnchanged)Error+=TEXT(" Production terrain/material binding changed.");
  }
  if(!CameraSettingsRestored)Error+=TEXT(" Transient camera postprocess settings failed restoration.");
  if(Error.IsEmpty() && Shots.Num()!=UE_ARRAY_COUNT(Files))Error=TEXT(" Three actual causal lighting captures are required.");
  if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);
  J->SetBoolField(TEXT("restored"),Restored);J->SetBoolField(TEXT("cleanup_attempted"),Cleaned);J->SetArrayField(TEXT("captures"),Shots);
  J->SetNumberField(TEXT("player_pose_writes"),0);J->SetNumberField(TEXT("player_input_writes"),0);
  J->SetNumberField(TEXT("asset_or_map_writes"),0);J->SetNumberField(TEXT("production_light_or_postprocess_writes"),0);
  J->SetNumberField(TEXT("console_cvar_writes"),0);J->SetBoolField(TEXT("visual_acceptance"),false);J->SetBoolField(TEXT("performance_evidence"),false);
  J->SetStringField(TEXT("scope"),TEXT("Exact C923 comparison20 camera/FOV65 at Day12. Three20-second-settled normal Lit variants: production baseline; only transient camera AutoExposureBias0; same camera bias0 plus viewport Fog=false. Effective scene-view exposure and fog flags are verified, not inferred from requested values. Camera postprocess, original view target, viewport flags/mode/buffer cache and clock restore on all exits. No production material, light, exposure, fog actor, map, player pose/input, CVar or Nanite changes. Fog=false isolates the renderer Fog show flag; sky atmosphere remains unchanged. These diagnostic images are not production visual acceptance or performance evidence."));
  FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));
  if(!Directory.IsEmpty())
  {const FString File=Directory/TEXT("receipt.json");if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Json,*File))Test->AddError(TEXT("Immutable lighting diagnosis receipt cannot be written"));}
  return true;
 }

 void Apply()
 {
  auto* C=Camera->GetCameraComponent();C->PostProcessSettings=OldCameraSettings;C->PostProcessBlendWeight=OldCameraWeight;
  if(Stage>0){C->PostProcessSettings.bOverride_AutoExposureBias=true;C->PostProcessSettings.AutoExposureBias=0;C->PostProcessBlendWeight=1;}
  Viewport->EngineShowFlags=OldFlags;Viewport->ViewModeIndex=VMI_Lit;
  if(Stage==2)Viewport->EngineShowFlags.SetFog(false);
  AtWall=FPlatformTime::Seconds();AtGame=World->GetTimeSeconds();Frames=0;
 }
public:
 explicit FDiagnosis(FAutomationTestBase* InTest):Test(InTest){}
 virtual ~FDiagnosis(){Cleanup();}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>180)return End(TEXT("Headland lighting diagnosis timeout"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  auto* PC=W->GetFirstPlayerController();auto* Client=W->GetGameViewport();if(!PC || !Client || !Client->Viewport)return false;
  if(!Ready)
  {
   if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling())return false;
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);if(Run.IsEmpty())return End(TEXT("Unique lighting diagnosis evidence label required"));
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("HeadlandLightingDiagnosis923");
   IFileManager::Get().MakeDirectory(*Directory,true);if(IFileManager::Get().FileExists(*(Directory/TEXT("receipt.json"))))return End(TEXT("Prior lighting diagnosis evidence is immutable"));
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel){Clock=*I;break;}
   for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->GetFName()==TEXT("StaticMeshActor_183")){Terrain=I->GetStaticMeshComponent();break;}
   if(!Clock.IsValid() || !Terrain.IsValid())return End(TEXT("Actual mainland headland and clock are required"));
   World=W;Controller=PC;Viewport=Client;OldView=PC->GetViewTarget();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;
   OldFlags=Client->EngineShowFlags;OldFlagsText=OldFlags.ToString();OldMode=Client->ViewModeIndex;OldBufferMode=Client->GetCurrentBufferVisualizationMode();
   OldMesh=Terrain->GetStaticMesh();OldOverrides=Terrain->OverrideMaterials;SavedState=true;
   if(OldMode!=VMI_Lit || !OldFlags.Lighting || OldFlags.VisualizeBuffer)return End(TEXT("Lighting diagnosis requires a normal fresh Lit viewport"));
   if(!OldFlags.Fog)return End(TEXT("Normal viewport already disables Fog; a fog contribution A/B cannot be claimed"));
   FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;Camera=W->SpawnActor<ACameraActor>(Spawn);
   if(!Camera.IsValid())return End(TEXT("Transient lighting diagnosis camera spawn failed"));
   auto* C=Camera->GetCameraComponent();OldCameraSettings=C->PostProcessSettings;OldCameraWeight=C->PostProcessBlendWeight;SavedCamera=true;
   if(OldCameraSettings.bOverride_AutoExposureBias)return End(TEXT("Default comparison camera unexpectedly overrides exposure bias"));
   C->FieldOfView=65;Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
   Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);PC->SetViewTarget(Camera.Get());Ready=true;Apply();return false;
  }
  if(W!=World.Get() || PC!=Controller.Get() || Client!=Viewport.Get() || !Camera.IsValid() || !Clock.IsValid())return End(TEXT("Diagnosis world/view/camera changed unexpectedly"));
  ++Frames;
  if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling()){AtWall=FPlatformTime::Seconds();AtGame=W->GetTimeSeconds();Frames=0;return false;}
  if(FPlatformTime::Seconds()-AtWall<20 || W->GetTimeSeconds()-AtGame<20 || Frames<48)return false;
  if(PC->GetViewTarget()!=Camera.Get() || !FMath::IsNearlyEqual(Clock->GetTimeOfDay(),12.f,.001f) || Clock->bCycleEnabled)return End(TEXT("Fixed camera/daylight fixture drifted"));
  if(Client->ViewModeIndex!=VMI_Lit)return End(TEXT("A causal lighting variant left normal Lit mode"));
  FEngineShowFlags Effective=Client->EngineShowFlags;EngineShowFlagOverride(ESFIM_Game,VMI_Lit,Effective,false);
  if(!Effective.Lighting || Effective.VisualizeBuffer)return End(TEXT("Effective scene view is not normal Lit"));
  if(Stage<2 && !Effective.Fog)return End(TEXT("Effective baseline Fog is disabled; this build cannot provide the requested fog A/B"));
  if(Stage==2 && Effective.Fog)return End(TEXT("Fog=false did not affect the effective renderer flags in this build"));
  auto* Local=PC->GetLocalPlayer();if(!Local)return End(TEXT("Actual local player missing"));
  FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client->Viewport,W->Scene,Effective).SetRealtimeUpdate(true));Family.ViewMode=VMI_Lit;
  FVector Location;FRotator Rotation;auto* View=Local->CalcSceneView(&Family,Location,Rotation,Client->Viewport);
  if(!View)return End(TEXT("Actual effective scene-view exposure could not be read"));
  if(!Location.Equals(Eye,.1f))return End(TEXT("Effective scene-view eye differs from comparison20"));
  const auto& P=View->FinalPostProcessSettings;
  if(Stage>0 && !FMath::IsNearlyZero(P.AutoExposureBias,.001f))return End(TEXT("Camera bias0 override did not become the effective scene-view exposure bias"));
  if(Stage==0)BaselineBias=P.AutoExposureBias;
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("file"),Files[Stage]);
  J->SetNumberField(TEXT("wall_settle_seconds"),FPlatformTime::Seconds()-AtWall);J->SetNumberField(TEXT("game_settle_seconds"),W->GetTimeSeconds()-AtGame);
  J->SetNumberField(TEXT("settled_frames"),Frames);J->SetStringField(TEXT("actual_eye_cm"),Location.ToString());J->SetNumberField(TEXT("fov"),Camera->GetCameraComponent()->FieldOfView);
  J->SetBoolField(TEXT("camera_bias_override"),Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias);
  J->SetNumberField(TEXT("camera_postprocess_blend_weight"),Camera->GetCameraComponent()->PostProcessBlendWeight);
  J->SetNumberField(TEXT("effective_exposure_bias"),P.AutoExposureBias);J->SetNumberField(TEXT("baseline_effective_bias"),BaselineBias);
  J->SetNumberField(TEXT("bias_difference_from_baseline_ev"),P.AutoExposureBias-BaselineBias);
  J->SetNumberField(TEXT("eye_adaptation_exposure"),View->GetLastEyeAdaptationExposure());J->SetNumberField(TEXT("average_scene_luminance"),View->GetLastAverageSceneLuminance());
  J->SetNumberField(TEXT("min_brightness_or_ev100"),P.AutoExposureMinBrightness);J->SetNumberField(TEXT("max_brightness_or_ev100"),P.AutoExposureMaxBrightness);
  J->SetNumberField(TEXT("exposure_method"),int32(P.AutoExposureMethod));J->SetNumberField(TEXT("film_slope"),P.FilmSlope);
  J->SetNumberField(TEXT("local_exposure_highlight_contrast"),P.LocalExposureHighlightContrastScale);
  J->SetNumberField(TEXT("local_exposure_shadow_contrast"),P.LocalExposureShadowContrastScale);J->SetNumberField(TEXT("local_exposure_detail_strength"),P.LocalExposureDetailStrength);
  J->SetBoolField(TEXT("effective_lighting"),Family.EngineShowFlags.Lighting);J->SetBoolField(TEXT("effective_fog"),Family.EngineShowFlags.Fog);
  J->SetBoolField(TEXT("effective_volumetric_fog_flag"),Family.EngineShowFlags.VolumetricFog);J->SetBoolField(TEXT("effective_atmosphere_flag"),Family.EngineShowFlags.Atmosphere);
  J->SetStringField(TEXT("viewport_showflags"),Client->EngineShowFlags.ToString());J->SetNumberField(TEXT("view_mode"),Client->ViewModeIndex);
  for(const TCHAR* Name:{TEXT("r.Fog"),TEXT("r.VolumetricFog"),TEXT("ShowFlag.Fog"),TEXT("r.DefaultFeature.AutoExposure.Bias"),TEXT("r.EyeAdaptationQuality"),TEXT("r.UsePreExposure")})
   if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(Name))J->SetStringField(Name,Variable->GetString());
  if(!FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Files[Stage]))return End(TEXT("Actual causal lighting image readback failed"));
  Shots.Add(MakeShared<FJsonValueObject>(J));if(++Stage==UE_ARRAY_COUNT(Files))return End();Apply();return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeadlandLightingDiagnosis923,"NeonCity.Phase205c.MainlandC923.Standalone.HeadlandLightingDiagnosis",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHeadlandLightingDiagnosis923::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || GUsingNullRHI)
 {AddError(TEXT("Lighting diagnosis requires actual rendered stills and disabled automatic persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(HeadlandLighting923::FDiagnosis(this));return true;
}
#endif
