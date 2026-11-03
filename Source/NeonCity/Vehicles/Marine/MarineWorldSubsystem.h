#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "Water/WaterStateInterface.h"
#include "MarineWorldSubsystem.generated.h"
class ANeonCityWaterVolume;
class ANeonCityMarineVehicleBase;
class ACoastalOceanSurface;
class ANeonCityMainlandSite;
class UPrimitiveComponent;
struct FTidewaterWaterSample;
#if WITH_DEV_AUTOMATION_TESTS
struct FNeonCityOceanLeaseDiagnostic;
#endif
/** Registered water queries and stable marine snapshots; no Actor references reach disk. */
UCLASS()
class NEONCITY_API UMarineWorldSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 /** Providers may be native volumes or WaterBody adapters implementing IWaterStateInterface. */
 void RegisterWater(AActor* Water);
 void UnregisterWater(AActor* Water);
 bool SampleWater(FVector Point,float& Height,FVector& Current) const;
 bool QueryWater(FVector Point,FWaterSurfaceSample& Sample) const;
 bool QueryHullWater(UPrimitiveComponent* Body,int32 SampleIndex,FVector Point,FWaterSurfaceSample& Sample,FTidewaterWaterSample* Snapshot=nullptr)const;
#if WITH_DEV_AUTOMATION_TESTS
 FNeonCityOceanLeaseDiagnostic QueryHullLeaseDiagnostic(UPrimitiveComponent* Body,int32 SampleIndex,FVector Point)const;
#endif
 bool IsDeepWater(FVector Point,float Submersion=80) const;
 void RegisterBoat(ANeonCityMarineVehicleBase* Boat);
 void UnregisterBoat(ANeonCityMarineVehicleBase* Boat);
 /** Swell samples on the shared ocean for a seagoing hull (idempotent; no-op inland or before the ocean exists). */
 void RegisterOceanSamples(ANeonCityMarineVehicleBase* Boat);
 TArray<FNeonCityMarineSaveData> BuildPersistenceSnapshot(bool Defaults=false) const;
 void NormalizePersistenceSnapshot(TArray<FNeonCityMarineSaveData>& Records,TArray<FString>& Warnings) const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityMarineSaveData>& Records);
 /** Only the canonical boat still at the exact retired mainland default opts in.
  * Player-chosen berths and all record values other than the pose stay intact. */
 static bool MigrateUntouchedMainlandSpeedboatBerth(const ANeonCityMainlandSite* Site,FNeonCityMarineSaveData& Record);
 /** Narrow compatibility for the obstructed, retired C923 yacht berth only. */
 static bool MigrateRetiredReferenceYachtBerth(const ANeonCityMarineVehicleBase* Boat,const FTransform& AuthoredBerth,FNeonCityMarineSaveData& Record,const TArray<FNeonCityMarineSaveData>* OtherRecords=nullptr);
 static FGuid SpeedboatID() { return FGuid(0x18A00001,0x53424541,0x4348424F,0x41543031); }
 static FGuid HarborlineID() { return FGuid(0x205C0001,0x424F4154,0x50415249,0x54590001); }
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
private:
 TArray<TWeakObjectPtr<AActor>> Waters;
 TWeakObjectPtr<ACoastalOceanSurface> Ocean;
 TMap<FGuid,TWeakObjectPtr<ANeonCityMarineVehicleBase>> Boats;
 TMap<FGuid,FNeonCityMarineSaveData> DesignDefaults;
};
