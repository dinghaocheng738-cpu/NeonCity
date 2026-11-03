#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Opportunities/NeonCityOpportunityData.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "NeonCityOpportunitySubsystem.generated.h"
class APlayerController;
class INeonCityOpportunityProvider;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNeonCityOpportunitiesChanged);

/** Bounded discovery/choice router. Providers own gameplay and expose snapshots. */
UCLASS()
class NEONCITY_API UNeonCityOpportunitySubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 bool RegisterProvider(UObject* Provider);
 void UnregisterProvider(UObject* Provider);
 void NotifyChanged(UObject* Provider);
 UFUNCTION(BlueprintCallable) bool Choose(FName ID,ENeonCityOpportunityChoice Choice,APlayerController* Player);
 UFUNCTION(BlueprintPure) bool CanChoose(FName ID,ENeonCityOpportunityChoice Choice) const;
 UFUNCTION(BlueprintPure) TArray<FNeonCityOpportunityData> GetOpportunities() const;
 UPROPERTY(BlueprintAssignable) FNeonCityOpportunitiesChanged OnOpportunitiesChanged;
 TArray<FNeonCityOpportunitySaveData> BuildSnapshot(bool Defaults) const;
 TArray<FNeonCityMessageSaveData> BuildDefaultMessages() const;
 void NormalizeSnapshot(TArray<FNeonCityOpportunitySaveData>& Snapshot,TArray<FString>& Warnings) const;
 void ApplySnapshot(const TArray<FNeonCityOpportunitySaveData>& Snapshot);
 virtual void Deinitialize() override;
private:
 INeonCityOpportunityProvider* FindProvider(FName ID) const;
 TMap<FName,TWeakObjectPtr<UObject>> Providers;
};
