#include "Interaction/Examples/NeonCityInteractionTestDoor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ANeonCityInteractionTestDoor::ANeonCityInteractionTestDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
	SetRootComponent(DoorPivot);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetRelativeLocation(FVector(0.f, 50.f, 100.f));
	DoorMesh->SetRelativeScale3D(FVector(0.2f, 1.f, 2.f));
	DoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	DoorMesh->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		DoorMesh->SetStaticMesh(Cube.Object);
	}
}

bool ANeonCityInteractionTestDoor::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor) && Interactor != this && Interactor->GetWorld() == GetWorld()
		&& FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) <= FMath::Square(300.f);
}

FText ANeonCityInteractionTestDoor::GetInteractionName_Implementation() const
{
	return bIsOpen ? NSLOCTEXT("NeonCity", "CloseTestDoor", "Close door")
		: NSLOCTEXT("NeonCity", "OpenTestDoor", "Open door");
}

void ANeonCityInteractionTestDoor::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	bIsOpen = !bIsOpen;
	// Rotate the hinge root; no timeline, art asset, or gameplay system required.
	AddActorLocalRotation(FRotator(0.f, bIsOpen ? 90.f : -90.f, 0.f));
}
