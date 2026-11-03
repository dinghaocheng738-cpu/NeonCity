#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
AFixedWingTestArea::AFixedWingTestArea()
{
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root); Root->SetMobility(EComponentMobility::Static);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Asphalt(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Asphalt.M_SB_Asphalt"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> White(TEXT("/Game/NeonCity/Environment/Materials/M_SB_White.M_SB_White"));
 auto Block=[&](const TCHAR* Name,FVector Center,FVector Size,bool Collision=true,FRotator Rotation=FRotator::ZeroRotator)
 {
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name); Mesh->SetupAttachment(Root); Mesh->SetMobility(EComponentMobility::Static); Mesh->SetStaticMesh(Cube.Object);
  Mesh->SetMaterial(0,Collision?Asphalt.Object:White.Object);
  Mesh->SetRelativeLocation(Center); Mesh->SetRelativeRotation(Rotation); Mesh->SetRelativeScale3D(Size/100);
  Mesh->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
 };
 Block(TEXT("Runway"),FVector(0,7000,350),FVector(50000,2000,100));
 // Narrow ramp at the east edge, clear of the apartment, crosses above the unchanged boundary.
 Block(TEXT("AccessRamp"),FVector(6200,1600,194),FVector(3224,280,24),true,FRotationMatrix::MakeFromX(FVector(0,3200,392)).Rotator());
 Block(TEXT("AccessBridge"),FVector(6200,4650,380),FVector(280,2900,40));
 for(int Index=0;Index<25;++Index)
  Block(*FString::Printf(TEXT("Centerline%d"),Index),FVector(-24000+Index*2000,7000,401),FVector(600,25,2),false);
 Block(TEXT("ThresholdWest"),FVector(-22000,7000,402),FVector(100,1500,2),false);
 Block(TEXT("ThresholdEast"),FVector(22000,7000,402),FVector(100,1500,2),false);
 auto* Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("RunwaySign")); Sign->SetupAttachment(Root);
 Sign->SetRelativeLocation(FVector(15500,6100,600)); Sign->SetRelativeRotation(FRotator(0,-90,0)); Sign->SetWorldSize(28);
 Sign->SetText(NSLOCTEXT("NeonCity","FixedWingInstructions","LIGHT AIRCRAFT / RUNWAY 09 - 27\nW/S throttle | A/D bank | Q/E rudder\nMouse Y or Down/Up pitch | Space ground brake\nF enter / exit when safely stopped"));
}
void AFixedWingTestArea::BeginPlay()
{
 Super::BeginPlay(); if(!HasAuthority()) return;
 const FTransform Stand=StandTransform();
 Aircraft=GetWorld()->SpawnActorDeferred<ANeonCityFixedWingAircraft>(ANeonCityFixedWingAircraft::StaticClass(),Stand,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(Aircraft) { Aircraft->AircraftID=PrototypeID(); Aircraft->FinishSpawning(Stand); }
}
void AFixedWingTestArea::EndPlay(EEndPlayReason::Type Reason)
{ if(Reason==EEndPlayReason::Destroyed && IsValid(Aircraft)) Aircraft->Destroy(); Super::EndPlay(Reason); }
