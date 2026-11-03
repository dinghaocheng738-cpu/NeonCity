#include "Interaction/InteractiveBuildingDoor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AInteractiveBuildingDoor::AInteractiveBuildingDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot")));
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(RootComponent);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	DoorMesh->SetRelativeLocation(FVector(0.f, 80.f, 120.f));
	DoorMesh->SetRelativeScale3D(FVector(0.16f, 1.6f, 2.4f));
	DoorMesh->SetMobility(EComponentMobility::Movable);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { DoorMesh->SetStaticMesh(Cube.Object); }
}

bool AInteractiveBuildingDoor::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && Interactor != this && Interactor->GetWorld() == GetWorld()
		&& FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) <= FMath::Square(300.f);
}

FText AInteractiveBuildingDoor::GetInteractionName_Implementation() const
{
	return bIsOpen ? NSLOCTEXT("NeonCity", "CloseBuildingDoor", "Close entrance")
		: NSLOCTEXT("NeonCity", "OpenBuildingDoor", "Open entrance");
}

void AInteractiveBuildingDoor::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor)) { return; }
	bIsOpen = !bIsOpen;
	Hinge->SetRelativeRotation(FRotator(0.f, bIsOpen ? 90.f : 0.f, 0.f));
}
