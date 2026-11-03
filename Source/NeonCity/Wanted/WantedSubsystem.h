#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Crime/CrimeEvent.h"
#include "WantedSubsystem.generated.h"
class UWantedLevelData;
class AController;
class UPoliceResponseData;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityWantedChanged,int32,Previous,int32,Current);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNeonCityResponseRequest,int32,Level,FVector,LastKnownLocation);
/** Single-player prototype: confirmed evidence, bounded escalation and sight-dependent decay. */
UCLASS()
class NEONCITY_API UWantedSubsystem : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(NeonCityWanted,STATGROUP_Tickables); }
 virtual bool IsTickable() const override;
 UFUNCTION(BlueprintPure,Category="NeonCity|Wanted") int32 GetWantedLevel() const { return Level; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Wanted") int32 GetEvidenceScore() const { return Score; }
 UPoliceResponseData* GetResponseProfile() const { return Response; }
 UFUNCTION(BlueprintCallable) bool ConfigureResponse(UPoliceResponseData* Data);
 void ResetForLoad();
 int32 GetVehicleBudget() const;
 float GetSearchRadius() const;
 const UWantedLevelData* GetProfile() const { return Profile; }
 UPROPERTY(BlueprintAssignable) FNeonCityWantedChanged OnWantedLevelChanged;
 UPROPERTY(BlueprintAssignable) FNeonCityResponseRequest OnRoadblockRequested;
 UPROPERTY(BlueprintAssignable) FNeonCityResponseRequest OnSpecialUnitRequested;
 UPROPERTY(BlueprintAssignable) FNeonCityResponseRequest OnHighIntensityRequested;
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
 UFUNCTION() void CrimeRecorded(const FNeonCityCrimeEvent& Event);
 UFUNCTION() void ReportReceived(const FNeonCityReportEvent& Report);
 void Confirm(const FNeonCityCrimeEvent& Event);
 void SetLevel(int32 NewLevel);
 UPROPERTY() TObjectPtr<UWantedLevelData> Profile;
 UPROPERTY() TObjectPtr<UPoliceResponseData> Response;
 TWeakObjectPtr<AController> Suspect;
 TArray<FGuid> Evaluated;
 int32 Level=0,Score=0;
 float NextDecay=0,LastThreatScoreTime=-100,LastSevereTime=-100,TickSeconds=0;
};
