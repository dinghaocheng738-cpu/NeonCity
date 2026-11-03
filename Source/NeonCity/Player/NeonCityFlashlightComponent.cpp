#include "Player/NeonCityFlashlightComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Adventure/DivingComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Fishing/FishingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Phone/PhoneComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"

UNeonCityFlashlightComponent::UNeonCityFlashlightComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.bStartWithTickEnabled=false;
 PrimaryComponentTick.TickInterval=.05f;
 MeshAsset=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/Immersion/Flashlight/SM_NC_HandheldFlashlight.SM_NC_HandheldFlashlight")));
}
bool UNeonCityFlashlightComponent::CanUse() const
{
 const auto* Player=Cast<ANeonCityPlayerCharacter>(GetOwner());
 const auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
 if(!PC || !PC->IsLocalController() || PC->GetPawn()!=Player || Player->IsHidden()
  || Player->GetCombatAdapter()->IsStoreOpen())return false;
 const auto* Dive=Player->FindComponentByClass<UDivingComponent>();
 if(Player->GetCharacterMovement()->IsSwimming() && (!Dive || !Dive->IsEquipped()))return false;
 if(const auto* Body=Player->FindComponentByClass<UDamageableCharacterComponent>();Body && !Body->CanAct())return false;
 if(const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();Phone && Phone->IsPhoneOpen())return false;
 if(const auto* Fishing=PC->FindComponentByClass<UFishingComponent>();Fishing && Fishing->IsActive())return false;
 return true;
}
void UNeonCityFlashlightComponent::SetupInput(UInputComponent* Input)
{
 RemoveInput();
 const auto* Player=Cast<ANeonCityPlayerCharacter>(GetOwner());
 auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
 auto* Enhanced=Cast<UEnhancedInputComponent>(Input);
 auto* Subsystem=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Enhanced || !Subsystem)return;
 if(!Mapping)
 {
  Mapping=NewObject<UInputMappingContext>(this,NAME_None,RF_Transient);
  Action=NewObject<UInputAction>(this,NAME_None,RF_Transient);
  Action->ValueType=EInputActionValueType::Boolean;Mapping->MapKey(Action,EKeys::L);
 }
 BindingHandle=Enhanced->BindAction(Action,ETriggerEvent::Started,this,&UNeonCityFlashlightComponent::Toggle).GetHandle();
 BoundInput=Enhanced;InputSubsystem=Subsystem;bBound=true;
 FModifyContextOptions Options;Options.bIgnoreAllPressedKeysUntilRelease=true;
 Subsystem->AddMappingContext(Mapping,5,Options);
}
void UNeonCityFlashlightComponent::RemoveInput()
{
 if(bBound && BoundInput.IsValid())BoundInput->RemoveBindingByHandle(BindingHandle);
 if(InputSubsystem.IsValid() && Mapping)InputSubsystem->RemoveMappingContext(Mapping);
 bBound=false;BoundInput.Reset();InputSubsystem.Reset();
}
bool UNeonCityFlashlightComponent::CreateVisuals()
{
 if(Visual && Beam)return true;
 auto* Player=Cast<ANeonCityPlayerCharacter>(GetOwner());
 auto* Mesh=MeshAsset.LoadSynchronous();
 if(!Player || !Mesh || !Player->GetMesh()->DoesSocketExist(TEXT("hand_r")) || !Mesh->FindSocket(TEXT("LightEmission")))return false;
 Visual=NewObject<UStaticMeshComponent>(Player,TEXT("HandheldFlashlight"));
 Player->AddInstanceComponent(Visual);Visual->SetStaticMesh(Mesh);
 Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Visual->SetGenerateOverlapEvents(false);Visual->SetCanEverAffectNavigation(false);
 Visual->SetupAttachment(Player->GetMesh(),TEXT("hand_r"));
 // The mesh origin is its handle, while hand_r is the wrist. This fixed grip
 // offset matches the retargeted lantern pose; animation owns all motion.
 Visual->SetRelativeLocation(FVector(-8.f,3.f,1.f));
 Visual->SetRelativeRotation(FRotator(0,180,90));
 Visual->SetVisibility(false);Visual->RegisterComponent();
 const int32 LensIndex=Mesh->GetMaterialIndex(TEXT("M_NC_FlashlightLens"));
 if(LensIndex!=INDEX_NONE)Lens=Visual->CreateDynamicMaterialInstance(LensIndex);
 Beam=NewObject<USpotLightComponent>(Player,TEXT("HandheldFlashlightBeam"));
 Player->AddInstanceComponent(Beam);Beam->SetupAttachment(Visual,TEXT("LightEmission"));
 Beam->SetMobility(EComponentMobility::Movable);Beam->SetIntensityUnits(ELightUnits::Lumens);
 Beam->SetIntensity(650.f);Beam->SetAttenuationRadius(2200.f);
 Beam->SetInnerConeAngle(12.f);Beam->SetOuterConeAngle(28.f);
 Beam->SetSourceRadius(2.f);Beam->SetSoftSourceRadius(3.f);
 Beam->SetLightColor(FLinearColor(1.f,.92f,.79f));
 Beam->SetVisibility(false);Beam->RegisterComponent();
 FNeonCityLocalLightPolicy Policy;
 Policy.Intensity=650.f;Policy.DayMultiplier=1.f;Policy.ShadowDistance=2500.f;
 Policy.CullDistance=3000.f;Policy.Priority=100;Policy.VolumetricScattering=.8f;Policy.bEnabled=false;
 auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>();
 if(!Lighting || !Lighting->RegisterLight(Beam,Policy))
 {
  Beam->DestroyComponent();Visual->DestroyComponent();Beam=nullptr;Visual=nullptr;Lens=nullptr;return false;
 }
 return true;
}
bool UNeonCityFlashlightComponent::SetEquipped(bool bEquip)
{
 if(bEquip && (!CanUse() || BatteryCharge<=0 || !CreateVisuals()))return false;
 const bool bChanged=bEquipped!=bEquip;
 if(bEquip)
 {
  // Keep inventory, ammunition and weapon actor. Release trigger/aim before
  // the utility takes the hand, including automatic-fire timers.
  CastChecked<ANeonCityPlayerCharacter>(GetOwner())->GetCombatAdapter()->StowForUtility();
 }
 bEquipped=bEquip;SetComponentTickEnabled(bEquip);
 if(bEquip)UpdateMount();
 if(Visual)Visual->SetVisibility(bEquip);
 if(Lens)Lens->SetScalarParameterValue(TEXT("LightEnabled"),bEquip?1.f:0.f);
 if(Beam)
 {
  if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
  {Lighting->SetLightEnabled(Beam,bEquip);Lighting->RefreshLighting();}
  else Beam->SetVisibility(false);
 }
 if(bChanged)OnLightSwitched.Broadcast(bEquip);
 return true;
}
void UNeonCityFlashlightComponent::Toggle()
{
 if(!bEquipped && CanUse() && BatteryCharge<=0)
 {
  if(const auto* Pawn=Cast<APawn>(GetOwner());Pawn && Pawn->GetController())
   if(auto* Navigation=Pawn->GetController()->FindComponentByClass<UNeonCityNavigationComponent>())
    Navigation->ShowNotice(NSLOCTEXT("NeonCity","FlashlightEmpty","Flashlight battery depleted"));
  return;
 }
 SetEquipped(!bEquipped);
}
bool UNeonCityFlashlightComponent::SetBatteryCharge(float Charge)
{
 if(!FMath::IsFinite(Charge) || Charge<0 || Charge>1)return false;
 BatteryCharge=Charge;if(Charge<=0)SetEquipped(false);return true;
}
void UNeonCityFlashlightComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Tick,Function);
 const auto* Player=Cast<ANeonCityPlayerCharacter>(GetOwner());
 const auto* Weapon=Player?Player->GetCombatComponent()->GetWeapon():nullptr;
 if(!CanUse() || (Weapon && Weapon->IsEquipped()))SetEquipped(false);
 else if(bEquipped)UpdateMount();
}
void UNeonCityFlashlightComponent::UpdateMount()
{
 auto* P=Cast<ANeonCityPlayerCharacter>(GetOwner());if(!P || !Visual)return;
 const auto* Dive=P->FindComponentByClass<UDivingComponent>();const bool Mounted=Dive && Dive->IsEquipped();
 if(Mounted!=bDiveMounted)
 {
  UStaticMesh* Mesh=Mounted?LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/OceanExperience/Equipment/R2/SM_OceanDiveLight")):MeshAsset.LoadSynchronous();
  if(!Mesh || !Mesh->FindSocket(TEXT("LightEmission")))return;
  Visual->EmptyOverrideMaterials();Visual->SetStaticMesh(Mesh);bDiveMounted=Mounted;
  Visual->SetAbsolute(Mounted,Mounted,false);
  if(!Mounted){Visual->SetRelativeLocation(FVector(-8,3,1));Visual->SetRelativeRotation(FRotator(0,180,90));}
  const int32 Slot=Mesh->GetMaterialIndex(Mounted?TEXT("M_OceanEquipment_LampLens"):TEXT("M_NC_FlashlightLens"));
  Lens=Slot!=INDEX_NONE?Visual->CreateDynamicMaterialInstance(Slot):nullptr;
  if(Lens)Lens->SetScalarParameterValue(TEXT("LightEnabled"),bEquipped?1:0);
 }
 Visual->SetOwnerNoSee(Mounted && Dive->IsFirstPerson());
 if(Mounted)
 {
  // The separate compact lamp clamps to the mask strap. Both hands remain on
  // the spear/DPV; the land lantern carry animation is never used underwater.
  const FVector At=P->GetMesh()->GetSocketLocation(TEXT("head"))+P->GetActorRightVector()*12+FVector(0,0,3);
  const FVector Direction=P->GetBaseAimRotation().Vector();
  const FVector Target=Dive->IsFirstPerson()?P->GetFollowCamera()->GetComponentLocation()+Direction*750:At+Direction*750;
  Visual->SetWorldLocationAndRotation(At,(Target-At).Rotation());
 }
}
void UNeonCityFlashlightComponent::Suspend() { SetEquipped(false);RemoveInput(); }
void UNeonCityFlashlightComponent::EndPlay(EEndPlayReason::Type Reason)
{
 Suspend();
 if(Beam)
 {
  if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())Lighting->UnregisterLight(Beam);
  Beam->DestroyComponent();Beam=nullptr;
 }
 if(Visual){Visual->DestroyComponent();Visual=nullptr;}
 Lens=nullptr;Super::EndPlay(Reason);
}
