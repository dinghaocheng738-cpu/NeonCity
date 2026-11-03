#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Adventure/CoastalWildlife.h"
#include "Adventure/MarineSchool.h"
#include "Adventure/WingsuitComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Fishing/FishingComponent.h"
#include "Phone/PhoneComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"

UDivingComponent::UDivingComponent() { PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false; }
void UDivingComponent::BeginPlay()
{
 Super::BeginPlay();GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->OnReset.AddDynamic(this,&UDivingComponent::Unequip);
}
bool UDivingComponent::Equip()
{
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(bEquipped)return true;
 if(!PC || !P->GetCharacterMovement()->IsMovingOnGround() || !P->FindComponentByClass<UDamageableCharacterComponent>()->CanAct())return false;
 auto* Suit=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Diving/SK_ExpeditionDrysuit"));
 auto* Equipment=P->FindComponentByClass<UDiveEquipmentComponent>();
 if(!Suit || !Equipment)return false;
 P->GetCombatAdapter()->StowForUtility();
 if(!Equipment->Equip())return false;
 if(auto* Flight=P->FindComponentByClass<UWingsuitComponent>())Flight->Unequip();
 P->GetCombatAdapter()->StowForUtility();
 auto* Camera=P->GetFollowCamera();OldCamera=Camera->GetRelativeTransform();OldUseControl=Camera->bUsePawnControlRotation;
 OldOwnerNoSee=P->GetMesh()->bOwnerNoSee;
 OriginalSkin=P->GetMesh()->GetSkeletalMeshAsset();OriginalMaterials=P->GetMesh()->OverrideMaterials;P->GetMesh()->EmptyOverrideMaterials();P->GetMesh()->SetSkeletalMesh(Suit,false);
 auto* Oxygen=P->FindComponentByClass<UOxygenComponent>();OldDrain=Oxygen->DrainRate;Oxygen->DrainRate=.55f;
 HUD=CreateWidget<UPolishDivingWidget>(PC);
 HUD->WidgetTree=NewObject<UWidgetTree>(HUD);auto* Canvas=HUD->WidgetTree->ConstructWidget<UCanvasPanel>();HUD->WidgetTree->RootWidget=Canvas;
 auto* Panel=HUD->WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.012f,.035f,.043f,.78f));Panel->SetPadding(FMargin(14,10));
 auto* Slot=Canvas->AddChildToCanvas(Panel);Slot->SetAnchors(FAnchors(0,1));Slot->SetAlignment(FVector2D(0,1));Slot->SetPosition(FVector2D(28,-30));Slot->SetAutoSize(true);
 Status=HUD->WidgetTree->ConstructWidget<UTextBlock>();auto Font=Status->GetFont();Font.Size=15;Status->SetFont(Font);Panel->SetContent(Status);
 auto* Cross=HUD->WidgetTree->ConstructWidget<UTextBlock>();Cross->SetText(FText::FromString(TEXT("+")));Crosshair=Cross;Cross->SetVisibility(ESlateVisibility::Hidden);
 auto* CrossSlot=Canvas->AddChildToCanvas(Cross);CrossSlot->SetAnchors(FAnchors(.5,.5));CrossSlot->SetAlignment(FVector2D(.5,.5));CrossSlot->SetAutoSize(true);
 HUD->AddToViewport(30);HUD->SetVisibility(ESlateVisibility::HitTestInvisible);
 bEquipped=true;bOwnsSuit=true;bOwnsDPV=true;bFirstPerson=false;SetComponentTickEnabled(true);InstallEquipmentInput();Notice=TEXT("R05 fitted. Q deploys DPV underwater. Return to this station to remove gear.");return true;
}
void UDivingComponent::SetFirstPerson(bool Enabled)
{
 if(!bEquipped || bFirstPerson==Enabled)return;
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P)return;
 auto* Camera=P->GetFollowCamera();bFirstPerson=Enabled;Focus.Reset();CaptureSeconds=0;TrackingSeconds=0;
 CaptureStage=EDiveCaptureStage::Searching;bTriggerHeld=false;
 if(Enabled)
 {
  Camera->AttachToComponent(P->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
  Camera->SetRelativeLocationAndRotation(FVector(5,0,64),FRotator::ZeroRotator);Camera->bUsePawnControlRotation=true;
 }
 else
 {
  Camera->AttachToComponent(P->GetCameraBoom(),FAttachmentTransformRules::KeepRelativeTransform,TEXT("SpringEndpoint"));
  Camera->SetRelativeTransform(OldCamera);Camera->bUsePawnControlRotation=OldUseControl;
 }
 P->GetMesh()->SetOwnerNoSee(Enabled || OldOwnerNoSee);
 if(Crosshair)Crosshair->SetVisibility(Enabled?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
}
void UDivingComponent::Unequip()
{
 bRestoreWorn=false;
 FinishGearChange();RemoveEquipmentInput();
 if(!bEquipped)return;SetFirstPerson(false);bEquipped=false;SetComponentTickEnabled(false);Focus.Reset();CaptureSeconds=0;
 if(auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner()))
 {
  if(auto* Equipment=P->FindComponentByClass<UDiveEquipmentComponent>())Equipment->Stow();
  auto* Camera=P->GetFollowCamera();Camera->AttachToComponent(P->GetCameraBoom(),FAttachmentTransformRules::KeepRelativeTransform,TEXT("SpringEndpoint"));
  Camera->SetRelativeTransform(OldCamera);Camera->bUsePawnControlRotation=OldUseControl;P->GetMesh()->SetOwnerNoSee(OldOwnerNoSee);
  P->FindComponentByClass<UOxygenComponent>()->DrainRate=OldDrain;
  if(OriginalSkin)P->GetMesh()->SetSkeletalMesh(OriginalSkin,false);OriginalSkin=nullptr;
  P->GetMesh()->EmptyOverrideMaterials();for(int32 I=0;I<OriginalMaterials.Num();++I)P->GetMesh()->SetMaterial(I,OriginalMaterials[I]);OriginalMaterials.Reset();
 }
 if(Net)Net->DestroyComponent();if(Tank)Tank->DestroyComponent();Net=nullptr;Tank=nullptr;
 if(HUD)HUD->RemoveFromParent();HUD=nullptr;Status=nullptr;Crosshair=nullptr;
}
void UDivingComponent::Operate(bool Aim,bool Collect,float Delta)
{
 if(!bEquipped)return;
 if(auto* Equipment=GetOwner()->FindComponentByClass<UDiveEquipmentComponent>())
 {
  Equipment->Operate(Aim,Collect,Delta);Notice=Equipment->GetStatus();
  const auto Phase=Equipment->GetSpearPhase();
  CaptureStage=Phase==ESpearPhase::Ready?(Aim?EDiveCaptureStage::Tracking:EDiveCaptureStage::Searching)
   :Phase==ESpearPhase::Flying?EDiveCaptureStage::Sweeping:Phase==ESpearPhase::Tethered?EDiveCaptureStage::Securing:EDiveCaptureStage::Recovering;
 }
}
void UDivingComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!PC || PC->GetPawn()!=P || !P->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()){Unequip();return;}
 if(bRestoreWorn)
 {
  if(CanChangeGear())
  {
   const bool DPVOwned=bOwnsDPV;bRestoreWorn=false;Equip();bOwnsDPV=DPVOwned;
  }
  else if(GetWorld()->GetTimeSeconds()>RestoreUntil){bRestoreWorn=false;SetComponentTickEnabled(bEquipped);}
  return;
 }
 if(bChanging)
 {
  P->GetCharacterMovement()->StopMovementImmediately();P->GetCombatAdapter()->StowForUtility();
  const float Age=GetWorld()->GetTimeSeconds()-ChangeStarted;
  if(!bChanged && Age>=.8f)
  {
   bChanged=true;
   if(bRemoving)
   {
    // Keep the final adjustment animation and our own movement lock after the mesh swap.
    const float Started=ChangeStarted;auto* Locked=LockedController.Get();LockedController.Reset();bChanging=false;
    Unequip();bChanging=true;bRemoving=true;bChanged=true;ChangeStarted=Started;LockedController=Locked;SetComponentTickEnabled(true);
   }
   else if(!Equip()){FinishGearChange();return;}
  }
  if(Status)Status->SetText(FText::FromString(bRemoving?TEXT("Stowing regulator and harness..."):TEXT("Checking shoulder straps, mask and regulator...")));
  if(Age>=3.2f)FinishGearChange();
  return;
 }
 auto* Phone=PC->FindComponentByClass<UPhoneComponent>();auto* Fishing=PC->FindComponentByClass<UFishingComponent>();
 const bool Free=(!Phone || !Phone->IsPhoneOpen()) && (!Fishing || !Fishing->IsActive()) && !P->GetCombatAdapter()->IsStoreOpen();
 if(Free && PC->WasInputKeyJustPressed(EKeys::V))SetFirstPerson(!bFirstPerson);
 if(Free && PC->WasInputKeyJustPressed(EKeys::B)) { bBallast=!bBallast;Notice=bBallast?TEXT("Ballast engaged: settle gently onto the seabed."):TEXT("Neutral buoyancy: free swimming."); }
 if(!Free)bEquipmentTrigger=false;
 P->GetCombatAdapter()->StowForUtility();
 if(auto* Equipment=P->FindComponentByClass<UDiveEquipmentComponent>())
 {
  Equipment->Operate(PC->IsInputKeyDown(EKeys::RightMouseButton),bEquipmentTrigger,Delta,Free);
  Notice=Equipment->GetStatus();
 }
 if(Crosshair)Crosshair->SetColorAndOpacity(FSlateColor(bFirstPerson && PC->IsInputKeyDown(EKeys::RightMouseButton)?FLinearColor(.25f,1.f,.7f):FLinearColor::White));
 if(Status)Status->SetText(FText::FromString(FString::Printf(TEXT("R05  |  AIR %.0f%%  |  %s\n%s\nG Speargun   Q DPV   V View   B Buoyancy   L Light\nSpace Rise   C Dive   R Retrieve   F at station: Remove gear"),P->FindComponentByClass<UOxygenComponent>()->CurrentOxygen,bBallast?TEXT("BALLAST"):TEXT("NEUTRAL"),*Notice)));
}
void UDivingComponent::EndPlay(EEndPlayReason::Type Reason)
{
 GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->OnReset.RemoveDynamic(this,&UDivingComponent::Unequip);
 Unequip();Super::EndPlay(Reason);
}

