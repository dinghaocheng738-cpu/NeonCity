#pragma once
#include "CoreMinimal.h"
#include "Adventure/DiveLocker.h"
#include "WingsuitLaunchPoint.generated.h"
UCLASS()
class NEONCITY_API AWingsuitLaunchPoint : public ADiveLocker
{
 GENERATED_BODY()
public:
 AWingsuitLaunchPoint();
 UPROPERTY(EditAnywhere,Category="Launch Safety") bool bRoofSafetyGate=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Launch Safety") TObjectPtr<UStaticMeshComponent> SafetyGate;
 bool IsGateOpen() const { return GateFraction>.99f; }
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void Tick(float DeltaSeconds) override;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
protected:
 virtual void BeginPlay() override;
private:
 TWeakObjectPtr<AActor> ArmedVisitor;
 float GateFraction=0;
};
