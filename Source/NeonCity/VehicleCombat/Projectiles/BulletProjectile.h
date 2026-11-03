#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/ProjectileBase.h"
#include "VehicleCombat/VehicleWeaponData.h"
#include "BulletProjectile.generated.h"
UCLASS()
class NEONCITY_API ABulletProjectile : public AProjectileBase
{
 GENERATED_BODY()
public:
 ABulletProjectile();
 bool LaunchVehicle(FVector Direction,FVector InheritedVelocity,const UVehicleWeaponData& Profile,const FNeonCityDamageEvent& Event,AActor* Target=nullptr);
 FVector GetFlightVelocity() const;
 float GetTravelled() const { return Travelled; }
 virtual bool HasGuidance() const { return false; }
 /** Gravity on the round in flight: bullets drop like real rounds (the aim reticle predicts it). */
 static float BallisticGravityScale(const UVehicleWeaponData& Data);
 /** Effect / momentum size relative to the heavy machine-gun round. */
 static float RoundCalibre(const UVehicleWeaponData& Data);
protected:
 virtual void Tick(float Delta) override;
 virtual void ResolveImpact(AActor* Other,const FHitResult& Hit) override;
 /** Leaves the barrel / rail at this speed (rockets then accelerate under their motor). */
 virtual float LaunchSpeed(const UVehicleWeaponData& Data) const { return Data.MuzzleVelocity; }
 /** Crossed the lake / sea surface: a bullet throws up spray and dies in the water within a metre. */
 virtual void EnterWater(const FVector& Point);
 UPROPERTY() TObjectPtr<UVehicleWeaponData> Profile;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> TracerMesh;
 TWeakObjectPtr<AActor> Target;
 FVector PreviousLocation=FVector::ZeroVector;
 float Travelled=0,Age=0;
 bool bWasAboveWater=true;
};
