#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PropertyData.generated.h"

UENUM(BlueprintType)
enum class ENeonCityPropertyType : uint8 { Apartment, House, Shop, TeaEstate };

/** Authored catalogue data. Session ownership never mutates this shared asset. */
UCLASS(BlueprintType)
class NEONCITY_API UNeonCityPropertyData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") FName PropertyID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") FText DisplayName;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") ENeonCityPropertyType Type=ENeonCityPropertyType::Apartment;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property",meta=(ClampMin="0")) int64 Price=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") FVector Location=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") FText Address;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") bool bInitiallyOwned=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property",meta=(ClampMin="0",ClampMax="8")) int32 GarageSlotCount=0;
 /** Among owned properties, a preferred one is the player's home (spawn / Home in the phone). */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property") bool bPreferredHome=false;
 /** A business (shop, tea estate): what it earns its owner each calendar day, paid at midnight. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Property|Business",meta=(ClampMin="0")) int64 DailyIncome=0;
 bool IsBusiness() const { return Type==ENeonCityPropertyType::Shop || Type==ENeonCityPropertyType::TeaEstate; }
 bool IsValidData() const;
};

USTRUCT(BlueprintType)
struct FNeonCityGarageSlot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly,SaveGame) int32 SlotIndex=0;
 UPROPERTY(BlueprintReadOnly,SaveGame) FVector Location=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly,SaveGame) FGuid VehicleID;
};

/** Value-only boundary for a future save implementation; no actor references. */
USTRUCT(BlueprintType)
struct FNeonCityPropertyRecord
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly,SaveGame) FName PropertyID;
 UPROPERTY(BlueprintReadOnly) FText DisplayName;
 UPROPERTY(BlueprintReadOnly) ENeonCityPropertyType Type=ENeonCityPropertyType::Apartment;
 UPROPERTY(BlueprintReadOnly) int64 Price=0;
 UPROPERTY(BlueprintReadOnly,SaveGame) FVector Location=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FText Address;
 UPROPERTY(BlueprintReadOnly,SaveGame) bool Owned=false;
 UPROPERTY(BlueprintReadOnly,SaveGame) TArray<FNeonCityGarageSlot> GarageSlots;
 UPROPERTY(BlueprintReadOnly) int64 DailyIncome=0;
 /** Calendar day the business was last paid for (-1: never). */
 UPROPERTY(BlueprintReadOnly,SaveGame) int32 LastIncomeDay=-1;
};
