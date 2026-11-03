#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SearchLightData.generated.h"
UCLASS(BlueprintType)
class NEONCITY_API USearchLightData : public UDataAsset
{
 GENERATED_BODY()
public:
 // Candela, calibrated against the existing South Beach night exposure.
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Intensity=12000;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Range=6500;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float InnerCone=14;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float OuterCone=26;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RotationSpeed=60;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DayMultiplier=.08f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float ShadowDistance=8000;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float CullDistance=20000;
 // Separate art gain keeps the air cone readable without clipping sand/paint.
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="16")) float VolumetricScattering=12.f;
 bool IsValidProfile() const;
};
