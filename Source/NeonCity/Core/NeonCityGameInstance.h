#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "NeonCityGameInstance.generated.h"

/** Session lifetime root. UE owns subsystem creation and teardown. */
UCLASS()
class NEONCITY_API UNeonCityGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure, Category="NeonCity|Core")
	bool IsFrameworkReady() const { return bFrameworkReady; }

private:
	bool bFrameworkReady = false;
};
