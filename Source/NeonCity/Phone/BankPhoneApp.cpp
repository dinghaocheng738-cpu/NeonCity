#include "Phone/BankPhoneApp.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
void UBankPhoneApp::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);Collection.InitializeDependency<UPhoneSubsystem>();Collection.InitializeDependency<UNeonCityEconomySubsystem>();
 TScriptInterface<IPhoneAppInterface> App;App.SetObject(this);App.SetInterface(this);
 GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->RegisterApp(App);
}
void UBankPhoneApp::Deinitialize()
{
 if(auto* Phone=GetGameInstance()->GetSubsystem<UPhoneSubsystem>())Phone->UnregisterApp(ENeonCityPhoneApp::Bank,this);
 Super::Deinitialize();
}
bool UBankPhoneApp::OpenPhoneApp_Implementation(APlayerController* Player)
{
 if(!IsValid(Player) || !Player->IsLocalController() || Player->GetGameInstance()!=GetGameInstance())return false;
 auto* Phone=Player->FindComponentByClass<UPhoneComponent>();return Phone && Phone->ShowBank();
}
