#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "AircraftVehicleInterface.generated.h"
UINTERFACE(BlueprintType)
class NEONCITY_API UAircraftVehicleInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IAircraftVehicleInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool EnterAircraft(AActor* Driver);
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool ExitAircraft();
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool StartEngine();
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) void StopEngine();
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) float GetAltitude() const; // ground/water clearance, cm
 // APawn already exposes virtual GetVelocity(); this name avoids a duplicate reflected UFUNCTION.
 virtual FVector GetVelocity() const = 0;
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) EAircraftFlightState GetFlightState() const;
};
/** A future AI controller uses the same validated command port as the player adapter. */
UINTERFACE(BlueprintType)
class NEONCITY_API UAircraftDriverInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IAircraftDriverInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool SubmitFlightInput(AController* Driver,FVector Translation,float Yaw);
};
