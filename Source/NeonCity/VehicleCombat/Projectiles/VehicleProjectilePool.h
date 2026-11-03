#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VehicleProjectilePool.generated.h"
class ABulletProjectile;
/** Bounded allocation/release interface. v1 destroys returned actors; no hidden permanent projectile pool. */
UCLASS()
class NEONCITY_API UVehicleProjectilePool : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 ABulletProjectile* Acquire(TSubclassOf<ABulletProjectile> Type,const FTransform& Transform,AActor* Owner);
 void Release(ABulletProjectile* Projectile);
 void ResetForLoad();
 int32 GetActiveCount() const;
 static constexpr int32 MaxActive=64;
private:
 TArray<TWeakObjectPtr<ABulletProjectile>> Active;
};
