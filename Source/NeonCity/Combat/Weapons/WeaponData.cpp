#include "Combat/Weapons/WeaponData.h"
void UWeaponData::Configure(ENeonCityWeaponClass Kind)
{
 WeaponClass=Kind; SlotType=int32(Kind)+1;
 if(Kind==ENeonCityWeaponClass::SMG)
 {
  WeaponId=TEXT("NeonCity_SMG"); DisplayName=FText::FromString(TEXT("NeonCity SMG")); AmmoType=ENeonCityAmmoType::SMGAmmo;
  FireMode=ENeonCityFireMode::Automatic; Damage=12; Range=4500; FireRate=10; MagazineSize=30; StartingReserve=90; ReloadSeconds=1.6f;
  Recoil=.3f; HorizontalRecoil=.16f; RecoverySpeed=3; SustainedFireMultiplier=.08f; Spread=.22f; AimSpread=.45f; HipFireSpread=1.1f; Price=900; InventoryWeight=2;
 }
 else if(Kind==ENeonCityWeaponClass::Rifle)
 {
  WeaponId=TEXT("NeonCity_Rifle"); DisplayName=FText::FromString(TEXT("NeonCity Rifle")); AmmoType=ENeonCityAmmoType::RifleAmmo;
  FireMode=ENeonCityFireMode::Automatic; Damage=32; Range=16000; FireRate=6; MagazineSize=24; StartingReserve=72; ReloadSeconds=2;
  Recoil=.9f; HorizontalRecoil=.1f; RecoverySpeed=2.5f; SustainedFireMultiplier=.12f; Spread=.16f; AimSpread=.18f; HipFireSpread=1.5f; Price=1600; InventoryWeight=3.5f;
 }
 else if(Kind==ENeonCityWeaponClass::Shotgun)
 {
  WeaponId=TEXT("NeonCity_Shotgun"); DisplayName=FText::FromString(TEXT("NeonCity Shotgun")); AmmoType=ENeonCityAmmoType::ShotgunShell;
  Damage=8; DamagePerPellet=8; MaxShotDamage=64; PelletCount=8; Range=3000; FireRate=1.1f; MagazineSize=6; StartingReserve=24; ReloadSeconds=2.2f;
  Recoil=2; HorizontalRecoil=.2f; RecoverySpeed=3; SustainedFireMultiplier=.04f; Spread=.2f; AimSpread=3; HipFireSpread=5; Price=1200; InventoryWeight=4;
 }
}
bool UWeaponData::IsValidProfile() const
{
 for(float V:{Damage,Range,FireRate,ReloadSeconds,Recoil,HorizontalRecoil,RecoverySpeed,SustainedFireMultiplier,Spread,AimSpread,HipFireSpread,MovementSpreadMultiplier,MovementReferenceSpeed,SprintSpreadMultiplier,CrouchSpreadMultiplier,DamagePerPellet,MaxShotDamage,ProjectileSpeed,InventoryWeight})
  if(!FMath::IsFinite(V) || V<0) return false;
 return !WeaponId.IsNone() && uint8(WeaponClass)<=uint8(ENeonCityWeaponClass::Shotgun) && uint8(AmmoType)<=3
  && uint8(FireMode)<=uint8(ENeonCityFireMode::Automatic) && FireRate>=.2f && FireRate<=10 && MagazineSize>=1 && MagazineSize<=100
  && StartingReserve>=0 && StartingReserve<=1000 && Damage>0 && Damage<=1000 && Range>=100 && Range<=100000
  && ReloadSeconds>=.1f && ReloadSeconds<=10 && PelletCount>=1 && PelletCount<=12 && MaxShotDamage>0 && MaxShotDamage<=1000
  && MovementReferenceSpeed>=100 && MovementReferenceSpeed<=1000 && AimSpread<=HipFireSpread && HipFireSpread<=15 && RecoverySpeed>0 && Price>0 && SlotType>=1 && SlotType<=4;
}
