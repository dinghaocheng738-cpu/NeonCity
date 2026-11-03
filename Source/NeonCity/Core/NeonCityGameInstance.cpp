#include "Core/NeonCityGameInstance.h"

void UNeonCityGameInstance::Init()
{
	Super::Init();
	bFrameworkReady = true;
}

void UNeonCityGameInstance::Shutdown()
{
	bFrameworkReady = false;
	Super::Shutdown();
}
