#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "NeonCityShipLock.generated.h"
class UStaticMeshComponent;
class UTextRenderComponent;
class UAudioComponent;
class ANeonCityMarineVehicleBase;

/** The lock chamber's water: a contained basin whose surface is the lock's current level, not its box top. */
UCLASS(NotPlaceable)
class NEONCITY_API ANeonCityLockWater : public ANeonCityWaterVolume
{
 GENERATED_BODY()
public:
 float Level=0;                                  // world Z (cm)
 virtual bool QueryWaterSurface_Implementation(FVector Point,FWaterSurfaceSample& Result) const override;
};

UENUM(BlueprintType)
enum class ENeonCityLockPhase : uint8 { Ready, ClosingGate, Filling, Draining, OpeningGate };

/** A ship lock between two water levels (the canal at West Lake level and the sea). Local X runs along the lock,
 * +X toward the lower (sea) side. Two mitre gates close the chamber; the chamber's water (ANeonCityLockWater, with
 * its visible surface) rises and falls between the levels, carrying whatever floats in it. Fully automatic:
 * a vessel that has stopped in the chamber is locked through to the side it faces; a vessel waiting at a closed
 * gate has the empty chamber brought to its level and the gate opened. Gate leaves block while closed. */
UCLASS()
class NEONCITY_API ANeonCityShipLock : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityShipLock();
 UPROPERTY(EditAnywhere,Category="Lock") float ChamberLength=8600;
 UPROPERTY(EditAnywhere,Category="Lock") float ChamberWidth=1800;
 /** World water levels (cm): upper = the canal / lake, lower = the sea. */
 UPROPERTY(EditAnywhere,Category="Lock") float UpperLevel=-50.5f;
 UPROPERTY(EditAnywhere,Category="Lock") float LowerLevel=-130.f;
 UPROPERTY(EditAnywhere,Category="Lock") float FloorZ=-520.f;
 UPROPERTY(EditAnywhere,Category="Lock") float GateSeconds=7;
 UPROPERTY(EditAnywhere,Category="Lock") float LevelSeconds=16;
 /** A vessel this far beyond a gate and heading for it calls the chamber to its side (cm). */
 UPROPERTY(EditAnywhere,Category="Lock") float CallRange=20000;
 ENeonCityLockPhase GetPhase() const { return Phase; }
 bool IsUpperGateOpen() const { return Upper>=1.f; }
 bool IsLowerGateOpen() const { return Lower>=1.f; }
 float GetLevel() const { return Level; }
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float Delta) override;
private:
 enum class ESide : uint8 { Upper, Lower };
 void Present();
 void Survey(float Delta,bool& bSettledInChamber,ESide& Wants,bool& bChamberEmpty,bool& bCallUpper,bool& bCallLower);
 void Begin(ESide Target);
 UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Leaves;     // upper A, upper B, lower A, lower B
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Surface;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Sign;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Horn;
 UPROPERTY(Transient) TObjectPtr<ANeonCityLockWater> Water;
 ENeonCityLockPhase Phase=ENeonCityLockPhase::Ready;
 ESide At=ESide::Upper,Goal=ESide::Upper;
 float Upper=1,Lower=0,Level=0,Settled=0;
};
