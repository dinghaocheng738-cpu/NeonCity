#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Combat/CombatCollision.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Combat/CombatComponent.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/DamageSystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Combat/Weapons/WeaponData.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Player/PlayerCombatAdapter.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleInteraction.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include <limits>

namespace NeonCityCombatTests
{
class FVerify : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if (!Started) { Started=FPlatformTime::Seconds(); }
  if (FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Combat PIE timed out")); return true; }
  UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
  APlayerController* PC=World ? World->GetFirstPlayerController() : nullptr;
  if (!PC || !PC->GetPawn() || !World->HasBegunPlay()) { return false; }
  float Now=World->GetTimeSeconds();
  if(Stage>0) Evidence.Record(World,PC,Stage);
  auto Next=[&](int32 Value) { Stage=Value; Phase=Now; return false; };
  auto* Damage=World->GetSubsystem<UDamageSystem>();
  if (Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
   if (!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround()) { return false; }
   int32 Targets=0;
   for (TActorIterator<ACombatTestTarget> It(World); It; ++It) { Target=*It; ++Targets; }
   if (!Test->TestEqual(TEXT("One authored test target"),Targets,1)) { return true; }
   for (TActorIterator<ANeonCityNPCCharacter> It(World); It; ++It) { NPC=*It; break; }
   for (TActorIterator<ANeonCityBaseVehicle> It(World); It; ++It) { if(It->bStartsOwned) { Car=*It; break; } }
   if (!Test->TestNotNull(TEXT("NPC exists"),NPC.Get()) || !Test->TestNotNull(TEXT("Car exists"),Car.Get())) { return true; }
   AnimClass=Player->GetMesh()->GetAnimClass();
   SavedFOV=Player->GetFollowCamera()->FieldOfView;
   SavedArm=Player->GetCameraBoom()->TargetArmLength;
   SavedOffset=Player->GetCameraBoom()->SocketOffset;
   Player->SetActorLocationAndRotation(FVector(2200,-800,136),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   PC->SetControlRotation(FRotator::ZeroRotator);
   Test->TestEqual(TEXT("Initial health"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
   Test->TestTrue(TEXT("NPC combat hit capsule"),NPC->GetCapsuleComponent()->GetCollisionResponseToChannel(NeonCityCombat::WeaponTrace)==ECR_Block);
   return Next(1);
  }
  if (Stage==1 && Now-Phase>.4f) { Key(PC,EKeys::One,IE_Pressed); return Next(2); }
  if (Stage==2 && Now-Phase>.2f)
  {
   Key(PC,EKeys::One,IE_Released);
   Weapon=Player->GetCombatComponent()->GetWeapon();
   if (!Test->TestTrue(TEXT("1 equips pistol"),Weapon.IsValid() && Weapon->IsEquipped())) { return true; }
   Test->TestEqual(TEXT("Full magazine"),Weapon->GetAmmo(),12);
   Test->TestEqual(TEXT("Data asset used"),Weapon->GetData()->GetPathName(),FString(TEXT("/Game/NeonCity/Combat/DA_NeonCityPistol.DA_NeonCityPistol")));
   Key(PC,EKeys::RightMouseButton,IE_Pressed); return Next(3);
  }
  if (Stage==3 && Now-Phase>.6f)
  {
   Test->TestTrue(TEXT("RMB shoulder aim"),Player->GetCombatAdapter()->IsAiming());
   Test->TestTrue(TEXT("Shoulder FOV and offset"),Player->GetFollowCamera()->FieldOfView<SavedFOV-10 && Player->GetCameraBoom()->SocketOffset.Y>60);
   Test->TestTrue(TEXT("Original animation blueprint retained"),Player->GetMesh()->GetAnimClass()==AnimClass.Get());
   AimAt(PC,Target->GetActorLocation()); return Next(4);
  }
  if (Stage==4 && Now-Phase>.3f) { AimAt(PC,Target->GetActorLocation()); return Next(5); }
  if (Stage==5 && Now-Phase>.3f)
  {
   BeforePitch=FRotator::NormalizeAxis(PC->GetControlRotation().Pitch);
   Key(PC,EKeys::LeftMouseButton,IE_Pressed); return Next(6);
  }
  if (Stage==6 && Now-Phase>.05f)
  {
   Key(PC,EKeys::LeftMouseButton,IE_Released);
   Test->TestEqual(TEXT("LMB consumed one round"),Weapon->GetAmmo(),11);
   Test->TestEqual(TEXT("Hitscan reduced target health"),Target->GetHealthComponent()->GetCurrentHealth(),75.f);
   Test->TestTrue(TEXT("Unified event source and controller"),Damage->GetLastDamage().SourceActor==Weapon.Get() && Damage->GetLastDamage().Instigator==PC);
   Test->TestTrue(TEXT("Ballistic damage type"),Damage->GetLastDamage().DamageType==ENeonCityDamageType::Ballistic);
   Test->TestTrue(TEXT("Recoil changed pitch"),FRotator::NormalizeAxis(PC->GetControlRotation().Pitch)>BeforePitch+.4f);
   Test->TestTrue(TEXT("Feedback dispatched"),Player->GetCombatAdapter()->GetMuzzleFeedbackCount()==1 && Player->GetCombatAdapter()->GetImpactFeedbackCount()==1);
   FCombatAttackRequest Request; Request.ViewOrigin=Weapon->GetMuzzleLocation(); Request.Direction=FVector::ForwardVector;
   Test->TestFalse(TEXT("Cooldown rejects immediate second fire"),Player->GetCombatComponent()->Attack(Request));
   Shot(TEXT("Combat-Hit.png"));
   Key(PC,EKeys::R,IE_Pressed); return Next(7);
  }
  if (Stage==7 && Now-Phase>.2f)
  {
   Key(PC,EKeys::R,IE_Released);
   Test->TestTrue(TEXT("R begins timed reload"),Weapon->IsReloading());
   FCombatAttackRequest Request; Request.Direction=FVector::ForwardVector;
   Test->TestFalse(TEXT("Reload blocks fire"),Weapon->Fire(Request));
   Test->TestEqual(TEXT("No early ammo transfer"),Weapon->GetAmmo(),11);
   return Next(8);
  }
  if (Stage==8 && Now-Phase>1.2f)
  {
   Test->TestFalse(TEXT("Reload finished"),Weapon->IsReloading());
   Test->TestEqual(TEXT("Magazine refilled"),Weapon->GetAmmo(),12);
   Test->TestEqual(TEXT("Reserve decremented exactly"),Weapon->GetReserveAmmo(),47);
   Key(PC,EKeys::RightMouseButton,IE_Released); return Next(9);
  }
  if (Stage==9 && Now-Phase>.2f)
  {
   Test->TestFalse(TEXT("RMB release ends aim"),Player->GetCombatAdapter()->IsAiming());
   Test->TestEqual(TEXT("Default FOV restored"),Player->GetFollowCamera()->FieldOfView,SavedFOV);
   Test->TestEqual(TEXT("Default arm restored"),Player->GetCameraBoom()->TargetArmLength,SavedArm);
   Test->TestTrue(TEXT("Default offset and movement policy restored"),Player->GetCameraBoom()->SocketOffset.Equals(SavedOffset) && Player->GetCharacterMovement()->bOrientRotationToMovement && !Player->bUseControllerRotationYaw);
   Key(PC,EKeys::One,IE_Pressed); return Next(10);
  }
  if (Stage==10 && Now-Phase>.2f)
  {
   Key(PC,EKeys::One,IE_Released);
   Test->TestFalse(TEXT("1 stows pistol"),Weapon->IsEquipped());
   // Isolated Actor mounts prove the weapon has no Character/Player dependency.
   Host=World->SpawnActor<ACombatTestTarget>(FVector(-1000,0,5000),FRotator::ZeroRotator);
   Remote=World->SpawnActor<ACombatTestTarget>(FVector(0,0,5000),FRotator::ZeroRotator);
   if (!Test->TestTrue(TEXT("Non-Pawn mount equips same weapon"),Host->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),Host->GetRootComponent()))) { return true; }
   FCombatAttackRequest Request=GenericRequest();
   Test->TestTrue(TEXT("ICombatInterface attacks from generic Actor"),ICombatInterface::Execute_Attack(Host.Get(),Request));
   Test->TestEqual(TEXT("Generic weapon uses shared damage"),Remote->GetHealthComponent()->GetCurrentHealth(),75.f);
   Test->TestTrue(TEXT("Cancel reload started"),Host->GetCombatComponent()->ReloadWeapon());
   Host->GetCombatComponent()->UnequipWeapon();
   return Next(11);
  }
  if (Stage==11 && Now-Phase>1.3f)
  {
   auto* W=Host->GetCombatComponent()->GetWeapon();
   Test->TestTrue(TEXT("Stow cancels reload without transferring ammo"),!W->IsReloading() && W->GetAmmo()==11 && W->GetReserveAmmo()==48);
   Host->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),Host->GetRootComponent());
   Blocker=World->SpawnActor<AActor>();
   auto* Box=NewObject<UBoxComponent>(Blocker.Get());
   Blocker->SetRootComponent(Box); Box->SetBoxExtent(FVector(40,100,50));
   Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
   Blocker->SetActorLocation(FVector(-500,0,5000));
   auto Request=GenericRequest(); Request.ViewOrigin+=FVector(0,0,300); Request.Direction=(Remote->GetActorLocation()-Request.ViewOrigin).GetSafeNormal();
   Test->TestTrue(TEXT("Blocked muzzle still fires"),ICombatInterface::Execute_Attack(Host.Get(),Request));
   Test->TestEqual(TEXT("Camera cannot shoot through muzzle obstruction"),Remote->GetHealthComponent()->GetCurrentHealth(),75.f);
   Test->TestTrue(TEXT("Muzzle obstruction is actual hit"),W->GetLastHit().GetActor()==Blocker.Get());
   Blocker->Destroy();
   FNeonCityDamageEvent Event; Event.SourceActor=Player.Get(); Event.Instigator=PC; Event.DamageAmount=-5;
   const auto Count=Damage->GetDamageCount();
   Test->TestEqual(TEXT("Negative damage rejected"),Damage->ApplyDamage(Remote.Get(),Event),0.f);
   Event.DamageAmount=std::numeric_limits<float>::quiet_NaN();
   Test->TestEqual(TEXT("NaN damage rejected"),Damage->ApplyDamage(Remote.Get(),Event),0.f);
   Test->TestEqual(TEXT("Invalid damage emits no event"),Damage->GetDamageCount(),Count);
   Test->TestEqual(TEXT("Healing clamps to max"),Remote->GetHealthComponent()->Heal(1000),25.f);
   Event.DamageAmount=60;
   ICombatInterface::Execute_TakeCombatDamage(NPC.Get(),Event);
   Test->TestTrue(TEXT("NPC immediate HitReaction"),Body(NPC.Get())->GetBodyState()==ENeonCityBodyState::HitReaction);
   Player->GetPlayerDataComponent()->SetHealth(40);
   Test->TestEqual(TEXT("PlayerData uses common health"),Player->GetHealthComponent()->GetCurrentHealth(),40.f);
   Test->TestTrue(TEXT("Player immediate HitReaction"),Body(Player.Get())->GetBodyState()==ENeonCityBodyState::HitReaction);
   return Next(12);
  }
  if (Stage==12 && Now-Phase>.5f)
  {
   for (ACharacter* Character:{static_cast<ACharacter*>(Player.Get()),static_cast<ACharacter*>(NPC.Get())})
   {
    auto* H=Character->FindComponentByClass<UHealthComponent>();
    Test->TestTrue(TEXT("Reaction settles to Injured"),Body(Character)->GetBodyState()==ENeonCityBodyState::Injured);
    FNeonCityDamageEvent Event; Event.DamageAmount=25;
    H->TakeDamage(Event);
    Test->TestTrue(TEXT("Low health becomes Downed"),Body(Character)->GetBodyState()==ENeonCityBodyState::Downed);
    Test->TestTrue(TEXT("Downed movement disabled"),Character->GetCharacterMovement()->MovementMode==MOVE_None);
    H->Heal(25);
    Test->TestTrue(TEXT("Healing downed restores movement"),Body(Character)->GetBodyState()==ENeonCityBodyState::Injured && Character->GetCharacterMovement()->MovementMode!=MOVE_None);
    Character->GetCharacterMovement()->DisableMovement(); // Seated-character fixture.
    H->TakeDamage(Event);
    Body(Character)->RestoreLocomotionMode(MOVE_Walking,0);
    Test->TestTrue(TEXT("Exiting while downed remains locked"),Character->GetCharacterMovement()->MovementMode==MOVE_None);
    H->Heal(25);
    Test->TestTrue(TEXT("Healing after exit restores walking"),Character->GetCharacterMovement()->MovementMode==MOVE_Walking);
   }
   NPC->GetHealthComponent()->Die();
   Test->TestTrue(TEXT("Dead NPC retained"),IsValid(NPC.Get()) && Body(NPC.Get())->GetBodyState()==ENeonCityBodyState::Dead);
   auto* W=Host->GetCombatComponent()->GetWeapon();
   Test->TestTrue(TEXT("Reload before death"),W->Reload());
   Host->GetHealthComponent()->Die();
   Test->TestTrue(TEXT("Generic death cancels weapon and reload"),!W->IsEquipped() && !W->IsReloading());
   Car->GetHealthComponent()->Die();
   Test->TestTrue(TEXT("Vehicle shares death state without destruction"),IsValid(Car.Get()) && ICombatInterface::Execute_GetCombatState(Car.Get())==ENeonCityCombatState::Dead);
   Test->TestFalse(TEXT("Dead vehicle cannot be entered"),Car->FindComponentByClass<UVehicleInteraction>()->CanEnter(Player.Get()));
   FNeonCityDamageEvent Lethal; Lethal.DamageAmount=1000; Lethal.Instigator=PC; Lethal.SourceActor=Weapon.Get();
   Test->TestEqual(TEXT("Lethal damage clamped to remaining health"),Damage->ApplyDamage(Target.Get(),Lethal),75.f);
   Test->TestTrue(TEXT("Target dies and remains"),IsValid(Target.Get()) && Target->GetHealthComponent()->IsDead());
   const auto Count=Damage->GetDamageCount();
   Target->GetHealthComponent()->Die(); Damage->ApplyDamage(Target.Get(),Lethal);
   Test->TestEqual(TEXT("Dead target cannot receive duplicate death/damage"),Damage->GetDamageCount(),Count);
   Test->TestEqual(TEXT("Heal cannot resurrect"),Target->GetHealthComponent()->Heal(100),0.f);
   Player->GetHealthComponent()->Heal(100);
   // Check possession cleanup while aiming without conflating this with the existing driving regression.
   Key(PC,EKeys::One,IE_Pressed);
   return Next(13);
  }
  if (Stage==13 && Now-Phase>.2f)
  {
   Key(PC,EKeys::One,IE_Released); Key(PC,EKeys::RightMouseButton,IE_Pressed); return Next(14);
  }
  if (Stage==14 && Now-Phase>.3f)
  {
   Test->TestTrue(TEXT("Aim before possession change"),Player->GetCombatAdapter()->IsAiming());
   PC->Possess(Car.Get());
   Test->TestTrue(TEXT("Unpossess suspends combat"),!Player->GetCombatAdapter()->IsAiming() && !Weapon->IsEquipped());
   Test->TestEqual(TEXT("Unpossess restores camera"),Player->GetFollowCamera()->FieldOfView,SavedFOV);
   PC->Possess(Player.Get());
   Key(PC,EKeys::RightMouseButton,IE_Released);
   return Next(15);
  }
  if (Stage==15 && Now-Phase>.3f)
  {
   Key(PC,EKeys::One,IE_Pressed); return Next(16);
  }
  if (Stage==16 && Now-Phase>.2f)
  {
   Key(PC,EKeys::One,IE_Released);
   Test->TestTrue(TEXT("Combat input returns after possession"),Weapon->IsEquipped());
   Player->GetHealthComponent()->Die();
   Test->TestTrue(TEXT("Player Dead retained"),IsValid(Player.Get()) && Body(Player.Get())->GetBodyState()==ENeonCityBodyState::Dead);
   Host->Destroy(); Remote->Destroy();
   Host=World->SpawnActor<ACombatTestTarget>(FVector(-1000,0,5000),FRotator::ZeroRotator);
   Remote=World->SpawnActor<ACombatTestTarget>(FVector(0,0,5000),FRotator::ZeroRotator);
   Host->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),Host->GetRootComponent());
   return Next(17);
  }
  if (Stage==17 && Now-Phase>.27f)
  {
   auto* W=Host->GetCombatComponent()->GetWeapon();
   if (W->GetAmmo()>0)
   {
    Test->TestTrue(TEXT("Valid paced shot"),ICombatInterface::Execute_Attack(Host.Get(),GenericRequest()));
    if (W->GetAmmo()==8) { Test->TestTrue(TEXT("Four hits kill target without destruction"),IsValid(Remote.Get()) && Remote->GetHealthComponent()->IsDead()); }
    return Next(17);
   }
   Test->TestFalse(TEXT("Empty magazine blocks fire"),ICombatInterface::Execute_Attack(Host.Get(),GenericRequest()));
   Test->TestEqual(TEXT("Empty fire cannot consume reserve"),W->GetReserveAmmo(),48);
   Host->Destroy(); Remote->Destroy(); return true;
  }
  return false;
 }
