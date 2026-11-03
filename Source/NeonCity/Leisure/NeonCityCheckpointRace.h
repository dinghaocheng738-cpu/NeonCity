#pragma once
#include "CoreMinimal.h"
#include "Leisure/NeonCityLeisureActivity.h"
#include "NeonCityCheckpointRace.generated.h"
class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;

/** One gate: the line between two markers, passed in the direction of Yaw. */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityRaceGate
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector Location=FVector::ZeroVector;     // world cm, on the water / ground
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Yaw=0;                              // direction of travel through it
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float HalfWidth=900;                      // cm from the centre to each marker
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float HalfHeight=1500;                    // cm above / below the centre that counts
 /** A gate inside a bridge arch: no markers (the arch is the gate), only its guide light. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bBridgeArch=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Label;
};

/** Timed course through ordered gates, raced in a required vehicle: the West Lake night regatta. Accepting marks
 * the start; stopping there in the right vehicle counts down 3-2-1 and starts the clock. Gates are red silk
 * lanterns on floating buoys (SM_WL_LanternBuoy); the next gate's lanterns glow and light the water, the one after
 * is lit dimly, passed gates go dark. Split times against your best at every gate; gold / silver / bronze times
 * and a local leaderboard; a better medal than before pays its full prize, a repeat pays a fifth. */
UCLASS()
class NEONCITY_API ANeonCityCheckpointRace : public ANeonCityLeisureActivity
{
 GENERATED_BODY()
public:
 ANeonCityCheckpointRace();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") TArray<FNeonCityRaceGate> Gates;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") float StartYaw=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") float GoldSeconds=120;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") float SilverSeconds=150;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") float BronzeSeconds=190;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") int64 GoldReward=1200;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") int64 SilverReward=700;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") int64 BronzeReward=400;
 /** Below this speed (km/h) at the start the countdown begins. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") float StartSpeedKmh=12;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") TSoftObjectPtr<UStaticMesh> MarkerMesh;
 /** Marker pivot height above the gate location (cm); the buoy's waterline. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Race") float MarkerZ=0;
 int32 GetNextGate() const { return Progress.ActivityCheckpoint; }
 int32 GetMarkerCount() const { return Markers.Num(); }
 /** 0 none, 1 bronze .. 3 gold. */
 int32 MedalFor(float Seconds) const;
 /** Did the move From -> To cross gate Index forwards between its markers? */
 bool Crosses(int32 Index,const FVector& From,const FVector& To) const;
protected:
 virtual void OnAccepted(APlayerController* Player) override;
 virtual void TickActivity(float Delta,APlayerController* Player,APawn* Pawn) override;
 virtual void ClearRun() override;
 virtual FString DescribeRules() const override;
 virtual FText CheckFailure(APlayerController* Player,APawn* Pawn) const override;
private:
 void SpawnMarkers();
 void LightGates();
 void ShowRaceHUD(float Speed);
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Markers;
 UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> GateLights;
 TArray<float> Splits,BestSplits;
 FVector LastLocation=FVector::ZeroVector;bool bHasLast=false;
 float Countdown=0;int32 LastBeep=0;float SplitShow=0;float SplitDelta=0;
};
