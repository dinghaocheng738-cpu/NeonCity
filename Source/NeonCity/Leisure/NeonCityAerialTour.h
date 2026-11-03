#pragma once
#include "CoreMinimal.h"
#include "Leisure/NeonCityLeisureActivity.h"
#include "NeonCityAerialTour.generated.h"
class UStaticMeshComponent;
class UStaticMesh;
class AStaticMeshActor;

UENUM(BlueprintType)
enum class ENeonCityAerialKind : uint8 { Sightseeing, Airdrop };

/** A sight to photograph (Sightseeing) or a drop zone (Airdrop). */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityAerialStop
{
 GENERATED_BODY()
 /** Sightseeing: the point to frame (world cm). Airdrop: the centre of the target on the ground. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector Location=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Label;
 /** Sightseeing: farthest (cm) a photograph counts from. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaxDistance=45000;
};

/** Helicopter work from the residence helipad, on the leisure base (phone, save, wallet, navigation, HUD):
 *  - Sightseeing: fly the West Lake sights in order and photograph each (keep it in the middle of the view,
 *    within range and in plain sight, while the viewfinder fills; a real screenshot is saved), then land back on
 *    the helipad. Paid per photograph plus a return bonus; the time is recorded.
 *  - Airdrop: three crates hang under the helicopter on a sling; hover low and slow over each island's target, with the load steady,
 *    until the release fires, and the crate falls with physics. Each landing is scored by its distance from the
 *    bullseye (3 m / 8 m / 15 m) and the score sets the pay. */
UCLASS()
class NEONCITY_API ANeonCityAerialTour : public ANeonCityLeisureActivity
{
 GENERATED_BODY()
public:
 ANeonCityAerialTour();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") ENeonCityAerialKind Kind=ENeonCityAerialKind::Sightseeing;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") TArray<FNeonCityAerialStop> Stops;
 /** Where the run ends (the helipad's floor top, world cm); Sightseeing lands here. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") FVector PadLocation=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") int64 RewardPerStop=250;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") int64 FinishBonus=400;
 /** Seconds the subject must stay framed (Sightseeing) or the hover held (Airdrop). */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") float HoldSeconds=1.4f;
 /** Half angle (degrees) of the view centre a subject must sit in. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") float FrameAngle=11;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") TSoftObjectPtr<UStaticMesh> CrateMesh;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Aerial") TSoftObjectPtr<UStaticMesh> TargetMesh;
 int32 GetStopIndex() const { return Progress.ActivityCheckpoint; }
 int32 GetScore() const { return Score; }
 const TArray<FString>& GetPhotos() const { return Photos; }
 /** Airdrop: the crate in flight (null when none). */
 AStaticMeshActor* GetFallingCrate() const { return Falling.Get(); }
 /** How well a crate that came to rest at At scored: 3 bullseye, 2 good, 1 on target, 0 missed. */
 int32 ScoreLanding(int32 Stop,const FVector& At) const;
 /** Is the stop framed from the view point (in range, near the view centre, not hidden)? */
 bool IsFramed(int32 Stop,const FVector& Eye,const FRotator& View,AActor* Ignore) const;
protected:
 virtual void OnAccepted(APlayerController* Player) override;
 virtual void TickActivity(float Delta,APlayerController* Player,APawn* Pawn) override;
 virtual void ClearRun() override;
 virtual FString DescribeRules() const override;
 virtual FText CheckFailure(APlayerController* Player,APawn* Pawn) const override;
private:
 void TickSightseeing(float Delta,APlayerController* Player,APawn* Pawn);
 void TickAirdrop(float Delta,APlayerController* Player,APawn* Pawn);
 void UpdateSling(float Delta,APawn* Heli);
 void Release(APawn* Heli);
 void TakePhoto(int32 Stop,APawn* Heli);
 void SetShotHidden(AActor* Actor,bool bHide);
 void GuideNext();
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Slung;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cable;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Targets;
 TWeakObjectPtr<AStaticMeshActor> Falling;
 TWeakObjectPtr<APawn> HiddenFor;int32 HiddenFrames=0;
 TArray<FString> Photos;
 FVector SlingPos=FVector::ZeroVector,SlingVel=FVector::ZeroVector;
 float Hold=0,FallSeconds=0;int32 Score=0,Loaded=0;bool bSlingInit=false;
};
