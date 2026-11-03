#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleInteraction.generated.h"
class ACharacter;
class APlayerController;
class USceneComponent;

UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UVehicleInteraction : public UActorComponent
{
	GENERATED_BODY()
public:
	UVehicleInteraction();
	UFUNCTION(BlueprintPure, Category="NeonCity|Vehicle")
	bool CanEnter(AActor* Interactor) const;
	UFUNCTION(BlueprintCallable, Category="NeonCity|Vehicle")
	bool Enter(AActor* Interactor);
	UFUNCTION(BlueprintCallable, Category="NeonCity|Vehicle")
	bool Exit();
	UFUNCTION(BlueprintPure, Category="NeonCity|Vehicle")
	ACharacter* GetDriver() const;
	bool RestoreDriver(bool bForce = false);
 bool GetPersistenceExitTransform(FTransform& Transform) const { return FindExitTransform(Driver.Get(),Transform); }
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	bool FindExitTransform(const ACharacter* Character, FTransform& OutTransform, bool AllowWater=false) const;
	void RestoreCharacterState(ACharacter* Character, const FTransform& Transform);
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> Driver;
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> DriverController;
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> OriginalAttachParent;
	FTransform EntryTransform = FTransform::Identity;
	FName OriginalAttachSocket = NAME_None;
	uint8 OriginalMovementMode = 0;
	uint8 OriginalCustomMovementMode = 0;
	bool bOriginalHidden = false;
	bool bOriginalCollision = true;
	bool bRestoring = false;
 bool bOriginalVolumeUpdates=true;
};
