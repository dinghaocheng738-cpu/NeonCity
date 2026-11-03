#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VehicleCombat/VehicleWeaponData.h"
#include "WeaponHardpoint.generated.h"
/** Local transform is authored configuration, not a runtime actor-name based identity. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UWeaponHardpointComponent : public USceneComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName HardpointID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<EVehicleWeaponType> AllowedWeaponTypes;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float AimAngle=80;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool FixedForward=false;
 bool Allows(EVehicleWeaponType Type) const { return AllowedWeaponTypes.Contains(Type); }
 FVector ClampAim(FVector Direction) const;
};
