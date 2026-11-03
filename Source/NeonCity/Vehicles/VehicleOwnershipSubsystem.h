#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Vehicles/VehicleData.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "VehicleOwnershipSubsystem.generated.h"
class ANeonCityBaseVehicle;
USTRUCT(BlueprintType)
struct FNeonCityOwnedVehicle
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly,SaveGame) FGuid VehicleID;
 UPROPERTY(BlueprintReadOnly,SaveGame) ENeonCityVehicleType VehicleType=ENeonCityVehicleType::Sedan;
 UPROPERTY(BlueprintReadOnly,SaveGame) FVector Location=FVector::ZeroVector;
 /** Normalized remaining durability, including zero for a retained destroyed vehicle. */
 UPROPERTY(BlueprintReadOnly,SaveGame) float Condition=1;
};
/** Session registry and value-only snapshots. No purchases, slots or disk IO. */
UCLASS()
class NEONCITY_API UVehicleOwnershipSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 TArray<FNeonCityVehicleSaveData> BuildPersistenceSnapshot(bool Defaults=false);
 void NormalizePersistenceSnapshot(TArray<FNeonCityVehicleSaveData>& Snapshot,TArray<FString>& Warnings) const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityVehicleSaveData>& Snapshot);

 virtual void Deinitialize() override;
 bool RegisterLiveVehicle(ANeonCityBaseVehicle* Vehicle);
 void UnregisterLiveVehicle(ANeonCityBaseVehicle* Vehicle);
 UFUNCTION(BlueprintCallable,Category="Vehicle|Ownership") bool GrantVehicle(ANeonCityBaseVehicle* Vehicle);
 UFUNCTION(BlueprintPure,Category="Vehicle|Ownership") bool IsOwned(FGuid VehicleID) const { return Records.Contains(VehicleID); }
 UFUNCTION(BlueprintCallable,Category="Vehicle|Ownership") TArray<FNeonCityOwnedVehicle> BuildSaveSnapshot();
 /** Call before world vehicles register. All-or-nothing validation; never overwrites a live fleet. */
 UFUNCTION(BlueprintCallable,Category="Vehicle|Ownership") bool RestoreRegistrySnapshot(const TArray<FNeonCityOwnedVehicle>& Snapshot);
 UFUNCTION(BlueprintPure,Category="Vehicle|Ownership") int32 GetOwnedCount() const { return Records.Num(); }
 UFUNCTION(BlueprintPure,Category="Vehicle|Ownership") ANeonCityBaseVehicle* FindLiveVehicle(FGuid VehicleID) const;
 /** Retrieves an owned retired default without changing its identity/profile,
  * health, roof state or any other collectible. Refuses an obstructed bay. */
 UFUNCTION(BlueprintCallable,Category="Vehicle|Ownership") ANeonCityBaseVehicle* DeployStoredVehicle(FGuid VehicleID,const FTransform& BayTransform);
 bool IsLegacyFleetStored(FGuid VehicleID) const;
private:
 bool bInitialSnapshotSealed=false;
 TMap<FGuid,FNeonCityVehicleSaveData> InitialVehicles;
 TMap<FGuid,FNeonCityVehicleSaveData> PersistentVehicles;
 FNeonCityVehicleSaveData CapturePersistence(ANeonCityBaseVehicle* Vehicle) const;
 void Capture(ANeonCityBaseVehicle* Vehicle);
 UPROPERTY() TMap<FGuid,FNeonCityOwnedVehicle> Records;
 TMap<FGuid,TWeakObjectPtr<ANeonCityBaseVehicle>> LiveVehicles;
};
