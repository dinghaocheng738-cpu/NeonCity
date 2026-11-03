#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MarineEcologySubsystem.generated.h"
class ACoastalWildlife;
class AMarineSchool;
class UNeonCityFishingSubsystem;
class UOceanWildlifeCatalog;
class AOceanHabitat;
struct FNeonCityCaughtFish;

/** A shared, immutable 10 Hz neighbour snapshot; fish never scan every actor per frame. */
UCLASS()
class NEONCITY_API UMarineEcologySubsystem : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override;
 virtual void Deinitialize() override;
 int32 GetStreamedSchoolCount() const { return Population.Num(); }
 void RegisterFish(ACoastalWildlife* Fish);
 void UnregisterFish(ACoastalWildlife* Fish);
 FVector SchoolDirection(const ACoastalWildlife* Fish, const FVector& Position, const FVector& Heading);
 ACoastalWildlife* NearestFish(const FVector& Position,float Range);
 void RegisterSchool(AMarineSchool* School);
 void UnregisterSchool(AMarineSchool* School);
 bool NearestCreaturePosition(const FVector& Position,float Range,FVector& Result);
 bool ReserveFishingEncounter(FVector Bobber,float Hour,bool DeepRig,FRandomStream& Random,FNeonCityCaughtFish& Catch,
  FVector& Bait,float& WaitSeconds,FString& Habitat);
 bool MoveFishingEncounter(FGuid Ticket,FVector Target,bool Hooked,bool Landed);
 bool FishingEncounterPosition(FGuid Ticket,FVector& Position) const;
 bool KeepFishingEncounter(FGuid Ticket,UNeonCityFishingSubsystem* Inventory,const FNeonCityCaughtFish& Catch);
 void ReleaseFishingEncounter(FGuid Ticket);
private:
 UPROPERTY(Transient) TObjectPtr<UOceanWildlifeCatalog> Catalog;
 struct FPopulationZone { FIntVector Key;TWeakObjectPtr<AMarineSchool> School;TWeakObjectPtr<AOceanHabitat> Habitat; };
 TArray<FPopulationZone> Population;
 TMap<FIntVector,TSet<int32>> Depleted;
 TMap<FIntVector,double> ZoneRetryAfter;
 double NextPopulationUpdate=0;
 float CatalogRetryAt=0;
 bool SpawnZone(FIntVector Key,const FVector& Eye);
 AOceanHabitat* BuildZoneHabitat(AMarineSchool* School,float Depth,bool Reef,FRandomStream& Random);
 struct FMember { TWeakObjectPtr<ACoastalWildlife> Fish; FVector Position, Heading; FName School; };
 TArray<TWeakObjectPtr<ACoastalWildlife>> Members;
 TArray<TWeakObjectPtr<AMarineSchool>> Schools;
 TArray<FMember> Snapshot;
 TMap<FIntVector,TArray<int32>> Cells;
 double NextSnapshot=-1;
 struct FHook { TWeakObjectPtr<AMarineSchool> School;TWeakObjectPtr<ACoastalWildlife> Fish;int32 Index=INDEX_NONE; };
 TMap<FGuid,FHook> Hooks;
 void RefreshSnapshot();
};
