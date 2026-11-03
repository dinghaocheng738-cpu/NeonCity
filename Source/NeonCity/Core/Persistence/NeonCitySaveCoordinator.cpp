#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Adventure/IslandOwnedAppearanceState.h"
#include "Adventure/IslandResidenceLift.h"
#include "Components/PrimitiveComponent.h"
#include "Water/SwimmingComponent.h"
#include "Aircraft/Lighting/HelicopterSearchLightComponent.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Core/Persistence/NeonCityMarineDeckRestore.h"
#include "Property/PropertySubsystem.h"
#include "Property/HomeArrival.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Player/PlayerCombatAdapter.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Fishing/FishingComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Combat/CombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "Combat/CombatContextSubsystem.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Destruction/DestructionSubsystem.h"
#include "Explosion/ExplosionSubsystem.h"
#include "World/WorldEventSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Adventure/IslandHomeSeat.h"
#include "Adventure/IslandEstateSport.h"
#include "Adventure/IslandEstateAmenity.h"
#include "Adventure/IslandEstatePrecisionSport.h"
#include "Adventure/IslandPartyControl.h"
#include "Adventure/IslandScenePanel.h"
#include "Adventure/IslandHomePet.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/MainlandVehicleRecovery.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Vehicles/VehicleInteraction.h"
#include "World/NeonCityWorldManager.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Wanted/WantedSubsystem.h"
#include "Crime/CrimeSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"

