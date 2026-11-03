#pragma once
#include "CoreMinimal.h"
#include "IslandResidenceLifeSave.generated.h"

/** Optional tagged extensions of the existing shared SaveGame, never a second
 * inventory, wallet or save slot. Stable facility IDs contain values only. */
USTRUCT()
struct FNeonCityResidenceSceneSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName ZoneID;
 UPROPERTY(SaveGame) uint8 Scene=0;
};
USTRUCT()
struct FNeonCityPartyGuestSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName GuestID;
 UPROPERTY(SaveGame) FTransform Transform=FTransform::Identity;
 UPROPERTY(SaveGame) int32 Station=0;
 UPROPERTY(SaveGame) float WaitSeconds=0;
};
USTRUCT()
struct FNeonCityResidencePartySaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName PropertyID;
 UPROPERTY(SaveGame) bool Active=false;
 UPROPERTY(SaveGame) bool Ending=false;
 UPROPERTY(SaveGame) uint8 NextArrival=0;
 UPROPERTY(SaveGame) TArray<FNeonCityPartyGuestSaveData> Guests;
};
USTRUCT()
struct FNeonCityResidencePetSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FName PropertyID;
 UPROPERTY(SaveGame) FName PetID;
 UPROPERTY(SaveGame) bool Owned=false;
 /** 0=home activity, 1=follow, 2=stay. */
 UPROPERTY(SaveGame) uint8 Mode=0;
 UPROPERTY(SaveGame) FTransform Transform=FTransform::Identity;
};
USTRUCT()
struct FNeonCityResidenceLifeSaveData
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) TArray<FNeonCityResidenceSceneSaveData> Scenes;
 UPROPERTY(SaveGame) FNeonCityResidencePartySaveData Party;
 UPROPERTY(SaveGame) FNeonCityResidencePetSaveData Pet;
};
