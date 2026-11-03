#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TransportOccupantAnimInstance.generated.h"

/** Presentation-only IK targets in mesh component space. No vehicle simulation. */
UCLASS(Transient,Blueprintable)
class NEONCITY_API UTransportOccupantAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector LeftHand;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector RightHand;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector LeftFoot;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector RightFoot;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector LeftElbow;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector RightElbow;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector LeftKnee;
 UPROPERTY(BlueprintReadOnly,Category="Occupant") FVector RightKnee;
};
