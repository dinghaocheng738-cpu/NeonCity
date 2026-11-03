#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/WorldEventTypes.h"
#include "WorldEventSubsystem.generated.h"
/** Bounded world-event ledger. Future weather, traffic and random-event systems subscribe instead of polling. */
UCLASS()
class NEONCITY_API UWorldEventSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="NeonCity|World")
 FGuid Publish(ENeonCityWorldEventType Type,FVector Location,AActor* SourceActor,AController* Instigator,float Magnitude,FName ContextID=NAME_None);
 const TArray<FNeonCityWorldEvent>& GetRecentEvents() const { return Recent; }
 UFUNCTION(BlueprintPure,Category="NeonCity|World") int32 GetEventCount() const { return EventCount; }
 int32 CountOfType(ENeonCityWorldEventType Type) const;
 void ResetForLoad();
 UPROPERTY(BlueprintAssignable) FNeonCityWorldEventSignal OnWorldEvent;
 static constexpr int32 Capacity=128;
private:
 UPROPERTY() TArray<FNeonCityWorldEvent> Recent;
 int32 EventCount=0;
};