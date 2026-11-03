#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "Aircraft/Lighting/HelicopterSearchLightComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
ANeonCityHelicopterPrototype::ANeonCityHelicopterPrototype()
{
 CreateDefaultSubobject<UHelicopterSearchLightComponent>(TEXT("SearchLight"));
 // Primitive prototype body; visual parts never become separate floors or physics bodies.
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Part=[&](const TCHAR* Name,FVector Location,FVector Scale)
 {
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name); Mesh->SetupAttachment(GetHull()); Mesh->SetStaticMesh(Cube.Object);
  Mesh->SetRelativeLocation(Location); Mesh->SetRelativeScale3D(Scale); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 };
 Part(TEXT("Cabin"),FVector::ZeroVector,FVector(3.2,1.8,1.5));
 Part(TEXT("Nose"),FVector(175,0,-10),FVector(.6,1.5,1.1));
 Part(TEXT("TailBoom"),FVector(-280,0,20),FVector(3.3,.25,.3));
 Part(TEXT("TailFin"),FVector(-430,0,60),FVector(.2,.2,1.2));
 Part(TEXT("LeftSkid"),FVector(0,-85,-68),FVector(3.6,.12,.14));
 Part(TEXT("RightSkid"),FVector(0,85,-68),FVector(3.6,.12,.14));
 Part(TEXT("Mast"),FVector(0,0,125),FVector(.15,.15,1));
 Rotor=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainRotor")); Rotor->SetupAttachment(GetHull());
 Rotor->SetStaticMesh(Cube.Object); Rotor->SetRelativeLocation(FVector(0,0,180)); Rotor->SetRelativeScale3D(FVector(8,.15,.06));
 Rotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 NeonCityVisuals::Create(*this,GetHull(),TEXT("Aircraft"),TEXT("Helicopter"),
  {TEXT("Cabin"),TEXT("Nose"),TEXT("TailBoom"),TEXT("TailFin"),TEXT("LeftSkid"),TEXT("RightSkid"),TEXT("Mast")});
 NeonCityVisuals::Apply(Rotor,TEXT("Aircraft"),TEXT("HelicopterRotor"));
 TailRotor=NeonCityVisuals::Create(*this,GetHull(),TEXT("Aircraft"),TEXT("TailRotor"),{},TEXT("TailRotorVisual"));
 TailRotor->SetRelativeLocationAndRotation(FVector(-420,18,70),FRotator(0,0,90));
}
void ANeonCityHelicopterPrototype::BeginPlay()
{
 Super::BeginPlay();
 // The standard player helicopter uses the existing Phase 19 loadout. Previously
 // only the opt-in proving-ground actor installed it. Weapons still start safe.
 UVehicleCombatComponent::Install(this,EVehicleCombatMountPreset::Helicopter);
}
void ANeonCityHelicopterPrototype::Tick(float Delta)
{
 Super::Tick(Delta);
 const float Power=GetAircraftMovement()->GetCurrentPower();
 if(GetAircraftMovement()->IsEngineRunning()) Rotor->AddLocalRotation(FRotator(0,(700+Power*300)*Delta,0));
 if(GetAircraftMovement()->IsEngineRunning()) TailRotor->AddLocalRotation(FRotator(0,(1200+Power*600)*Delta,0));
}
