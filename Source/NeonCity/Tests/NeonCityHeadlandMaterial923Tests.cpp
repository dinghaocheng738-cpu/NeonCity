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
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HeadlandMaterial923
{
// Same geometry/FOV as the already published comparison20. Only diagnostic
// viewport modes change; these images never certify production presentation.
const FVector Eye(15000,-6500,6500),Aim(18700,4600,1200);
const TCHAR* Files[]={TEXT("C923-Ground-00-Lit.jpg"),TEXT("C923-Ground-01-BaseColor.jpg"),TEXT("C923-Ground-02-Unlit.jpg")};
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
 FString OldFlagsText,OldBuffer,Directory;
 FName OldBufferMode=NAME_None;
 IConsoleVariable* Buffer=nullptr;
 EConsoleVariableFlags OldBufferFlags=ECVF_Default;
 int32 OldMode=VMI_Lit,Stage=0,Frames=0;
 float OldHour=12,AtGame=0;
 double Started=FPlatformTime::Seconds(),AtWall=0;
 bool Ready=false,SavedState=false,OldCycle=true,TemporaryBuffer=false,Cleaned=false;
 const FName BufferTag=TEXT("NeonCity.Headland923.MaterialDiagnosis");
 TArray<TSharedPtr<FJsonValue>> Shots;
 void Cleanup()
 {
  if(Cleaned)return;Cleaned=true;
  if(TemporaryBuffer){Buffer->Unset(ECVF_SetByTemp,BufferTag);TemporaryBuffer=false;}
  if(SavedState && Viewport.IsValid()){Viewport->EngineShowFlags=OldFlags;Viewport->ViewModeIndex=OldMode;Viewport->SetCurrentBufferVisualizationMode(OldBufferMode);}
  if(SavedState && Controller.IsValid() && OldView.IsValid())Controller->SetViewTarget(OldView.Get());
  if(Camera.IsValid())Camera->Destroy();
  if(SavedState && Clock.IsValid()){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
 }
 bool End(FString Error=FString())
 {
  Cleanup();
  if(SavedState)
  {
   if(!Viewport.IsValid() || Viewport->EngineShowFlags.ToString()!=OldFlagsText || Viewport->ViewModeIndex!=OldMode || Viewport->GetCurrentBufferVisualizationMode()!=OldBufferMode)
    Error+=TEXT(" Diagnostic viewport failed exact restoration.");
   if(!Buffer || Buffer->GetString()!=OldBuffer || Buffer->GetFlags()!=OldBufferFlags)
    Error+=TEXT(" Buffer target value/priority failed exact restoration.");
   if(!Terrain.IsValid() || Terrain->GetStaticMesh()!=OldMesh || Terrain->OverrideMaterials!=OldOverrides)
    Error+=TEXT(" Terrain mesh/material binding changed.");
   if(!Clock.IsValid() || !FMath::IsNearlyEqual(Clock->GetTimeOfDay(),OldHour,.001f) || Clock->bCycleEnabled!=OldCycle)
    Error+=TEXT(" Clock failed restoration.");
   if(!Controller.IsValid() || Controller->GetViewTarget()!=OldView.Get())Error+=TEXT(" Camera view failed restoration.");
  }
  if(Error.IsEmpty() && Shots.Num()!=UE_ARRAY_COUNT(Files))Error=TEXT(" Missing material diagnostic captures.");
  if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);
  J->SetArrayField(TEXT("captures"),Shots);J->SetBoolField(TEXT("restored"),Cleaned);
  J->SetNumberField(TEXT("player_pose_writes"),0);J->SetNumberField(TEXT("asset_or_map_writes"),0);
  J->SetBoolField(TEXT("visual_acceptance"),false);J->SetBoolField(TEXT("performance_evidence"),false);
  J->SetStringField(TEXT("scope"),TEXT("C923 comparison20 camera/FOV65 at Day12, three causal variants: normal Lit; GBuffer BaseColor visualization; Unlit. Temporary buffer-target CVar priority, viewport flags/mode, camera and clock restore on every exit. Effective scene-view eye adaptation/exposure/grade settings are read for each variant; no production material, light, exposure, Nanite or player/input writes. Diagnostic captures identify pale ground/foliage causes and do not certify actual visual quality or performance."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
  if(!Directory.IsEmpty())
  {const FString File=Directory/TEXT("receipt.json");if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Text,*File))Test->AddError(TEXT("Immutable diagnosis receipt cannot be written"));}
  return true;
 }
 void Apply()
 {
  if(TemporaryBuffer){Buffer->Unset(ECVF_SetByTemp,BufferTag);TemporaryBuffer=false;}
  Viewport->EngineShowFlags=OldFlags;
  Viewport->ViewModeIndex=Stage==1?VMI_VisualizeBuffer:Stage==2?VMI_Unlit:OldMode;
  if(Stage==1){Buffer->Set(TEXT("BaseColor"),ECVF_SetByTemp,BufferTag);TemporaryBuffer=true;Viewport->EngineShowFlags.SetVisualizeBuffer(true);}
  if(Stage==2)Viewport->EngineShowFlags.SetLighting(false);
  AtWall=FPlatformTime::Seconds();AtGame=World->GetTimeSeconds();Frames=0;
 }
public:
 explicit FDiagnosis(FAutomationTestBase* InTest):Test(InTest){}
 virtual ~FDiagnosis(){Cleanup();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Started>180)return End(TEXT("Headland material diagnosis timeout"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  auto* PC=W->GetFirstPlayerController();auto* Client=W->GetGameViewport();if(!PC || !Client || !Client->Viewport)return false;
  if(!Ready)
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return End(TEXT("Actual diagnostic still readback required"));
   if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling())return false;
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);if(Run.IsEmpty())return End(TEXT("Unique diagnosis evidence label required"));
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("HeadlandMaterialDiagnosis923");
   IFileManager::Get().MakeDirectory(*Directory,true);if(IFileManager::Get().FileExists(*(Directory/TEXT("receipt.json"))))return End(TEXT("Prior diagnosis is immutable"));
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel){Clock=*I;break;}
   for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->GetFName()==TEXT("StaticMeshActor_183")){Terrain=I->GetStaticMeshComponent();break;}
   Buffer=IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationTarget"));
   if(!Clock.IsValid() || !Terrain.IsValid() || !Buffer)return End(TEXT("Native headland/clock/buffer interface missing"));
   World=W;Controller=PC;Viewport=Client;OldView=PC->GetViewTarget();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;
   OldFlags=Client->EngineShowFlags;OldFlagsText=OldFlags.ToString();OldMode=Client->ViewModeIndex;
   OldBufferMode=Client->GetCurrentBufferVisualizationMode();
   OldBuffer=Buffer->GetString();OldBufferFlags=Buffer->GetFlags();OldMesh=Terrain->GetStaticMesh();OldOverrides=Terrain->OverrideMaterials;
   SavedState=true;
   if(OldMode!=VMI_Lit || OldFlags.VisualizeBuffer)return End(TEXT("Diagnosis requires a normal fresh Lit viewport baseline"));
   Camera=W->SpawnActor<ACameraActor>();if(!Camera.IsValid())return End(TEXT("Diagnosis camera spawn failed"));
   Camera->GetCameraComponent()->FieldOfView=65;Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
   Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);PC->SetViewTarget(Camera.Get());Ready=true;Apply();return false;
  }
  ++Frames;
  if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling()){AtWall=FPlatformTime::Seconds();AtGame=W->GetTimeSeconds();Frames=0;return false;}
  if(FPlatformTime::Seconds()-AtWall<20 || W->GetTimeSeconds()-AtGame<20 || Frames<48)return false;
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("file"),Files[Stage]);
  J->SetNumberField(TEXT("wall_settle_seconds"),FPlatformTime::Seconds()-AtWall);J->SetNumberField(TEXT("view_mode"),Client->ViewModeIndex);
  J->SetStringField(TEXT("buffer_target"),Buffer->GetString());J->SetStringField(TEXT("viewport_showflags"),Client->EngineShowFlags.ToString());
  J->SetStringField(TEXT("actual_buffer_visualization_mode"),Client->GetCurrentBufferVisualizationMode().ToString());
  if(auto* Local=PC->GetLocalPlayer())
  {
   FEngineShowFlags Effective=Client->EngineShowFlags;
   EngineShowFlagOverride(ESFIM_Game,EViewModeIndex(Client->ViewModeIndex),Effective,false);
   if(Stage==1 && !Effective.VisualizeBuffer)return End(TEXT("Buffer visualization did not become an effective render flag"));
   if(Stage==2 && Effective.Lighting)return End(TEXT("Unlit diagnosis did not disable effective lighting"));
   FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client->Viewport,W->Scene,Effective).SetRealtimeUpdate(true));
   FVector Location;FRotator Rotation;
   if(auto* View=Local->CalcSceneView(&Family,Location,Rotation,Client->Viewport))
   {
    const auto& P=View->FinalPostProcessSettings;
    J->SetNumberField(TEXT("eye_adaptation_exposure"),View->GetLastEyeAdaptationExposure());
    J->SetNumberField(TEXT("average_scene_luminance"),View->GetLastAverageSceneLuminance());
    J->SetNumberField(TEXT("min_brightness_or_ev100"),P.AutoExposureMinBrightness);J->SetNumberField(TEXT("max_brightness_or_ev100"),P.AutoExposureMaxBrightness);
    J->SetNumberField(TEXT("exposure_bias"),P.AutoExposureBias);J->SetNumberField(TEXT("exposure_method"),int32(P.AutoExposureMethod));
    J->SetNumberField(TEXT("saturation"),P.ColorSaturation.W);J->SetNumberField(TEXT("highlight_saturation"),P.ColorSaturationHighlights.W);
    J->SetNumberField(TEXT("bloom"),P.BloomIntensity);J->SetNumberField(TEXT("film_slope"),P.FilmSlope);
    J->SetNumberField(TEXT("local_exposure_highlight_contrast"),P.LocalExposureHighlightContrastScale);
    J->SetNumberField(TEXT("local_exposure_shadow_contrast"),P.LocalExposureShadowContrastScale);
    J->SetNumberField(TEXT("local_exposure_detail_strength"),P.LocalExposureDetailStrength);
    J->SetBoolField(TEXT("effective_lighting"),Family.EngineShowFlags.Lighting);J->SetBoolField(TEXT("effective_visualize_buffer"),Family.EngineShowFlags.VisualizeBuffer);
   }
   else return End(TEXT("Effective scene-view exposure could not be read"));
  }
  else return End(TEXT("Actual local player missing"));
  if(Stage==1 && (Buffer->GetString()!=TEXT("BaseColor") || Client->GetCurrentBufferVisualizationMode()!=TEXT("BaseColor")))return End(TEXT("Actual viewport did not enter BaseColor visualization"));
  for(const TCHAR* Name:{TEXT("r.DefaultFeature.AutoExposure.Bias"),TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"),TEXT("r.EyeAdaptationQuality"),TEXT("r.UsePreExposure")})
   if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(Name))J->SetStringField(Name,Variable->GetString());
  if(!FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Files[Stage]))return End(TEXT("Actual material diagnostic image readback failed"));
  Shots.Add(MakeShared<FJsonValueObject>(J));
  if(++Stage==UE_ARRAY_COUNT(Files))return End();Apply();return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeadlandMaterialDiagnosis923,"NeonCity.Phase205c.MainlandC923.Standalone.HeadlandMaterialDiagnosis",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHeadlandMaterialDiagnosis923::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("Material diagnosis requires actual readback and disabled automatic persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(HeadlandMaterial923::FDiagnosis(this));return true;
}
#endif
