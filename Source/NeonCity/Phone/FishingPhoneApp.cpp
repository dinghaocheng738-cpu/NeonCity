#include "Phone/FishingPhoneApp.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Fishing/FishingStation.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
void UFishingPhoneApp::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);Collection.InitializeDependency<UPhoneSubsystem>();Collection.InitializeDependency<UNeonCityFishingSubsystem>();
 TScriptInterface<IPhoneAppInterface> App;App.SetObject(this);App.SetInterface(this);GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->RegisterApp(App);
}
void UFishingPhoneApp::Deinitialize()
{ if(auto* Phone=GetGameInstance()->GetSubsystem<UPhoneSubsystem>())Phone->UnregisterApp(ENeonCityPhoneApp::Fishing,this);Super::Deinitialize(); }
bool UFishingPhoneApp::ValidPlayer(APlayerController* Player) const
{ return IsValid(Player) && Player->IsLocalController() && Player->GetGameInstance()==GetGameInstance(); }
bool UFishingPhoneApp::OpenPhoneApp_Implementation(APlayerController* Player)
{ if(!ValidPlayer(Player))return false;Notice.Reset();auto* Phone=Player->FindComponentByClass<UPhoneComponent>();return Phone && Phone->ShowPage(ENeonCityPhoneApp::Fishing); }
FText UFishingPhoneApp::BuildPage(APlayerController* Player)
{
 if(!ValidPlayer(Player))return FText::GetEmpty();auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
 FString Text=TEXT("NEONCITY / COASTAL LIFE\n\n");
 if(Tab==0)
 {
  TArray<FNeonCityCaughtFish> Pouch;for(const auto& Fish:Inventory->GetFish())if(Fish.Storage==ENeonCityFishStorage::Pouch)Pouch.Add(Fish);
  Text+=FString::Printf(TEXT("FISH POUCH  %d / 64\n\n"),Pouch.Num());
  if(Pouch.IsEmpty())Text+=TEXT("No catches yet.\nChoose Places to find a coastal spot.\nStow your weapon before fishing.\n");
  else
  {
   Selected=FMath::Clamp(Selected,0,Pouch.Num()-1);const auto& Fish=Pouch[Selected];
   if(const auto* Profile=Inventory->FindSpecies(Fish.SpeciesID))Text+=FString::Printf(TEXT("%d / %d  %s\n%.2f kg  |  %.1f cm\nMarket value  $%lld\nCaught  %s UTC\n\nSell at the fish market or cook at a grill.\n"),Selected+1,Pouch.Num(),*Profile->Name.ToString(),Fish.WeightKg,Fish.SizeCm,Inventory->GetSaleValue(Fish),*Fish.CaughtAt.ToString(TEXT("%Y-%m-%d %H:%M")));
  }
 }
 else if(Tab==1)
 {
  const auto& Meals=Inventory->GetMeals();Text+=FString::Printf(TEXT("PREPARED FOOD  %d / 16\n\n"),Meals.Num());
  if(Meals.IsEmpty())Text+=TEXT("No prepared food.\nTake a kept catch to a grill.\n");
  else { Selected=FMath::Clamp(Selected,0,Meals.Num()-1);Text+=FString::Printf(TEXT("%d / %d  Grilled fish\n\nRestores up to 25%% maximum health\nand 20 stamina.\nFood is kept if you are already full.\n\nENTER  Eat selected meal\n"),Selected+1,Meals.Num()); }
 }
 else
 {
  Selected=FMath::Clamp(Selected,0,2);const TCHAR* Places[]={TEXT("Coastal fishing point"),TEXT("Fresh fish market"),TEXT("Public grill")};
  Text+=FString::Printf(TEXT("PLACES\n\n%d / 3  %s\n\nENTER  Set a walking destination\nUse the map to reach the place.\nFishing requires casting, hooking\nand controlling line tension.\n"),Selected+1,Places[Selected]);
 }
 if(!Notice.IsEmpty())Text+=TEXT("\n")+Notice+TEXT("\n");
 Text+=TEXT("\nLEFT / RIGHT  Browse\nBACKSPACE  Pouch / Food / Places\nN  Messages    P  Close phone");return FText::FromString(Text);
}
void UFishingPhoneApp::PageInput(APlayerController* Player,ENeonCityPhonePageInput Input)
{
 if(!ValidPlayer(Player))return;auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Notice.Reset();
 if(Input==ENeonCityPhonePageInput::Alternate) { Tab=(Tab+1)%3;Selected=0;return; }
 const int32 Count=Tab==0?Inventory->PouchCount():(Tab==1?Inventory->GetMeals().Num():3);
 if(Input==ENeonCityPhonePageInput::Next || Input==ENeonCityPhonePageInput::Previous)
 { if(Count)Selected=(FMath::Clamp(Selected,0,Count-1)+Count+(Input==ENeonCityPhonePageInput::Next?1:-1))%Count;return; }
 if(Input!=ENeonCityPhonePageInput::Accept)return;
 if(Tab==1 && Count)
 {
  Selected=FMath::Clamp(Selected,0,Count-1);const FGuid ID=Inventory->GetMeals()[Selected].MealID;
  Notice=Inventory->EatMeal(ID,Player->GetPawn())?TEXT("Meal eaten."):TEXT("Food kept. Eat on foot when you need health or stamina.");
 }
 else if(Tab==2 && Player->GetPawn())
 {
  const ENeonCityFishingStation Kinds[]={ENeonCityFishingStation::CastingPoint,ENeonCityFishingStation::FishMarket,ENeonCityFishingStation::Grill};
  auto* Station=Inventory->FindNearestStation(Player->GetPawn()->GetActorLocation(),Kinds[FMath::Clamp(Selected,0,2)]);
  auto* Navigation=Player->FindComponentByClass<UNeonCityNavigationComponent>();
  if(Station && Navigation)
  {
   FNeonCityNavigationDestination Destination;Destination.OwnerID=TEXT("Phone.CoastalLife");Destination.DestinationID=Station->StationID;
   Destination.Label=Station->DisplayName;Destination.Location=Station->GetActorLocation();Destination.MovingTarget=Station;
   Notice=Navigation->SetDestination(Destination)?TEXT("Destination set. Close the phone to follow the map."):TEXT("Destination unavailable.");
  }
  else Notice=TEXT("No matching place is available in this area.");
 }
}