ANeonCityPlayerCharacter* UNeonCitySaveSubsystem::ReadyPlayer() const
{
 UWorld* World=GetWorld(); if(!World || !World->IsGameWorld() || !World->HasBegunPlay()) return nullptr;
 // Saved island floor/boat state cannot be applied before the connected region
 // has registered collision and translated materials in the actual world.
 if(!World->GetSubsystem<UNeonCityWorldManager>()->IsSystemReady_Implementation()) return nullptr;
 auto* PC=World->GetFirstPlayerController(); if(!PC || !PC->IsLocalController()) return nullptr;
 auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
 if(auto* Vehicle=Cast<ANeonCityBaseVehicle>(PC->GetPawn())) Player=Cast<ANeonCityPlayerCharacter>(Vehicle->GetVehicleInteraction()->GetDriver());
 if(auto* Boat=Cast<ANeonCityMarineVehicleBase>(PC->GetPawn())) Player=Cast<ANeonCityPlayerCharacter>(Boat->GetMarineInteraction()->GetDriver());
 if(auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(PC->GetPawn())) Player=Cast<ANeonCityPlayerCharacter>(Aircraft->GetAircraftInteraction()->GetDriver());
 return Player && Player->HasActorBegunPlay() && Player->HasAuthority() ? Player : nullptr;
}
UNeonCitySaveGame* UNeonCitySaveSubsystem::BuildModel(bool Defaults)
{
 auto* Player=ReadyPlayer(); if(!Player) return nullptr;
 if(!Defaults)if(const auto* Kit=Player->FindComponentByClass<UDivingComponent>();Kit && Kit->IsChangingGear())return nullptr;
 if(!Defaults)if(const auto* Suit=Player->FindComponentByClass<UWingsuitComponent>();Suit && Suit->IsFlying())return nullptr;
 auto* GI=GetGameInstance(); auto* Model=NewObject<UNeonCitySaveGame>(this);
 Model->FormatID=TEXT("NeonCity.SaveGame"); Model->SaveID=FGuid::NewGuid(); Model->SavedAt=FDateTime::UtcNow();
 Model->Player=Defaults ? FNeonCityPlayerSaveData() : Player->GetPlayerDataComponent()->BuildPersistenceSnapshot();
 if(Defaults)
 {
  int32 Count=0; for(TActorIterator<APlayerStart> It(GetWorld());It;++It) { Model->Player.Transform=It->GetActorTransform(); ++Count; }
  if(Count!=1) return nullptr;
 }
 else if(auto* Vehicle=Cast<ANeonCityBaseVehicle>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 {
  if(Vehicle->GetVelocity().Size()>150 || !Vehicle->GetVehicleInteraction()->GetPersistenceExitTransform(Model->Player.Transform)) return nullptr;
 }
 else if(auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 {
  if(!Boat->GetMarineInteraction()->FindExitTransform(Model->Player.Transform)) return nullptr;
 }
 else if(auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 {
  if(!Aircraft->GetAircraftInteraction()->FindExitTransform(Model->Player.Transform)) return nullptr;
 }
 else if(const auto* Seat=Cast<AIslandHomeSeat>(Player->GetAttachParentActor()))
 {if(!Seat->FindPersistenceExit(Model->Player.Transform))return nullptr;}
 else if(Player->GetCharacterMovement()->IsFalling() || Player->GetCharacterMovement()->IsSwimming()) return nullptr;
 if(!Defaults)if(const auto* Base=Cast<UPrimitiveComponent>(Player->GetMovementBaseObject()))
  if(const auto* Lift=Cast<AIslandResidenceLift>(Base->GetOwner());Lift && !Lift->FindPersistenceExit(Player,Model->Player.Transform))return nullptr;
 Model->Economy=Defaults?FNeonCityEconomySaveData():GI->GetSubsystem<UNeonCityEconomySubsystem>()->BuildPersistenceSnapshot();
 if(Defaults)Model->Economy.HasBanking=true;
 Model->Player.Money=Model->Economy.Balance;
 Model->Fishing=GI->GetSubsystem<UNeonCityFishingSubsystem>()->BuildPersistenceSnapshot(Defaults);Model->HasFishing=true;
 Model->Properties=GI->GetSubsystem<UPropertySubsystem>()->BuildPersistenceSnapshot(Defaults);
 Model->Vehicles=GI->GetSubsystem<UVehicleOwnershipSubsystem>()->BuildPersistenceSnapshot(Defaults);
 Model->Marine=GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot(Defaults);
 Model->HasMarine=true;
 Model->Aircraft=GetWorld()->GetSubsystem<UAircraftWorldSubsystem>()->BuildPersistenceSnapshot(Defaults); Model->HasAircraft=true;
 for(auto& Vehicle:Model->Vehicles) for(const auto& Property:Model->Properties) for(const auto& Slot:Property.GarageSlots)
  if(Slot.VehicleID==Vehicle.VehicleID) { Vehicle.StoredPropertyID=Property.PropertyID; Vehicle.GarageSlot=Slot.SlotIndex; }
 Model->VehicleWeapons=GetWorld()->GetSubsystem<UVehicleWeaponRegistry>()->BuildSnapshot(Defaults); Model->HasVehicleWeapons=true;
 Model->Weapons=Player->GetCombatAdapter()->BuildPersistenceSnapshot(Defaults);
 Model->Phone=GI->GetSubsystem<UPhoneSubsystem>()->BuildPersistenceSnapshot(Defaults);
 Model->World=GetWorld()->GetSubsystem<UNeonCityWorldManager>()->BuildPersistenceSnapshot(Defaults);
 Model->HasPlayer=Model->HasEconomy=Model->HasVehicles=Model->HasProperties=Model->HasWeapons=Model->HasPhone=Model->HasWorld=true;
 for(TActorIterator<AIslandScenePanel> It(GetWorld());It;++It)
 {
  FNeonCityResidenceSceneSaveData Scene;Scene.ZoneID=It->ZoneID;Scene.Scene=Defaults?0:It->GetScene();
  if(!Scene.ZoneID.IsNone())Model->ResidenceLife.Scenes.Add(Scene);
 }
 for(TActorIterator<AIslandPartyControl> It(GetWorld());It;++It)Model->ResidenceLife.Party=It->BuildPersistenceSnapshot(Defaults);
 for(TActorIterator<AIslandHomePet> It(GetWorld());It;++It)Model->ResidenceLife.Pet=It->BuildPersistenceSnapshot(Defaults);
 Model->HasResidenceLife=true;
 return Model;
}
bool UNeonCitySaveSubsystem::EnsureDefaults()
{
 if(!ReadyPlayer()) return false;
 if(!DesignDefaults || ActiveWorld.Get()!=GetWorld()) { ActiveWorld=GetWorld(); DesignDefaults=BuildModel(true); }
 return DesignDefaults && !DesignDefaults->World.WorldID.IsNone();
}
bool UNeonCitySaveSubsystem::PrepareLoad(UNeonCitySaveGame* Model,FTransform& PlayerDestination)
{
 if(!EnsureDefaults()) return Finish(ENeonCitySaveResult::NotReady,TEXT("Supported world/player/defaults not ready."));
 Model->FillMissingSections(*DesignDefaults,RestoreWarnings);
 FString Error; if(!Model->ValidateEnvelope(Error)) return Finish(ENeonCitySaveResult::InvalidData,Error);
 if(!GetWorld()->GetSubsystem<UNeonCityWorldManager>()->CanApplyPersistenceSnapshot(Model->World)) return Finish(ENeonCitySaveResult::WrongWorld,TEXT("Save belongs to a different or unavailable world."));
 auto* GI=GetGameInstance(); auto* Player=ReadyPlayer();
 UNeonCityEconomySubsystem::NormalizePersistenceSnapshot(Model->Economy,RestoreWarnings);
 GI->GetSubsystem<UNeonCityFishingSubsystem>()->NormalizePersistenceSnapshot(Model->Fishing,RestoreWarnings);
 const auto PreviousProperties=Model->Properties;
 GI->GetSubsystem<UPropertySubsystem>()->NormalizePersistenceSnapshot(Model->Properties,RestoreWarnings);
 const int32 RelocatedGarageCars=NeonCityVehiclePersistence::MigrateAuthoredMainlandGarageParking(
  GetWorld(),PreviousProperties,Model->Properties,Model->Vehicles);
 if(RelocatedGarageCars>0)RestoreWarnings.Add(FString::Printf(
  TEXT("Restored %d owned mainland garage vehicle(s) to their supported authored replacement bays."),RelocatedGarageCars));
 if(bSessionStartLoad)
 {
  // The session opens at home: the player at the home spawn, the home garage holding its own cars.
  bSessionStartLoad=false;FTransform HomeSpawn;auto* Homes=GI->GetSubsystem<UPropertySubsystem>();
  if(Homes->GetHomeSpawnTransform(HomeSpawn))
  {
   const int32 Parked=NeonCityHomeArrival::PrepareSessionStart(Homes->GetHomePropertyID(),HomeSpawn,*DesignDefaults,*Model);
   if(Parked>0)RestoreWarnings.Add(FString::Printf(TEXT("Session start: %d owned car(s) parked back in the home garage."),Parked));
  }
 }
 GI->GetSubsystem<UVehicleOwnershipSubsystem>()->NormalizePersistenceSnapshot(Model->Vehicles,RestoreWarnings);
 GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->NormalizePersistenceSnapshot(Model->Marine,RestoreWarnings);
 GetWorld()->GetSubsystem<UAircraftWorldSubsystem>()->NormalizePersistenceSnapshot(Model->Aircraft,RestoreWarnings);
 GetWorld()->GetSubsystem<UVehicleWeaponRegistry>()->Normalize(Model->VehicleWeapons,RestoreWarnings);
 UCombatComponent::NormalizePersistenceSnapshot(Model->Weapons,RestoreWarnings);
 GI->GetSubsystem<UPhoneSubsystem>()->NormalizePersistenceSnapshot(Model->Phone,RestoreWarnings);
 TSet<FName> Receipts; for(FName ID:Model->Economy.CreditReceipts) if(!ID.IsNone()) Receipts.Add(ID);
 for(auto& Opportunity:Model->Phone.Opportunities)
 { if(Opportunity.State==2) Receipts.Add(Opportunity.OpportunityID); if(Receipts.Contains(Opportunity.OpportunityID)) Opportunity.State=2; }
 Model->Economy.CreditReceipts=Receipts.Array();
 // Normalization can add authored defaults or missing completion receipts. Check bounds again before any mutation.
 if(!Model->ValidateEnvelope(Error)) return Finish(ENeonCitySaveResult::InvalidData,Error);
 for(auto& Vehicle:Model->Vehicles) { Vehicle.StoredPropertyID=NAME_None; Vehicle.GarageSlot=INDEX_NONE; }
 for(auto& Property:Model->Properties) for(auto& Slot:Property.GarageSlots)
 {
  auto* Vehicle=Model->Vehicles.FindByPredicate([&](const auto& V){ return V.VehicleID==Slot.VehicleID; });
  if(!Property.Owned || !Vehicle || !Vehicle->Owned || Vehicle->Condition<=0 || FVector::Dist2D(Vehicle->Transform.GetLocation(),Slot.StoredLocation)>250)
  { Slot.VehicleID.Invalidate(); continue; }
  Vehicle->StoredPropertyID=Property.PropertyID; Vehicle->GarageSlot=Slot.SlotIndex;
 }
 if(auto* Mounted=Cast<ANeonCityBaseVehicle>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 {
  FTransform Exit; if(Mounted->GetVelocity().Size()>150 || !Mounted->GetVehicleInteraction()->GetPersistenceExitTransform(Exit)) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Stop with a clear vehicle exit before loading."));
 }
 if(auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 {
  FTransform Exit; if(!Boat->GetMarineInteraction()->FindExitTransform(Exit)) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Stop the boat with a clear exit before loading."));
 }
 if(auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 {
  FTransform Exit; if(!Aircraft->GetAircraftInteraction()->FindExitTransform(Exit)) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Land with a clear aircraft exit before loading."));
 }
 if (auto* Coast=ANeonCityCoastalRegion::Find(GetWorld()))
  if (!Coast->EnsureDestinationReady(Model->Player.Transform.GetLocation()))
   return Finish(ENeonCitySaveResult::NotReady,TEXT("Saved offshore region collision is not ready; no saved values applied."));
 FCollisionQueryParams Query(SCENE_QUERY_STAT(PersistencePlayer),false,Player);
 for(TActorIterator<ANeonCityAircraftVehicleBase> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
 for(TActorIterator<ANeonCityMarineVehicleBase> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
 for(TActorIterator<ANeonCityBaseVehicle> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
 auto ValidSavedDeck=[&](const FTransform& Transform)
 {
  FGuid ID;int32 Valid=0;
  for(const auto& Boat:Model->Marine)if(NeonCityValidateSavedMarineDeck(GetWorld(),Player,Boat,Transform)){ID=Boat.VehicleID;++Valid;}
  return Valid==1?ID:FGuid();
 };
 auto PositionClear=[&](const FTransform& Transform)
 {
  const auto* Capsule=Player->GetCapsuleComponent(); const FVector Location=Transform.GetLocation();
  if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->IsDeepWater(Location,0)) return false;
  if(GetWorld()->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query)) return false;
  for(const auto& Vehicle:Model->Vehicles)
  { const FVector Delta=Vehicle.Transform.InverseTransformPosition(Location); if(FMath::Abs(Delta.X)<285 && FMath::Abs(Delta.Y)<145 && FMath::Abs(Delta.Z)<220) return false; }
  const FGuid DeckID=ValidSavedDeck(Transform);
  for(const auto& Boat:Model->Marine)
  { const FVector Delta=Boat.Transform.InverseTransformPosition(Location); if(Boat.VehicleID!=DeckID && FMath::Abs(Delta.X)<300 && FMath::Abs(Delta.Y)<150 && FMath::Abs(Delta.Z)<220) return false; }
  for(const auto& Aircraft:Model->Aircraft)
  { const FVector Local=Aircraft.Transform.InverseTransformPosition(Location); if(FMath::Abs(Local.X)<500 && FMath::Abs(Local.Y)<180 && FMath::Abs(Local.Z)<300) return false; }
  return true;
 };
 auto Clear=[&](const FTransform& Transform)
 {
  if(!PositionClear(Transform)) return false;
  if(ValidSavedDeck(Transform).IsValid())return true;
  const FVector Location=Transform.GetLocation(); FHitResult Floor;
  auto* Movement=Player->GetCharacterMovement();
  if(GetWorld()->LineTraceSingleByChannel(Floor,Location,Location-FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+20),ECC_Visibility,Query) && Floor.ImpactNormal.Z>=Movement->GetWalkableFloorZ())return true;
  // A vertical visibility ray can miss the support found by the walking
  // capsule, even for a genuine grounded save. Reuse normal CharacterMovement
  // floor detection at exactly
  // the saved position; keep all capsule/water/vehicle guards above. This
  // neither moves the player sideways nor accepts an airborne capsule.
  FFindFloorResult StandingFloor;Movement->FindFloor(Location,StandingFloor,false);
  const bool Standing=StandingFloor.IsWalkableFloor() && StandingFloor.GetDistanceToFloor()>=0 && StandingFloor.GetDistanceToFloor()<=3.f;
  if(Standing)UE_LOG(LogTemp,Display,TEXT("Persistence capsule support: position%s actual_floor%s distance%.3f normal%s; centre ray missed actual walking support"),*Location.ToString(),*GetNameSafe(StandingFloor.HitResult.GetActor()),StandingFloor.GetDistanceToFloor(),*StandingFloor.HitResult.ImpactNormal.ToString());
  return Standing;
 };
 PlayerDestination=Model->Player.Transform;
 // Static world geometry is authoritative. A released save may predate a
 // small pavement-height repair; settle only downward at the same XY, without
 // relaxing capsule, water, vehicle or walkable-floor safety checks. Never
 // search through an obstruction or move the authored residence to match it.
 if(!Clear(PlayerDestination) && PositionClear(PlayerDestination))
 {
  constexpr double MaximumSettlementCm=50.;
  const FVector Location=PlayerDestination.GetLocation();
  const double HalfHeight=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  FHitResult Floor;
  if(GetWorld()->LineTraceSingleByChannel(Floor,Location,Location-FVector(0,0,HalfHeight+MaximumSettlementCm),ECC_Visibility,Query)
   && Floor.ImpactNormal.Z>=Player->GetCharacterMovement()->GetWalkableFloorZ())
  {
   FTransform Settled=PlayerDestination;
   Settled.SetLocation(FVector(Location.X,Location.Y,Floor.ImpactPoint.Z+HalfHeight+2.));
   const double Drop=Location.Z-Settled.GetLocation().Z;
   if(Drop>0 && Drop<=MaximumSettlementCm && Clear(Settled))
   {
    PlayerDestination=Settled;
    RestoreWarnings.Add(TEXT("Saved player floor height adjusted to current static world geometry."));
   }
  }
 }
 if(!Clear(PlayerDestination))
 {
  PlayerDestination=DesignDefaults->Player.Transform;
  if(!Clear(PlayerDestination)) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Saved position and default spawn obstructed; no state applied."));
  RestoreWarnings.Add(TEXT("Saved player position obstructed; authored safe spawn used."));
 }
 return true;
}
bool UNeonCitySaveSubsystem::ApplyModel(UNeonCitySaveGame* Model,const FTransform& Destination)
{
 auto* Player=ReadyPlayer(); auto* PC=GetWorld()->GetFirstPlayerController(); auto* GI=GetGameInstance();
 for(TActorIterator<AIslandEstateSport> It(GetWorld());It;++It)It->EndSession();
 for(TActorIterator<AIslandEstatePrecisionSport> It(GetWorld());It;++It)It->FinishSession();
 for(TActorIterator<AIslandEstateAmenity> It(GetWorld());It;++It)
  if(It->IsUsing() && !It->FinishUse())return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Estate amenity exit became blocked; no saved values applied."));
 if(auto* Seat=Cast<AIslandHomeSeat>(Player->GetAttachParentActor()))if(!Seat->Leave())return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Seat exit became blocked; no saved values applied."));
 if(auto* Mounted=Cast<ANeonCityBaseVehicle>(PC->GetPawn())) if(!Mounted->GetVehicleInteraction()->RestoreDriver(false)) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Vehicle exit became blocked; no saved values applied."));
 if(auto* Boat=Cast<ANeonCityMarineVehicleBase>(PC->GetPawn())) if(!Boat->GetMarineInteraction()->Exit()) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Boat exit became blocked; no saved values applied."));
 if(auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(PC->GetPawn())) if(!Aircraft->GetAircraftInteraction()->Exit()) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Aircraft exit became blocked; no saved values applied."));
 GetWorld()->GetSubsystem<UVehicleProjectilePool>()->ResetForLoad();
 GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->ResetForLoad();
 // Transient city damage is restored; only surfaces that opted into persistence keep their damage.
 GetWorld()->GetSubsystem<UDestructionSubsystem>()->RestoreTransientDestruction();
 GetWorld()->GetSubsystem<UNeonCityExplosionSubsystem>()->ResetForLoad();
 GetWorld()->GetSubsystem<UWorldEventSubsystem>()->ResetForLoad();
 if(auto* Fishing=PC->FindComponentByClass<UFishingComponent>())Fishing->Stop();
 Player->GetCombatAdapter()->PrepareForRestore();
 Player->SetActorTransform(Destination,false,nullptr,ETeleportType::TeleportPhysics); PC->SetControlRotation(Destination.Rotator());
 Player->GetCharacterMovement()->StopMovementImmediately();
 Player->GetPlayerDataComponent()->ApplyPersistenceSnapshot(Model->Player);
 if(auto* Swimming=Player->FindComponentByClass<USwimmingComponent>()) Swimming->ResetForLoad();
 GI->GetSubsystem<UNeonCityEconomySubsystem>()->ApplyPersistenceSnapshot(Model->Economy);
 GI->GetSubsystem<UPropertySubsystem>()->ApplyPersistenceSnapshot(Model->Properties);
 GI->GetSubsystem<UVehicleOwnershipSubsystem>()->ApplyPersistenceSnapshot(Model->Vehicles);
 GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->ApplyPersistenceSnapshot(Model->Marine);
 GetWorld()->GetSubsystem<UAircraftWorldSubsystem>()->ApplyPersistenceSnapshot(Model->Aircraft);
 for(TActorIterator<ANeonCityAircraftVehicleBase> It(GetWorld());It;++It)
  if(auto* Light=It->FindComponentByClass<UHelicopterSearchLightComponent>()) Light->ResetForLoad();
 GetWorld()->GetSubsystem<UVehicleWeaponRegistry>()->ApplySnapshot(Model->VehicleWeapons);
 Player->GetCombatAdapter()->ApplyPersistenceSnapshot(Model->Weapons);
 if(auto* Kit=Player->FindComponentByClass<UDivingComponent>())Kit->RestoreGear(Model->Player.OwnsR05,Model->Player.OwnsDPV,Model->Player.R05Equipped);
 if(auto* Wingsuit=Player->FindComponentByClass<UWingsuitComponent>())Wingsuit->RestoreEquipped(Model->Player.WingsuitEquipped);
 // Gear owns its skin/material restoration. Restore clothing afterward;
 // if gear is worn, the selected owned palette waits until normal on-foot clothing returns.
 if(auto* Appearance=UIslandOwnedAppearanceState::FindOrCreate(Player))
  Appearance->RestoreState(Model->Player.OwnedAppearancePalette,Model->Player.OwnedAppearanceMask);
 GI->GetSubsystem<UPhoneSubsystem>()->ApplyPersistenceSnapshot(Model->Phone);
 GetWorld()->GetSubsystem<UNeonCityWorldManager>()->ApplyPersistenceSnapshot(Model->World);
 GI->GetSubsystem<UNeonCityFishingSubsystem>()->ApplyPersistenceSnapshot(Model->Fishing);
 for(TActorIterator<AIslandScenePanel> It(GetWorld());It;++It)
 {
  const auto* Scene=Model->ResidenceLife.Scenes.FindByPredicate([&](const auto& S){return S.ZoneID==It->ZoneID;});
  It->SetScene(Scene?Scene->Scene:0);
 }
 for(TActorIterator<AIslandPartyControl> It(GetWorld());It;++It)It->ApplyPersistenceSnapshot(Model->ResidenceLife.Party);
 for(TActorIterator<AIslandHomePet> It(GetWorld());It;++It)It->ApplyPersistenceSnapshot(Model->ResidenceLife.Pet);
 GetWorld()->GetSubsystem<UCombatContextSubsystem>()->ResetForLoad();
 GetWorld()->GetSubsystem<UCrimeSubsystem>()->ResetForLoad(); GetWorld()->GetSubsystem<UWantedSubsystem>()->ResetForLoad();
 bWriteProtected=false;
 return Finish(ENeonCitySaveResult::Success,RestoreWarnings.IsEmpty()?TEXT("Restored player/economy/property/vehicle/weapon/phone/world; Wanted reset to 0."):FString::Join(RestoreWarnings,TEXT(" ")));
}
bool UNeonCitySaveSubsystem::SaveGame()
{
 if(IsOperationInProgress() || GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->IsAccountOperationInProgress() || GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->IsInventoryOperationInProgress()) return Finish(ENeonCitySaveResult::Busy,TEXT("Save/load or account operation already active."));
 TGuardValue<bool> Guard(bBusy,true);
 if(!EnsureDefaults()) return Finish(ENeonCitySaveResult::NotReady,TEXT("World and player must be ready."));
 auto* Model=BuildModel(false); if(!Model) return Finish(ENeonCitySaveResult::UnsafeState,TEXT("Save on foot or parked with a clear exit."));
 if(!WriteGeneration(Model)) return false;
 GetWorld()->GetTimerManager().ClearTimer(AutosaveTimer);
 return Finish(ENeonCitySaveResult::Success,TEXT("Verified SaveGame written; previous valid generation retained."));
}
bool UNeonCitySaveSubsystem::CreateNewGameSave()
{
 if(IsOperationInProgress() || GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->IsAccountOperationInProgress() || GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->IsInventoryOperationInProgress()) return Finish(ENeonCitySaveResult::Busy,TEXT("Save/load or account operation already active."));
 if(DoesSaveExist()) return Finish(ENeonCitySaveResult::AlreadyExists,TEXT("Existing save retained. Delete explicitly before creating a new game."));
 if(!ReadyPlayer())
 {
  if(!GetWorld() || !GetWorld()->IsGameWorld()) return Finish(ENeonCitySaveResult::NotReady,TEXT("No game world."));
  PendingOperation=2; ReadyDeadline=FPlatformTime::Seconds()+15; GetWorld()->GetTimerManager().SetTimer(ReadyTimer,this,&UNeonCitySaveSubsystem::DeferredLoad,.1f,true);
  return Finish(ENeonCitySaveResult::Deferred,TEXT("New game queued until world/player initialization completes."));
 }
 TGuardValue<bool> Guard(bBusy,true); RestoreWarnings.Reset();
 if(!EnsureDefaults()) return Finish(ENeonCitySaveResult::NotReady,TEXT("Unsupported or incomplete world."));
 auto* Model=DuplicateObject<UNeonCitySaveGame>(DesignDefaults,this); Model->SaveID=FGuid::NewGuid(); Model->SavedAt=FDateTime::UtcNow();
 FTransform Destination; if(!PrepareLoad(Model,Destination) || !WriteGeneration(Model)) return false; return ApplyModel(Model,Destination);
}
bool UNeonCitySaveSubsystem::LoadGame()
{
 if(IsOperationInProgress() || GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->IsAccountOperationInProgress() || GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->IsInventoryOperationInProgress()) return Finish(ENeonCitySaveResult::Busy,TEXT("Save/load or account operation already active."));
 if(!ReadyPlayer())
 {
  if(!GetWorld() || !GetWorld()->IsGameWorld()) return Finish(ENeonCitySaveResult::NotReady,TEXT("No game world."));
  PendingOperation=1; ReadyDeadline=FPlatformTime::Seconds()+15; GetWorld()->GetTimerManager().SetTimer(ReadyTimer,this,&UNeonCitySaveSubsystem::DeferredLoad,.1f,true);
  return Finish(ENeonCitySaveResult::Deferred,TEXT("Load queued until world/player initialization completes."));
 }
 if(!DoesSaveExist()) return CreateNewGameSave();
 TGuardValue<bool> Guard(bBusy,true); RestoreWarnings.Reset(); auto* Model=ReadBestGeneration(); if(!Model) return false;
 FTransform Destination; if(!PrepareLoad(Model,Destination)) { bWriteProtected=true; return false; }
 GetWorld()->GetTimerManager().ClearTimer(AutosaveTimer); return ApplyModel(Model,Destination);
}
void UNeonCitySaveSubsystem::NotifyWorldReady(UWorld* World)
{
 if(!World || World->GetGameInstance()!=GetGameInstance()) return;
 ActiveWorld=World; DesignDefaults=nullptr;
 // Tests explicitly opt into dedicated slots; never read/write the user's normal slot during automation.
 if(GIsAutomationTesting || FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))) return;
 bAutosaveEnabled=true; PendingOperation=1; ReadyDeadline=FPlatformTime::Seconds()+15; bSessionStartLoad=true;
 World->GetTimerManager().SetTimer(ReadyTimer,this,&UNeonCitySaveSubsystem::DeferredLoad,.1f,true);
}
void UNeonCitySaveSubsystem::DeferredLoad()
{
 if(!ReadyPlayer() && FPlatformTime::Seconds()<ReadyDeadline) return;
 const uint8 Request=PendingOperation; PendingOperation=0; GetWorld()->GetTimerManager().ClearTimer(ReadyTimer);
 if(!ReadyPlayer()) { Finish(ENeonCitySaveResult::NotReady,TEXT("Initialization timeout; no disk data applied.")); return; }
 if(GetWorld()->GetSubsystem<UNeonCityWorldManager>()->BuildPersistenceSnapshot().WorldID.IsNone()) return;
 if(Request==2) CreateNewGameSave(); else LoadGame();
}
bool UNeonCitySaveSubsystem::IsAutosaveSafe(bool AllowDoorTransition) const
{
 auto* Player=ReadyPlayer(); if(!Player || Player->GetHealthComponent()->IsDead() || (!AllowDoorTransition && Player->GetCharacterMovement()->IsFalling())) return false;
 if(auto* Body=Player->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
 if(Player->GetCharacterMovement()->IsSwimming()) return false;
 if(auto* Boat=Cast<ANeonCityMarineVehicleBase>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 { FTransform Exit; if(Boat->GetSpeed_Implementation()>80 || !Boat->GetMarineInteraction()->FindExitTransform(Exit)) return false; }
 if(auto* Aircraft=Cast<ANeonCityAircraftVehicleBase>(GetWorld()->GetFirstPlayerController()->GetPawn()))
 { FTransform Exit; if(!Aircraft->GetAircraftInteraction()->FindExitTransform(Exit)) return false; }
 if(GetWorld()->GetSubsystem<UWantedSubsystem>()->GetWantedLevel()!=0) return false;
 if(auto* Vehicle=Cast<ANeonCityBaseVehicle>(GetWorld()->GetFirstPlayerController()->GetPawn())) return Vehicle->GetVelocity().Size()<80;
 return true;
}
bool UNeonCitySaveSubsystem::RequestAutosave(ENeonCityAutosaveReason Reason)
{
 if(!bAutosaveEnabled || bWriteProtected || IsOperationInProgress() || uint8(Reason)>uint8(ENeonCityAutosaveReason::SavePoint) || !IsAutosaveSafe(Reason==ENeonCityAutosaveReason::EnteredHome)) return false;
 // Door teleport can briefly mark the character as falling. Queue once, then require fully safe ground at flush.
 const double Delay=FMath::Max(.5,15.-(FPlatformTime::Seconds()-LastWriteTime));
 if(!GetWorld()->GetTimerManager().IsTimerActive(AutosaveTimer)) GetWorld()->GetTimerManager().SetTimer(AutosaveTimer,this,&UNeonCitySaveSubsystem::FlushAutosave,float(Delay),false);
 return true;
}
void UNeonCitySaveSubsystem::FlushAutosave() { if(bAutosaveEnabled && !IsOperationInProgress() && IsAutosaveSafe()) SaveGame(); }
void UNeonCitySaveSubsystem::Deinitialize()
{
 if(auto* World=ActiveWorld.Get()) { World->GetTimerManager().ClearTimer(ReadyTimer); World->GetTimerManager().ClearTimer(AutosaveTimer); }
 PendingOperation=0; DesignDefaults=nullptr; OnSaveOperation.Clear(); Super::Deinitialize();
}
