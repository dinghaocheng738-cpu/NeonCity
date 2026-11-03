#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/NeonCityDayNightCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "MaterialShared.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace CoastalNightGlass921
{
class FDiagnostic : public IAutomationLatentCommand
{
 struct FVisibilityGuard
 {
  TWeakObjectPtr<UStaticMeshComponent> Component;
  bool Visible=false,Hidden=false;
 };
 FAutomationTestBase* Test;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<ACameraActor> Camera;
 TWeakObjectPtr<AActor> OldView;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 TWeakObjectPtr<UStaticMeshComponent> Glass;
 TArray<FVisibilityGuard> Guards;
 TMap<FString,FString> Quality;
 TArray<TSharedPtr<FJsonValue>> Captures;
 TSharedPtr<FJsonObject> MaterialFacts=MakeShared<FJsonObject>();
 double Began=FPlatformTime::Seconds(),AtWall=0;
 float AtGame=0,OldHour=12;
 int32 Stage=0,Frames=0;
 bool OldCycle=true,ClockCaptured=false,Modified=false,Cleaned=false,Restored=false;
 FString Directory,ActorPath,ComponentPath,MeshPath;

 void Cleanup()
 {
  if(Cleaned)return;
  Cleaned=true;
  if(Modified && Glass.IsValid())
   for(const auto& G:Guards)if(G.Component==Glass)
   {Glass->SetVisibility(G.Visible,false);Glass->SetHiddenInGame(G.Hidden,false);Restored=Glass->GetVisibleFlag()==G.Visible && Glass->bHiddenInGame==G.Hidden;break;}
  if(Controller.IsValid() && OldView.IsValid())Controller->SetViewTarget(OldView.Get());
  if(Camera.IsValid())Camera->Destroy();
  if(ClockCaptured && Clock.IsValid()){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
 }
 bool End(FString Error=FString())
 {
  bool OtherVisibilityUnchanged=true,QualityUnchanged=true;
  for(const auto& G:Guards)if(G.Component!=Glass)
   OtherVisibilityUnchanged&=G.Component.IsValid() && G.Component->GetVisibleFlag()==G.Visible && G.Component->bHiddenInGame==G.Hidden;
  for(const auto& Item:Quality)
  {const auto* V=IConsoleManager::Get().FindConsoleVariable(*Item.Key);QualityUnchanged&=V && V->GetString()==Item.Value;}
  Cleanup();
  if(!OtherVisibilityUnchanged)Error+=TEXT(" Other translucent-component visibility changed during the diagnostic.");
  if(!QualityUnchanged)Error+=TEXT(" Rendering quality/AA changed during the diagnostic.");
  if(Modified && !Restored)Error+=TEXT(" Courtyard glass visibility did not restore exactly.");
  if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);
  J->SetStringField(TEXT("actor"),ActorPath);J->SetStringField(TEXT("component"),ComponentPath);J->SetStringField(TEXT("mesh"),MeshPath);
  J->SetObjectField(TEXT("material_and_shadow_flags"),MaterialFacts);J->SetArrayField(TEXT("captures"),Captures);
  J->SetNumberField(TEXT("translucent_components_guarded"),Guards.Num());J->SetBoolField(TEXT("other_component_visibility_unchanged"),OtherVisibilityUnchanged);
  J->SetBoolField(TEXT("normal_AA_and_quality_unchanged"),QualityUnchanged);J->SetBoolField(TEXT("courtyard_glass_restored"),Restored);
  J->SetNumberField(TEXT("components_temporarily_hidden"),Modified?1:0);J->SetNumberField(TEXT("player_pose_writes"),0);J->SetNumberField(TEXT("asset_or_map_writes"),0);
  J->SetStringField(TEXT("scope"),TEXT("Same C920View15 eye/aim/FOV at22h: baseline, then hide only StaticMeshActor_1665's complete Courtyard21_Glass component, the smallest existing native component. This briefly hides all panes of that one building, not only this room. Every other translucent component stays unchanged. No material/geometry/AA/quality writes, player teleport or production fix. Visibility, camera and clock restore. Captures diagnose the black lines; screenshot success does not certify their removal."));
  auto Q=MakeShared<FJsonObject>();for(const auto& Item:Quality)Q->SetStringField(Item.Key,Item.Value);J->SetObjectField(TEXT("quality"),Q);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
  if(!Directory.IsEmpty())
  {const FString File=Directory/TEXT("receipt.json");if(IFileManager::Get().FileExists(*File) || !FFileHelper::SaveStringToFile(Text,*File))Test->AddError(TEXT("Night glass receipt exists or cannot be written; prior evidence is immutable"));}
  return true;
 }
 void RestartWait(){AtWall=FPlatformTime::Seconds();AtGame=World->GetTimeSeconds();Frames=0;}
public:
 explicit FDiagnostic(FAutomationTestBase* T):Test(T){}
 virtual ~FDiagnostic(){Cleanup();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Began>70)return End(TEXT("Night glass focused diagnostic timed out"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  auto* PC=W->GetFirstPlayerController();if(!PC)return false;World=W;
  if(Stage==0)
  {
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return End(TEXT("NightGlassDiagnostic requires actual rendered stills"));
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);if(Run.IsEmpty())return End(TEXT("Unique evidence label required"));
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("NightGlassDiagnostic921");IFileManager::Get().MakeDirectory(*Directory,true);
   if(IFileManager::Get().FileExists(*(Directory/TEXT("receipt.json"))))return End(TEXT("Never overwrite an existing night glass result"));
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel){Clock=*I;break;}
   for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->GetFName()==TEXT("StaticMeshActor_1665"))
   {if(Glass.IsValid())return End(TEXT("Ambiguous exact Courtyard glass actor"));Glass=I->GetStaticMeshComponent();ActorPath=I->GetPathName();}
   if(!Clock.IsValid() || !Glass.IsValid() || !Glass->GetStaticMesh())return End(TEXT("Actual clock or Courtyard glass component missing"));
   ComponentPath=Glass->GetPathName();MeshPath=Glass->GetStaticMesh()->GetPathName();
   if(MeshPath!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/SM_C920_Courtyard21_Glass.SM_C920_Courtyard21_Glass")
    || Glass->GetNumMaterials()!=1 || !Glass->GetVisibleFlag() || Glass->bHiddenInGame
    || !Glass->GetComponentLocation().Equals(FVector(0,-800,55),.01f))return End(TEXT("Exact Courtyard glazing mesh/material-count/pose/visibility preflight failed"));
   auto* Material=Glass->GetMaterial(0)?Glass->GetMaterial(0)->GetMaterial():nullptr;
   if(!Material || Material->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC920/M_C920_OpticalGlass.M_C920_OpticalGlass"))return End(TEXT("Expected unchanged C920 OpticalGlass material missing"));
   MaterialFacts->SetStringField(TEXT("material"),Material->GetPathName());MaterialFacts->SetNumberField(TEXT("blend_mode"),Material->GetBlendMode());
   MaterialFacts->SetNumberField(TEXT("shading_models"),Material->GetShadingModels().GetShadingModelField());MaterialFacts->SetBoolField(TEXT("two_sided"),Material->IsTwoSided());
   MaterialFacts->SetNumberField(TEXT("translucency_lighting_mode"),Material->TranslucencyLightingMode);MaterialFacts->SetNumberField(TEXT("refraction_method"),Material->RefractionMethod);
   MaterialFacts->SetBoolField(TEXT("casts_shadow"),Glass->CastShadow);MaterialFacts->SetBoolField(TEXT("casts_dynamic_shadow"),Glass->bCastDynamicShadow);
   MaterialFacts->SetBoolField(TEXT("casts_volumetric_translucent_shadow"),Glass->bCastVolumetricTranslucentShadow);MaterialFacts->SetNumberField(TEXT("collision_enabled"),Glass->GetCollisionEnabled());
   for(const auto& Item:{TPair<EMaterialProperty,const TCHAR*>(MP_Refraction,TEXT("refraction_input")),TPair<EMaterialProperty,const TCHAR*>(MP_WorldPositionOffset,TEXT("world_position_offset_input"))})
   {const auto* Input=Material->GetExpressionInputForProperty(Item.Key);MaterialFacts->SetStringField(Item.Value,Input && Input->Expression?Input->Expression->GetPathName():TEXT("disconnected"));}
   for(TActorIterator<AActor> I(W);I;++I)
   {TInlineComponentArray<UStaticMeshComponent*> Components(*I);for(auto* C:Components)
    {bool Translucent=false;for(int32 K=0;K<C->GetNumMaterials();++K)if(auto* M=C->GetMaterial(K))Translucent|=IsTranslucentBlendMode(M->GetBlendMode());if(Translucent)Guards.Add({C,C->GetVisibleFlag(),bool(C->bHiddenInGame)});}}
   for(const TCHAR* Name:{TEXT("r.AntiAliasingMethod"),TEXT("r.ScreenPercentage"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality")})
    if(const auto* V=IConsoleManager::Get().FindConsoleVariable(Name))Quality.Add(Name,V->GetString());
   Controller=PC;OldView=PC->GetViewTarget();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;ClockCaptured=true;
   Camera=W->SpawnActor<ACameraActor>();if(!Camera.IsValid())return End(TEXT("Comparison camera could not spawn"));
   const FVector Eye(-270,-1250,640),Aim(-680,-1180,565);Camera->GetCameraComponent()->FieldOfView=65;Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
   Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(22);PC->SetViewTarget(Camera.Get());Stage=1;RestartWait();return false;
  }
  ++Frames;const double WallAge=FPlatformTime::Seconds()-AtWall;const float GameAge=W->GetTimeSeconds()-AtGame;
  if(WallAge<3.25 || GameAge<3.f || Frames<24)return false;
  const FString File=Stage==1?TEXT("C921-NightGlass-00-Baseline.jpg"):TEXT("C921-NightGlass-01-CourtyardGlassHidden.jpg");
  if(!FNeonCityVisualEvidence::CaptureRenderedJPEG(W,File))return End(TEXT("Actual night glass screenshot capture failed: ")+File);
  auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("file"),File);C->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());C->SetBoolField(TEXT("courtyard_glass_visible"),Glass->GetVisibleFlag());
  C->SetNumberField(TEXT("stable_wall_seconds"),WallAge);C->SetNumberField(TEXT("stable_game_seconds"),GameAge);C->SetNumberField(TEXT("stable_frames"),Frames);Captures.Add(MakeShared<FJsonValueObject>(C));
  if(Stage==1){Glass->SetVisibility(false,false);Modified=true;Stage=2;RestartWait();return false;}
  return End();
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalNightGlassDiagnostic921,"NeonCity.Phase205c.MainlandC921.Standalone.NightGlassDiagnostic",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalNightGlassDiagnostic921::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Night glass diagnostic preserves human saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(CoastalNightGlass921::FDiagnostic(this));return true;}
#endif
