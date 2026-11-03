#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Phone/PhoneAppInterface.h"
#include "BankPhoneApp.generated.h"
UCLASS()
class NEONCITY_API UBankPhoneApp : public UGameInstanceSubsystem,public IPhoneAppInterface
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual ENeonCityPhoneApp GetPhoneApp_Implementation() const override { return ENeonCityPhoneApp::Bank; }
 virtual bool OpenPhoneApp_Implementation(APlayerController* Player) override;
};
