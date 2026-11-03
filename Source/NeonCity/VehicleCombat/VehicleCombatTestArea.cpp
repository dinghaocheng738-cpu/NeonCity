#include "VehicleCombat/VehicleCombatTestArea.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/HealthComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
AVehicleCombatTestArea::AVehicleCombatTestArea()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 auto* Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Instructions")); Sign->SetupAttachment(GetRootComponent());
 Sign->SetRelativeLocation(FVector(6200,6200,600)); Sign->SetRelativeRotation(FRotator(0,-90,0)); Sign->SetWorldSize(25);
 Sign->SetText(FText::FromString(TEXT("VEHICLE COMBAT RANGE\nV arm (boat: B) / LMB fire / RMB hold to lock\nR reload / Tab next hardpoint\nSUV + Pickup / Speedboat / Helicopter / Light Aircraft\nTargets at west runway end; public damage still creates crime")));
}
void AVehicleCombatTestArea::BeginPlay()
{
 Super::BeginPlay(); if(!HasAuthority()) return;
 bool HasStrip=false; for(TActorIterator<AFixedWingTestArea> It(GetWorld());It;++It) HasStrip=true;
 if(!HasStrip) Fixtures.Add(GetWorld()->SpawnActor<AFixedWingTestArea>());
 for(TActorIterator<ANeonCityBaseVehicle> It(GetWorld());It;++It)
  if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::SUV || It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::Pickup) UVehicleCombatComponent::Install(*It,EVehicleCombatMountPreset::Roof);
 for(TActorIterator<ANeonCityMarineVehicleBase> It(GetWorld());It;++It)
  if(It->VehicleID==UMarineWorldSubsystem::SpeedboatID()) UVehicleCombatComponent::Install(*It,EVehicleCombatMountPreset::Boat);
 for(TActorIterator<ANeonCityAircraftVehicleBase> It(GetWorld());It;++It) UVehicleCombatComponent::Install(*It,It->IsA<ANeonCityFixedWingAircraft>()?EVehicleCombatMountPreset::FixedWing:EVehicleCombatMountPreset::Helicopter);
 for(FVector Location:{FVector(-17000,7000,550),FVector(-18000,6500,3000)})
 {
  auto* Target=GetWorld()->SpawnActor<ACombatTestTarget>(Location,FRotator::ZeroRotator);
  if(Target) { Target->GetHealthComponent()->ApplyPersistenceHealth(1000,1000); Fixtures.Add(Target); }
 }
 const FTransform Stand(FRotator::ZeroRotator,FVector(-16000,6600,545));
 auto* Car=GetWorld()->SpawnActorDeferred<ANeonCityStarterSedan>(ANeonCityStarterSedan::StaticClass(),Stand,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(Car) { Car->VehicleID=FGuid(0x19000001,0x434F4D42,0x41545241,0x4E474531); Car->FinishSpawning(Stand); Fixtures.Add(Car); }
}
void AVehicleCombatTestArea::EndPlay(EEndPlayReason::Type Reason)
{ if(Reason==EEndPlayReason::Destroyed) for(auto A:Fixtures) if(IsValid(A)) A->Destroy(); Super::EndPlay(Reason); }
