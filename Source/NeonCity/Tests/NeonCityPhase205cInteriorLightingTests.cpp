#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandResidence.h"
#include "Adventure/IslandLandLight.h"
#include "Adventure/IslandLandInteriorExposure.h"
#include "Adventure/IslandScenePanel.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace InteriorLightingNative
{
static FVector VectorField(const TSharedPtr<FJsonObject>& Row,const TCHAR* Field)
{
 const auto& V=Row->GetArrayField(Field);return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());
}

/** Fix-specific lighting evidence, not a rerun of the cached 19-furniture gate.
 * Camera/clock fixtures are declared and restored; normal player/state untouched. */
class FFlow:public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* In,bool Sunset=false):Test(In),Evidence(Sunset?TEXT("InteriorLightingSunsetFix"):TEXT("InteriorLighting")),SunsetOnly(Sunset){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>160)return End(TEXT("Affected lighting capture timed out"));
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);
  if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
  if(!Initialized)
  {
   const bool Motion=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion"));
   const bool Stills=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills"));
   const bool Metrics=FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"));
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD")) || (!Motion && !(Stills && Metrics)))
    return End(TEXT("Real offscreen stills require ArtMotion or ArtStills+ArtMetrics, with HUD capture enabled"));
   Evidence.Record(W,PC,0);
   FString Text;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/InteriorLighting01/plan.json")))
    || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Plan))
    return End(TEXT("Measured local-light plan missing"));
   Placements=Plan->GetArrayField(TEXT("placements"));Views=Plan->GetArrayField(TEXT("camera_plan"));Hours=Plan->GetArrayField(TEXT("hours"));
   if(Placements.Num()!=5 || Views.Num()!=5 || Hours.Num()!=3)return End(TEXT("Expected five real fixtures and five affected views, at three times"));
   if(SunsetOnly){const auto One=Hours[1];Hours.Reset();Hours.Add(One);}
   for(TActorIterator<AIslandLandResidence> I(W);I;++I){if(Residence)return End(TEXT("Ambiguous residence"));Residence=*I;}
   for(TActorIterator<ANeonCityDayNightCycle> I(W);I;++I)Clock=*I;
   Scheduler=W->GetSubsystem<UWorldLightingSubsystem>();
   if(!Residence || !Clock || !Scheduler)return End(TEXT("Actual residence/clock/light scheduler unavailable"));
   Origin=Residence->GetActorLocation();
   for(TActorIterator<AIslandScenePanel> I(W);I;++I)if(I->ZoneID==TEXT("Aurelia.Leisure.Spa"))Spa=*I;
   int32 FixtureCount=0,LightCount=0;
   for(TActorIterator<AActor> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.InteriorLighting01")))
   {
    if(I->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.InteriorLighting01.Fixture")))++FixtureCount;
    if(I->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.InteriorLighting01.Light")))++LightCount;
   }
   if(FixtureCount!=5 || LightCount!=5 || !Spa)return End(TEXT("Real lighting batch missing, duplicated, or existing SPA scene panel missing"));
   SurveyMounts();
   if(!Failures.IsEmpty())return End(TEXT("Local fixture survey failed; cached furniture/gameplay gates were not rerun"));
   OldView=PC->GetViewTarget();OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;
   Clock->SetCycleEnabled(false);ClockChanged=true;
   Camera=W->SpawnActor<ACameraActor>();if(!Camera)return End(TEXT("Affected camera unavailable"));
   Initialized=true;SetView();return false;
  }
  if(W->GetTimeSeconds()-ViewStarted<4)return false;
  Scheduler->RefreshLighting();
  const auto View=Views[ViewIndex]->AsObject(),Time=Hours[HourIndex]->AsObject();
  const FString Zone=View->GetStringField(TEXT("light_zone"));
  auto* Lamp=Lights.FindRef(Zone);auto* C=Lamp?Cast<UPointLightComponent>(Lamp->GetLightComponent()):nullptr;
  if(!C)return End(TEXT("Affected actual lamp vanished"));
  const float ScenePower[]={1.f,.58f,.12f};
  const float Power=Zone==TEXT("SPA")?ScenePower[FMath::Clamp(Spa->GetScene(),0,2)]:1.f;
  const float Expected=Lamp->Lumens*Power*FMath::Lerp(Lamp->DayMultiplier,1.f,Scheduler->GetNightAmount());
  const bool Visible=C->IsVisible();const bool Intensity=FMath::Abs(C->Intensity-Expected)<.2f;
  if(!Visible || !Intensity)Failures.Add(TEXT("Real clock/scene light not rendering with declared output: ")+Zone);
  const FString Name=FString::Printf(TEXT("InteriorLighting-%s-%s.png"),*View->GetStringField(TEXT("name")),*Time->GetStringField(TEXT("label")));
  const bool Captured=FNeonCityVisualEvidence::CaptureHUD(W,Name);
  if(!Captured)Failures.Add(TEXT("Actual affected still failed: ")+Name);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("image"),TEXT("../UI/")+Name);
  Row->SetStringField(TEXT("view"),View->GetStringField(TEXT("name")));Row->SetStringField(TEXT("light_zone"),Zone);
  Row->SetNumberField(TEXT("hour"),Clock->GetTimeOfDay());Row->SetNumberField(TEXT("night_amount"),Scheduler->GetNightAmount());
  Row->SetNumberField(TEXT("actual_lumens"),C->Intensity);Row->SetNumberField(TEXT("expected_lumens"),Expected);
  Row->SetBoolField(TEXT("actual_light_visible"),Visible);Row->SetBoolField(TEXT("actual_cast_shadows"),C->CastShadows);
  Row->SetNumberField(TEXT("global_active_light_count"),Scheduler->GetActiveLightCount());Row->SetNumberField(TEXT("global_shadow_light_count"),Scheduler->GetShadowLightCount());
  Row->SetStringField(TEXT("actual_eye_cm"),Camera->GetActorLocation().ToString());Row->SetBoolField(TEXT("captured"),Captured);
  // Read-only diagnostics distinguish physical illumination from a missing
  // metering volume. No existing exposure field is changed by this test.
  TArray<TSharedPtr<FJsonValue>> Metering;
  for(TActorIterator<AIslandLandInteriorExposure> I(W);I;++I)
  {
   const FVector Local=I->Bounds->GetComponentTransform().InverseTransformPosition(Camera->GetActorLocation());
   const FVector Extent=I->Bounds->GetUnscaledBoxExtent();
   if(FMath::Abs(Local.X)>Extent.X || FMath::Abs(Local.Y)>Extent.Y || FMath::Abs(Local.Z)>Extent.Z)continue;
   auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("actor"),I->GetName());M->SetNumberField(TEXT("priority"),I->Exposure->Priority);
   M->SetNumberField(TEXT("min_ev100"),I->Exposure->Settings.AutoExposureMinBrightness);M->SetNumberField(TEXT("max_ev100"),I->Exposure->Settings.AutoExposureMaxBrightness);
   M->SetNumberField(TEXT("daylight_floor"),I->DaylightEV100Floor);
   const float SolarDay=FMath::Clamp(FMath::Sin((Clock->GetTimeOfDay()-6)*PI/12),0.f,1.f);
   const float ExpectedFloor=AIslandLandInteriorExposure::MinimumEV100(SolarDay,I->DaylightEV100Floor);
   M->SetNumberField(TEXT("expected_min_ev100"),ExpectedFloor);
   if(SunsetOnly && FMath::Abs(I->Exposure->Settings.AutoExposureMinBrightness-ExpectedFloor)>.01f)Failures.Add(TEXT("Actual containing room metering did not adopt bounded twilight: ")+I->GetName());
   M->SetNumberField(TEXT("bias"),I->Exposure->Settings.AutoExposureBias);Metering.Add(MakeShared<FJsonValueObject>(M));
  }
  Row->SetArrayField(TEXT("existing_containing_metering_read_only"),Metering);Screenshots.Add(MakeShared<FJsonValueObject>(Row));
  ++HourIndex;if(HourIndex==Hours.Num()){HourIndex=0;++ViewIndex;}
  if(ViewIndex==Views.Num())return End(TEXT(""));SetView();return false;
 }
