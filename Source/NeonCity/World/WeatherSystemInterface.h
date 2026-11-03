#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WeatherSystemInterface.generated.h"

/** Future weather provider contract; no weather simulation. */
UINTERFACE(BlueprintType)
class NEONCITY_API UWeatherSystemInterface : public UInterface
{
	GENERATED_BODY()
};

class NEONCITY_API IWeatherSystemInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|WeatherSystem")
	FName GetWeatherId() const;

	virtual FName GetWeatherId_Implementation() const;
};
