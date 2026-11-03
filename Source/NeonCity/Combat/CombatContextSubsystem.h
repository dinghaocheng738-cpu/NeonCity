#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DamageEvent.h"
#include "CombatContextSubsystem.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNeonCityCombatContextReset);
/** Bounded transient aggression pairs. Context is evidence, not a complete legal verdict. */
UCLASS()
class NEONCITY_API UCombatContextSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 void RecordAggression(AActor* Attacker,AActor* Victim);
 void AnnotateDamage(AActor* Victim,FNeonCityDamageEvent& Event);
 void ResetForLoad();
 UPROPERTY(BlueprintAssignable) FNeonCityCombatContextReset OnReset;
private:
 struct FEpisode { FGuid Id; TWeakObjectPtr<AActor> Aggressor,Defender; float Expires=0; };
 FEpisode* Find(AActor* A,AActor* B);
 TArray<FEpisode> Episodes;
};
