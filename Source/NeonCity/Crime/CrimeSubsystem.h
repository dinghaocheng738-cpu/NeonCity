#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/ThreatEvent.h"
#include "Crime/CrimeEvent.h"
#include "CrimeSubsystem.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityCrimeRecorded,const FNeonCityCrimeEvent&,Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityReportRecorded,const FNeonCityReportEvent&,Event);
/** World-local, bounded event ledger. No enforcement, scores, police spawning or persistence. */
UCLASS()
class NEONCITY_API UCrimeSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 void ResetForLoad();
 UFUNCTION(BlueprintCallable,Category="NeonCity|Crime") bool ReportEvent(FGuid CrimeId,AActor* Witness);
 UFUNCTION(BlueprintPure,Category="NeonCity|Crime") const TArray<FNeonCityCrimeEvent>& GetEvents() const { return Events; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Crime") const TArray<FNeonCityReportEvent>& GetReports() const { return Reports; }
 UPROPERTY(BlueprintAssignable) FNeonCityCrimeRecorded OnCrimeRecorded;
 UPROPERTY(BlueprintAssignable) FNeonCityReportRecorded OnReportEvent;
private:
 UFUNCTION() void RecordThreat(const FNeonCityThreatEvent& Event);
 UPROPERTY() TArray<FNeonCityCrimeEvent> Events;
 UPROPERTY() TArray<FNeonCityReportEvent> Reports;
 TMap<FGuid,TSet<TWeakObjectPtr<AActor>>> ReportedWitnesses;
 static constexpr int32 Capacity=256;
};
