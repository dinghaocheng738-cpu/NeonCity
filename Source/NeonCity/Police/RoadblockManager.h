#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoadblockManager.generated.h"
class ANeonCityTrafficRoute;
class APoliceVehicle;
UCLASS()
class NEONCITY_API URoadblockManager : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool TryDeploy(ANeonCityTrafficRoute* Road,int32 Maximum);
 void Reset();
 void TrimTo(int32 Maximum);
 int32 GetCount() const { return Cars.Num(); }
 const TArray<TObjectPtr<APoliceVehicle>>& GetCars() const { return Cars; }
 const TArray<FVector>& GetPositions() const { return Positions; }
private:
 UPROPERTY() TArray<TObjectPtr<APoliceVehicle>> Cars;
 UPROPERTY() TArray<TObjectPtr<AActor>> Fixtures;
 TArray<FVector> Positions;
 TArray<int32> FixtureStarts;
};
