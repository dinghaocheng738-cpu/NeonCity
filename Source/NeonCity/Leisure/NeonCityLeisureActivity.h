#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Opportunities/NeonCityOpportunityProvider.h"
#include "NeonCityLeisureActivity.generated.h"
class USoundBase;

/** What the player has to be in for an activity. */
UENUM(BlueprintType)
enum class ENeonCityLeisureVehicle : uint8 { Any, OnFoot, Boat, Aircraft, Car };

/** A rival on an activity's leaderboard (authored local names and times; the player's best is ranked among them). */
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityLeisureRival
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FString Name;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float Seconds=0;
};

/** Shared base of the West Lake leisure activities (night regatta, helicopter tours and drops, tide chase, snow
 * scene): listed in the phone (P -> O) through the existing opportunity domain, saved with it, paid once per run
 * through the existing wallet, guided by the existing navigation, shown on the activity HUD. Subclasses run the
 * activity itself in TickActivity and finish with Complete / Fail. */
UCLASS(Abstract)
class NEONCITY_API ANeonCityLeisureActivity : public AActor,public INeonCityOpportunityProvider
{
 GENERATED_BODY()
public:
 ANeonCityLeisureActivity();
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") FName ActivityID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") FText Title;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") FText Contact;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure",meta=(MultiLine=true)) FText Description;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") ENeonCityLeisureVehicle Vehicle=ENeonCityLeisureVehicle::Any;
 /** Hours the activity can be accepted (wraps past midnight when From > To); From == To: always open. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") float OpenFromHour=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") float OpenToHour=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") int64 RewardPreview=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") float MaximumSeconds=900;
 /** Where the run starts (world, cm): the first navigation goal. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") FVector StartLocation=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") float StartRadius=1500;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") FLinearColor Accent=FLinearColor(1.f,.36f,.18f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Leisure") TArray<FNeonCityLeisureRival> Rivals;

 virtual FNeonCityOpportunityData GetOpportunityData() const override;
 virtual bool ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player) override;
 virtual FNeonCityOpportunitySaveData BuildOpportunitySnapshot(bool Defaults) const override;
 virtual bool IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) const override;
 virtual void ApplyOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) override;
 virtual FNeonCityMessageSaveData BuildDefaultOpportunityMessage() const override;

 bool IsRunning() const { return Progress.State==1; }
 uint8 GetStage() const { return Progress.ActivityStage; }
 float GetRunSeconds() const { return Progress.ActivitySeconds; }
 float GetBestSeconds() const { return Progress.ActivityBestSeconds; }
 int32 GetCompletions() const { return Progress.ActivityCompletions; }
 FName GetReceiptID() const;
 bool IsOpenNow() const;
 /** 1-based place of Seconds among the rivals (lower is better). */
 int32 RankOf(float Seconds) const;
 static ENeonCityLeisureVehicle Classify(const APawn* Pawn);
 static FString FormatTime(float Seconds);
 static float GetHour(const UWorld* World);
 /** Accept and start straight away for this player (tests and debug). */
 bool StartFor(APlayerController* Player) { return ChooseOpportunity(ENeonCityOpportunityChoice::Accept,Player); }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float Delta) override;
 /** Runs every tick while accepted, with the player's current pawn. */
 virtual void TickActivity(float Delta,APlayerController* Player,APawn* Pawn) {}
 virtual void OnAccepted(APlayerController* Player) {}
 /** Remove anything the run spawned (gates, cargo, lights). */
 virtual void ClearRun() {}
 /** Extra phone text (rules, medal times). */
 virtual FString DescribeRules() const { return Description.ToString(); }
 /** Fail reason checked every tick; empty while the run is fine. */
 virtual FText CheckFailure(APlayerController* Player,APawn* Pawn) const;
 virtual bool CanAcceptNow(FText& Reason) const;
 /** Pays Reward once for this run, records the time and completion, shows the results banner. */
 void Complete(int64 Reward,float Seconds,const FText& Headline,const FText& Detail);
 void Fail(const FText& Reason);
 void SetStage(uint8 Stage);
 void Notify(const FText& Text,float Seconds=6) const;
 void Guide(const FVector& Location,const FText& Label,bool Road=false);
 void ShowHUD(const struct FNeonCityActivityHUDState& State) const;
 void PlayCue(const TCHAR* Name,float Volume=1.f) const;
 TWeakObjectPtr<APlayerController> Customer;
 UPROPERTY(Transient) FNeonCityOpportunitySaveData Progress;
 float VehicleMissingSeconds=0;
private:
 void EndRun();
};
