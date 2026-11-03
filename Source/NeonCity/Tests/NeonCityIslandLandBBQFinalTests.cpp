#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandDoor.h"
#include "Adventure/IslandLandGrill.h"
#include "Adventure/IslandLandResidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Phone/PhoneComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWaveProcedural.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/SecureHash.h"
#include "Misc/App.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioMixerDevice.h"

namespace NeonCityLandBBQFinal
{
const FString ArrivalSlot=TEXT("NeonCity_Test_LandFinal_BBQArrival");
const FString StoreSlot=TEXT("NeonCity_Test_LandFinal_BBQStore15");
const FString EatenSlot=TEXT("NeonCity_Test_LandFinal_BBQEaten15");
const FVector MansionOrigin=NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30480));
FVector M(double X,double Y,double Z=0){return MansionOrigin+FVector(100*X,-100*Y,100*Z);}
TSharedPtr<FJsonValue> Vec(const FVector& P)
{TArray<TSharedPtr<FJsonValue>> A;for(double N:{P.X,P.Y,P.Z})A.Add(MakeShared<FJsonValueNumber>(N));return MakeShared<FJsonValueArray>(A);}
void Key(APlayerController* PC,FKey K,EInputEvent Event)
{PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));}
FString SlotHash(const FString& Slot)
{
 TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Slot+TEXT(".sav")))))return {};
 FSHAHash Hash;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash.Hash);return Hash.ToString();
}
TSharedPtr<FJsonValue> FishJSON(const FNeonCityCaughtFish& F)
{
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("fish_guid"),F.FishID.ToString());J->SetStringField(TEXT("species"),F.SpeciesID.ToString());
 J->SetStringField(TEXT("fishing_point"),F.FishingPointID.ToString());J->SetStringField(TEXT("caught_utc"),F.CaughtAt.ToIso8601());
 J->SetNumberField(TEXT("weight_kg"),F.WeightKg);J->SetNumberField(TEXT("size_cm"),F.SizeCm);J->SetNumberField(TEXT("storage"),int32(F.Storage));return MakeShared<FJsonValueObject>(J);
}
class FBBQCrossSession final:public IAutomationLatentCommand
{
public:
 FBBQCrossSession(FAutomationTestBase* T,FString S,FString P):Test(T),Session(MoveTemp(S)),Pair(MoveTemp(P)),Receipt(MakeShared<FJsonObject>())
 {
  // Constructed while commands are queued, before this test creates PIE.
  // StartRecordingOutput resets any recording already held by the submix;
  // UE 5.8 exposes no public bIsRecording query. Refuse all pre-existing
  // devices rather than overwrite another recorder or the editor's device.
  if(auto* Manager=GEngine?GEngine->GetAudioDeviceManager():nullptr)
   for(auto* Device:Manager->GetAudioDevices())if(Device)PreexistingAudioDevices.Add(Device->DeviceID);
 }
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();Now=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(Now-Started>240)return Fail(TEXT("Timed out at stage ")+FString::FromInt(Stage));
  if(!Player || W->GetTimeSeconds()<4)return false;
  SampleFrame();
  if(TappedKey.IsValid())
  {
   if(Now-TappedAt<.16)return false;Key(PC,TappedKey,IE_Released);TappedKey=FKey();return Next(TapNext);
  }
  if(Stage==90)return Finish();
  if(Stage==0)return Initialize();
  auto* Region=ANeonCityCoastalRegion::Find(W);if(Region)Region->EnsureDestinationReady(Player->GetActorLocation());
  if(Stage==1)
  {
   if(Save->IsOperationInProgress() || Now-At<.7 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   if(!Test->TestEqual(TEXT("Native checkpoint load succeeded"),Save->GetLastResult(),ENeonCitySaveResult::Success))return Fail(Save->GetLastDetail());
   if(!Test->TestTrue(TEXT("Native load retains the exact saved mansion checkpoint"),FVector::Dist(Player->GetActorLocation(),Expected->Player.Transform.GetLocation())<20))return Fail(TEXT("Saved checkpoint unexpectedly moved"));
   if(!Test->TestTrue(TEXT("Mansion static authority is unchanged by load"),Residence->GetActorTransform().Equals(ResidenceBeforeLoad,.001)))return Fail(TEXT("Residence transform changed"));
   if(auto* Weapon=Player->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {
    const int32 Slot=Weapon->GetData()?Weapon->GetData()->SlotType:0;
    const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};
    if(Slot<1 || Slot>8)return Fail(TEXT("Existing equipped weapon has no native inventory-slot key"));
    if(++StowAttempts>1)return Fail(TEXT("Native slot input failed to stow the saved equipped weapon"));
    Event(TEXT("NATIVE_INVENTORY_SLOT_INPUT_STOWS_EXISTING_WEAPON"));return Tap(Slots[Slot-1],1);
   }
   for(const FGuid& ID:{FirstFish,SecondFish})
    if(Session==TEXT("A") || ID==SecondFish)if(!Inventory->FindFish(ID))return Fail(TEXT("Native load lost an original caught fish GUID"));
   if(Session==TEXT("B"))
   {
    if(!HasMeal(FirstMeal) || Inventory->FindFish(FirstFish))return Fail(TEXT("Cross-process stored food/raw-fish transaction did not restore"));
    Event(TEXT("CROSS_PROCESS_STORED_MEAL_RESTORED"));Shot(TEXT("StoredMeal-NativeLoad"));
    Path={M(10.2,16.2),M(12.2,16.2)};Waypoint=0;return Next(4);
   }
   if(FVector::Dist2D(Player->GetActorLocation(),Door->GetActorLocation())>500)return Fail(TEXT("Arrival checkpoint is not the real mansion entry checkpoint"));
   Path={M(-14.0,1.15)};Waypoint=0;return Next(2);
  }
  if(Stage==2)
  {
   if(!WalkPath())return false;if(Test->HasAnyErrors())return Finish();
   if(Now-LastWalkDone<.35 || Player->GetVelocity().Size2D()>60)return false;
   const FVector Aim=Door->GetActorTransform().TransformPosition(FVector(65,0,110));PC->SetControlRotation((Aim-Player->GetPawnViewLocation()).Rotation());
   if(!Test->TestEqual(TEXT("Native mansion entry prompt is focused from the actual checkpoint"),Player->GetInteractionComponent()->RefreshFocusedInteractable(),static_cast<AActor*>(Door)))return Fail(TEXT("Mansion door cannot be focused"));
   Event(TEXT("MANSION_PHYSICAL_DOOR_PROMPT"));Shot(TEXT("MansionEntry-Prompt"));
   if(Door->IsOpen())return Next(3);return Tap(EKeys::F,3);
  }
  if(Stage==3)
  {
   if(!Door->IsOpen())
   {
    if(Now-At>20)return Fail(TEXT("Native F input did not open the physical mansion door within20s"));
    return false;
   }
   Event(TEXT("MANSION_PHYSICAL_DOOR_OPENED"));
   Path={M(-14.0,4.2),M(-7,5),M(0,5),M(0,8),M(0,11.8),M(3,12.2),M(9.3,12.2),M(10.2,13.2),M(10.2,16.2),M(12.2,16.2)};
   Waypoint=0;return Next(4);
  }
  if(Stage==4)
  {
   if(!WalkPath())return false;if(Test->HasAnyErrors())return Finish();
   if(Now-LastWalkDone<.45 || Player->GetVelocity().Size2D()>60)return false;
   PC->SetControlRotation((Grill->InteractionBounds->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());
   if(!Test->TestEqual(TEXT("Normal player eye trace focuses the actual BBQ"),Player->GetInteractionComponent()->RefreshFocusedInteractable(),static_cast<AActor*>(Grill)))return Fail(TEXT("Grill focus failed after the real walk"));
   if(!Grill->CanInteract_Implementation(Player))return Fail(TEXT("Actual grill is not reachable through native collision"));
   Event(TEXT("GRILL_INTERACTION_PROMPT"));Shot(TEXT("Grill-Prompt"));return Tap(EKeys::F,5);
  }
  if(Stage==5)
  {
   if(Now-At<.35)return false;if(!Grill->IsLit())return Fail(TEXT("First native F did not ignite BBQ"));
   if(!Heat || Heat->Intensity<=0 || !Coal || Coal->K2_GetScalarParameterValue(TEXT("Heat"))<.9f)return Fail(TEXT("Ignition lacks real ember light/material feedback"));
   Event(TEXT("GRILL_IGNITED_NATIVE_HEAT"));Shot(TEXT("Grill-Ignited"));return Tap(EKeys::F,6);
  }
  if(Stage==6)
  {
   if(Now-At<.35)return false;if(Fishing->GetActiveStation()!=Grill || !PC->IsMoveInputIgnored())return Fail(TEXT("Second native F did not open the fish selection menu"));
   WantedFish=Session==TEXT("A")?FirstFish:SecondFish;Pouch.Reset();
   for(const auto& F:Inventory->GetFish())if(F.Storage==ENeonCityFishStorage::Pouch)Pouch.Add(F);
   TargetIndex=Pouch.IndexOfByPredicate([&](const auto& F){return F.FishID==WantedFish;});
   if(TargetIndex==INDEX_NONE)return Fail(TEXT("Actual caught ingredient is absent from the native pouch"));
   SelectedIndex=0;return Next(7);
  }
  if(Stage==7)
  {
   if(Now-At<.25)return false;
   if(SelectedIndex<TargetIndex){++SelectedIndex;return Tap(EKeys::Right,7);}
   const auto* Profile=Inventory->FindSpecies(Pouch[TargetIndex].SpeciesID);
   if(!Profile || !Fishing->GetDisplayText().ToString().Contains(Profile->Name.ToString())
    || !Fishing->GetDisplayText().ToString().Contains(FString::Printf(TEXT("%d / %d"),TargetIndex+1,Pouch.Num())))
    return Fail(TEXT("Native menu does not present the selected actual species/index"));
   CurrentSpecies=Profile;MealsBefore=Inventory->GetMeals();FishCountBefore=Inventory->GetFish().Num();
   Event(TEXT("ACTUAL_FISH_SELECTED_INGREDIENT: ")+WantedFish.ToString());Shot(TEXT("Fish-Selected"));
   if(!StartCookingOutputRecording())return Fail(TEXT("Actual PIE master-output recording could not start safely"));return Tap(EKeys::Enter,8);
  }
  if(Stage==8)
  {
   if(Now-At<.25)return false;
   if(!Fishing->IsCookingAt(Grill) || !Grill->IsCooking() || !Inventory->FindFish(WantedFish) || Inventory->GetMeals().Num()!=MealsBefore.Num())
    return Fail(TEXT("Native ENTER did not start timed cooking while retaining the raw ingredient"));
   if(Fishing->GetCookingSeconds()<=0 || Fishing->GetCookingSeconds()>3)return Fail(TEXT("Native three-second cooking state invalid"));
   FoodVisual=FindFoodVisual(true);
   auto* Wave=Sizzle?Cast<USoundWaveProcedural>(Sizzle->Sound):nullptr;
   if(!FoodVisual || !Grill->IsCookingAudioPlaying() || !Sizzle->bOverrideAttenuation || !Sizzle->AttenuationOverrides.bSpatialize
    || !Wave || Wave->GetAvailableAudioByteCount()==0 || Heat->Intensity<=0 || Coal->K2_GetScalarParameterValue(TEXT("Heat"))<.9f)
    return Fail(TEXT("Cooking lacks actual fish-on-grate, spatial PCM audio, or heat feedback"));
   CookStartGame=W->GetTimeSeconds();HeatMinimum=HeatMaximum=Coal->K2_GetScalarParameterValue(TEXT("Heat"));
   Event(TEXT("TIMED_COOKING_STARTED_WITH_VISUAL_AUDIO_HEAT"));Shot(TEXT("Cooking-ActualFish"));return Next(9);
  }
  if(Stage==9)
  {
   if(Fishing->IsCookingAt(Grill))
   {
    const float H=Coal->K2_GetScalarParameterValue(TEXT("Heat"));HeatMinimum=FMath::Min(HeatMinimum,H);HeatMaximum=FMath::Max(HeatMaximum,H);
    if(!Inventory->FindFish(WantedFish) || Inventory->GetMeals().Num()!=MealsBefore.Num())return Fail(TEXT("Ingredient was consumed before timed cooking completed"));
    if(Now-At>12)return Fail(TEXT("Cooking timer did not complete"));return false;
   }
   FinishedGame=W->GetTimeSeconds();if(!StopCookingOutputRecording(true))return Fail(TEXT("Actual PIE master-output recording could not stop on its original device"));return Next(10);
  }
  if(Stage==10)
  {
   if(Now-At<.5)return false;
   if(Grill->IsCooking() || Grill->IsCookingAudioPlaying() || Inventory->FindFish(WantedFish) || Inventory->GetFish().Num()!=FishCountBefore-1
    || Inventory->GetMeals().Num()!=MealsBefore.Num()+1 || !Fishing->GetDisplayText().ToString().Contains(TEXT("Grilled fish ready.")))
    return Fail(TEXT("Cooking completion did not commit one real ingredient into prepared food with feedback"));
   if(FinishedGame-CookStartGame<2.3f || HeatMaximum-HeatMinimum<.02f)return Fail(TEXT("Cooking did not retain real duration and changing heat feedback"));
   CurrentMeal.Invalidate();for(const auto& Meal:Inventory->GetMeals())
    if(!MealsBefore.ContainsByPredicate([&](const auto& Old){return Old.MealID==Meal.MealID;}))CurrentMeal=Meal.MealID;
   if(!CurrentMeal.IsValid() || FindFoodVisual(true))return Fail(TEXT("Raw food visual/result state did not change at completion"));
   if(Session==TEXT("A")){FirstMeal=CurrentMeal;if(!Inventory->FindFish(SecondFish))return Fail(TEXT("Store cook consumed the second actual fish"));}
   else{SecondMeal=CurrentMeal;if(!HasMeal(FirstMeal))return Fail(TEXT("Second cook lost the cross-process stored meal"));}
   auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("ingredient_guid"),WantedFish.ToString());C->SetStringField(TEXT("meal_guid"),CurrentMeal.ToString());
   C->SetNumberField(TEXT("timed_cooking_game_seconds"),FinishedGame-CookStartGame);C->SetNumberField(TEXT("heat_min"),HeatMinimum);C->SetNumberField(TEXT("heat_max"),HeatMaximum);
   C->SetBoolField(TEXT("food_mesh_visible_while_cooking"),true);C->SetBoolField(TEXT("spatial_pcm_audio_played"),true);C->SetBoolField(TEXT("audio_stopped_after_completion"),true);
   C->SetStringField(TEXT("completion_notice"),Fishing->GetDisplayText().ToString());Cooking.Add(MakeShared<FJsonValueObject>(C));
   Event(TEXT("COOKING_COMMITTED_PREPARED_FOOD"));Shot(TEXT("Cooking-Completed"));return Tap(EKeys::P,11);
  }
  if(Stage==11)
  {
   if(Now-At<.3)return false;
   if(!Phone->IsPhoneOpen() || Fishing->IsActive() || PC->IsMoveInputIgnored())return Fail(TEXT("Native P did not leave BBQ and open the normal phone"));
   if(Phone->GetActiveApp()!=ENeonCityPhoneApp::Fishing)return Tap(EKeys::N,12);return Next(12);
  }
  if(Stage==12)
  {
   if(Now-At<.25)return false;if(Phone->GetActiveApp()!=ENeonCityPhoneApp::Fishing)return Fail(TEXT("Native N did not open Coastal Life"));
   if(!Phone->GetDisplayedText().ToString().Contains(TEXT("PREPARED FOOD")))
   {if(++TabChanges>3)return Fail(TEXT("Native BACKSPACE could not reach Prepared Food"));return Tap(EKeys::BackSpace,12);}
   MealTarget=Inventory->GetMeals().IndexOfByPredicate([&](const auto& Meal){return Meal.MealID==CurrentMeal;});MealSelected=0;
   if(MealTarget==INDEX_NONE)return Fail(TEXT("Cooked food result missing from the native phone"));return Next(13);
  }
  if(Stage==13)
  {
   if(Now-At<.25)return false;if(MealSelected<MealTarget){++MealSelected;return Tap(EKeys::Right,13);}
   Event(TEXT("PREPARED_FOOD_RESULT_PRESENTED_IN_NATIVE_PHONE"));Shot(TEXT("Prepared-Food-Result"));
   if(Session==TEXT("A"))return Tap(EKeys::P,20);
   NeedHealth=Player->GetHealthComponent()->GetCurrentHealth();NeedStamina=Player->GetPlayerDataComponent()->GetStamina();MealsForEat=Inventory->GetMeals().Num();
   if(NeedHealth>=Player->GetHealthComponent()->GetMaxHealth() && NeedStamina>=100)
   {Event(TEXT("FULL_PLAYER_ENTER_NEGATIVE_CHECK"));return Tap(EKeys::Enter,14);}
   Receipt->SetStringField(TEXT("eat_need_origin"),TEXT("Existing saved player health/stamina need; no test condition change"));return Next(16);
  }
  if(Stage==14)
  {
   if(Now-At<.3)return false;
   if(Inventory->GetMeals().Num()!=MealsForEat || !HasMeal(SecondMeal) || !Phone->GetDisplayedText().ToString().Contains(TEXT("Food kept.")))
    return Fail(TEXT("Full-player native ENTER wasted prepared food"));
   Receipt->SetBoolField(TEXT("full_player_native_enter_retains_food"),true);Shot(TEXT("Full-Player-Food-Retained"));
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityLandBBQDamageStimulus")))
    return Fail(TEXT("Full player cannot eat by design; opt-in native damage stimulus is required for the separate positive UI case"));
   // Explicit functional-test stimulus through the existing damage pipeline.
   // This is not natural sprint exhaustion, a stamina fixture or food bypass.
   FNeonCityDamageEvent D;D.DamageAmount=20;D.DamageType=ENeonCityDamageType::Environmental;D.HitLocation=Player->GetActorLocation();
   const float Before=Player->GetHealthComponent()->GetCurrentHealth();const float Applied=W->GetSubsystem<UDamageSystem>()->ApplyDamage(Player,D);
   auto Stimulus=MakeShared<FJsonObject>();Stimulus->SetBoolField(TEXT("test_only_damage_stimulus"),true);Stimulus->SetNumberField(TEXT("requested_damage"),20);
   Stimulus->SetNumberField(TEXT("applied_damage"),Applied);Stimulus->SetNumberField(TEXT("health_before"),Before);Stimulus->SetNumberField(TEXT("health_after"),Player->GetHealthComponent()->GetCurrentHealth());
   Stimulus->SetNumberField(TEXT("stamina_before"),NeedStamina);Stimulus->SetNumberField(TEXT("stamina_after"),Player->GetPlayerDataComponent()->GetStamina());
   Stimulus->SetStringField(TEXT("scope"),TEXT("Opt-in native DamageSystem stimulus for phone Eat positive case; not natural gameplay stamina expenditure"));
   Receipt->SetObjectField(TEXT("eat_precondition_stimulus"),Stimulus);
   if(!FMath::IsNearlyEqual(Applied,20.f) || Player->GetHealthComponent()->GetCurrentHealth()<=0 || Player->GetPlayerDataComponent()->GetStamina()!=NeedStamina)
    return Fail(TEXT("Native controlled damage stimulus did not preserve nonlethal health and unchanged stamina"));
   Event(TEXT("TEST_ONLY_NATIVE_DAMAGE_STIMULUS_20"));return Next(15);
  }
  if(Stage==15)
  {
   if(Now-At<.65)return false;auto* Body=Player->FindComponentByClass<UDamageableCharacterComponent>();
   if(Body && !Body->CanAct())return Fail(TEXT("Player did not recover from the native hit reaction"));return Next(16);
  }
  if(Stage==16)
  {
   NeedHealth=Player->GetHealthComponent()->GetCurrentHealth();NeedStamina=Player->GetPlayerDataComponent()->GetStamina();
   Event(TEXT("NATIVE_PHONE_ENTER_EAT_SELECTED_SECOND_MEAL"));return Tap(EKeys::Enter,17);
  }
  if(Stage==17)
  {
   if(Now-At<.35)return false;
   if(Inventory->GetMeals().Num()!=MealsForEat-1 || HasMeal(SecondMeal) || !HasMeal(FirstMeal) || !Phone->GetDisplayedText().ToString().Contains(TEXT("Meal eaten.")))
    return Fail(TEXT("Native phone ENTER did not eat exactly the second cooked meal while retaining the stored meal"));
   const float ExpectedHealth=FMath::Min(Player->GetHealthComponent()->GetMaxHealth(),NeedHealth+Player->GetHealthComponent()->GetMaxHealth()*.25f);
   const float ExpectedStamina=NeedStamina<100?FMath::Min(100.f,NeedStamina+20):NeedStamina;
   if(!FMath::IsNearlyEqual(Player->GetHealthComponent()->GetCurrentHealth(),ExpectedHealth) || !FMath::IsNearlyEqual(Player->GetPlayerDataComponent()->GetStamina(),ExpectedStamina))
    return Fail(TEXT("Native Eat did not apply the authored bounded health/stamina recovery"));
   Receipt->SetNumberField(TEXT("eat_health_before"),NeedHealth);Receipt->SetNumberField(TEXT("eat_health_after"),Player->GetHealthComponent()->GetCurrentHealth());
   Receipt->SetNumberField(TEXT("eat_stamina_before"),NeedStamina);Receipt->SetNumberField(TEXT("eat_stamina_after"),Player->GetPlayerDataComponent()->GetStamina());
   Event(TEXT("SECOND_REAL_FISH_MEAL_EATEN_BY_NATIVE_PHONE_INPUT"));Shot(TEXT("Phone-Meal-Eaten"));return Tap(EKeys::P,21);
  }
  if(Stage==20 || Stage==21)
  {
   if(Now-At<.25)return false;if(Phone->IsPhoneOpen() || Fishing->IsActive())return Fail(TEXT("Native phone/activity did not close before Save"));
   const FString Destination=Session==TEXT("A")?StoreSlot:EatenSlot;
   if(!Save->ConfigureSlot(Destination,false) || !Save->SaveGame())return Fail(TEXT("Native dedicated food Save failed: ")+Save->GetLastDetail());
   TStrongObjectPtr<UNeonCitySaveGame> Model(Save->ReadSaveForInspection());if(!Model.IsValid())return Fail(TEXT("Native food save integrity readback failed"));
   if(!Model->Fishing.Meals.ContainsByPredicate([&](const auto& Meal){return Meal.MealID==FirstMeal;}) || Model->Fishing.Fish.ContainsByPredicate([&](const auto& Fish){return Fish.FishID==FirstFish;}))
    return Fail(TEXT("Native saved stored meal/raw ingredient state invalid"));
   if(Session==TEXT("A"))
   {
    if(!Model->Fishing.Fish.ContainsByPredicate([&](const auto& Fish){return Fish.FishID==SecondFish;}))return Fail(TEXT("Store save lost the untouched second real fish"));
    Receipt->SetStringField(TEXT("stored_save_guid"),Model->SaveID.ToString());Receipt->SetStringField(TEXT("stored_primary_sha1"),SlotHash(StoreSlot));
    Event(TEXT("NATIVE_STORE_SAVE_COMMITTED_WITH_FIRST_MEAL_AND_SECOND_RAW_FISH"));
   }
   else
   {
    if(Model->Fishing.Meals.ContainsByPredicate([&](const auto& Meal){return Meal.MealID==SecondMeal;}) || Model->Fishing.Fish.ContainsByPredicate([&](const auto& Fish){return Fish.FishID==SecondFish;}))
     return Fail(TEXT("Native Eat save duplicated consumed meal or raw fish"));
    Receipt->SetStringField(TEXT("eaten_save_guid"),Model->SaveID.ToString());Event(TEXT("NATIVE_EAT_RESULT_SAVED"));
   }
   if(SlotHash(ArrivalSlot)!=ArrivalHash)return Fail(TEXT("Read-only continuous-arrival checkpoint was rewritten"));
   Receipt->SetBoolField(TEXT("arrival_source_primary_unchanged"),true);return Finish();
  }
  return false;
 }
private:
 bool Initialize()
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD"))
   || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))return Fail(TEXT("Final BBQ requires native stills and opt-in game audio; no SkipHUD"));
  if(Pair.IsEmpty() || FPaths::MakeValidFileName(Pair)!=Pair || (Session!=TEXT("A") && Session!=TEXT("B")))return Fail(TEXT("Exact A/B session and unique plain pair label required"));
  Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1/BBQCrossSession")/Pair;IFileManager::Get().MakeDirectory(*Directory,true);
  if(IFileManager::Get().FileExists(*(Directory/(TEXT("Session")+Session+TEXT(".json")))))return Fail(TEXT("Previous BBQ evidence retained; use a new pair label"));
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(MansionOrigin))return false;
  Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  Fishing=PC->FindComponentByClass<UFishingComponent>();Phone=PC->FindComponentByClass<UPhoneComponent>();
  for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(It->StationID==TEXT("Island.Land.AureliaBBQ"))Grill=*It;
  for(TActorIterator<AIslandLandDoor> It(W);It;++It)if(It->GetActorLocation().Z>30000)Door=*It;
  for(TActorIterator<AIslandLandResidence> It(W);It;++It)Residence=*It;
  if(!Save || !Inventory || !Fishing || !Phone || !Grill || !Door || !Residence)return Fail(TEXT("Existing native mansion/BBQ/input/persistence actors missing"));
  ResidenceBeforeLoad=Residence->GetActorTransform();Heat=Grill->FindComponentByClass<UPointLightComponent>();Sizzle=Grill->FindComponentByClass<UAudioComponent>();
  Coal=Cast<UMaterialInstanceDynamic>(Grill->Coals->GetMaterial(0));ArrivalHash=SlotHash(ArrivalSlot);if(ArrivalHash.IsEmpty())return Fail(TEXT("Continuous driving native arrival save is missing"));
  Receipt->SetStringField(TEXT("session"),Session);Receipt->SetStringField(TEXT("pair"),Pair);Receipt->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());
  Receipt->SetStringField(TEXT("scope"),TEXT("Actual arrival checkpoint and actual caught fish; native walk, prompt, F/ENTER/P/N/BACKSPACE/RIGHT cooking and phone input. No actor relocation or fish/meal grants."));
  Receipt->SetStringField(TEXT("execution_mode"),TEXT("NO COMPUTER CONTROL; opt-in RenderOffscreen normal PIE"));Receipt->SetNumberField(TEXT("explicit_actor_relocation_count"),0);
  Receipt->SetStringField(TEXT("audio_subjective_status"),TEXT("USER AUDIO REVIEW REQUIRED"));Receipt->SetBoolField(TEXT("six_scene_performance_evidence"),false);
  FString ArtRun;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),ArtRun);Receipt->SetStringField(TEXT("art_run"),ArtRun);
   AudioReceipt=MakeShared<FJsonObject>();Receipt->SetObjectField(TEXT("actual_pie_master_recording"),AudioReceipt);
   AudioReceipt->SetStringField(TEXT("scope"),TEXT("Current normal PIE device master mix during native fish Cooking through Ready; includes environment/other audible game sound, not an isolated grill stem."));
   AudioReceipt->SetStringField(TEXT("subjective_status"),TEXT("USER AUDIO REVIEW REQUIRED"));AudioReceipt->SetBoolField(TEXT("public_existing_recording_query_available"),false);
   AudioReceipt->SetStringField(TEXT("existing_recording_safety"),TEXT("Fresh process-specific PIE device only, absent from pre-PIE device IDs and used exclusively by this PIE world; no other test records in this dedicated A/B process."));
   AudioReceipt->SetStringField(TEXT("export_status"),TEXT("NOT STARTED"));
   AudioDirectory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(ArtRun)/TEXT("Audio"));
   AudioName=TEXT("BBQFinal15-")+Session+TEXT("-Native-PIE-Master");AudioFile=AudioDirectory/(AudioName+TEXT(".wav"));
   if(ArtRun.IsEmpty() || FPaths::MakeValidFileName(ArtRun)!=ArtRun || IFileManager::Get().FileExists(*AudioFile))return Fail(TEXT("Unique plain art-run label and previously absent A/B WAV required"));
   AudioReceipt->SetStringField(TEXT("relative_saved_path"),TEXT("VisualOverhaul/")+ArtRun+TEXT("/Audio/")+AudioName+TEXT(".wav"));
  Receipt->SetStringField(TEXT("visual_evidence_status"),TEXT("CAPTURED NORMAL-PLAYER IMAGES REQUIRE EVIDENCE REVIEW"));
  const FString SourceSlot=Session==TEXT("A")?ArrivalSlot:StoreSlot;Receipt->SetStringField(TEXT("native_source_slot"),SourceSlot);Receipt->SetStringField(TEXT("arrival_primary_sha1"),ArrivalHash);
  if(!Save->ConfigureSlot(SourceSlot,false) || !Save->DoesSaveExist())return Fail(TEXT("Required native source checkpoint missing"));
  Expected.Reset(Save->ReadSaveForInspection());if(!Expected.IsValid() || !Expected->HasFishing)return Fail(TEXT("Native source checkpoint integrity read failed"));
  Receipt->SetStringField(TEXT("native_source_save_guid"),Expected->SaveID.ToString());Receipt->SetField(TEXT("saved_player_world_cm"),Vec(Expected->Player.Transform.GetLocation()));
  if(Session==TEXT("A"))
  {
   for(const auto& F:Expected->Fishing.Fish)
    if(F.Storage==ENeonCityFishStorage::Pouch && F.FishID.IsValid() && Inventory->FindSpecies(F.SpeciesID)
     && !F.FishingPointID.IsNone() && !F.FishingPointID.ToString().StartsWith(TEXT("Test")) && F.CaughtAt.GetTicks()>0)ActualFish.Add(F);
   if(ActualFish.Num()<2)return Fail(TEXT("Arrival checkpoint must contain at least two actual kept fish; this test does not create ingredients"));
   FirstFish=ActualFish[0].FishID;SecondFish=ActualFish[1].FishID;
  }
  else
  {
   FString Text;TSharedPtr<FJsonObject> A;
   if(!FFileHelper::LoadFileToString(Text,*(Directory/TEXT("SessionA.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),A)
    || A->GetStringField(TEXT("technical_status"))!=TEXT("PASS"))return Fail(TEXT("Successful paired process A receipt missing"));
   if(uint32(A->GetNumberField(TEXT("process_id")))==FPlatformProcess::GetCurrentProcessId())return Fail(TEXT("Store must be reopened in a different OS process"));
   if(A->GetStringField(TEXT("stored_save_guid"))!=Expected->SaveID.ToString() || A->GetStringField(TEXT("stored_primary_sha1"))!=SlotHash(StoreSlot))
    return Fail(TEXT("Process B did not reopen the exact integrity-checked Store generation"));
   if(!FGuid::Parse(A->GetStringField(TEXT("first_fish_guid")),FirstFish) || !FGuid::Parse(A->GetStringField(TEXT("second_fish_guid")),SecondFish)
    || !FGuid::Parse(A->GetStringField(TEXT("first_meal_guid")),FirstMeal))return Fail(TEXT("Paired original GUID provenance invalid"));
   Receipt->SetNumberField(TEXT("source_process_id"),A->GetNumberField(TEXT("process_id")));Receipt->SetArrayField(TEXT("original_actual_fish"),A->GetArrayField(TEXT("original_actual_fish")));
  }
  if(Session==TEXT("A")){TArray<TSharedPtr<FJsonValue>> F;for(const auto& Fish:ActualFish)F.Add(FishJSON(Fish));Receipt->SetArrayField(TEXT("original_actual_fish"),F);}
  if(!Save->LoadGame())return Fail(TEXT("Native source LoadGame failed: ")+Save->GetLastDetail());Event(TEXT("NATIVE_SOURCE_CHECKPOINT_LOADED"));return Next(1);
 }
 bool StartCookingOutputRecording()
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityIsolatedAutomation"))
   || !FParse::Param(FCommandLine::Get(),TEXT("unattended")) || !FApp::IsUnattended() || !FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")))
  {Test->AddError(TEXT("Master recording requires an explicitly isolated unattended RenderOffscreen audio-check process"));return false;}
  auto* Manager=GEngine?GEngine->GetAudioDeviceManager():nullptr;auto Device=W->GetAudioDevice();
  if(!Manager || !Device.IsValid() || !W->AllowAudioPlayback() || PreexistingAudioDevices.Contains(Device.GetDeviceID()))
  {Test->AddError(TEXT("Refusing to record a missing, silent, shared or pre-existing audio device"));return false;}
  const auto Worlds=Manager->GetWorldsUsingAudioDevice(Device.GetDeviceID());
  if(Worlds.Num()!=1 || Worlds[0]!=W)
  {Test->AddError(TEXT("PIE master device is not exclusively associated with the current normal PIE world"));return false;}
  auto* Mixer=FAudioDeviceManager::GetAudioMixerDeviceFromWorldContext(W);
  if(!Mixer || !Mixer->GetMasterSubmix().IsValid() || AudioRecording || AudioExportPending)
  {Test->AddError(TEXT("Current PIE mixer/master unavailable or this test already owns a recording"));return false;}
  if(!IFileManager::Get().MakeDirectory(*AudioDirectory,true))return false;
  RecordingWorld=W;RecordingDeviceID=Device.GetDeviceID();RecordingStartWall=FPlatformTime::Seconds();
  AudioReceipt->SetNumberField(TEXT("device_id"),RecordingDeviceID);AudioReceipt->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());
  AudioReceipt->SetNumberField(TEXT("recording_start_game_seconds"),W->GetTimeSeconds());AudioReceipt->SetNumberField(TEXT("recording_start_wall_seconds"),RecordingStartWall);
  UAudioMixerBlueprintLibrary::StartRecordingOutput(W,8.f,nullptr);AudioRecording=true;AudioReceipt->SetStringField(TEXT("export_status"),TEXT("RECORDING REQUESTED"));
  Event(TEXT("ACTUAL_PIE_MASTER_RECORDING_STARTED_BEFORE_NATIVE_COOK_INPUT"));return true;
 }
 bool StopCookingOutputRecording(bool Completed)
 {
  if(!AudioRecording)return AudioStopRequested;
  UWorld* OriginalWorld=RecordingWorld.Get();
  if(!OriginalWorld || !OriginalWorld->GetAudioDevice().IsValid() || OriginalWorld->GetAudioDevice().GetDeviceID()!=RecordingDeviceID)
  {AudioRecording=false;AudioReceipt->SetStringField(TEXT("export_status"),TEXT("FAILED: ORIGINAL PIE DEVICE LOST"));return false;}
  AudioRecording=false;AudioStopRequested=true;AudioExportPending=true;RecordingCompleted=Completed;AudioStopWall=FPlatformTime::Seconds();
  AudioReceipt->SetBoolField(TEXT("native_cooking_reached_ready"),Completed);AudioReceipt->SetNumberField(TEXT("recording_stop_game_seconds"),OriginalWorld->GetTimeSeconds());
  AudioReceipt->SetNumberField(TEXT("recording_wall_seconds"),AudioStopWall-RecordingStartWall);AudioReceipt->SetStringField(TEXT("export_status"),TEXT("WAV EXPORT REQUESTED; AWAITING REAL FILE"));
  // WavFile writes asynchronously and returns nullptr even on success. Only
  // a parsed non-silent PCM file below is accepted as exported evidence.
  UAudioMixerBlueprintLibrary::StopRecordingOutput(OriginalWorld,EAudioRecordingExportType::WavFile,AudioName,AudioDirectory,nullptr);
  Event(Completed?TEXT("ACTUAL_PIE_MASTER_RECORDING_STOPPED_AT_READY"):TEXT("PARTIAL_PIE_MASTER_RECORDING_STOPPED_ON_TEST_FAILURE"));return true;
 }
 bool CheckCookingOutputFile()
 {
  if(!AudioExportPending)return true;
  TArray<uint8> Bytes;FWaveModInfo Info;
  if(IFileManager::Get().FileSize(*AudioFile)<=44 || !FFileHelper::LoadFileToArray(Bytes,*AudioFile)
   || !Info.ReadWaveInfo(Bytes.GetData(),Bytes.Num()))return false;
  if(Info.SampleDataStart && (Info.SampleDataStart<Bytes.GetData() || Info.SampleDataStart>Bytes.GetData()+Bytes.Num()
   || int64(Info.SampleDataSize)>Bytes.Num()-(Info.SampleDataStart-Bytes.GetData())))return false;
  AudioExportPending=false;
  if(!Info.pBitsPerSample || *Info.pBitsPerSample!=16 || !Info.pChannels || !*Info.pChannels || !Info.pSamplesPerSec || !*Info.pSamplesPerSec || !Info.SampleDataStart)
  {AudioReceipt->SetStringField(TEXT("export_status"),TEXT("FAILED: INVALID PCM16 WAV"));Test->AddError(TEXT("Actual PIE BBQ recording is not valid PCM16 WAV"));return true;}
  const int32 Count=Info.SampleDataSize/sizeof(int16);double Energy=0,Peak=0;int32 Nonzero=0;
  for(int32 I=0;I<Count;++I)
  {int16 PCM=0;FMemory::Memcpy(&PCM,Info.SampleDataStart+I*sizeof(int16),sizeof(int16));const double V=double(PCM)/32768.;Energy+=V*V;Peak=FMath::Max(Peak,FMath::Abs(V));if(PCM)++Nonzero;}
  const double Duration=double(Count)/(double(*Info.pChannels)*double(*Info.pSamplesPerSec));const double RMS=Count?FMath::Sqrt(Energy/Count):0;
  const bool Audible=Count>0 && Nonzero>0 && RMS>1e-6 && Peak>1e-5 && (!RecordingCompleted || Duration>=1.5);
  AudioReceipt->SetNumberField(TEXT("file_bytes"),Bytes.Num());AudioReceipt->SetNumberField(TEXT("channels"),*Info.pChannels);AudioReceipt->SetNumberField(TEXT("sample_rate_hz"),*Info.pSamplesPerSec);
  AudioReceipt->SetNumberField(TEXT("pcm_duration_seconds"),Duration);AudioReceipt->SetNumberField(TEXT("pcm_samples"),Count);AudioReceipt->SetNumberField(TEXT("nonzero_samples"),Nonzero);
  AudioReceipt->SetNumberField(TEXT("pcm_rms"),RMS);AudioReceipt->SetNumberField(TEXT("pcm_peak"),Peak);AudioReceipt->SetBoolField(TEXT("real_non_silent_pcm_output"),Audible);
  AudioReceipt->SetStringField(TEXT("export_status"),Audible?(RecordingCompleted?TEXT("EXPORTED AND NON-SILENT"):TEXT("PARTIAL FAILURE RECORDING EXPORTED")):TEXT("FAILED: EMPTY OR SILENT OUTPUT"));
  Test->TestTrue(TEXT("Native current-PIE master WAV contains actual non-silent PCM output"),Audible);return true;
 }
 bool Next(int32 NewStage){Stage=NewStage;At=Now;return false;}
 bool Tap(FKey K,int32 NewStage){Key(PC,K,IE_Pressed);TappedKey=K;TappedAt=Now;TapNext=NewStage;return false;}
 bool HasMeal(FGuid ID)const{return Inventory && Inventory->GetMeals().ContainsByPredicate([&](const auto& M){return M.MealID==ID;});}
 UStaticMeshComponent* FindFoodVisual(bool VisibleOnly)const
 {
  if(!CurrentSpecies)return nullptr;auto* Mesh=CurrentSpecies->Mesh.Get();if(!Mesh)return nullptr;
  TArray<UStaticMeshComponent*> Components;Player->GetComponents(Components);
  for(auto* C:Components)if(C->GetStaticMesh()==Mesh && (!VisibleOnly || C->IsVisible())
   && FVector::Dist(C->GetComponentLocation(),Grill->GetActorTransform().TransformPosition(FVector(0,0,89)))<15)return C;
  return nullptr;
 }
 bool WalkPath()
 {
  if(Waypoint>=Path.Num())return true;
  const FVector Target=Path[Waypoint],P=Player->GetActorLocation();
  if(FVector::Dist2D(P,Target)<26)
  {
   Key(PC,EKeys::W,IE_Released);HeldW=false;LastWalkDone=Now;
   if(!Player->GetCharacterMovement()->IsMovingOnGround()){Fail(TEXT("Normal mansion/terrace walk lost floor contact"));return true;}
   ++Waypoint;WalkAt=Now;return Waypoint>=Path.Num();
  }
  if(!WalkAt)WalkAt=Now;
  if(Now-WalkAt>18)
  {
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(LandFinalBBQWalk),false,Player);
   W->SweepSingleByChannel(Hit,P,FVector(Target.X,Target.Y,P.Z),FQuat::Identity,ECC_Pawn,
    FCollisionShape::MakeCapsule(Player->GetCapsuleComponent()->GetScaledCapsuleRadius(),Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q);
   Test->AddError(FString::Printf(TEXT("Normal BBQ walk blocked at waypoint %d: %s / %s player %s target %s"),Waypoint,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*P.ToString(),*Target.ToString()));
   Key(PC,EKeys::W,IE_Released);HeldW=false;return true;
  }
  FRotator Direction=(Target-P).Rotation();Direction.Pitch=-5;Direction.Roll=0;PC->SetControlRotation(Direction);
  // Use the existing analog movement input function, just as an ordinary
  // controller does. Slow the approach according to the actual game step so
  // a long render frame cannot repeatedly leap across the unchanged26cm goal.
  // CharacterMovement still performs every floor/obstacle collision itself.
  const float Step=FMath::Clamp(W->GetDeltaSeconds(),1.f/120.f,.5f);
  const float Speed=Player->GetCharacterMovement()->GetMaxSpeed();
  const float ApproachDistance=FMath::Max(120.f,Speed*Step*3.f);
  LastWalkAnalog=FMath::Clamp(static_cast<float>(FVector::Dist2D(P,Target))/ApproachDistance,.08f,1.f);
  Player->DoMove(0,LastWalkAnalog);return false;
 }
 void Event(const FString& Name)
 {
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("event"),Name);J->SetNumberField(TEXT("stage"),Stage);
  J->SetNumberField(TEXT("game_seconds"),W?W->GetTimeSeconds():0);if(Player)J->SetField(TEXT("player_world_cm"),Vec(Player->GetActorLocation()));Events.Add(MakeShared<FJsonValueObject>(J));
 }
 void Shot(const TCHAR* Name)
 {
  const FString File=TEXT("BBQFinal15-")+Session+TEXT("-")+Name+TEXT(".png");const bool OK=FNeonCityVisualEvidence::CaptureHUD(W,File);
  Test->TestTrue(*FString::Printf(TEXT("Actual normal-player BBQ still: %s"),*File),OK);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("image"),File);
  FString ArtRun;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),ArtRun);
  J->SetStringField(TEXT("relative_saved_path"),TEXT("VisualOverhaul/")+FPaths::MakeValidFileName(ArtRun)+TEXT("/UI/")+File);
  J->SetBoolField(TEXT("fixed_camera"),false);J->SetBoolField(TEXT("simulation_paused"),false);J->SetBoolField(TEXT("captured"),OK);Shots.Add(MakeShared<FJsonValueObject>(J));
  LastReadback=true;
 }
 void SampleFrame()
 {
  if(Directory.IsEmpty() || LastFrame==GFrameCounter)return;LastFrame=GFrameCounter;
  const double Dt=FrameAt?(Now-FrameAt)*1000:0;FrameAt=Now;const float HeatValue=Coal?Coal->K2_GetScalarParameterValue(TEXT("Heat")):0;
  const FString Row=FString::Printf(TEXT("%llu,%.5f,%d,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d\n"),uint64(GFrameCounter),W->GetTimeSeconds(),Stage,Dt,
   FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),Player->GetHealthComponent()->GetCurrentHealth(),Player->GetPlayerDataComponent()->GetStamina(),
   Fishing?Fishing->GetCookingSeconds():0,HeatValue,Inventory?Inventory->GetFish().Num():0,Inventory?Inventory->GetMeals().Num():0,int32(LastReadback));
  const FString CSV=Directory/(TEXT("Session")+Session+TEXT("-frames.csv"));
  if(!CSVStarted){FFileHelper::SaveStringToFile(TEXT("engine_frame,game_seconds,stage,wall_ms,last_completed_gpu_ms,health,stamina,cooking_seconds,heat,raw_fish,meals,previous_update_synchronous_capture\n"),*CSV);CSVStarted=true;}
  FFileHelper::SaveStringToFile(Row,*CSV,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);LastReadback=false;
 }
 bool Fail(const FString& Reason){Test->AddError(TEXT("Actual BBQ final: ")+Reason);if(W && Player && !Directory.IsEmpty())Shot(TEXT("Failure"));return Finish();}
 bool Finish()
 {
  if(PC){Key(PC,EKeys::W,IE_Released);for(const FKey K:{EKeys::F,EKeys::Enter,EKeys::P,EKeys::N,EKeys::Right,EKeys::BackSpace})Key(PC,K,IE_Released);}
  if(AudioRecording && !StopCookingOutputRecording(false))Test->AddError(TEXT("Failed to finalize this test's original PIE recording"));
  if(!CheckCookingOutputFile())
  {
   if(FPlatformTime::Seconds()-AudioStopWall<20)return Next(90);
   AudioExportPending=false;AudioReceipt->SetStringField(TEXT("export_status"),TEXT("FAILED: WAV EXPORT TIMEOUT OR UNREADABLE FILE"));Test->AddError(TEXT("Actual PIE BBQ WAV did not become readable after export"));
  }
  Receipt->SetStringField(TEXT("technical_status"),Test->HasAnyErrors()?TEXT("FAIL"):TEXT("PASS"));Receipt->SetStringField(TEXT("first_fish_guid"),FirstFish.ToString());
  Receipt->SetStringField(TEXT("second_fish_guid"),SecondFish.ToString());Receipt->SetStringField(TEXT("first_meal_guid"),FirstMeal.ToString());Receipt->SetStringField(TEXT("second_meal_guid"),SecondMeal.ToString());
  Receipt->SetArrayField(TEXT("events"),Events);Receipt->SetArrayField(TEXT("cooking_transactions"),Cooking);Receipt->SetArrayField(TEXT("normal_player_screenshots"),Shots);
  Receipt->SetStringField(TEXT("frame_metrics_scope"),TEXT("Native automation/BBQ timing only; synchronous still frames marked; not comparable six-scene performance evidence"));
  Receipt->SetStringField(TEXT("walk_input_driver"),TEXT("Existing native DoMove analog input, approaching the unchanged26cm waypoint through CharacterMovement; no pose/velocity/handling writes"));
  if(!Directory.IsEmpty())
  {
   FString Text;FJsonSerializer::Serialize(Receipt.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));
   Test->TestTrue(TEXT("Paired BBQ provenance receipt saved"),FFileHelper::SaveStringToFile(Text,*(Directory/(TEXT("Session")+Session+TEXT(".json"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
  }
  return true;
 }
 FAutomationTestBase* Test;FString Session,Pair,Directory,ArrivalHash;TSharedPtr<FJsonObject> Receipt;TArray<TSharedPtr<FJsonValue>> Events,Cooking,Shots;
 TSharedPtr<FJsonObject> AudioReceipt;FString AudioDirectory,AudioName,AudioFile;TSet<uint32> PreexistingAudioDevices;TWeakObjectPtr<UWorld> RecordingWorld;
 uint32 RecordingDeviceID=0;double RecordingStartWall=0,AudioStopWall=0;bool AudioRecording=false,AudioStopRequested=false,AudioExportPending=false,RecordingCompleted=false;
 TStrongObjectPtr<UNeonCitySaveGame> Expected;TArray<FNeonCityCaughtFish> ActualFish,Pouch;TArray<FNeonCityFishMeal> MealsBefore;TArray<FVector> Path;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;
 UFishingComponent* Fishing=nullptr;UPhoneComponent* Phone=nullptr;AIslandLandGrill* Grill=nullptr;AIslandLandDoor* Door=nullptr;AIslandLandResidence* Residence=nullptr;
 UPointLightComponent* Heat=nullptr;UAudioComponent* Sizzle=nullptr;UMaterialInstanceDynamic* Coal=nullptr;const UFishData* CurrentSpecies=nullptr;UStaticMeshComponent* FoodVisual=nullptr;
 FTransform ResidenceBeforeLoad;FGuid FirstFish,SecondFish,WantedFish,FirstMeal,SecondMeal,CurrentMeal;FKey TappedKey;
 double Started=0,Now=0,At=0,TappedAt=0,WalkAt=0,LastWalkDone=0,FrameAt=0;uint64 LastFrame=0;
 float CookStartGame=0,FinishedGame=0,HeatMinimum=0,HeatMaximum=0,NeedHealth=0,NeedStamina=0,LastWalkAnalog=0;
 int32 Stage=0,TapNext=0,Waypoint=0,TargetIndex=0,SelectedIndex=0,MealTarget=0,MealSelected=0,FishCountBefore=0,MealsForEat=0,TabChanges=0,StowAttempts=0;
 bool HeldW=false,CSVStarted=false,LastReadback=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandBBQFinalCrossSessionPIE,"NeonCity.IslandLand.BBQFinalCrossSessionPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandBBQFinalCrossSessionPIE::RunTest(const FString&)
{
 FString Session,Pair;FParse::Value(FCommandLine::Get(),TEXT("NeonCityLandBBQSession="),Session);FParse::Value(FCommandLine::Get(),TEXT("NeonCityLandBBQPair="),Pair);
 if((Session!=TEXT("A") && Session!=TEXT("B")) || Pair.IsEmpty()){AddError(TEXT("Run paired dedicated processes with -NeonCityLandBBQSession=A/B and -NeonCityLandBBQPair=<unique>"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandBBQFinal::FBBQCrossSession(this,Session,Pair));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
