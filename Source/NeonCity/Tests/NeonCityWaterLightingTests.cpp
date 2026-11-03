#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCitySwimmingComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Water/SwimmingData.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Police/PoliceObservation.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftFlightStatusComponent.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Aircraft/Lighting/HelicopterSearchLightComponent.h"
#include "Aircraft/Lighting/SearchLightData.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NeonCityVisualEvidence.h"
#include "World/NeonCityDayNightCycle.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Templates/SharedPointer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaterProfiles,"NeonCity.Water.ProfileAndInterfaces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWaterProfiles::RunTest(const FString&)
{
 auto* Swim=NewObject<USwimmingData>();
 TestTrue(TEXT("Default swimming profile is valid"),Swim->IsValidProfile());
 Swim->UnderwaterSpeed=0; TestFalse(TEXT("Invalid swimming profile rejected"),Swim->IsValidProfile());
 auto* Light=NewObject<USearchLightData>(); TestTrue(TEXT("Default search light profile is valid"),Light->IsValidProfile());
 Light->OuterCone=Light->InnerCone; TestFalse(TEXT("Search light cone ordering is validated"),Light->IsValidProfile());
 TestTrue(TEXT("Water states expose dry/shallow/surface/underwater"),uint8(EWaterInteractionState::Underwater)==3);
 return true;
}

namespace NeonCityWaterLightingTests
{
void Key(APlayerController* PC,FKey KeyName,EInputEvent Event)
{ if(PC) PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),KeyName,Event,FPlatformTime::Cycles64())); }
void Capture(UWorld* World,APlayerController* PC,const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityWaterLightingCapture")) || !World || !PC || !World->GetGameViewport() || !World->GetGameViewport()->Viewport) return;
 World->GetGameViewport()->Viewport->Draw(false); TArray<FColor> Pixels;
 if(!World->GetGameViewport()->Viewport->ReadPixels(Pixels)) return;
 for(FColor& Pixel:Pixels) Pixel.A=255;
 const FIntPoint Size=World->GetGameViewport()->Viewport->GetSizeXY(); TArray64<uint8> PNG;
 FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("WaterLighting")),true);
 FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("WaterLighting")/Name));
}
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  float Height=0; FVector Flow; const bool WaterRegistered=W && W->GetSubsystem<UMarineWorldSubsystem>() && W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(FVector(0,-14500,-150),Height,Flow);
  return Player && PC && W->GetTimeSeconds()>2 && Player->GetCharacterMovement()->IsMovingOnGround() && WaterRegistered;
 }
 void PlaceWater()
 {
  Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  Player->SetActorLocation(FVector(0,-14500,-150),false,nullptr,ETeleportType::TeleportPhysics);
  Player->GetCharacterMovement()->StopMovementImmediately(); Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 }
};

