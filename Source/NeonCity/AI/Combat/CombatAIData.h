#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "CombatAIData.generated.h"
class AWeaponBase;
UENUM(BlueprintType)
enum class ENeonCityCombatRole : uint8 { Civilian, ArmedCivilian, Security, Hostile };
/** Opt-in decision tuning. Weapon ballistics/ammo remain in the existing WeaponData. */
UCLASS(BlueprintType)
class NEONCITY_API UCombatAIData : public UDataAsset
{
 GENERATED_BODY()
public:
 UCombatAIData();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSubclassOf<AWeaponBase> WeaponClass;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityCombatRole Role=ENeonCityCombatRole::Civilian;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool CanFight=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool RequiresArmedThreat=false;
 // Wanted response is distinct from autonomous policing of every unrelated NPC encounter.
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool PlayerResponseOnly=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityFaction Faction=ENeonCityFaction::Civilian;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<ENeonCityFaction> ProactiveEnemies={ENeonCityFaction::Player};
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool AllowChase=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool RetreatWhenInjured=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RetreatHealthFraction=.35f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Accuracy=.55f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float AimError=5.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float ReactionTime=.8f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 BurstLength=3;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float BurstPause=.9f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float EngagementRange=1600.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float SightRange=2200.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float SightHalfAngle=85.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float SearchSeconds=7.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float ChaseLeash=2200.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaximumCombatSeconds=45.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MoveSpeed=240.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float CoverRadius=1100.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float PerceptionInterval=.2f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float CoverQueryInterval=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float FarUpdateInterval=1.f;
 bool IsValidProfile() const;
};
