#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PhonePageInterface.generated.h"
class APlayerController;
enum class ENeonCityPhonePageInput : uint8 { Previous, Next, Accept, Delete, Alternate };
/** Optional native page adapter; keeps new app content out of the controller's phone view. */
UINTERFACE()
class NEONCITY_API UPhonePageInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IPhonePageInterface
{
 GENERATED_BODY()
public:
 virtual FText BuildPage(APlayerController* Player)=0;
 virtual void PageInput(APlayerController* Player,ENeonCityPhonePageInput Input)=0;
};
