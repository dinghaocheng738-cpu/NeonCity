#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftExtensionInterfaces.h"
#include "NeonCityFixedWingAircraft.generated.h"
class UFixedWingFlightComponent;
UCLASS()
class NEONCITY_API ANeonCityFixedWingAircraft : public ANeonCityAircraftVehicleBase,
 public ILandingGearInterface,public IWeaponHardpointInterface,public IAircraftPilotInterface
{
 GENERATED_BODY()
public:
 ANeonCityFixedWingAircraft(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
 virtual const UAircraftVehicleData* GetProfile() const override;
 UFixedWingFlightComponent* GetFlight() const;
 virtual bool IsSafeRestoreTransform(const FTransform& Transform) const override;
 virtual bool ResolveRestoreTransform(FTransform& Transform,const FTransform& Stand) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 virtual bool IsLandingGearDeployed_Implementation() const override { return true; }
 virtual bool RequestLandingGear_Implementation(bool Deployed) override { return Deployed; }
 virtual TArray<FName> GetWeaponHardpoints_Implementation() const override;
 virtual bool GetHardpointTransform_Implementation(FName Name,FTransform& Transform) const override;
 virtual bool SubmitAircraftControls_Implementation(AController* Pilot,float Throttle,float Pitch,float Roll,float Yaw,bool Brake) override;
 virtual bool SupportsDistantSimulation_Implementation() const override { return false; }
protected:
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 virtual void Tick(float Delta) override;
 virtual void UpdateAircraftCamera(float Delta) override;
 virtual bool CanSupportStandingCharacter() const override;
private:
 void ThrottleAxis(const FInputActionValue& Value);
 void PitchAxis(const FInputActionValue& Value);
 void RollAxis(const FInputActionValue& Value);
 void RudderAxis(const FInputActionValue& Value);
 void BrakeAxis(const FInputActionValue& Value);
 void ExitAircraft();
 float ThrottleDelta=0,PitchCommand=0,RollCommand=0,RudderCommand=0;
 bool bBrakeCommand=false,bInputDirty=false;
 UPROPERTY() TArray<TObjectPtr<UBoxComponent>> Wings;
 UPROPERTY() TMap<FName,TObjectPtr<USceneComponent>> Hardpoints;
 UPROPERTY() TObjectPtr<UInputAction> BrakeAction;
};