private:
 FCombatAttackRequest GenericRequest() const
 {
  FCombatAttackRequest R; R.ViewOrigin=Host->GetActorLocation(); R.Direction=(Remote->GetActorLocation()-R.ViewOrigin).GetSafeNormal(); return R;
 }
 static UDamageableCharacterComponent* Body(AActor* A) { return A->FindComponentByClass<UDamageableCharacterComponent>(); }
 static void AimAt(APlayerController* PC,FVector Point)
 {
  FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin,Rotation); PC->SetControlRotation((Point-Origin).Rotation());
 }
 static void Key(APlayerController* PC,FKey Value,EInputEvent Event)
 {
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Value,Event,FPlatformTime::Cycles64()));
 }
 static void Shot(const TCHAR* Name)
 {
  if (!FParse::Param(FCommandLine::Get(),TEXT("NeonCityCombatCapture"))) { return; }
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase8")),true);
  FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase8")/Name,true,false);
 }
 FNeonCityVisualEvidence Evidence{TEXT("G-PistolAimFire")};
 FAutomationTestBase* Test;
 int32 Stage=0; double Started=0; float Phase=0,BeforePitch=0,SavedFOV=0,SavedArm=0;
 FVector SavedOffset;
 TWeakObjectPtr<UClass> AnimClass;
 TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
 TWeakObjectPtr<ANeonCityNPCCharacter> NPC;
 TWeakObjectPtr<ANeonCityBaseVehicle> Car;
 TWeakObjectPtr<AWeaponBase> Weapon;
 TWeakObjectPtr<ACombatTestTarget> Target,Host,Remote;
 TWeakObjectPtr<AActor> Blocker;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCombat,"NeonCity.Combat.FoundationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCombat::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityCombatTests::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
