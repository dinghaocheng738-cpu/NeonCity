#include "Fishing/FishingComponent.h"
#include "Fishing/FishingStation.h"
#include "Fishing/FishingWidget.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/PlayerCombatAdapter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "World/NeonCityWorldManager.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Adventure/MarineEcologySubsystem.h"
#include "Animation/AnimSequence.h"
#include "Adventure/IslandLandGrill.h"
#include "Adventure/IslandLandAquarium.h"

UFishingComponent::UFishingComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f; }
void UFishingComponent::BeginPlay()
{ Super::BeginPlay();Random.Initialize(FMath::Rand());const auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->IsLocalController())SetComponentTickEnabled(false); }
ANeonCityPlayerCharacter* UFishingComponent::Player() const
{ const auto* PC=Cast<APlayerController>(GetOwner());return PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr; }
void UFishingComponent::SetupInput(UInputComponent* Input)
{
 Stop();RemoveInput();auto* PC=Cast<APlayerController>(GetOwner());auto* Enhanced=Cast<UEnhancedInputComponent>(Input);
 auto* Subsystem=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Enhanced || !Subsystem)return;BoundInput=Enhanced;InputSubsystem=Subsystem;Mapping=NewObject<UInputMappingContext>(this);Actions.Reset();
 const auto Bind=[&](FKey Key,void(UFishingComponent::*Method)())
 {
  auto* Action=NewObject<UInputAction>(this);Action->ValueType=EInputActionValueType::Boolean;Action->bConsumeInput=true;
  Actions.Add(Action);Mapping->MapKey(Action,Key);Bindings.Add(Enhanced->BindAction(Action,ETriggerEvent::Started,this,Method).GetHandle());return Action;
 };
 auto* Trigger=Bind(EKeys::LeftMouseButton,&UFishingComponent::Press);
 Bindings.Add(Enhanced->BindAction(Trigger,ETriggerEvent::Completed,this,&UFishingComponent::Release).GetHandle());
 Bindings.Add(Enhanced->BindAction(Trigger,ETriggerEvent::Canceled,this,&UFishingComponent::Release).GetHandle());
 Bind(EKeys::F,&UFishingComponent::Stop);Bind(EKeys::P,&UFishingComponent::OpenPhone);
 Bind(EKeys::One,&UFishingComponent::Keep);Bind(EKeys::Two,&UFishingComponent::ReleaseCatch);
 Bind(EKeys::Enter,&UFishingComponent::MenuAction);Bind(EKeys::Right,&UFishingComponent::NextItem);Bind(EKeys::Left,&UFishingComponent::PreviousItem);
 Bind(EKeys::Escape,&UFishingComponent::Stop);
 for(const FKey Key:{EKeys::RightMouseButton,EKeys::R,EKeys::Tab,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::SpaceBar})Bind(Key,&UFishingComponent::ConsumeInput);
}
void UFishingComponent::RemoveInput()
{
 if(BoundInput.IsValid())for(uint32 Handle:Bindings)BoundInput->RemoveBindingByHandle(Handle);
 if(InputSubsystem.IsValid() && Mapping)InputSubsystem->RemoveMappingContext(Mapping);
 Bindings.Reset();BoundInput.Reset();InputSubsystem.Reset();
}
bool UFishingComponent::CanStart(const AFishingStation* Point) const
{
 auto* P=Player();auto* PC=Cast<APlayerController>(GetOwner());
 if(IsActive() || !PC || !PC->IsLocalController() || !P || !P->HasAuthority() || !IsValid(Point)
  || !Mapping || !InputSubsystem.IsValid() || !Point->IsInReach(P) || !P->GetCharacterMovement()->IsMovingOnGround()
  || P->GetCharacterMovement()->Velocity.Size2D()>80 || P->GetHealthComponent()->IsDead())return false;
 if(const auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>();Body && !Body->CanAct())return false;
 if(const auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())return false;
 if(P->GetCombatAdapter()->IsAiming() || P->GetCombatAdapter()->IsStoreOpen())return false;
 if(const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();Phone && Phone->IsPhoneOpen())return false;
 if(!GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->IsRegisteredStation(Point))return false;
 if(Point->Kind==ENeonCityFishingStation::CastingPoint)
 {
  FVector Target;if(!Point->ResolveWaterTarget(Target) || GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->GetSpecies().IsEmpty())return false;
 }
 return true;
}
bool UFishingComponent::Start(AFishingStation* Point)
{
 if(!CanStart(Point) || !Point->IsFacingWater(Player()))return false;
 ActiveStation=Point;ActivePlayer=Player();StartHealth=ActivePlayer->GetHealthComponent()->GetCurrentHealth();SelectedItem=0;
 Notice.Reset();ResetCast();auto* PC=CastChecked<APlayerController>(GetOwner());PC->SetIgnoreMoveInput(true);bIgnoredMove=true;
 FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;InputSubsystem->AddMappingContext(Mapping,240,Options);
 PrimaryComponentTick.TickInterval=0;
 AddTickPrerequisiteComponent(ActivePlayer->GetCharacterMovement());ActivePlayer->GetMesh()->AddTickPrerequisiteComponent(this);
 if(Point->Kind!=ENeonCityFishingStation::FishMarket)CreateVisuals();
 if(Point->Kind==ENeonCityFishingStation::CastingPoint)
 {
  if(!Rod || !Float || Line.Num()!=12 || ReelParts.Num()!=5 || ReelParts.Contains(nullptr)) { Stop();return false; }
  if(!ActivePlayer->GetCameraExtensionComponent()->BeginFishing(ActivePlayer->GetFollowCamera(),ActivePlayer->GetCameraBoom())) { Stop();return false; }
  Point->ResolveWaterTarget(WaterTarget);
  PC->SetControlRotation(FRotator(-12,(WaterTarget-ActivePlayer->GetActorLocation()).Rotation().Yaw,0));
 }
 UpdateWidget();return true;
}
void UFishingComponent::Stop()
{
 if(auto* Grill=Cast<AIslandLandGrill>(ActiveStation.Get()))Grill->LeaveCooking(ActivePlayer.Get());
 if(auto* Ecology=GetWorld()?GetWorld()->GetSubsystem<UMarineEcologySubsystem>():nullptr)Ecology->ReleaseFishingEncounter(PendingCatch.FishID);
 if(bIgnoredMove)if(auto* PC=Cast<APlayerController>(GetOwner()))PC->SetIgnoreMoveInput(false);
 bIgnoredMove=false;
 if(InputSubsystem.IsValid() && Mapping) { FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;InputSubsystem->RemoveMappingContext(Mapping,Options); }
 if(ActivePlayer.IsValid()) { ActivePlayer->GetCameraExtensionComponent()->EndFishing();RemoveTickPrerequisiteComponent(ActivePlayer->GetCharacterMovement());ActivePlayer->GetMesh()->RemoveTickPrerequisiteComponent(this); }
 Session.Cancel();ActiveStation.Reset();ActivePlayer.Reset();PendingCatch=FNeonCityCaughtFish();CookingFish.Invalidate();CookSeconds=0;
 DestroyVisuals();PrimaryComponentTick.TickInterval=.1f;if(Widget)Widget->RemoveFromParent();
}
void UFishingComponent::OpenPhone()
{ Stop();if(auto* Phone=GetOwner()->FindComponentByClass<UPhoneComponent>();Phone && !Phone->IsPhoneOpen())Phone->TogglePhone(); }
void UFishingComponent::ResetCast()
{
 GetWorld()->GetSubsystem<UMarineEcologySubsystem>()->ReleaseFishingEncounter(PendingCatch.FishID);
 Session.Reset(Random.RandRange(1,MAX_int32),1);PendingCatch=FNeonCityCaughtFish();RuleAccumulator=0;VisualClock=0;EncounterSearchClock=0;
 if(WindupMotion)Session.WindupDuration=WindupMotion->GetPlayLength();
}
bool UFishingComponent::FindEncounter()
{
 auto* Ecology=GetWorld()->GetSubsystem<UMarineEcologySubsystem>();
 const float Hour=GetWorld()->GetSubsystem<UNeonCityWorldManager>()->BuildPersistenceSnapshot().TimeOfDay;
 float Delay;
 if(!Ecology->ReserveFishingEncounter(WaterTarget,Hour,bDeepRig,Random,PendingCatch,BaitTarget,Delay,HabitatDescription))return false;
 const auto* Profile=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->FindSpecies(PendingCatch.SpeciesID);
 if(!Profile){Ecology->ReleaseFishingEncounter(PendingCatch.FishID);PendingCatch=FNeonCityCaughtFish();return false;}
 FNeonCityFishFightConfig Config;Config.WeightKg=PendingCatch.WeightKg;Config.SpeciesMaximumKg=Profile->WeightKg.Y;
 Config.SpeciesFight=Profile->FightStrength;Config.SpeciesStaminaSeconds=Profile->FightStaminaSeconds;
 Config.LineRatingKg=bDeepRig?26:7;Config.ReelSpeedMps=bDeepRig?1.6:1.1;
 Config.DistanceM=FMath::Max(3.,FVector::Dist(Player()->GetActorLocation(),BaitTarget)/100.);
 if(!Session.SetEncounter(Random.RandRange(1,MAX_int32),Config,Delay))
 {Ecology->ReleaseFishingEncounter(PendingCatch.FishID);PendingCatch=FNeonCityCaughtFish();return false;}
 Ecology->FishingEncounterPosition(PendingCatch.FishID,HookPosition);return true;
}
void UFishingComponent::UpdateEncounter(float Delta)
{
 auto* Ecology=GetWorld()->GetSubsystem<UMarineEcologySubsystem>();
 if(Session.State==ENeonCityFishingState::Escaped || Session.State==ENeonCityFishingState::Cancelled)
 {Ecology->ReleaseFishingEncounter(PendingCatch.FishID);PendingCatch=FNeonCityCaughtFish();return;}
 if(!PendingCatch.FishID.IsValid())
 {
  if(Session.State==ENeonCityFishingState::Waiting)
  { EncounterSearchClock+=Delta;if(EncounterSearchClock>=3){EncounterSearchClock=0;FindEncounter();} }
  return;
 }
 FVector At;
 if(!Ecology->FishingEncounterPosition(PendingCatch.FishID,At))
 {Session.Cancel();Ecology->ReleaseFishingEncounter(PendingCatch.FishID);PendingCatch=FNeonCityCaughtFish();Notice=TEXT("The fish has moved away. Cast again.");return;}
 const bool Fighting=Session.State==ENeonCityFishingState::Fighting,Caught=Session.State==ENeonCityFishingState::Caught;
 if(!Fighting && !Caught)
 {
  if(Session.State==ENeonCityFishingState::Waiting && Session.Elapsed>90)
  {Ecology->ReleaseFishingEncounter(PendingCatch.FishID);PendingCatch=FNeonCityCaughtFish();Session.Cancel();Notice=TEXT("The fish lost interest. Try another cast or depth rig.");return;}
  Ecology->MoveFishingEncounter(PendingCatch.FishID,BaitTarget,false,false);
  if(Session.State==ENeonCityFishingState::Waiting && FVector::DistSquared(At,BaitTarget)>FMath::Square(150.f))
   Session.WaitSeconds=FMath::Max(Session.WaitSeconds,Session.Elapsed+.15f);
  HookPosition=At;return;
 }
 if(Fighting)
 {
  const FVector Origin=Player()->GetActorLocation();const FVector Heading=(BaitTarget-Origin).GetSafeNormal2D();
  const FVector Side=FVector::CrossProduct(FVector::UpVector,Heading);
  FVector Desired=Origin+Heading*Session.RemainingLineM*100+Side*FMath::Sin(VisualClock*1.7f)*Session.Surge*70;
  Desired.Z=FMath::Lerp(BaitTarget.Z,WaterTarget.Z-45,FMath::Clamp(Session.Progress,0.f,1.f));
  HookPosition=FMath::VInterpConstantTo(At,Desired,Delta,320);
  FHitResult Barrier;
  if(GetWorld()->LineTraceSingleByObjectType(Barrier,At,HookPosition,FCollisionObjectQueryParams(ECC_WorldStatic)))HookPosition=Barrier.ImpactPoint+Barrier.ImpactNormal*30;
 }
 Ecology->MoveFishingEncounter(PendingCatch.FishID,HookPosition,Fighting,Caught);
}
void UFishingComponent::Press()
{
 if(!IsActive() || ActiveStation->Kind!=ENeonCityFishingStation::CastingPoint)return;
 if(Session.State==ENeonCityFishingState::Escaped || Session.State==ENeonCityFishingState::Cancelled) { Notice.Reset();ResetCast(); }
 if(Session.State==ENeonCityFishingState::Ready)
 {
  auto* Inventory=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  if(!Inventory->HasPouchSpace()) { Notice=TEXT("Your fish pouch is full. Visit the fish market or release a catch.");return; }
  if(!ActiveStation->ResolveWaterTarget(WaterTarget)) { Stop();return; }
  Notice.Reset();Session.WaitSeconds=MAX_flt;
 }
 Session.Press();UpdateWidget();
}
void UFishingComponent::Release()
{
 const bool Cast=Session.State==ENeonCityFishingState::Windup;
 const bool AnimationReady=GetCastMotionCurve(TEXT("CastReady"))>=.99f;
 const float ReleaseMarker=GetCastMotionCurve(TEXT("ReleaseSeconds"));
 if(Cast)Session.CastPower=FMath::Clamp(GetCastPoseTime()/Session.WindupDuration,0.f,1.f);
 Session.Release(AnimationReady);if(!Cast || !IsActive())return;RuleAccumulator=0;
 Session.LaunchDelay=Session.BackSwingRemaining+FMath::Max(.1f,ReleaseMarker);
 FVector Nominal;if(!ActiveStation->ResolveWaterTarget(Nominal)){Stop();return;}
 const FVector Origin=ActivePlayer->GetActorLocation();
 const FVector Heading=(Nominal-Origin).GetSafeNormal2D();
 const float Reach=FVector::Dist2D(Origin,Nominal)*(.55f+.45f*Session.CastPower)*(Session.PerfectCast?1.12f:1.f);
 const FVector Right=FVector::CrossProduct(FVector::UpVector,Heading);
 FVector Desired=Origin+Heading*Reach+Right*Random.FRandRange(-1,1)*(Session.PerfectCast?6.f:45.f*(1-Session.CastPower));
 FWaterSurfaceSample Surface;FHitResult Barrier;FCollisionQueryParams Query(SCENE_QUERY_STAT(FishingCastLanding),false,ActivePlayer.Get());
 const bool Open=GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Desired,Surface)
  && !GetWorld()->LineTraceSingleByChannel(Barrier,Surface.SurfacePoint+FVector(0,0,300),Surface.SurfacePoint-FVector(0,0,30),ECC_WorldStatic,Query);
 WaterTarget=Open?Surface.SurfacePoint+Surface.Normal*3:Nominal;
 bCastLaunched=false;
 Notice=Session.PerfectCast?TEXT("Perfect release"):Session.CastPower<.5f?TEXT("Short cast"):TEXT("Good cast");
 // Reserve a fish at the actual landing habitat, after release determines it.
 if(!FindEncounter())Notice+=TEXT(" · Quiet water");UpdateWidget();
}
void UFishingComponent::Keep()
{
 if(auto* Grill=Cast<AIslandLandGrill>(ActiveStation.Get()))
 {Notice=Grill->StorePlatedMeal()?TEXT("Grilled fish stored in your food inventory."):TEXT("Wait until the fish is plated.");UpdateWidget();return;}
 if(!IsActive() || Session.State!=ENeonCityFishingState::Caught)return;
 if(GetWorld()->GetSubsystem<UMarineEcologySubsystem>()->KeepFishingEncounter(PendingCatch.FishID,
   GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>(),PendingCatch))
 { ++KeptCatchCount;ResetCast();Notice=TEXT("Catch kept. Your fish pouch is saved with your game."); }
 else Notice=TEXT("No room for this catch. Press 2 to release it.");
 UpdateWidget();
}
void UFishingComponent::ReleaseCatch()
{
 if(auto* Grill=Cast<AIslandLandGrill>(ActiveStation.Get()))
 {Notice=Grill->EatPlatedMeal(ActivePlayer.Get())?TEXT("Meal eaten."):TEXT("Wait until the fish is plated, or store it if you are full.");UpdateWidget();return;}
 if(IsActive() && Session.State==ENeonCityFishingState::Caught) { ResetCast();Notice=TEXT("Catch released back to the sea.");UpdateWidget(); }
}
TArray<FNeonCityCaughtFish> UFishingComponent::Pouch() const
{
 TArray<FNeonCityCaughtFish> Result;
 const auto* Aquarium=Cast<AIslandLandAquarium>(ActiveStation.Get());
 for(const auto& Record:GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->GetFish())
  if(Record.Storage==ENeonCityFishStorage::Pouch || (Aquarium && Record.Storage==ENeonCityFishStorage::Aquarium && Record.ContainerID==Aquarium->ContainerID))Result.Add(Record);
 return Result;
}
void UFishingComponent::NextItem()
{ if(IsActive() && ActiveStation->Kind==ENeonCityFishingStation::CastingPoint)
  {
   if(Session.State==ENeonCityFishingState::Escaped || Session.State==ENeonCityFishingState::Cancelled)ResetCast();
   if(Session.State==ENeonCityFishingState::Ready){bDeepRig=!bDeepRig;Notice.Reset();UpdateWidget();}return;
  }
 if(IsActive() && !CookingFish.IsValid()) { const int32 Count=Pouch().Num();if(Count)SelectedItem=(SelectedItem+1)%Count;Notice.Reset();UpdateWidget(); } }
