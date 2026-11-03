#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Water/CoastalOceanSurface.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Engine/GameInstance.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "Property/NeonCityStarterApartment.h"
#include "Interaction/InteractionComponent.h"
#include "InputKeyEventArgs.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "Editor.h"

namespace
{
// Opens the shipping default map and uses its actual startup. No spawned island,
// fixture water, actor teleport, hidden terrain or alternative test map.
class FPlayerFeedbackWorldEntry : public IAutomationLatentCommand
{
public:
 explicit FPlayerFeedbackWorldEntry(FAutomationTestBase* InTest, bool InVoyage = false)
  : Test(InTest), bVoyage(InVoyage), Evidence(InVoyage?TEXT("NormalHomeToIsland"):TEXT("NormalHomeToShore")) {}
 bool Update() override
 {
  if (!Started) Started = FPlatformTime::Seconds();
  if (FPlatformTime::Seconds() - Started > (bVoyage ? 600 : 150))
  {
   Test->AddError(TEXT("Normal South Beach world audit timed out"));
   return true;
  }
  UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
  auto* Player = PC ? Cast<ANeonCityPlayerCharacter>(PC->GetPawn()) : nullptr;
  if (Stage >= 10 && World && PC) return Voyage(World, PC);
  if (!Player || World->GetTimeSeconds() < 12) return false;
  if (Stage > 0) return WalkToShore(World, PC, Player);
  int32 Islands = 0, FishingBoats = 0;
  for (TActorIterator<AActor> It(World); It; ++It)
  {
   if (It->ActorHasTag(TEXT("NeonCity.Tidewater.Island")))
   {
    ++Islands;
    Test->AddInfo(FString::Printf(TEXT("Actual island: %s at %s; hidden=%d"),
     *It->GetPathName(), *It->GetActorLocation().ToString(), It->IsHidden()));
   }
   if (It->IsA<ANeonCityFishingBoat>())
   {
    ++FishingBoats;
    Test->AddInfo(FString::Printf(TEXT("Actual fishing boat: %s at %s"),
     *It->GetPathName(), *It->GetActorLocation().ToString()));
   }
  }
  Test->AddInfo(FString::Printf(TEXT("Normal world=%s; partitioned=%d; levels=%d; streams=%d; islands=%d; fishing boats=%d; player=%s"),
   *World->GetMapName(), World->IsPartitionedWorld(), World->GetLevels().Num(),
   World->GetStreamingLevels().Num(), Islands, FishingBoats, *Player->GetActorLocation().ToString()));
  for (auto* Level : World->GetStreamingLevels()) if (Level)
   Test->AddInfo(FString::Printf(TEXT("Stream %s loaded=%d visible=%d"),
    *Level->GetWorldAssetPackageName(), Level->IsLevelLoaded(), Level->IsLevelVisible()));
  Test->TestTrue(TEXT("Real configured South Beach remains the persistent world"),
   World->GetMapName().EndsWith(TEXT("NeonCity_SouthBeach_Test")));
  Test->TestEqual(TEXT("PF-007: actual island is present in normal South Beach world"), Islands, 1);
  Test->TestEqual(TEXT("PF-006: one fishing boat, no duplicate test placement"), FishingBoats, 1);
  if (const auto* Coast = ANeonCityCoastalRegion::Find(World))
  {
   auto* Marine = World->GetSubsystem<UMarineWorldSubsystem>();
   auto Legacy = Marine->BuildPersistenceSnapshot(true);
   auto* Record = Legacy.FindByPredicate([](const auto& R){return R.VehicleID == UMarineWorldSubsystem::HarborlineID();});
   if (Test->TestNotNull(TEXT("Island berth has a stable registered save identity"), Record))
   {
    const FTransform IslandBerth = Record->Transform;
    Record->Transform.SetLocation(FVector(6000,-4700,-130));
    Record->AuthoredBerthVersion = 0; Record->Owned = false; Record->Condition = .43f;
    TArray<FString> Warnings;
    Marine->NormalizePersistenceSnapshot(Legacy, Warnings);
    Record = Legacy.FindByPredicate([](const auto& R){return R.VehicleID == UMarineWorldSubsystem::HarborlineID();});
    if (Test->TestNotNull(TEXT("Legacy berth record survives normalization"), Record))
    {
     Test->TestTrue(TEXT("Untouched legacy prototype berth migrates to island"), Record->Transform.Equals(IslandBerth));
     Test->TestEqual(TEXT("Berth migration preserves condition"), Record->Condition, .43f);
     Test->TestFalse(TEXT("Berth migration preserves ownership"), Record->Owned);
     Record->Transform.SetLocation(FVector(6400,-5200,-130)); Record->AuthoredBerthVersion = 0;
     Marine->NormalizePersistenceSnapshot(Legacy, Warnings);
     Record = Legacy.FindByPredicate([](const auto& R){return R.VehicleID == UMarineWorldSubsystem::HarborlineID();});
     Test->TestTrue(TEXT("Player-selected legacy position is not reset"), Record && Record->Transform.GetLocation().Equals(FVector(6400,-5200,-130)));
    }
    Test->TestTrue(TEXT("Canonical authored berth belongs to connected island"), IslandBerth.Equals(Coast->GetFishingBerth()));
   }
  }
  FNeonCityVisualEvidence::CaptureHUD(World, TEXT("PF-007-NormalStartup.png"));
  if (Islands != 1) return true;
  Stage = 1;
  Walker = Player;
  PhaseStarted = World->GetTimeSeconds();
  return false;
 }
private:
 bool WalkToShore(UWorld* World, APlayerController* PC, ANeonCityPlayerCharacter* Player)
 {
  Evidence.Record(World, PC, Stage);
  auto Key = [&](EInputEvent Event)
  { PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::F, Event, FPlatformTime::Cycles64())); };
  auto Walk = [&](FVector Destination)
  {
   const FVector Delta = Destination - Player->GetActorLocation();
   if (Delta.Size2D() < 18) return true;
   PC->SetControlRotation(FRotator(-5, Delta.Rotation().Yaw, 0));
   Player->DoMove(0, FMath::Clamp(Delta.Size2D()/140., .18, .75));
   return false;
  };
  if (World->GetTimeSeconds() - PhaseStarted > 30)
  {
   Test->AddError(FString::Printf(TEXT("Normal home-to-shore walk blocked at stage %d, location %s"), Stage, *Player->GetActorLocation().ToString()));
   FNeonCityVisualEvidence::CaptureHUD(World, TEXT("PF-007-RouteBlocked.png"));
   return true;
  }
  if (Stage == 1)
  {
   if (!Walk(FVector(4100, 1790, 0))) return false;
   FVector Eye; FRotator Rotation; Player->GetActorEyesViewPoint(Eye, Rotation);
   PC->SetControlRotation((FVector(4100, 1550, 170) - Eye).Rotation());
   auto* Focus = Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable();
   if (!Test->TestTrue(TEXT("Normal F prompt reaches the home exit"), Focus && Focus->IsA<ANeonCityStarterApartment>())) return true;
   Key(IE_Pressed); Stage = 2; PhaseStarted = World->GetTimeSeconds(); return false;
  }
  if (Stage == 2)
  {
   Key(IE_Released);
   if (World->GetTimeSeconds() - PhaseStarted < .6f) return false;
   if (!Test->TestTrue(TEXT("Normal home interaction exits to the street"), Player->GetActorLocation().Y < 1500)) return true;
   Stage = 3; PhaseStarted = World->GetTimeSeconds(); return false;
  }
  const FVector Route[] = {FVector(3450,1280,0), FVector(3450,-950,0), FVector(5200,-960,0),
   FVector(5200,-1450,0), FVector(5200,-2150,0), FVector(5200,-3100,0)};
  if (Stage >= 3 && Stage < 3 + UE_ARRAY_COUNT(Route))
  {
   if (!Walk(Route[Stage-3])) return false;
   ++Stage; PhaseStarted = World->GetTimeSeconds(); return false;
  }
  const auto* Coast = ANeonCityCoastalRegion::Find(World);
  if (!Test->TestNotNull(TEXT("Authored coastal connection is present"), Coast)) return true;
  const FVector Peak = Coast->GetActorLocation() + FVector(-3500,51000,29332);
  PC->SetControlRotation(FRotator(2, (Peak-Player->GetActorLocation()).Rotation().Yaw, 0));
  if (World->GetTimeSeconds() - PhaseStarted < 5) return false;
  FVector Eye; FRotator Rotation; Player->GetActorEyesViewPoint(Eye, Rotation);
  FHitResult Hit;
  World->LineTraceSingleByChannel(Hit, Eye, Peak + FVector(0,0,-100), ECC_Visibility,
   FCollisionQueryParams(SCENE_QUERY_STAT(CoastalIslandSightline), false, Player));
  Test->TestTrue(TEXT("Island terrain has a real unobstructed shore sightline"), Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("NeonCity.Tidewater.Island")));
  Test->TestTrue(TEXT("Player reached shore using ordinary walking"), Player->GetCharacterMovement()->IsMovingOnGround());
  FNeonCityVisualEvidence::CaptureHUD(World, TEXT("PF-007-IslandFromShore.png"));
  Test->AddInfo(TEXT("Automated normal-game route: spawn at home, F exit, walk streets to shoreline. No fixture teleport. Visual/manual acceptance separate."));
  if (bVoyage)
  {
   for (TActorIterator<ANeonCityMarineVehicleBase> It(World); It; ++It)
    if (It->VehicleID == UMarineWorldSubsystem::SpeedboatID()) Speedboat = *It;
   if (!Test->TestNotNull(TEXT("Existing mainland speedboat"), Speedboat.Get())) return true;
   Stage = 10; PhaseStarted = World->GetTimeSeconds(); return false;
  }
  return true;
 }
 bool Voyage(UWorld* World, APlayerController* PC)
 {
  auto* Player = Walker.Get(); auto* Boat = Speedboat.Get();
  if (!Player || !Boat) { Test->AddError(TEXT("Voyage lost its real player or speedboat")); return true; }
  Evidence.Record(World, PC, Stage);
  const float Elapsed = World->GetTimeSeconds() - PhaseStarted;
  auto Key = [&](FKey K, EInputEvent E) { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); };
  auto Held = [&](FKey K, bool Down) { if (Keys.Contains(K) == Down) return; Key(K,Down?IE_Pressed:IE_Released); if (Down) Keys.Add(K); else Keys.Remove(K); };
  auto Next = [&](int32 N) { Stage=N; PhaseStarted=World->GetTimeSeconds(); return false; };
  auto Walk = [&](FVector Goal) {
   const FVector Delta=Goal-Player->GetActorLocation(); if(Delta.Size2D()<18) return true;
   PC->SetControlRotation(FRotator(-5,Delta.Rotation().Yaw,0)); Player->DoMove(0,FMath::Clamp(Delta.Size2D()/140.,.18,.75)); return false;
  };
  auto Fail = [&](const FString& Why) {
   const auto Copy=Keys; for(const auto& K:Copy) Held(K,false);
   Test->AddError(FString::Printf(TEXT("%s; stage %d, player %s, boat %s yaw %.1f speed %.1f"),*Why,Stage,*Player->GetActorLocation().ToString(),*Boat->GetActorLocation().ToString(),Boat->GetActorRotation().Yaw,Boat->GetSpeed_Implementation()));
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("PF-009-VoyageBlocked.png")); return true;
  };
  if (Elapsed>180) return Fail(TEXT("Continuous coastal route timed out"));
  if(Stage==10)
  {
   if(!Walk(FVector(5080,-3310,0))) return false;
   return Next(11);
  }
  if(Stage==11)
  {
   FVector Eye;FRotator R;Player->GetActorEyesViewPoint(Eye,R);
   PC->SetControlRotation((Boat->GetActorLocation()+FVector(0,0,40)-Eye).Rotation());
   if(Elapsed<.5f)return false;
   if(Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()!=Boat) return Fail(TEXT("Normal F prompt cannot reach speedboat from the pier"));
   Key(EKeys::F,IE_Pressed);return Next(12);
  }
  if(Stage==12)
  {
   Key(EKeys::F,IE_Released);if(Elapsed<.6f)return false;
   if(PC->GetPawn()!=Boat)return Fail(TEXT("Ordinary F boarding did not take the helm"));
   const auto* Coast=ANeonCityCoastalRegion::Find(World);if(!Coast)return Fail(TEXT("Missing connected island"));
   IslandOrigin=Coast->GetActorLocation();StartHealth=Boat->GetHealthComponent()->GetCurrentHealth();
   // West-side passage reviewed against the pinned source heightfield: each
   // segment has >4m depth and 20m lateral clearance from shallow terrain.
   // Inputs alone drive Chaos; no set-transform, replacement water or fixture boats.
   SeaRoute={FVector(4860,-8000,0),NeonCityCoastalLayout::FromLegacyIsland(FVector(-23000,-115000,0)),NeonCityCoastalLayout::FromLegacyIsland(FVector(-23000,-142000,0)),
    NeonCityCoastalLayout::FromLegacyIsland(FVector(-14000,-155000,0)),NeonCityCoastalLayout::FromLegacyIsland(FVector(2000,-191000,0)),NeonCityCoastalLayout::FromLegacyIsland(FVector(14000,-209000,0)),
    NeonCityCoastalLayout::FromLegacyIsland(FVector(30000,-210000,0)),IslandOrigin+FVector(6300,-7500,0)};
   Held(EKeys::W,true);return Next(13);
  }
  if(Stage==13)
  {
   const FVector Delta=SeaRoute[SeaPoint]-Boat->GetActorLocation();
   const float Angle=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Delta.Rotation().Yaw);
   Held(EKeys::D,Angle>2);Held(EKeys::A,Angle<-2);Held(EKeys::W,true);
   auto* Water=World->GetSubsystem<UMarineWorldSubsystem>();
   ACoastalOceanSurface* Ocean=nullptr;
   for(TActorIterator<ACoastalOceanSurface> It(World);It;++It){Ocean=*It;break;}
   if(!Ocean || !Ocean->IsReady())return Fail(TEXT("Continuous voyage lost the active ocean"));
   const float Now=World->GetTimeSeconds();
   if(Ocean->GetQuerySequence()!=LastWaterSequence){LastWaterSequence=Ocean->GetQuerySequence();LastWaterTime=Now;}
   if(Now-LastWaterTime>.5f)return Fail(TEXT("Ocean query readback stopped advancing"));
   float MinimumClearance=BIG_NUMBER;int32 PointIndex=0;
   const FVector Extent=Boat->GetProfile()->HullExtent;
   for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})
   {
    const FVector Point=Boat->GetHull()->GetComponentTransform().TransformPosition(FVector(X*Extent.X*.8f,Y*Extent.Y*.85f,-Extent.Z));
    FWaterSurfaceSample Surface;float Height=0;FVector Normal,Velocity;
    if(!Water->QueryWater(Point,Surface) || !Ocean->SampleBody(Boat->GetHull(),PointIndex++,Point,Height,Normal,Velocity))
     return Fail(TEXT("Continuous voyage lost a live named hull-water query"));
    MinimumClearance=FMath::Min(MinimumClearance,float(Point.Z-Height));
   }
   // IsAfloat counts immersed hull points, not available water providers.
   // A real crest may briefly put all four points in air. Require live queries,
   // a bounded hop and recovery rather than forcing the boat onto the surface.
   if(!Boat->GetMarineMovement()->IsAfloat())
   {
    if(AirborneSince<0){AirborneSince=Now;++AirborneEpisodes;}
    LongestHop=FMath::Max(LongestHop,Now-AirborneSince);
    HighestClearance=FMath::Max(HighestClearance,MinimumClearance);
    if(Now-AirborneSince>1.25f || MinimumClearance>150 || Boat->GetActorUpVector().Z<.55)
     return Fail(FString::Printf(TEXT("Unstable wave departure: %.3f s, minimum hull clearance %.2f cm, up %.3f"),Now-AirborneSince,MinimumClearance,Boat->GetActorUpVector().Z));
   }
   else AirborneSince=-1;
   if(Boat->GetHealthComponent()->GetCurrentHealth()<StartHealth-1)return Fail(TEXT("Island voyage collided or damaged the boat"));
   if(Delta.Size2D()<1600)
   {
    Test->AddInfo(FString::Printf(TEXT("Coastal waypoint %d reached by W/A/D, speed %.1f km/h"),SeaPoint,Boat->GetSpeed_Implementation()*.036f));
    FNeonCityVisualEvidence::CaptureHUD(World,FString::Printf(TEXT("PF-009-Approach-%d.png"),SeaPoint));
    ++SeaPoint;PhaseStarted=World->GetTimeSeconds();
    if(SeaPoint==SeaRoute.Num())
    {Held(EKeys::W,false);Held(EKeys::A,false);Held(EKeys::D,false);Held(EKeys::SpaceBar,true);return Next(14);}
   }
   return false;
  }
  if(Stage==14)
  {
   if(Boat->GetSpeed_Implementation()>50)return false;
   Held(EKeys::SpaceBar,false);Key(EKeys::F,IE_Pressed);return Next(15);
  }
  if(Stage==15)
  {
   Key(EKeys::F,IE_Released);if(Elapsed<.8f)return false;
   if(PC->GetPawn()!=Player)return Fail(TEXT("Cannot safely leave stopped speedboat"));
   Test->TestTrue(TEXT("Normal coastal voyage retains player health"),Player->GetHealthComponent()->GetCurrentHealth()>=99);
   Test->TestTrue(TEXT("Stopped boat regained physical water support"),Boat->GetMarineMovement()->IsAfloat());
   Test->AddInfo(FString::Printf(TEXT("Continuous live hull queries; %d brief wave departures, longest %.3f s, highest full-hull clearance %.2f cm"),AirborneEpisodes,LongestHop,HighestClearance));
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("PF-009-IslandArrival.png"));
   Test->AddInfo(TEXT("Continuous automated home-to-island approach via ordinary walking/F/WASD/Space; docking and shore landing require their own acceptance."));
   return true;
  }
  return false;
 }
 FAutomationTestBase* Test;
 double Started = 0;
 int32 Stage = 0;
 float PhaseStarted = 0;
 bool bVoyage = false;
 TWeakObjectPtr<ANeonCityPlayerCharacter> Walker;
 TWeakObjectPtr<ANeonCityMarineVehicleBase> Speedboat;
 TSet<FKey> Keys;
 TArray<FVector> SeaRoute;
 FVector IslandOrigin;
 int32 SeaPoint = 0;
 float StartHealth = 0;
 uint64 LastWaterSequence=0;
 float LastWaterTime=0,AirborneSince=-1,LongestHop=0,HighestClearance=0;
 int32 AirborneEpisodes=0;
 FNeonCityVisualEvidence Evidence;
};

