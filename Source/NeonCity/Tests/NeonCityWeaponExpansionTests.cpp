#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/WeaponStoreActor.h"
#include "Combat/CombatComponent.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/ThreatSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/NeonCityCombatEncounter.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/Police/PoliceCharacter.h"
#include "Police/PoliceResponseData.h"
#include "AI/Police/PoliceResponseDirector.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponProfiles,"NeonCity.Weapons.ProfileInvariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWeaponProfiles::RunTest(const FString& Parameters)
{
 auto* Police=NewObject<UPoliceResponseData>();
 TestTrue(TEXT("Only a minority of high-tier officers may receive rifles"),Police->EnableRifleResponse && Police->RifleMinimumLevel>=4 && Police->RifleEveryNthOfficer>=3);
 TSet<FName> IDs; TSet<uint8> Ammo; TSet<float> Recoil;
 for(int32 I=1;I<=4;++I)
 {
  const auto* P=NeonCityWeapons::Profile(NeonCityWeapons::Slot(I)); if(!TestNotNull(TEXT("Registered profile"),P)) return false;
  TestTrue(TEXT("Bounded valid profile"),P->IsValidProfile()); IDs.Add(P->WeaponId); Ammo.Add(uint8(P->AmmoType)); Recoil.Add(P->Recoil);
  TestTrue(TEXT("Aiming tighter than hip fire"),P->AimSpread<P->HipFireSpread);
  TestTrue(TEXT("Semi vs auto modes"),P->FireMode==((I==2 || I==3)?ENeonCityFireMode::Automatic:ENeonCityFireMode::SemiAutomatic));
  TestTrue(TEXT("Stable factory identity"),GetDefault<AWeaponBase>(NeonCityWeapons::Resolve(P->WeaponId))->BuildDefaultSaveData().DataAssetID==NeonCityWeapons::AssetID(P->WeaponId));
 }
 TestEqual(TEXT("Four stable IDs"),IDs.Num(),4); TestEqual(TEXT("Four ammo pools"),Ammo.Num(),4); TestEqual(TEXT("Distinct vertical recoil"),Recoil.Num(),4);
 TestTrue(TEXT("Rifle longer range than SMG and shotgun"),NeonCityWeapons::Profile(NeonCityWeapons::Slot(3))->Range>NeonCityWeapons::Profile(NeonCityWeapons::Slot(2))->Range);
 auto* Bad=NewObject<UWeaponData>(); Bad->PelletCount=1000; TestFalse(TEXT("Trace storms refused"),Bad->IsValidProfile());
 Bad=NewObject<UWeaponData>(); Bad->FireRate=1000; TestFalse(TEXT("Unbounded rate refused"),Bad->IsValidProfile());
 Bad=NewObject<UWeaponData>(); Bad->Damage=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("NaN rejected"),Bad->IsValidProfile());
 TestFalse(TEXT("Unknown weapon never resolves an arbitrary Actor"),bool(NeonCityWeapons::Resolve(TEXT("Unknown.Class"))));
 return true;
}
namespace NeonCityWeaponTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 UWeaponInventoryComponent* Inventory=nullptr; UNeonCityEconomySubsystem* Wallet=nullptr; UNeonCitySaveSubsystem* Save=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || !W->HasBegunPlay() || W->GetTimeSeconds()<1 || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  Inventory=Player->FindComponentByClass<UWeaponInventoryComponent>(); Wallet=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>(); Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); return Inventory && Wallet && Save;
 }
 void Place(FVector P) { Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately(); }
 void Aim(FVector Target) { FVector Eye; FRotator R; PC->GetPlayerViewPoint(Eye,R); PC->SetControlRotation((Target-Eye).Rotation()); }
 void IsolatePolice() { for(TActorIterator<APoliceResponseDirector> It(W);It;++It) It->Destroy(); }
 ACombatTestTarget* Target(FVector P,float Health=10000)
 { auto* T=W->SpawnActor<ACombatTestTarget>(P,FRotator::ZeroRotator); T->GetHealthComponent()->ApplyPersistenceHealth(Health,Health); return T; }
 FCombatAttackRequest Request(AWeaponBase* Gun,FVector Destination)
 { FCombatAttackRequest R; R.ViewOrigin=Gun->GetMuzzleLocation(); R.Direction=(Destination-R.ViewOrigin).GetSafeNormal(); R.Instigator=PC; R.Aiming=true; return R; }
 void Shot(const TCHAR* Name,FVector Eye,FVector Target)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityWeaponCapture"))) return;
  auto* Camera=W->SpawnActor<ACameraActor>(); Camera->SetActorLocation(Eye); Camera->SetActorRotation((Target-Eye).Rotation()); Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;
  Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
  PC->SetViewTarget(Camera); if(PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase17")),true);
  // Capture the PIE render target explicitly: the global screenshot request can be
  // consumed by an editor viewport when many maps have been opened in one test process.
  bool Captured=false;
  if(auto* Client=W->GetGameViewport(); Client && Client->Viewport)
  {
   if(PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
   Client->Viewport->Draw(false);
   TArray<FColor> Pixels;
   if(Client->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pixel:Pixels) Pixel.A=255;
    const FIntPoint Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    Captured=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Phase17")/Name));
   }
  }
  if(!Captured) UE_LOG(LogTemp,Error,TEXT("PIE viewport capture failed: %s"),Name);
  // Capture must not alter later spawn-visibility decisions or possession camera state.
  FTimerHandle Restore; TWeakObjectPtr<APlayerController> Owner=PC; TWeakObjectPtr<ACameraActor> ShotCamera=Camera;
  W->GetTimerManager().SetTimer(Restore,[Owner,ShotCamera]() { if(Owner.IsValid() && ShotCamera.IsValid() && Owner->GetViewTarget()==ShotCamera.Get()) Owner->SetViewTarget(Owner->GetPawn()); if(ShotCamera.IsValid()) ShotCamera->Destroy(); },.25f,false);
 }
};