void UFishingComponent::PreviousItem()
{ if(IsActive() && ActiveStation->Kind==ENeonCityFishingStation::CastingPoint){NextItem();return;}
 if(IsActive() && !CookingFish.IsValid()) { const int32 Count=Pouch().Num();if(Count)SelectedItem=(SelectedItem+Count-1)%Count;Notice.Reset();UpdateWidget(); } }
void UFishingComponent::MenuAction()
{
 if(auto* Aquarium=Cast<AIslandLandAquarium>(ActiveStation.Get()))
 {
  const auto Items=Pouch();if(Items.IsEmpty())return;SelectedItem=FMath::Clamp(SelectedItem,0,Items.Num()-1);
  const bool WasPouch=Items[SelectedItem].Storage==ENeonCityFishStorage::Pouch;
  Notice=Aquarium->TransferFish(Items[SelectedItem].FishID)?(WasPouch?TEXT("Living fish moved into the marine tank."):TEXT("The same fish returned to your pouch.")):
   TEXT("This tank accepts living saltwater aquarium species up to 60 cm. Make room before removing a fish.");UpdateWidget();return;
 }
 if(auto* Grill=Cast<AIslandLandGrill>(ActiveStation.Get()))
 {
  const auto Items=Pouch();if(Items.IsEmpty()){Notice=TEXT("No catch in your pouch. Catch and keep a fish first.");UpdateWidget();return;}
  SelectedItem=FMath::Clamp(SelectedItem,0,Items.Num()-1);
  Notice=Grill->BeginCook(Items[SelectedItem].FishID,ActivePlayer.Get())?TEXT(""):TEXT("Stand close to the front of the grill, and wait for the previous preparation to finish.");
  UpdateWidget();return;
 }
 if(!IsActive() || ActiveStation->Kind==ENeonCityFishingStation::CastingPoint || CookingFish.IsValid())return;
 const auto Items=Pouch();if(Items.IsEmpty())return;SelectedItem=FMath::Clamp(SelectedItem,0,Items.Num()-1);
 auto* Inventory=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();const auto Record=Items[SelectedItem];
 if(ActiveStation->Kind==ENeonCityFishingStation::FishMarket)
 {
  const int64 Price=Inventory->GetSaleValue(Record);
  Notice=Inventory->SellFish(Record.FishID)?FString::Printf(TEXT("Sold for $%lld cash."),Price):TEXT("Sale unavailable. Your catch is still in your pouch.");
 }
 else { CookingFish=Record.FishID;CookSeconds=3;Notice.Reset(); }
 UpdateWidget();
}
void UFishingComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Tick,Function);
 if(!IsActive())
 {
  if(bIgnoredMove)Stop();auto* P=Player();FocusedStation=P?Cast<AFishingStation>(P->GetInteractionComponent()->GetFocusedInteractable()):nullptr;
  UpdateWidget();return;
 }
 RuleAccumulator=FMath::Min(RuleAccumulator+Delta,.2f);
 if(RuleAccumulator>=.05f)
 {
  auto* P=Player();auto* Phone=GetOwner()->FindComponentByClass<UPhoneComponent>();
  const auto* Body=P?P->FindComponentByClass<UDamageableCharacterComponent>():nullptr;
  if(!P || P!=ActivePlayer.Get() || (Body && !Body->CanAct()) || !ActiveStation->IsInReach(P) || !P->GetCharacterMovement()->IsMovingOnGround()
   || P->GetHealthComponent()->GetCurrentHealth()<StartHealth || P->GetHealthComponent()->IsDead() || (Phone && Phone->IsPhoneOpen())) { Stop();return; }
  if(ActiveStation->Kind==ENeonCityFishingStation::CastingPoint)
  {
   if(Session.State==ENeonCityFishingState::Ready || Session.State==ENeonCityFishingState::Windup)
   { FVector Surface;if(!ActiveStation->ResolveWaterTarget(Surface)) { Stop();return; }WaterTarget=Surface; }
   else
   {
    // The float stays at its cast position on the water while a boat drifts underneath the angler.
    FWaterSurfaceSample Surface;
    if(!GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(WaterTarget,Surface)) { Stop();return; }
    WaterTarget=Surface.SurfacePoint+Surface.Normal*3;
    if(FVector::Dist2D(WaterTarget,P->GetActorLocation())>6000) { Stop();return; }
   }
  }
  const auto Before=Session.State;
  while(RuleAccumulator>=.05f)
  {
   if(ActiveStation->Kind==ENeonCityFishingStation::CastingPoint)UpdateEncounter(.05f);
   Session.Step(.05f);RuleAccumulator-=.05f;
   if(CookingFish.IsValid() && !Cast<AIslandLandGrill>(ActiveStation.Get()))
   {
    CookSeconds-=.05f;if(CookSeconds<=0)
    {
     const bool Success=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->CookFish(CookingFish);
     Notice=Success?TEXT("Grilled fish ready. Eat it from your phone's fish inventory."):TEXT("Cooking could not finish. Your catch was not consumed.");CookingFish.Invalidate();
    }
   }
  }
  if(Before!=ENeonCityFishingState::Caught && Session.State==ENeonCityFishingState::Caught)
  {PendingCatch.CaughtAt=FDateTime::UtcNow();UpdateEncounter(.05f);}
  UpdateWidget();
 }
 ActivePlayer->GetCameraExtensionComponent()->ApplyFishingCamera(Delta);
 VisualClock+=FMath::Clamp(Delta,0.f,.1f);UpdateVisuals(Delta);
 if(Session.State==ENeonCityFishingState::Windup)UpdateWidget();
}
void UFishingComponent::UpdateWidget()
{
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->IsLocalController())return;
 const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();
 if((!IsActive() && !FocusedStation.IsValid()) || (Phone && Phone->IsPhoneOpen())) { if(Widget)Widget->RemoveFromParent();return; }
 if(!Widget)Widget=CreateWidget<UFishingWidget>(PC,UFishingWidget::StaticClass());if(!Widget)return;
 if(!Widget->IsInViewport())Widget->AddToViewport(15);Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
 Widget->ShowState(GetDisplayText(),IsActive() && Session.State==ENeonCityFishingState::Fighting,Session.Tension,Session.Progress,
  IsActive() && Session.State==ENeonCityFishingState::Windup,GetCastPoseTime()/Session.WindupDuration);
}
FText UFishingComponent::GetDisplayText() const
{
 if(!IsActive())
 {
  if(!FocusedStation.IsValid())return FText::GetEmpty();
  if(!FocusedStation->IsFacingWater(Player()))return NSLOCTEXT("NeonCity","FishFaceWater","Walk toward the sea beside this sign,\nthen press F to fish.");
  return FText::FromString(FString::Printf(TEXT("%s\nF  %s"),*FocusedStation->DisplayName.ToString(),*FocusedStation->GetInteractionName_Implementation().ToString()));
 }
 const auto* Inventory=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
 FString Text=ActiveStation->DisplayName.ToString()+TEXT("\n\n");
 if(ActiveStation->Kind==ENeonCityFishingStation::CastingPoint)
 {
  Text+=bDeepRig?TEXT("Deep rig / 26 kg line\n"):TEXT("Shallow rig / 7 kg line\n");
  if(!HabitatDescription.IsEmpty())Text+=HabitatDescription+TEXT("\n\n");
  switch(Session.State)
  {
  case ENeonCityFishingState::Ready:Text+=TEXT("LEFT / RIGHT  Depth rig\nHold LMB to wind up · Release to cast\n");break;
  case ENeonCityFishingState::Windup:Text+=TEXT("Release in the gold window\n");break;
  case ENeonCityFishingState::Casting:Text+=TEXT("Casting...\n");break;
  case ENeonCityFishingState::Waiting:Text+=TEXT("Waiting for a bite...\nRelease the mouse. Watch the float.\n");break;
  case ENeonCityFishingState::Bite:Text+=TEXT("BITE!\nLEFT MOUSE  Hook now\n");break;
  case ENeonCityFishingState::Fighting:
   Text+=Session.Tension>.75f?TEXT("High tension! Give it some line.\n"):Session.Tension<.3f?TEXT("Line is slack. Keep reeling.\n"):TEXT("Steady tension. Reel it closer.\n");
   Text+=FString::Printf(TEXT("Hold LEFT MOUSE to reel.\nRelease to reduce tension.\nLine %.1f m  |  Fish stamina %.0f%%\n"),Session.RemainingLineM,Session.FishStamina*100);break;
  case ENeonCityFishingState::Caught:
   if(const auto* Species=Inventory->FindSpecies(PendingCatch.SpeciesID))Text+=FString::Printf(TEXT("CAUGHT  %s\n%.2f kg  |  %.1f cm  |  %s\n1  Keep    2  Release\n"),*Species->Name.ToString(),PendingCatch.WeightKg,PendingCatch.SizeCm,
    Species->Rarity==ENeonCityFishRarity::Rare?TEXT("Rare"):Species->Rarity==ENeonCityFishRarity::Uncommon?TEXT("Uncommon"):TEXT("Common"));break;
  case ENeonCityFishingState::Escaped:
   Text+=Session.Failure==ENeonCityFishingFailure::EarlyHook?TEXT("Too soon. There was no bite.\n"):(Session.Failure==ENeonCityFishingFailure::MissedBite?TEXT("Missed the bite. The fish got away.\n"):(Session.Failure==ENeonCityFishingFailure::BrokenLine?TEXT("Too much tension. The line broke.\n"):TEXT("The fish slipped away.\n")));
   Text+=TEXT("LEFT MOUSE  Cast again\n");break;
  case ENeonCityFishingState::Cancelled:Text+=TEXT("The fish moved away. LEFT MOUSE to cast again.\n");break;
  default:break;
  }
  Text+=FString::Printf(TEXT("\nFish pouch  %d / 64\n"),Inventory->PouchCount());
  Text+=Session.State==ENeonCityFishingState::Caught?TEXT("F  Release and leave"):TEXT("F  Leave    P  Phone");
 }
 else
 {
  if(const auto* Grill=Cast<AIslandLandGrill>(ActiveStation.Get());Grill && Grill->GetCookingStage()!=ENeonCityGrillStage::Idle)
  {Text+=Grill->GetCookingInstructions().ToString()+TEXT("\n\nF Leave · unfinished catch returns to your pouch\n");if(!Notice.IsEmpty())Text+=TEXT("\n")+Notice;return FText::FromString(Text);}
  const auto Items=Pouch();
  if(Items.IsEmpty())Text+=TEXT("No fish in your pouch.\nCatch and keep a fish at the coast.\n");
  else
  {
   const int32 Index=FMath::Clamp(SelectedItem,0,Items.Num()-1);const auto& Record=Items[Index];
   if(const auto* Species=Inventory->FindSpecies(Record.SpeciesID))Text+=FString::Printf(TEXT("%d / %d  %s\n%.2f kg  |  %.1f cm\n"),Index+1,Items.Num(),*Species->Name.ToString(),Record.WeightKg,Record.SizeCm);
   if(CookingFish.IsValid())Text+=FString::Printf(TEXT("Grilling... %.1f seconds\nStay at the cooking point.\n"),FMath::Max(0.f,CookSeconds));
   else
   {
    Text+=ActiveStation->Kind==ENeonCityFishingStation::Aquarium?(Record.Storage==ENeonCityFishStorage::Aquarium?TEXT("ENTER Return this living fish to pouch\n"):TEXT("ENTER Place this living fish in aquarium\n")):
     ActiveStation->Kind==ENeonCityFishingStation::FishMarket?FString::Printf(TEXT("ENTER  Sell for $%lld\n"),Inventory->GetSaleValue(Record)):TEXT("ENTER  Grill this fish\n");
    Text+=TEXT("LEFT / RIGHT  Choose catch\n");
   }
  }
  Text+=TEXT("\nF  Leave    P  Phone");
 }
 if(!Notice.IsEmpty())Text+=TEXT("\n\n")+Notice;return FText::FromString(Text);
}
void UFishingComponent::EndPlay(EEndPlayReason::Type Reason)
{ Stop();RemoveInput();Super::EndPlay(Reason); }
