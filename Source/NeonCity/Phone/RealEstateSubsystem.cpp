#include "Phone/RealEstateSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Phone/PhoneSubsystem.h"
#include "Property/PropertySubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
void URealEstateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection); Collection.InitializeDependency<UPropertySubsystem>(); Collection.InitializeDependency<UPhoneSubsystem>();
 TScriptInterface<IPhoneAppInterface> App; App.SetObject(this); App.SetInterface(this);
 GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->RegisterApp(App);
}
void URealEstateSubsystem::Deinitialize()
{
 if(auto* Phone=GetGameInstance()->GetSubsystem<UPhoneSubsystem>()) Phone->UnregisterApp(ENeonCityPhoneApp::Properties,this);
 Super::Deinitialize();
}
bool URealEstateSubsystem::OpenPhoneApp_Implementation(APlayerController* Player)
{
 if(!IsValid(Player) || !Player->IsLocalController() || Player->GetGameInstance()!=GetGameInstance()) return false;
 auto* Phone=Player->FindComponentByClass<UPhoneComponent>(); return Phone && Phone->ShowRealEstate();
}