class FStore : public IAutomationLatentCommand
{
public:
 explicit FStore(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>45) { Test->AddError(FString::Printf(TEXT("Store timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.IsolatePolice();
   for(TActorIterator<AWeaponStoreActor> It(F.W);It;++It) Store=*It;
   if(!Test->TestNotNull(TEXT("South Beach has one accessible store"),Store)) return true;
   // Explicit empty-inventory fixture permits testing a pistol purchase without changing released new-game ownership.
   F.Player->GetCombatAdapter()->ApplyPersistenceSnapshot({}); F.Place(Store->GetActorLocation()+FVector(-220,0,40));
   Crimes=F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().Num(); Cash=F.Wallet->GetBalance(); Stage=1; Phase=F.W->GetTimeSeconds(); return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto Next=[&](int32 S){Stage=S;Phase=Now;return false;};
  if(Stage==1 && Now-Phase>.5f)
  {
   FVector Eye; FRotator R; F.Player->GetActorEyesViewPoint(Eye,R); F.PC->SetControlRotation((Store->GetActorLocation()-Eye).Rotation());
   Key(F.PC,EKeys::F,IE_Pressed); return Next(2);
  }
  if(Stage==2 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::F,IE_Released); Test->TestTrue(TEXT("Actual F opens text store"),F.Player->GetCombatAdapter()->IsStoreOpen());
   Key(F.PC,EKeys::One,IE_Pressed); return Next(3);
  }
  if(Stage==3 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::One,IE_Released); Test->TestTrue(TEXT("Shop key purchases pistol ownership"),F.Inventory->Owns(NeonCityWeapons::Slot(1)));
   Test->TestEqual(TEXT("Pistol purchase debits price"),F.Wallet->GetBalance(),Cash-350);
   const auto LogCount=F.Wallet->GetTransactions().Num();
   Test->TestFalse(TEXT("Duplicate refused"),Store->Buy(F.Player,1,false)); Test->TestEqual(TEXT("Duplicate creates no debit"),F.Wallet->GetTransactions().Num(),LogCount);
   Key(F.PC,EKeys::Five,IE_Pressed); return Next(4);
  }
  if(Stage==4 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::Five,IE_Released); Test->TestEqual(TEXT("Ammo pack finite reserve"),F.Inventory->GetReserve(ENeonCityAmmoType::PistolAmmo),72);
   Test->TestEqual(TEXT("Ammo price charged"),F.Wallet->GetBalance(),Cash-410);
   Test->TestEqual(TEXT("Purchases create no crime"),F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().Num(),Crimes);
   F.Shot(TEXT("A-Store.png"),Store->GetActorLocation()+FVector(-420,-350,210),Store->GetActorLocation()); return Next(5);
  }
  if(Stage==5 && Now-Phase>.5f)
  {
   F.Wallet->RemoveMoney(F.Wallet->GetBalance()-5,TEXT("Test.LowFunds"));
   Test->TestFalse(TEXT("Insufficient weapon funds"),Store->Buy(F.Player,3,false)); Test->TestFalse(TEXT("Insufficient ammo funds"),Store->Buy(F.Player,1,true));
   Test->TestEqual(TEXT("No negative balance"),F.Wallet->GetBalance(),int64(5));
   F.Wallet->AddMoney(10000,TEXT("Test.WeaponPurchase"));
   for(int32 I=2;I<=4;++I) Test->TestTrue(TEXT("All additional weapons purchasable"),Store->Buy(F.Player,I,false));
   Test->TestEqual(TEXT("Four ownership records"),F.Inventory->BuildPersistenceSnapshot().Num(),4);
   Test->TestTrue(TEXT("Rifle pack purchase"),Store->Buy(F.Player,3,true)); Test->TestEqual(TEXT("Rifle reserve pack size"),F.Inventory->GetReserve(ENeonCityAmmoType::RifleAmmo),120);
   Key(F.PC,EKeys::Escape,IE_Pressed); return Next(6);
  }
  if(Stage==6 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::Escape,IE_Released); Test->TestFalse(TEXT("Escape closes shop"),F.Player->GetCombatAdapter()->IsStoreOpen());
   Key(F.PC,EKeys::One,IE_Pressed); return Next(7);
  }
  if(Stage==7 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::One,IE_Released); auto* Gun=F.Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestNotNull(TEXT("Purchased pistol equips"),Gun)) return true;
   F.Place(FVector(2200,-800,136)); Target=F.Target(FVector(2850,-800,150)); return Next(8);
  }
  if(Stage==8 && Now-Phase>.5f) { Key(F.PC,EKeys::RightMouseButton,IE_Pressed); return Next(81); }
  if(Stage==81) { F.Aim(Target->GetActorLocation()); if(Now-Phase>.7f) return Next(82); return false; }
  if(Stage==82 && Now-Phase>.3f) { F.Aim(Target->GetActorLocation()); Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); return Next(9); }
  if(Stage==9 && Now-Phase>.8f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released); auto* Gun=F.Player->GetCombatComponent()->GetWeapon();
   Test->TestEqual(TEXT("Holding pistol trigger fires exactly once"),Gun->GetAmmo(),11);
   Test->AddInfo(FString::Printf(TEXT("Purchased pistol hit %s damage %.1f"),*GetNameSafe(Gun->GetLastHit().GetActor()),Gun->GetLastAppliedDamage()));
   Test->TestTrue(TEXT("Purchased pistol shoots a real target"),Gun->GetLastAppliedDamage()>0);
   Key(F.PC,EKeys::RightMouseButton,IE_Released);
   F.Shot(TEXT("A-Pistol.png"),F.Player->GetActorLocation()+FVector(-200,-350,230),Target->GetActorLocation()); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int32 Stage=0,Crimes=0; double Started=0; float Phase=0; int64 Cash=0; AWeaponStoreActor* Store=nullptr; ACombatTestTarget* Target=nullptr;
};
class FInputAmmo : public IAutomationLatentCommand
{
public:
 explicit FInputAmmo(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>55) { Test->AddError(FString::Printf(TEXT("Input/ammo timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.IsolatePolice(); F.Place(FVector(2200,-800,136)); F.PC->SetControlRotation(FRotator(-20,0,0));
   Test->TestFalse(TEXT("Missing slot cannot equip"),F.Inventory->EquipSlot(4,F.Player->GetMesh(),TEXT("hand_r")));
   F.Wallet->AddMoney(4000,TEXT("WeaponInputFixture")); Test->TestTrue(TEXT("Buy SMG"),F.Inventory->PurchaseWeapon(NeonCityWeapons::Slot(2)));
   Test->TestTrue(TEXT("Buy rifle"),F.Inventory->PurchaseWeapon(NeonCityWeapons::Slot(3)));
   Stage=1; Phase=F.W->GetTimeSeconds(); Key(F.PC,EKeys::Two,IE_Pressed); return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto Next=[&](int32 S){Stage=S;Phase=Now;return false;}; auto* Gun=F.Player->GetCombatComponent()->GetWeapon();
  Evidence.Record(F.W,F.PC,Stage);
  if(Stage==1 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::Two,IE_Released); if(!Test->TestTrue(TEXT("Slot 2 equips SMG"),Gun && Gun->GetData()->WeaponClass==ENeonCityWeaponClass::SMG)) return true;
   Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); return Next(2);
  }
  if(Stage==2 && Now-Phase>1.2f)
  {
   const int32 Shots=30-Gun->GetAmmo(); Test->TestTrue(TEXT("Auto fire builds sustained spread"),Gun->GetSustainedFire()>1); Test->TestTrue(TEXT("Held auto obeys 10 Hz, multiple rounds"),Shots>=6 && Shots<=14);
   FCombatAttackRequest Hip,Aim; Aim.Aiming=true;
   Test->TestTrue(TEXT("Live aim spread is smaller"),Gun->GetSpreadDegrees(Aim)<Gun->GetSpreadDegrees(Hip));
   Aim.MovementRatio=2; Test->TestTrue(TEXT("Sprint-like speed prevents precise aim"),Gun->GetSpreadDegrees(Aim)>Gun->GetSpreadDegrees(Hip));
   Old=Gun; Key(F.PC,EKeys::Three,IE_Pressed); return Next(3);
  }
  if(Stage==3 && Now-Phase>.4f)
  {
   Key(F.PC,EKeys::Three,IE_Released); Key(F.PC,EKeys::LeftMouseButton,IE_Released);
   Test->TestTrue(TEXT("Switch disposes old Actor and equips rifle"),!Old.IsValid() && Gun && Gun->GetData()->WeaponClass==ENeonCityWeaponClass::Rifle);
   Test->TestEqual(TEXT("Held trigger cannot continue across switch"),Gun->GetAmmo(),24);
   int32 Live=0; for(TActorIterator<AWeaponBase> It(F.W);It;++It) if(It->GetOwner()==F.Player) ++Live;
   Test->TestEqual(TEXT("Only one player weapon Actor"),Live,1);
   Key(F.PC,EKeys::Two,IE_Pressed); return Next(4);
  }
  if(Stage==4 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::Two,IE_Released); Test->TestTrue(TEXT("Switch back preserves spent SMG rounds"),Gun->GetAmmo()<30 && Gun->GetAmmo()>=16);
   Total=Gun->GetAmmo()+Gun->GetReserveAmmo(); Key(F.PC,EKeys::R,IE_Pressed); return Next(5);
  }
  if(Stage==5 && Now-Phase>.2f)
  { Key(F.PC,EKeys::R,IE_Released); Test->TestTrue(TEXT("Timed reload begins"),Gun->IsReloading()); return Next(6); }
  if(Stage==6 && Now-Phase>1.7f)
  {
   Test->TestTrue(TEXT("Stopped fire recovers spread"),Gun->GetSustainedFire()<.1f);
   Test->TestEqual(TEXT("Magazine filled from reserve"),Gun->GetAmmo(),30); Test->TestEqual(TEXT("Reload conserves total ammunition"),Gun->GetAmmo()+Gun->GetReserveAmmo(),Total);
   Test->TestFalse(TEXT("Full magazine cannot reload endlessly"),Gun->Reload());
   F.Shot(TEXT("B-SMG-Reload.png"),F.Player->GetActorLocation()+FVector(-200,-300,180),F.Player->GetActorLocation());
   auto Record=Gun->BuildPersistenceSnapshot(); Record.Ammo=0; Record.ReserveAmmo=2; Gun->ApplyPersistenceAmmo(Record);
   Test->TestFalse(TEXT("Empty gun cannot fire"),Gun->Fire(F.Request(Gun,Gun->GetMuzzleLocation()+FVector(1000,0,0))));
   Test->TestTrue(TEXT("Partial reserve reload starts"),Gun->Reload()); return Next(7);
  }
  if(Stage==7 && Now-Phase>1.8f)
  {
   Test->TestEqual(TEXT("Only two real rounds transferred"),Gun->GetAmmo(),2); Test->TestEqual(TEXT("Reserve exhausted"),Gun->GetReserveAmmo(),0);
   Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); return Next(8);
  }
  if(Stage==8 && Now-Phase>.7f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released); Test->TestEqual(TEXT("Auto stops on empty"),Gun->GetAmmo(),0); Test->TestFalse(TEXT("No infinite reserve reload"),Gun->Reload());
   Test->TestTrue(TEXT("Ammo purchase while equipped"),F.Inventory->PurchaseAmmo(ENeonCityAmmoType::SMGAmmo)); Gun->Reload(); return Next(9);
  }
  if(Stage==9 && Now-Phase>1.8f) { Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); return Next(10); }
  if(Stage==10 && Now-Phase>.4f)
  { F.Player->GetHealthComponent()->SetCurrentHealth(15); AmmoAtDown=Gun->GetAmmo(); return Next(11); }
  if(Stage==11 && Now-Phase>.7f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released); Test->TestEqual(TEXT("Downed stops pending automatic fire"),Gun->GetAmmo(),AmmoAtDown);
   Test->TestFalse(TEXT("Downed direct weapon Fire also refused"),Gun->Fire(F.Request(Gun,Gun->GetMuzzleLocation()+FVector(1000,0,0))));
   Test->TestTrue(TEXT("Downed retains weapon ownership"),F.Inventory->Owns(NeonCityWeapons::Slot(2)));
   F.Player->GetHealthComponent()->Heal(100); return true;
  }
  return false;
 }
 FNeonCityVisualEvidence Evidence{TEXT("H-SMGRifleReload")};
 FAutomationTestBase* Test; FScene F; int32 Stage=0,Total=0,AmmoAtDown=0; double Started=0; float Phase=0; TWeakObjectPtr<AWeaponBase> Old;
};

