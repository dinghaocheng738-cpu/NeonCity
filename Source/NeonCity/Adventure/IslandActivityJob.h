#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "Opportunities/NeonCityOpportunityProvider.h"
#include "IslandActivityJob.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ANeonCityNPCCharacter;
class UTransportOccupantComponent;
class AIslandActivityJob;

UENUM(BlueprintType)
enum class ENeonCityIslandActivity : uint8 { Delivery, Photography, Taxi, FishingOrder, DiveSalvage, GuidedTour, SeaRescue, WingsuitChallenge, TimedHike, HiddenLandmark };

/** Physical pickup, return or trail-stop point; F uses normal interaction traces. */
UCLASS()
class NEONCITY_API AIslandActivityPoint : public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandActivityPoint();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Sign;
 UPROPERTY() TObjectPtr<AIslandActivityJob> Job;
 int32 Index=0;
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
};

/** Bounded authored content on the existing opportunities, wallet, navigation,
 * NPC, fish inventory and phone save domains. No parallel wallet or disk file. */
UCLASS()
class NEONCITY_API AIslandActivityJob : public AActor,public INeonCityOpportunityProvider
{
 GENERATED_BODY()
public:
 AIslandActivityJob();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName ActivityID;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityIslandActivity Kind=ENeonCityIslandActivity::Delivery;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Title;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Contact;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector PickupLocal=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector ReturnLocal=FVector(2000,0,0);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FVector> RoutePointsLocal;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<AActor> PhotoTarget;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector PhotoTargetLocal=FVector(4000,0,1000);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName RequiredSpecies=TEXT("Fish.Sardine");
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D RequiredWeightKg=FVector2D(.05,.40);
 UPROPERTY(EditAnywhere,BlueprintReadOnly) int64 Reward=120;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MaximumSeconds=900;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> PickupMesh;
 virtual FNeonCityOpportunityData GetOpportunityData() const override;
 virtual bool ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player) override;
 virtual FNeonCityOpportunitySaveData BuildOpportunitySnapshot(bool Defaults) const override;
 virtual bool IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) const override;
 virtual void ApplyOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) override;
 bool CanInteractAt(int32 Index,AActor* Visitor) const;
 bool InteractAt(int32 Index,AActor* Visitor);
 FText GetPointInstruction(int32 Index) const;
 uint8 GetActivityStage() const {return Progress.ActivityStage;}
 FGuid GetRunID() const {return Progress.ActivityRunID;}
 FName GetReceiptID() const;
 ANeonCityNPCCharacter* GetActivityNPC() const {return Guest;}
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TArray<TObjectPtr<AIslandActivityPoint>> Points;
 UPROPERTY(Transient) TObjectPtr<ANeonCityNPCCharacter> Guest;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cargo;
 UPROPERTY(Transient) TObjectPtr<UTransportOccupantComponent> Passenger;
 UPROPERTY(Transient) FNeonCityOpportunitySaveData Progress;
 TWeakObjectPtr<APlayerController> Customer;
 TWeakObjectPtr<APawn> Transport;
 FVector LastPlayerLocation=FVector::ZeroVector,TransportPickup=FVector::ZeroVector;
 float WaitSeconds=0,StopSeconds=0,NoticeClock=0;
 bool bGuestBoarded=false,bGuestExiting=false;
 FVector PointLocation(int32 Index) const;
 bool PerformAction(APlayerController* Player);
 bool CapturePhotograph(APlayerController* Player);
 void PhotographCaptured(int32 Width,int32 Height,const TArray<FColor>& Pixels);
 FString PendingPhotoFile;
 FDelegateHandle PhotoCaptureHandle;
 TWeakObjectPtr<APlayerController> PhotoViewOwner;
 TWeakObjectPtr<APawn> PhotoSubject;
 bool bPhotoSubjectHiddenByUs=false;
 float PhotoRequestedAt=0;
 void RestorePhotoView();
 bool Complete(bool FishAlreadyPaid=false);
 void Cancel(const FText& Reason);
 void Notify(const FText& Text,float Seconds=6) const;
 void UpdateNavigation();
 void CreateGuest(FVector Location,bool Swimming=false);
 bool MoveGuest(FVector Goal,float Delta);
 void KeepRescueGuestAfloat(float Delta);
 bool BoardGuest(APawn* Vehicle,bool Boat);
 bool ExitGuest();
 void ClearPresentation();
 APawn* FindStoppedTransport(FVector At,bool Boat) const;
 void SetProgressStage(uint8 Stage);
};
