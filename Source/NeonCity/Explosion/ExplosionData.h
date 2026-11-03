#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/DamageEvent.h"
#include "Physics/PhysicalImpactEvent.h"
#include "ExplosionData.generated.h"
UENUM(BlueprintType)
enum class ENeonCityExplosionSourceType : uint8 { Generic, Grenade, Vehicle, Rocket, Missile, Environmental };
/** Per-kind radial impulse limits. Impulse is mass * bounded delta velocity, never unbounded force. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityImpulsePolicy
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float VehicleDeltaVelocity=650;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float PropDeltaVelocity=1400;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float WorldDeltaVelocity=0;
 /** Characters receive damage and hit reaction, never a launch impulse. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float CharacterDeltaVelocity=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaximumImpulse=90000;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaximumDeltaVelocity=1800;
 /** A ground blast reflects off the surface and lifts what it pushes: 0 is purely radial, 1 adds an equal upward part. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="1")) float UpwardBias=0;
 /** Fraction of the body radius the impulse is applied off-centre (toward the blast), so pushed bodies also tumble. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="1")) float TumbleOffset=0;
 /** Simulating bodies that are not combat targets (debris, loose props, dropped parts). 0 leaves them untouched. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float LooseBodyDeltaVelocity=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="64")) int32 MaximumLooseBodies=24;
 /** Speed handed to a character's bounded hit reaction (stagger / knockdown); 0 keeps damage-only reactions. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float CharacterReactionSpeed=0;
};
/** Single authoritative explosion profile. Radius, damage, impulse and feedback are data, not projectile constants. */
UCLASS(BlueprintType)
class NEONCITY_API UNeonCityExplosionData : public UDataAsset
{
 GENERATED_BODY()
public:
 UNeonCityExplosionData();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion") FName ExplosionID=TEXT("NeonCity.SmallExplosion");
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="1",ClampMax="5000")) float Damage=60;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0",ClampMax="800")) float InnerRadius=120;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="1",ClampMax="800")) float OuterRadius=500;
 /** Falloff exponent. 1 is linear, above 1 keeps the inner blast lethal and the fringe mild. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0.25",ClampMax="6")) float DamageFalloff=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0",ClampMax="1000")) float ImpulseStrength=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0",ClampMax="800")) float ImpulseRadius=500;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0.2",ClampMax="10")) float CooldownSeconds=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion") ENeonCityDamageType DamageType=ENeonCityDamageType::Explosion;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion") ENeonCityExplosionSourceType SourceType=ENeonCityExplosionSourceType::Generic;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion") bool bRequiresLineOfSight=true;
 /** Damage retained through a wall. 0 forbids pass-through damage entirely. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0",ClampMax="1")) float OccludedDamageFraction=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion",meta=(ClampMin="0",ClampMax="1")) float OccludedImpulseFraction=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0",ClampMax="60")) float FireDuration=6;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0",ClampMax="60")) float SmokeDuration=10;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Targets",meta=(ClampMin="0",ClampMax="4")) float EnvironmentDamageMultiplier=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Targets",meta=(ClampMin="0",ClampMax="4")) float VehicleDamageMultiplier=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Targets",meta=(ClampMin="0",ClampMax="4")) float CharacterDamageMultiplier=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Targets",meta=(ClampMin="1",ClampMax="64")) int32 MaximumTargets=24;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Targets") FNeonCityImpulsePolicy Impulse;
 /** Bystanders hear the blast; a future weather system scales fire/smoke/impulse through the same profile. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback") bool bEmitDangerSignal=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0.2",ClampMax="4")) float DangerRadiusScale=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weather",meta=(ClampMin="0",ClampMax="2")) float RainImpulseScale=1;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weather",meta=(ClampMin="0",ClampMax="2")) float UnderwaterDamageScale=.35f;
 bool IsValidProfile() const;
 /** Upper bound of an authored impulse cap (kg cm/s): a 2.5 t vehicle at the 1000 cm/s ceiling. */
 static constexpr float MaximumAuthoredImpulse=3000000.f;
 /** Radius attenuation with a flat lethal core. */
 static float Falloff(float Distance,float InnerRadius,float OuterRadius,float Exponent);
 float DamageAt(float Distance) const;
 float ImpulseAt(float Distance) const;
 float DamageMultiplier(EImpactTargetKind Kind) const;
 float ImpulseDeltaVelocityLimit(EImpactTargetKind Kind) const;
 static EImpactTargetKind KindOf(const AActor* Actor);
};
namespace NeonCityExplosions
{
 /** Native authored profiles: SmallExplosion, VehicleExplosion, RocketExplosion, MissileExplosion. */
 NEONCITY_API const UNeonCityExplosionData& Profile(FName ID);
 NEONCITY_API FName NativeID(ENeonCityExplosionSourceType Type);
 NEONCITY_API bool IsNativeID(FName ID);
}