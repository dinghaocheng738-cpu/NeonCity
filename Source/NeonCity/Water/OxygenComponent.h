#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OxygenComponent.generated.h"
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UOxygenComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UOxygenComponent();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaxOxygen=100;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float CurrentOxygen=100;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DrainRate=5;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RecoveryRate=25;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DrowningDamage=10;
 UFUNCTION(BlueprintCallable) void ResetOxygen();
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 float DamageElapsed=0;
};
