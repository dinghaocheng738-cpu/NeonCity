#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Phone/PhoneComponent.h"
#include "Property/NeonCityStarterApartment.h"
#include "Interaction/InteractionComponent.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Editor.h"
#include "EngineUtils.h"
#include <limits>

namespace
{
class FFlashlightCheck : public IAutomationLatentCommand
{
public:
 explicit FFlashlightCheck(FAutomationTestBase* In) : Test(In) {}
 void Key(FKey K,EInputEvent E)
 { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
 bool Next(int32 Value) { Stage=Value;At=World->GetTimeSeconds();return false; }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90){Test->AddError(FString::Printf(TEXT("Flashlight stage %d timed out"),Stage));return true;}
  if(!Stage)
  {
   World=GEditor?GEditor->PlayWorld.Get():nullptr;PC=World?World->GetFirstPlayerController():nullptr;
   Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
   if(!Player || World->GetTimeSeconds()<4 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   Light=Player->FindComponentByClass<UNeonCityFlashlightComponent>();
   for(TActorIterator<ANeonCityStarterApartment> It(World);It;++It)Home=*It;
   if(!Test->TestNotNull(TEXT("Normal home route is available"),Home))return true;
   if(!Test->TestNotNull(TEXT("Normal player contains held utility"),Light))return true;
   for(TActorIterator<ANeonCityDayNightCycle> It(World);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(22);}
   Test->TestFalse(TEXT("Light starts stowed"),Light->IsEquipped());
   Key(EKeys::One,IE_Pressed);return Next(1);
  }
  const float Elapsed=World->GetTimeSeconds()-At;
  Evidence.Record(World,PC,Stage);
  if(Stage==1 && Elapsed>.3f)
  {
   Key(EKeys::One,IE_Released);
   Weapon=Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestTrue(TEXT("Real weapon key equips owned pistol"),Weapon.IsValid() && Weapon->IsEquipped()))return true;
   Ammo=Weapon->GetAmmo();Reserve=Weapon->GetReserveAmmo();
   Key(EKeys::RightMouseButton,IE_Pressed);return Next(2);
  }
  if(Stage==2 && Elapsed>.35f)
  {
   Test->TestTrue(TEXT("Weapon aim active before utility transition"),Player->GetCombatAdapter()->IsAiming());
   Key(EKeys::L,IE_Pressed);return Next(3);
  }
  if(Stage==3 && Elapsed>.3f)
  {
   Key(EKeys::L,IE_Released);Key(EKeys::RightMouseButton,IE_Released);
   if(!Test->TestTrue(TEXT("L equips real held mesh and light"),Light->IsEquipped() && Light->GetVisual() && Light->GetVisual()->IsVisible() && Light->GetBeam() && Light->GetBeam()->IsVisible()))return true;
   Test->TestTrue(TEXT("Holster retains original weapon and ammunition"),Player->GetCombatComponent()->GetWeapon()==Weapon.Get() && !Weapon->IsEquipped() && Weapon->GetAmmo()==Ammo && Weapon->GetReserveAmmo()==Reserve);
   Test->TestFalse(TEXT("Aim adapter released"),Player->GetCombatAdapter()->IsAiming());
   Test->TestEqual(TEXT("Default character camera restored"),Player->GetCameraExtensionComponent()->GetCameraContext(),ENeonCityCameraContext::Default);
   Test->TestTrue(TEXT("Utility attached to right hand"),Light->GetVisual()->GetAttachParent()==Player->GetMesh() && Light->GetVisual()->GetAttachSocketName()==TEXT("hand_r"));
   Test->TestTrue(TEXT("Beam originates at optical socket"),Light->GetBeam()->GetComponentLocation().Equals(Light->GetVisual()->GetSocketLocation(TEXT("LightEmission")),.1));
   Test->TestTrue(TEXT("Physical beam uses shared shadow budget"),Light->GetBeam()->CastShadows && World->GetSubsystem<UWorldLightingSubsystem>()->GetShadowLightCount()<=UWorldLightingSubsystem::ShadowLightBudget);
   Origin=Player->GetActorLocation();return Next(4);
  }
  if(Stage==4)
  {
   if(Elapsed<1.2f){Player->AddMovementInput(Player->GetActorForwardVector(),.5f);return false;}
   if(Elapsed<1.6f)return false;
   Test->TestTrue(TEXT("Inherited locomotion continues with flashlight"),FVector::Dist2D(Origin,Player->GetActorLocation())>100);
   const auto* Anim=Cast<UCharacterPresentationAnimInstance>(Player->GetMesh()->GetAnimInstance());
   Test->TestTrue(TEXT("Dedicated right-arm carry layer without firing/support-hand pose"),Anim && Anim->FlashlightAlpha>.9f && Anim->ArmedAlpha<.1f && Anim->SupportHandAlpha<.1f);
   Test->AddInfo(FString::Printf(TEXT("Flashlight optical direction %s, character forward %s, grip %s"),*Light->GetBeam()->GetForwardVector().ToString(),*Player->GetActorForwardVector().ToString(),*Light->GetVisual()->GetComponentLocation().ToString()));
   Test->AddInfo(FString::Printf(TEXT("Flashlight up %s, hand frame %s, index %s, middle %s, thumb %s"),
    *Light->GetVisual()->GetUpVector().ToString(),*Player->GetMesh()->GetSocketTransform(TEXT("hand_r")).Rotator().ToString(),
    *Light->GetVisual()->GetComponentTransform().InverseTransformPosition(Player->GetMesh()->GetSocketLocation(TEXT("index_02_r"))).ToString(),
    *Light->GetVisual()->GetComponentTransform().InverseTransformPosition(Player->GetMesh()->GetSocketLocation(TEXT("middle_02_r"))).ToString(),
    *Light->GetVisual()->GetComponentTransform().InverseTransformPosition(Player->GetMesh()->GetSocketLocation(TEXT("thumb_03_r"))).ToString()));
   Test->TestTrue(TEXT("Held beam points in front of character"),FVector::DotProduct(Light->GetBeam()->GetForwardVector(),Player->GetActorForwardVector())>.4);
   Test->TestTrue(TEXT("Lantern body hangs upright below the grip"),Light->GetVisual()->GetUpVector().Z>.75f);
   Test->TestTrue(TEXT("Handle meets the closed hand rather than the wrist"),FVector::Distance(Light->GetVisual()->GetComponentLocation(),Player->GetMesh()->GetSocketLocation(TEXT("middle_02_r")))<3.5f);
   Test->TestTrue(TEXT("Native night/player camera evidence"),FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Flashlight-Night-PlayerView.png")));
   // Clearly identified inspection camera, not a manual player-route claim.
   const FVector Target=Light->GetVisual()->GetComponentLocation()+FVector(0,0,-5);
   const FVector Eye=Target+Player->GetActorForwardVector()*70+Player->GetActorRightVector()*95+FVector(0,0,35);
   Camera=World->SpawnActor<ACameraActor>(Eye,(Target-Eye).Rotation());PC->SetViewTarget(Camera);
   return Next(5);
  }
  if(Stage==5 && Elapsed>1)
  {
   Test->TestTrue(TEXT("Native hand-contact inspection evidence"),FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Flashlight-Hand-Inspection.png")));
   PC->SetViewTarget(Player);Camera->Destroy();Camera=nullptr;
   Key(EKeys::P,IE_Pressed);return Next(6);
  }
  if(Stage==6 && Elapsed>.3f)
  {
   Key(EKeys::P,IE_Released);
   Test->TestTrue(TEXT("Normal phone input opened the phone"),PC->FindComponentByClass<UPhoneComponent>()->IsPhoneOpen());
   Test->TestFalse(TEXT("Phone use stows held utility"),Light->IsEquipped());
   Test->TestFalse(TEXT("No utility draw through open phone"),Light->SetEquipped(true));
   // Enhanced Input samples once per frame; a release and repress in the same
   // latent update is one continuous press, not a second physical key stroke.
   return Next(60);
  }
  if(Stage==60 && Elapsed>.1f){Key(EKeys::P,IE_Pressed);return Next(7);}
  if(Stage==7 && Elapsed>.3f)
  {
   Key(EKeys::P,IE_Released);
   if(!Test->TestFalse(TEXT("Separate second P press closes phone"),PC->FindComponentByClass<UPhoneComponent>()->IsPhoneOpen()))return true;
   Test->TestTrue(TEXT("Battery can be depleted by a future system"),Light->SetBatteryCharge(0));
   Test->TestFalse(TEXT("Empty battery cannot enable light"),Light->SetEquipped(true));
   Test->TestFalse(TEXT("Invalid battery rejected"),Light->SetBatteryCharge(std::numeric_limits<float>::quiet_NaN()));
   Test->TestEqual(TEXT("Invalid value preserves valid battery"),Light->GetBatteryCharge(),0.f);
   Test->TestTrue(TEXT("Battery recharge interface"),Light->SetBatteryCharge(1));
   Key(EKeys::L,IE_Pressed);return Next(8);
  }
  if(Stage==8 && Elapsed>.3f)
  {
   Key(EKeys::L,IE_Released);Test->TestTrue(TEXT("Light can be drawn again"),Light->IsEquipped());
   Key(EKeys::One,IE_Pressed);return Next(9);
  }
  if(Stage==9 && Elapsed>.3f)
  {
   Key(EKeys::One,IE_Released);
   Test->TestFalse(TEXT("Weapon key stows light"),Light->IsEquipped());
   Test->TestTrue(TEXT("Original weapon reequips with unchanged ammunition"),Weapon->IsEquipped() && Weapon->GetAmmo()==Ammo && Weapon->GetReserveAmmo()==Reserve);
   Light->SetEquipped(true);PC->UnPossess();
   Test->TestFalse(TEXT("Possession loss immediately removes hand light"),Light->IsEquipped());
   Test->TestFalse(TEXT("Unpossessed utility rejects use"),Light->SetEquipped(true));
   PC->Possess(Player);return Next(10);
  }
  if(Stage==10 && Elapsed>.4f){Key(EKeys::L,IE_Pressed);return Next(11);}
  if(Stage==11 && Elapsed>.3f)
  {
   Key(EKeys::L,IE_Released);
   Test->TestTrue(TEXT("Re-possession installs exactly one working toggle"),Light->IsEquipped());
   Player->GetCombatAdapter()->PrepareForRestore();
   Test->TestFalse(TEXT("Load boundary clears transient hand light"),Light->IsEquipped());
   Test->TestFalse(TEXT("Load boundary disables actual beam"),Light->GetBeam()->IsVisible());
   Light->SetEquipped(true);return Next(12);
  }
  if(Stage==12)
  {
   FVector Target=Home->GetActorLocation()+FVector(0,150,0);Target.Z=Player->GetActorLocation().Z;
   if(FVector::Dist2D(Target,Player->GetActorLocation())>10)
   {Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),.5);return false;}
   Player->GetCharacterMovement()->StopMovementImmediately();
   FVector Eye;FRotator Rotation;Player->GetActorEyesViewPoint(Eye,Rotation);
   PC->SetControlRotation((Home->GetActorLocation()+FVector(0,0,120)-Eye).Rotation());
   if(!Test->TestTrue(TEXT("Walked to ordinary home door interaction"),Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==Home))return true;
   Key(EKeys::F,IE_Pressed);return Next(13);
  }
  if(Stage==13 && Elapsed>.4f)
  {
   Key(EKeys::F,IE_Released);
   if(!Test->TestFalse(TEXT("F exits home without losing held light"),Home->IsInsideInterior_Implementation(Player)))return true;
   Test->TestTrue(TEXT("Light remains active after interior transition"),Light->IsEquipped());return Next(14);
  }
  if(Stage==14)
  {
   if(Elapsed<2.f){Player->AddMovementInput(FVector(0,-1,0),.5);return false;}
   if(Elapsed<3.f)return false;
   Test->TestTrue(TEXT("Normal outdoor night beam evidence"),FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Flashlight-Night-Outside.png")));
   Light->SetEquipped(false);return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;
 UWorld* World=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;
 UNeonCityFlashlightComponent* Light=nullptr;ACameraActor* Camera=nullptr;
 ANeonCityStarterApartment* Home=nullptr;
 TWeakObjectPtr<AWeaponBase> Weapon;
 FVector Origin=FVector::ZeroVector;
 int32 Stage=0,Ammo=0,Reserve=0;double Started=0;float At=0;
 FNeonCityVisualEvidence Evidence{TEXT("ImmersionFlashlight")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmersionFlashlightTest,"NeonCity.Immersion.FlashlightPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImmersionFlashlightTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(FFlashlightCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
