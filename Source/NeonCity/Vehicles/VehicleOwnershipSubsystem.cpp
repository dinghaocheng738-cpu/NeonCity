#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Combat/HealthComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
bool UVehicleOwnershipSubsystem::RegisterLiveVehicle(ANeonCityBaseVehicle* Vehicle)
{
 if(!IsValid(Vehicle) || Vehicle->GetGameInstance()!=GetGameInstance() || !Vehicle->HasAuthority() || !Vehicle->VehicleID.IsValid()) return false;
 const auto* Existing=LiveVehicles.Find(Vehicle->VehicleID);
 if(Existing && Existing->IsValid()) return Existing->Get()==Vehicle;
 auto* Saved=Records.Find(Vehicle->VehicleID);
 if(Saved && Saved->VehicleType!=Vehicle->GetVehicleProfile()->VehicleType) return false;
 LiveVehicles.Add(Vehicle->VehicleID,Vehicle);
 if(!bInitialSnapshotSealed) { auto Initial=CapturePersistence(Vehicle); Initial.Owned=Vehicle->bStartsOwned; InitialVehicles.Add(Vehicle->VehicleID,Initial); }
 // A matching saved registry restores condition, while future world loading owns location placement.
 if(Saved) Vehicle->GetHealthComponent()->SetCurrentHealth(Saved->Condition*Vehicle->GetHealthComponent()->GetMaxHealth());
 return true;
}
bool UVehicleOwnershipSubsystem::GrantVehicle(ANeonCityBaseVehicle* Vehicle)
{
 if(!IsValid(Vehicle) || Records.Num()>=256 || Records.Contains(Vehicle->VehicleID)) return false;
 const auto* Registered=LiveVehicles.Find(Vehicle->VehicleID);
 if(!Registered || Registered->Get()!=Vehicle) return false;
 FNeonCityOwnedVehicle Record; Record.VehicleID=Vehicle->VehicleID; Record.VehicleType=Vehicle->GetVehicleProfile()->VehicleType;
 Records.Add(Record.VehicleID,Record); Capture(Vehicle); return true;
}
void UVehicleOwnershipSubsystem::Capture(ANeonCityBaseVehicle* Vehicle)
{
 if(!IsValid(Vehicle)) return;
 PersistentVehicles.Add(Vehicle->VehicleID,CapturePersistence(Vehicle));
 auto* Record=Records.Find(Vehicle->VehicleID); if(!Record) return;
 Record->Location=Vehicle->GetActorLocation(); Record->Condition=Vehicle->GetDamageState()->GetConditionFraction();
}
TArray<FNeonCityOwnedVehicle> UVehicleOwnershipSubsystem::BuildSaveSnapshot()
{
 for(auto It=LiveVehicles.CreateIterator();It;++It)
 { if(It.Value().IsValid()) Capture(It.Value().Get()); else It.RemoveCurrent(); }
 TArray<FNeonCityOwnedVehicle> Result; Records.GenerateValueArray(Result);
 Result.Sort([](const auto& A,const auto& B){ return A.VehicleID.ToString()<B.VehicleID.ToString(); }); return Result;
}
void UVehicleOwnershipSubsystem::UnregisterLiveVehicle(ANeonCityBaseVehicle* Vehicle)
{
 if(!Vehicle) return;
 auto* Existing=LiveVehicles.Find(Vehicle->VehicleID);
 if(Existing && Existing->Get()==Vehicle) { Capture(Vehicle); LiveVehicles.Remove(Vehicle->VehicleID); }
}
bool UVehicleOwnershipSubsystem::RestoreRegistrySnapshot(const TArray<FNeonCityOwnedVehicle>& Snapshot)
{
 if(Snapshot.Num()>256) return false;
 for(const auto& Live:LiveVehicles) if(Live.Value.IsValid()) return false;
 TMap<FGuid,FNeonCityOwnedVehicle> Validated;
 for(const auto& Record:Snapshot)
 {
  if(!Record.VehicleID.IsValid() || Validated.Contains(Record.VehicleID) || Record.Location.ContainsNaN()
   || !FMath::IsFinite(Record.Condition) || Record.Condition<0 || Record.Condition>1
   || uint8(Record.VehicleType)>uint8(ENeonCityVehicleType::Pickup)) return false;
  Validated.Add(Record.VehicleID,Record);
 }
 Records=MoveTemp(Validated); LiveVehicles.Reset(); return true;
}
void UVehicleOwnershipSubsystem::Deinitialize() { LiveVehicles.Reset(); Records.Reset(); PersistentVehicles.Reset(); InitialVehicles.Reset(); bInitialSnapshotSealed=false; Super::Deinitialize(); }

ANeonCityBaseVehicle* UVehicleOwnershipSubsystem::FindLiveVehicle(FGuid ID) const
{ const auto* Found=LiveVehicles.Find(ID); return Found ? Found->Get() : nullptr; }
