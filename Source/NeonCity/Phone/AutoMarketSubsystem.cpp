#include "Phone/AutoMarketSubsystem.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Vehicles/VehicleData.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
void UAutoMarketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection); Collection.InitializeDependency<UPhoneSubsystem>();
 for(const TCHAR* Name:{TEXT("DA_StarterSedan"),TEXT("DA_SportsCar"),TEXT("DA_SUV"),TEXT("DA_Pickup")})
 {
  const FString Path=FString::Printf(TEXT("/Game/NeonCity/Vehicles/%s.%s"),Name,Name);
  auto* Data=LoadObject<UVehicleData>(nullptr,*Path);
  if(Data && Data->IsValidProfile()) Profiles.Add(Data);
 }
 TScriptInterface<IPhoneAppInterface> App; App.SetObject(this); App.SetInterface(this);
 GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->RegisterApp(App);
}
void UAutoMarketSubsystem::Deinitialize()
{
 if(auto* Phone=GetGameInstance()->GetSubsystem<UPhoneSubsystem>()) Phone->UnregisterApp(ENeonCityPhoneApp::Vehicles,this);
 Profiles.Reset(); Super::Deinitialize();
}
TArray<UVehicleData*> UAutoMarketSubsystem::GetVehicleProfiles() const
{ TArray<UVehicleData*> Result; for(UVehicleData* Data:Profiles) Result.Add(Data); return Result; }
bool UAutoMarketSubsystem::OpenPhoneApp_Implementation(APlayerController* Player)
{
 if(!IsValid(Player) || !Player->IsLocalController() || Player->GetGameInstance()!=GetGameInstance()) return false;
 auto* Phone=Player->FindComponentByClass<UPhoneComponent>(); return Phone && Phone->ShowVehicleMarket();
}
