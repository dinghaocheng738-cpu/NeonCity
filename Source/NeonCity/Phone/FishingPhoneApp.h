#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Phone/PhoneAppInterface.h"
#include "Phone/PhonePageInterface.h"
#include "FishingPhoneApp.generated.h"
UCLASS()
class NEONCITY_API UFishingPhoneApp : public UGameInstanceSubsystem,public IPhoneAppInterface,public IPhonePageInterface
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual ENeonCityPhoneApp GetPhoneApp_Implementation() const override { return ENeonCityPhoneApp::Fishing; }
 virtual bool OpenPhoneApp_Implementation(APlayerController* Player) override;
 virtual FText BuildPage(APlayerController* Player) override;
 virtual void PageInput(APlayerController* Player,ENeonCityPhonePageInput Input) override;
private:
 bool ValidPlayer(APlayerController* Player) const;
 int32 Tab=0,Selected=0;
 FString Notice;
};
