#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandResidence.h"
#include "Adventure/IslandLandInteriorExposure.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandScenePanel.h"
#include "Adventure/IslandLandLight.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace GymLightingNative
{
/** Two declared fixed render cameras, three times. No gym use, player pose,
 * input, save, reward or30-second animation regression. Clock/view restored. */
class FFlow:public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* In):Test(In),Evidence(TEXT("GymLighting")){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>90)return End(TEXT("Short Gym lighting comparison timed out"));
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
  if(!Initialized)
  {
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")) || (!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")) && !(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")))))return End(TEXT("Actual Gym stills require motion or stills+metrics with HUD enabled"));
   Evidence.Record(W,PC,0);
   for(TActorIterator<AIslandLandResidence> I(W);I;++I){if(Residence)return End(TEXT("Ambiguous actual mansion"));Residence=*I;}
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)Clock=*I;
   for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)if(I->Kind==EIslandLeisureKind::ExerciseBike)Bike=*I;
   for(TActorIterator<AIslandLandInteriorExposure> I(W);I;++I)if(I->GetFName()==TEXT("IslandLandInteriorExposure_6"))Gym=*I;
   if(!Residence || !Clock || !Bike || !Gym || !Bike->ScenePanel || !Bike->Furniture)return End(TEXT("Native Gym, room metering, clock or real facility absent"));
   Origin=Residence->GetActorLocation();
   if(FMath::Abs(Gym->DaylightEV100Floor-7.5f)>.001f || Gym->Exposure->bUnbound || FMath::Abs(Gym->Exposure->Priority-8)>.001f || !Gym->Bounds->GetUnscaledBoxExtent().Equals(FVector(390,500,190),.01))return End(TEXT("Expected guarded local Gym daylight candidate7.5; no global or other-zone override"));
   auto& S=Gym->Exposure->Settings;if(FMath::Abs(S.AutoExposureMaxBrightness-15.8f)>.001f || FMath::Abs(S.AutoExposureBias)>.001f || FMath::Abs(S.LocalExposureShadowContrastScale-.65f)>.001f || FMath::Abs(Gym->DaylightHighlightContrast-.55f)>.001f)return End(TEXT("Expected local window highlight candidate missing, or retained maximum/bias/shadow settings changed"));
   MediumEye=Point(FVector(-6.5,-8,10));MediumAim=Point(FVector(-3.4,-5.35,9.35));
   CloseEye=Bike->GetActorTransform().TransformPosition(FVector(90,90,130));CloseAim=Bike->GetActorTransform().TransformPosition(FVector(42,0,109));
   if(!CheckEye(MediumEye,TEXT("Medium235"),true) || !CheckEye(CloseEye,TEXT("CloseRight245"),false))return End(TEXT("Gym fixed comparison eye lacks actual support/clearance"));
   OldView=PC->GetViewTarget();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);ClockChanged=true;
   Camera=W->SpawnActor<ACameraActor>();if(!Camera)return End(TEXT("Gym fixed evidence camera unavailable"));Camera->GetCameraComponent()->FieldOfView=65;
   Initialized=true;SetView();return false;
  }
  if(W->GetTimeSeconds()-ViewStarted<4)return false;
  auto* Scheduler=W->GetSubsystem<UWorldLightingSubsystem>();if(Scheduler)Scheduler->RefreshLighting();
  const float Hours[]={10,17.8f,22};const TCHAR* Labels[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};
  const float Day=FMath::Clamp(FMath::Sin((Hours[Hour]-6)*PI/12),0.f,1.f),Expected=AIslandLandInteriorExposure::MinimumEV100(Day,7.5f);
  const bool FloorMatched=FMath::Abs(Gym->Exposure->Settings.AutoExposureMinBrightness-Expected)<.01f;
  if(!FloorMatched)Failures.Add(TEXT("Actual Gym metering does not match existing solar rule"));
  const float ExpectedHighlights=FMath::Lerp(.85f,.55f,FMath::SmoothStep(.01f,.15f,Day));
  if(FMath::Abs(Gym->Exposure->Settings.LocalExposureHighlightContrastScale-ExpectedHighlights)>.001f)Failures.Add(TEXT("Actual Gym local window highlights do not match solar policy"));
  if(Hour==2 && FMath::Abs(Gym->Exposure->Settings.AutoExposureMinBrightness+1.5f)>.001f)Failures.Add(TEXT("Absolute original Night EV100-1.5 changed"));
  const FString View=ViewIndex?TEXT("CloseRight245"):TEXT("Medium235"),Name=TEXT("GymLighting-")+View+TEXT("-")+Labels[Hour]+TEXT(".png");
  const bool Captured=FNeonCityVisualEvidence::CaptureHUD(W,Name);if(!Captured)Failures.Add(TEXT("Native Gym image capture failed: ")+Name);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("image"),TEXT("../UI/")+Name);Row->SetStringField(TEXT("view"),View);Row->SetStringField(TEXT("time"),Labels[Hour]);Row->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());Row->SetBoolField(TEXT("captured"),Captured);
  Row->SetStringField(TEXT("eye_cm"),Camera->GetActorLocation().ToString());Row->SetNumberField(TEXT("expected_min_ev100"),Expected);Row->SetNumberField(TEXT("actual_min_ev100"),Gym->Exposure->Settings.AutoExposureMinBrightness);Row->SetNumberField(TEXT("max_ev100"),Gym->Exposure->Settings.AutoExposureMaxBrightness);Row->SetNumberField(TEXT("bias"),Gym->Exposure->Settings.AutoExposureBias);Row->SetBoolField(TEXT("night_branch_original"),Hour!=2 || FMath::Abs(Expected+1.5f)<.001f);Row->SetNumberField(TEXT("actual_sun_lux"),Clock->GetSun()->Intensity);
  Row->SetNumberField(TEXT("expected_local_highlights"),ExpectedHighlights);Row->SetNumberField(TEXT("actual_local_highlights"),Gym->Exposure->Settings.LocalExposureHighlightContrastScale);
  TArray<TSharedPtr<FJsonValue>> Lamps;for(const auto& Lamp:Bike->ScenePanel->Lights)if(Lamp){auto* C=Cast<UPointLightComponent>(Lamp->GetLightComponent());auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("actor"),Lamp->GetName());L->SetNumberField(TEXT("lumens_policy"),Lamp->Lumens);L->SetNumberField(TEXT("day_multiplier"),Lamp->DayMultiplier);L->SetNumberField(TEXT("actual_intensity"),C?C->Intensity:0);L->SetBoolField(TEXT("visible"),C && C->IsVisible());Lamps.Add(MakeShared<FJsonValueObject>(L));}Row->SetArrayField(TEXT("unchanged_real_room_lights"),Lamps);Shots.Add(MakeShared<FJsonValueObject>(Row));
  ++Hour;if(Hour==3){Hour=0;++ViewIndex;}if(ViewIndex==2)return End(TEXT(""));SetView();return false;
 }
