#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/WeaponBase.h"
#include "VehicleCombat/VehicleWeaponData.h"
#include "VehicleMountedWeapon.generated.h"
class ABulletProjectile;
/** Shared WeaponBase owns all ammunition, rate limiting, reload and feedback. */
UCLASS()
class NEONCITY_API AVehicleMountedWeapon : public AWeaponBase
{
 GENERATED_BODY()
public:
 AVehicleMountedWeapon(const FObjectInitializer& Initializer=FObjectInitializer::Get());
 const UVehicleWeaponData* GetVehicleData() const;
 void SetLockedTarget(AActor* Target) { LockedTarget=Target; }
 float GetHeat() const;
 void ResetVehicleTransientState() { Heat=0; HeatAt=0; LockedTarget.Reset(); LastProjectile.Reset(); }
 ABulletProjectile* GetLastProjectile() const { return LastProjectile.Get(); }
protected:
 virtual bool FireShot(const FCombatAttackRequest& Request) override;
 virtual ENeonCityWeaponReport GetReportKind() const override;
 virtual void BeginPlay() override;
 /** Side-mount stores (rocket pod, missile rack): a stub wing from the store back into the fuselage, laid out at
  * BeginPlay toward the carrier's centreline. */
 void AddStubWing();
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> StubWing;
private:
 TWeakObjectPtr<AActor> LockedTarget;
 TWeakObjectPtr<ABulletProjectile> LastProjectile;
 float Heat=0;
 double HeatAt=0;
};
UCLASS()
class NEONCITY_API AVehicleCannon : public AVehicleMountedWeapon
{ GENERATED_BODY() public: AVehicleCannon(); };
/** M261 19-tube 2.75 in rocket pod hung under the pylon (Hydra 70 rockets); fires from the tube bundle. */
UCLASS()
class NEONCITY_API AVehicleRocketPod : public AVehicleMountedWeapon
{ GENERATED_BODY() public: AVehicleRocketPod(); };
/** M299 four-rail launcher with its Hellfire missiles: each loaded round hangs on its rail and disappears when
 * fired (the next shot leaves from the next missile's nose); a reload re-arms the rails. */
UCLASS()
class NEONCITY_API AVehicleMissileLauncher : public AVehicleMountedWeapon
{
 GENERATED_BODY()
public:
 AVehicleMissileLauncher();
 static constexpr int32 Rails=4;
 int32 GetMissilesOnRails() const;
protected:
 virtual void Tick(float Delta) override;
 virtual bool FireShot(const FCombatAttackRequest& Request) override;
private:
 void PresentRails();
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<class UStaticMeshComponent>> RailMissiles;
};