class FWaterScenario : public IAutomationLatentCommand
{
public:
 FWaterScenario(FAutomationTestBase* InTest,int32 InKind):Test(InTest),Kind(InKind) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(FString::Printf(TEXT("Water scenario timeout stage %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   if(Kind==3)
   {
    Save=F.W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); Save->ConfigureSlot(TEXT("NeonCity_Test_Water_Restore")); Save->DeleteSave();
    if(!Test->TestTrue(TEXT("Dry land save succeeds"),Save->SaveGame())) return true;
   }
   F.PlaceWater(); Stage=1; Phase=F.W->GetTimeSeconds(); return false;
  }
  if(Stage==1)
  {
   auto* Move=F.Player->GetCharacterMovement(); auto* Water=F.Player->FindComponentByClass<UWaterInteractionComponent>();
   if(!Move->IsSwimming()) { if(F.W->GetTimeSeconds()-Phase>8) { Test->AddError(TEXT("Character did not enter native swimming mode")); return true; } return false; }
   // Native CMC immersion can be observed one frame before the sampled water
   // component's interval tick. Refresh explicitly so the assertion checks
   // the same frame's volume query rather than a transient default snapshot.
   if(Water) Water->Refresh();
   if(!Water || Water->GetWaterState()==EWaterInteractionState::Dry)
   {
    if(F.W->GetTimeSeconds()-Phase>8) { Test->AddError(TEXT("Water component did not resolve a registered water volume")); return true; }
    return false;
   }
   Test->TestTrue(TEXT("Native physics volume reports water"),F.Player->GetPhysicsVolume() && F.Player->GetPhysicsVolume()->bWaterVolume);
   Test->AddInfo(FString::Printf(TEXT("Water state=%d playerZ=%.1f sampleZ=%.1f depth=%.1f"),Water?int32(Water->GetWaterState()):-1,F.Player->GetActorLocation().Z,Water?Water->GetSample().SurfacePoint.Z:0,Water?Water->GetSample().DepthAt(F.Player->GetActorLocation()):0));
   Test->TestTrue(TEXT("Water component reports a non-dry state"),Water && Water->GetWaterState()!=EWaterInteractionState::Dry);
   Test->TestEqual(TEXT("Swimming disables ordinary combat"),F.Player->GetCombatComponent()->GetCombatState(),ENeonCityCombatState::Disabled);
   if(Kind==2)
   {
    F.Player->SetActorLocation(FVector(5200,-4380,-160),false,nullptr,ETeleportType::TeleportPhysics);
    Move->StopMovementImmediately(); F.Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
    Stage=6; Phase=F.W->GetTimeSeconds(); return false;
   }
   if(Kind==3)
   {
    Test->TestFalse(TEXT("Transient swimming state cannot overwrite disk save"),Save->SaveGame());
    auto* Model=Save->ReadSaveForInspection();
    if(!Test->TestNotNull(TEXT("Dry save remains readable"),Model)) return true;
    Model->Player.Transform.SetLocation(FVector(0,-14500,-400));
    Test->TestTrue(TEXT("Legacy water-location fixture writes"),Save->WriteTestFixture(Model));
    F.Player->FindComponentByClass<UOxygenComponent>()->CurrentOxygen=5;
    for(TActorIterator<ANeonCityHelicopterPrototype> It(F.W);It;++It) if(auto* Light=It->FindComponentByClass<UHelicopterSearchLightComponent>()) Light->SetEnabled(true);
    Test->TestTrue(TEXT("Load chooses safe land position"),Save->LoadGame());
    Stage=7; Phase=F.W->GetTimeSeconds(); return false;
   }
   if(Kind==0) { OriginOxygen=F.Player->FindComponentByClass<UOxygenComponent>()->CurrentOxygen; Key(F.PC,EKeys::C,IE_Pressed); Stage=2; Phase=F.W->GetTimeSeconds(); return false; }
   if(Kind==1)
   {
    F.Player->SetActorLocation(FVector(0,-14500,-400),false,nullptr,ETeleportType::TeleportPhysics); F.Player->GetCapsuleComponent()->UpdatePhysicsVolume(true); Water->Refresh();
    Test->TestTrue(TEXT("Drowning fixture is fully submerged"),Water->IsFullySubmerged());
    F.Player->FindComponentByClass<UOxygenComponent>()->CurrentOxygen=0; Stage=3; Phase=F.W->GetTimeSeconds(); return false;
   }
  }
  if(Stage==2)
  {
   auto* Water=F.Player->FindComponentByClass<UWaterInteractionComponent>();
   // A wave can already cover the mouth when this stage starts. Keep the real
   // C input held until the body has actually dived below the surface band.
   if(Water && Water->GetWaterState()==EWaterInteractionState::Underwater
    && F.W->GetTimeSeconds()-Phase>.25f
    && Water->GetSample().DepthAt(F.Player->GetActorLocation())>F.Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*1.2f+120)
   {
    Key(F.PC,EKeys::C,IE_Released); Test->TestTrue(TEXT("C transitions surface swimming to underwater"),true);
    OriginOxygen=F.Player->FindComponentByClass<UOxygenComponent>()->CurrentOxygen;
    const FVector Eye=F.Player->GetActorLocation()+FVector(300,0,0);
    Test->TestFalse(TEXT("Police cannot observe a fully submerged player"),NeonCityObservation::Visible(F.W,nullptr,Eye,FVector(-1,0,0),1000,90,F.Player));
    Phase=F.W->GetTimeSeconds(); Stage=4; Capture(F.W,F.PC,TEXT("A-Underwater.png")); return false;
   }
   if(F.W->GetTimeSeconds()-Phase>12) { Key(F.PC,EKeys::C,IE_Released); Test->AddError(TEXT("C did not submerge the player")); return true; }
   return false;
  }
  if(Stage==3)
  {
   auto* H=F.Player->GetHealthComponent();
   if(H->GetCurrentHealth()<H->GetMaxHealth())
   {
    Test->TestTrue(TEXT("Zero oxygen applies unified drowning damage"),true);
    H->ApplyPersistenceHealth(10,H->GetMaxHealth()); Stage=8; Phase=F.W->GetTimeSeconds(); return false;
   }
   if(F.W->GetTimeSeconds()-Phase>8) { Test->AddError(TEXT("Zero oxygen did not damage the player")); return true; }
   return false;
  }
  if(Stage==4)
  {
   auto* Oxygen=F.Player->FindComponentByClass<UOxygenComponent>();
   if(F.W->GetTimeSeconds()-Phase>1.2f)
   {
    Test->TestTrue(TEXT("Underwater oxygen drains"),Oxygen->CurrentOxygen<OriginOxygen);
    LowOxygen=Oxygen->CurrentOxygen; Key(F.PC,EKeys::SpaceBar,IE_Pressed); Stage=5; Phase=F.W->GetTimeSeconds(); return false;
   }
   return false;
  }
  if(Stage==5)
  {
   auto* Water=F.Player->FindComponentByClass<UWaterInteractionComponent>(); auto* Oxygen=F.Player->FindComponentByClass<UOxygenComponent>();
   if(Water && Water->GetWaterState()!=EWaterInteractionState::Underwater) Key(F.PC,EKeys::SpaceBar,IE_Released);
   if(Water && Water->GetWaterState()==EWaterInteractionState::Surface && Oxygen->CurrentOxygen>LowOxygen+2)
   { Stage=90; Phase=F.W->GetTimeSeconds(); return false; }
   if(F.W->GetTimeSeconds()-Phase>10) { Key(F.PC,EKeys::SpaceBar,IE_Released); Test->AddError(TEXT("Space did not return the player to surface and restore oxygen")); return true; }
   return false;
  }
  if(Stage==6)
  {
   F.Player->AddMovementInput(FVector(0,1,0),1);
   if(F.Player->GetActorLocation().Y>-3780 && F.Player->GetCharacterMovement()->IsMovingOnGround())
   { Test->TestTrue(TEXT("Swimming reaches the real low shore and resumes walking"),true); Capture(F.W,F.PC,TEXT("E-Shore.png")); return true; }
   if(F.W->GetTimeSeconds()-Phase>15) { Test->AddError(FString::Printf(TEXT("Shore exit stuck: %s mode %d"),*F.Player->GetActorLocation().ToString(),int(F.Player->GetCharacterMovement()->MovementMode))); return true; }
   return false;
  }
  if(Stage==7)
  {
   if(F.W->GetTimeSeconds()-Phase<.5f) return false;
   Test->TestTrue(TEXT("Restore is walking on safe land"),F.Player->GetCharacterMovement()->IsMovingOnGround() && !F.W->GetSubsystem<UMarineWorldSubsystem>()->IsDeepWater(F.Player->GetActorLocation(),0));
   auto* Oxygen=F.Player->FindComponentByClass<UOxygenComponent>(); Test->TestEqual(TEXT("Restore resets oxygen"),Oxygen->CurrentOxygen,Oxygen->MaxOxygen);
   for(TActorIterator<ANeonCityHelicopterPrototype> It(F.W);It;++It) if(auto* Light=It->FindComponentByClass<UHelicopterSearchLightComponent>()) Test->TestFalse(TEXT("Real save load clears search light"),Light->IsEnabled());
   Capture(F.W,F.PC,TEXT("G-SafeLoad.png")); Save->DeleteSave(); return true;
  }
  if(Stage==8)
  {
   if(F.Player->GetHealthComponent()->IsDead())
   {
    Test->TestTrue(TEXT("Drowning keeps Actor and resolves unified death state"),IsValid(F.Player) && F.Player->FindComponentByClass<UDamageableCharacterComponent>()->GetBodyState()==ENeonCityBodyState::Dead);
    Capture(F.W,F.PC,TEXT("B-Drowning.png")); return true;
   }
   if(F.W->GetTimeSeconds()-Phase>4) { Test->AddError(TEXT("Drowning did not progress from downed to death")); return true; }
   return false;
  }
  if(Stage==90)
  {
   // Ascending out of a real dive is not a settled surface sample. Require
   // convergence within two seconds, then measure the same 35 cm tolerance.
   const auto* Water=F.Player->FindComponentByClass<UWaterInteractionComponent>();
   const float Depth=Water->GetSample().DepthAt(F.Player->GetActorLocation());
   const float Target=F.Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.55f;
   const float RelativeSpeed=F.Player->GetVelocity().Z-Water->GetSample().Flow.Z;
   if(F.W->GetTimeSeconds()-Phase>.5f && FMath::Abs(Depth-Target)<15 && FMath::Abs(RelativeSpeed)<30)
   { Stage=9; Phase=F.W->GetTimeSeconds(); SurfaceMin=SurfaceMax=Depth; return false; }
   if(F.W->GetTimeSeconds()-Phase>2)
   { Test->AddError(FString::Printf(TEXT("Surface failed to settle: depth %.2f target %.2f relative speed %.2f"),Depth,Target,RelativeSpeed)); return true; }
   return false;
  }
  if(Stage==9)
  {
   const auto* Water=F.Player->FindComponentByClass<UWaterInteractionComponent>();
   const float Depth=Water->GetSample().DepthAt(F.Player->GetActorLocation());
   SurfaceMin=FMath::Min(SurfaceMin,Depth); SurfaceMax=FMath::Max(SurfaceMax,Depth);
   if(F.W->GetTimeSeconds()-Phase<2) return false;
   Test->TestTrue(TEXT("Surface buoyancy settles within 35 cm relative to the actual wave"),SurfaceMax-SurfaceMin<35);
   Test->AddInfo(FString::Printf(TEXT("Surface-swim relative depth range %.2f .. %.2f cm"),SurfaceMin,SurfaceMax));
   Capture(F.W,F.PC,TEXT("C-Surface.png")); return true;
  }
  return true;
 }
 FAutomationTestBase* Test; int32 Kind=0,Stage=0; double Started=0,Phase=0; float OriginOxygen=0,LowOxygen=0,SurfaceMin=0,SurfaceMax=0; FScene F; UNeonCitySaveSubsystem* Save=nullptr;
};

