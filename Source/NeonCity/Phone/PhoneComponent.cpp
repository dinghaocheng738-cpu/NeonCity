#include "Phone/PhoneComponent.h"
#include "Phone/PhoneWidget.h"
#include "Phone/PhonePageInterface.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Phone/AutoMarketSubsystem.h"
#include "Property/PropertySubsystem.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UPhoneComponent::UPhoneComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UPhoneComponent::BeginPlay()
{
	Super::BeginPlay();
	auto* PC=Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController()) { return; }
	auto* GI=GetWorld()->GetGameInstance();
	GI->GetSubsystem<UMessageSystem>()->OnInboxChanged.AddDynamic(this,&UPhoneComponent::InboxChanged);
 GI->GetSubsystem<UNeonCityFishingSubsystem>()->OnInventoryChanged.AddDynamic(this,&UPhoneComponent::PageChanged);
	GI->GetSubsystem<UNeonCityEconomySubsystem>()->OnAccountsChanged.AddDynamic(this,&UPhoneComponent::AccountsChanged);
	GI->GetSubsystem<UPropertySubsystem>()->OnPropertyChanged.AddDynamic(this,&UPhoneComponent::PropertyChanged);
	GI->GetSubsystem<UNeonCityOpportunitySubsystem>()->OnOpportunitiesChanged.AddDynamic(this,&UPhoneComponent::OpportunitiesChanged);
	TScriptInterface<IPhoneAppInterface> App; App.SetObject(this); App.SetInterface(this);
	GI->GetSubsystem<UPhoneSubsystem>()->RegisterApp(App);
}
void UPhoneComponent::SetupInput(UInputComponent* Input)
{
	RemoveInput();
	auto* PC=Cast<APlayerController>(GetOwner());
	auto* Enhanced=Cast<UEnhancedInputComponent>(Input);
	auto* Subsystem=PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Enhanced || !Subsystem) { return; }
	ToggleAction=NewObject<UInputAction>(this); ToggleAction->ValueType=EInputActionValueType::Boolean;
	AcceptAction=NewObject<UInputAction>(this); AcceptAction->ValueType=EInputActionValueType::Boolean;
	ToggleAction->bConsumeInput=true; AcceptAction->bConsumeInput=true;
	Mapping=NewObject<UInputMappingContext>(this);
	AppMapping=NewObject<UInputMappingContext>(this);
	Mapping->MapKey(ToggleAction,EKeys::P); AppMapping->MapKey(AcceptAction,EKeys::Enter);
	Subsystem->AddMappingContext(Mapping,20);
	InputSubsystem=Subsystem; BoundInput=Enhanced;
 MarketAction=NewObject<UInputAction>(this); NextAction=NewObject<UInputAction>(this); PreviousAction=NewObject<UInputAction>(this);
 for(auto* Action:{MarketAction.Get(),NextAction.Get(),PreviousAction.Get()}) { Action->ValueType=EInputActionValueType::Boolean; Action->bConsumeInput=true; }
 AppMapping->MapKey(MarketAction,EKeys::V); AppMapping->MapKey(NextAction,EKeys::Right); AppMapping->MapKey(PreviousAction,EKeys::Left);
 PropertyAction=NewObject<UInputAction>(this); PropertyAction->ValueType=EInputActionValueType::Boolean; PropertyAction->bConsumeInput=true;
 AppMapping->MapKey(PropertyAction,EKeys::H);
 OpportunityAction=NewObject<UInputAction>(this);DeclineAction=NewObject<UInputAction>(this);IgnoreAction=NewObject<UInputAction>(this);
 for(auto* Action:{OpportunityAction.Get(),DeclineAction.Get(),IgnoreAction.Get()}) { Action->ValueType=EInputActionValueType::Boolean;Action->bConsumeInput=true; }
 AppMapping->MapKey(OpportunityAction,EKeys::O);AppMapping->MapKey(DeclineAction,EKeys::Delete);AppMapping->MapKey(IgnoreAction,EKeys::BackSpace);
 Bindings.Add(Enhanced->BindAction(OpportunityAction,ETriggerEvent::Started,this,&UPhoneComponent::SwitchOpportunityApp).GetHandle());
 Bindings.Add(Enhanced->BindAction(DeclineAction,ETriggerEvent::Started,this,&UPhoneComponent::DeclineInput).GetHandle());
 Bindings.Add(Enhanced->BindAction(IgnoreAction,ETriggerEvent::Started,this,&UPhoneComponent::IgnoreInput).GetHandle());
 ScrollUpAction=NewObject<UInputAction>(this);ScrollDownAction=NewObject<UInputAction>(this);
 for(auto* Action:{ScrollUpAction.Get(),ScrollDownAction.Get()}) { Action->ValueType=EInputActionValueType::Boolean;Action->bConsumeInput=true; }
 AppMapping->MapKey(ScrollUpAction,EKeys::PageUp);AppMapping->MapKey(ScrollDownAction,EKeys::PageDown);
 Bindings.Add(Enhanced->BindAction(ScrollUpAction,ETriggerEvent::Started,this,&UPhoneComponent::ScrollUp).GetHandle());
 Bindings.Add(Enhanced->BindAction(ScrollDownAction,ETriggerEvent::Started,this,&UPhoneComponent::ScrollDown).GetHandle());
 // Open-screen focus is expressed through Enhanced Input, independently of
 // weapon owners. Driving/movement and P remain available; app/combat keys
 // cannot reach lower-priority character or vehicle actions while reading.
 SuppressCombatAction=NewObject<UInputAction>(this);
 SuppressCombatAction->ValueType=EInputActionValueType::Boolean;
 SuppressCombatAction->bConsumeInput=true;
 for(const FKey Key:{EKeys::LeftMouseButton,EKeys::RightMouseButton,EKeys::R,EKeys::Tab})AppMapping->MapKey(SuppressCombatAction,Key);
 Bindings.Add(Enhanced->BindAction(SuppressCombatAction,ETriggerEvent::Started,this,&UPhoneComponent::ConsumeAppInput).GetHandle());
 SetupBankInput(Enhanced);
 FishingAction=NewObject<UInputAction>(this);FishingAction->ValueType=EInputActionValueType::Boolean;FishingAction->bConsumeInput=true;
 AppMapping->MapKey(FishingAction,EKeys::N);Bindings.Add(Enhanced->BindAction(FishingAction,ETriggerEvent::Started,this,&UPhoneComponent::SwitchFishingApp).GetHandle());
 if(bOpen)Subsystem->AddMappingContext(AppMapping,200);
 Bindings.Add(Enhanced->BindAction(PropertyAction,ETriggerEvent::Started,this,&UPhoneComponent::SwitchPropertyApp).GetHandle());
 Subsystem->RequestRebuildControlMappings();
 Bindings.Add(Enhanced->BindAction(MarketAction,ETriggerEvent::Started,this,&UPhoneComponent::SwitchApp).GetHandle());
 Bindings.Add(Enhanced->BindAction(NextAction,ETriggerEvent::Started,this,&UPhoneComponent::NextVehicle).GetHandle());
 Bindings.Add(Enhanced->BindAction(PreviousAction,ETriggerEvent::Started,this,&UPhoneComponent::PreviousVehicle).GetHandle());
	Bindings.Add(Enhanced->BindAction(ToggleAction,ETriggerEvent::Started,this,&UPhoneComponent::TogglePhone).GetHandle());
	Bindings.Add(Enhanced->BindAction(AcceptAction,ETriggerEvent::Started,this,&UPhoneComponent::AcceptInput).GetHandle());
}
void UPhoneComponent::RemoveInput()
{
	if (BoundInput.IsValid()) { for (uint32 Handle:Bindings) { BoundInput->RemoveBindingByHandle(Handle); } }
	if (InputSubsystem.IsValid() && Mapping) { InputSubsystem->RemoveMappingContext(Mapping); }
	if (InputSubsystem.IsValid() && AppMapping) { InputSubsystem->RemoveMappingContext(AppMapping); }
	Bindings.Reset(); BoundInput.Reset(); InputSubsystem.Reset();
}
bool UPhoneComponent::OpenPhoneApp_Implementation(APlayerController* Player)
{
	if (Player!=GetOwner()) { return false; }
	ActiveApp=ENeonCityPhoneApp::Messages;
	if (!bOpen) { TogglePhone(); }
 Refresh(); return bOpen;
}
void UPhoneComponent::TogglePhone()
{
	auto* PC=Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController()) { return; }
	if (bOpen)
	{
		if (Widget) { Widget->RemoveFromParent(); }
		bOpen=false;bConfirmLoan=false;
  if(InputSubsystem.IsValid() && AppMapping)InputSubsystem->RemoveMappingContext(AppMapping);
  return;
	}
 if(auto* Fishing=PC->FindComponentByClass<UFishingComponent>();Fishing && Fishing->IsActive())Fishing->Stop();
	if (!Widget) { Widget=CreateWidget<UPhoneWidget>(PC,UPhoneWidget::StaticClass()); }
	if (!Widget) { return; }
	Widget->AddToViewport(20);
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	bOpen=true;
 if(InputSubsystem.IsValid() && AppMapping)InputSubsystem->AddMappingContext(AppMapping,200);
 Refresh();
}
void UPhoneComponent::Refresh()
{
	if (!bOpen || !Widget) { return; }
	auto* GI=GetWorld()->GetGameInstance();
 if(auto* Page=Cast<IPhonePageInterface>(GI->GetSubsystem<UPhoneSubsystem>()->GetAppObject(ActiveApp)))
 { DisplayedText=Page->BuildPage(Cast<APlayerController>(GetOwner()));Widget->SetPhoneText(DisplayedText);return; }
 if(ActiveApp==ENeonCityPhoneApp::Bank) { RefreshBank();return; }
	if(ActiveApp==ENeonCityPhoneApp::Opportunities)
 {
  auto* Router=GI->GetSubsystem<UNeonCityOpportunitySubsystem>();const auto Offers=Router->GetOpportunities();
  FString Text=FString::Printf(TEXT("NEONCITY / OPPORTUNITIES\nCash  $%lld\n\n"),GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance());
  if(!Offers.IsEmpty())
  {
   int32 Index=Offers.IndexOfByPredicate([&](const auto& Offer){return Offer.OpportunityID==SelectedOpportunity;});
   if(Index==INDEX_NONE)Index=0;const auto& Offer=Offers[Index];SelectedOpportunity=Offer.OpportunityID;
   const TCHAR* Sources[]={TEXT("NPC message"),TEXT("Phone"),TEXT("City event"),TEXT("Location"),TEXT("Time"),TEXT("Relationship")};
   const TCHAR* States[]={TEXT("Available"),TEXT("In progress"),TEXT("Completed"),TEXT("Declined"),TEXT("Saved for later")};
   const bool AwaitingChoice=Offer.State==ENeonCityOpportunityState::Offered || Offer.State==ENeonCityOpportunityState::Ignored;
   const TCHAR* Status=AwaitingChoice && !Offer.AcceptanceAvailable?TEXT("Unavailable"):States[FMath::Clamp(int32(Offer.State),0,4)];
   Text+=FString::Printf(TEXT("%d / %d  |  %s\n%s\n%s\n\n%s\n\nReward  $%lld\n%s\n\n"),Index+1,Offers.Num(),Sources[FMath::Clamp(int32(Offer.Source),0,5)],*Offer.Contact.ToString(),*Offer.Title.ToString(),*Offer.Description.ToString(),Offer.RewardPreview,Status);
   if(AwaitingChoice && !Offer.AcceptanceAvailable && !Offer.UnavailableReason.IsEmpty())Text+=Offer.UnavailableReason.ToString()+TEXT("\n\n");
   if(Router->CanChoose(Offer.OpportunityID,ENeonCityOpportunityChoice::Accept))Text+=Offer.Repeatable && Offer.State==ENeonCityOpportunityState::Completed?TEXT("ENTER  Play again\n"):TEXT("ENTER  Accept    DEL  Decline\n");
   if(Router->CanChoose(Offer.OpportunityID,ENeonCityOpportunityChoice::Action))Text+=TEXT("ENTER  ")+Offer.ActiveActionLabel.ToString()+TEXT("\n");
   if(Router->CanChoose(Offer.OpportunityID,ENeonCityOpportunityChoice::Cancel))Text+=TEXT("DEL  Cancel activity safely\n");
   if(Router->CanChoose(Offer.OpportunityID,ENeonCityOpportunityChoice::Ignore))Text+=TEXT("BACKSPACE  Keep for later\n");
   if(Offer.State==ENeonCityOpportunityState::Accepted)Text+=TEXT("Close phone to follow the map.\n");
  }
  else Text+=TEXT("No opportunities nearby.\nExplore the city at your own pace.\n\n");
  Text+=TEXT("\nLEFT / RIGHT  Browse\nO  Messages    P  Close phone");
  DisplayedText=FText::FromString(Text);Widget->SetPhoneText(DisplayedText);
  if(const auto* Selected=Offers.FindByPredicate([&](const auto& Offer){return Offer.OpportunityID==SelectedOpportunity;}))Widget->SetPhotoPreview(Selected->PreviewImageFile);
  return;
 }
	if(ActiveApp==ENeonCityPhoneApp::Properties)
 {
  const auto Properties=GI->GetSubsystem<UPropertySubsystem>()->GetProperties();
  FString Text=TEXT("NEONCITY / REAL ESTATE\nProperty catalogue\n\n");
  if(!Properties.IsEmpty())
  {
   SelectedProperty=FMath::Clamp(SelectedProperty,0,Properties.Num()-1); const auto& Data=Properties[SelectedProperty];
   int32 Stored=0; for(const auto& Slot:Data.GarageSlots) if(Slot.VehicleID.IsValid()) ++Stored;
   Text+=FString::Printf(TEXT("%d / %d\n%s\n\nPrice  $%lld\n%s\nLocation  %.0f, %.0f\n\n%s\nGarage  %d / %d registered\n\n"),SelectedProperty+1,Properties.Num(),*Data.DisplayName.ToString(),Data.Price,*Data.Address.ToString(),Data.Location.X,Data.Location.Y,Data.Owned ? (Data.PropertyID==GI->GetSubsystem<UPropertySubsystem>()->GetHomePropertyID() ? TEXT("Owned / Home") : TEXT("Owned")) : TEXT("Not owned"),Stored,Data.GarageSlots.Num());
  }
  else Text+=TEXT("Catalogue unavailable.\n\n");
  Text+=TEXT("LEFT / RIGHT  Browse properties\nH  Messages\nP  Close phone");
  DisplayedText=FText::FromString(Text); Widget->SetPhoneText(DisplayedText); return;
 }
 if(ActiveApp==ENeonCityPhoneApp::Vehicles)
 {
  const auto Profiles=GI->GetSubsystem<UAutoMarketSubsystem>()->GetVehicleProfiles();
  FString Text=TEXT("NEONCITY / AUTOMARKET\nVehicle catalogue\n\n");
  if(!Profiles.IsEmpty())
  {
   SelectedVehicle=FMath::Clamp(SelectedVehicle,0,Profiles.Num()-1); const auto* Data=Profiles[SelectedVehicle];
   Text+=FString::Printf(TEXT("%d / %d  -  %s\n\nMax speed    %.0f km/h\nAcceleration %.1f m/s2\nWeight       %.0f kg\nHandling     %.0f deg\nGrip         %.1f\nDurability   %.0f HP\n\n"),SelectedVehicle+1,Profiles.Num(),*Data->DisplayName.ToString(),Data->MaxSpeed,Data->Acceleration,Data->Weight,Data->Handling,Data->Grip,Data->Durability);
  }
  else Text+=TEXT("Catalogue unavailable.\n\n");
  Text+=FString::Printf(TEXT("Owned vehicles: %d\n\nLEFT / RIGHT  Browse models\nV  Messages\nH  Real Estate\nP  Close phone"),GI->GetSubsystem<UVehicleOwnershipSubsystem>()->GetOwnedCount());
  DisplayedText=FText::FromString(Text); Widget->SetPhoneText(DisplayedText); return;
 }
 const auto& Inbox=GI->GetSubsystem<UMessageSystem>()->GetMessages();
	FString Text=FString::Printf(TEXT("NEONCITY / PHONE\nCash  $%lld\n\nMESSAGES\n"),GI->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance());
	if (!Inbox.IsEmpty())
	{
		SelectedMessage=SelectedMessage==INDEX_NONE?Inbox.Num()-1:FMath::Clamp(SelectedMessage,0,Inbox.Num()-1);
		const auto& Message=Inbox[SelectedMessage];
		Text+=FString::Printf(TEXT("%d / %d\n"),SelectedMessage+1,Inbox.Num());
		Text+=FString::Printf(TEXT("%s  |  %s UTC\n\n%s\n\n"),*Message.Sender.ToString(),*Message.Timestamp.ToString(TEXT("%H:%M")),*Message.Content.ToString());
		if (GI->GetSubsystem<UPhoneSubsystem>()->HasAction(Message.ActionId)) { Text+=TEXT("ENTER  Accept\n"); }
  auto* Opportunities=GI->GetSubsystem<UNeonCityOpportunitySubsystem>();
  for(const auto& Offer:Opportunities->GetOpportunities())if(Offer.OpportunityID==Message.ActionId && !Offer.AcceptanceAvailable && !Offer.UnavailableReason.IsEmpty()
   && (Offer.State==ENeonCityOpportunityState::Offered || Offer.State==ENeonCityOpportunityState::Ignored))Text+=Offer.UnavailableReason.ToString()+TEXT("\n");
  if(Opportunities->CanChoose(Message.ActionId,ENeonCityOpportunityChoice::Decline))Text+=TEXT("DEL  Decline\n");
  if(Opportunities->CanChoose(Message.ActionId,ENeonCityOpportunityChoice::Ignore))Text+=TEXT("BACKSPACE  Keep for later\n");
	}
	else { Text+=TEXT("No messages.\n\n"); }
	Text+=TEXT("\nLEFT / RIGHT  Messages\nO  Opportunities    B  Bank\nV  AutoMarket    H  Real Estate\nN  Coastal Life    P  Close phone");
 DisplayedText=FText::FromString(Text);
	Widget->SetPhoneText(DisplayedText);
}
FName UPhoneComponent::SelectedAction() const
{
 if(!bOpen)return NAME_None;
 if(ActiveApp==ENeonCityPhoneApp::Opportunities)return SelectedOpportunity;
 if(ActiveApp!=ENeonCityPhoneApp::Messages)return NAME_None;
 const auto& Inbox=GetWorld()->GetGameInstance()->GetSubsystem<UMessageSystem>()->GetMessages();
 return Inbox.IsEmpty()?NAME_None:Inbox[FMath::Clamp(SelectedMessage==INDEX_NONE?Inbox.Num()-1:SelectedMessage,0,Inbox.Num()-1)].ActionId;
}
void UPhoneComponent::AcceptInput()
{
 if(DispatchPage(ENeonCityPhonePageInput::Accept))return;
 if(IsBankActive()) { ConfirmLoanInput();return; }
 if(ActiveApp==ENeonCityPhoneApp::Opportunities)
 {
  auto* Router=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>();
  if(Router->CanChoose(SelectedAction(),ENeonCityOpportunityChoice::Action))
  {Router->Choose(SelectedAction(),ENeonCityOpportunityChoice::Action,Cast<APlayerController>(GetOwner()));Refresh();return;}
 }
 if(!SelectedAction().IsNone())GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->ExecuteAction(SelectedAction(),Cast<APlayerController>(GetOwner()));
 Refresh();
}
void UPhoneComponent::DeclineInput()
{
 if(DispatchPage(ENeonCityPhonePageInput::Delete))return;
 if(ActiveApp==ENeonCityPhoneApp::Opportunities)
 {
  auto* Router=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>();
  if(Router->CanChoose(SelectedAction(),ENeonCityOpportunityChoice::Cancel))
  {Router->Choose(SelectedAction(),ENeonCityOpportunityChoice::Cancel,Cast<APlayerController>(GetOwner()));Refresh();return;}
 }
 if(!SelectedAction().IsNone())GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->Choose(SelectedAction(),ENeonCityOpportunityChoice::Decline,Cast<APlayerController>(GetOwner()));
 Refresh();
}
void UPhoneComponent::IgnoreInput()
{
 if(DispatchPage(ENeonCityPhonePageInput::Alternate))return;
 if(!SelectedAction().IsNone())GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->Choose(SelectedAction(),ENeonCityOpportunityChoice::Ignore,Cast<APlayerController>(GetOwner()));
 Refresh();
}
bool UPhoneComponent::AcceptLatestMessage()
{
	if (!bOpen || ActiveApp!=ENeonCityPhoneApp::Messages) { return false; }
	const auto& Inbox=GetWorld()->GetGameInstance()->GetSubsystem<UMessageSystem>()->GetMessages();
	if (Inbox.IsEmpty()) { return false; }
	// Copy before executing: the handler may append a message and reallocate the inbox.
	const FName Action=Inbox.Last().ActionId;
	bool Accepted=GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->ExecuteAction(Action,Cast<APlayerController>(GetOwner()));
	Refresh(); return Accepted;
}
void UPhoneComponent::InboxChanged()
{
	SelectedMessage=INDEX_NONE;
	Refresh();
	if (!bOpen && GEngine) { GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),15.f,FColor::Cyan,TEXT("New phone message. Press P to read.")); }
}
void UPhoneComponent::AccountsChanged() { bConfirmLoan=false;BankNotice.Reset();Refresh(); }
void UPhoneComponent::EndPlay(EEndPlayReason::Type Reason)
{
	RemoveInput();
	if (Widget) { Widget->RemoveFromParent(); }
	if (UGameInstance* GI=GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
  if(auto* Fish=GI->GetSubsystem<UNeonCityFishingSubsystem>())Fish->OnInventoryChanged.RemoveAll(this);
		if(auto* Property=GI->GetSubsystem<UPropertySubsystem>()) Property->OnPropertyChanged.RemoveAll(this);
  if(auto* Opportunities=GI->GetSubsystem<UNeonCityOpportunitySubsystem>())Opportunities->OnOpportunitiesChanged.RemoveAll(this);
		if (auto* M=GI->GetSubsystem<UMessageSystem>()) { M->OnInboxChanged.RemoveAll(this); }
		if (auto* E=GI->GetSubsystem<UNeonCityEconomySubsystem>()) { E->OnAccountsChanged.RemoveAll(this); }
		if (auto* P=GI->GetSubsystem<UPhoneSubsystem>()) { P->UnregisterApp(ENeonCityPhoneApp::Messages,this); }
	}
	Super::EndPlay(Reason);
}

