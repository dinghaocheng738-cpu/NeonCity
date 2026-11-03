#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "Physics/PhysicalImpactEvent.h"
#include "ImpactDamageData.generated.h"
UCLASS(BlueprintType)
class NEONCITY_API UImpactDamageData : public UDataAsset
{
 GENERATED_BODY()
public:
 UImpactDamageData();
 UPROPERTY(EditAnywhere) FRuntimeFloatCurve VehicleDamage;
 UPROPERTY(EditAnywhere) FRuntimeFloatCurve CharacterDamage;
 UPROPERTY(EditAnywhere) FRuntimeFloatCurve PropDamage;
 UPROPERTY(EditAnywhere) float MinimumSpeed=150;
 UPROPERTY(EditAnywhere) float MediumSpeed=500;
 UPROPERTY(EditAnywhere) float HighSpeed=1100;
 UPROPERTY(EditAnywhere) float ExtremeSpeed=2200;
 UPROPERTY(EditAnywhere) float PairCooldown=.6f;
 UPROPERTY(EditAnywhere) float MaximumSpeed=6000;
 UPROPERTY(EditAnywhere) float MaximumPropSpeed=600;
 float Damage(float NormalSpeed,float SourceMass,float TargetMass,EImpactTargetKind Kind,float SurfaceScale=1) const;
 EImpactSeverity Severity(float EquivalentSpeed) const;
 bool IsValidProfile() const;
};
