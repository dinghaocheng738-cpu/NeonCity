#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "Opportunities/NeonCityOpportunityProvider.h"
#include "NeonCityTransportTest.generated.h"
class ANeonCityBaseVehicle;
class APlayerController;
class USceneComponent;
UENUM(BlueprintType)
enum class ENeonCityTransportState : uint8 { Offered=0, Accepted=1, Completed=2, Declined=3, Ignored=4 };

/** Authored Marcus delivery; discovery and player choices use the shared provider. */
UCLASS()
class NEONCITY_API ANeonCityTransportTest : public AActor, public INeonCityOpportunityProvider
{
	GENERATED_BODY()
public:
 FNeonCityOpportunitySaveData BuildPersistenceSnapshot(bool Defaults=false) const;
 FNeonCityMessageSaveData BuildDefaultMessage() const;
 void ApplyPersistenceSnapshot(const FNeonCityOpportunitySaveData& Snapshot);
 virtual FNeonCityOpportunityData GetOpportunityData() const override;
 virtual bool ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player) override;
 virtual FNeonCityOpportunitySaveData BuildOpportunitySnapshot(bool Defaults) const override { return BuildPersistenceSnapshot(Defaults); }
 virtual bool IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) const override;
 virtual void ApplyOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) override { ApplyPersistenceSnapshot(Data); }
 virtual FNeonCityMessageSaveData BuildDefaultOpportunityMessage() const override { return BuildDefaultMessage(); }

	ANeonCityTransportTest();
	UPROPERTY(EditInstanceOnly, Category="Delivery") TObjectPtr<ANeonCityBaseVehicle> DeliveryVehicle;
	UPROPERTY(EditAnywhere, Category="Delivery", meta=(ClampMin="1",ClampMax="10000")) int64 Reward=350;
	UFUNCTION(BlueprintPure, Category="Delivery") ENeonCityTransportState GetState() const { return State; }
	static FName ReceiptKey() { return TEXT("SouthBeach.Marcus.Transport.001"); }
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	bool Accept(APlayerController* Player);
	void EvaluateDelivery(float DeltaSeconds);
	void UpdateNavigation();
	void Send(const FText& Text,FName Action=NAME_None);
	UPROPERTY() TObjectPtr<USceneComponent> Marker;
	TWeakObjectPtr<APlayerController> Customer;
	ENeonCityTransportState State=ENeonCityTransportState::Offered;
	float StoppedSeconds=0;
	bool UnavailableReported=false;
};