bool UPhoneComponent::ShowVehicleMarket()
{
 auto* PC=Cast<APlayerController>(GetOwner()); if(!PC || !PC->IsLocalController()) return false;
 ActiveApp=ENeonCityPhoneApp::Vehicles; if(!bOpen) TogglePhone(); Refresh(); return bOpen;
}
void UPhoneComponent::SwitchApp()
{
 if(!bOpen) return;
 auto* Router=GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>();
 Router->OpenApp(ActiveApp==ENeonCityPhoneApp::Vehicles?ENeonCityPhoneApp::Messages:ENeonCityPhoneApp::Vehicles,Cast<APlayerController>(GetOwner()));
}
void UPhoneComponent::NextVehicle()
{
 if(DispatchPage(ENeonCityPhonePageInput::Next))return;
 if(bOpen && (ActiveApp==ENeonCityPhoneApp::Messages || ActiveApp==ENeonCityPhoneApp::Opportunities)) { BrowseMessagesOrOpportunities(1);return; }
 if(bOpen && ActiveApp==ENeonCityPhoneApp::Properties)
 { const int32 Count=GetWorld()->GetGameInstance()->GetSubsystem<UPropertySubsystem>()->GetProperties().Num(); if(Count>0) SelectedProperty=(SelectedProperty+1)%Count; Refresh(); return; }
 if(!bOpen || ActiveApp!=ENeonCityPhoneApp::Vehicles) return;
 const int32 Count=GetWorld()->GetGameInstance()->GetSubsystem<UAutoMarketSubsystem>()->GetVehicleProfiles().Num();
 if(Count>0) SelectedVehicle=(SelectedVehicle+1)%Count; Refresh();
}
void UPhoneComponent::PreviousVehicle()
{
 if(DispatchPage(ENeonCityPhonePageInput::Previous))return;
 if(bOpen && (ActiveApp==ENeonCityPhoneApp::Messages || ActiveApp==ENeonCityPhoneApp::Opportunities)) { BrowseMessagesOrOpportunities(-1);return; }
 if(bOpen && ActiveApp==ENeonCityPhoneApp::Properties)
 { const int32 Count=GetWorld()->GetGameInstance()->GetSubsystem<UPropertySubsystem>()->GetProperties().Num(); if(Count>0) SelectedProperty=(SelectedProperty+Count-1)%Count; Refresh(); return; }
 if(!bOpen || ActiveApp!=ENeonCityPhoneApp::Vehicles) return;
 const int32 Count=GetWorld()->GetGameInstance()->GetSubsystem<UAutoMarketSubsystem>()->GetVehicleProfiles().Num();
 if(Count>0) SelectedVehicle=(SelectedVehicle+Count-1)%Count; Refresh();
}

