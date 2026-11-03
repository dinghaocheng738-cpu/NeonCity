#include "Physics/NeonCityPhysicsTestArea.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Vehicles/VehicleData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
void ANeonCityPhysicsTestArea::BeginPlay()
{
 Super::BeginPlay(); if(!HasAuthority() || UGameplayStatics::GetCurrentLevelName(GetWorld(),true)!=TEXT("NeonCity_SouthBeach_Test")) return;
 const float HalfHeight[]={32.5f,47.5f,37.5f,95,160,55,75};
 for(int32 I=0;I<7;++I)
 {
  const FTransform T(FVector(-6200+I*135,2600,HalfHeight[I]+2));
  auto* Prop=GetWorld()->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),T);
  if(Prop) { Prop->Configure(ECityPhysicsProp(I)); Prop->FinishSpawning(T); Fixtures.Add(Prop); }
 }
 for(int32 I=0;I<2;++I)
 {
  const FTransform T(FRotator(0,90,0),FVector(-6100+I*550,900,120));
  auto* Car=GetWorld()->SpawnActorDeferred<ANeonCityPrototypeVehicle>(ANeonCityPrototypeVehicle::StaticClass(),T);
  if(Car) { Car->VehicleData=LoadObject<UVehicleData>(nullptr,I==0?TEXT("/Game/NeonCity/Vehicles/DA_SportsCar.DA_SportsCar"):TEXT("/Game/NeonCity/Vehicles/DA_SUV.DA_SUV")); Car->FinishSpawning(T); Fixtures.Add(Car); }
 }
 const FTransform T(FVector(-6050,1800,94));
 auto* NPC=GetWorld()->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),T);
 if(NPC) { NPC->InitializePedestrian(TEXT("PhysicsTest.Civilian"),nullptr,{},115); NPC->FinishSpawning(T); Fixtures.Add(NPC); }
}
void ANeonCityPhysicsTestArea::EndPlay(EEndPlayReason::Type Reason)
{ if(Reason==EEndPlayReason::Destroyed) for(auto Fixture:Fixtures) if(IsValid(Fixture)) Fixture->Destroy(); Super::EndPlay(Reason); }
