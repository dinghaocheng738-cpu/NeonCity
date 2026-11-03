#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteriorInterface.generated.h"
UENUM(BlueprintType)
enum class ENeonCityHomeService : uint8 { Sleep, SavePoint };
UINTERFACE(BlueprintType)
class NEONCITY_API UInteriorInterface : public UInterface { GENERATED_BODY() };
/** Interior and life-service boundary independent of the player class. */
class NEONCITY_API IInteriorInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Property|Interior") bool CanEnterInterior(AActor* Visitor) const;
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Property|Interior") bool EnterInterior(AActor* Visitor);
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Property|Interior") bool ExitInterior(AActor* Visitor);
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Property|Interior") bool IsInsideInterior(AActor* Visitor) const;
 /** True means the request was emitted, not that sleep or disk saving was performed. */
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Property|Life") bool RequestHomeService(AActor* Visitor,ENeonCityHomeService Service);
};
