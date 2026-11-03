#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Property/PropertyData.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "PropertySubsystem.generated.h"
class UPropertyComponent;
class ANeonCityBaseVehicle;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityPropertyChanged,FName,PropertyID);

/** Session property registry, the purchase of properties for sale and the daily income of owned businesses. */
UCLASS()
class NEONCITY_API UPropertySubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 TArray<FNeonCityPropertySaveData> BuildPersistenceSnapshot(bool Defaults=false);
 void NormalizePersistenceSnapshot(TArray<FNeonCityPropertySaveData>& Snapshot,TArray<FString>& Warnings) const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityPropertySaveData>& Snapshot);

 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 bool RegisterProperty(UNeonCityPropertyData* Data);
 bool RegisterLiveProperty(UPropertyComponent* Component);
 void UnregisterLiveProperty(UPropertyComponent* Component);
 UFUNCTION(BlueprintPure,Category="Property") bool IsOwned(FName PropertyID) const;
 UFUNCTION(BlueprintPure,Category="Property") TArray<FNeonCityPropertyRecord> GetProperties() const;
 UFUNCTION(BlueprintPure,Category="Property") bool GetProperty(FName PropertyID,FNeonCityPropertyRecord& Record) const;
 UFUNCTION(BlueprintPure,Category="Property") FName GetHomePropertyID() const { return HomePropertyID; }
 UFUNCTION(BlueprintCallable,Category="Property") bool GetHomeSpawnTransform(FTransform& Transform) const;
 /** Records a parked, stationary, unoccupied owned vehicle already in the authored slot. */
 UFUNCTION(BlueprintCallable,Category="Property|Garage") bool StoreVehicle(FName PropertyID,int32 SlotIndex,ANeonCityBaseVehicle* Vehicle);
 UFUNCTION(BlueprintCallable,Category="Property|Garage") bool ReleaseVehicle(FName PropertyID,int32 SlotIndex);
 UFUNCTION(BlueprintCallable,Category="Property|Save") TArray<FNeonCityPropertyRecord> BuildSaveSnapshot();
 ANeonCityBaseVehicle* FindParkedVehicle(FName PropertyID,int32 SlotIndex) const;
 void ReconcileGarage(FName PropertyID);
 /** Buy a property for sale: its price comes out of cash, the rest from the bank. Fails with a reason. */
 bool Purchase(FName PropertyID,FText& Reason);
 /** Pay every owned business for the days since it was last paid (at most 7 at once), once per day. */
 int64 SettleBusinessIncome(int32 GameDay);
 UPROPERTY(BlueprintAssignable) FNeonCityPropertyChanged OnPropertyChanged;
private:
 /** Narrow authored-address migration; never replaces the shared catalogue asset or ownership. */
 bool CanAdoptMainlandPresentation(const UPropertyComponent* Component,const UNeonCityPropertyData* Canonical) const;
 bool IsGarageVehicleEligible(const FNeonCityGarageSlot& Slot,const ANeonCityBaseVehicle* Vehicle) const;
 bool IsParked(const FNeonCityGarageSlot& Slot,const ANeonCityBaseVehicle* Vehicle) const;
 /** Existing links tolerate vertical suspension travel only while real wheels remain over static support. */
 bool CanRetainGarageVehicle(const FNeonCityGarageSlot& Slot,const ANeonCityBaseVehicle* Vehicle) const;
 UPROPERTY() TMap<FName,TObjectPtr<UNeonCityPropertyData>> Catalogue;
 UPROPERTY() TMap<FName,FNeonCityPropertyRecord> Records;
 TMap<FName,TWeakObjectPtr<UPropertyComponent>> LiveProperties;
 FName HomePropertyID;
 /** Home choice among owned properties: an authored preferred home first, then the lexically smallest ID. */
 bool IsBetterHome(FName Candidate,FName Current) const;
};
