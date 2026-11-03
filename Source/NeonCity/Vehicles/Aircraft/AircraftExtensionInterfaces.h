#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AircraftExtensionInterfaces.generated.h"

/** Attachment metadata only. No weapon ownership, spawning or firing policy. */
UINTERFACE(BlueprintType)
class NEONCITY_API UWeaponHardpointInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IWeaponHardpointInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) TArray<FName> GetWeaponHardpoints() const;
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool GetHardpointTransform(FName Name,FTransform& Transform) const;
};
UINTERFACE(BlueprintType)
class NEONCITY_API ULandingGearInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API ILandingGearInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool IsLandingGearDeployed() const;
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool RequestLandingGear(bool Deployed);
};
/** The caller must be the possessing controller; does not create AI pilots. */
UINTERFACE(BlueprintType)
class NEONCITY_API UAircraftPilotInterface : public UInterface { GENERATED_BODY() };
class NEONCITY_API IAircraftPilotInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool SubmitAircraftControls(AController* Pilot,float Throttle,float Pitch,float Roll,float Yaw,bool Brake);
 // Future LOD scheduler must select an actual simulation implementation before opting out.
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable) bool SupportsDistantSimulation() const;
};
