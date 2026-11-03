#include "Phone/PhoneSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
bool UPhoneSubsystem::RegisterAction(FName Id, FNeonCityPhoneAction Handler)
{
	if (Id.IsNone() || !Handler.IsBound() || HasAction(Id)) { return false; }
	Actions.Add(Id,Handler); return true;
}
void UPhoneSubsystem::UnregisterAction(FName Id,const UObject* Owner)
{
	auto* Handler=Actions.Find(Id);
	if (Handler && Handler->GetUObject()==Owner) { Actions.Remove(Id); }
}
bool UPhoneSubsystem::HasAction(FName Id) const
{
 if(GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->CanChoose(Id,ENeonCityOpportunityChoice::Accept))return true;
	const auto* Handler=Actions.Find(Id); return Handler && Handler->IsBound();
}
bool UPhoneSubsystem::ExecuteAction(FName Id,APlayerController* Player)
{
 auto* Opportunities=GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>();
 if(Opportunities->CanChoose(Id,ENeonCityOpportunityChoice::Accept))return Opportunities->Choose(Id,ENeonCityOpportunityChoice::Accept,Player);
	auto* Handler=Actions.Find(Id);
	if (!IsValid(Player) || !Player->IsLocalController() || !Handler || !Handler->IsBound()) { return false; }
	// Copy: the owner can unregister itself when the action is accepted.
	FNeonCityPhoneAction Call=*Handler;
	return Call.Execute(Player);
}
bool UPhoneSubsystem::RegisterApp(TScriptInterface<IPhoneAppInterface> App)
{
	UObject* Object=App.GetObject();
	if (!IsValid(Object) || !Object->Implements<UPhoneAppInterface>()) { return false; }
	const auto Id=IPhoneAppInterface::Execute_GetPhoneApp(Object);
	if (Apps.Contains(Id)) { return false; }
	Apps.Add(Id,App); return true;
}
bool UPhoneSubsystem::OpenApp(ENeonCityPhoneApp Id,APlayerController* Player)
{
	const auto* App=Apps.Find(Id);
	return IsValid(Player) && Player->IsLocalController() && App && IsValid(App->GetObject())
		&& IPhoneAppInterface::Execute_OpenPhoneApp(App->GetObject(),Player);
}
void UPhoneSubsystem::UnregisterApp(ENeonCityPhoneApp Id,const UObject* Owner)
{
	const auto* App=Apps.Find(Id);
	if (App && App->GetObject()==Owner) { Apps.Remove(Id); }
}
