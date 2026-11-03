#include "Phone/OpportunityPhoneApp.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
void UOpportunityPhoneApp::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);Collection.InitializeDependency<UPhoneSubsystem>();Collection.InitializeDependency<UNeonCityOpportunitySubsystem>();
 TScriptInterface<IPhoneAppInterface> App;App.SetObject(this);App.SetInterface(this);
 GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->RegisterApp(App);
}
void UOpportunityPhoneApp::Deinitialize()
{
 if(auto* Phone=GetGameInstance()->GetSubsystem<UPhoneSubsystem>())Phone->UnregisterApp(ENeonCityPhoneApp::Opportunities,this);
 Super::Deinitialize();
}
bool UOpportunityPhoneApp::OpenPhoneApp_Implementation(APlayerController* Player)
{
 if(!IsValid(Player) || !Player->IsLocalController() || Player->GetGameInstance()!=GetGameInstance())return false;
 auto* Phone=Player->FindComponentByClass<UPhoneComponent>();return Phone && Phone->ShowOpportunities();
}