class FSearchLightScenario : public IAutomationLatentCommand
{
public:
 explicit FSearchLightScenario(FAutomationTestBase* InTest):Test(InTest) {}
 bool Next(int32 Value) { Stage=Value; Phase=F.W->GetTimeSeconds(); return false; }
 void BeginInspectionView()
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")) && !FParse::Param(FCommandLine::Get(),TEXT("NeonCityWaterLightingCapture")))return;
  const FVector Target=Heli->GetActorLocation()+FVector(0,-700,-700), Eye=Heli->GetActorLocation()+FVector(-2000,-2200,1600);
  InspectionCamera=F.W->SpawnActor<ACameraActor>(Eye,(Target-Eye).Rotation()); F.PC->SetViewTarget(InspectionCamera);
  F.PC->PlayerCameraManager->SetGameCameraCutThisFrame(); F.PC->PlayerCameraManager->UpdateCamera(0);
 }
 void Shot(const TCHAR* Name)
 {
  Capture(F.W,F.PC,Name);
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
  {
   F.W->GetGameViewport()->Viewport->Draw(false);
   Test->TestTrue(TEXT("Search-light actual HUD/world still captured"),FNeonCityVisualEvidence::CaptureHUD(F.W,Name));
  }
  F.PC->SetViewTarget(Heli);if(InspectionCamera)InspectionCamera->Destroy();InspectionCamera=nullptr;
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>160) { Test->AddError(FString::Printf(TEXT("Search light scenario timeout stage %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(F.W);It;++It) { Heli=*It; Light=It->FindComponentByClass<UHelicopterSearchLightComponent>(); break; }
   for(TActorIterator<ANeonCityDayNightCycle> It(F.W);It;++It) { Cycle=*It; break; }
   if(!Light || !Heli || !Cycle) { Test->AddError(TEXT("Helicopter or day/night fixture missing")); return true; }
   Cycle->SetTimeOfDay(22);
   Home=Heli->GetActorLocation();
   F.Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   F.Player->SetActorLocation(Heli->GetActorLocation()+FVector(0,-315,10),false,nullptr,ETeleportType::TeleportPhysics);
   F.Player->GetCharacterMovement()->StopMovementImmediately();
   if(!Heli->GetAircraftInteraction()->Enter(F.Player)) { Test->AddError(TEXT("Helicopter entry failed")); return true; }
   return Next(1);
  }
  const float E=F.W->GetTimeSeconds()-Phase;
  Evidence.Record(F.W,F.PC,Stage);
  if(Stage==1 && E>.3f) { Key(F.PC,EKeys::L,IE_Pressed); return Next(2); }
  if(Stage==2 && E>.2f)
  {
   Key(F.PC,EKeys::L,IE_Released);
   if(!Test->TestTrue(TEXT("L enables possessed helicopter spot light"),Light->IsEnabled() && Light->GetLight() && Light->GetLight()->IsVisible())) return true;
   Test->TestTrue(TEXT("Near light uses real volumetric shadows and bounded quality rate"),Light->IsHighQuality() && Light->GetLight()->CastShadows && Light->GetLight()->bCastVolumetricShadow && Light->GetUpdateInterval()<=.1f);
   int32 FogCount=0;
   for(TActorIterator<AExponentialHeightFog> It(F.W);It;++It)
    if(const auto* Fog=It->GetComponent())
     if(Fog->bEnableVolumetricFog && Fog->VolumetricFogDistance<=12000 && Fog->FogDensity>0)++FogCount;
   Test->TestTrue(TEXT("Normal map supplies one bounded real scattering medium"),FogCount==1 && Light->GetLight()->VolumetricScatteringIntensity>0);
   auto* Housing=Light->GetHousing();
   if(!Test->TestNotNull(TEXT("Optical housing exists on ordinary map helicopter"),Housing))return true;
   Test->TestTrue(TEXT("Housing preserves original gameplay collision and mass"),Housing->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Housing->IsSimulatingPhysics());
   Test->TestEqual(TEXT("Housing uses authored helicopter bracket"),Housing->GetAttachSocketName(),FName(TEXT("SearchlightMount")));
   Test->TestTrue(TEXT("Light starts at real optical exit, not old floating root offset"),Light->GetLight()->GetComponentLocation().Equals(Housing->GetSocketLocation(TEXT("LightEmission")),.01));
   auto* Lens=Cast<UMaterialInstanceDynamic>(Housing->GetMaterial(Housing->GetMaterialIndex(TEXT("M_NC205B_WhiteLight"))));
   Test->TestTrue(TEXT("Enabled lens matches actual enabled light"),Lens && Lens->K2_GetScalarParameterValue(TEXT("LightEnabled"))>.9f);
   NightIntensity=Light->GetLight()->Intensity; Start=Heli->GetActorLocation(); Key(F.PC,EKeys::SpaceBar,IE_Pressed); return Next(3);
  }
  if(Stage==3 && E>4)
  {
   Test->TestTrue(TEXT("Pilot sees the actual search-light state"),Heli->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("Searchlight ON")));
   Key(F.PC,EKeys::SpaceBar,IE_Released); Test->TestTrue(TEXT("Night takeoff with light gains height"),Heli->GetActorLocation().Z>Start.Z+1000);
   Start=Heli->GetActorLocation(); Key(F.PC,EKeys::W,IE_Pressed); return Next(4);
  }
  if(Stage==4 && E>2)
  {
   Key(F.PC,EKeys::W,IE_Released); Test->TestTrue(TEXT("Lit helicopter flies over South Beach"),FVector::Dist2D(Start,Heli->GetActorLocation())>400);
   return Next(5);
  }
  if(Stage==5 && E>1)
  {
   FHitResult Hit; const FVector Origin=Light->GetLight()->GetComponentLocation();
   FCollisionQueryParams Query(SCENE_QUERY_STAT(SearchLightAcceptance),false,Heli); Query.AddIgnoredActor(F.Player);
   Test->TestTrue(TEXT("Search beam reaches real city geometry within range"),F.W->LineTraceSingleByChannel(Hit,Origin,Origin+Light->GetLight()->GetForwardVector()*Light->GetProfile()->Range,ECC_Visibility,Query));
   BeginInspectionView();return Next(50);
  }
  if(Stage==50 && E>.8f)
  {
   Shot(TEXT("D-SearchLightNight.png"));
   F.PC->SetControlRotation(Heli->GetActorRotation()+FRotator(55,70,0)); return Next(6);
  }
  if(Stage==6 && E>1)
  {
   const FRotator Relative=Light->GetAimRotation();
   Test->TestTrue(TEXT("Independent aim rotates within bounded offset"),Relative.Yaw>5 && Relative.Yaw<=25.1f && Relative.Pitch>=-75.1f && Relative.Pitch<=-34.9f);
   Test->TestTrue(TEXT("Aiming keeps real light aligned with rotated lens"),Light->GetLight()->GetForwardVector().Equals(Light->GetHousing()->GetForwardVector(),.001));
   Test->TestTrue(TEXT("Aiming preserves emitter contact"),Light->GetLight()->GetComponentLocation().Equals(Light->GetHousing()->GetSocketLocation(TEXT("LightEmission")),.01));
   BeginInspectionView();return Next(51);
  }
  if(Stage==51 && E>.8f)
  {
   Shot(TEXT("D-SearchLightAim.png"));
   F.PC->SetControlRotation(Heli->GetActorRotation());Key(F.PC,EKeys::W,IE_Pressed);return Next(60);
  }
  if(Stage==60 && Heli->GetActorLocation().Y<-1000)
  { Key(F.PC,EKeys::W,IE_Released);return Next(61); }
  if(Stage==61 && E>2)
  {
   Test->TestTrue(TEXT("Beach reached by powered flight with searchlight on"),Heli->GetActorLocation().Y<-1000 && Light->IsEnabled());
   BeginInspectionView();return Next(62);
  }
  if(Stage==62 && E>1)
  { Shot(TEXT("D-SearchLightBeach.png"));Key(F.PC,EKeys::W,IE_Pressed);return Next(63); }
  if(Stage==63 && Heli->GetActorLocation().Y<-5500)
  { Key(F.PC,EKeys::W,IE_Released);return Next(64); }
  if(Stage==64 && E>2)
  {
   float WaterHeight;FVector Flow;
   Test->TestTrue(TEXT("Powered flight reaches authored ocean water"),F.W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Heli->GetActorLocation(),WaterHeight,Flow));
   BeginInspectionView();return Next(65);
  }
  if(Stage==65 && E>1)
  {
   Shot(TEXT("D-SearchLightOceanOn.png"));Key(F.PC,EKeys::L,IE_Pressed);return Next(66);
  }
  if(Stage==66 && E>.3f)
  {
   Key(F.PC,EKeys::L,IE_Released);Test->TestFalse(TEXT("Ocean comparison uses real L off"),Light->IsEnabled());
   Test->TestTrue(TEXT("Pilot sees searchlight off"),Heli->GetFlightStatus()->GetDisplayedStatus().ToString().Contains(TEXT("Searchlight OFF")));
   BeginInspectionView();return Next(67);
  }
  if(Stage==67 && E>1)
  {
   Shot(TEXT("D-SearchLightOceanOff.png"));Key(F.PC,EKeys::L,IE_Pressed);Key(F.PC,EKeys::V,IE_Pressed);return Next(68);
  }
  if(Stage==68 && E>.3f)
  {
   Key(F.PC,EKeys::L,IE_Released);Key(F.PC,EKeys::V,IE_Released);
   auto* Combat=Heli->FindComponentByClass<UVehicleCombatComponent>();
   if(!Test->TestTrue(TEXT("Normal loadout armed while light enabled"),Combat && Combat->IsArmed() && Light->IsEnabled()))return true;
   BeforeAmmo=Combat->GetManager()->GetWeapon()->GetAmmo();Key(F.PC,EKeys::LeftMouseButton,IE_Pressed);return Next(69);
  }
  if(Stage==69 && E>.35f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released);
   Test->TestTrue(TEXT("Real mounted fire consumes ammunition without turning off searchlight"),Heli->FindComponentByClass<UVehicleCombatComponent>()->GetManager()->GetWeapon()->GetAmmo()<BeforeAmmo && Light->IsEnabled() && Light->GetLight()->IsVisible());
   Cycle->SetTimeOfDay(12);return Next(7);
  }
  if(Stage==7 && E>.3f)
  {
   Test->TestTrue(TEXT("Daytime automatically reduces intensity"),Light->GetLight()->Intensity<NightIntensity*.2f);
   Key(F.PC,EKeys::L,IE_Pressed); return Next(8);
  }
  if(Stage==8 && E>.2f)
  {
   Key(F.PC,EKeys::L,IE_Released); Test->TestFalse(TEXT("L disables light during flight"),Light->IsEnabled() || Light->GetLight()->IsVisible());
   auto* Lens=Cast<UMaterialInstanceDynamic>(Light->GetHousing()->GetMaterial(Light->GetHousing()->GetMaterialIndex(TEXT("M_NC205B_WhiteLight"))));
   Test->TestTrue(TEXT("Disabled lens does not falsely glow"),Lens && Lens->K2_GetScalarParameterValue(TEXT("LightEnabled"))==0);
   // A distant spectator exercises quality culling without moving the aircraft.
   FarCamera=F.W->SpawnActor<ACameraActor>(Heli->GetActorLocation()+FVector(30000,0,0),FRotator::ZeroRotator);
   F.PC->SetViewTarget(FarCamera); F.PC->PlayerCameraManager->UpdateCamera(0); Light->SetEnabled(true); return Next(9);
  }
  if(Stage==9 && E>1.2f)
  {
   Test->TestTrue(TEXT("Distant light drops shadows, culls and updates at one Hz"),!Light->IsHighQuality() && !Light->GetLight()->CastShadows && !Light->GetLight()->IsVisible() && Light->GetUpdateInterval()>=1);
   Light->ResetForLoad(); Test->TestFalse(TEXT("Load reset clears transient enabled state"),Light->IsEnabled());
   Test->TestTrue(TEXT("Load reset returns the optical body to neutral aim"),Light->GetAimRotation().Equals(FRotator(-55,0,0),.01));
   F.PC->SetViewTarget(Heli); FarCamera->Destroy();return Next(70);
  }
  if(Stage==70)
  {
   // Disclosed return pilot uses the existing force-input port only. No hull
   // transform, velocity, gravity or time-scale writes hide the return/landing.
   const FVector Offset=FVector(Home.X,Home.Y,Heli->GetActorLocation().Z)-Heli->GetActorLocation();
   const FVector V=Heli->GetVelocity(),Flat(V.X,V.Y,0);
   const FVector Desired=Offset.GetClampedToMaxSize(1400)*.8f;
   const FVector Accel=(Desired-Flat)*1.6f+Flat*Heli->GetProfile()->AirResistance;
   const FRotator Heading(0,Heli->GetActorRotation().Yaw,0);
   FVector Command=Heading.UnrotateVector(Accel)/(Heli->GetProfile()->Acceleration*100);Command.Z=0;
   Heli->SubmitFlightInput_Implementation(F.PC,Command,0);
   if(Offset.Size()<20 && Flat.Size()<25)
   { Heli->SubmitFlightInput_Implementation(F.PC,FVector::ZeroVector,0);Key(F.PC,EKeys::C,IE_Pressed);return Next(71); }
   if(E>40) { Test->AddError(TEXT("Searchlight flight failed physical return to original pad"));return true; }
  }
  if(Stage==71)
  {
   if(Heli->GetAircraftMovement()->IsGrounded())
   {
    if(GroundAt==0)GroundAt=F.W->GetTimeSeconds();
    if(F.W->GetTimeSeconds()-GroundAt<.8f)return false;
    Key(F.PC,EKeys::C,IE_Released);Key(F.PC,EKeys::F,IE_Pressed);return Next(72);
   }
   if(E>25) { Test->AddError(TEXT("Searchlight route failed physical landing"));return true; }
  }
  if(Stage==72 && E>.8f)
  {
   Key(F.PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("Night-flight route exits with character control and visible collision"),F.PC->GetPawn()==F.Player && !F.Player->IsHidden() && F.Player->GetActorEnableCollision());
   Test->TestTrue(TEXT("Return and landing preserve airframe health"),Heli->GetHealthComponent()->GetCurrentHealth()==600);
   Test->TestFalse(TEXT("Pilot HUD is removed after exit"),Heli->GetFlightStatus()->IsVisible());return true;
  }
  return false;
 }
 FAutomationTestBase* Test; int32 Stage=0; double Started=0,Phase=0; float NightIntensity=0; FVector Start; FScene F; ANeonCityAircraftVehicleBase* Heli=nullptr; UHelicopterSearchLightComponent* Light=nullptr; ANeonCityDayNightCycle* Cycle=nullptr; ACameraActor* FarCamera=nullptr;
 FNeonCityVisualEvidence Evidence{TEXT("H-SearchLight")};
 ACameraActor* InspectionCamera=nullptr;
 FVector Home;int32 BeforeAmmo=0;float GroundAt=0;
};
}

