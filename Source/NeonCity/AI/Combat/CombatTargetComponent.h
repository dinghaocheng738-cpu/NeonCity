#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTargetComponent.generated.h"
UENUM(BlueprintType)
enum class ENeonCityFaction : uint8 { Neutral, Player, Civilian, Hostile, Police };
/** Shared identification boundary. It does not grant perception or combat capability. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UCombatTargetComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCombatTargetComponent() { PrimaryComponentTick.bCanEverTick=false; }
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat") ENeonCityFaction Faction=ENeonCityFaction::Civilian;
 static ENeonCityFaction GetFaction(const AActor* Actor);
 static bool AreFriendly(const AActor* A,const AActor* B);
 static bool IsAliveTarget(const AActor* Actor);
};
