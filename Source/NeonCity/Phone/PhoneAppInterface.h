#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PhoneAppInterface.generated.h"
class APlayerController;

/** Stable app slots, reserved for future implementations; Messages has a minimal prototype view. */
UENUM(BlueprintType)
enum class ENeonCityPhoneApp : uint8 { Contacts, Bank, Map, Vehicles, Properties, Messages, Opportunities, Fishing };
UINTERFACE(BlueprintType)
class NEONCITY_API UPhoneAppInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IPhoneAppInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Phone")
	ENeonCityPhoneApp GetPhoneApp() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|Phone")
	bool OpenPhoneApp(APlayerController* Player);
};