class FBallistics : public IAutomationLatentCommand
{
public:
 explicit FBallistics(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>40) { Test->AddError(TEXT("Ballistics timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.IsolatePolice();
   Host=F.Target(FVector(0,0,5000)); Target=F.Target(FVector(500,0,5000)); Target->SetActorScale3D(FVector(.3f,8,8));
   Test->TestTrue(TEXT("Generic non-character host equips shotgun"),Host->GetCombatComponent()->EquipWeapon(ANeonCityShotgun::StaticClass(),Host->GetRootComponent()));
   auto* G=Host->GetCombatComponent()->GetWeapon(); Test->TestTrue(TEXT("Shotgun near shot"),G->Fire(F.Request(G,Target->GetActorLocation())));
   Test->TestEqual(TEXT("Exactly eight independent pellet traces"),G->GetLastPelletCount(),8); Test->TestEqual(TEXT("One shell per shot"),G->GetAmmo(),5);
   NearDamage=G->GetLastAppliedDamage(); Test->TestTrue(TEXT("Total pellet damage capped at 64"),NearDamage>50 && NearDamage<=64);
   NearFootprint=0; for(const auto& R:G->GetLastPelletDirections()) { NearFootprint=FMath::Max(NearFootprint,float(FVector::Dist(R,FVector::ForwardVector)*500)); DrawDebugLine(F.W,G->GetMuzzleLocation(),G->GetMuzzleLocation()+R*500,FColor::Yellow,false,2); }
   F.Shot(TEXT("D-Shotgun-Near.png"),FVector(-800,-1000,5500),Target->GetActorLocation()); Stage=1; Phase=F.W->GetTimeSeconds(); return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto Next=[&](int32 S){Stage=S;Phase=Now;return false;}; auto* G=Host->GetCombatComponent()->GetWeapon();
  if(Stage==1 && Now-Phase>1.3f)
  {
   Target->SetActorLocation(FVector(2400,0,5000)); Target->SetActorScale3D(FVector(.3f,10,10));
   Test->TestTrue(TEXT("Shotgun far shot"),G->Fire(F.Request(G,Target->GetActorLocation())));
   Test->TestTrue(TEXT("Far shotgun damage declines"),G->GetLastAppliedDamage()>0 && G->GetLastAppliedDamage()<NearDamage);
   float FarFootprint=0; for(const auto& R:G->GetLastPelletDirections()) { FarFootprint=FMath::Max(FarFootprint,float(FVector::Dist(R,FVector::ForwardVector)*2400)); DrawDebugLine(F.W,G->GetMuzzleLocation(),G->GetMuzzleLocation()+R*2400,FColor::Cyan,false,2); }
   Test->TestTrue(TEXT("Far pellet footprint wider than near"),FarFootprint>NearFootprint*2);
   F.Shot(TEXT("D-Shotgun-Far.png"),FVector(-800,-1600,5800),FVector(1300,0,5000)); return Next(2);
  }
  if(Stage==2 && Now-Phase>1)
  {
   Host->GetCombatComponent()->EquipWeapon(ANeonCityRifle::StaticClass(),Host->GetRootComponent()); G=Host->GetCombatComponent()->GetWeapon();
   Target->SetActorLocation(FVector(8000,0,5000)); const float Health=Target->GetHealthComponent()->GetCurrentHealth();
   Test->TestTrue(TEXT("Rifle medium range shot"),G->Fire(F.Request(G,Target->GetActorLocation())));
   Test->TestEqual(TEXT("Rifle causes 32 damage at 80m"),Health-Target->GetHealthComponent()->GetCurrentHealth(),32.f);
   F.Shot(TEXT("C-Rifle-Range.png"),FVector(6900,-1400,5550),Target->GetActorLocation()); return Next(3);
  }
  if(Stage==3 && Now-Phase>.6f)
  {
   Target->SetActorLocation(FVector(17000,0,5000)); const float Health=Target->GetHealthComponent()->GetCurrentHealth();
   G->Fire(F.Request(G,Target->GetActorLocation())); Test->TestEqual(TEXT("Beyond rifle range applies no damage"),Target->GetHealthComponent()->GetCurrentHealth(),Health);
   const FTransform T(FVector(1000,0,5000)); Glass=F.W->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),T); Glass->Configure(ECityPhysicsProp::Glass); Glass->FinishSpawning(T);
   return Next(4);
  }
  if(Stage==4 && Now-Phase>.6f)
  {
   Test->TestTrue(TEXT("Rifle hits Phase15 glass"),G->Fire(F.Request(G,Glass->GetActorLocation())));
   Test->TestTrue(TEXT("Rifle impact reaches glass"),G->GetLastHit().GetActor()==Glass);
   if(++GlassShots<3) return Next(4);
   Test->TestTrue(TEXT("Unified damage breaks glass after its existing damage threshold"),Glass->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   FCombatAttackRequest Aim; Aim.Aiming=true; Aim.Crouched=true; FCombatAttackRequest Standing; Standing.Aiming=true;
   Test->TestTrue(TEXT("Crouch improves spread"),G->GetSpreadDegrees(Aim)<G->GetSpreadDegrees(Standing));
   Host->GetHealthComponent()->Die(); Test->TestFalse(TEXT("Dead generic host stops fire"),G->Fire(F.Request(G,Target->GetActorLocation())));
   Host->Destroy(); Target->Destroy(); Glass->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int32 Stage=0,GlassShots=0; double Started=0; float Phase=0,NearDamage=0,NearFootprint=0;
 ACombatTestTarget* Host=nullptr; ACombatTestTarget* Target=nullptr; ANeonCityPhysicsProp* Glass=nullptr;
};
class FAIWeapons : public IAutomationLatentCommand
{
public:
 explicit FAIWeapons(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>65) { Test->AddError(TEXT("AI weapon timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.IsolatePolice(); F.Place(ANeonCityCombatEncounter::PlayerTestLocation()); F.Player->GetHealthComponent()->ApplyPersistenceHealth(10000,10000);
   Encounter=F.W->SpawnActor<ANeonCityCombatEncounter>();
   if(!Test->TestEqual(TEXT("Existing encounter two NPCs"),Encounter->GetEnemies().Num(),2)) return true;
   A=Encounter->GetEnemies()[0]; B=Encounter->GetEnemies()[1];
   Test->TestTrue(TEXT("CombatAI accepts SMG"),A->GetCombatAI()->ConfigureWeapon(ANeonCitySMG::StaticClass()));
   Test->TestTrue(TEXT("CombatAI accepts rifle"),B->GetCombatAI()->ConfigureWeapon(ANeonCityRifle::StaticClass()));
   Stage=1; Phase=F.W->GetTimeSeconds(); return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto Next=[&](int32 S){Stage=S;Phase=Now;return false;};
  if(Stage==1 && Now-Phase>8)
  {
   Test->TestTrue(TEXT("Both AI use new weapons with real shots"),A->GetCombatAI()->GetShotsFired()>0 && B->GetCombatAI()->GetShotsFired()>0);
   auto* WA=A->FindComponentByClass<UCombatComponent>()->GetWeapon(); auto* WB=B->FindComponentByClass<UCombatComponent>()->GetWeapon();
   if(!Test->TestTrue(TEXT("Correct SMG/rifle live classes"),WA && WB && WA->GetData()->WeaponClass==ENeonCityWeaponClass::SMG && WB->GetData()->WeaponClass==ENeonCityWeaponClass::Rifle)) return true;
   Test->TestTrue(TEXT("AI recoil/spread does not remove finite accuracy"),A->GetCombatAI()->GetLastAimErrorDegrees()>0 && B->GetCombatAI()->GetLastAimErrorDegrees()>0);
   ShotsA=A->GetCombatAI()->GetShotsFired(); ShotsB=B->GetCombatAI()->GetShotsFired();
   F.Shot(TEXT("E-Mixed-NPC-Weapons.png"),FVector(-6200,2350,500),FVector(-5850,2850,120)); return Next(2);
  }
  if(Stage==2 && Now-Phase>2)
  {
   Test->TestTrue(TEXT("AI auto SMG never exceeds weapon FireRate"),A->GetCombatAI()->GetShotsFired()-ShotsA<=FMath::CeilToInt((Now-Phase)*10)+1);
   Test->TestTrue(TEXT("AI auto rifle never exceeds weapon FireRate"),B->GetCombatAI()->GetShotsFired()-ShotsB<=FMath::CeilToInt((Now-Phase)*6)+1);
   return Next(3);
  }
  if(Stage==3 && Now-Phase>22)
  {
   Test->TestTrue(TEXT("Both AI perform finite magazine reloads"),A->GetCombatAI()->GetReloadCount()>0 && B->GetCombatAI()->GetReloadCount()>0);
   Test->TestTrue(TEXT("Actual shots damage player"),F.Player->GetHealthComponent()->GetCurrentHealth()<10000);
   A->GetCombatAI()->Disengage(); B->GetCombatAI()->Disengage(); A->GetCombatAI()->Configure(nullptr); B->GetCombatAI()->Configure(nullptr);
   F.Player->GetHealthComponent()->ApplyPersistenceHealth(10000,10000); F.Inventory->GrantWeapon(NeonCityWeapons::Slot(3));
   F.Inventory->EquipSlot(3,F.Player->GetMesh(),TEXT("hand_r")); A->GetHealthComponent()->ApplyPersistenceHealth(500,500);
   A->SetActorLocation(F.Player->GetActorLocation()+FVector(500,0,0)); A->GetCharacterMovement()->DisableMovement();
   Before=A->GetHealthComponent()->GetCurrentHealth(); return Next(4);
  }
  if(Stage==4 && Now-Phase>.5f)
  {
   auto* Gun=F.Player->GetCombatComponent()->GetWeapon(); Test->TestTrue(TEXT("Player rifle fires back at actual NPC"),F.Player->GetCombatComponent()->Attack(F.Request(Gun,A->GetActorLocation())));
   Test->TestTrue(TEXT("Rifle damages shared NPC health"),A->GetHealthComponent()->GetCurrentHealth()<Before);
   const auto* Event=F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().FindByPredicate([&](const auto& E){return E.Victim.Get()==A && E.Instigator.Get()==F.PC;});
   Test->TestNotNull(TEXT("Rifle retains crime damage event"),Event);
   A->GetHealthComponent()->Die(); const auto Shots=A->GetCombatAI()->GetShotsFired();
   Test->TestTrue(TEXT("Dead NPC retained, cannot fight"),IsValid(A) && A->GetCombatAI()->GetHealthState()==ENeonCityCombatHealth::Dead && A->GetCombatAI()->GetShotsFired()==Shots);
   Encounter->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; int32 Stage=0,ShotsA=0,ShotsB=0; double Started=0; float Phase=0,Before=0;
 ANeonCityCombatEncounter* Encounter=nullptr; ANeonCityNPCCharacter* A=nullptr; ANeonCityNPCCharacter* B=nullptr;
};

class FPoliceWeapons : public IAutomationLatentCommand
{
public:
 explicit FPoliceWeapons(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>50) { Test->AddError(TEXT("Police weapons timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.IsolatePolice(); F.Place(FVector(-6100,1750,94)); F.Player->GetHealthComponent()->ApplyPersistenceHealth(5000,5000);
   Officer=F.W->SpawnActor<APoliceCharacter>(FVector(-6000,3000,94),FRotator(0,-90,0)); Officer->GetController()->SetActorTickEnabled(false);
   auto* P=NewObject<UCombatAIData>(Officer); P->Role=ENeonCityCombatRole::Security; P->Faction=ENeonCityFaction::Police; P->CanFight=true; P->RequiresArmedThreat=true; P->AllowChase=false; P->Accuracy=.85f; P->AimError=5;
   Officer->GetCombatAI()->Configure(P,166); F.Inventory->GrantWeapon(NeonCityWeapons::Slot(3)); Key(F.PC,EKeys::Three,IE_Pressed);
   Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto* AI=Officer->GetCombatAI(); AI->UpdateLocomotion(.02f);
  if(Stage==1 && Now-Phase>.5f)
  {
   Key(F.PC,EKeys::Three,IE_Released); PlayerWeapon=F.Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestTrue(TEXT("Ordinary slot input equips rifle"),PlayerWeapon.IsValid() && PlayerWeapon->IsEquipped())) return true;
   Key(F.PC,EKeys::RightMouseButton,IE_Pressed); F.Aim(Officer->GetActorLocation()+FVector(0,0,35));
   Phase=Now; Stage=11; return false;
  }
  if(Stage==11 && Now-Phase>.5f)
  { F.Aim(Officer->GetActorLocation()+FVector(0,0,35)); Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); Phase=Now; Stage=12; return false; }
  if(Stage==12 && Now-Phase>.065f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released); Key(F.PC,EKeys::RightMouseButton,IE_Released);
   Test->TestTrue(TEXT("Weapon attack still increases Wanted"),F.W->GetSubsystem<UWantedSubsystem>()->GetEvidenceScore()>=6);
   PlayerAmmo=PlayerWeapon->GetAmmo(); PlayerReserve=PlayerWeapon->GetReserveAmmo();
   Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && (AI->GetReloadCount()>0 || Now-Phase>25))
  {
   Test->TestTrue(TEXT("Police pistol combat reload retained"),AI->GetShotsFired()>=12 && AI->GetReloadCount()>0);
   Test->TestTrue(TEXT("Police projectiles actually damage player through unified system"),F.Player->GetHealthComponent()->GetCurrentHealth()<5000);
   Test->TestTrue(TEXT("Weapon x Police x Damage x Wanted preserves original player weapon"),PlayerWeapon.IsValid() && F.Player->GetCombatComponent()->GetWeapon()==PlayerWeapon.Get() && PlayerWeapon->IsEquipped() && !PlayerWeapon->IsHidden());
   Test->TestTrue(TEXT("Police hits never lose or duplicate player ammunition"),PlayerWeapon.IsValid() && PlayerWeapon->GetAmmo()==PlayerAmmo && PlayerWeapon->GetReserveAmmo()==PlayerReserve);
   Test->TestFalse(TEXT("Police hit reactions do not stow able player"),F.Player->GetCombatComponent()->IsInjuryStowed());
   auto* G=Officer->FindComponentByClass<UCombatComponent>()->GetWeapon();
   Test->TestTrue(TEXT("Ordinary police retain pistol"),G && G->GetData()->WeaponClass==ENeonCityWeaponClass::Pistol);
   F.Shot(TEXT("F-Police-Firefight.png"),FVector(-6250,2300,370),Officer->GetActorLocation());
   Shots=AI->GetShotsFired(); F.Place(FVector(-4400,3300,94)); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>2)
  {
   Test->TestEqual(TEXT("Police cannot fire through cafe"),AI->GetShotsFired(),Shots); Test->TestFalse(TEXT("LOS and last-known rules preserved"),AI->GetPerception().HasVisualContact);
   Officer->Destroy(); return true;
  }
  return false;
 }
 TWeakObjectPtr<AWeaponBase> PlayerWeapon; int32 PlayerAmmo=0,PlayerReserve=0;
 FAutomationTestBase* Test; FScene F; int32 Stage=0,Shots=0; double Started=0; float Phase=0; APoliceCharacter* Officer=nullptr;
};
class FPersistence : public IAutomationLatentCommand
{
public:
 FPersistence(FAutomationTestBase* T,FString InMode=TEXT("RoundTrip")):Test(T),Mode(MoveTemp(InMode)) {}
 TArray<FNeonCityWeaponSaveData> Expected()
 {
  TArray<FNeonCityWeaponSaveData> Data;
  const int32 M[]={7,19,13,2},R[]={31,64,52,8};
  for(int32 I=1;I<=4;++I) { auto E=GetDefault<AWeaponBase>(NeonCityWeapons::Resolve(NeonCityWeapons::Slot(I)))->BuildDefaultSaveData(); E.Ammo=M[I-1]; E.ReserveAmmo=R[I-1]; E.Equipped=I==3; Data.Add(E); }
  return Data;
 }
 void Verify()
 {
  const auto Actual=F.Inventory->BuildPersistenceSnapshot(); const auto Wanted=Expected();
  Test->TestEqual(TEXT("All four owned weapon records restored"),Actual.Num(),4);
  for(const auto& E:Wanted)
  {
   const auto* Found=Actual.FindByPredicate([&](const auto& A){return A.WeaponID==E.WeaponID;});
   if(!Test->TestNotNull(TEXT("Stable weapon ID restored"),Found)) continue;
   Test->TestTrue(TEXT("Owned, equipped and magazine/reserve exact"),Found->Owned && Found->Equipped==E.Equipped && Found->Ammo==E.Ammo && Found->ReserveAmmo==E.ReserveAmmo && Found->DataAssetID==E.DataAssetID);
  }
  Test->TestTrue(TEXT("Rifle equipped after load"),F.Inventory->GetEquippedWeaponID()==NeonCityWeapons::Slot(3));
  int32 Count=0; for(TActorIterator<AWeaponBase> It(F.W);It;++It) if(It->GetOwner()==F.Player) ++Count;
  Test->TestEqual(TEXT("Restore spawns only equipped weapon"),Count,1);
 }
 void Manifest(UNeonCitySaveGame* Data)
 {
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase17")),true);
  FString Entries;
  for(const auto& E:F.Inventory->BuildPersistenceSnapshot()) Entries+=FString::Printf(TEXT("%s:%d:%d:%d;"),*E.WeaponID.ToString(),E.Ammo,E.ReserveAmmo,E.Equipped?1:0);
  const FString Text=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"weapons\":\"%s\",\"equipped\":\"%s\"}"),FPlatformProcess::GetCurrentProcessId(),*Data->SaveID.ToString(),*Entries,*F.Inventory->GetEquippedWeaponID().ToString());
  Test->TestTrue(TEXT("Weapon cross-session manifest written"),FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Phase17")/(TEXT("Session")+Mode+TEXT(".json")))));
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>40) { Test->AddError(TEXT("Weapon persistence timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.IsolatePolice();
   F.Save->ConfigureSlot(Mode==TEXT("RoundTrip")?TEXT("NeonCity_Test_Phase17_RoundTrip"):TEXT("NeonCity_Test_Phase17_CrossSession"));
   if(Mode!=TEXT("B"))
   {
    F.Save->DeleteSave(); if(!Test->TestTrue(TEXT("Create new game retains design defaults"),F.Save->CreateNewGameSave())) { Test->AddError(F.Save->GetLastDetail()); return true; }
    Test->TestEqual(TEXT("New game starter pistol only"),F.Inventory->BuildPersistenceSnapshot().Num(),1);
    F.Wallet->AddMoney(10000,TEXT("PersistenceWeaponPurchase"));
    for(int32 I=2;I<=4;++I) Test->TestTrue(TEXT("Purchase persistent weapon"),F.Inventory->PurchaseWeapon(NeonCityWeapons::Slot(I)));
    F.Player->GetCombatAdapter()->ApplyPersistenceSnapshot(Expected()); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
   }
   if(!Test->TestTrue(TEXT("Session B sees A's disk save"),F.Save->DoesSaveExist())) return true;
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection()); if(!Test->TestNotNull(TEXT("Cross-process disk model readable"),Data.Get())) return true;
   FString Previous; FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Phase17/SessionA.json")));
   uint32 PID=0; FParse::Value(*Previous,TEXT("\"processId\":"),PID);
   Test->TestTrue(TEXT("Different OS process and matching SaveID"),PID && PID!=FPlatformProcess::GetCurrentProcessId() && Previous.Contains(Data->SaveID.ToString()));
   Test->TestTrue(TEXT("Session B loads"),F.Save->LoadGame()); Verify(); Manifest(Data.Get());
   F.Shot(TEXT("G-SessionB-Restored.png"),F.Player->GetActorLocation()+FVector(-200,-300,200),F.Player->GetActorLocation());
   F.Save->DeleteSave(); return true;
  }
  if(Stage==1 && F.W->GetTimeSeconds()-Phase>.7f)
  {
   if(!Test->TestTrue(TEXT("Save four weapon inventory to disk"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   TStrongObjectPtr<UNeonCitySaveGame> Data(F.Save->ReadSaveForInspection()); Verify();
   if(Mode==TEXT("A")) { Manifest(Data.Get()); F.Shot(TEXT("G-SessionA-Saved.png"),F.Player->GetActorLocation()+FVector(-200,-300,200),F.Player->GetActorLocation()); return true; }
   F.Player->GetCombatAdapter()->ApplyPersistenceSnapshot({});
   Test->TestTrue(TEXT("Round trip load"),F.Save->LoadGame()); Verify();
   // Released Version 1 wire fields remain identical. Missing weapon section uses the original pistol defaults.
   Data->HasWeapons=false; Data->Weapons.Reset(); Test->TestTrue(TEXT("Write absent legacy weapons"),F.Save->WriteTestFixture(Data.Get()));
   Test->TestTrue(TEXT("Legacy Version 1 safely defaults"),F.Save->LoadGame());
   Test->TestTrue(TEXT("Legacy default pistol owned"),F.Inventory->Owns(NeonCityWeapons::Slot(1)) && F.Inventory->BuildPersistenceSnapshot().Num()==1);
   Data->HasWeapons=true; Data->Weapons=Expected(); Data->Weapons[0].DataAssetID=NAME_None; Data->Weapons[0].Equipped=true;
   FNeonCityWeaponSaveData Invalid; Invalid.WeaponID=TEXT("PIE_TransientWeapon"); Data->Weapons.Add(Invalid); const auto Duplicate=Data->Weapons[1]; Data->Weapons.Add(Duplicate);
   Test->TestTrue(TEXT("Write malformed entries fixture"),F.Save->WriteTestFixture(Data.Get()));
   Test->TestTrue(TEXT("Invalid IDs and duplicate records safely isolated"),F.Save->LoadGame());
   const auto Actual=F.Inventory->BuildPersistenceSnapshot(); int32 Equipped=0; for(const auto& E:Actual) if(E.Equipped) ++Equipped;
   Test->TestEqual(TEXT("Only four valid unique IDs"),Actual.Num(),4); Test->TestEqual(TEXT("Multiple equipped normalized to one"),Equipped,1);
   F.Save->DeleteSave(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; FString Mode; int32 Stage=0; double Started=0; float Phase=0;
};
}
#define WEAPON_TEST(Class,Name,Command) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString& P) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand()); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWeaponTests::Command>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
WEAPON_TEST(FWeaponStore,"NeonCity.Weapons.StorePIE",FStore)
WEAPON_TEST(FWeaponInput,"NeonCity.Weapons.InputAmmoPIE",FInputAmmo)
WEAPON_TEST(FWeaponBallistics,"NeonCity.Weapons.BallisticsPIE",FBallistics)
WEAPON_TEST(FWeaponAI,"NeonCity.Weapons.AIWeaponsPIE",FAIWeapons)
WEAPON_TEST(FWeaponPolice,"NeonCity.Weapons.PoliceWeaponsPIE",FPoliceWeapons)
WEAPON_TEST(FWeaponPersistence,"NeonCity.Weapons.PersistencePIE",FPersistence)
#undef WEAPON_TEST
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FWeaponCrossSession,"NeonCity.Weapons.CrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FWeaponCrossSession::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 FString Mode; FParse::Value(FCommandLine::Get(),TEXT("NeonCityWeaponSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")) { Names.Add(Mode); Commands.Add(Mode); }
}
bool FWeaponCrossSession::RunTest(const FString& P)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWeaponTests::FPersistence>(this,P));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