void UPhoneComponent::PropertyChanged(FName PropertyID) { Refresh(); }
bool UPhoneComponent::ShowRealEstate()
{
 auto* PC=Cast<APlayerController>(GetOwner()); if(!PC || !PC->IsLocalController()) return false;
 ActiveApp=ENeonCityPhoneApp::Properties; if(!bOpen) TogglePhone(); Refresh(); return bOpen;
}
void UPhoneComponent::SwitchPropertyApp()
{
 if(!bOpen) return;
 GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->OpenApp(ActiveApp==ENeonCityPhoneApp::Properties ? ENeonCityPhoneApp::Messages : ENeonCityPhoneApp::Properties,Cast<APlayerController>(GetOwner()));
}

void UPhoneComponent::OpportunitiesChanged() { Refresh(); }
void UPhoneComponent::ScrollUp() { if(bOpen && Widget)Widget->ScrollPage(-1); }
void UPhoneComponent::ScrollDown() { if(bOpen && Widget)Widget->ScrollPage(1); }
bool UPhoneComponent::ShowOpportunities()
{
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->IsLocalController())return false;
 ActiveApp=ENeonCityPhoneApp::Opportunities;if(!bOpen)TogglePhone();Refresh();return bOpen;
}
void UPhoneComponent::SwitchOpportunityApp()
{
 if(!bOpen)return;
 GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->OpenApp(ActiveApp==ENeonCityPhoneApp::Opportunities?ENeonCityPhoneApp::Messages:ENeonCityPhoneApp::Opportunities,Cast<APlayerController>(GetOwner()));
}
void UPhoneComponent::BrowseMessagesOrOpportunities(int32 Direction)
{
 auto* GI=GetWorld()->GetGameInstance();
 if(ActiveApp==ENeonCityPhoneApp::Messages)
 {
  const int32 Count=GI->GetSubsystem<UMessageSystem>()->GetMessages().Num();
  if(Count>0)SelectedMessage=((SelectedMessage==INDEX_NONE?Count-1:SelectedMessage)+Count+Direction)%Count;
 }
 else if(ActiveApp==ENeonCityPhoneApp::Opportunities)
 {
  const auto Offers=GI->GetSubsystem<UNeonCityOpportunitySubsystem>()->GetOpportunities();
  const int32 Index=Offers.IndexOfByPredicate([&](const auto& Offer){return Offer.OpportunityID==SelectedOpportunity;});
  if(!Offers.IsEmpty())SelectedOpportunity=Offers[(FMath::Max(0,Index)+Offers.Num()+Direction)%Offers.Num()].OpportunityID;
 }
 Refresh();
}

bool UPhoneComponent::ShowPage(ENeonCityPhoneApp App)
{
 if(!Cast<IPhonePageInterface>(GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->GetAppObject(App)))return false;
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->IsLocalController())return false;
 ActiveApp=App;if(!bOpen)TogglePhone();Refresh();return bOpen;
}
bool UPhoneComponent::DispatchPage(ENeonCityPhonePageInput Input)
{
 if(!bOpen)return false;
 auto* Page=Cast<IPhonePageInterface>(GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->GetAppObject(ActiveApp));
 if(!Page)return false;Page->PageInput(Cast<APlayerController>(GetOwner()),Input);Refresh();return true;
}
void UPhoneComponent::PageChanged() { Refresh(); }
void UPhoneComponent::SwitchFishingApp()
{
 if(!bOpen)return;
 GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->OpenApp(ActiveApp==ENeonCityPhoneApp::Fishing?ENeonCityPhoneApp::Messages:ENeonCityPhoneApp::Fishing,Cast<APlayerController>(GetOwner()));
}
