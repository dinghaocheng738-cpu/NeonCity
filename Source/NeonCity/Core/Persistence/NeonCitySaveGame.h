#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "VehicleCombat/VehicleCombatSaveTypes.h"
#include "Adventure/IslandResidenceLifeSave.h"
#include "NeonCitySaveGame.generated.h"

/** Tagged UE SaveGame model. Presence flags distinguish absent legacy sections from valid empty sections. */
UCLASS()
class NEONCITY_API UNeonCitySaveGame : public USaveGame
{
 GENERATED_BODY()
public:
 /** 2: the Tidewater island moved 800 m south; saved positions on it move with it. */
 static constexpr int32 CurrentVersion=2;
 UPROPERTY(SaveGame) FName FormatID;
 UPROPERTY(SaveGame) int32 SaveVersion=CurrentVersion;
 UPROPERTY(SaveGame) FGuid SaveID;
 UPROPERTY(SaveGame) FDateTime SavedAt;
 UPROPERTY(SaveGame) bool HasPlayer=false;
 UPROPERTY(SaveGame) bool HasEconomy=false;
 UPROPERTY(SaveGame) bool HasVehicles=false;
 UPROPERTY(SaveGame) bool HasProperties=false;
 UPROPERTY(SaveGame) bool HasWeapons=false;
 UPROPERTY(SaveGame) bool HasPhone=false;
 UPROPERTY(SaveGame) bool HasWorld=false;
 UPROPERTY(SaveGame) bool HasVehicleWeapons=false;
 UPROPERTY(SaveGame) TArray<FVehicleWeaponSaveData> VehicleWeapons;
 UPROPERTY(SaveGame) bool HasAircraft=false;
 UPROPERTY(SaveGame) TArray<FNeonCityAircraftSaveData> Aircraft;
 UPROPERTY(SaveGame) bool HasMarine=false;
 UPROPERTY(SaveGame) TArray<FNeonCityMarineSaveData> Marine;
 UPROPERTY(SaveGame) FNeonCityPlayerSaveData Player;
 UPROPERTY(SaveGame) FNeonCityEconomySaveData Economy;
 UPROPERTY(SaveGame) bool HasFishing=false;
 UPROPERTY(SaveGame) FNeonCityFishingSaveData Fishing;
 UPROPERTY(SaveGame) TArray<FNeonCityVehicleSaveData> Vehicles;
 UPROPERTY(SaveGame) TArray<FNeonCityPropertySaveData> Properties;
 UPROPERTY(SaveGame) TArray<FNeonCityWeaponSaveData> Weapons;
 UPROPERTY(SaveGame) FNeonCityPhoneSaveData Phone;
 UPROPERTY(SaveGame) FNeonCityWorldSaveData World;
 UPROPERTY(SaveGame) bool HasResidenceLife=false;
 UPROPERTY(SaveGame) FNeonCityResidenceLifeSaveData ResidenceLife;
 /** Future ordered migrations belong here, before defaults and domain validation. Never downgrade. */
 bool MigrateToCurrent(FString& Error);
 bool ValidateEnvelope(FString& Error) const;
 void FillMissingSections(const UNeonCitySaveGame& Defaults,TArray<FString>& Warnings);
};
