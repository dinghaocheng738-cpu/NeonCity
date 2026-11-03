#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/WeaponHardpoint.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "Combat/CombatComponent.h"
#include "Vehicles/VehicleInterface.h"
#include "Engine/World.h"
void UVehicleWeaponManager::BeginPlay() { Super::BeginPlay(); GetWorld()->GetSubsystem<UVehicleWeaponRegistry>()->Register(this); }
FName UVehicleWeaponManager::GetVehicleID() const { return GetOwner()->Implements<UVehicleInterface>()?IVehicleInterface::Execute_GetVehicleId(GetOwner()):NAME_None; }
UWeaponHardpointComponent* UVehicleWeaponManager::AddHardpoint(FName ID,const FTransform& Local,const TArray<EVehicleWeaponType>& Allowed,bool FixedForward)
{
 if(ID.IsNone() || Local.ContainsNaN() || Mounts.Num()>=8 || Mounts.ContainsByPredicate([&](const auto& M){return M.Point->HardpointID==ID;})) return nullptr;
 auto* Point=NewObject<UWeaponHardpointComponent>(GetOwner()); Point->HardpointID=ID; Point->AllowedWeaponTypes=Allowed; Point->FixedForward=FixedForward;
 Point->SetupAttachment(GetOwner()->GetRootComponent()); Point->SetRelativeTransform(Local); Point->RegisterComponent();
 auto* Combat=NewObject<UCombatComponent>(GetOwner()); Combat->RegisterComponent();
 FVehicleWeaponMount M; M.Point=Point; M.Combat=Combat; Mounts.Add(M); return Point;
}
TSubclassOf<AVehicleMountedWeapon> UVehicleWeaponManager::ResolveWeapon(FName ID)
{
 if(ID==TEXT("NeonCity.MountedMachineGun")) return AVehicleMountedWeapon::StaticClass();
 if(ID==TEXT("NeonCity.AircraftCannon")) return AVehicleCannon::StaticClass();
 if(ID==TEXT("NeonCity.RocketPod")) return AVehicleRocketPod::StaticClass();
 if(ID==TEXT("NeonCity.GuidedMissile")) return AVehicleMissileLauncher::StaticClass();
 return nullptr;
}
const UVehicleWeaponData* UVehicleWeaponManager::Profile(FName ID) { auto Type=ResolveWeapon(ID); return Type?GetDefault<AVehicleMountedWeapon>(Type)->GetVehicleData():nullptr; }
bool UVehicleWeaponManager::Equip(FName ID,FName WeaponID)
{
 auto* Mount=Mounts.FindByPredicate([&](const auto& M){return M.Point->HardpointID==ID;}); const auto* Data=Profile(WeaponID);
 if(!Mount || !Data || !Mount->Point->Allows(Data->WeaponType)) return false;
 if(!Mount->Combat->EquipWeapon(ResolveWeapon(WeaponID),Mount->Point)) return false;
 if(Mount->DefaultWeaponID.IsNone()) Mount->DefaultWeaponID=WeaponID;
 return true;
}
bool UVehicleWeaponManager::Select(FName ID)
{ for(int32 I=0;I<Mounts.Num();++I) if(Mounts[I].Point->HardpointID==ID) { Selected=I; return true; } return false; }
void UVehicleWeaponManager::NextWeapon() { if(Mounts.Num()) Selected=(Selected+1)%Mounts.Num(); }
UCombatComponent* UVehicleWeaponManager::GetCombat() const { return Mounts.IsValidIndex(Selected)?Mounts[Selected].Combat.Get():nullptr; }
AVehicleMountedWeapon* UVehicleWeaponManager::GetWeapon() const { auto* C=GetCombat(); return C?Cast<AVehicleMountedWeapon>(C->GetWeapon()):nullptr; }
UWeaponHardpointComponent* UVehicleWeaponManager::GetPoint() const { return Mounts.IsValidIndex(Selected)?Mounts[Selected].Point.Get():nullptr; }
TArray<FVehicleWeaponSaveData> UVehicleWeaponManager::BuildSnapshot(bool Defaults) const
{
 TArray<FVehicleWeaponSaveData> Out;
 for(int32 I=0;I<Mounts.Num();++I)
 {
  const auto& M=Mounts[I]; const auto* W=M.Combat->GetWeapon();
  FVehicleWeaponSaveData R; R.VehicleID=GetVehicleID(); R.HardpointID=M.Point->HardpointID; R.Selected=Defaults?I==0:I==Selected;
  if(Defaults) { const auto* D=Profile(M.DefaultWeaponID); if(!D) continue; R.WeaponID=D->WeaponId; R.Ammo=D->MagazineSize; R.Reserve=D->StartingReserve; }
  else { if(!W) continue; R.WeaponID=W->GetData()->WeaponId; R.Ammo=W->GetAmmo(); R.Reserve=W->GetReserveAmmo(); }
  Out.Add(R);
 }
 return Out;
}
bool UVehicleWeaponManager::IsValidRecord(const FVehicleWeaponSaveData& R) const
{
 const auto* D=Profile(R.WeaponID); const auto* M=Mounts.FindByPredicate([&](const auto& V){return V.Point->HardpointID==R.HardpointID;});
 return R.VehicleID==GetVehicleID() && D && M && M->Point->Allows(D->WeaponType) && R.Ammo>=0 && R.Ammo<=D->MagazineSize && R.Reserve>=0 && R.Reserve<=1000;
}
void UVehicleWeaponManager::ApplySnapshot(const TArray<FVehicleWeaponSaveData>& Records)
{
 for(const auto& R:Records)
 {
  if(!IsValidRecord(R)) continue;
  auto* M=Mounts.FindByPredicate([&](const auto& V){return V.Point->HardpointID==R.HardpointID;});
  FNeonCityWeaponSaveData Ammo; Ammo.Ammo=R.Ammo; Ammo.ReserveAmmo=R.Reserve; if(!M->Combat->RestoreMountedWeapon(ResolveWeapon(R.WeaponID),M->Point,Ammo)) continue;
  if(auto* W=Cast<AVehicleMountedWeapon>(M->Combat->GetWeapon())) W->ResetVehicleTransientState();
  if(R.Selected) Select(R.HardpointID);
 }
 if(auto* Adapter=GetOwner()->FindComponentByClass<UVehicleCombatComponent>()) Adapter->ResetTransientState();
}
void UVehicleWeaponManager::EndPlay(EEndPlayReason::Type Reason)
{
 for(auto& M:Mounts) { if(IsValid(M.Combat)) M.Combat->DestroyComponent(); if(IsValid(M.Point)) M.Point->DestroyComponent(); }
 Mounts.Reset(); Super::EndPlay(Reason);
}
bool UVehicleWeaponRegistry::Register(UVehicleWeaponManager* M)
{
 if(!M || M->GetWorld()!=GetWorld() || !M->GetOwner()->HasAuthority() || M->GetVehicleID().IsNone()) return false;
 auto* Existing=Managers.Find(M->GetVehicleID()); if(Existing && Existing->IsValid() && Existing->Get()!=M) return false;
 Managers.Add(M->GetVehicleID(),M); return true;
}
TArray<FVehicleWeaponSaveData> UVehicleWeaponRegistry::BuildSnapshot(bool Defaults) const
{
 TArray<FVehicleWeaponSaveData> Out;
 for(const auto& Pair:Managers) if(Pair.Value.IsValid()) Out.Append(Pair.Value->BuildSnapshot(Defaults));
 Out.Sort([](const auto& A,const auto& B){return A.VehicleID==B.VehicleID?A.HardpointID.LexicalLess(B.HardpointID):A.VehicleID.LexicalLess(B.VehicleID);}); return Out;
}
void UVehicleWeaponRegistry::Normalize(TArray<FVehicleWeaponSaveData>& Records,TArray<FString>& Warnings) const
{
 TSet<FString> Seen; TSet<FName> Selected;
 Records.RemoveAll([&](auto& R)
 {
  const auto* M=Managers.Find(R.VehicleID); const FString Key=R.VehicleID.ToString()+TEXT("/")+R.HardpointID.ToString();
  if(!M || !M->IsValid() || !(*M)->IsValidRecord(R) || Seen.Contains(Key)) { Warnings.Add(TEXT("Unknown/invalid vehicle hardpoint loadout skipped.")); return true; }
  Seen.Add(Key); if(R.Selected && Selected.Contains(R.VehicleID)) R.Selected=false;
  if(R.Selected) Selected.Add(R.VehicleID); return false;
 });
 for(const auto& R:BuildSnapshot(true)) if(!Seen.Contains(R.VehicleID.ToString()+TEXT("/")+R.HardpointID.ToString())) Records.Add(R);
}
void UVehicleWeaponRegistry::ApplySnapshot(const TArray<FVehicleWeaponSaveData>& Records)
{ for(const auto& Pair:Managers) if(Pair.Value.IsValid()) Pair.Value->ApplySnapshot(Records); }
