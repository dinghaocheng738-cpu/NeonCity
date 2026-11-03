#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TransportOccupantComponent.generated.h"
class ACharacter;
class USkeletalMeshComponent;
class UAnimSequence;
class UAnimInstance;

/** Visible seated representation of the real driver. Possession, safe exit and
 * hidden character collision remain owned by the existing interaction adapters.
 * One mesh follows the vehicle hierarchy, never a per-frame actor teleport. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UTransportOccupantComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTransportOccupantComponent();
 bool ShowDriver(ACharacter* Character);
 void HideDriver();
 USkeletalMeshComponent* GetDriverMesh() const { return DriverMesh; }
 ACharacter* GetRepresentedDriver() const;
 UPROPERTY(EditDefaultsOnly,Category="Occupant") FVector SeatHipLocation=FVector(-5,-43,35);
 UPROPERTY(EditDefaultsOnly,Category="Occupant") float SeatYaw=0;
 /** Additional seat-back rake; pelvis and existing limb IK contacts stay fitted. */
 UPROPERTY(EditDefaultsOnly,Category="Occupant",meta=(ClampMin="0",ClampMax="40")) float SeatReclineDegrees=0;
 UPROPERTY(EditDefaultsOnly,Category="Occupant") FVector LeftHandTarget=FVector(15,-53,66);
 UPROPERTY(EditDefaultsOnly,Category="Occupant") FVector RightHandTarget=FVector(15,-27,66);
 UPROPERTY(EditDefaultsOnly,Category="Occupant") FVector LeftFootTarget=FVector(40,-58,10);
 UPROPERTY(EditDefaultsOnly,Category="Occupant") FVector RightFootTarget=FVector(40,-28,10);
 UPROPERTY(EditDefaultsOnly,Category="Occupant") TObjectPtr<UAnimSequence> SeatedAnimation;
 UPROPERTY(EditDefaultsOnly,Category="Occupant") TSubclassOf<UAnimInstance> SeatedAnimClass;
 /** Vehicles with their own occupant presentation (e.g. rowing) switch the generic seated driver off. */
 UPROPERTY(EditAnywhere,Category="Occupant") bool bPresentDriver=true;
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> DriverMesh;
 TWeakObjectPtr<ACharacter> Driver;
};
