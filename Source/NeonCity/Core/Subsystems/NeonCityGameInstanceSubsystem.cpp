#include "Core/Subsystems/NeonCityGameInstanceSubsystem.h"

void UNeonCityGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bSystemReady = true;
}

void UNeonCityGameInstanceSubsystem::Deinitialize()
{
	bSystemReady = false;
	Super::Deinitialize();
}

bool UNeonCityGameInstanceSubsystem::IsSystemReady_Implementation() const
{
	return bSystemReady;
}