private:
 FVector WorldPoint(FVector M)const{return Origin+FVector(M.X*100,-M.Y*100,M.Z*100);}
 void SurveyMounts()
 {
  for(const auto& V:Placements)
  {
   const auto Row=V->AsObject();const FString Zone=Row->GetStringField(TEXT("zone"));
   const FName Tag(*FString(TEXT("NeonCity.Phase205cUpgrade.InteriorLighting01.")+Zone));
   AStaticMeshActor* Fixture=nullptr;AIslandLandLight* Lamp=nullptr;
   for(TActorIterator<AActor> I(W);I;++I)if(I->ActorHasTag(Tag))
   {if(auto* A=Cast<AStaticMeshActor>(*I))Fixture=A;if(auto* A=Cast<AIslandLandLight>(*I))Lamp=A;}
   auto* C=Fixture?Fixture->GetStaticMeshComponent():nullptr;auto* Light=Lamp?Cast<UPointLightComponent>(Lamp->GetLightComponent()):nullptr;
   if(!C || !C->GetStaticMesh() || !Light){Failures.Add(TEXT("Actual pendant mesh/light missing: ")+Zone);continue;}
   if(!C->DoesSocketExist(TEXT("LightEmission")) || C->IsCollisionEnabled()
    || C->GetStaticMesh()->GetOutermost()->GetName()!=Row->GetStringField(TEXT("fixture_package")))
    Failures.Add(TEXT("Reused pendant/socket/no-collision mismatch: ")+Zone);
   const FVector Socket=C->GetSocketLocation(TEXT("LightEmission")),Mount=Fixture->GetActorLocation();
   if(FVector::Dist(Socket,Lamp->GetActorLocation())>.25 || FVector::Dist(Mount,WorldPoint(VectorField(Row,TEXT("mount_m"))))>2)
    Failures.Add(TEXT("Physical socket or ceiling mount differs: ")+Zone);
   if(FMath::Abs(Lamp->Lumens-Row->GetNumberField(TEXT("lumens")))>.01
    || FMath::Abs(Lamp->DayMultiplier-Row->GetNumberField(TEXT("day_multiplier")))>.001
    || FMath::Abs(Light->AttenuationRadius-Row->GetNumberField(TEXT("attenuation_radius_cm")))>.1
    || Light->IntensityUnits!=ELightUnits::Lumens || !Light->bUseInverseSquaredFalloff)
    Failures.Add(TEXT("Actual local photometry differs: ")+Zone);
   FCollisionQueryParams Q(SCENE_QUERY_STAT(InteriorLightingCeilingMount),true,P);Q.AddIgnoredActor(Fixture);Q.AddIgnoredActor(Lamp);
   FHitResult Hit;const bool Support=W->LineTraceSingleByChannel(Hit,Mount-FVector(0,0,30),Mount+FVector(0,0,30),ECC_Visibility,Q)
    && Hit.ImpactNormal.Z<-.7 && FMath::Abs(Hit.ImpactPoint.Z-Mount.Z)<2;
   if(!Support)Failures.Add(TEXT("Native actual ceiling support absent: ")+Zone);
   if(Zone==TEXT("SPA") && !Spa->Lights.Contains(Lamp))Failures.Add(TEXT("Existing SPA scene does not own the new north pendant: ")+Zone);
   Lights.Add(Zone,Lamp);
   auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("zone"),Zone);M->SetStringField(TEXT("fixture"),Fixture->GetName());M->SetStringField(TEXT("light"),Lamp->GetName());
   M->SetStringField(TEXT("ceiling_actor"),GetNameSafe(Hit.GetActor()));M->SetBoolField(TEXT("ceiling_supported"),Support);M->SetNumberField(TEXT("emitter_socket_distance_cm"),FVector::Dist(Socket,Lamp->GetActorLocation()));Mounts.Add(MakeShared<FJsonValueObject>(M));
  }
  // Only the replacement wide arrival view lacks a cached native clearance result.
  const auto Wide=Views.Last()->AsObject();const FVector Eye=WorldPoint(VectorField(Wide,TEXT("eye_m")));
  const FVector Floor(Eye.X,Eye.Y,Origin.Z+Wide->GetNumberField(TEXT("floor_m"))*100);
  FCollisionQueryParams Q(SCENE_QUERY_STAT(InteriorLightingNewWideCamera),false,P);FHitResult Hit;
  const bool FloorOK=W->LineTraceSingleByChannel(Hit,Floor+FVector(0,0,50),Floor-FVector(0,0,25),ECC_Pawn,Q)
   && Hit.ImpactNormal.Z>.7 && FMath::Abs(Hit.ImpactPoint.Z-Floor.Z)<3;
  const FVector Stand=Floor+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
  const bool Capsule=W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
  const bool EyeBlocked=W->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12),Q);
  if(!FloorOK || Capsule || EyeBlocked)Failures.Add(TEXT("New wide arrival view lacks actual normal floor/capsule/eye clearance"));
  auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("zone"),TEXT("Arrival-wide-camera"));M->SetBoolField(TEXT("native_floor_supported"),FloorOK);M->SetBoolField(TEXT("normal_capsule_clear"),!Capsule);M->SetBoolField(TEXT("eye_clear"),!EyeBlocked);Mounts.Add(MakeShared<FJsonValueObject>(M));
 }
 void SetView()
 {
  const auto View=Views[ViewIndex]->AsObject(),Time=Hours[HourIndex]->AsObject();
  const FVector Eye=WorldPoint(VectorField(View,TEXT("eye_m"))),Aim=WorldPoint(VectorField(View,TEXT("aim_m")));
  Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());Camera->GetCameraComponent()->FieldOfView=View->GetNumberField(TEXT("fov"));
  PC->SetViewTarget(Camera);Clock->SetTimeOfDay(Time->GetNumberField(TEXT("hour")));ViewStarted=W->GetTimeSeconds();
 }
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty())Failures.Add(Error);
  if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();
  if(ClockChanged && IsValid(Clock)){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  for(const auto& Failure:Failures)Test->AddError(Failure);
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);Run=FPaths::MakeValidFileName(Run.IsEmpty()?TEXT("UnlabelledInteriorLighting"):Run);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/(SunsetOnly?TEXT("InteriorLightingSunsetFix"):TEXT("InteriorLighting"));IFileManager::Get().MakeDirectory(*Dir,true);
  auto Receipt=MakeShared<FJsonObject>();Receipt->SetBoolField(TEXT("technical_capture_passed"),Failures.IsEmpty() && Screenshots.Num()==(SunsetOnly?5:15));
  Receipt->SetStringField(TEXT("scope"),TEXT("Fix-specific native five real ceiling-supported CC0 pendants/socket-aligned world-budgeted lights including the dark private landing; four unchanged235 affected cameras plus one declared supported replacement wide arrival x Day10/Sunset17.8/Night22. Arrival replacement is not an identical before/after view claim. SPA appended to existing scene. No exposure/Bloom/global light changes, player relocation, input, save or facility completion; cached furniture/route/finite gameplay not repeated. Clock/view restored."));
  Receipt->SetStringField(TEXT("visual_evidence_review"),TEXT("REQUIRED; image capture/visible lamp is not premium or subjective acceptance"));Receipt->SetStringField(TEXT("user_subjective_review"),TEXT("REQUIRED"));
  Receipt->SetStringField(TEXT("required_visual_gate"),TEXT("Night Window-Control railing and drop edge must be readable in actual unchanged235 camera; visible lamp/15 captured files alone is not visual PASS."));
  if(SunsetOnly){Receipt->SetStringField(TEXT("scope"),TEXT("Fix-specific five identical257 Sunset17.8 PIE stills only. Day/Night257 results cached, no finite gameplay or furniture regression. SolarDay(.01,.15) minimum blends continuously from original absoluteNight-1.5 to existing daylight floor; Day>=.15, lights/sun/sky/Bloom/bias/max unchanged. Actual camera-containing room floor, priority and real physical lamp output recorded. Camera/clock restored."));Receipt->SetStringField(TEXT("required_visual_gate"),TEXT("Study/Arrival/SPA/SeaLounge dusk interior detail readable; Window sunset sky must retain colour and horizon. Actual screenshots require review; no Premium PASS from capture."));}
  Receipt->SetArrayField(TEXT("native_mount_and_new_wide_clearance"),Mounts);Receipt->SetArrayField(TEXT("fixed_views"),Screenshots);
  TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));Receipt->SetArrayField(TEXT("failures"),Errors);
  FString JSON;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&JSON));if(!FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("receipt.json"))))Test->AddError(TEXT("Affected lighting receipt write failed"));return true;
 }
 FAutomationTestBase* Test=nullptr;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 AIslandLandResidence* Residence=nullptr;ANeonCityDayNightCycle* Clock=nullptr;UWorldLightingSubsystem* Scheduler=nullptr;AIslandScenePanel* Spa=nullptr;ACameraActor* Camera=nullptr;
 TWeakObjectPtr<AActor> OldView;FVector Origin=FVector::ZeroVector;TSharedPtr<FJsonObject> Plan;
 TArray<TSharedPtr<FJsonValue>> Placements,Views,Hours,Mounts,Screenshots;TMap<FString,AIslandLandLight*> Lights;TArray<FString> Failures;
 FNeonCityVisualEvidence Evidence;double Started=0;float ViewStarted=0,OldHour=0;int32 ViewIndex=0,HourIndex=0;bool Initialized=false,ClockChanged=false,OldCycle=false,SunsetOnly=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorLightingVisualPIE,"NeonCity.Phase205c.InteriorLighting.VisualPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInteriorLightingVisualPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(InteriorLightingNative::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorLightingSunsetFixPIE,"NeonCity.Phase205c.InteriorLighting.SunsetFixPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInteriorLightingSunsetFixPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(InteriorLightingNative::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
