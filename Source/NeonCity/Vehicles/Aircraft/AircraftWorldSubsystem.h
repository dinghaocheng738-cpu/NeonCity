#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "AircraftWorldSubsystem.generated.h"
class ANeonCityAircraftVehicleBase;
class ANeonCityMainlandSite;
/** Authored identity registry and value snapshots; never disk I/O or arbitrary class spawning. */
UCLASS()
class NEONCITY_API UAircraftWorldSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 void RegisterAircraft(ANeonCityAircraftVehicleBase* Aircraft);
 void UnregisterAircraft(ANeonCityAircraftVehicleBase* Aircraft);
 TArray<FNeonCityAircraftSaveData> BuildPersistenceSnapshot(bool Defaults=false) const;
 void NormalizePersistenceSnapshot(TArray<FNeonCityAircraftSaveData>& Records,TArray<FString>& Warnings) const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityAircraftSaveData>& Records);
 /** Exact retired canonical spawn only; custom saved landing choices are retained. */
 static bool MigrateUntouchedMainlandHelicopterLanding(const ANeonCityMainlandSite* Site,FNeonCityAircraftSaveData& Record);
 static FGuid HelicopterID() { return FGuid(0x18B00001,0x53424541,0x43484845,0x4C493031); }
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
private:
 TMap<FGuid,TWeakObjectPtr<ANeonCityAircraftVehicleBase>> Aircrafts;
 TMap<FGuid,FNeonCityAircraftSaveData> DesignDefaults;
};
