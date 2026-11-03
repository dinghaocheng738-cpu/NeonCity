#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Components/StaticMeshComponent.h"
namespace
{
void Appearance(AActor& Weapon,const TCHAR* Style)
{
 TInlineComponentArray<UStaticMeshComponent*> Parts(&Weapon);
 for(auto* Mesh:Parts) if(Mesh->GetFName()==TEXT("ArtVisual")) NeonCityVisuals::Apply(Mesh,TEXT("Weapons"),Style);
}
}
ANeonCitySMG::ANeonCitySMG() { DataAsset.Reset(); DefaultProfile->Configure(ENeonCityWeaponClass::SMG); Appearance(*this,TEXT("SMG")); Muzzle->SetRelativeLocation(FVector(55,0,8)); }
ANeonCityRifle::ANeonCityRifle() { DataAsset.Reset(); DefaultProfile->Configure(ENeonCityWeaponClass::Rifle); Appearance(*this,TEXT("Rifle")); Muzzle->SetRelativeLocation(FVector(73,0,8)); }
ANeonCityShotgun::ANeonCityShotgun() { DataAsset.Reset(); DefaultProfile->Configure(ENeonCityWeaponClass::Shotgun); Appearance(*this,TEXT("Shotgun")); Muzzle->SetRelativeLocation(FVector(79,0,8)); }
FName NeonCityWeapons::Slot(int32 Index)
{
 static const FName IDs[]={TEXT("NeonCity_Pistol"),TEXT("NeonCity_SMG"),TEXT("NeonCity_Rifle"),TEXT("NeonCity_Shotgun")};
 return Index>=1 && Index<=4?IDs[Index-1]:NAME_None;
}
TSubclassOf<AWeaponBase> NeonCityWeapons::Resolve(FName ID)
{
 if(ID==Slot(1)) return ANeonCityPistol::StaticClass();
 if(ID==Slot(2)) return ANeonCitySMG::StaticClass();
 if(ID==Slot(3)) return ANeonCityRifle::StaticClass();
 if(ID==Slot(4)) return ANeonCityShotgun::StaticClass();
 return nullptr;
}
const UWeaponData* NeonCityWeapons::Profile(FName ID)
{ auto Type=Resolve(ID); return Type?GetDefault<AWeaponBase>(Type)->GetDefaultData():nullptr; }
FName NeonCityWeapons::AssetID(FName ID)
{
 if(ID==Slot(1)) return TEXT("DA_NeonCityPistol");
 if(ID==Slot(2)) return TEXT("Native.Weapon.SMG.v1");
 if(ID==Slot(3)) return TEXT("Native.Weapon.Rifle.v1");
 if(ID==Slot(4)) return TEXT("Native.Weapon.Shotgun.v1");
 return NAME_None;
}
