#pragma once
#include "CoreMinimal.h"
#include "Vehicles/VehicleData.h"
#include "SuspectDescriptor.generated.h"
UENUM(BlueprintType)
enum class EPoliceEvidenceSource : uint8 { Witness, Police, Camera };
USTRUCT(BlueprintType)
struct FVehicleDescriptor
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FGuid VehicleID;
 UPROPERTY(BlueprintReadOnly) ENeonCityVehicleType VehicleType=ENeonCityVehicleType::Sedan;
 UPROPERTY(BlueprintReadOnly) FName AppearanceTag;
 UPROPERTY(BlueprintReadOnly) FName LicensePlate;
 bool IsKnown() const { return VehicleID.IsValid(); }
};
/** Observed values only. No actor/controller reference and no saved live tracking target. */
USTRUCT(BlueprintType)
struct FSuspectDescriptor
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FVector LastKnownLocation=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector LastSeenDirection=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float LastSeenTime=-1;
 UPROPERTY(BlueprintReadOnly) bool InVehicle=false;
 UPROPERTY(BlueprintReadOnly) FVehicleDescriptor Vehicle;
 UPROPERTY(BlueprintReadOnly) float Confidence=0;
 UPROPERTY(BlueprintReadOnly) float DescriptionTimestamp=-1;
 UPROPERTY(BlueprintReadOnly) FName ClothingDescription;
 UPROPERTY(BlueprintReadOnly) FName CharacterAppearance;
 UPROPERTY(BlueprintReadOnly) EPoliceEvidenceSource Source=EPoliceEvidenceSource::Witness;
 bool IsValid() const { return FMath::IsFinite(LastSeenTime) && LastSeenTime>=0 && FMath::IsFinite(Confidence) && Confidence>0 && !LastKnownLocation.ContainsNaN() && !LastSeenDirection.ContainsNaN(); }
};
