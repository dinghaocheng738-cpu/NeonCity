#include "Property/HomeArrival.h"
#include "Core/Persistence/NeonCitySaveGame.h"

namespace NeonCityHomeArrival
{
int32 PrepareSessionStart(FName HomeID,const FTransform& HomeSpawn,const UNeonCitySaveGame& Defaults,UNeonCitySaveGame& Model)
{
 if(HomeID.IsNone() || HomeSpawn.ContainsNaN())return 0;
 auto* Home=Model.Properties.FindByPredicate([HomeID](const auto& P){return P.PropertyID==HomeID;});
 if(!Home || !Home->Owned)return 0;
 Model.Player.Transform=HomeSpawn;
 constexpr double BayRadius=250.;
 int32 Parked=0;
 for(auto& Slot:Home->GarageSlots)
 {
  if(Slot.StoredLocation.ContainsNaN())continue;
  // A bay already holding a car (registered or merely parked there) is left alone.
  const bool Occupied=Model.Vehicles.ContainsByPredicate([&Slot](const auto& V)
   {return V.Condition>0 && FVector::Dist2D(V.Transform.GetLocation(),Slot.StoredLocation)<BayRadius;});
  if(Occupied)continue;
  // The owned car the world authors in this bay, and its exact parked transform there.
  const auto* Authored=Defaults.Vehicles.FindByPredicate([&Slot](const auto& V)
   {return V.Owned && FVector::Dist2D(V.Transform.GetLocation(),Slot.StoredLocation)<BayRadius;});
  if(!Authored)continue;
  auto* Car=Model.Vehicles.FindByPredicate([Authored](const auto& V){return V.VehicleID==Authored->VehicleID;});
  if(!Car || !Car->Owned || Car->LegacyFleetStored || !FMath::IsFinite(Car->Condition) || Car->Condition<=0)continue;
  if(!Car->StoredPropertyID.IsNone() && Car->StoredPropertyID!=HomeID)continue;              // the player's own choice elsewhere
  Car->Transform=Authored->Transform;
  Slot.VehicleID=Car->VehicleID;
  ++Parked;
 }
 return Parked;
}
}
