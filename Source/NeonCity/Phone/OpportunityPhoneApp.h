#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Phone/PhoneAppInterface.h"
#include "OpportunityPhoneApp.generated.h"

/** UI adapter kept outside the gameplay opportunity router. */
UCLASS()
class NEONCITY_API UOpportunityPhoneApp : public UGameInstanceSubsystem, public IPhoneAppInterface
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual ENeonCityPhoneApp GetPhoneApp_Implementation() const override { return ENeonCityPhoneApp::Opportunities; }
 virtual bool OpenPhoneApp_Implementation(APlayerController* Player) override;
};
