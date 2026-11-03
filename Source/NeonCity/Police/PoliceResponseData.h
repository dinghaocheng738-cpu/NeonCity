#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PoliceResponseData.generated.h"
UENUM(BlueprintType)
enum class EPoliceOffense : uint8 { Attention, Assault, WeaponAttack, VehicleHit, PropertyDamage, PoliceAttack, VehicleDestruction, PublicDanger };
USTRUCT(BlueprintType)
struct FPoliceResponseBudget
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) int32 Officers=1;
 UPROPERTY(EditAnywhere) int32 Vehicles=0;
 UPROPERTY(EditAnywhere) int32 Roadblocks=0;
 UPROPERTY(EditAnywhere) float SearchRadius=600;
 UPROPERTY(EditAnywhere) float ChaseSpeed=1200;
};
UCLASS(BlueprintType)
class NEONCITY_API UPoliceResponseData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPoliceResponseData();
 UPROPERTY(EditAnywhere) TArray<FPoliceResponseBudget> Levels;
 UPROPERTY(EditAnywhere) TMap<EPoliceOffense,int32> Severity;
 UPROPERTY(EditAnywhere) float ReplacementInterval=4;
 UPROPERTY(EditAnywhere) float KnownVehicleIdentificationSeconds=.25f;
 UPROPERTY(EditAnywhere) float PersonIdentificationSeconds=.65f;
 UPROPERTY(EditAnywhere) float EvidenceMemorySeconds=45;
 /** Only a minority of newly dispatched officers; existing patrol loadouts never silently upgrade. */
 UPROPERTY(EditAnywhere) bool EnableRifleResponse=true;
 UPROPERTY(EditAnywhere) int32 RifleMinimumLevel=4;
 UPROPERTY(EditAnywhere) int32 RifleEveryNthOfficer=3;
 bool IsValidProfile() const;
 int32 GetSeverity(EPoliceOffense Offense,float Damage) const;
};
