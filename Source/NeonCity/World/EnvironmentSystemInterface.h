#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnvironmentSystemInterface.generated.h"

/** Future environment provider contract; no concrete environment logic. */
UINTERFACE(BlueprintType)
class NEONCITY_API UEnvironmentSystemInterface : public UInterface
{
	GENERATED_BODY()
};

class NEONCITY_API IEnvironmentSystemInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|EnvironmentSystem")
	FName GetEnvironmentId() const;

	virtual FName GetEnvironmentId_Implementation() const;
};
