#include "Vehicles/Aircraft/AircraftTestArea.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "World/NeonCityMainlandSite.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
AAircraftTestArea::AAircraftTestArea()
{
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Block=[&](const TCHAR* Name,FVector Center,FVector Size,bool Collision=true)
 {
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name); Mesh->SetupAttachment(Root); Mesh->SetStaticMesh(Cube.Object);
  Mesh->SetRelativeLocation(Center); Mesh->SetRelativeScale3D(Size/100); Mesh->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
 };
 // Retained for isolated legacy worlds without an authored mainland site.
 Block(TEXT("Helipad"),FVector(-3000,2450,45),FVector(880,1100,30));
 Block(TEXT("Approach"),FVector(-3000,1775,20),FVector(500,250,40));
 Block(TEXT("MarkLeft"),FVector(-3140,2450,61),FVector(25,300,2),false);
 Block(TEXT("MarkRight"),FVector(-2860,2450,61),FVector(25,300,2),false);
 Block(TEXT("MarkCenter"),FVector(-3000,2450,61),FVector(280,25,2),false);
 Block(TEXT("ImpactBarrier"),FVector(-3000,3350,250),FVector(850,40,500));
 auto* Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("FlightSign")); Sign->SetupAttachment(Root);
 Sign->SetRelativeLocation(FVector(-3280,1860,180)); Sign->SetRelativeRotation(FRotator(0,-90,0));
 Sign->SetText(NSLOCTEXT("NeonCity","Helipad","SOUTH BEACH / HELIPAD\nF enter / exit on ground\nW S move / A D turn / Q E strafe\nSpace rise / C descend / Mouse look"));
 Sign->SetWorldSize(18);
}
void AAircraftTestArea::PostInitializeComponents()
{
 Super::PostInitializeComponents();
 if(const auto* Site=ANeonCityMainlandSite::Find(GetWorld())) Site->ApplyLegacyTransportFixturePolicy(*this);
}
void AAircraftTestArea::BeginPlay()
{
 Super::BeginPlay();
 const auto* Site=ANeonCityMainlandSite::Find(GetWorld());
 if(Site) Site->ApplyLegacyTransportFixturePolicy(*this);
 if(!HasAuthority()) return;
 const FTransform Transform=Site?Site->GetHelicopterLanding():FTransform(FRotator(0,-90,0),FVector(-3000,2450,137));
 auto* Spawned=GetWorld()->SpawnActorDeferred<ANeonCityHelicopterPrototype>(ANeonCityHelicopterPrototype::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(Spawned) { Spawned->AircraftID=UAircraftWorldSubsystem::HelicopterID(); Spawned->FinishSpawning(Transform); Aircraft=Spawned; }
}
void AAircraftTestArea::EndPlay(EEndPlayReason::Type Reason)
{
 if(Reason==EEndPlayReason::Destroyed && IsValid(Aircraft)) Aircraft->Destroy();
 Super::EndPlay(Reason);
}
