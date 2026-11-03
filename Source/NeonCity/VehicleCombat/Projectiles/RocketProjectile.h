#pragma once
#include "CoreMinimal.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "RocketProjectile.generated.h"
class UExplosionDamageComponent;
class UParticleSystem;
class UParticleSystemComponent;
class UPointLightComponent;
class UStaticMeshComponent;
UCLASS()
class NEONCITY_API ARocketProjectile : public ABulletProjectile
{
 GENERATED_BODY()
public:
 ARocketProjectile();
 bool HasExhaust() const { return bExhaustLit; }
 bool IsMotorBurning() const { return bExhaustLit && !bBurnedOut; }
protected:
 virtual void Tick(float Delta) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void ResolveImpact(AActor* Other,const FHitResult& Hit) override;
 /** Flight: drops off the rail, the motor lights after a moment and accelerates the round to its muzzle
  * velocity, burns for the profile's FuelTime, then the round coasts and falls (RocketProjectile.cpp). */
 virtual float LaunchSpeed(const UVehicleWeaponData& Data) const override;
 /** A rocket or missile striking the water detonates at the surface and throws up a column of spray. */
 virtual void EnterWater(const FVector& Point) override;
 UPROPERTY() TObjectPtr<UExplosionDamageComponent> Explosion;
 /** Motor plume (emissive cone mesh) and its light, carried with the round. */
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Plume;
 UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
 /** Airframe length (cm): the body mesh's nozzle sits this far behind the actor origin's half point. */
 float BodyLength=140.f;
private:
 void LightExhaust();
 void BurnOut();
 FVector Nozzle() const;
 FVector LastPuff=FVector::ZeroVector;
 bool bExhaustLit=false,bBurnedOut=false;
 FVector LaunchDir=FVector::ZeroVector;
};
