#pragma once
#include "CoreMinimal.h"
#include "PhysicalImpactEvent.generated.h"
UENUM(BlueprintType)
enum class EImpactSeverity : uint8 { Low, Medium, High, Extreme };
UENUM(BlueprintType)
enum class EImpactTargetKind : uint8 { World, Vehicle, Character, Prop };
/** Transient contact evidence. Mass in kg, velocity in cm/s; zero target mass means immovable. */
USTRUCT(BlueprintType)
struct NEONCITY_API FPhysicalImpactEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> SourceActor;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> TargetActor;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
 UPROPERTY(BlueprintReadOnly) FVector RelativeVelocity=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float ImpactSpeed=0;
 UPROPERTY(BlueprintReadOnly) FVector ImpactDirection=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector ImpactNormal=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float SourceMass=0;
 UPROPERTY(BlueprintReadOnly) float TargetMass=0;
 UPROPERTY(BlueprintReadOnly) FVector ImpactLocation=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) TEnumAsByte<EPhysicalSurface> SurfaceType=SurfaceType_Default;
 UPROPERTY(BlueprintReadOnly) float Timestamp=0;
 UPROPERTY(BlueprintReadOnly) EImpactSeverity Severity=EImpactSeverity::Low;
 UPROPERTY(BlueprintReadOnly) float AppliedDamage=0;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhysicalImpactSignal,const FPhysicalImpactEvent&,Event);
