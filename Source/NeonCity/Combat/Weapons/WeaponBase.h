#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/CombatInterface.h"
#include "Combat/Weapons/WeaponData.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "WeaponBase.generated.h"
class UWeaponData;
class USceneComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityMuzzleFlash,FTransform,MuzzleTransform);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityWeaponImpact,const FHitResult&,Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityWeaponRecoil,float,Pitch);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCitySurfaceImpact,ENeonCityImpactSurface,Surface,const FHitResult&,Hit);
/** Shared ammo/lifecycle policy. Shot implementations have no player/camera/input dependency. */
UCLASS(Abstract)
class NEONCITY_API AWeaponBase : public AActor
{
	GENERATED_BODY()
public:
 FNeonCityWeaponSaveData BuildPersistenceSnapshot() const;
 FNeonCityWeaponSaveData BuildDefaultSaveData() const;
 void ApplyPersistenceAmmo(const FNeonCityWeaponSaveData& Snapshot);

	AWeaponBase(const FObjectInitializer& Initializer=FObjectInitializer::Get());
	UFUNCTION(BlueprintCallable,Category="NeonCity|Weapon") bool Equip(USceneComponent* Mount,FName Socket=NAME_None);
	UFUNCTION(BlueprintCallable,Category="NeonCity|Weapon") void Unequip();
	UFUNCTION(BlueprintCallable,Category="NeonCity|Weapon") bool Fire(const FCombatAttackRequest& Request);
	UFUNCTION(BlueprintCallable,Category="NeonCity|Weapon") bool Reload();
	void CancelReload();
	int32 GetAmmo() const { return Ammo; }
	int32 GetReserveAmmo() const { return Reserve; }
	bool IsReloading() const { return bReloading; }
	bool IsEquipped() const { return bEquipped; }
	FVector GetMuzzleLocation() const;
	const UWeaponData* GetData() const { return Data; }
	const UWeaponData* GetDefaultData() const;
	float GetSpreadDegrees(const FCombatAttackRequest& Request) const;
	float GetSustainedFire() const;
	float GetLastYawRecoil() const { return LastYawRecoil; }
	int32 GetShotCount() const { return ShotCount; }
	int32 GetLastPelletCount() const { return LastPelletDirections.Num(); }
	const TArray<FVector>& GetLastPelletDirections() const { return LastPelletDirections; }
	void SetReserveAmmo(int32 Value) { Reserve=FMath::Clamp(Value,0,1000); }
	const FHitResult& GetLastHit() const { return LastHit; }
	float GetLastAppliedDamage() const { return LastAppliedDamage; }
	UPROPERTY(BlueprintAssignable) FNeonCityMuzzleFlash OnMuzzleFlash;
	UPROPERTY(BlueprintAssignable) FNeonCityWeaponImpact OnImpactEffect;
	UPROPERTY(BlueprintAssignable) FNeonCityWeaponRecoil OnRecoil;
	UPROPERTY(BlueprintAssignable) FNeonCitySurfaceImpact OnSurfaceImpact;
protected:
	virtual bool FireShot(const FCombatAttackRequest& Request) PURE_VIRTUAL(AWeaponBase::FireShot,return false;);
	/** Muzzle flash and report every successful shot presents (UNeonCityWeaponEffectsSubsystem). */
	virtual ENeonCityWeaponReport GetReportKind() const { return ENeonCityWeaponReport::SmallArms; }
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Muzzle;
	UPROPERTY(EditDefaultsOnly,Category="Weapon") TSoftObjectPtr<UWeaponData> DataAsset;
	UPROPERTY() TObjectPtr<UWeaponData> Data;
	UPROPERTY(VisibleDefaultsOnly,Category="Weapon") TObjectPtr<UWeaponData> DefaultProfile;
	TArray<FVector> LastPelletDirections;
	FHitResult LastHit;
	float LastAppliedDamage=0;
private:
	void FinishReload();
	int32 Ammo=0,Reserve=0;
	bool bEquipped=false,bReloading=false;
	double NextFireTime=0;
	double LastShotTime=-100;
	float SustainedFire=0,LastYawRecoil=0;
	int32 ShotCount=0;
	FTimerHandle ReloadTimer;
};
