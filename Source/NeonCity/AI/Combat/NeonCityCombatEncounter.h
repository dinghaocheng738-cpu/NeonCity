#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityCombatEncounter.generated.h"
class ANeonCityNPCCharacter;
class ACoverPoint;
/** Explicit temporary South Beach fixture. Not authored into the map or saved. */
UCLASS()
class NEONCITY_API ANeonCityCombatEncounter : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityCombatEncounter();
 const TArray<TObjectPtr<ANeonCityNPCCharacter>>& GetEnemies() const { return Enemies; }
 const TArray<TObjectPtr<ACoverPoint>>& GetCovers() const { return Covers; }
 static FVector PlayerTestLocation() { return FVector(-6100,1750,98); }
 static FVector HiddenTestLocation() { return FVector(-4800,3250,98); }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TArray<TObjectPtr<ANeonCityNPCCharacter>> Enemies;
 UPROPERTY() TArray<TObjectPtr<ACoverPoint>> Covers;
 UPROPERTY() TArray<TObjectPtr<AActor>> Props;
};
