#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/Police/PoliceState.h"
#include "PoliceController.generated.h"
UCLASS()
class NEONCITY_API APoliceController : public AAIController
{
 GENERATED_BODY()
public:
 APoliceController();
 UFUNCTION(BlueprintPure,Category="NeonCity|Police") ENeonCityPoliceState GetPoliceState() const { return State; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Police") FVector GetNavigationTarget() const { return NavigationTarget; }
protected:
 virtual void OnPossess(APawn* InPawn) override;
 virtual void Tick(float DeltaSeconds) override;
private:
 FVector Home=FVector::ZeroVector, NavigationTarget=FVector::ZeroVector;
 ENeonCityPoliceState State=ENeonCityPoliceState::Patrol;
 int32 Waypoint=0;
 float SearchStepTime=0;
};
