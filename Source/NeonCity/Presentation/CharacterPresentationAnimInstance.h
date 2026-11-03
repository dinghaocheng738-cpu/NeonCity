#pragma once
#include "CoreMinimal.h"
#include "Water/SwimmingAnimInstance.h"
#include "CharacterPresentationAnimInstance.generated.h"
class AWeaponBase;
class UStaticMeshComponent;

/** Presentation reads generic combat/health state. It never changes input,
 * movement, damage, ammunition or ownership. Epic land locomotion stays linked. */
UCLASS(Transient, Blueprintable)
class NEONCITY_API UCharacterPresentationAnimInstance : public USwimmingAnimInstance
{
 GENERATED_BODY()
public:
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float GearChangeAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float GearChangeTime=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float ArmedAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float FlashlightAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float AimAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float AimUpAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float AimDownAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float ReloadAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float ReloadTime=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float ShotAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float ShotTime=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float HitAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float HitTime=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float InjuryAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float IncapacitatedAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float CollapseTime=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float WalkAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float WalkRate=1;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float SupportHandAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FVector SupportHandRightSpace=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FVector SupportElbow=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float FishingAlpha=0;
 /** Body and hand weights are separate so a court player keeps land gait. */
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float FishingBodyAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float CookingBodyAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FRotator CookingSpineRotation=FRotator::ZeroRotator;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") int32 FishingPose=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") float FishingPoseTime=0;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FVector FishingRightHand=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FVector FishingLeftHand=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FRotator FishingRightWrist=FRotator::ZeroRotator;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FRotator FishingLeftWrist=FRotator::ZeroRotator;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FVector FishingRightElbow=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Presentation") FVector FishingLeftElbow=FVector::ZeroVector;
private:
 float CookingLeanDegrees=0;
 TWeakObjectPtr<AWeaponBase> LastWeapon;
 TWeakObjectPtr<UStaticMeshComponent> WeaponVisual;
 int32 LastShotCount=0;
 double ShotAt=-100,ReloadAt=0,CollapseAt=0;
 bool bWasReloading=false,bWasIncapacitated=false;
};
