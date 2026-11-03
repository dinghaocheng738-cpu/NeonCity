#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VehicleCombat/TargetingComponent.h"
#include "LockOnSystem.generated.h"
UENUM(BlueprintType)
enum class EVehicleLockState : uint8 { Searching, Locking, Locked, Lost };
UCLASS(BlueprintType)
class NEONCITY_API ULockOnSystem : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float LockTime=1.2f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaxRange=40000;              // 400 m: designator and seeker reach
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float AngleLimit=25;
 UPROPERTY(BlueprintReadOnly) EVehicleLockState State=EVehicleLockState::Searching;
 void Update(const FVehicleTargetInfo& Info,float Delta);
 void Reset();
 AActor* GetLockedTarget() const { return State==EVehicleLockState::Locked?Target.Get():nullptr; }
 AActor* GetCandidate() const { return Target.Get(); }
 float GetProgress() const { return FMath::Clamp(Elapsed/FMath::Max(.1f,LockTime),0.f,1.f); }
private:
 TWeakObjectPtr<AActor> Target;
 float Elapsed=0;
};
