#include "VehicleCombat/VehicleWeaponData.h"
UVehicleWeaponData::UVehicleWeaponData() { ConfigureVehicle(EVehicleWeaponType::MachineGun); }
void UVehicleWeaponData::ConfigureVehicle(EVehicleWeaponType Type)
{
 WeaponType=Type; WeaponClass=ENeonCityWeaponClass::Vehicle; FireMode=ENeonCityFireMode::Automatic;
 Damage=14; FireRate=8; Range=18000; MagazineSize=40; StartingReserve=160; ReloadSeconds=1.8f;
 Recoil=0; HorizontalRecoil=0; Spread=.1f; AimSpread=.1f; HipFireSpread=.1f; MuzzleVelocity=30000;
 ProjectileType=EVehicleProjectileType::Bullet; ExplosionRadius=0; Lifetime=5; FuelTime=4; TurnRate=45;
 WeaponId=TEXT("NeonCity.MountedMachineGun"); DisplayName=FText::FromString(TEXT("Mounted Machine Gun"));
 if(Type==EVehicleWeaponType::Cannon)
 { WeaponId=TEXT("NeonCity.AircraftCannon"); DisplayName=FText::FromString(TEXT("Aircraft Cannon")); Damage=28; FireRate=10; MagazineSize=60; StartingReserve=180; MuzzleVelocity=45000; Range=30000; }
 if(Type==EVehicleWeaponType::Rocket || Type==EVehicleWeaponType::Missile)
 {
  const bool Guided=Type==EVehicleWeaponType::Missile;
  WeaponId=Guided?TEXT("NeonCity.GuidedMissile"):TEXT("NeonCity.RocketPod"); DisplayName=FText::FromString(Guided?TEXT("Guided Missile"):TEXT("Rocket Pod"));
  ProjectileType=Guided?EVehicleProjectileType::Missile:EVehicleProjectileType::Rocket;
  Damage=120; FireRate=1; MagazineSize=Guided?4:6; StartingReserve=Guided?8:12; ReloadSeconds=3;      // a full M299 rack: four Hellfires
  // 400 m reach so a helicopter in its normal hover band can still strike the ground it sees.
  ExplosionRadius=Guided?700:450; MuzzleVelocity=Guided?6000:9000; Range=40000; Lifetime=Guided?8:6;
  ExplosionDataID=Guided?TEXT("NeonCity.MissileExplosion"):TEXT("NeonCity.RocketExplosion");
 }
}
bool UVehicleWeaponData::IsValidProfile() const
{
 for(float V:{Damage,FireRate,Range,ReloadSeconds,Spread,MuzzleVelocity,ExplosionRadius,Lifetime,TurnRate,FuelTime,HeatPerShot,HeatRecovery})
  if(!FMath::IsFinite(V) || V<0) return false;
 return !WeaponId.IsNone() && uint8(WeaponType)<=3 && uint8(ProjectileType)<=2 && Damage>0 && Damage<=500
  && FireRate>=.2f && FireRate<=10 && Range>=100 && Range<=50000 && MagazineSize>0 && MagazineSize<=100
  && StartingReserve>=0 && StartingReserve<=1000 && ReloadSeconds>=.1f && ReloadSeconds<=10
  && MuzzleVelocity>=100 && MuzzleVelocity<=90000 && ExplosionRadius<=800 && Lifetime>=.1f && Lifetime<=10
  && ((uint8(WeaponType)<2 && ProjectileType==EVehicleProjectileType::Bullet) || (WeaponType==EVehicleWeaponType::Rocket && ProjectileType==EVehicleProjectileType::Rocket) || (WeaponType==EVehicleWeaponType::Missile && ProjectileType==EVehicleProjectileType::Missile))
  && TurnRate>0 && TurnRate<=90 && FuelTime>0 && (ProjectileType!=EVehicleProjectileType::Missile || FuelTime<=Lifetime) && Spread<=5 && !ExplosionDataID.IsNone();
}
