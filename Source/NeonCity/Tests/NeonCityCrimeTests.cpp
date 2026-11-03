#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Crime/CrimeSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
namespace NeonCityCrimeTests
{
class FVerify : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if (!Started) { Started=FPlatformTime::Seconds(); }
  if (FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Crime PIE timed out")); return true; }
  UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
  APlayerController* PC=World ? World->GetFirstPlayerController() : nullptr;
  if (!PC || !PC->GetPawn() || !World->HasBegunPlay()) { return false; }
  const float Now=World->GetTimeSeconds();
  auto Next=[&](int32 Value) { Stage=Value; Phase=Now; return false; };
  auto* Crime=World->GetSubsystem<UCrimeSubsystem>(); auto* Threat=World->GetSubsystem<UThreatSubsystem>();
  auto* Damage=World->GetSubsystem<UDamageSystem>();
  if (Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
   if (!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround()) { return false; }
   Test->TestEqual(TEXT("No ambient behavior is recorded as crime"),Crime->GetEvents().Num(),0);
   for (TActorIterator<ANeonCityBaseVehicle> It(World); It; ++It) { if(It->bStartsOwned) { Car=*It; break; } }
   for (TActorIterator<ANeonCityNPCCharacter> It(World); It; ++It)
   { if (It->GetActorLocation().X<-3000) { FarNPC=*It; break; } }
   if (!Test->TestNotNull(TEXT("Sedan exists"),Car.Get()) || !Test->TestNotNull(TEXT("Distant civilian exists"),FarNPC.Get())) { return true; }
   // Additional deterministic test civilians exist only in this PIE world, not in the authored map.
   Victim=SpawnNPC(World,FVector(2350,-950,132),TEXT("CrimeTestVictim"),{FVector(1900,-950,132),FVector(3400,-950,132)});
   Witness=SpawnNPC(World,FVector(2550,-750,132),TEXT("CrimeTestWitness"),{FVector(2300,-750,132),FVector(3400,-750,132)});
   Player->SetActorLocationAndRotation(FVector(1850,-950,136),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation(FRotator::ZeroRotator); Player->GetCharacterMovement()->StopMovementImmediately();
   return Next(1);
  }
  if (Stage==1 && Now-Phase>.3f) { Key(PC,EKeys::One,IE_Pressed); return Next(2); }
  if (Stage==2 && Now-Phase>.2f)
  {
   Key(PC,EKeys::One,IE_Released); Weapon=Player->GetCombatComponent()->GetWeapon();
   if (!Test->TestNotNull(TEXT("Real player weapon equipped"),Weapon.Get())) { return true; }
   Key(PC,EKeys::RightMouseButton,IE_Pressed); return Next(3);
  }
  if (Stage==3 && Now-Phase>.5f) { AimAt(PC,Victim->GetActorLocation()); return Next(4); }
  if (Stage==4 && Now-Phase>.2f) { AimAt(PC,Victim->GetActorLocation()); return Next(5); }
  if (Stage==5 && Now-Phase>.2f)
  {
   Origin=Victim->GetActorLocation(); Key(PC,EKeys::LeftMouseButton,IE_Pressed); return Next(6);
  }
  if (Stage==6 && Now-Phase>.06f)
  {
   Key(PC,EKeys::LeftMouseButton,IE_Released);
   if (!Test->TestEqual(TEXT("Real LMB hits NPC through shared damage"),Victim->GetHealthComponent()->GetCurrentHealth(),75.f)) { return true; }
   Test->TestTrue(TEXT("NPC HitReaction"),Body(Victim.Get())->GetBodyState()==ENeonCityBodyState::HitReaction);
   const auto* Event=Crime->GetEvents().FindByPredicate([&](const FNeonCityCrimeEvent& E) { return E.CrimeType==ENeonCityCrimeType::DamageNPC && E.Victim==Victim; });
   if (!Test->TestNotNull(TEXT("NPC damage recorded"),Event)) { return true; }
   IncidentId=Event->Id;
   Test->TestTrue(TEXT("Crime location and instigator"),Event->Instigator.Get()==PC && FVector::Dist(Event->Location,Origin)<20);
   Test->TestTrue(TEXT("Gunshot also produces threat event"),Crime->GetEvents().ContainsByPredicate([](const FNeonCityCrimeEvent& E) { return E.CrimeType==ENeonCityCrimeType::ThreatEvent; }));
   Test->TestTrue(TEXT("Nearby witness perceived attack"),Witness->GetThreatComponent()->HasPerceived(IncidentId));
   Test->TestFalse(TEXT("Distant NPC did not perceive attack"),FarNPC->GetThreatComponent()->HasPerceived(IncidentId));
   Test->TestFalse(TEXT("Unaware NPC cannot fabricate report"),Crime->ReportEvent(IncidentId,FarNPC.Get()));
   Test->TestFalse(TEXT("Unknown event cannot be reported"),Crime->ReportEvent(FGuid::NewGuid(),Victim.Get()));
   return Next(7);
  }
  if (Stage==7 && Now-Phase>2.f)
  {
   const uint32 Mask=Victim->GetNPCState()->GetVisitedStateMask();
   Test->TestTrue(TEXT("Alert and Flee visited"),(Mask&(1u<<uint8(ENeonCityNPCState::Alert))) && (Mask&(1u<<uint8(ENeonCityNPCState::Flee))));
   Test->TestTrue(TEXT("Civilian physically flees more than 150 cm"),FVector::Dist2D(Origin,Victim->GetActorLocation())>150);
   Test->TestTrue(TEXT("Civilian moves away from shooter"),FVector::Dist2D(Player->GetActorLocation(),Victim->GetActorLocation())>FVector::Dist2D(Player->GetActorLocation(),Origin)+100);
   Shot(TEXT("Crime-Flee.png")); return Next(8);
  }
  if (Stage==8 && Now-Phase>3.f && !FScreenshotRequest::IsScreenshotRequested())
  {
   Test->TestTrue(TEXT("CallHelp produces real report"),Crime->GetReports().ContainsByPredicate([&](const FNeonCityReportEvent& R) { return R.CrimeId==IncidentId && R.Witness==Victim; }));
   Test->TestTrue(TEXT("Nearby civilian also reports"),Crime->GetReports().ContainsByPredicate([&](const FNeonCityReportEvent& R) { return R.CrimeId==IncidentId && R.Witness==Witness; }));
   const int32 Reports=Crime->GetReports().Num();
   Test->TestFalse(TEXT("Same witness cannot report event twice"),Crime->ReportEvent(IncidentId,Victim.Get()));
   Test->TestEqual(TEXT("Duplicate report leaves ledger unchanged"),Crime->GetReports().Num(),Reports);
   Test->TestTrue(TEXT("Recover reached"),(Victim->GetNPCState()->GetVisitedStateMask()&(1u<<uint8(ENeonCityNPCState::Recover)))!=0);
   Test->TestFalse(TEXT("Civilian never fights"),(Victim->GetNPCState()->GetVisitedStateMask()&(1u<<uint8(ENeonCityNPCState::Fight)))!=0);
   return Next(9);
  }
  if (Stage==9 && Now-Phase>2.f)
  {
   Test->TestFalse(TEXT("Quiet period ends danger response"),Victim->GetThreatComponent()->HasActiveThreat());
   Test->TestTrue(TEXT("Returns to ecology"),(Victim->GetNPCState()->GetVisitedStateMask()&(1u<<uint8(ENeonCityNPCState::Return)))!=0);
   FNeonCityDamageEvent E; E.DamageAmount=35; E.Instigator=PC; E.SourceActor=Weapon.Get();
   Damage->ApplyDamage(Victim.Get(),E); return Next(10);
  }
  if (Stage==10 && Now-Phase>.5f)
  {
   Test->TestTrue(TEXT("NPC Injured through existing framework"),Body(Victim.Get())->GetBodyState()==ENeonCityBodyState::Injured);
   FNeonCityDamageEvent E; E.DamageAmount=25; E.Instigator=PC; E.SourceActor=Weapon.Get();
   Damage->ApplyDamage(Victim.Get(),E);
   Test->TestTrue(TEXT("NPC Downed retained"),IsValid(Victim.Get()) && Body(Victim.Get())->GetBodyState()==ENeonCityBodyState::Downed);
   Test->TestFalse(TEXT("Downed witness cannot report"),Crime->ReportEvent(Victim->GetThreatComponent()->GetIncidentId(),Victim.Get()));
   E.DamageAmount=100; Damage->ApplyDamage(Victim.Get(),E);
   Test->TestTrue(TEXT("NPC Dead retained"),IsValid(Victim.Get()) && Body(Victim.Get())->GetBodyState()==ENeonCityBodyState::Dead);
   auto Before=Crime->GetEvents().Num(); Damage->ApplyDamage(Victim.Get(),E);
   Test->TestEqual(TEXT("Repeated dead damage produces no crime"),Crime->GetEvents().Num(),Before);
   // Reuse the Phase 8 vehicle health contract; never destroy meshes or simulate destruction.
   E.DamageAmount=25; Damage->ApplyDamage(Car.Get(),E);
   Test->TestEqual(TEXT("Vehicle health changes"),Car->GetHealthComponent()->GetCurrentHealth(),75.f);
   Test->TestEqual(TEXT("Nonlethal vehicle damage not destruction"),CountType(Crime,ENeonCityCrimeType::DestroyVehicle),0);
   E.DamageAmount=75; Damage->ApplyDamage(Car.Get(),E);
   Test->TestTrue(TEXT("Dead vehicle actor retained"),IsValid(Car.Get()) && Car->GetHealthComponent()->IsDead());
   Test->TestEqual(TEXT("One vehicle destruction event"),CountType(Crime,ENeonCityCrimeType::DestroyVehicle),1);
   Damage->ApplyDamage(Car.Get(),E);
   Test->TestEqual(TEXT("No duplicate vehicle destruction"),CountType(Crime,ENeonCityCrimeType::DestroyVehicle),1);
   Before=Crime->GetEvents().Num(); FarNPC->GetHealthComponent()->SetCurrentHealth(90);
   Test->TestEqual(TEXT("Admin health changes never generate crime"),Crime->GetEvents().Num(),Before);
   CheckPerceptionAndBounds(World,PC,Crime,Threat);
   Key(PC,EKeys::RightMouseButton,IE_Released); Victim->Destroy(); Witness->Destroy();
   return true;
  }
  return false;
 }
private:
 void CheckPerceptionAndBounds(UWorld* World,APlayerController* PC,UCrimeSubsystem* Crime,UThreatSubsystem* Threat)
 {
  auto* Observer=SpawnNPC(World,FVector(0,0,5000),TEXT("HearingFixture"),{});
  auto* Source=World->SpawnActor<ACombatTestTarget>(FVector(1000,0,5000),FRotator::ZeroRotator);
  auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
  Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(50,200,300)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
  Wall->SetActorLocation(FVector(500,0,5000));
  const auto Hidden=Threat->EmitNearbyDanger(Source,PC,Source->GetActorLocation(),1200);
  Test->TestFalse(TEXT("Walls block visual danger"),Observer->GetThreatComponent()->HasPerceived(Hidden));
  const auto Heard=Threat->EmitGunshot(Source,PC,Source->GetActorLocation());
  Test->TestTrue(TEXT("Gunfire can be heard through wall"),Observer->GetThreatComponent()->HasPerceived(Heard));
  Test->TestTrue(TEXT("Heard danger can be reported"),Crime->ReportEvent(Heard,Observer));
  Test->TestFalse(TEXT("Hearing does not identify invisible shooter"),Crime->GetReports().Last().Instigator.IsValid());
  for (int32 I=0; I<40; ++I) { Threat->EmitGunshot(Source,PC,Source->GetActorLocation()); }
  Test->TestTrue(TEXT("Repeated shots preserve active incident memory"),Observer->GetThreatComponent()->HasPerceived(Heard));
  Observer->GetHealthComponent()->SetCurrentHealth(15);
  const auto WhileDown=Threat->EmitGunshot(Source,PC,Source->GetActorLocation());
  Test->TestFalse(TEXT("Downed NPC ignores new threats"),Observer->GetThreatComponent()->HasPerceived(WhileDown));
  Test->TestFalse(TEXT("Downed NPC cannot report"),Crime->ReportEvent(Heard,Observer));
  Observer->GetHealthComponent()->Heal(85);
  const int32 Before=Crime->GetEvents().Num();
  Test->TestFalse(TEXT("Invalid danger radius rejected"),Threat->EmitNearbyDanger(Source,PC,Source->GetActorLocation(),-1).IsValid());
  Test->TestEqual(TEXT("Invalid danger is not recorded"),Crime->GetEvents().Num(),Before);
  Wall->Destroy();
  for (int32 I=0; I<270; ++I)
  {
   const auto Visible=Threat->EmitNearbyDanger(Source,PC,Source->GetActorLocation(),1200);
   Test->TestTrue(TEXT("Visible nearby danger can be reported"),Crime->ReportEvent(Visible,Observer));
  }
  Test->TestEqual(TEXT("Crime ledger is bounded"),Crime->GetEvents().Num(),256);
  Test->TestEqual(TEXT("Report ledger is bounded after overflow"),Crime->GetReports().Num(),256);
  Test->TestFalse(TEXT("Expired crime cannot be re-reported"),Crime->ReportEvent(Heard,Observer));
  auto* UnknownVictim=SpawnNPC(World,FVector(0,4000,5000),TEXT("UnknownAttackerFixture"),{});
  FNeonCityDamageEvent Anonymous; Anonymous.DamageAmount=1; Anonymous.SourceActor=Source;
  World->GetSubsystem<UDamageSystem>()->ApplyDamage(UnknownVictim,Anonymous);
  Test->TestTrue(TEXT("Unknown instigator still records NPC damage fact"),Crime->GetEvents().Last().CrimeType==ENeonCityCrimeType::DamageNPC && !Crime->GetEvents().Last().Instigator.IsValid());
  UnknownVictim->Destroy(); Observer->Destroy(); Source->Destroy();
 }
 static int32 CountType(UCrimeSubsystem* Crime,ENeonCityCrimeType Type)
 { int32 N=0; for (const auto& E:Crime->GetEvents()) { N+=E.CrimeType==Type; } return N; }
 static ANeonCityNPCCharacter* SpawnNPC(UWorld* World,FVector Location,FName Id,const TArray<FVector>& Route)
 {
  const FTransform Transform(Location);
  auto* NPC=World->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  NPC->InitializePedestrian(Id,nullptr,Route,71); NPC->FinishSpawning(Transform); return NPC;
 }
 static UDamageableCharacterComponent* Body(AActor* Actor) { return Actor->FindComponentByClass<UDamageableCharacterComponent>(); }
 static void AimAt(APlayerController* PC,FVector Point)
 { FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin,Rotation); PC->SetControlRotation((Point-Origin).Rotation()); }
 static void Key(APlayerController* PC,FKey Value,EInputEvent Event)
 { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Value,Event,FPlatformTime::Cycles64())); }
 static void Shot(const TCHAR* Name)
 {
  if (!FParse::Param(FCommandLine::Get(),TEXT("NeonCityCrimeCapture"))) { return; }
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase9")),true);
  FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase9")/Name,true,false);
 }
 FAutomationTestBase* Test; int32 Stage=0; double Started=0; float Phase=0;
 FVector Origin; FGuid IncidentId;
 TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
 TWeakObjectPtr<AWeaponBase> Weapon;
 TWeakObjectPtr<ANeonCityNPCCharacter> Victim,Witness,FarNPC;
 TWeakObjectPtr<ANeonCityBaseVehicle> Car;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCrime,"NeonCity.Crime.ReactionPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCrime::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityCrimeTests::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