// Focused persistence fixture, deliberately separate from the no-teleport voyage.
// Exercises actual SaveGame disk IO after the destination level was unloaded.
class FCoastalRestore : public IAutomationLatentCommand
{
public:
 explicit FCoastalRestore(FAutomationTestBase* In) : Test(In) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100){Test->AddError(TEXT("Coastal save restore timeout"));return true;}
  auto* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  auto* Coast=ANeonCityCoastalRegion::Find(World);
  if(!Player || !Coast || !Coast->IsInitialRegionReady() || World->GetTimeSeconds()<2)return false;
  auto* Save=World->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  if(Stage==0)
  {
   Mainland=Player->GetActorTransform();
   Slot=TEXT("NeonCity_Test_Coastal_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
   if(!Test->TestTrue(TEXT("Dedicated isolated coastal test slot"),Save->ConfigureSlot(Slot,false)))return true;
   const FVector Start=Coast->GetActorLocation()+FVector(5500,-3510,230+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4);
   Player->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Player->GetCharacterMovement()->StopMovementImmediately();At=World->GetTimeSeconds();Stage=1;return false;
  }
  if(World->GetTimeSeconds()-At<1)return false;
  if(Stage==1)
  {
   if(!Test->TestTrue(TEXT("Authored island pier supports saved character"),Player->GetCharacterMovement()->IsMovingOnGround()))return true;
   SavedLocation=Player->GetActorLocation();
   if(!Test->TestTrue(TEXT("Write island destination to disk"),Save->SaveGame()))return true;
   Player->SetActorTransform(Mainland,false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   for(auto* Level:World->GetStreamingLevels())if(Level && Level->GetWorldAssetPackageName().EndsWith(TEXT("NeonCity_Tidewater_Island")))Stream=Level;
   if(!Test->TestNotNull(TEXT("Real authored island streaming entry"),Stream))return true;
   Stream->SetShouldBeVisible(false);Stream->SetShouldBeLoaded(false);
   World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
   Test->TestFalse(TEXT("Fixture actually unloads destination collision"),Stream->IsLevelLoaded());
   if(!Test->TestTrue(TEXT("Disk load requests island before checking saved floor"),Save->LoadGame()))return true;
   Test->TestTrue(TEXT("Destination level restored before player relocation"),Stream->IsLevelVisible());
   Test->TestTrue(TEXT("Saved island position retained instead of default apartment fallback"),Player->GetActorLocation().Equals(SavedLocation,5));
   At=World->GetTimeSeconds();Stage=2;return false;
  }
  Test->TestTrue(TEXT("Restored character remains supported on island pier"),Player->GetCharacterMovement()->IsMovingOnGround());
  int32 RestoredSurfaces=0;
  for(TActorIterator<ANeonCityReferenceVillage> It(World);It;++It)
  {
   TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
   for(auto* Mesh:Meshes)if(Mesh->GetName().StartsWith(TEXT("VillageSurface_")))
   {
    ++RestoredSurfaces;
    const auto* Material=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
    Test->TestTrue(TEXT("Reloaded village keeps a valid authored material parent"),
     Material && Material->Parent && !Material->Parent->IsA<UMaterialInstanceDynamic>()
     && Material->Parent->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/Tidewater/Village/")));
   }
  }
  Test->TestEqual(TEXT("All village visual parts survive island restore"),RestoredSurfaces,31);
  if(auto* Model=Save->ReadSaveForInspection())
  {
   const auto* Boat=Model->Marine.FindByPredicate([](const auto& R){return R.VehicleID==UMarineWorldSubsystem::HarborlineID();});
   Test->TestTrue(TEXT("Disk generation includes stable fishing boat and berth revision"),Boat && Boat->AuthoredBerthVersion==1);
  }
  else Test->AddError(TEXT("Could not inspect written coastal generation"));
  Test->TestTrue(TEXT("Delete only this isolated test save"),Save->DeleteSave());
  Test->TestFalse(TEXT("Deleted isolated save no longer exists"),Save->DoesSaveExist());
  return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;float At=0;int32 Stage=0;
 FTransform Mainland;FVector SavedLocation;FString Slot;ULevelStreaming* Stream=nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerFeedbackWorldEntryTest,
 "NeonCity.Quality.NormalWorldEntryPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerFeedbackWorldEntryTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPlayerFeedbackWorldEntry>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerFeedbackCoastalVoyageTest,
 "NeonCity.Quality.CoastalVoyagePIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerFeedbackCoastalVoyageTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPlayerFeedbackWorldEntry>(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerFeedbackCoastalRestoreTest,
 "NeonCity.Quality.CoastalRestorePIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerFeedbackCoastalRestoreTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCoastalRestore>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