bool UDivingComponent::ControlsAvailable() const
{
 const auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());const auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!PC || PC->GetPawn()!=P || bChanging || !P->FindComponentByClass<UDamageableCharacterComponent>()->CanAct())return false;
 const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();const auto* Fishing=PC->FindComponentByClass<UFishingComponent>();
 return (!Phone || !Phone->IsPhoneOpen()) && (!Fishing || !Fishing->IsActive()) && !P->GetCombatAdapter()->IsStoreOpen();
}
bool UDivingComponent::CanChangeGear() const
{
 const auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());
 return P && ControlsAvailable() && P->GetCharacterMovement()->IsMovingOnGround()
  && P->FindComponentByClass<UWaterInteractionComponent>()->GetWaterState()==EWaterInteractionState::Dry;
}
bool UDivingComponent::BeginGearChange()
{
 if(!CanChangeGear())return false;
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());auto* PC=Cast<APlayerController>(P->GetController());
 SetFirstPerson(false);P->GetCombatAdapter()->StowForUtility();StopEquipmentTrigger();
 if(auto* E=P->FindComponentByClass<UDiveEquipmentComponent>())E->StowDPV();
 bChanging=true;bRemoving=bEquipped;bChanged=false;ChangeStarted=GetWorld()->GetTimeSeconds();
 LockedController=PC;PC->SetIgnoreMoveInput(true);P->GetCharacterMovement()->StopMovementImmediately();
 SetComponentTickEnabled(true);return true;
}
float UDivingComponent::GetGearChangeTime() const
{
 if(!bChanging || !GetWorld())return 0;
 const float Age=FMath::Clamp(GetWorld()->GetTimeSeconds()-ChangeStarted,0.f,3.2f);
 return bRemoving?3.2f-Age:Age;
}
void UDivingComponent::FinishGearChange()
{
 if(auto* PC=LockedController.Get())PC->SetIgnoreMoveInput(false);
 LockedController.Reset();bChanging=false;bChanged=false;SetComponentTickEnabled(bEquipped);
}
void UDivingComponent::RestoreGear(bool SuitOwned,bool DPVOwned,bool Worn)
{
 Unequip();bOwnsSuit=SuitOwned;bOwnsDPV=SuitOwned && DPVOwned;
 // Water/ground probes refresh after the coordinator moves the capsule. Wait
 // for that safe state; a blocked restore keeps the kit owned and stowed.
 bRestoreWorn=Worn && SuitOwned;RestoreUntil=GetWorld()->GetTimeSeconds()+2;
 if(bRestoreWorn)SetComponentTickEnabled(true);
}
void UDivingComponent::InstallEquipmentInput()
{
 RemoveEquipmentInput();auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());
 auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 auto* Input=P?Cast<UEnhancedInputComponent>(P->InputComponent):nullptr;
 auto* Subsystem=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Input || !Subsystem)return;
 if(!EquipmentMapping)
 {
  EquipmentMapping=NewObject<UInputMappingContext>(this,TEXT("IMC_R05Equipment"));
  DPVAction=NewObject<UInputAction>(this,TEXT("IA_EquipStowDPV"));DPVAction->ValueType=EInputActionValueType::Boolean;
  SpeargunAction=NewObject<UInputAction>(this,TEXT("IA_EquipStowSpeargun"));SpeargunAction->ValueType=EInputActionValueType::Boolean;
  EquipmentTriggerAction=NewObject<UInputAction>(this,TEXT("IA_DiveEquipmentTrigger"));EquipmentTriggerAction->ValueType=EInputActionValueType::Boolean;
  EquipmentMapping->MapKey(DPVAction,EKeys::Q);EquipmentMapping->MapKey(EquipmentTriggerAction,EKeys::LeftMouseButton);
  EquipmentMapping->MapKey(SpeargunAction,EKeys::G);
 }
 EquipmentBindings.Add(Input->BindAction(DPVAction,ETriggerEvent::Started,this,&UDivingComponent::TogglePropulsor).GetHandle());
 EquipmentBindings.Add(Input->BindAction(SpeargunAction,ETriggerEvent::Started,this,&UDivingComponent::ToggleSpeargun).GetHandle());
 EquipmentBindings.Add(Input->BindAction(EquipmentTriggerAction,ETriggerEvent::Started,this,&UDivingComponent::StartEquipmentTrigger).GetHandle());
 EquipmentBindings.Add(Input->BindAction(EquipmentTriggerAction,ETriggerEvent::Completed,this,&UDivingComponent::StopEquipmentTrigger).GetHandle());
 EquipmentBindings.Add(Input->BindAction(EquipmentTriggerAction,ETriggerEvent::Canceled,this,&UDivingComponent::StopEquipmentTrigger).GetHandle());
 EquipmentInput=Input;EquipmentSubsystem=Subsystem;Subsystem->AddMappingContext(EquipmentMapping,10);
}
void UDivingComponent::RemoveEquipmentInput()
{
 if(auto* Input=EquipmentInput.Get())for(uint32 Handle:EquipmentBindings)Input->RemoveBindingByHandle(Handle);
 if(auto* Subsystem=EquipmentSubsystem.Get())Subsystem->RemoveMappingContext(EquipmentMapping);
 EquipmentBindings.Reset();EquipmentInput.Reset();EquipmentSubsystem.Reset();bEquipmentTrigger=false;
}
void UDivingComponent::TogglePropulsor()
{
 if(!bEquipped || !bOwnsDPV || !ControlsAvailable())return;
 StopEquipmentTrigger();if(auto* E=GetOwner()->FindComponentByClass<UDiveEquipmentComponent>())E->ToggleDPV();
}
void UDivingComponent::StartEquipmentTrigger(){bEquipmentTrigger=bEquipped && ControlsAvailable();}
void UDivingComponent::ToggleSpeargun()
{
 if(!bEquipped || !ControlsAvailable())return;
 StopEquipmentTrigger();if(auto* E=GetOwner()->FindComponentByClass<UDiveEquipmentComponent>())E->ToggleSpeargun();
}
void UDivingComponent::StopEquipmentTrigger(){bEquipmentTrigger=false;}
