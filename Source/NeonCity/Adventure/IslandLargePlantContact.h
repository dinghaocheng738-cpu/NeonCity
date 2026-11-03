#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IslandLargePlantContact.generated.h"
class UCapsuleComponent;
/** A small predictive capsule pool for mature shrubs; grass stays nonblocking. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UIslandLargePlantContact : public UActorComponent
{
 GENERATED_BODY()
public:
 UIslandLargePlantContact();
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 struct FShrub { FVector Root;float Radius,Height; };
 TArray<FShrub> Shrubs;
 TMap<FIntPoint,TArray<int32>> Cells;
 UPROPERTY() TArray<TObjectPtr<UCapsuleComponent>> Pool;
 TMap<int32,UCapsuleComponent*> ActiveShrubs;
 TArray<UCapsuleComponent*> FreeCapsules;
};
