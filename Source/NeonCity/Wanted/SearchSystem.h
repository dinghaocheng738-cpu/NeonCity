#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Crime/CrimeEvent.h"
#include "SearchSystem.generated.h"
class UPoliceSensorComponent;
class AController;
class UPoliceResponseData;
/** Evidence memory. Navigation receives snapshots only; failed vision never updates a location. */
UCLASS()
class NEONCITY_API USearchSystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 void RegisterSensor(UPoliceSensorComponent* Sensor);
 void UnregisterSensor(UPoliceSensorComponent* Sensor);
 bool WitnessCrime(const FNeonCityCrimeEvent& Event);
 void RecordReport(AController* Suspect,FVector Location,float EvidenceTime);
 void RecordObservation(const FSuspectDescriptor& Evidence,bool Visual=false);
 void NotifyVehiclePossessed(AActor* Vehicle);
 bool ObserveVehicleTransition(AActor* Person,AActor* Vehicle);
 bool ObserveVehicleExit(AActor* Person,AActor* Vehicle);
 const FSuspectDescriptor& GetDescriptor() const { return Description; }
 float GetConfidence() const;
 float GetDynamicSearchRadius(float Base) const;
 FVector GetLastKnownVehicleLocation() const { return VehicleLocation; }
 void UpdateSight(AController* Suspect);
 void RecordUnidentified(FVector Location,float EvidenceTime);
 void Clear();
 UFUNCTION(BlueprintPure,Category="NeonCity|Search") bool HasLocation() const { return bHasLocation; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Search") FVector GetLastKnownLocation() const { return LastKnownLocation; }
 UFUNCTION(BlueprintPure,Category="NeonCity|Search") bool HasVisualContact() const { return bVisualContact; }
 float GetEvidenceTime() const { return LastEvidenceTime; }
 bool HasUnidentifiedIncident() const;
 FVector GetSearchPoint(int32 Index,float Radius) const;
 int32 GetSensorCount() const { return Sensors.Num(); }
private:
 bool CanSee(UPoliceSensorComponent* Sensor,AActor* Target,FVector Point) const;
 bool Observe(UPoliceSensorComponent* Sensor,AActor* Candidate,FSuspectDescriptor& Evidence) const;
 void Remember(FVector Location,float Time);
 TArray<TWeakObjectPtr<UPoliceSensorComponent>> Sensors;
 struct FRecognition { TWeakObjectPtr<UPoliceSensorComponent> Sensor; TWeakObjectPtr<AActor> Candidate; float Since=0,Last=0; };
 TArray<FRecognition> Recognitions;
 UPROPERTY() TObjectPtr<UPoliceResponseData> Response;
 FSuspectDescriptor Description;
 FVector VehicleLocation=FVector::ZeroVector;
 FVector LastKnownLocation=FVector::ZeroVector;
 float LastEvidenceTime=-1,UnidentifiedUntil=-1,LastVehicleMotionTime=-1;
 bool bHasLocation=false,bVisualContact=false;
};
