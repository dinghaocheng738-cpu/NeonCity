#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Fishing/FishData.h"
#include "NeonCityFishingSubsystem.generated.h"

class AFishingStation;
enum class ENeonCityFishingStation : uint8;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNeonCityFishInventoryChanged);

/** Bounded local-player fish ownership and value snapshots; catching remains an input-driven session. */
UCLASS()
class NEONCITY_API UNeonCityFishingSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 bool IsRegisteredStation(const AFishingStation* Station) const;
 void RegisterStation(AFishingStation* Station);
 void UnregisterStation(AFishingStation* Station);
 AFishingStation* FindNearestStation(FVector Location,ENeonCityFishingStation Kind) const;
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 const UFishData* FindSpecies(FName ID) const;
 const TArray<TObjectPtr<UFishData>>& GetSpecies() const { return Species; }
 const TArray<FNeonCityCaughtFish>& GetFish() const { return Fish; }
 const TArray<FNeonCityFishMeal>& GetMeals() const { return Meals; }
 const FNeonCityCaughtFish* FindFish(FGuid ID) const;
 FNeonCityCaughtFish SampleCatch(FRandomStream& Random,FName Habitat,float TimeOfDay,FName Weather,FName PointID) const;
 bool StoreCatch(const FNeonCityCaughtFish& Catch);
 bool ReleaseFish(FGuid FishID);
 bool SellFish(FGuid FishID);
 bool CookFish(FGuid FishID);
 bool ReserveForGrill(FGuid FishID,FName GrillID);
 bool UpdateGrillReservation(FGuid FishID,FName GrillID,ENeonCityGrillStage Stage,float Seconds,float CookFraction);
 bool CancelGrillReservation(FGuid FishID,FName GrillID);
 bool CompleteGrillReservation(FGuid FishID,FName GrillID,FGuid& MealID);
 bool PutInAquarium(FGuid FishID,FName ContainerID,bool Saltwater);
 bool RemoveFromAquarium(FGuid FishID,FName ContainerID);
 bool DeliverFishForOrder(FGuid FishID,FName SpeciesID,float MinimumKg,float MaximumKg,int64 Reward,FName Receipt);
 static bool IsSupportedAquarium(FName ContainerID);
 bool EatMeal(FGuid MealID,AActor* Consumer);
 int64 GetSaleValue(const FNeonCityCaughtFish& Catch) const;
 int32 PouchCount() const;
 bool HasPouchSpace() const { return Fish.Num()<128 && PouchCount()<64; }
 bool IsInventoryOperationInProgress() const { return bChangingInventory; }
 FNeonCityFishingSaveData BuildPersistenceSnapshot(bool Defaults=false) const;
 static bool ValidateEnvelope(const FNeonCityFishingSaveData& Snapshot,FString& Error);
 void NormalizePersistenceSnapshot(FNeonCityFishingSaveData& Snapshot,TArray<FString>& Warnings) const;
 void ApplyPersistenceSnapshot(const FNeonCityFishingSaveData& Snapshot);
 UPROPERTY(BlueprintAssignable) FNeonCityFishInventoryChanged OnInventoryChanged;
private:
 bool ValidFish(const FNeonCityCaughtFish& Record) const;
 UPROPERTY() TArray<TObjectPtr<UFishData>> Species;
 UPROPERTY() TArray<FNeonCityCaughtFish> Fish;
 UPROPERTY() TArray<FNeonCityFishMeal> Meals;
 TMap<FName,TWeakObjectPtr<AFishingStation>> Stations;
 bool bChangingInventory=false;
};
