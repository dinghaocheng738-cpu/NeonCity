#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Phone/PhoneAppInterface.h"
#include "AutoMarketSubsystem.generated.h"
class UVehicleData;
/** Read-only vehicle catalogue behind the existing Vehicles phone-app slot. */
UCLASS()
class NEONCITY_API UAutoMarketSubsystem : public UGameInstanceSubsystem, public IPhoneAppInterface
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual ENeonCityPhoneApp GetPhoneApp_Implementation() const override { return ENeonCityPhoneApp::Vehicles; }
 virtual bool OpenPhoneApp_Implementation(APlayerController* Player) override;
 UFUNCTION(BlueprintPure,Category="Phone|AutoMarket") TArray<UVehicleData*> GetVehicleProfiles() const;
private:
 UPROPERTY() TArray<TObjectPtr<UVehicleData>> Profiles;
};
