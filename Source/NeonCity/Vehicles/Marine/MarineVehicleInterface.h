#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MarineVehicleInterface.generated.h"
UINTERFACE(BlueprintType)
class NEONCITY_API UMarineVehicleInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IMarineVehicleInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool EnterVehicle(AActor* Driver);
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool ExitVehicle();
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool StartEngine();
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) void StopEngine();
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) float GetSpeed() const;
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) FVector GetWaterVelocity() const;
 /** Future AI adapters submit through their possessing controller; no NPC driver is created here. */
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool SubmitDriverInput(AController* Driver,float Throttle,float Steering,float Brake);
};
