#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionEvent.h"
#include "Explosion/ExplosionFutureInterfaces.h"
#include "ExplosionSubsystem.generated.h"
class UPrimitiveComponent;
/** World authority for every explosion: occlusion-aware damage, clamped impulse, reactions and events. */
UCLASS()
class NEONCITY_API UNeonCityExplosionSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 /** One detonation request. Legacy adapters and native weapons share this exact path. */
 struct FRequest
 {
  const UNeonCityExplosionData* Data=nullptr;
  FVector Center=FVector::ZeroVector;
  AController* Instigator=nullptr;
  AActor* SourceActor=nullptr;
  TArray<TWeakObjectPtr<AActor>,TInlineAllocator<4>> Ignored;
  int32 ChainDepth=0;
  FGuid ParentExplosionId;
  /** Negative uses the profile damage; adapters that carry a payload override it explicitly. */
  float DamageOverride=-1;
  ECollisionChannel OcclusionChannel=ECC_Visibility;
  bool bApplyImpulse=true;
  bool bNotifyResponses=true;
  bool bEmitDangerSignal=true;
  bool bPublishWorldEvent=true;
 };
 FNeonCityExplosionResult Detonate(const FRequest& Request);
 UFUNCTION(BlueprintCallable,Category="NeonCity|Explosion")
 bool DetonateProfile(FName DataID,FVector Center,AController* Instigator,AActor* SourceActor);
 bool CanDetonate(const AActor* Source,float CooldownSeconds) const;
 /** Future weather pushes one modifier here instead of every system reading its own weather state. */
 UFUNCTION(BlueprintCallable,Category="NeonCity|Explosion") bool SetWeatherModifier(const FNeonCityWeatherModifier& Modifier);
 const FNeonCityWeatherModifier& GetWeatherModifier() const { return Weather; }
 /** Reserved water hook. Records the request and reports unsupported until the water phase is authored. */
 bool SubmitWaterImpact(const FNeonCityWaterExplosionRequest& Request);
 int32 GetWaterImpactCount() const { return WaterImpacts; }
 const TArray<FNeonCityExplosionEvent>& GetRecentExplosions() const { return Recent; }
 int32 GetDetonationCount() const { return DetonationCount; }
 int32 GetRefusedCount() const { return RefusedCount; }
 int32 GetLastDamageTargetCount() const { return LastDamageTargets; }
 int32 GetLastImpulseTargetCount() const { return LastImpulseTargets; }
 int32 GetLastLooseBodyCount() const { return LastLooseBodyTargets; }
 void ResetForLoad();
 static constexpr int32 MaximumDetonationsPerSecond=8;
 static constexpr int32 RecentCapacity=32;
 UPROPERTY(BlueprintAssignable) FNeonCityExplosionSignal OnExplosion;
private:
 UPROPERTY() TArray<FNeonCityExplosionEvent> Recent;
 TArray<float> DetonationTimes;
 TMap<TWeakObjectPtr<const AActor>,float> Cooldowns;
 FNeonCityWeatherModifier Weather;
 int32 DetonationCount=0,RefusedCount=0,WaterImpacts=0,LastDamageTargets=0,LastImpulseTargets=0,LastLooseBodyTargets=0;
 int32 PushLooseBodies(const FRequest& Request,const UNeonCityExplosionData& Data,const TArray<struct FOverlapResult>& Overlaps,
  const TSet<AActor*>& Targets,const TSet<const AActor*>& Ignored,const struct FCollisionQueryParams& Query);
};