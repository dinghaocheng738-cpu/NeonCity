#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Opportunities/NeonCityOpportunityData.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "NeonCityOpportunityProvider.generated.h"
class APlayerController;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class NEONCITY_API UNeonCityOpportunityProvider : public UInterface { GENERATED_BODY() };
/** Gameplay owners retain progress, eligibility and rewards. No UI or disk API. */
class NEONCITY_API INeonCityOpportunityProvider
{
 GENERATED_BODY()
public:
 virtual FNeonCityOpportunityData GetOpportunityData() const=0;
 virtual bool ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player)=0;
 virtual FNeonCityOpportunitySaveData BuildOpportunitySnapshot(bool Defaults) const=0;
 virtual bool IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) const=0;
 virtual void ApplyOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data)=0;
 virtual FNeonCityMessageSaveData BuildDefaultOpportunityMessage() const { return {}; }
};
