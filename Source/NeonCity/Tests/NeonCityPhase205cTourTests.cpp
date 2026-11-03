#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandActivityJob.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace NativeIslandTour
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,int Session):Test(T),Session(Session){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>230)return End(TEXT("Native walking tour timed out"));if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(52000,-159000,300))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Tour.Village"))Job=*It;
   for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job && It->Index==0)Begin=*It;
   Phone=PC->FindComponentByClass<UPhoneComponent>();Wallet=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Job || !Begin || !Phone || !Save->ConfigureSlot(Session==2?TEXT("NeonCity_Test_TourDone01"):TEXT("NeonCity_Test_TourMid01"),false))return End(TEXT("Actual tour, phone, start or isolated save domain missing"));
   if(Session>0){if(!Save->DoesSaveExist())return End(TEXT("Genuine prior tour checkpoint required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist())return End(TEXT("Preserve genuine tour evidence; source checkpoint already exists"));
   const FVector Setup=Begin->GetActorLocation()+FVector(-180,-120,100);FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(TourSupportedStart),false,P);
   if(!W->LineTraceSingleByChannel(Floor,Setup+FVector(0,0,600),Setup-FVector(0,0,1000),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return End(TEXT("Actual tour start lacks supported walking ground"));
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual tour start has blocked standing clearance"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);const auto D=Job->BuildOpportunitySnapshot(false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());Before=Wallet->GetBalance();Run=D.ActivityRunID;
   if(Session==2){if(D.State!=2 || D.ActivityCompletions!=1 || !Wallet->HasCredit(Job->GetReceiptID()) || Before!=2050)return End(TEXT("Cold tour result lost one200credit/completion/receipt"));Key(EKeys::P,true);return Next(1);}
   const auto* M=Save->ReadSaveForInspection();const auto* SavedTour=M?M->Phone.Opportunities.FindByPredicate([&](const auto& V){return V.OpportunityID==Job->ActivityID;}):nullptr;
   if(!M || !SavedTour)return End(TEXT("Genuine mid-tour model missing"));const FVector Stored=M->Player.Transform.GetLocation();FCollisionQueryParams Q(SCENE_QUERY_STAT(TourActualRestoreDiagnosis),false,P);TArray<FOverlapResult> Hits;
   W->OverlapMultiByChannel(Hits,Stored,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);RestoreDiagnosis=FString::Printf(TEXT("saved_player%s restored_player%s saved_guest%s\n"),*Stored.ToString(),*P->GetActorLocation().ToString(),*SavedTour->ActivityNPCPosition.ToString());
   for(const auto& H:Hits)if(H.bBlockingHit)RestoreDiagnosis+=FString::Printf(TEXT("blocking%s/%s\n"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()));FHitResult Floor;W->LineTraceSingleByChannel(Floor,Stored,Stored-FVector(0,0,P->GetSimpleCollisionHalfHeight()+20),ECC_Visibility,Q);RestoreDiagnosis+=FString::Printf(TEXT("actual_floor%s/%s z%.3f normal%s distance%.3f\n"),*GetNameSafe(Floor.GetActor()),*GetNameSafe(Floor.GetComponent()),Floor.ImpactPoint.Z,*Floor.ImpactNormal.ToString(),Floor.Distance);
   for(const auto& V:M->Vehicles){const FVector Local=V.Transform.InverseTransformPosition(Stored);if(FMath::Abs(Local.X)<285 && FMath::Abs(Local.Y)<145 && FMath::Abs(Local.Z)<220)RestoreDiagnosis+=FString::Printf(TEXT("saved_vehicle_blocks%s location%s\n"),*V.VehicleID.ToString(),*V.Transform.GetLocation().ToString());}UE_LOG(LogTemp,Display,TEXT("Tour exact restore diagnosis: %s"),*RestoreDiagnosis);
   if(Session==3){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Tour-Cold-Restore-Diagnosis.png"));return End(TEXT(""));}
   auto* Guest=Job->GetActivityNPC();if(D.State!=1 || D.ActivityStage!=1 || D.ActivityCheckpoint!=1 || !Guest || !SavedTour->ActivityNPCPositionValid || FVector::Dist(Guest->GetActorLocation(),SavedTour->ActivityNPCPosition)>100 || FVector::Dist(P->GetActorLocation(),Stored)>100 || Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Cold unfinished tour lost actual player/visitor/checkpoint or unpaid run; see exact saved-position diagnosis"));
   Checkpoint=1;LastProgress=W->GetTimeSeconds();return Next(8);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Normal P phone input failed"));Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Actual walking-tour offer undiscoverable"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run || !Job->GetActivityNPC())return End(TEXT("Native tour acceptance/replay did not create a fresh real visitor run"));
   if(Session==2){Key(EKeys::Delete,true);return Next(21);}Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==21 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Job->GetActivityNPC() || Wallet->GetBalance()!=Before)return End(TEXT("Native replay cancellation duplicated income or left a guest"));return End(TEXT(""));}
  if(Stage==6 && Age>.2){Key(EKeys::P,false);PC->SetControlRotation((Begin->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Begin)return End(TEXT("Actual meet-visitor F prompt inaccessible"));Key(EKeys::F,true);return Next(7);}
  if(Stage==7 && Age>.2){Key(EKeys::F,false);if(Job->GetActivityStage()!=1)return End(TEXT("Normal F did not start real walking tour"));LastProgress=W->GetTimeSeconds();return Next(8);}
  if(Stage==8)
  {
   const auto D=Job->BuildOpportunitySnapshot(false);auto* Guest=Job->GetActivityNPC();
   if(D.State==2){Key(EKeys::W,false);if(Session!=1 || D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Actual walking-tour completion/payment failed"));if(!Save->ConfigureSlot(TEXT("NeonCity_Test_TourDone01"),false) || Save->DoesSaveExist())return End(TEXT("Preserve earlier completed-tour save evidence"));Key(EKeys::F6,true);return Next(11);}
   if(D.State!=1 || !Guest || Guest->IsHidden() || !Guest->GetActorEnableCollision() || !Guest->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Actual visible walking visitor lost normal character physics"));
   const float Now=W->GetTimeSeconds();if(LastSample>0){const float Speed=FVector::Dist2D(Guest->GetActorLocation(),LastGuest)/(Now-LastSample);if(Speed>350)return End(TEXT("Visitor position changed faster than actual walking; no teleport acceptance"));GuestDistance+=FVector::Dist2D(Guest->GetActorLocation(),LastGuest);}LastGuest=Guest->GetActorLocation();LastSample=Now;
   CSV+=FString::Printf(TEXT("%.4f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),Now,D.ActivityCheckpoint,P->GetActorLocation().X,P->GetActorLocation().Y,P->GetActorLocation().Z,LastGuest.X,LastGuest.Y,LastGuest.Z);
   if(D.ActivityCheckpoint!=Checkpoint){Checkpoint=D.ActivityCheckpoint;LastProgress=Now;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Tour-Native-View-Stop.png"));}
   if(Session==0 && D.ActivityCheckpoint>=1){Key(EKeys::W,false);if(GuestDistance<500 || Wallet->GetBalance()!=Before)return End(TEXT("Visitor did not physically follow the first view without reward"));Key(EKeys::F6,true);return Next(10);}
   if(Now-LastProgress>45)
   {
    FHitResult Hit,GuestHit;FCollisionQueryParams Q(SCENE_QUERY_STAT(TourActualWalkBlocker),false,P),GQ(SCENE_QUERY_STAT(TourActualGuestBlocker),false,Guest);
    W->SweepSingleByChannel(Hit,P->GetActorLocation(),P->GetActorLocation()+P->GetActorForwardVector()*150,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
    const FVector Direction=(P->GetActorLocation()-P->GetActorForwardVector()*130-Guest->GetActorLocation()).GetSafeNormal2D();
    W->SweepSingleByChannel(GuestHit,Guest->GetActorLocation(),Guest->GetActorLocation()+Direction*150,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Guest->GetSimpleCollisionRadius(),Guest->GetSimpleCollisionHalfHeight()),GQ);
    return End(FString::Printf(TEXT("Normal tour blocked at checkpoint%d: player%s guest%s player_collider%s/%s guest_collider%s/%s"),Checkpoint,*P->GetActorLocation().ToString(),*Guest->GetActorLocation().ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*GetNameSafe(GuestHit.GetActor()),*GetNameSafe(GuestHit.GetComponent())));
   }
   const FVector Goal=Job->GetActorTransform().TransformPosition(Job->RoutePointsLocal.IsValidIndex(Checkpoint)?Job->RoutePointsLocal[Checkpoint]:Job->ReturnLocal);
   const float Gap=FVector::Dist2D(Guest->GetActorLocation(),P->GetActorLocation());const bool AtView=FVector::Dist2D(P->GetActorLocation(),Goal)<70;
   PC->SetControlRotation(FRotator(0,(Goal-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,!AtView && Gap<450);return false;
  }
  if(Stage==10 && Age>.8)
  {
   Key(EKeys::F6,false);auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.ActivityRunID==Run && D.State==1 && D.ActivityCheckpoint==1 && D.ActivityNPCPositionValid;}) || Wallet->GetBalance()!=Before)return End(TEXT("Genuine mid-tour F6 lost unpaid run/real visitor/checkpoint"));return End(TEXT(""));
  }
  if(Stage==11 && Age>.8)
  {
   Key(EKeys::F6,false);auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.ActivityRunID==Run && D.State==2 && D.ActivityCompletions==1;}) || Job->GetActivityNPC())return End(TEXT("Genuine finished-tour F6 or guest cleanup failed"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Tour-Native-Completed-Saved.png"));return End(TEXT(""));
  }
  return false;
 }
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("session"),Session);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("checkpoint"),Checkpoint);J->SetNumberField(TEXT("actual_guest_walk_cm"),GuestDistance);J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);
  J->SetStringField(TEXT("scope"),Session==0?TEXT("One physically supported player start; native P/O/RIGHT/ENTER offer, actual NPC spawn, F meet, held mapped W walking with real waits for slower visitor. First genuine four-second viewpoint, actual visible normal walking NPC trajectory and F6 unpaid mid-route save. No NPC pose writes, completion, rewards or inventory fixtures."):Session==1?TEXT("Independent genuine F7 restores actual mid-tour checkpoint/guest position/run with zero pose/state/reward fixtures. Held native W with actual visitor following and four-second ordered view stops, on-foot arrival, single shared200credit, guest cleanup and completed F6 in a separate fresh slot. No post-setup player or guest pose writes."):TEXT("Independent genuine F7 completed-tour result and200income/unique receipt. Native P/O/RIGHT/ENTER fresh-run replay and DELETE cancellation with guest cleanup and no extra payment. Zero pose/state/reward fixtures. Subjective visitor/whole-village quality remains separate."));
  J->SetStringField(TEXT("restore_diagnosis"),RestoreDiagnosis);if(Session==3)J->SetStringField(TEXT("scope"),TEXT("Minimum independent genuine F7 mid-tour restoration diagnosis only. Read the original actual saved player and guest poses, trace actual saved-position floor/capsule blockers and check saved vehicle safety envelopes. No position/state/reward/save writes. Diagnostic completion is never full-tour gameplay PASS."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("NativeTour");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));if(Session<2)FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("trajectory.csv")));return true;
 }
 FAutomationTestBase* Test;int Session;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Begin=nullptr;UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;TMap<FKey,bool> Keys;int Stage=0,Browsed=0,Checkpoint=0;float At=0,LastProgress=0,LastSample=0,GuestDistance=0;double Start=0;int64 Before=0;FGuid Run;FVector LastGuest=FVector::ZeroVector;FString CSV=TEXT("game_seconds,checkpoint,player_x,player_y,player_z,guest_x,guest_y,guest_z\n");FNeonCityVisualEvidence Evidence{TEXT("NativeTour")};
 FString RestoreDiagnosis;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTourSource,"NeonCity.Phase205c.Activity.Tour.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTourSource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTour::FFlow(this,0));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTourResume,"NeonCity.Phase205c.Activity.Tour.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTourResume::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTour::FFlow(this,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTourResultCold,"NeonCity.Phase205c.Activity.Tour.C",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTourResultCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTour::FFlow(this,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTourRestoreDiagnosis,"NeonCity.Phase205c.Activity.Tour.RestoreDiagnosisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTourRestoreDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTour::FFlow(this,3));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
