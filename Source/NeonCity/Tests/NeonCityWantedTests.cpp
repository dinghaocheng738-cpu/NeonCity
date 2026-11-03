#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceController.h"
#include "AI/Police/PoliceVehicle.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "AI/Police/PoliceResponseDirector.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCThreatComponent.h"
#include "Wanted/WantedSubsystem.h"
#include "Wanted/WantedLevelData.h"
#include "Wanted/SearchSystem.h"
#include "Crime/CrimeSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/ThreatSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Interaction/InteractiveBuildingDoor.h"
#include "Interaction/InteractionComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
namespace NeonCityWantedTests
{
static ANeonCityNPCCharacter* NPC(UWorld* World,FVector Position)
{
 auto* Actor=World->SpawnActor<ANeonCityNPCCharacter>(Position,FRotator::ZeroRotator);
 Actor->GetCharacterMovement()->DisableMovement(); return Actor;
}
static void PositionPlayer(ANeonCityPlayerCharacter* Player,FVector Position)
{
 Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
 Player->GetCharacterMovement()->StopMovementImmediately();
}
static void Key(APlayerController* PC,FKey Value,EInputEvent Event)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Value,Event,FPlatformTime::Cycles64())); }
static void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityWantedCapture"))) return;
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase10")),true);
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase10")/Name,true,false);
}
class FEvidence : public IAutomationLatentCommand
{
public:
 explicit FEvidence(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Evidence PIE timeout")); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr; auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || !W->HasBegunPlay()) return false;
  auto* Wanted=W->GetSubsystem<UWantedSubsystem>(); auto* Search=W->GetSubsystem<USearchSystem>();
  auto* Crime=W->GetSubsystem<UCrimeSubsystem>(); auto* Threat=W->GetSubsystem<UThreatSubsystem>();
  if(Stage==0)
  {
   Test->TestEqual(TEXT("No initial wanted"),Wanted->GetWantedLevel(),0);
   Test->TestTrue(TEXT("Authored six-level profile loaded"),Wanted->GetProfile()->GetPathName().Contains(TEXT("DA_WantedLevels")) && Wanted->GetProfile()->IsValidProfile());
   Test->TestEqual(TEXT("One authored officer sensor"),Search->GetSensorCount(),1);
   // Isolated perception fixtures above the map; no runtime actors or assets are replaced.
   PositionPlayer(Player,FVector(3000,0,5000)); Player->GetCharacterMovement()->DisableMovement();
   Victim=NPC(W,FVector(400,0,5000)); Witness=NPC(W,FVector(200,200,5000)); OtherWitness=NPC(W,FVector(200,-200,5000));
   Officer=W->SpawnActor<APoliceCharacter>(FVector(-800,0,5000),FRotator::ZeroRotator);
   Officer->GetCharacterMovement()->DisableMovement(); Officer->GetController()->SetActorTickEnabled(false); Officer->GetSensor()->SightRange=100;
   const FGuid Heard=Threat->EmitGunshot(Victim.Get(),PC,Victim->GetActorLocation());
   Test->TestTrue(TEXT("Unidentified report accepted by crime ledger"),Crime->ReportEvent(Heard,Witness.Get()));
   Test->TestEqual(TEXT("Hearing alone cannot identify player"),Wanted->GetWantedLevel(),0);
   Test->TestTrue(TEXT("Anonymous report creates location investigation"),Search->HasUnidentifiedIncident() && Search->GetLastKnownLocation().Equals(Victim->GetActorLocation()));
   PositionPlayer(Player,FVector(0,0,5000));
   FNeonCityDamageEvent E; E.DamageAmount=1; E.Instigator=PC; E.SourceActor=Player;
   W->GetSubsystem<UDamageSystem>()->ApplyDamage(Victim.Get(),E);
   const auto Id=Crime->GetEvents().Last().Id;
   Test->TestEqual(TEXT("Unseen unreported crime does not raise wanted"),Wanted->GetWantedLevel(),0);
   Test->TestTrue(TEXT("Identifying witness reports actual damage event"),Crime->ReportEvent(Id,Witness.Get()));
   Test->TestEqual(TEXT("Damage NPC evaluates to level 2"),Wanted->GetWantedLevel(),2);
   const int32 Score=Wanted->GetEvidenceScore();
   Test->TestTrue(TEXT("Second witness can report same incident"),Crime->ReportEvent(Id,OtherWitness.Get()));
   Test->TestEqual(TEXT("Same crime never scores twice"),Wanted->GetEvidenceScore(),Score);
   Officer->GetSensor()->SightRange=2500; Search->UpdateSight(PC);
   Test->TestTrue(TEXT("Visible suspect acquired"),Search->HasVisualContact());
   Test->TestTrue(TEXT("Only successful sighting refreshes exact position"),Search->GetLastKnownLocation().Equals(Player->GetActorLocation()));
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)
   {
    const FTransform Parked=It->GetActorTransform();
    It->SetActorLocation(FVector(150,600,5000),false,nullptr,ETeleportType::TeleportPhysics); PC->Possess(*It);
    Search->UpdateSight(PC);
    Test->TestTrue(TEXT("Possession switches visible suspect to real vehicle Pawn"),Search->HasVisualContact() && Search->GetLastKnownLocation().Equals(It->GetActorLocation()));
    PC->Possess(Player); It->SetActorTransform(Parked,false,nullptr,ETeleportType::TeleportPhysics); break;
   }
   Search->UpdateSight(PC);
   const FVector Known=Search->GetLastKnownLocation(); const float EvidenceTime=Search->GetEvidenceTime();
   Search->RecordReport(PC,FVector(9000,9000,0),EvidenceTime-1);
   Test->TestTrue(TEXT("Delayed report cannot rewind fresh sighting"),Search->GetLastKnownLocation().Equals(Known));
   auto* Wall=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
   Box->SetBoxExtent(FVector(50,700,700)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Wall->SetActorLocation(FVector(-400,0,5000));
   PositionPlayer(Player,FVector(100,300,5000)); Search->UpdateSight(PC);
   Test->TestFalse(TEXT("Wall blocks police sight"),Search->HasVisualContact());
   Test->TestTrue(TEXT("Hidden route changes do not leak live position"),Search->GetLastKnownLocation().Equals(Known));
   Wall->Destroy(); Officer->SetActorRotation(FRotator(0,180,0)); Search->UpdateSight(PC);
   Test->TestFalse(TEXT("Behind observer is outside vision cone"),Search->HasVisualContact());
   Officer->SetActorRotation(FRotator::ZeroRotator); Officer->GetSensor()->SightRange=100; Search->UpdateSight(PC);
   Test->TestFalse(TEXT("Outside range remains unseen"),Search->HasVisualContact());
   Officer->GetSensor()->SightRange=2500;
   const int32 BeforeOfficerHit=Wanted->GetEvidenceScore();
   W->GetSubsystem<UDamageSystem>()->ApplyDamage(Officer.Get(),E);
   Test->TestEqual(TEXT("Conscious hit officer identifies visible attacker without civilian report"),Wanted->GetEvidenceScore(),BeforeOfficerHit+3);
   Officer->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(15); Search->UpdateSight(PC);
   Test->TestFalse(TEXT("Downed officer cannot provide visual contact"),Search->HasVisualContact());
   Officer->FindComponentByClass<UHealthComponent>()->Die(); Search->UpdateSight(PC);
   Test->TestFalse(TEXT("Dead officer cannot spot suspect"),Search->HasVisualContact());
   const int32 Sensors=Search->GetSensorCount(); Officer->Destroy();
   Test->TestEqual(TEXT("Sensor unregisters on actor removal"),Search->GetSensorCount(),Sensors-1);
   const int32 Expected[]={3,4,5,5,5};
   for(int32 Target:Expected)
   {
    W->GetSubsystem<UDamageSystem>()->ApplyDamage(Victim.Get(),E);
    Test->TestTrue(TEXT("New distinct crime report accepted"),Crime->ReportEvent(Crime->GetEvents().Last().Id,Witness.Get()));
    Test->TestEqual(TEXT("Escalation follows data thresholds, capped at 5"),Wanted->GetWantedLevel(),Target);
   }
   Test->TestEqual(TEXT("High-tier response capped at two cars"),Wanted->GetVehicleBudget(),2);
   Phase=W->GetTimeSeconds(); Stage=1; return false;
  }
  if(W->GetTimeSeconds()-Phase<1) return false;
  int32 Cars=0; for(TActorIterator<APoliceVehicle> It(W);It;++It) ++Cars;
  Test->TestEqual(TEXT("Actual high-tier deployment limited to two cars"),Cars,2);
  Victim->Destroy(); Witness->Destroy(); OtherWitness->Destroy();
  Test->AddInfo(TEXT("Evidence boundaries: anonymous hearing, confirmed reporting, dedupe, range, cone, occlusion, stale snapshots, dead sensors and level cap verified."));
  return true;
 }
private:
 FAutomationTestBase* Test; double Started=0; int32 Stage=0; float Phase=0;
 TWeakObjectPtr<ANeonCityNPCCharacter> Victim,Witness,OtherWitness;
 TWeakObjectPtr<APoliceCharacter> Officer;
};
class FLoop : public IAutomationLatentCommand
{
public:
 explicit FLoop(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Wanted loop timeout at stage %d"),Stage)); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr; auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || !W->HasBegunPlay()) return false;
  auto* Wanted=W->GetSubsystem<UWantedSubsystem>(); auto* Search=W->GetSubsystem<USearchSystem>();
  auto* Crime=W->GetSubsystem<UCrimeSubsystem>(); const float Now=W->GetTimeSeconds();
  auto Next=[&](int32 Value) { UE_LOG(LogTemp,Display,TEXT("WantedLoop %d -> %d time=%.2f level=%d visible=%d known=%s player=%s"),Stage,Value,Now,Wanted->GetWantedLevel(),Search->HasVisualContact(),*Search->GetLastKnownLocation().ToString(),*Player->GetActorLocation().ToString()); Stage=Value; Phase=Now; return false; };
  for(TActorIterator<APoliceCharacter> It(W);It;++It)
  {
   if(auto* C=Cast<APoliceController>(It->GetController())) Visited|=1u<<uint8(C->GetPoliceState());
  }
  if(Stage==0)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround()) return false;
   for(TActorIterator<AInteractiveBuildingDoor> It(W);It;++It) Door=*It;
   PositionPlayer(Player,FVector(-2500,-930,136));
   Victim=NPC(W,FVector(-2200,-930,136));
   FNeonCityDamageEvent E; E.DamageAmount=1; E.Instigator=PC; E.SourceActor=Player;
   W->GetSubsystem<UDamageSystem>()->ApplyDamage(Victim.Get(),E);
   Incident=Crime->GetEvents().Last().Id;
   return Next(1);
  }
  if(Stage==1)
  {
   if(Now-Phase<2) return false;
   Test->TestTrue(TEXT("Civilian automatically reports criminal attack"),Crime->GetReports().ContainsByPredicate([&](const FNeonCityReportEvent& R){return R.CrimeId==Incident;}));
   if(!Test->TestEqual(TEXT("Report raises wanted to level 2"),Wanted->GetWantedLevel(),2)) return true;
   for(TActorIterator<APoliceVehicle> It(W);It;++It) Car=*It;
   if(!Test->TestNotNull(TEXT("Level 2 deploys a road response car"),Car.Get())) return true;
   CarStart=Car->GetActorLocation();
   PositionPlayer(Player,FVector(-1500,1180,136)); PC->SetControlRotation(FRotator(0,90,0));
   return Next(2);
  }
  if(Stage==2)
  {
   bSawChase|=Car->GetPoliceState()==ENeonCityPoliceState::Chase;
   if(Now-Phase<2) return false;
   Test->TestTrue(TEXT("Police car chases a visually acquired suspect"),bSawChase);
   Test->TestTrue(TEXT("Police car physically advances toward sighted road position"),Car->GetActorLocation().X>CarStart.X+100);
   Test->TestTrue(TEXT("Response vehicle siren hook active"),Car->IsSirenEnabled());
   Test->TestTrue(TEXT("Visible suspect keeps level 2 active"),Search->HasVisualContact() && Wanted->GetWantedLevel()==2);
   Test->TestTrue(TEXT("Building door focused"),Player->GetInteractionComponent()->RefreshFocusedInteractable()==Door.Get());
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityWantedCapture")))
   {
    CaptureCamera=W->SpawnActor<ACameraActor>(); const FVector Location=Car->GetActorLocation()+FVector(-800,-900,480);
    CaptureCamera->SetActorLocation(Location); CaptureCamera->SetActorRotation((Car->GetActorLocation()-Location).Rotation());
    CaptureCamera->GetCameraComponent()->SetFieldOfView(60); PC->SetViewTarget(CaptureCamera.Get()); return Next(20);
   }
   Key(PC,EKeys::F,IE_Pressed); return Next(3);
  }
  if(Stage==20 && Now-Phase>.7f) { Shot(TEXT("Police-Chase.png")); return Next(21); }
  if(Stage==21 && Now-Phase>.5f && !FScreenshotRequest::IsScreenshotRequested())
  { PC->SetViewTarget(Player); CaptureCamera->Destroy(); Key(PC,EKeys::F,IE_Pressed); return Next(3); }
  if(Stage==3 && Now-Phase>.25f)
  {
   Key(PC,EKeys::F,IE_Released); Test->TestTrue(TEXT("F opens building for hiding"),Door->IsOpen()); return Next(4);
  }
  if(Stage==4)
  {
   Player->DoMove(0,1);
   if(Now-Phase<1.f) return false;
   Test->TestTrue(TEXT("Player walks through actual lobby opening"),Player->GetActorLocation().Y>1510);
   Player->GetCharacterMovement()->StopMovementImmediately();
   FVector Center,Extent; Door->GetActorBounds(false,Center,Extent);
   PC->SetControlRotation((Center-Player->GetPawnViewLocation()).Rotation()); return Next(22);
  }
  if(Stage==22 && Now-Phase>.2f)
  {
   Test->TestTrue(TEXT("Open door focused from inside"),Player->GetInteractionComponent()->RefreshFocusedInteractable()==Door.Get());
   Key(PC,EKeys::F,IE_Pressed); return Next(23);
  }
  if(Stage==23 && Now-Phase>.25f)
  {
   Key(PC,EKeys::F,IE_Released); Test->TestFalse(TEXT("F closes entrance behind player"),Door->IsOpen());
   PC->SetControlRotation(FRotator(0,180,0)); return Next(5);
  }
  if(Stage==5)
  {
   Player->DoMove(0,1); // Change route behind the hall's opaque front pier.
   if(Now-Phase<.7f) return false;
   Player->GetCharacterMovement()->StopMovementImmediately(); return Next(6);
  }
  if(Stage==6 && Now-Phase>.6f)
  {
   Test->TestFalse(TEXT("Building wall breaks visual contact"),Search->HasVisualContact());
   HiddenSnapshot=Search->GetLastKnownLocation(); HiddenPlayerStart=Player->GetActorLocation();
   PC->SetControlRotation(FRotator(0,90,0)); return Next(7);
  }
  if(Stage==7)
  {
   Player->DoMove(0,1);
   if(Now-Phase<.6f) return false;
   Player->GetCharacterMovement()->StopMovementImmediately();
   Test->TestTrue(TEXT("Hidden player changes route physically"),FVector::Dist2D(HiddenPlayerStart,Player->GetActorLocation())>100);
   Test->TestTrue(TEXT("Last known location stays frozen behind wall"),Search->GetLastKnownLocation().Equals(HiddenSnapshot,1));
   Test->TestFalse(TEXT("No through-building sight"),Search->HasVisualContact()); Shot(TEXT("Police-Search.png")); return Next(8);
  }
  if(Stage==8)
  {
   bSawSearch|=Car->GetPoliceState()==ENeonCityPoliceState::Search;
   if(Now-LastDiagnostic>3)
   {
    LastDiagnostic=Now;
    UE_LOG(LogTemp,Display,TEXT("WantedEscape level=%d visible=%d carState=%d car=%s known=%s"),Wanted->GetWantedLevel(),Search->HasVisualContact(),uint8(Car->GetPoliceState()),*Car->GetActorLocation().ToString(),*Search->GetLastKnownLocation().ToString());
   }
   bSawLevelOne|=Wanted->GetWantedLevel()==1;
   if(Wanted->GetWantedLevel()!=0) return false;
   Test->TestTrue(TEXT("Response car searches last recorded area"),bSawSearch);
   Test->TestTrue(TEXT("Wanted descends through level 1"),bSawLevelOne);
   Test->TestFalse(TEXT("Escape clears stale suspect memory"),Search->HasLocation());
   Test->TestTrue(TEXT("Foot officer preserved patrol behavior"),(Visited&(1u<<uint8(ENeonCityPoliceState::Patrol)))!=0);
   Test->TestTrue(TEXT("Foot officer investigates last known area"),(Visited&(1u<<uint8(ENeonCityPoliceState::Investigate)))!=0);
   return Next(9);
  }
  if(Stage==9 && Now-Phase>.6f)
  {
   Test->TestFalse(TEXT("Escape disables response siren"),Car->IsSirenEnabled());
   Test->TestTrue(TEXT("Car retained and returns to patrol"),Car.IsValid() && Car->GetPoliceState()==ENeonCityPoliceState::Patrol);
   Shot(TEXT("Police-Escape.png")); return Next(10);
  }
  if(Stage==10 && Now-Phase>.5f && !FScreenshotRequest::IsScreenshotRequested())
  {
   Victim->Destroy(); Test->AddInfo(TEXT("Crime -> NPC report -> wanted 2 -> visible car chase -> F-key building entry -> hidden route change -> search -> 2/1/0 escape verified in PIE.")); return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test; int32 Stage=0; double Started=0; float Phase=0,LastDiagnostic=0;
 uint32 Visited=0; bool bSawChase=false,bSawSearch=false,bSawLevelOne=false;
 FVector CarStart,HiddenSnapshot,HiddenPlayerStart; FGuid Incident;
 TWeakObjectPtr<ANeonCityNPCCharacter> Victim;
 TWeakObjectPtr<APoliceVehicle> Car;
 TWeakObjectPtr<AInteractiveBuildingDoor> Door;
 TWeakObjectPtr<ACameraActor> CaptureCamera;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWantedEvidence,"NeonCity.Wanted.EvidencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWantedEvidence::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWantedTests::FEvidence>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWantedLoop,"NeonCity.Wanted.PursuitEscapePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWantedLoop::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityWantedTests::FLoop>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
