#pragma once
#include "CoreMinimal.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "CoastalAmbientAnimInstance.generated.h"

/** Only the authored coastal NPC graph reads these licensed activity layers. */
UCLASS(Transient,Blueprintable)
class NEONCITY_API UCoastalAmbientAnimInstance : public UCharacterPresentationAnimInstance
{
 GENERATED_BODY()
public:
 static constexpr float CupCycleSeconds=18.f;
 virtual void NativeUpdateAnimation(float DeltaSeconds)override;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") float AmbientAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") float AmbientSeatAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") int32 AmbientActivityIndex=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") float AmbientIdleOffset=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") float AmbientLookOffset=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") float AmbientTalkOffset=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") float AmbientCupAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") FVector AmbientCupHand=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") FVector AmbientCupElbow=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient") FRotator AmbientCupWrist=FRotator::ZeroRotator;
 /** Read only by the private opt-in cafe graph. Targets are derived from the
  * measured shoe surface and actual floor, never a replacement seated pose. */
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") float AmbientFeetAlpha=0;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") FVector AmbientLeftFoot=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") FVector AmbientRightFoot=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") FVector AmbientLeftKnee=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") FVector AmbientRightKnee=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") bool bAmbientFeetGrounded=false;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") FVector AmbientLeftGroundWorld=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") FVector AmbientRightGroundWorld=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,Category="Coastal Ambient|Feet") int32 AmbientFootFloorQueries=0;
private:
 void UpdateGroundedCafeFeet(float DeltaSeconds,bool Active);
 float FeetTraceCountdown=0;
 bool bFeetFloorCached=false;
 bool bPhaseAssigned=false;
};
