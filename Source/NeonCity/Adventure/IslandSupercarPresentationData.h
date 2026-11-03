#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IslandSupercarPresentationData.generated.h"
class UStaticMesh;
class UPhysicsAsset;

USTRUCT(BlueprintType)
struct FIslandCarWheelBinding
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Caliper;
 UPROPERTY(EditAnywhere) FVector CentreCm=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) float RadiusCm=32;
 UPROPERTY(EditAnywhere) float AxleRollDegrees=-90;
};
USTRUCT(BlueprintType)
struct FIslandRoofAnchor
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) FVector BodyPinCm=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) FVector RoofPinCm=FVector::ZeroVector;
};
USTRUCT(BlueprintType)
struct FIslandRoofLeaf
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere) FVector CoverLocalHingeCm=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) float MaximumAngleDegrees=0;
 UPROPERTY(EditAnywhere) FVector2D OpeningPhase=FVector2D(.075,.12);
 UPROPERTY(EditAnywhere) FVector2D ClosingPhase=FVector2D(.72,.82);
};
USTRUCT(BlueprintType)
struct FIslandCarChassisBox
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) FVector CentreCm=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) FVector DimensionsCm=FVector::ZeroVector;
};

/** Presentation and fitted mechanics for the existing third fleet identity.
 * Ownership and persistence continue to use DA_HeroNacre; this data owns no
 * controller, possession, save domain or duplicate vehicle simulation. */
UCLASS(BlueprintType)
class NEONCITY_API UIslandSupercarPresentationData : public UDataAsset
{
 GENERATED_BODY()
public:
 /** Existing720S data defaults to its full mechanism; a fixed-roof coupe requires only the common fitted car data. */
 UPROPERTY(EditAnywhere) bool bRequiresRetractableRoof=true;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Body;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UPhysicsAsset> Chassis;
 UPROPERTY(EditAnywhere) TArray<FIslandCarChassisBox> CollisionBoxes;
 UPROPERTY(EditAnywhere) TArray<FIslandCarWheelBinding> Wheels;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Roof;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Tonneau;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Backlight;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Arm;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Carrier;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> FixedGuides;
 UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> RoofMounts;
 UPROPERTY(EditAnywhere) TArray<FIslandRoofAnchor> Anchors;
 UPROPERTY(EditAnywhere) TArray<FIslandRoofLeaf> Leaves;
 UPROPERTY(EditAnywhere) FVector CoverPivotCm=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) float CycleSeconds=11;
 UPROPERTY(EditAnywhere) float SafeOperationSpeedKmh=8;
 UPROPERTY(EditAnywhere) float ArmLengthCm=60;
 UPROPERTY(EditAnywhere) float ArmClosedDegrees=35;
 UPROPERTY(EditAnywhere) float ArmOpenDegrees=-70;
 UPROPERTY(EditAnywhere) FVector2D CoverOpenPhase=FVector2D(.035,.22);
 UPROPERTY(EditAnywhere) FVector2D CoverClosePhase=FVector2D(.82,1);
 UPROPERTY(EditAnywhere) FVector2D PanelTransferPhase=FVector2D(.22,.82);
 UPROPERTY(EditAnywhere) FVector2D CarrierRaisePhase=FVector2D(.12,.22);
 UPROPERTY(EditAnywhere) FVector2D CarrierLowerPhase=FVector2D(.64,.82);
 UPROPERTY(EditAnywhere) float CarrierLiftCm=4;
 UPROPERTY(EditAnywhere) float CoverPeakDegrees=78;
 UPROPERTY(EditAnywhere) FVector2D BacklightLowerPhase=FVector2D(0,.12);
 UPROPERTY(EditAnywhere) FVector2D BacklightAeroPhase=FVector2D(.86,1);
 UPROPERTY(EditAnywhere) float BacklightDropCm=6;
 UPROPERTY(EditAnywhere) float BacklightAeroReturnCm=3.5;
 UPROPERTY(EditAnywhere) FVector CentreOfMassCm=FVector(-12,0,3);
 UPROPERTY(EditAnywhere) FVector ChassisDimensionsCm=FVector(190,190,110);
 UPROPERTY(EditAnywhere) FVector InteractionHalfExtentsCm=FVector(235,110,70);
 UPROPERTY(EditAnywhere) FVector DriverHipCm=FVector(-23,-40,21);
 UPROPERTY(EditAnywhere,meta=(ClampMin="0",ClampMax="40")) float DriverReclineDegrees=0;
 UPROPERTY(EditAnywhere) FVector LeftHandCm=FVector(34,-54,48);
 UPROPERTY(EditAnywhere) FVector RightHandCm=FVector(34,-26,48);
 UPROPERTY(EditAnywhere) FVector LeftFootCm=FVector(60,-52,-9);
 UPROPERTY(EditAnywhere) FVector RightFootCm=FVector(60,-28,-9);
 UPROPERTY(EditAnywhere) FVector LeftHeadlightCm=FVector(226,-68,29);
 UPROPERTY(EditAnywhere) FVector RightHeadlightCm=FVector(226,68,29);
 UPROPERTY(EditAnywhere) float PeakEngineTorqueNm=770;
 UPROPERTY(EditAnywhere) float MaximumEngineRPM=8500;
 UPROPERTY(EditAnywhere) float FinalDriveRatio=3.308;
 UPROPERTY(EditAnywhere) TArray<float> ForwardGearRatios;
 bool IsComplete() const;
 bool ResolveRequiredAssets();
 FVector PanelTranslationCm(float Progress) const;
 float CarrierHeightCm(float Progress) const;
 float ArmAngleDegrees(float Progress) const;
 static float Phase(float Progress,FVector2D Bounds);
private:
 UPROPERTY(Transient) TArray<TObjectPtr<UObject>> ResolvedAssets;
};