private:
 FVector Point(FVector M)const{return Origin+FVector(M.X*100,-M.Y*100,M.Z*100);}
 bool CheckEye(FVector Eye,const TCHAR* Name,bool Standing)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(GymLightingFixedEye),false,P);const FVector Ground(Eye.X,Eye.Y,Origin.Z+840);FHitResult H;
  const bool Supported=W->LineTraceSingleByChannel(H,Ground+FVector(0,0,50),Ground-FVector(0,0,25),ECC_Pawn,Q) && H.ImpactNormal.Z>.7 && FMath::Abs(H.ImpactPoint.Z-Ground.Z)<3;
  const bool Clear=!W->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12),Q);
  const FVector Local=Gym->Bounds->GetComponentTransform().InverseTransformPosition(Eye),Extent=Gym->Bounds->GetUnscaledBoxExtent();const bool InRoom=FMath::Abs(Local.X)<Extent.X && FMath::Abs(Local.Y)<Extent.Y && FMath::Abs(Local.Z)<Extent.Z;
  bool Capsule=true;if(Standing)Capsule=!W->OverlapBlockingTestByChannel(Ground+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("view"),Name);J->SetBoolField(TEXT("floor_supported"),Supported);J->SetBoolField(TEXT("eye_clear"),Clear);J->SetBoolField(TEXT("inside_gym_metering"),InRoom);J->SetBoolField(TEXT("standing_capsule_tested"),Standing);J->SetBoolField(TEXT("standing_capsule_clear"),Capsule);Checks.Add(MakeShared<FJsonValueObject>(J));return Supported && Clear && InRoom && Capsule;
 }
 void SetView(){const float Hours[]={10,17.8f,22};const FVector Eye=ViewIndex?CloseEye:MediumEye,Aim=ViewIndex?CloseAim:MediumAim;Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PC->SetViewTarget(Camera);Clock->SetTimeOfDay(Hours[Hour]);ViewStarted=W->GetTimeSeconds();}
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Failures.Add(Error);if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();if(ClockChanged && IsValid(Clock)){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}for(const auto& F:Failures)Test->AddError(F);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("technical_capture_passed"),Failures.IsEmpty() && Shots.Num()==6);J->SetStringField(TEXT("visual_evidence_review"),TEXT("REQUIRED: white-window clipping reduction and readable interior/skin/railing detail at all three times. File capture alone is not visual PASS."));
  J->SetStringField(TEXT("scope"),TEXT("Fix-specific six real PIE render stills: identical235 medium camera and245 right grip camera geometry, without re-running occupied Gym actions. Close is a declared free render camera, not proof of player standing at the bike. Day10 retains floor7.5+log2(solarDay); low-sun(.01,.15) minimum smoothly joins to original absolute Night-1.5. Max15.8/bias0 and unchanged room light outputs read. No player relocation/input, finite exercise completion, save/load/rewards, lamp/material/global exposure/Bloom edits. Clock/view restored."));J->SetArrayField(TEXT("native_fixed_eye_support"),Checks);J->SetArrayField(TEXT("fixed_views"),Shots);TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& F:Failures)Errors.Add(MakeShared<FJsonValueString>(F));J->SetArrayField(TEXT("failures"),Errors);
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("GymLighting");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));if(!FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json"))))Test->AddError(TEXT("Gym lighting receipt write failed"));return true;
 }
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandResidence* Residence=nullptr;AIslandLeisureFacility* Bike=nullptr;AIslandLandInteriorExposure* Gym=nullptr;ANeonCityDayNightCycle* Clock=nullptr;ACameraActor* Camera=nullptr;TWeakObjectPtr<AActor> OldView;FVector Origin,MediumEye,MediumAim,CloseEye,CloseAim;TArray<TSharedPtr<FJsonValue>> Shots,Checks;TArray<FString> Failures;double Started=0;float ViewStarted=0,OldHour=0;int32 Hour=0,ViewIndex=0;bool Initialized=false,ClockChanged=false,OldCycle=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGymLightingVisualPIE,"NeonCity.Phase205c.GymLighting.VisualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGymLightingVisualPIE::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(GymLightingNative::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
