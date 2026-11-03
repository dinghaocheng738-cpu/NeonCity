#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/Camera/MarineCameraSettings.h"
#include "NeonCityCameraExtensionComponent.generated.h"

class UCameraComponent;
class UFixedWingAircraftData;
class USpringArmComponent;
class ANeonCityPlayerCharacter;
class USkinnedMeshComponent;

UENUM(BlueprintType)
enum class ENeonCityCameraContext : uint8
{
	Default,
	Vehicle,
	Aim,
	Indoor,
	Marine,
	Aircraft,
	Fishing
};

/** Opt-in camera policies. No Tick or automatic view-target changes. */
UCLASS(Blueprintable, ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityCameraExtensionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNeonCityCameraExtensionComponent();
	bool BeginAim(UCameraComponent* Camera,USpringArmComponent* Arm);
	void ApplyAimCamera(float DeltaSeconds);
	void EndAim();
	bool BeginFishing(UCameraComponent* Camera,USpringArmComponent* Arm);
	void ApplyFishingCamera(float DeltaSeconds);
	void EndFishing();
 /** Shared room ownership prevents nested volumes from saving an already
  * shortened boom/hidden mesh as the outdoor baseline. No automatic Tick. */
 void ApplyInteriorCamera(AActor* Room,ANeonCityPlayerCharacter* Player,float ArmLength,float Delta);
 bool ReleaseInteriorCamera(AActor* Room);
 void AddVehicleImpact(float Strength);
 void ApplyFixedWingCamera(UCameraComponent* Camera,USpringArmComponent* Arm,const UFixedWingAircraftData& Data,float Speed,float Bank,float Delta);
 void ApplyAircraftCamera(UCameraComponent* Camera,USpringArmComponent* Arm,float Speed,float MaxSpeed,float Altitude,float Delta);
 void ApplyMarineCamera(UCameraComponent* Camera,USpringArmComponent* Arm,float Speed,float MaxSpeed,float Steering,float Delta,
  const FMarineCameraSettings& Rig=FMarineCameraSettings(),bool FirstPerson=false,FRotator Look=FRotator::ZeroRotator);

	UFUNCTION(BlueprintPure, Category="NeonCity|Camera")
	ENeonCityCameraContext GetCameraContext() const { return CameraContext; }

	UFUNCTION(BlueprintCallable, Category="NeonCity|Camera")
	void SetCameraContext(ENeonCityCameraContext NewContext);

	/**
	 * Explicitly update a vehicle-owned camera and spring arm while in Vehicle context.
	 * Speed and MaxSpeed are centimeters per second; DeltaSeconds is elapsed game time.
	 * Signed reverse speed is supported. Default character cameras are never updated.
	 */
	UFUNCTION(BlueprintCallable, Category="NeonCity|Camera|Vehicle")
	void ApplyVehicleCamera(UCameraComponent* Camera, USpringArmComponent* SpringArm,
		float SpeedCmPerSecond, float MaxSpeedCmPerSecond, float DeltaSeconds);

	/** Owners may opt into additional camera policies. Never called automatically. */
	UFUNCTION(BlueprintImplementableEvent, Category="NeonCity|Camera")
	void UpdateCameraExtension(UCameraComponent* Camera, float DeltaSeconds);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="NeonCity|Camera")
	void OnCameraContextChanged(ENeonCityCameraContext PreviousContext, ENeonCityCameraContext NewContext);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NeonCity|Camera|Vehicle", meta=(ClampMin="100.0", Units="cm"))
	float VehicleArmLength = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NeonCity|Camera|Vehicle", meta=(ClampMin="0.0", Units="cm"))
	float VehicleSpeedArmExtension = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NeonCity|Camera|Vehicle", meta=(ClampMin="40.0", ClampMax="120.0", Units="deg"))
	float VehicleFieldOfView = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NeonCity|Camera|Vehicle", meta=(ClampMin="0.0", ClampMax="10.0", Units="deg"))
	float VehicleSpeedFieldOfViewIncrease = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NeonCity|Camera|Vehicle", meta=(ClampMin="0.1"))
	float VehicleCameraInterpolationSpeed = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NeonCity|Camera|Vehicle", meta=(ClampMin="0.1"))
	float VehicleCameraLagSpeed = 6.0f;

private:
 TMap<TWeakObjectPtr<AActor>,float> InteriorRooms;
 TWeakObjectPtr<ANeonCityPlayerCharacter> InteriorPlayer;
 TArray<TPair<TWeakObjectPtr<USkinnedMeshComponent>,bool>> InteriorVisibility;
 float SavedInteriorLength=400;
	TWeakObjectPtr<UCameraComponent> AimCamera;
	TWeakObjectPtr<USpringArmComponent> AimArm;
	TWeakObjectPtr<UCameraComponent> FishingCamera;
	TWeakObjectPtr<USpringArmComponent> FishingArm;
	float SavedFishingFOV=90, SavedFishingLength=400;
	FVector SavedFishingOffset=FVector::ZeroVector;
	float VehicleImpact=0;
	float SavedAimFOV=90, SavedAimLength=400;
	FVector SavedAimOffset=FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NeonCity|Camera", meta=(AllowPrivateAccess="true"))
	ENeonCityCameraContext CameraContext = ENeonCityCameraContext::Default;
};
