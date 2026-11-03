#include "Property/PropertySubsystem.h"
#include "Property/PropertyComponent.h"
TArray<FNeonCityPropertySaveData> UPropertySubsystem::BuildPersistenceSnapshot(bool Defaults)
{
 if(!Defaults) BuildSaveSnapshot();
 TArray<FNeonCityPropertySaveData> Result;
 for(const auto& Record:GetProperties())
 {
  FNeonCityPropertySaveData Data; Data.PropertyID=Record.PropertyID;
  const auto* Profile=Catalogue.Find(Record.PropertyID); Data.Owned=Defaults && Profile ? (*Profile)->bInitiallyOwned : Record.Owned;
  Data.LastIncomeDay=Defaults?-1:Record.LastIncomeDay;
  for(const auto& Slot:Record.GarageSlots)
  { FNeonCityGarageSaveData Value; Value.SlotIndex=Slot.SlotIndex; Value.StoredLocation=Slot.Location; if(!Defaults) Value.VehicleID=Slot.VehicleID; Data.GarageSlots.Add(Value); }
  Result.Add(Data);
 }
 return Result;
}
void UPropertySubsystem::NormalizePersistenceSnapshot(TArray<FNeonCityPropertySaveData>& Snapshot,TArray<FString>& Warnings) const
{
 TSet<FName> Seen; TSet<FGuid> Parked;
 Snapshot.RemoveAll([&](auto& Data)
 {
  const auto* Record=Records.Find(Data.PropertyID);
  if(!Record || Seen.Contains(Data.PropertyID)) { Warnings.Add(TEXT("Unknown/duplicate PropertyID skipped.")); return true; }
  Seen.Add(Data.PropertyID); TSet<int32> Slots;
  Data.GarageSlots.RemoveAll([&](auto& Slot)
  {
   if(!Record->GarageSlots.IsValidIndex(Slot.SlotIndex) || Slots.Contains(Slot.SlotIndex)) { Warnings.Add(TEXT("Invalid garage slot skipped.")); return true; }
   Slots.Add(Slot.SlotIndex); Slot.StoredLocation=Record->GarageSlots[Slot.SlotIndex].Location;
   if(!Data.Owned || Parked.Contains(Slot.VehicleID)) Slot.VehicleID.Invalidate();
   if(Slot.VehicleID.IsValid()) Parked.Add(Slot.VehicleID); return false;
  });
  return false;
 });
}
void UPropertySubsystem::ApplyPersistenceSnapshot(const TArray<FNeonCityPropertySaveData>& Snapshot)
{
 HomePropertyID=NAME_None;
 for(auto& Pair:Records)
 {
  auto& Record=Pair.Value; const auto* Data=Snapshot.FindByPredicate([&](const auto& S){ return S.PropertyID==Pair.Key; });
  Record.Owned=Data ? Data->Owned : Catalogue.FindChecked(Pair.Key)->bInitiallyOwned;
  Record.LastIncomeDay=Data && Record.Owned && Record.DailyIncome>0 ? FMath::Max(-1,Data->LastIncomeDay) : -1;
  for(auto& Slot:Record.GarageSlots)
  {
   Slot.VehicleID.Invalidate();
   if(Data && Record.Owned) if(const auto* Value=Data->GarageSlots.FindByPredicate([&](const auto& S){ return S.SlotIndex==Slot.SlotIndex; })) Slot.VehicleID=Value->VehicleID;
  }
  if(Record.Owned && IsBetterHome(Pair.Key,HomePropertyID)) HomePropertyID=Pair.Key;
 }
 // Broadcast only after the complete registry has changed; listeners never see a half-applied property set.
 TArray<FName> IDs; Records.GetKeys(IDs); for(FName ID:IDs) OnPropertyChanged.Broadcast(ID);
}
