#include "Player/PlayerCombatAdapter.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/WingsuitComponent.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Player/CombatFeedbackWidget.h"
#include "Phone/PhoneComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/WeaponStoreActor.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "TimerManager.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Combat/Weapons/WeaponData.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/HealthComponent.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
UPlayerCombatAdapter::UPlayerCombatAdapter() { PrimaryComponentTick.bCanEverTick=true; }
ANeonCityPlayerCharacter* UPlayerCombatAdapter::Player() const { return Cast<ANeonCityPlayerCharacter>(GetOwner()); }
bool UPlayerCombatAdapter::CanUse() const
{
	auto* P=Player(); auto* PC=P ? Cast<APlayerController>(P->GetController()) : nullptr;
	if (ActiveStore.IsValid() || (P && P->GetCharacterMovement()->IsSwimming())) return false;
	if (!PC || !PC->IsLocalController() || PC->GetPawn()!=P || P->GetHealthComponent()->IsDead()) { return false; }
 if(const auto* Dive=P->FindComponentByClass<UDivingComponent>();Dive && (Dive->IsEquipped() || Dive->IsChangingGear()))return false;
 if(const auto* Flight=P->FindComponentByClass<UWingsuitComponent>();Flight && Flight->IsEquipped())return false;
	if (auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { return false; }
	if (auto* Phone=PC->FindComponentByClass<UPhoneComponent>(); Phone && Phone->IsPhoneOpen()) { return false; }
	return true;
}
void UPlayerCombatAdapter::SetupInput(UInputComponent* Input)
{
	RemoveInput();
	auto* P=Player(); auto* PC=P ? Cast<APlayerController>(P->GetController()) : nullptr;
	auto* Enhanced=Cast<UEnhancedInputComponent>(Input);
	auto* Subsystem=PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Enhanced || !Subsystem) { return; }
	Mapping=NewObject<UInputMappingContext>(this); Actions.Reset();
	for (FKey Key:{EKeys::One,EKeys::RightMouseButton,EKeys::LeftMouseButton,EKeys::R,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Escape})
	{
		auto* Action=NewObject<UInputAction>(this); Action->ValueType=EInputActionValueType::Boolean;
		Actions.Add(Action); Mapping->MapKey(Action,Key);
	}
	Bindings.Add(Enhanced->BindAction(Actions[0],ETriggerEvent::Started,this,&UPlayerCombatAdapter::ToggleEquip).GetHandle());
	Bindings.Add(Enhanced->BindAction(Actions[1],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Aim).GetHandle());
	Bindings.Add(Enhanced->BindAction(Actions[1],ETriggerEvent::Completed,this,&UPlayerCombatAdapter::StopAim).GetHandle());
	Bindings.Add(Enhanced->BindAction(Actions[1],ETriggerEvent::Canceled,this,&UPlayerCombatAdapter::StopAim).GetHandle());
	Bindings.Add(Enhanced->BindAction(Actions[2],ETriggerEvent::Started,this,&UPlayerCombatAdapter::StartFire).GetHandle());
	Bindings.Add(Enhanced->BindAction(Actions[3],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Reload).GetHandle());
	Bindings.Add(Enhanced->BindAction(Actions[2],ETriggerEvent::Completed,this,&UPlayerCombatAdapter::StopFire).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[2],ETriggerEvent::Canceled,this,&UPlayerCombatAdapter::StopFire).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[4],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Slot2).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[5],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Slot3).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[6],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Slot4).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[7],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Ammo1).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[8],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Ammo2).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[9],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Ammo3).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[10],ETriggerEvent::Started,this,&UPlayerCombatAdapter::Ammo4).GetHandle());
 Bindings.Add(Enhanced->BindAction(Actions[11],ETriggerEvent::Started,this,&UPlayerCombatAdapter::CloseStore).GetHandle());
 FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease=true;
	Subsystem->AddMappingContext(Mapping,5,Options); InputSubsystem=Subsystem; BoundInput=Enhanced;
}
void UPlayerCombatAdapter::RemoveInput()
{
	if (BoundInput.IsValid()) { for (uint32 Handle:Bindings) { BoundInput->RemoveBindingByHandle(Handle); } }
	if (InputSubsystem.IsValid() && Mapping) { InputSubsystem->RemoveMappingContext(Mapping); }
	Bindings.Reset(); BoundInput.Reset(); InputSubsystem.Reset();
}
void UPlayerCombatAdapter::ToggleEquip() { SelectSlot(1); }
void UPlayerCombatAdapter::SelectSlot(int32 Slot)
{
 if(ActiveStore.IsValid()) { ShopResult=ActiveStore->Buy(Player(),Slot,false)?TEXT("Purchase complete"):TEXT("Purchase rejected: owned / insufficient cash / unavailable"); return; }
 if(!CanUse()) return;
 auto* P=Player(); auto* Inventory=P->FindComponentByClass<UWeaponInventoryComponent>();
 if(!Inventory || !Inventory->Owns(NeonCityWeapons::Slot(Slot))) return;
 if(auto* Light=P->FindComponentByClass<UNeonCityFlashlightComponent>())Light->SetEquipped(false);
 StopFire(); StopAim(); auto* Combat=P->GetCombatComponent();
 if(auto* W=Combat->GetWeapon(); W && W->IsEquipped() && W->GetData()->SlotType==Slot)
 { Combat->UnequipWeapon(); if(Feedback) Feedback->RemoveFromParent(); return; }
 auto* Presentation=P->FindComponentByClass<UCharacterWeaponPresentationComponent>();
 if(Presentation && Inventory->EquipSlot(Slot,Presentation->GetGrip(),NAME_None)) AttachWeaponFeedback();
}
void UPlayerCombatAdapter::BuyAmmo(int32 Slot)
{ if(ActiveStore.IsValid()) ShopResult=ActiveStore->Buy(Player(),Slot,true)?TEXT("Ammo purchased"):TEXT("Ammo rejected: need weapon / cash / reserve capacity"); }
void UPlayerCombatAdapter::OpenStore(AWeaponStoreActor* Store)
{
 if(ActiveStore.Get()==Store) { CloseStore(); return; }
 if(!CanUse() || !IsValid(Store) || !Store->CanInteract_Implementation(Player())) return;
 StopFire(); StopAim(); Player()->GetCombatComponent()->UnequipWeapon(); ActiveStore=Store; ShopResult.Reset();
 auto* PC=Cast<APlayerController>(Player()->GetController());
 if(!Feedback) Feedback=CreateWidget<UCombatFeedbackWidget>(PC,UCombatFeedbackWidget::StaticClass());
 if(Feedback && !Feedback->IsInViewport()) Feedback->AddToViewport(10);
}
void UPlayerCombatAdapter::CloseStore() { ActiveStore.Reset(); ShopResult.Reset(); if(Feedback) Feedback->SetVisibility(ESlateVisibility::Collapsed); }
void UPlayerCombatAdapter::AttachWeaponFeedback()
{
 auto* P=Player(); auto* Weapon=P->GetCombatComponent()->GetWeapon();
 if(!Weapon) return;
	Weapon->OnMuzzleFlash.AddUniqueDynamic(this,&UPlayerCombatAdapter::MuzzleFlash);
	Weapon->OnImpactEffect.AddUniqueDynamic(this,&UPlayerCombatAdapter::Impact);
	Weapon->OnRecoil.AddUniqueDynamic(this,&UPlayerCombatAdapter::Recoil);
	auto* PC=Cast<APlayerController>(P->GetController());
	if (!Feedback) { Feedback=CreateWidget<UCombatFeedbackWidget>(PC,UCombatFeedbackWidget::StaticClass()); }
	if (Feedback) { if(!Feedback->IsInViewport()) Feedback->AddToViewport(10); Feedback->SetVisibility(ESlateVisibility::HitTestInvisible); }
}
void UPlayerCombatAdapter::Aim()
{
	if (!CanUse() || bAiming) { return; }
	auto* P=Player(); auto* W=P->GetCombatComponent()->GetWeapon();
	if (!W || !W->IsEquipped() || !P->GetCameraExtensionComponent()->BeginAim(P->GetFollowCamera(),P->GetCameraBoom())) { return; }
	bAiming=true; P->GetCombatComponent()->SetAiming(true);
	bSavedOrient=P->GetCharacterMovement()->bOrientRotationToMovement; bSavedYaw=P->bUseControllerRotationYaw;
	P->GetCharacterMovement()->bOrientRotationToMovement=false; P->bUseControllerRotationYaw=true;
}
void UPlayerCombatAdapter::StopAim()
{
	if (!bAiming) { return; }
	if (auto* P=Player())
	{
		P->GetCameraExtensionComponent()->EndAim(); P->GetCombatComponent()->SetAiming(false);
		P->GetCharacterMovement()->bOrientRotationToMovement=bSavedOrient; P->bUseControllerRotationYaw=bSavedYaw;
	}
	bAiming=false;
}
void UPlayerCombatAdapter::StartFire()
{ if(!CanUse()) return; StopFire(); bTriggerHeld=true; TriggerWeapon=Player()->GetCombatComponent()->GetWeapon(); Fire(); }
void UPlayerCombatAdapter::StopFire()
{ bTriggerHeld=false; TriggerWeapon.Reset(); GetWorld()->GetTimerManager().ClearTimer(FireTimer); }
void UPlayerCombatAdapter::Fire()
{
 if (!CanUse() || !TriggerWeapon.IsValid() || TriggerWeapon.Get()!=Player()->GetCombatComponent()->GetWeapon()) { StopFire(); return; }
	auto* PC=Cast<APlayerController>(Player()->GetController());
	FCombatAttackRequest Request; FRotator Rotation;
	PC->GetPlayerViewPoint(Request.ViewOrigin,Rotation); Request.Direction=Rotation.Vector(); Request.Instigator=PC;
	Request.Aiming=bAiming; Request.Crouched=Player()->bIsCrouched;
 const auto* Current=Player()->GetCombatComponent()->GetWeapon();
 Request.MovementRatio=Player()->GetVelocity().Size2D()/FMath::Max(100.f,Current?Current->GetData()->MovementReferenceSpeed:500.f);
 auto* Combat=Player()->GetCombatComponent(); Combat->Attack(Request);
 auto* W=Combat->GetWeapon();
 if(bTriggerHeld && W && W->IsEquipped() && W->GetAmmo()>0 && !W->IsReloading() && W->GetData()->FireMode==ENeonCityFireMode::Automatic)
 {
  // One-shot scheduling prevents a hitch from producing a burst of catch-up traces.
  GetWorld()->GetTimerManager().SetTimer(FireTimer,this,&UPlayerCombatAdapter::Fire,1.f/FMath::Clamp(W->GetData()->FireRate,.2f,10.f),false);
 }
}
void UPlayerCombatAdapter::Reload() { StopFire(); if (CanUse()) { Player()->GetCombatComponent()->ReloadWeapon(); } }
void UPlayerCombatAdapter::MuzzleFlash(FTransform Transform)
{
	++MuzzleCount; // flash and report: UNeonCityWeaponEffectsSubsystem (AWeaponBase::Fire)
}
void UPlayerCombatAdapter::Impact(const FHitResult& Hit)
{
	++ImpactCount;
	auto* W=Player()->GetCombatComponent()->GetWeapon();
	if (W && W->GetLastAppliedDamage()>0) { HitSeconds=.2f; }
	// The strike itself is presented by UNeonCityWeaponEffectsSubsystem (AHitscanWeapon::FireShot).
}
void UPlayerCombatAdapter::Recoil(float Pitch)
{
	if (!CanUse()) { return; }
	auto* PC=Cast<APlayerController>(Player()->GetController());
	FRotator Rotation=PC->GetControlRotation(); const float PreviousPitch=FRotator::NormalizeAxis(Rotation.Pitch);
 Rotation.Pitch=FMath::Clamp(PreviousPitch+Pitch,-80.f,80.f);
	auto* W=Player()->GetCombatComponent()->GetWeapon(); const float Yaw=W?W->GetLastYawRecoil():0;
 Rotation.Yaw+=Yaw; RecoilOffset.Pitch+=Rotation.Pitch-PreviousPitch; RecoilOffset.Yaw+=Yaw;
 LastRecoilTime=GetWorld()->GetTimeSeconds(); RecoilRecovery=W?W->GetData()->RecoverySpeed:4;
 PC->SetControlRotation(Rotation);
}
void UPlayerCombatAdapter::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
	Super::TickComponent(Delta,Tick,Function);
	auto* P=Player(); if (!P) { return; }
	auto* W=P->GetCombatComponent()->GetWeapon();
	if (!CanUse()) { StopFire(); StopAim(); }
	const auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>();
	// Only Downed/Dead stow the weapon, and that stow is recoverable. Hit reaction, police pursuit and
	// Wanted escalation must never leave the player unarmed by accident.
	const bool bIncapacitated=P->GetHealthComponent()->IsDead()
		|| (Body && (Body->GetBodyState()==ENeonCityBodyState::Downed || Body->GetBodyState()==ENeonCityBodyState::Dead));
	if (bIncapacitated) { P->GetCombatComponent()->StowForIncapacitation(); }
	else if (P->GetCombatComponent()->IsInjuryStowed()) { P->GetCombatComponent()->RestoreAfterIncapacitation(); }
	if (bAiming) { P->GetCameraExtensionComponent()->ApplyAimCamera(Delta); }
	HitSeconds=FMath::Max(0.f,HitSeconds-Delta); DamageSeconds=FMath::Max(0.f,DamageSeconds-Delta);
 if(GetWorld()->GetTimeSeconds()-LastRecoilTime>.2f && CanUse())
 {
  const auto Remaining=FMath::RInterpTo(RecoilOffset,FRotator::ZeroRotator,Delta,RecoilRecovery);
  if(auto* PC=Cast<APlayerController>(P->GetController())) PC->SetControlRotation(PC->GetControlRotation()-(RecoilOffset-Remaining));
  RecoilOffset=Remaining;
 }
 if(ActiveStore.IsValid())
 {
  auto* PC=Cast<APlayerController>(P->GetController());
  const auto* Phone=PC?PC->FindComponentByClass<UPhoneComponent>():nullptr;
  if((Phone && Phone->IsPhoneOpen()) || !ActiveStore->CanInteract_Implementation(P)) CloseStore();
  else if(Feedback) { Feedback->SetVisibility(ESlateVisibility::HitTestInvisible); Feedback->UpdateStatus(FText::FromString(ActiveStore->Describe(P).ToString()+TEXT("\n")+ShopResult),false,false); return; }
 }
	const bool bArmed=W && W->IsEquipped() && CanUse();
	const ENeonCityFeedbackTier Vitals=GetVitalsTier();
	const bool bVitals=Vitals!=ENeonCityFeedbackTier::Healthy;
	if (bArmed || bVitals || DamageSeconds>0)
	{
		if (auto* Widget=EnsureFeedback())
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (bArmed) { Widget->UpdateStatus(FText::FromString(FString::Printf(TEXT("%s   %d / %d   %s\n1-4 Equip / Stow   RMB Aim   LMB Fire   R Reload"),*W->GetData()->DisplayName.ToString(),W->GetAmmo(),W->GetReserveAmmo(),W->IsReloading() ? TEXT("RELOADING") : TEXT(""))),HitSeconds>0,bAiming); }
			else { Widget->UpdateStatus(FText::GetEmpty(),false,false); }
			Widget->UpdateDamage(BuildDamageIndicator(),DamageSeconds>0);
			const FLinearColor Color=Vitals==ENeonCityFeedbackTier::Dead?FLinearColor(1,.1f,.1f):Vitals==ENeonCityFeedbackTier::Downed?FLinearColor(1,.35f,.15f)
				:Vitals==ENeonCityFeedbackTier::Critical?FLinearColor(1,.7f,.15f):FLinearColor(1,.9f,.35f);
			Widget->UpdateVitals(bVitals?FText::FromString(Vitals==ENeonCityFeedbackTier::Dead?TEXT("DEAD"):Vitals==ENeonCityFeedbackTier::Downed?TEXT("DOWNED"):Vitals==ENeonCityFeedbackTier::Critical?TEXT("CRITICAL"):TEXT("INJURED")):FText::GetEmpty(),Color,bVitals);
		}
	}
	else if (Feedback) { Feedback->SetVisibility(ESlateVisibility::Collapsed); }
}
void UPlayerCombatAdapter::BeginPlay()
{
	Super::BeginPlay(); BindDamageFeedback();
}
void UPlayerCombatAdapter::BindDamageFeedback()
{
	auto* P=Player(); if(!P) return;
	if(auto* Source=P->FindComponentByClass<UCharacterDamageFeedbackComponent>())
	{
		DamageSource=Source;
		Source->OnDamageFeedback.AddUniqueDynamic(this,&UPlayerCombatAdapter::DamagedFeedback);
	}
}
void UPlayerCombatAdapter::DamagedFeedback(const FNeonCityDamageFeedbackEvent& Event)
{
	auto* P=Player(); if(!P) return;
	++DamageFeedbackCount; DamageSeconds=1.25f; LastDamageAmount=Event.Amount;
	// Bearing is stored relative to the view so the indicator stays readable while the player turns.
	const FRotator View=P->GetControlRotation();
	const FRotator From=Event.DamageDirection.IsNearlyZero()?View:Event.DamageDirection.Rotation();
	LastDamageYaw=FRotator::NormalizeAxis(From.Yaw-View.Yaw);
}
UCombatFeedbackWidget* UPlayerCombatAdapter::EnsureFeedback()
{
	if(Feedback) return Feedback;
	auto* P=Player(); auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
	if(!PC) return nullptr;
	Feedback=CreateWidget<UCombatFeedbackWidget>(PC,UCombatFeedbackWidget::StaticClass());
	if(Feedback && !Feedback->IsInViewport()) Feedback->AddToViewport(10);
	return Feedback;
}
FText UPlayerCombatAdapter::BuildDamageIndicator() const
{
	if(DamageSeconds<=0) return FText::GetEmpty();
	const float Yaw=FMath::Abs(LastDamageYaw);
	const TCHAR* Bearing=Yaw>=140?TEXT("BEHIND"):Yaw<=30?TEXT("FRONT"):LastDamageYaw<0?TEXT("<<"):TEXT(">>");
	return FText::FromString(FString::Printf(TEXT("HIT %d  %s"),FMath::RoundToInt(LastDamageAmount),Bearing));
}
ENeonCityFeedbackTier UPlayerCombatAdapter::GetVitalsTier() const
{
	if(const auto* Source=DamageSource.Get()) return Source->GetFeedbackTier();
	const auto* P=Player(); if(!P) return ENeonCityFeedbackTier::Healthy;
	const auto* H=P->GetHealthComponent(); const auto* Body=P->FindComponentByClass<UDamageableCharacterComponent>();
	if(H && H->IsDead()) return ENeonCityFeedbackTier::Dead;
	if(Body && Body->GetBodyState()==ENeonCityBodyState::Downed) return ENeonCityFeedbackTier::Downed;
	const float Fraction=H && H->GetMaxHealth()>0?H->GetCurrentHealth()/H->GetMaxHealth():1.f;
	return Fraction<=.35f?ENeonCityFeedbackTier::Critical:Fraction<=.5f?ENeonCityFeedbackTier::Injured:ENeonCityFeedbackTier::Healthy;
}
void UPlayerCombatAdapter::Suspend()
{
	StopFire(); CloseStore(); StopAim(); RemoveInput(); RecoilOffset=FRotator::ZeroRotator;
	if (auto* P=Player()) { P->GetCombatComponent()->UnequipWeapon(); }
	if (Feedback) { Feedback->RemoveFromParent(); }
}
void UPlayerCombatAdapter::EndPlay(EEndPlayReason::Type Reason) { Suspend(); Super::EndPlay(Reason); }
void UPlayerCombatAdapter::StowForUtility()
{
 StopFire();StopAim();RecoilOffset=FRotator::ZeroRotator;
 if(auto* P=Player())P->GetCombatComponent()->UnequipWeapon();
 if(Feedback)Feedback->SetVisibility(ESlateVisibility::Collapsed);
}
