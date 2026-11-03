#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/WeaponData.h"
#include "VehicleWeaponData.generated.h"
UENUM(BlueprintType)
enum class EVehicleWeaponType : uint8 { MachineGun, Cannon, Rocket, Missile };
UENUM(BlueprintType)
enum class EVehicleProjectileType : uint8 { Bullet, Rocket, Missile };
/** WeaponId, MagazineSize (ammo capacity), ReloadSeconds, Damage/Rate/Range/Spread are inherited. */
UCLASS(BlueprintType)
class NEONCITY_API UVehicleWeaponData : public UWeaponData
{
 GENERATED_BODY()
public:
 UVehicleWeaponData();
 void ConfigureVehicle(EVehicleWeaponType Type);
 virtual bool IsValidProfile() const override;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) EVehicleWeaponType WeaponType=EVehicleWeaponType::MachineGun;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) EVehicleProjectileType ProjectileType=EVehicleProjectileType::Bullet;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MuzzleVelocity=30000;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float ExplosionRadius=0;
 /** Blast profile resolved by the unified explosion subsystem; projectiles never author blast numbers. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName ExplosionDataID=TEXT("NeonCity.RocketExplosion");
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Lifetime=5;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float TurnRate=45;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float FuelTime=4;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float HeatPerShot=.08f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float HeatRecovery=.2f;
};
