#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/SystemInterface.h"
#include "NeonCityGameInstanceSubsystem.generated.h"

/** Optional native base for persistent systems; no concrete gameplay systems yet. */
UCLASS(Abstract)
class NEONCITY_API UNeonCityGameInstanceSubsystem : public UGameInstanceSubsystem, public ISystemInterface
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool IsSystemReady_Implementation() const override;

private:
	bool bSystemReady = false;
};
