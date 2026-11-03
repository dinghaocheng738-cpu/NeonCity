#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/ThreatEvent.h"
#include "ThreatSubsystem.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityThreatSignal,const FNeonCityThreatEvent&,Event);
/** Platform-neutral danger signals; no NPC/crime/police dependency. */
UCLASS()
class NEONCITY_API UThreatSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 FGuid EmitGunshot(AActor* Source,AController* Instigator,FVector Location);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Threat")
 FGuid EmitNearbyDanger(AActor* Source,AController* Instigator,FVector Location,float Radius=1200);
 /** Bystander-visible blast signal. Radius already encodes the authored blast range. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Threat")
 FGuid EmitExplosion(AActor* Source,AController* Instigator,FVector Location,float Radius=1200);
 UPROPERTY(BlueprintAssignable) FNeonCityThreatSignal OnThreat;
private:
 UFUNCTION() void DamageApplied(AActor* Victim,const FNeonCityDamageEvent& Damage);
 FGuid Publish(FNeonCityThreatEvent Event);
};
