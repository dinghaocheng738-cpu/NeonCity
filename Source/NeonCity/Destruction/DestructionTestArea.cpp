#include "Destruction/DestructionTestArea.h"
#include "Destruction/DestructibleStructure.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/Police/PoliceCharacter.h"
#include "Combat/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
ADestructionTestArea::ADestructionTestArea()
{
 PrimaryActorTick.bCanEverTick=false;
 Pad=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad")); SetRootComponent(Pad);
 Pad->SetMobility(EComponentMobility::Movable); Pad->SetCollisionProfileName(TEXT("BlockAll"));
 Pad->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Pad->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal")));
 Pad->SetRelativeScale3D(FVector(PadExtentX/100.f,PadExtentY/100.f,.5f));
}
ANeonCityPhysicsProp* ADestructionTestArea::AddProp(ECityPhysicsProp Type,FVector Location,FRotator Rotation)
{
 const FTransform Transform(Rotation,Location);
 auto* Prop=GetWorld()->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Prop) return nullptr;
 Prop->Configure(Type); Prop->FinishSpawning(Transform); Props.Add(Prop); return Prop;
}
ANeonCityBaseVehicle* ADestructionTestArea::AddVehicle(FVector Location,const TCHAR* DataPath)
{
 const FTransform Transform(FRotator(0,90,0),Location);
 auto* Car=GetWorld()->SpawnActorDeferred<ANeonCityPrototypeVehicle>(ANeonCityPrototypeVehicle::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Car) return nullptr;
 Car->VehicleData=LoadObject<UVehicleData>(nullptr,DataPath); Car->FinishSpawning(Transform);
 // Wrecks on this pad burn and blow up; the city default stays a non-exploding wreck.
 Car->GetDamageState()->EnableExplosion(TEXT("NeonCity.VehicleExplosion"),1.5f,2.5f);
 Vehicles.Add(Car); return Car;
}
void ADestructionTestArea::BeginPlay()
{
 Super::BeginPlay();
 if(!HasAuthority() || UGameplayStatics::GetCurrentLevelName(GetWorld(),true)!=TEXT("NeonCity_SouthBeach_Test")) return;
 const FVector O=GetActorLocation(),F=O+FVector(0,0,26);
 // Three wrecks in a row: the middle one chains, the far one proves the cascade stops.
 AddVehicle(F+FVector(-1400,-1200,60),TEXT("/Game/NeonCity/Vehicles/DA_SUV.DA_SUV"));
 AddVehicle(F+FVector(-900,-1200,60),TEXT("/Game/NeonCity/Vehicles/DA_SportsCar.DA_SportsCar"));
 AddVehicle(F+FVector(-300,-1200,60),TEXT("/Game/NeonCity/Vehicles/DA_SUV.DA_SUV"));
 // Storefront and window glass, thin enough for bullets, wide enough for a blast.
 for(int32 I=0;I<4;++I) AddProp(I<2?ECityPhysicsProp::StorefrontGlass:ECityPhysicsProp::Glass,F+FVector(700+I*190,400,140));
 for(int32 I=0;I<3;++I) AddProp(ECityPhysicsProp::LampPost,F+FVector(-1600+I*500,900,160));
 for(int32 I=0;I<3;++I) AddProp(ECityPhysicsProp::RoadBarrier,F+FVector(200+I*230,-2000,37));
 for(int32 I=0;I<3;++I) AddProp(ECityPhysicsProp::TrashCan,F+FVector(2000,-1400+I*210,47));
 AddProp(ECityPhysicsProp::Billboard,F+FVector(2400,900,100));
 AddProp(ECityPhysicsProp::Sign,F+FVector(1200,-800,95));
 AddProp(ECityPhysicsProp::Door,F+FVector(2600,-200,110));
 for(int32 I=0;I<2;++I) AddProp(ECityPhysicsProp::Fence,F+FVector(300+I*230,1700,55));
 // Small destructible wall: a stable shell with authored breakable parts, never a collapsing building.
 Structure=GetWorld()->SpawnActor<ADestructibleStructure>(ADestructibleStructure::StaticClass(),FTransform(F+FVector(1200,1700,0)));
 if(Structure)
 {
  Structure->Configure(TEXT("NeonCity.TestWall"),FVector(600,40,260),false);
  for(int32 I=0;I<4;++I) Structure->AddPart(ECityPhysicsProp::ConcreteWall,FVector(-215+I*143,0,130),FRotator::ZeroRotator,60);
 }
 // Bystanders: two near the wrecks and the glass, one officer at the pad edge.
 const FVector WalkerLocations[]={FVector(-1200,-1900,95),FVector(900,700,95),FVector(0,1200,95)};
 for(int32 I=0;I<3;++I)
 {
  const FTransform Spawn(FRotator(0,180,0),F+WalkerLocations[I]);
  auto* NPC=GetWorld()->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!NPC) continue;
  NPC->InitializePedestrian(FName(*FString::Printf(TEXT("DestructionTest.Civilian.%d"),I)),nullptr,{},211+I); NPC->FinishSpawning(Spawn);
  Pedestrians.Add(NPC);
 }
 // A visible officer keeps the pad readable; response stays test-controlled so fixtures remain deterministic.
 Officer=GetWorld()->SpawnActor<APoliceCharacter>(F+FVector(-2200,-1000,95),FRotator(0,90,0));
 if(Officer) { Officer->SetResponseEnabled(false); Pedestrians.Add(Officer); }
}
ANeonCityPhysicsProp* ADestructionTestArea::GetProp(ECityPhysicsProp Type,int32 Index) const
{
 int32 Found=0;
 for(const auto& Entry:Props) if(IsValid(Entry.Get()) && Entry->GetPropType()==Type) { if(Found++==Index) return Entry.Get(); }
 return nullptr;
}
void ADestructionTestArea::EndPlay(EEndPlayReason::Type Reason)
{
 // Explicit teardown keeps the fixtures removable; nothing here is part of the saved city.
 if(Reason==EEndPlayReason::Destroyed)
 {
  for(const auto& Entry:Props) if(IsValid(Entry.Get())) Entry->Destroy();
  for(const auto& Entry:Vehicles) if(IsValid(Entry.Get())) Entry->Destroy();
  for(const auto& Entry:Pedestrians) if(IsValid(Entry.Get())) Entry->Destroy();
  if(IsValid(Structure)) Structure->Destroy();
  if(IsValid(Officer)) Officer->Destroy();
 }
 Props.Reset(); Vehicles.Reset(); Pedestrians.Reset(); Structure=nullptr; Officer=nullptr;
 Super::EndPlay(Reason);
}
