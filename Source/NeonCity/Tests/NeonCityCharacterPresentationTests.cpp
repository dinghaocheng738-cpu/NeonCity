#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"

namespace NeonCityCharacterPresentationTest
{
class FVerify final : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100) { Test->AddError(TEXT("Character presentation timed out"));return true; }
  UWorld* World=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || (Stage==0 && !Player->GetCharacterMovement()->IsMovingOnGround())) return false;
  const float Now=World->GetTimeSeconds(); auto Next=[&](int32 S){Stage=S;Phase=Now;return false;};
  auto* Anim=Cast<UCharacterPresentationAnimInstance>(Player->GetMesh()->GetAnimInstance());
  if(!Test->TestNotNull(TEXT("Player uses presentation wrapper"),Anim))return true;
  if(Stage) Evidence.Record(World,PC,Stage);
  if(Stage==0)
  {
   Player->SetActorLocationAndRotation(FVector(2200,-800,136),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation(FRotator::ZeroRotator);
   for(TActorIterator<ANeonCityNPCCharacter> It(World);It;++It)
    Test->TestNotNull(TEXT("Pedestrians share skeletal reaction layer"),Cast<UCharacterPresentationAnimInstance>(It->GetMesh()->GetAnimInstance()));
   return Next(1);
  }
  if(Stage==1 && Now-Phase>.5f)
  { RestHand=Player->GetMesh()->GetSocketLocation(TEXT("hand_r"));Key(PC,EKeys::One,IE_Pressed);return Next(2); }
  if(Stage==2 && Now-Phase>.4f)
  {
   Key(PC,EKeys::One,IE_Released); Weapon=Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestTrue(TEXT("Normal equip input attaches visible weapon"),Weapon.IsValid() && Weapon->IsEquipped() && !Weapon->IsHidden()))return true;
   Key(PC,EKeys::RightMouseButton,IE_Pressed);return Next(3);
  }
  if(Stage==3 && Now-Phase>1)
  {
   const FVector Hand=Player->GetMesh()->GetSocketLocation(TEXT("hand_r"));
   Test->TestTrue(TEXT("Aim lifts actual hand bone above locomotion pose"),Hand.Z>RestHand.Z+25);
   Test->TestTrue(TEXT("Two-hand pose closes support hand around weapon"),FVector::Dist(Hand,Player->GetMesh()->GetSocketLocation(TEXT("hand_l")))<30);
   Test->TestTrue(TEXT("Grip follows hand without actor transform forcing"),FVector::Dist(Weapon->GetActorLocation(),Hand)<15);
   Test->TestTrue(TEXT("Barrel faces forward during shoulder aim"),FVector::DotProduct(Weapon->GetActorForwardVector(),Player->GetActorForwardVector())>.7f);
   UE_LOG(LogTemp,Display,TEXT("Presentation grip: hand=%s muzzle=%s weapon=%s forward=%s"),*Hand.ToString(),*Weapon->GetMuzzleLocation().ToString(),*Weapon->GetActorRotation().ToString(),*Weapon->GetActorForwardVector().ToString());
   for(FName Bone:{TEXT("hand_r"),TEXT("middle_01_r"),TEXT("middle_02_r"),TEXT("index_01_r"),TEXT("thumb_02_r"),TEXT("hand_l"),TEXT("middle_01_l")})
    UE_LOG(LogTemp,Display,TEXT("Grip contact %s weapon-local %s"),*Bone.ToString(),*Weapon->GetActorTransform().InverseTransformPosition(Player->GetMesh()->GetSocketLocation(Bone)).ToString());
   TInlineComponentArray<UStaticMeshComponent*> WeaponParts(Weapon.Get());
   for(const auto* Part:WeaponParts) if(Part->GetFName()==TEXT("ArtVisual"))
   {
    Test->TestTrue(TEXT("Live shot origin matches visible authored muzzle"),Weapon->GetMuzzleLocation().Equals(Part->GetSocketLocation(TEXT("Muzzle")),.1));
    if(Test->TestTrue(TEXT("Pistol authors actual hand contact sockets"),Part->DoesSocketExist(TEXT("SupportHand")) && Part->DoesSocketExist(TEXT("Palm"))))
    {
     Test->TestTrue(TEXT("Support wrist reaches pistol without stretching"),FVector::Dist(Player->GetMesh()->GetSocketLocation(TEXT("hand_l")),Part->GetSocketLocation(TEXT("SupportHand")))<2.5);
     const FVector Palm=(Hand+Player->GetMesh()->GetSocketLocation(TEXT("middle_01_r")))*.5;
     Test->TestTrue(TEXT("Grip is inside the shooting palm instead of the wrist"),FVector::Dist(Palm,Part->GetSocketLocation(TEXT("Palm")))<3);
    }
   }
   // This optional side camera only inspects the actual skeletal pose. Input,
   // weapon simulation, damage and normal linked locomotion stay authoritative.
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
   {
    Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=52;
    const FVector Focus=Player->GetActorLocation()+FVector(0,0,20);
    Camera->SetActorLocation(Focus+FVector(210,235,40));Camera->SetActorRotation((Focus-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);
   }
   return Next(4);
  }
  if(Stage==4 && Now-Phase>1) { Key(PC,EKeys::LeftMouseButton,IE_Pressed);return Next(5); }
  if(Stage==5 && Now-Phase>.12f)
  { Key(PC,EKeys::LeftMouseButton,IE_Released);Test->TestEqual(TEXT("Fire uses existing ammo"),Weapon->GetAmmo(),11);Key(PC,EKeys::R,IE_Pressed);return Next(6); }
  if(Stage==6 && Now-Phase>.45f)
  {
   Key(PC,EKeys::R,IE_Released);
   Test->TestTrue(TEXT("Reload presents a moving hand pose while gameplay timer runs"),Weapon->IsReloading() && FVector::Dist(Player->GetMesh()->GetSocketLocation(TEXT("hand_l")),Player->GetMesh()->GetSocketLocation(TEXT("hand_r")))>3);
   return Next(7);
  }
  if(Stage==7 && Now-Phase>1.4f)
  {
   Test->TestEqual(TEXT("Reload transfers ammunition once"),Weapon->GetAmmo(),12);SavedAmmo=Weapon->GetAmmo();SavedReserve=Weapon->GetReserveAmmo();
   Key(PC,EKeys::RightMouseButton,IE_Released);
   FNeonCityDamageEvent Event;Event.DamageAmount=30;Event.DamageType=ENeonCityDamageType::Ballistic;
   World->GetSubsystem<UDamageSystem>()->ApplyDamage(Player,Event);return Next(8);
  }
  if(Stage==8 && Now-Phase>.7f)
  {
   Test->TestTrue(TEXT("Nonlethal hit preserves same weapon and ammunition"),Player->GetCombatComponent()->GetWeapon()==Weapon.Get() && Weapon->IsEquipped() && !Weapon->IsHidden() && Weapon->GetAmmo()==SavedAmmo && Weapon->GetReserveAmmo()==SavedReserve);
   StandingHead=Player->GetMesh()->GetSocketLocation(TEXT("head")).Z;
   FNeonCityDamageEvent Event;Event.DamageAmount=55;Event.DamageType=ENeonCityDamageType::Ballistic;
   World->GetSubsystem<UDamageSystem>()->ApplyDamage(Player,Event);return Next(9);
  }
  if(Stage==9 && Now-Phase>3)
  {
   Test->TestTrue(TEXT("Downed retains actor with head visibly below standing height"),IsValid(Player) && !Player->IsHidden() && Player->GetMesh()->GetSocketLocation(TEXT("head")).Z<StandingHead-70);
   Test->TestEqual(TEXT("Unified body state remains downed"),Player->FindComponentByClass<UDamageableCharacterComponent>()->GetBodyState(),ENeonCityBodyState::Downed);
   Player->GetHealthComponent()->Heal(100);return Next(10);
  }
  if(Stage==10 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Recovery restores original weapon rather than spawning replacement"),Player->GetCombatComponent()->GetWeapon()==Weapon.Get() && Weapon->IsEquipped() && Weapon->GetAmmo()==SavedAmmo && Weapon->GetReserveAmmo()==SavedReserve);
   Test->TestTrue(TEXT("Recovery returns to standing skeletal pose"),Player->GetMesh()->GetSocketLocation(TEXT("head")).Z>StandingHead-25);
   const auto Snapshot=Player->GetCombatAdapter()->BuildPersistenceSnapshot(false);
   Player->GetCombatAdapter()->ApplyPersistenceSnapshot(Snapshot);
   Test->TestTrue(TEXT("Persistence reattaches equipment to character grip"),Player->GetCombatComponent()->GetWeapon()->GetRootComponent()->GetAttachParent()==Player->FindComponentByClass<UCharacterWeaponPresentationComponent>()->GetGrip());
   if(Camera) {PC->SetViewTarget(Player);Camera->Destroy();} return true;
  }
  return false;
 }
private:
 static void Key(APlayerController* PC,FKey K,EInputEvent E)
 {PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 FAutomationTestBase* Test;double Started=0;float Phase=0,StandingHead=0;int32 Stage=0,SavedAmmo=0,SavedReserve=0;FVector RestHand;
 TWeakObjectPtr<AWeaponBase> Weapon;ACameraActor* Camera=nullptr;
 FNeonCityVisualEvidence Evidence{TEXT("CharacterPresentation")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCharacterPresentation,"NeonCity.VerticalSlice.CharacterPresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCharacterPresentation::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityCharacterPresentationTest::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
