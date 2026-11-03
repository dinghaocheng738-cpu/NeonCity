#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/NeonCityCombatEncounter.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/Combat/CoverPoint.h"
#include "AI/Combat/CoverQuery.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCData.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/CombatContextSubsystem.h"
#include "Combat/DamageSystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAIProfile,"NeonCity.CombatAI.ProfileInvariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCombatAIProfile::RunTest(const FString& Parameters)
{
 auto* Data=NewObject<UCombatAIData>();
 TestTrue(TEXT("Civilian defaults are valid and cannot fight"),Data->IsValidProfile() && !Data->CanFight && Data->Role==ENeonCityCombatRole::Civilian);
 Data->CanFight=true; TestFalse(TEXT("Civilian role cannot opt in accidentally"),Data->IsValidProfile());
 Data->Role=ENeonCityCombatRole::Security; TestTrue(TEXT("Explicit security capability"),Data->IsValidProfile());
 Data->Accuracy=1; TestFalse(TEXT("Perfect accuracy disallowed"),Data->IsValidProfile()); Data->Accuracy=.6f;
 Data->ReactionTime=0; TestFalse(TEXT("Instant reactions disallowed"),Data->IsValidProfile()); Data->ReactionTime=.8f;
 Data->PerceptionInterval=.001f; TestFalse(TEXT("Expensive per-frame sensing disallowed"),Data->IsValidProfile()); Data->PerceptionInterval=.2f;
 Data->AimError=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Non-finite profile refused"),Data->IsValidProfile());
 return true;
}
namespace NeonCityCombatAITests
{
void Key(APlayerController* PC,FKey Value,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Value,Event,FPlatformTime::Cycles64())); }
void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityCombatAICapture"))) return;
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase14")),true);
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase14")/Name,true,false);
}
struct FWorld
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  return Player && W->HasBegunPlay() && Player->GetCharacterMovement()->IsMovingOnGround() && W->GetTimeSeconds()>1;
 }
 void Place(FVector Point) { Player->SetActorLocation(Point,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately(); }
 void Aim(AActor* Target)
 { FVector Eye; FRotator R; PC->GetPlayerViewPoint(Eye,R); PC->SetControlRotation((Target->GetActorLocation()+FVector(0,0,55)-Eye).Rotation()); }
};
class FCover : public IAutomationLatentCommand
{
public:
 explicit FCover(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Cover PIE timeout")); return true; }
  if(!Encounter)
  {
   if(!F.Ready()) return false;
   for(TActorIterator<ANeonCityNPCCharacter> It(F.W);It;++It)
   {
    Test->TestFalse(TEXT("Original civilians have no combat tick"),It->GetCombatAI()->IsComponentTickEnabled());
    It->GetNPCState()->SetState(ENeonCityNPCState::Fight);
    Test->TestTrue(TEXT("Civilian Fight request refused"),It->GetNPCState()->GetState()!=ENeonCityNPCState::Fight);
   }
   F.Place(ANeonCityCombatEncounter::PlayerTestLocation()); Encounter=F.W->SpawnActor<ANeonCityCombatEncounter>();
   for(auto NPC:Encounter->GetEnemies()) NPC->GetCombatAI()->Configure(nullptr);
   Phase=F.W->GetTimeSeconds(); return false;
  }
  if(F.W->GetTimeSeconds()-Phase<.5f) return false;
  const auto& Enemies=Encounter->GetEnemies(); const auto& Points=Encounter->GetCovers();
  if(!Test->TestTrue(TEXT("Two explicit enemies and two cover stances"),Enemies.Num()==2 && Points.Num()==2)) return true;
  const FVector Threat=F.Player->GetActorLocation()+FVector(0,0,35);
  Test->TestTrue(TEXT("Standing cover blocks the current threat"),Points[0]->ProvidesCover(Enemies[0],Threat));
  Test->TestTrue(TEXT("Crouched cover blocks crouched height"),Points[1]->ProvidesCover(Enemies[1],Threat));
  Points[1]->Stance=ENeonCityCoverStance::Standing;
  Test->TestFalse(TEXT("Low wall is not standing cover"),Points[1]->ProvidesCover(Enemies[1],Threat)); Points[1]->Stance=ENeonCityCoverStance::Crouched;
  Test->TestTrue(TEXT("First reservation succeeds"),Points[0]->Reserve(Enemies[0],Threat));
  Test->TestFalse(TEXT("Second NPC cannot reserve occupied cover"),Points[0]->Reserve(Enemies[1],Threat));
  Points[0]->Release(Enemies[1]); Test->TestTrue(TEXT("Only occupant can release"),Points[0]->GetOccupant()==Enemies[0]);
  Points[0]->Release(Enemies[0]); Test->TestTrue(TEXT("Release allows another owner"),Points[0]->Reserve(Enemies[1],Threat)); Points[0]->Release(Enemies[1]);
  Test->TestFalse(TEXT("Wrong enemy direction invalidates cover"),Points[0]->ProvidesCover(Enemies[0],Points[0]->GetActorLocation()+FVector(0,600,35)));
  FVector Peek; Test->TestTrue(TEXT("Reachable peek has a genuine line of fire"),Points[0]->FindPeek(Enemies[0],Threat,Peek));
  auto* Best=NeonCityCover::FindAndReserve(Enemies[0],Threat,1100);
  Test->TestNotNull(TEXT("Cover query selects physically valid reachable cover"),Best); if(Best) Best->Release(Enemies[0]);
  Encounter->Destroy(); return true;
 }
 FAutomationTestBase* Test; FWorld F; ANeonCityCombatEncounter* Encounter=nullptr; double Started=0; float Phase=0;
};
class FEncounter : public IAutomationLatentCommand
{
public:
 explicit FEncounter(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100) { Test->AddError(FString::Printf(TEXT("Encounter timeout stage %d"),Stage)); Dump(); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   F.Place(ANeonCityCombatEncounter::PlayerTestLocation()); F.PC->SetControlRotation(FRotator(0,90,0));
   F.Player->GetHealthComponent()->SetMaxHealth(1500); F.Player->GetHealthComponent()->Heal(1500);
   Encounter=F.W->SpawnActor<ANeonCityCombatEncounter>();
   if(!Test->TestEqual(TEXT("Encounter creates exactly two armed hostiles"),Encounter->GetEnemies().Num(),2)) return true;
   A=Encounter->GetEnemies()[0]; B=Encounter->GetEnemies()[1]; Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto* CA=A->GetCombatAI(); auto* CB=B->GetCombatAI();
  if(Stage==4 || Stage==5) F.Aim(A);
  auto Next=[&](int32 N){ UE_LOG(LogTemp,Display,TEXT("CombatAI encounter %d -> %d shots=%d/%d"),Stage,N,CA->GetShotsFired(),CB->GetShotsFired()); Stage=N; Phase=Now; return false; };
  if(Stage==1 && Now-Phase>7)
  {
   Test->TestTrue(TEXT("Both combat NPCs acquire the actual player"),CA->GetTarget()==F.Player && CB->GetTarget()==F.Player);
   Test->TestTrue(TEXT("Shared faction prevents hostile-on-hostile targeting"),UCombatTargetComponent::AreFriendly(A,B));
   const uint32 CoverBit=1u<<uint8(ENeonCityCombatAIState::MoveToCover);
   Test->TestTrue(TEXT("Both NPCs move to validated cover"),(CA->GetVisitedStates()&CoverBit) && (CB->GetVisitedStates()&CoverBit));
   Test->TestTrue(TEXT("Combatants reserve separate anchors"),CA->GetCover() && CB->GetCover() && CA->GetCover()!=CB->GetCover());
   Test->TestTrue(TEXT("Both NPCs fire real weapons"),CA->GetShotsFired()>0 && CB->GetShotsFired()>0);
   Test->TestTrue(TEXT("Real NPC weapon causes player damage"),F.Player->GetHealthComponent()->GetCurrentHealth()<1500);
   Test->TestTrue(TEXT("Aim error is nonzero"),CA->GetLastAimErrorDegrees()>0 && CB->GetLastAimErrorDegrees()>0);
   Test->TestTrue(TEXT("Queries are throttled"),CA->GetPerceptionQueries()<50 && CA->GetCoverQueries()<12);
   Test->TestEqual(TEXT("NPC attacking player does not create player Wanted"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),0);
   Shot(TEXT("CombatAI-Exchange.png")); return Next(2);
  }
  if(Stage==2 && Now-Phase>10)
  {
   if(!F.Player->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()) return false;
   Test->TestTrue(TEXT("Both NPCs must perform real magazine reloads"),CA->GetReloadCount()>0 && CB->GetReloadCount()>0);
   Test->TestTrue(TEXT("Shots consume finite reserve"),A->FindComponentByClass<UCombatComponent>()->GetWeapon() && A->FindComponentByClass<UCombatComponent>()->GetWeapon()->GetReserveAmmo()<48);
   Key(F.PC,EKeys::One,IE_Pressed); return Next(3);
  }
  if(Stage==3 && Now-Phase>.2f)
  {
   Key(F.PC,EKeys::One,IE_Released);
   if(!F.Player->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()) return false;
   Test->TestTrue(TEXT("Player equips weapon during NPC reload window"),F.Player->GetCombatComponent()->GetWeapon() && F.Player->GetCombatComponent()->GetWeapon()->IsEquipped());
   Key(F.PC,EKeys::RightMouseButton,IE_Pressed); F.Aim(A); return Next(4);
  }
  if(Stage==4 && Now-Phase>.6f) { F.Aim(A); EnemyHealth=A->GetHealthComponent()->GetCurrentHealth(); return Next(5); }
  if(Stage==5 && Now-Phase>.2f)
  {
   if(!F.Player->FindComponentByClass<UDamageableCharacterComponent>()->CanAct()) return false;
   Key(F.PC,EKeys::LeftMouseButton,IE_Pressed); return Next(6);
  }
  if(Stage==6 && Now-Phase>.1f)
  {
   Key(F.PC,EKeys::LeftMouseButton,IE_Released);
   if(auto* Weapon=F.Player->GetCombatComponent()->GetWeapon())
    Test->AddInfo(FString::Printf(TEXT("Player return fire: hit=%s at=%s damage=%.1f NPC=%s aiming=%d"),*GetNameSafe(Weapon->GetLastHit().GetActor()),*Weapon->GetLastHit().ImpactPoint.ToString(),Weapon->GetLastAppliedDamage(),*A->GetActorLocation().ToString(),F.Player->GetCombatAdapter()->IsAiming()));
   Test->TestTrue(TEXT("Actual player LMB returns fire and damages combat NPC"),A->GetHealthComponent()->GetCurrentHealth()<EnemyHealth);
   Test->TestTrue(TEXT("Real NPC hit enters the shared HitReaction"),A->FindComponentByClass<UDamageableCharacterComponent>()->GetBodyState()==ENeonCityBodyState::HitReaction);
   const auto* Event=F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().FindByPredicate([&](const auto& E){ return E.Victim.Get()==A && E.Instigator.Get()==F.PC; });
   Test->TestTrue(TEXT("Retaliation remains a crime-ledger event with original aggression context"),Event && Event->CrimeType==ENeonCityCrimeType::DamageNPC && Event->CombatContextID.IsValid() && Event->Disposition==ENeonCityAttackDisposition::ResponseToAggression && Event->OriginalAggressor.Get()==A);
   Test->TestTrue(TEXT("Response does not add assault score; public gunfire remains eligible"),F.W->GetSubsystem<UWantedSubsystem>()->GetEvidenceScore()<=1);
   Shot(TEXT("CombatAI-PlayerReturnFire.png"));
   return Next(61);
  }
  if(Stage==61 && Now-Phase>.5f)
  {
   Key(F.PC,EKeys::RightMouseButton,IE_Released);
   FNeonCityDamageEvent Hit; Hit.DamageAmount=1; Hit.DamageType=ENeonCityDamageType::Administrative;
   F.W->GetSubsystem<UDamageSystem>()->ApplyDamage(F.Player,Hit); return Next(62);
  }
  if(Stage==62 && Now-Phase>.1f)
  {
   Test->TestTrue(TEXT("Transient player HitReaction preserves equipped weapon"),F.Player->GetCombatComponent()->GetWeapon() && F.Player->GetCombatComponent()->GetWeapon()->IsEquipped());
   F.Place(ANeonCityCombatEncounter::HiddenTestLocation()); return Next(7);
  }
  if(Stage==7 && Now-Phase>.5f)
  {
   Test->TestFalse(TEXT("Existing cafe building blocks sight"),NeonCityCover::ClearSight(F.W,A,A->GetPawnViewLocation(),F.Player->GetActorLocation()+FVector(0,0,35),F.Player));
   Test->TestFalse(TEXT("Lost target visual contact"),CA->GetPerception().HasVisualContact);
   Known=CA->GetPerception().LastKnownTargetLocation; Shots=CA->GetShotsFired()+CB->GetShotsFired(); PlayerHealth=F.Player->GetHealthComponent()->GetCurrentHealth();
   F.Place(ANeonCityCombatEncounter::HiddenTestLocation()+FVector(400,0,0)); return Next(8);
  }
  if(Stage==8 && Now-Phase>1.5f)
  {
   Test->TestTrue(TEXT("Hidden route changes do not update LastKnownLocation"),CA->GetPerception().LastKnownTargetLocation.Equals(Known));
   Test->TestEqual(TEXT("No shots through cafe walls"),CA->GetShotsFired()+CB->GetShotsFired(),Shots);
   Test->TestEqual(TEXT("No through-wall damage"),F.Player->GetHealthComponent()->GetCurrentHealth(),PlayerHealth);
   Test->TestTrue(TEXT("Search uses recorded location"),!CA->IsMovingToGoal() || CA->GetMovementGoal().Equals(Known));
   Shot(TEXT("CombatAI-Hidden.png")); return Next(9);
  }
  if(Stage==9 && Now-Phase>7)
  {
   Test->TestTrue(TEXT("Bounded search disengages both enemies"),!CA->ControlsLocomotion() && !CB->ControlsLocomotion() && !CA->GetTarget() && !CB->GetTarget());
   Test->TestTrue(TEXT("Disengagement releases both cover reservations"),!CA->GetCover() && !CB->GetCover());
   // Reacquire from a visible location, then verify low-health decisions, Downed and Dead separately.
   F.Place(ANeonCityCombatEncounter::PlayerTestLocation()); return Next(10);
  }
  if(Stage==10 && Now-Phase>5)
  {
   Test->TestTrue(TEXT("Visible player can be reacquired after search"),CA->GetTarget()==F.Player || CB->GetTarget()==F.Player);
   A->GetHealthComponent()->SetCurrentHealth(30);
   Test->TestTrue(TEXT("Critical health distinct from Downed"),CA->GetHealthState()==ENeonCityCombatHealth::Critical);
   return Next(11);
  }
  if(Stage==11 && Now-Phase>.7f)
  {
   Test->TestTrue(TEXT("Low health visits Retreat"),(CA->GetVisitedStates()&(1u<<uint8(ENeonCityCombatAIState::Retreat)))!=0);
   A->GetHealthComponent()->SetCurrentHealth(15);
   Test->TestTrue(TEXT("Downed stops combat and releases target/cover"),CA->GetState()==ENeonCityCombatAIState::Downed && !CA->IsComponentTickEnabled() && !CA->GetTarget() && !CA->GetCover());
   Test->TestFalse(TEXT("Downed actor retained"),A->IsActorBeingDestroyed());
   A->GetHealthComponent()->Die(); B->GetHealthComponent()->Die(); Shots=CA->GetShotsFired()+CB->GetShotsFired(); Queries=CA->GetPerceptionQueries()+CB->GetPerceptionQueries();
   return Next(12);
  }
  if(Stage==12 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Dead actors remain with explicit terminal state"),!A->IsActorBeingDestroyed() && !B->IsActorBeingDestroyed() && CA->GetState()==ENeonCityCombatAIState::Dead && CB->GetState()==ENeonCityCombatAIState::Dead);
   Test->TestEqual(TEXT("Death stops all firing"),CA->GetShotsFired()+CB->GetShotsFired(),Shots);
   Test->TestEqual(TEXT("Death stops expensive perception"),CA->GetPerceptionQueries()+CB->GetPerceptionQueries(),Queries);
   Test->TestFalse(TEXT("Death stops help responses"),A->GetThreatComponent()->HasActiveThreat() || B->GetThreatComponent()->HasActiveThreat());
   auto* Save=F.W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); Save->ConfigureSlot(TEXT("NeonCity_Test_Phase14_Context")); Save->DeleteSave();
   Test->TestTrue(TEXT("Existing new-game persistence remains available after encounter"),Save->CreateNewGameSave());
   Test->TestTrue(TEXT("Existing Load restores core state and clears transient combat context"),Save->LoadGame()); Save->DeleteSave();
   Encounter->Destroy(); return true;
  }
  return false;
 }
 void Dump()
 { if(A) Test->AddInfo(FString::Printf(TEXT("A state %d shots %d cover %s pos %s"),int32(A->GetCombatAI()->GetState()),A->GetCombatAI()->GetShotsFired(),*GetNameSafe(A->GetCombatAI()->GetCover()),*A->GetActorLocation().ToString())); }
 FAutomationTestBase* Test; FWorld F; ANeonCityCombatEncounter* Encounter=nullptr; ANeonCityNPCCharacter *A=nullptr,*B=nullptr;
 int32 Stage=0,Shots=0,Queries=0; double Started=0; float Phase=0,EnemyHealth=0,PlayerHealth=0; FVector Known;
};
class FReactive : public IAutomationLatentCommand
{
public:
 explicit FReactive(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Reactive capability PIE timeout")); return true; }
  if(!NPC)
  {
   if(!F.Ready()) return false;
   F.Place(ANeonCityCombatEncounter::PlayerTestLocation());
   F.Player->GetHealthComponent()->SetMaxHealth(1500); F.Player->GetHealthComponent()->Heal(1500);
   auto* Profile=NewObject<UCombatAIData>(F.W); Profile->CanFight=true; Profile->Role=ENeonCityCombatRole::Security;
   Profile->RetreatWhenInjured=false; Profile->ReactionTime=1;
   auto* NPCProfile=NewObject<UNPCData>(F.W); NPCProfile->CombatProfile=Profile;
   const FTransform Spawn(FRotator(0,-90,0),FVector(-6250,2400,94));
   NPC=F.W->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),Spawn);
   NPC->InitializePedestrian(TEXT("ReactiveSecurity"),NPCProfile,{},21); NPC->FinishSpawning(Spawn);
   const FTransform BuddySpawn(FVector(-5800,2400,94));
   Buddy=F.W->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),BuddySpawn);
   Buddy->InitializePedestrian(TEXT("ReactiveCivilian"),nullptr,{},22); Buddy->FinishSpawning(BuddySpawn);
   Phase=F.W->GetTimeSeconds(); return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto* AI=NPC->GetCombatAI();
  if(Stage==0 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Security is reactive and has no idle combat tick"),AI->CanFight() && !AI->GetTarget() && !AI->IsComponentTickEnabled());
   F.W->GetSubsystem<UThreatSubsystem>()->EmitGunshot(Buddy,Buddy->GetController(),Buddy->GetActorLocation());
   Test->TestFalse(TEXT("Friendly gunshot does not provoke security combat"),AI->ControlsLocomotion());
   F.W->GetSubsystem<UThreatSubsystem>()->EmitGunshot(F.Player,F.PC,F.Player->GetActorLocation());
   Test->TestTrue(TEXT("Observed dangerous player wakes reactive security"),AI->GetTarget()==F.Player && AI->ControlsLocomotion());
   Test->TestEqual(TEXT("Reaction delay prevents instant fire"),AI->GetShotsFired(),0);
   NPC->GetHealthComponent()->SetCurrentHealth(40);
   Test->TestTrue(TEXT("Injured health classification"),AI->GetHealthState()==ENeonCityCombatHealth::Injured);
   Stage=1; Phase=Now; return false;
  }
  if(Stage==1 && Now-Phase>1.8f)
  {
   Test->TestTrue(TEXT("Configured injured security can continue fighting"),AI->GetShotsFired()>0 && !(AI->GetVisitedStates()&(1u<<uint8(ENeonCityCombatAIState::Retreat))));
   AI->SetDistantLOD(true); Stage=2; Phase=Now; return false;
  }
  if(Stage==2 && Now-Phase>.3f)
  {
   Test->TestTrue(TEXT("Distant LOD reduces decision frequency"),AI->GetComponentTickInterval()>=1.f);
   const auto CivilianState=Buddy->GetNPCState()->GetState(); const float Speed=Buddy->GetCharacterMovement()->MaxWalkSpeed;
   auto* Save=F.W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); Save->ConfigureSlot(TEXT("NeonCity_Test_Phase14_Reactive")); Save->DeleteSave();
   Test->TestTrue(TEXT("New-game apply clears active combat"),Save->CreateNewGameSave());
   const auto* Weapon=NPC->FindComponentByClass<UCombatComponent>()->GetWeapon();
   Test->TestTrue(TEXT("Load clears active targets, weapons and reservations"),!AI->ControlsLocomotion() && !AI->GetTarget() && !AI->GetCover() && !AI->IsComponentTickEnabled() && Weapon && !Weapon->IsEquipped());
   Test->TestTrue(TEXT("Load does not overwrite civilian ecology"),Buddy->GetNPCState()->GetState()==CivilianState && Buddy->GetCharacterMovement()->MaxWalkSpeed==Speed);
   Test->TestEqual(TEXT("Transient NPC health is not part of player save"),NPC->GetHealthComponent()->GetCurrentHealth(),40.f);
   Save->DeleteSave(); AI->SetDistantLOD(false); Stage=3; Phase=Now; return false;
  }
  if(Stage==3 && Now-Phase>3.1f)
  {
   F.Place(ANeonCityCombatEncounter::HiddenTestLocation());
   Test->TestFalse(TEXT("Unseen injury fixture blocks source sight"),NeonCityCover::ClearSight(F.W,NPC,NPC->GetPawnViewLocation(),F.Player->GetActorLocation()+FVector(0,0,35),F.Player));
   FNeonCityDamageEvent Damage; Damage.DamageAmount=1; Damage.Instigator=F.PC; Damage.SourceActor=F.Player; Damage.HitLocation=NPC->GetActorLocation();
   F.W->GetSubsystem<UDamageSystem>()->ApplyDamage(NPC,Damage);
   Test->TestTrue(TEXT("Unseen injury records incident point without live source coordinates"),AI->GetTarget()==F.Player && !AI->GetPerception().HasVisualContact && AI->GetPerception().LastKnownTargetLocation.Equals(Damage.HitLocation));
   AI->Disengage(); NPC->Destroy(); Buddy->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FWorld F; ANeonCityNPCCharacter *NPC=nullptr,*Buddy=nullptr;
 double Started=0; float Phase=0; int32 Stage=0;
};
class FCivilian : public IAutomationLatentCommand
{
public:
 explicit FCivilian(FAutomationTestBase* In):Test(In) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>50) { Test->AddError(TEXT("Civilian cooldown timeout")); return true; }
  if(!Civilian)
  {
   if(!F.Ready()) return false;
   FTransform Spawn(FVector(-5800,1900,94));
   Civilian=F.W->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),Spawn);
   Civilian->InitializePedestrian(TEXT("CooldownWitness"),nullptr,{FVector(-6250,2100,94),FVector(-6250,3000,94)},50); Civilian->FinishSpawning(Spawn);
   Source=F.W->SpawnActor<AActor>(); Source->SetActorLocation(FVector(-5600,1800,120));
   Phase=F.W->GetTimeSeconds(); Origin=Civilian->GetActorLocation(); return false;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Emitted<7 && Now-Phase>Emitted+.5f)
  { F.W->GetSubsystem<UThreatSubsystem>()->EmitGunshot(Source,nullptr,FVector(-5600,1800,120)); ++Emitted; Last=Now; }
  if(Emitted<7) return false;
  if(!Checked && Now-Last>6.5f)
  {
   Test->TestTrue(TEXT("Civilian flees away from sustained gunfire"),FVector::Dist2D(Origin,Civilian->GetActorLocation())>150);
   Test->TestTrue(TEXT("Sustained-danger cooldown exceeds ordinary quiet timeout"),Civilian->GetThreatComponent()->HasActiveThreat() && Civilian->GetThreatComponent()->GetDangerUntil()>Now);
   Test->TestTrue(TEXT("No Social or Fight while danger cooldown remains"),Civilian->GetNPCState()->GetState()!=ENeonCityNPCState::Social && Civilian->GetNPCState()->GetState()!=ENeonCityNPCState::Fight);
   Test->TestFalse(TEXT("Civilian does not run combat tick or own a weapon"),Civilian->GetCombatAI()->IsComponentTickEnabled() || Civilian->FindComponentByClass<UCombatComponent>()->GetWeapon()!=nullptr);
   Checked=true;
  }
  if(Checked && Now-Last>13)
  {
   Test->TestFalse(TEXT("Quiet eventually restores civilian ecology"),Civilian->GetThreatComponent()->HasActiveThreat());
   Civilian->Destroy(); Source->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FWorld F; ANeonCityNPCCharacter* Civilian=nullptr; AActor* Source=nullptr; FVector Origin;
 double Started=0; float Phase=0,Last=0; int32 Emitted=0; bool Checked=false;
};
}
#define NEONCITY_AI_PIE_TEST(ClassName,TestName,Command) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName,TestName,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool ClassName::RunTest(const FString& Parameters) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityCombatAITests::Command>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
NEONCITY_AI_PIE_TEST(FCombatAICover,"NeonCity.CombatAI.CoverPIE",FCover)
NEONCITY_AI_PIE_TEST(FCombatAIEncounter,"NeonCity.CombatAI.EncounterPIE",FEncounter)
NEONCITY_AI_PIE_TEST(FCombatAICivilian,"NeonCity.CombatAI.CivilianDangerPIE",FCivilian)
NEONCITY_AI_PIE_TEST(FCombatAIReactive,"NeonCity.CombatAI.ReactiveCapabilityPIE",FReactive)
#undef NEONCITY_AI_PIE_TEST
#endif
