#include "World/NeonCityWorldRegion.h"
#include "World/NeonCityWorldManager.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ANeonCityWorldRegion::ANeonCityWorldRegion()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RegionRoot")));
	PrimaryActorTick.bCanEverTick = false;
}

void ANeonCityWorldRegion::BeginPlay()
{
	Super::BeginPlay();
	if (!RegionId.IsNone())
	{
		if (UNeonCityWorldManager* Manager = GetWorld()->GetSubsystem<UNeonCityWorldManager>())
		{
			Manager->SetCurrentRegion(RegionId);
		}
	}
}

void ANeonCityWorldRegion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNeonCityWorldManager* Manager = GetWorld()->GetSubsystem<UNeonCityWorldManager>())
	{
		if (Manager->GetCurrentRegion() == RegionId) { Manager->SetCurrentRegion(NAME_None); }
	}
	Super::EndPlay(EndPlayReason);
}
