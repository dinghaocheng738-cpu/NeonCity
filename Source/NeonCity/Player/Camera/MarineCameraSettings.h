#pragma once
#include "CoreMinimal.h"
#include "MarineCameraSettings.generated.h"

/** Boat-local, centimetres. Shared by speedboats, working boats and future yachts. */
USTRUCT(BlueprintType)
struct NEONCITY_API FMarineCameraSettings
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector HelmEye=FVector(-12,-47,146);
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector ChaseAnchor=FVector(0,0,150);
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float ChaseDistance=700;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float HelmFieldOfView=80;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float HelmPitch=-3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float HorizonStabilization=.55f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float ChaseFieldOfView=90;
 bool IsValid() const
 {
  return !HelmEye.ContainsNaN() && !ChaseAnchor.ContainsNaN() && HelmEye.GetAbsMax()<5000 && ChaseAnchor.GetAbsMax()<4000
   && FMath::IsFinite(ChaseDistance) && ChaseDistance>=200 && ChaseDistance<=4000
   && FMath::IsFinite(HelmFieldOfView) && HelmFieldOfView>=50 && HelmFieldOfView<=110
   && FMath::IsFinite(HelmPitch) && HelmPitch>=-30 && HelmPitch<=15
   && FMath::IsFinite(HorizonStabilization) && HorizonStabilization>=0 && HorizonStabilization<=1
   && FMath::IsFinite(ChaseFieldOfView) && ChaseFieldOfView>=50 && ChaseFieldOfView<=110;
 }
};
