#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Fishing/NeonCityFishingSubsystem.h"
bool UNeonCitySaveGame::MigrateToCurrent(FString& Error)
{
 if(SaveVersion==1)
 {
  // Version 2: the offshore island (and its sea) moved to NeonCityCoastalLayout::IslandOrigin.
  // Every saved world position inside its former footprint moves with it; nothing else changes.
  using namespace NeonCityCoastalLayout;
  RelocateLegacyIslandTransform(Player.Transform);
  for(auto& V:Vehicles)RelocateLegacyIslandTransform(V.Transform);
  for(auto& A:Aircraft)RelocateLegacyIslandTransform(A.Transform);
  for(auto& M:Marine)RelocateLegacyIslandTransform(M.Transform);
  for(auto& P:Properties)for(auto& G:P.GarageSlots)RelocateLegacyIslandPoint(G.StoredLocation);
  for(auto& O:Phone.Opportunities){RelocateLegacyIslandPoint(O.ActivityNPCPosition);RelocateLegacyIslandPoint(O.ActivityTransportPickup);}
  for(auto& G:ResidenceLife.Party.Guests)RelocateLegacyIslandTransform(G.Transform);
  RelocateLegacyIslandTransform(ResidenceLife.Pet.Transform);
  SaveVersion=2;
 }
 if(SaveVersion!=CurrentVersion)
 { Error=FString::Printf(TEXT("Unsupported SaveVersion %d (reader %d); file preserved."),SaveVersion,CurrentVersion); return false; }
 return true;
}
bool UNeonCitySaveGame::ValidateEnvelope(FString& Error) const
{
 if(FormatID!=TEXT("NeonCity.SaveGame") || !SaveID.IsValid()) { Error=TEXT("Missing save identity or format."); return false; }
 if(VehicleWeapons.Num()>256 || Aircraft.Num()>32 || Marine.Num()>64 || Vehicles.Num()>256 || Properties.Num()>128 || Weapons.Num()>32 || Phone.Messages.Num()>100 || Phone.Opportunities.Num()>128 || Economy.CreditReceipts.Num()>1024)
 { Error=TEXT("Save section exceeds supported bounds."); return false; }
 if(HasPlayer && (!NeonCityPersistence::ValidTransform(Player.Transform) || !FMath::IsFinite(Player.MaxHealth) || Player.MaxHealth<=0 || Player.MaxHealth>1000000
  || !FMath::IsFinite(Player.Health) || Player.Health<0 || Player.Health>Player.MaxHealth || !FMath::IsFinite(Player.Stamina) || Player.Stamina<0 || Player.Stamina>1000000 || Player.Money<0))
 { Error=TEXT("Invalid player state."); return false; }
 if(HasPlayer && Player.R05Equipped && Player.WingsuitEquipped)
 { Error=TEXT("Conflicting worn diving and wingsuit equipment."); return false; }
 if(HasEconomy && (Economy.Balance<0 || (HasPlayer && Player.Money!=Economy.Balance))) { Error=TEXT("Invalid or inconsistent wallet."); return false; }
 if(HasEconomy && !UNeonCityEconomySubsystem::ValidatePersistenceSnapshot(Economy,Error))return false;
 if(HasFishing && !UNeonCityFishingSubsystem::ValidateEnvelope(Fishing,Error))return false;
 if(HasWorld && (World.WorldID.IsNone() || !FMath::IsFinite(World.TimeOfDay) || World.TimeOfDay<0 || World.TimeOfDay>=24))
 { Error=TEXT("Invalid world state."); return false; }
 if(HasResidenceLife)
 {
  const auto& P=ResidenceLife.Party;
  if(ResidenceLife.Scenes.Num()>32 || P.Guests.Num()>4 || P.NextArrival>4 || (P.Active && P.Ending) || (!P.Active && !P.Ending && !P.Guests.IsEmpty()))
  {Error=TEXT("Invalid residence facilities or party bounds.");return false;}
  TSet<FName> Zones,Guests;
  for(const auto& S:ResidenceLife.Scenes)
  {if(S.ZoneID.IsNone() || S.Scene>2 || Zones.Contains(S.ZoneID)){Error=TEXT("Invalid or duplicate residence lighting scene.");return false;}Zones.Add(S.ZoneID);}
  for(const auto& G:P.Guests)
  {if(G.GuestID.IsNone() || Guests.Contains(G.GuestID) || !NeonCityPersistence::ValidTransform(G.Transform) || G.Station<0 || G.Station>31 || !FMath::IsFinite(G.WaitSeconds) || G.WaitSeconds<0 || G.WaitSeconds>24){Error=TEXT("Invalid party guest continuation.");return false;}Guests.Add(G.GuestID);}
  const auto& Pet=ResidenceLife.Pet;
  if(Pet.Mode>2 || (Pet.Owned && (Pet.PetID.IsNone() || Pet.PropertyID.IsNone() || !NeonCityPersistence::ValidTransform(Pet.Transform))))
  {Error=TEXT("Invalid residence pet continuation.");return false;}
 }
 return true;
}
void UNeonCitySaveGame::FillMissingSections(const UNeonCitySaveGame& Defaults,TArray<FString>& Warnings)
{
 if(!HasPlayer) { Player=Defaults.Player; Warnings.Add(TEXT("Missing player section: design defaults used.")); }
 if(!HasEconomy) { Economy=Defaults.Economy; if(HasPlayer) Economy.Balance=Player.Money; Warnings.Add(TEXT("Missing economy section: defaults used.")); }
 Player.Money=Economy.Balance;
 if(!HasFishing)Fishing=Defaults.Fishing;
 HasFishing=true;
 if(!HasVehicles) { Vehicles=Defaults.Vehicles; Warnings.Add(TEXT("Missing vehicles: authored defaults used.")); }
 if(!HasProperties) { Properties=Defaults.Properties; Warnings.Add(TEXT("Missing properties: authored defaults used.")); }
 if(!HasWeapons) { Weapons=Defaults.Weapons; Warnings.Add(TEXT("Missing weapons: default inventory used.")); }
 if(!HasPhone) { Phone=Defaults.Phone; Warnings.Add(TEXT("Missing phone: default opportunities used.")); }
 if(!HasWorld) { World=Defaults.World; Warnings.Add(TEXT("Missing world: default clock used.")); }
 if(!HasMarine) { Marine=Defaults.Marine; if(!Marine.IsEmpty()) Warnings.Add(TEXT("Legacy save: authored marine defaults used.")); }
 if(!HasAircraft) { Aircraft=Defaults.Aircraft; if(!Aircraft.IsEmpty()) Warnings.Add(TEXT("Legacy save: authored aircraft defaults used.")); }
 if(!HasVehicleWeapons) { VehicleWeapons=Defaults.VehicleWeapons; if(!VehicleWeapons.IsEmpty()) Warnings.Add(TEXT("Legacy save: authored vehicle loadouts used.")); }
 if(!HasResidenceLife)ResidenceLife=Defaults.ResidenceLife;
 HasResidenceLife=true;
 HasVehicleWeapons=true;
 HasAircraft=true;
 HasPlayer=HasEconomy=HasVehicles=HasProperties=HasWeapons=HasPhone=HasWorld=HasMarine=true;
}
