#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"
UENUM(BlueprintType)
enum class ENeonCityWeaponClass : uint8 { Pistol, SMG, Rifle, Shotgun, Sniper, Heavy, Vehicle, Aircraft };
UENUM(BlueprintType)
enum class ENeonCityAmmoType : uint8 { PistolAmmo, SMGAmmo, RifleAmmo, ShotgunShell };
UENUM(BlueprintType)
enum class ENeonCityFireMode : uint8 { SemiAutomatic, Automatic, Burst };
UENUM(BlueprintType)
// Append values: existing assets and Blueprint enum indices retain their meaning.
enum class ENeonCityImpactSurface : uint8 { Concrete, Character, Metal, Glass, Wood };
UCLASS(BlueprintType)
class NEONCITY_API UWeaponData : public UDataAsset
{
	GENERATED_BODY()
public:
	void Configure(ENeonCityWeaponClass Kind);
	virtual bool IsValidProfile() const;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") FText DisplayName=FText::FromString(TEXT("NeonCity Pistol"));
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") ENeonCityWeaponClass WeaponClass=ENeonCityWeaponClass::Pistol;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") ENeonCityAmmoType AmmoType=ENeonCityAmmoType::PistolAmmo;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") ENeonCityFireMode FireMode=ENeonCityFireMode::SemiAutomatic;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") int32 BurstLength=3;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") int32 PelletCount=1;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") float DamagePerPellet=8;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") float MaxShotDamage=80;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") float ProjectileSpeed=0;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") int64 Price=350;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") float InventoryWeight=1;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") int32 SlotType=1;
	/** Cone half angles in degrees. Spread is the sustained-fire addition. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float Spread=.12f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float AimSpread=.05f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float HipFireSpread=.2f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float MovementSpreadMultiplier=2;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float MovementReferenceSpeed=500;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float SprintSpreadMultiplier=5;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Accuracy") float CrouchSpreadMultiplier=.8f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback") float HorizontalRecoil=.12f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback") float RecoverySpeed=4;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback") float SustainedFireMultiplier=.06f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon") FName WeaponId=TEXT("NeonCity_Pistol");
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon",meta=(ClampMin="1",ClampMax="100")) int32 MagazineSize=12;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon",meta=(ClampMin="0",ClampMax="1000")) int32 StartingReserve=48;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon",meta=(ClampMin="1",ClampMax="1000")) float Damage=25;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon",meta=(ClampMin="100",ClampMax="100000")) float Range=10000;
	/** Maximum shots per second, independently enforced for player and AI requests. */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon",meta=(ClampMin="0.2",ClampMax="10")) float FireRate=4;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Weapon",meta=(ClampMin="0.1",ClampMax="10")) float ReloadSeconds=1.2f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Feedback",meta=(ClampMin="0",ClampMax="5")) float Recoil=0.65f;
};
