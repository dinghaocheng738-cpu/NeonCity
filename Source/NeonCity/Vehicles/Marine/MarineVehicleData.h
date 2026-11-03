#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Player/Camera/MarineCameraSettings.h"
#include "MarineVehicleData.generated.h"

/** SI tuning except hull dimensions (cm) and turning rate (degrees/second). */
UCLASS(BlueprintType)
class NEONCITY_API UMarineVehicleData : public UPrimaryDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FName VehicleType=TEXT("NeonCity.Speedboat");
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FText DisplayName=NSLOCTEXT("NeonCity","Speedboat","NeonCity Speedboat");
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float MaxSpeed=65; // km/h
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Acceleration=3.5f; // m/s^2
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float TurningRate=32;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Mass=1100;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Buoyancy=1.8f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float WaterDrag=.22f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float LateralDrag=2.8f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float TurningResistance=3;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float VerticalDamping=4;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float EnginePower=100; // kW
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float ThrottleRiseRate=1.4f; // Full-scale units/second.
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float ThrottleFallRate=3.f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float BrakeDrag=1.8f;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float Durability=850;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FVector HullExtent=FVector(250,100,40);
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FVector CenterOfMass=FVector(0,0,-20);
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FMarineCameraSettings CameraRig;
 // Opt-in displacement hull. The speedboat retains its existing force model.
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) bool bReferenceDisplacementHull=false;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) bool bWalkableDeck=false;
 /** Walkable decks: how far from the helm eye a person on deck can take the helm with F (a big cockpit: anywhere in it). */
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,meta=(Units="cm",ClampMin="100")) float HelmReachCm=175.f;
 /** Direct helm boarding may still leave onto an exact authored deck floor. */
 //Compact decks exit safely ashore alongside their authored berth, otherwise onto deck.
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) bool bPreferDeckExit=false;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FVector DeckExit=FVector(-80,0,35);
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) FVector BoardingPoint=FVector(-175,0,35);
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,meta=(ClampMin="300",ClampMax="1000")) float InteractionReachCm=420;
 /** Lake / river craft: never leases ocean wave samples; floats on its contained basin. */
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) bool bInlandWater=false;
 /** Planing / powered hulls throw white foam ribbons astern; a pulling boat at walking pace does not. */
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) bool bFoamWake=true;
 bool IsValidProfile() const;
 virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("MarineVehicle"),VehicleType); }
};

UCLASS()
class NEONCITY_API UReferenceFishingBoatData : public UMarineVehicleData
{
 GENERATED_BODY()
public:
 UReferenceFishingBoatData();
};

/** Capacity contract only. No yacht, rooms, garages or passenger gameplay is spawned. */
UCLASS(BlueprintType)
class NEONCITY_API UYachtVehicleData : public UMarineVehicleData
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TArray<FName> DeckIDs;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TArray<FName> CabinIDs;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) int32 GarageCapacity=0;
};
