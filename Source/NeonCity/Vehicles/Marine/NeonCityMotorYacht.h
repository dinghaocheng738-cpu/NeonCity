#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "NeonCityMotorYacht.generated.h"
class UStaticMeshComponent;

/** Licensed 35-foot yacht presentation, using the existing marine controls and persistence. */
UCLASS()
class NEONCITY_API ANeonCityMotorYacht : public ANeonCityMarineVehicleBase
{
 GENERATED_BODY()
public:
 ANeonCityMotorYacht();
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 virtual FText GetInteractionName_Implementation() const override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> YachtBody;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> YachtGlass;
protected:
 virtual void BeginPlay() override;
};
