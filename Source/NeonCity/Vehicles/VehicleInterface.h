#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VehicleInterface.generated.h"

/** Identity contract for future vehicle actors; no physics or vehicle spawning. */
UINTERFACE(BlueprintType)
class NEONCITY_API UVehicleInterface : public UInterface
{
	GENERATED_BODY()
};

class NEONCITY_API IVehicleInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Vehicle")
	FName GetVehicleId() const;

	virtual FName GetVehicleId_Implementation() const;
};
