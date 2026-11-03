#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PoliceSensorComponent.generated.h"
/** Only SearchSystem may turn a live candidate position into a validated sighting. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UPoliceSensorComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPoliceSensorComponent();
 FVector GetEyeLocation() const;
 FVector GetViewDirection() const;
 bool CanObserve() const;
 void SetScanning(bool Value) { bScanning=Value; }
 void FocusEvidence(FVector Location,bool bEnable) { FocusLocation=Location; bFocus=bEnable; }
 bool bSurveillance=false;
 float NextObservationTime=0;
 UPROPERTY(EditAnywhere,Category="Perception") float SightRange=2500;
 UPROPERTY(EditAnywhere,Category="Perception") float HalfAngleDegrees=85;
 UPROPERTY(EditAnywhere,Category="Perception") FVector EyeOffset=FVector(0,0,65);
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 FVector FocusLocation=FVector::ZeroVector;
 bool bScanning=false,bFocus=false;
};
