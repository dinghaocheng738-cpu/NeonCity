#pragma once
#include "CoreMinimal.h"
#include "Economy/BankingData.h"
#include "Economy/TransactionData.h"
#include "Fishing/FishData.h"
#include "NeonCitySaveTypes.generated.h"

// Disk contracts contain values and explicit IDs only. No Actor, class or UObject references.
USTRUCT(BlueprintType)
struct FNeonCityPlayerSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FTransform Transform=FTransform::Identity;
 UPROPERTY(SaveGame) float Health=100;
 UPROPERTY(SaveGame) float MaxHealth=100;
 UPROPERTY(SaveGame) float Stamina=100;
 UPROPERTY(SaveGame) int64 Money=1850;
 // Additive Version 1 gear inventory. Legacy saves start without an issued kit.
 UPROPERTY(SaveGame) bool OwnsR05=false;
 UPROPERTY(SaveGame) bool OwnsDPV=false;
 UPROPERTY(SaveGame) bool R05Equipped=false;
 // Optional tagged field; old saves retain their ordinary clothes.
 UPROPERTY(SaveGame) bool WingsuitEquipped=false;
 /** Missing legacy fields use the same five starting owned coastal palettes. */
 UPROPERTY(SaveGame) int32 OwnedAppearancePalette=0;
 UPROPERTY(SaveGame) uint8 OwnedAppearanceMask=0x1f;
};
USTRUCT()
struct FNeonCityEconomySaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) int64 Balance=1850;
 UPROPERTY(SaveGame) TArray<FName> CreditReceipts;
 // Optional tagged Version 1 extension. Absent on pre-banking saves.
 UPROPERTY(SaveGame) bool HasBanking=false;
 UPROPERTY(SaveGame) int64 BankBalance=0;
 UPROPERTY(SaveGame) FNeonCityLoanData ActiveLoan;
 UPROPERTY(SaveGame) TArray<FTransactionData> Transactions;
};
USTRUCT()
struct FNeonCityVehicleSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid VehicleID;
 UPROPERTY(SaveGame) uint8 VehicleType=0;
 UPROPERTY(SaveGame) FName DataAssetID;
 UPROPERTY(SaveGame) bool Owned=false;
 UPROPERTY(SaveGame) FTransform Transform=FTransform::Identity;
 UPROPERTY(SaveGame) float Condition=1;
 UPROPERTY(SaveGame) FName StoredPropertyID;
 UPROPERTY(SaveGame) int32 GarageSlot=INDEX_NONE;
 // Tagged Version 1 additions. No fields on a legacy save means closed roof
 // and ordinary world placement. Retired defaults remain owned collectibles.
 UPROPERTY(SaveGame) float RoofProgress=0;
 UPROPERTY(SaveGame) bool RoofTargetOpen=false;
 UPROPERTY(SaveGame) bool LegacyFleetStored=false;
 UPROPERTY(SaveGame) uint8 IslandFleetMigrationVersion=0;
};
USTRUCT()
struct FNeonCityAircraftSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid AircraftID;
 UPROPERTY(SaveGame) FName DataAssetID;
 UPROPERTY(SaveGame) bool Owned=true;
 UPROPERTY(SaveGame) FTransform Transform=FTransform::Identity;
 UPROPERTY(SaveGame) float Condition=1;
};
USTRUCT()
struct FNeonCityMarineSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid VehicleID;
 UPROPERTY(SaveGame) FName DataAssetID;
 UPROPERTY(SaveGame) bool Owned=true;
 UPROPERTY(SaveGame) FTransform Transform=FTransform::Identity;
 UPROPERTY(SaveGame) float Condition=1;
 // Missing in older Version 1 saves. Only a boat still at the exact obsolete
 // prototype berth is relocated; player-chosen positions remain authoritative.
 UPROPERTY(SaveGame) int32 AuthoredBerthVersion=0;
};
USTRUCT()
struct FNeonCityGarageSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) int32 SlotIndex=0;
 UPROPERTY(SaveGame) FGuid VehicleID;
 UPROPERTY(SaveGame) FVector StoredLocation=FVector::ZeroVector;
};
USTRUCT()
struct FNeonCityPropertySaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName PropertyID;
 UPROPERTY(SaveGame) bool Owned=false;
 UPROPERTY(SaveGame) TArray<FNeonCityGarageSaveData> GarageSlots;
 // Additive: a business's last paid calendar day (-1 never paid / not a business).
 UPROPERTY(SaveGame) int32 LastIncomeDay=-1;
};
USTRUCT()
struct FNeonCityWeaponSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName WeaponID;
 UPROPERTY(SaveGame) FName DataAssetID;
 UPROPERTY(SaveGame) bool Owned=true;
 UPROPERTY(SaveGame) int32 Ammo=0;
 UPROPERTY(SaveGame) int32 ReserveAmmo=0;
 UPROPERTY(SaveGame) bool Equipped=false;
};
USTRUCT()
struct FNeonCityMessageSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid MessageID;
 UPROPERTY(SaveGame) FName Sender;
 UPROPERTY(SaveGame) FString Content;
 UPROPERTY(SaveGame) FDateTime Timestamp;
 UPROPERTY(SaveGame) FName ActionID;
};
USTRUCT()
struct FNeonCityOpportunitySaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName OpportunityID;
 UPROPERTY(SaveGame) uint8 State=0;
 UPROPERTY(SaveGame) FGuid VehicleID;
 // Optional island-activity state; legacy providers retain the existing schema.
 UPROPERTY(SaveGame) FGuid ActivityRunID;
 UPROPERTY(SaveGame) uint8 ActivityStage=0;
 UPROPERTY(SaveGame) int32 ActivityCheckpoint=0;
 UPROPERTY(SaveGame) float ActivitySeconds=0;
 UPROPERTY(SaveGame) float ActivityBestSeconds=0;
 UPROPERTY(SaveGame) int32 ActivityCompletions=0;
 UPROPERTY(SaveGame) FString ActivityPhotoFile;
 UPROPERTY(SaveGame) FVector ActivityNPCPosition=FVector::ZeroVector;
 UPROPERTY(SaveGame) bool ActivityNPCPositionValid=false;
 UPROPERTY(SaveGame) FGuid ActivityItemID;
 // Optional Version1 passenger continuation; absent legacy fields use the
 // previous safe pickup fallback. VehicleID above identifies the actual car/boat.
 UPROPERTY(SaveGame) FVector ActivityTransportPickup=FVector::ZeroVector;
 UPROPERTY(SaveGame) bool ActivityTransportPickupValid=false;
 UPROPERTY(SaveGame) bool ActivityPassengerExiting=false;
};
USTRUCT()
struct FNeonCityPhoneSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) TArray<FNeonCityMessageSaveData> Messages;
 UPROPERTY(SaveGame) TArray<FNeonCityOpportunitySaveData> Opportunities;
};
USTRUCT()
struct FNeonCityWorldSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName WorldID;
 UPROPERTY(SaveGame) float TimeOfDay=10;
 // Additive Version-1 field: older saves retain the authored Light state.
 UPROPERTY(SaveGame) uint8 CoastalSeaState=1;
 // Additive: the calendar day (business income, weather); older saves start on day 0.
 UPROPERTY(SaveGame) int32 GameDay=0;
};

namespace NeonCityPersistence
{
 inline bool ValidTransform(const FTransform& Value)
 {
  return !Value.ContainsNaN() && Value.GetRotation().IsNormalized()
   && Value.GetScale3D().Equals(FVector::OneVector,.001)
   && Value.GetLocation().GetAbsMax()<10000000;
 }
}
