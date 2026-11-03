#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"
USTRUCT(BlueprintType)
struct FVehicleTargetInfo
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FName TargetID;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Actor;
 UPROPERTY(BlueprintReadOnly) float Distance=0;
 UPROPERTY(BlueprintReadOnly) FVector Direction=FVector::ForwardVector;
 UPROPERTY(BlueprintReadOnly) bool Visible=false;
};
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UTargetingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UTargetingComponent();
 FVehicleTargetInfo FindTarget(FVector Origin,FVector Direction,float Range,float Angle);
 FVehicleTargetInfo Inspect(AActor* Target,FVector Origin,FVector Direction,float Range,float Angle);
 static bool HasLineOfSight(UWorld* World,AActor* Observer,AActor* Target,FVector Origin);
private:
 TMap<TWeakObjectPtr<AActor>,FGuid> RuntimeIDs;
};