#define WATER_TEST(Class,Name,Kind) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand()); FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWaterLightingTests::FWaterScenario>(this,Kind)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
WATER_TEST(FWaterSwimming,"NeonCity.Water.SwimmingPIE",0)
WATER_TEST(FWaterDrowning,"NeonCity.Water.DrowningPIE",1)
WATER_TEST(FWaterShore,"NeonCity.Water.ShorePIE",2)
WATER_TEST(FWaterRestore,"NeonCity.Water.PersistencePIE",3)
#undef WATER_TEST
class FLandVehicleScenario : public IAutomationLatentCommand
{
public:
 explicit FLandVehicleScenario(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Land vehicle water scenario timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) { Car=*It; break; }
   if(!Car) { Test->AddError(TEXT("Land vehicle fixture missing")); return true; }
   F.Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   F.Player->SetActorLocation(Car->GetActorLocation()+FVector(0,220,100),false,nullptr,ETeleportType::TeleportPhysics);
   F.Player->GetCharacterMovement()->StopMovementImmediately();
   if(!Test->TestTrue(TEXT("Driver enters before sinking"),Car->GetVehicleInteraction()->Enter(F.Player))) return true;
   Car->SetActorLocation(FVector(1000,-14500,-350),false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  if(Stage==1 && F.W->GetTimeSeconds()-Phase>1)
  {
   Test->TestEqual(TEXT("Land vehicle engine stalls in deep water"),Car->GetDrivingComponent()->GetEngineState(),ENeonCityEngineState::Stalled);
   NeonCityWaterLightingTests::Key(F.PC,EKeys::F,IE_Pressed); Phase=F.W->GetTimeSeconds(); Stage=2; return false;
  }
  if(Stage==2 && F.W->GetTimeSeconds()-Phase>.25f)
  {
   NeonCityWaterLightingTests::Key(F.PC,EKeys::F,IE_Released);
   Test->TestTrue(TEXT("F escapes sinking car into native swimming"),F.PC->GetPawn()==F.Player && F.Player->GetCharacterMovement()->IsSwimming() && !F.Player->IsHidden() && F.Player->GetActorEnableCollision());
   NeonCityWaterLightingTests::Capture(F.W,F.PC,TEXT("F-VehicleEscape.png")); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; double Started=0,Phase=0; int32 Stage=0; NeonCityWaterLightingTests::FScene F; ANeonCityBaseVehicle* Car=nullptr;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaterLandVehicle,"NeonCity.Water.LandVehiclePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWaterLandVehicle::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FLandVehicleScenario>(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHelicopterSearchLight,"NeonCity.Aircraft.SearchLightPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHelicopterSearchLight::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand()); FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWaterLightingTests::FSearchLightScenario>(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
