#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Phone/PhoneAppInterface.h"
#include "RealEstateSubsystem.generated.h"
/** Read-only property catalogue routed through the existing phone app interface. */
UCLASS()
class NEONCITY_API URealEstateSubsystem : public UGameInstanceSubsystem, public IPhoneAppInterface
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual ENeonCityPhoneApp GetPhoneApp_Implementation() const override { return ENeonCityPhoneApp::Properties; }
 virtual bool OpenPhoneApp_Implementation(APlayerController* Player) override;
};
