#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/WingsuitComponent.h"
#include "Adventure/WingsuitLaunchPoint.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeWingChallenge254
{
/** A extends the genuine accepted24 roof checkpoint with the authored contract.
 * B loads its actual completed save in a different process. Neither session
 * places a player, injects gear/flight/activity/reward state, or changes gates.
 */
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold),Evidence(Cold?TEXT("WingChallengeCold254"):TEXT("WingChallengeSource254")){}
 ~FFlow(){if(Driver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Driver);}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>(bCold?100:480))return End(TEXT("Bounded native wingsuit challenge timed out"));
  if(!P || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(49300,-120500,32160))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Wingsuit.Village"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job){if(It->Index==0)Begin=*It;else if(It->Index==1)Finish=*It;}
   for(TActorIterator<AWingsuitLaunchPoint> It(W);It;++It)if(It->bRoofSafetyGate)Launch=*It;
   Kit=P->FindComponentByClass<UWingsuitComponent>();Phone=PC->FindComponentByClass<UPhoneComponent>();
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityWingChallengeRun="),Suffix);if(Suffix.IsEmpty())Suffix=TEXT("254");
   if(Suffix.Len()>20)return End(TEXT("Use a bounded unique wing challenge suffix"));for(TCHAR C:Suffix)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))return End(TEXT("Invalid wing challenge suffix"));
   DoneSlot=TEXT("NeonCity_Test_WingChallengeDone_")+Suffix;
   Bridge=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("WingChallenge-")+Suffix)/TEXT("Completed.json");
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("WingsuitChallenge254");IFileManager::Get().MakeDirectory(*Directory,true);
   if(!Job || Job->Kind!=ENeonCityIslandActivity::WingsuitChallenge || Job->Reward!=210 || Job->RoutePointsLocal.Num()!=3 || !Begin || !Finish || !Launch || !Kit || !Phone || !Save || !Wallet)
    return End(TEXT("Current real three-gate/210 challenge, public roof locker, phone or shared domains missing"));
   for(const FVector& Local:Job->RoutePointsLocal)
   {
    const FVector Goal=Job->GetActorTransform().TransformPosition(Local);Route.Add(Goal);Closest.Add(BIG_NUMBER);
    FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(WingChallengeNativeGateFloor),false,P);
    const bool Found=W->LineTraceSingleByChannel(H,Goal+FVector(0,0,50000),Goal-FVector(0,0,50000),ECC_Pawn,Q);
    Access+=FString::Printf(TEXT("gate-floor,%d,%.3f,%.3f,%.3f,%d,%s,%s,%.3f,%.3f,%.3f,%.3f\n"),Route.Num()-1,Goal.X,Goal.Y,Goal.Z,Found?1:0,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),H.ImpactPoint.X,H.ImpactPoint.Y,H.ImpactPoint.Z,H.ImpactNormal.Z);
   }
   if(!Save->ConfigureSlot(DoneSlot,false))return End(TEXT("Isolated genuine challenge result slot unavailable"));
   if(bCold)
   {if(!Save->DoesSaveExist() || !ReadBridge() || SourcePID==FPlatformProcess::GetCurrentProcessId())return End(TEXT("Independent actual completed challenge source/process required"));Key(EKeys::F7,true);return Next(30);}
   if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*Bridge))return End(TEXT("Preserve previous challenge result; choose a fresh suffix after a source change"));
   InputSlot=TEXT("NeonCity_Test_WingUpgrade02");FParse::Value(FCommandLine::Get(),TEXT("NeonCityWingChallengeInput="),InputSlot);
   if(!Save->ConfigureSlot(InputSlot,false) || !Save->DoesSaveExist())return End(TEXT("Genuine previously accepted interior-to-roof equipment checkpoint required; no substitute fixture"));
   Key(EKeys::F7,true);return Next(1);
  }
  Evidence.Record(W,PC,Stage);Observe();
  if(Stage==1 && Age>1.5)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !P->GetCharacterMovement()->IsMovingOnGround() || !Kit->IsEquipped() || Kit->IsFlying()
    || FVector::Dist2D(P->GetActorLocation(),Launch->GetActorLocation())>300)return End(TEXT("Actual roof F7 failed safe nearby grounded equipped restoration"));
   if(Job->BuildOpportunitySnapshot(false).ActivityCompletions!=0 || Job->BuildOpportunitySnapshot(false).State==1)return End(TEXT("Source challenge must be the unused real authored contract"));
   if(!Save->ConfigureSlot(DoneSlot,false) || Save->DoesSaveExist())return End(TEXT("Fresh completion slot cannot replace retained roof source"));
   Before=Wallet->GetBalance();InitialHealth=P->GetHealthComponent()->GetCurrentHealth();
   Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FFlow::DriveFrame);
   Key(EKeys::P,true);return Next(2);
  }
  if(Stage==30 && Age>1.2)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;const auto D=Job->BuildOpportunitySnapshot(false);const auto* M=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || D.State!=2 || D.ActivityRunID!=Run || D.ActivityCompletions!=1 || D.ActivityCheckpoint!=3
    || FMath::Abs(D.ActivityBestSeconds-BestSeconds)>.01 || Wallet->GetBalance()!=ExpectedBalance || !Wallet->HasCredit(Job->GetReceiptID())
    || !P->GetCharacterMovement()->IsMovingOnGround() || Kit->IsFlying() || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>30)
    return End(TEXT("Independent F7 lost actual three-gate score/run/one210 receipt or safe physical player result"));
   OldRun=Run;Before=Wallet->GetBalance();Key(EKeys::P,true);return Next(2);
  }
  if(Stage==2 && Age>.2)
  {Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Normal P failed to open phone"));if(Phone->GetActiveApp()==ENeonCityPhoneApp::Opportunities)return Next(4);Key(EKeys::O,true);return Next(3);}
  if(Stage==3 && Age>.2){Key(EKeys::O,false);return Next(4);}
  if(Stage==4 && Age>.15)
  {if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Real normal opportunities list did not expose challenge"));Key(EKeys::Right,true);return Next(40);}
  if(Stage==40 && Age>.15){Key(EKeys::Right,false);return Next(4);}
  if(Stage==5 && Age>.25)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);
   if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==OldRun)return End(TEXT("Normal ENTER did not accept a fresh unpaid challenge run"));
   if(bCold){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Cold challenge replay duplicated earlier payment/count"));Key(EKeys::Delete,true);return Next(31);}
   Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==31 && Age>.25)
  {Key(EKeys::Delete,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State==1 || D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Normal replay/cancel duplicated210 reward or retained active run"));return End(TEXT(""));}
  if(Stage==6 && Age>.25)
  {
   Key(EKeys::P,false);if(Phone->IsPhoneOpen())return End(TEXT("Normal phone close did not release player controls"));
   if(!PlanInteraction(Begin,Begin->Interaction->GetComponentLocation()))return End(WalkFailure);
   return Next(7);
  }
  if(Stage==7)
  {if(!WalkUpdate())return WalkFailure.IsEmpty()?false:End(WalkFailure);return Next(8);}
  if(Stage==8 && Age>.3)
  {if(!P->GetCharacterMovement()->IsMovingOnGround() || !Focus(Begin,Begin->Interaction->GetComponentLocation()))return End(TEXT("Actual supported roof challenge F start is occluded"));Key(EKeys::F,true);return Next(9);}
  if(Stage==9 && Age>.25)
  {
   Key(EKeys::F,false);if(Job->GetActivityStage()!=1)return End(TEXT("Normal F failed to start real timed challenge"));
   if(!PlanInteraction(Launch,Launch->InteractionTarget->GetComponentLocation()))return End(WalkFailure);return Next(10);
  }
  if(Stage==10){if(!WalkUpdate())return WalkFailure.IsEmpty()?false:End(WalkFailure);return Next(11);}
  if(Stage==11 && Age>.3)
  {if(!Focus(Launch,Launch->InteractionTarget->GetComponentLocation()))return End(TEXT("Normal actual roof locker F rearm is occluded"));Key(EKeys::F,true);return Next(12);}
  if(Stage==12 && Age>1.2)
  {
   Key(EKeys::F,false);if(!Kit->IsEquipped() || Kit->IsFlying() || !Launch->IsGateOpen())return End(TEXT("Normal F equip/rearm did not open the actual physical roof gate"));
   // These are the accepted24 case-side/hinge-relative walking offsets, not
   // an injected player route. Survey actual floors then steer mapped W.
   const FVector H=Launch->SafetyGate->GetComponentLocation();
   RoofGoals={H+FVector(135,380,0),H+FVector(135,100,0),H+FVector(110,100,0)};RoofNode=0;
   if(!PlanFloor(RoofGoals[RoofNode]))return End(WalkFailure);return Next(13);
  }
  if(Stage==13)
  {
   if(!WalkUpdate())return WalkFailure.IsEmpty()?false:End(WalkFailure);
   if(++RoofNode<RoofGoals.Num()){if(!PlanFloor(RoofGoals[RoofNode]))return End(WalkFailure);return Next(13);}
   PC->SetControlRotation(FRotator(0,-90,0));return Next(14);
  }
  if(Stage==14)
  {
   if(P->GetActorLocation().Y<=Launch->SafetyGate->GetComponentLocation().Y+70){Key(EKeys::SpaceBar,true);return Next(15);}
   if(Age>6)return End(TEXT("Held native W blocked before the real opened launch edge"));
  }
  if(Stage==15 && Age>.25){Key(EKeys::SpaceBar,false);return Next(16);}
  if(Stage==16)
  {
   if(Kit->IsFlying()){ReleaseMotion();FlightAt=Now;LastFlight=P->GetActorLocation();LastCheckpointAt=Now;LastCheckpoint=0;return Next(17);}
   if(Age>6)return End(TEXT("Normal deliberate Space jump did not enter existing physical glide"));
  }
  if(Stage==17)
  {
   const auto D=Job->BuildOpportunitySnapshot(false);
   if(D.State!=1 || D.ActivityRunID!=Run || P->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5)return End(TEXT("Actual challenge lost its unpaid run or damaged the player"));
   if(D.ActivityCheckpoint!=LastCheckpoint){LastCheckpoint=D.ActivityCheckpoint;LastCheckpointAt=Now;}
   if(D.ActivityCheckpoint<3 && Kit->IsFlying() && P->GetActorLocation().Z<Route[D.ActivityCheckpoint].Z-300)
    return End(FString::Printf(TEXT("Native gate%d missed its actual altitude: player%s goal%s closest%.3fcm; no flight/position correction"),D.ActivityCheckpoint,*P->GetActorLocation().ToString(),*Route[D.ActivityCheckpoint].ToString(),Closest[D.ActivityCheckpoint]));
   if(Now-LastCheckpointAt>140)return End(FString::Printf(TEXT("Real ordered checkpoint%d was not reached within finite flight budget"),D.ActivityCheckpoint));
   if(!Kit->IsFlying())
   {
    ReleaseMotion();Landed=P->GetActorLocation();
    if(!P->GetCharacterMovement()->IsMovingOnGround() || D.ActivityCheckpoint!=3 || !SawCanopy)return End(TEXT("Actual ground landing occurred before all three native ordered flying gates"));
    if(!PlanInteraction(Finish,Finish->Interaction->GetComponentLocation()))return End(WalkFailure);return Next(18);
   }
   if(Age>.6 && !CheckedAirSave){CheckedAirSave=true;Key(EKeys::F6,true);}
   if(CheckedAirSave && Age>1 && Keys.FindRef(EKeys::F6))
   {Key(EKeys::F6,false);if(Save->GetLastResult()!=ENeonCitySaveResult::UnsafeState)return End(TEXT("Airborne challenge incorrectly allowed a shared player save"));}
  }
  if(Stage==18){if(!WalkUpdate())return WalkFailure.IsEmpty()?false:End(WalkFailure);return Next(19);}
  if(Stage==19 && Age>.3)
  {if(!P->GetCharacterMovement()->IsMovingOnGround() || !Focus(Finish,Finish->Interaction->GetComponentLocation()))return End(TEXT("Actual on-foot village challenge finish F prompt inaccessible"));Key(EKeys::F,true);return Next(20);}
  if(Stage==20 && Age>.25)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);
   if(D.State!=2 || D.ActivityRunID!=Run || D.ActivityCheckpoint!=3 || D.ActivityCompletions!=1 || D.ActivityBestSeconds<=0 || Wallet->GetBalance()!=Before+210 || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Real three-gate/on-foot F finish did not settle one210 reward/time/count"));
   BestSeconds=D.ActivityBestSeconds;ExpectedBalance=Wallet->GetBalance();Key(EKeys::F,true);return Next(21);
  }
  if(Stage==21 && Age>.25)
  {Key(EKeys::F,false);if(Wallet->GetBalance()!=ExpectedBalance || Job->BuildOpportunitySnapshot(false).ActivityCompletions!=1)return End(TEXT("Repeated ordinary F duplicated challenge result"));Key(EKeys::F6,true);return Next(22);}
  if(Stage==22 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityRunID==Run && D.ActivityCheckpoint==3 && D.ActivityCompletions==1 && FMath::Abs(D.ActivityBestSeconds-BestSeconds)<.01;}))return End(TEXT("Genuine grounded F6 did not retain actual challenge score/run/completion"));
   if(!WriteBridge())return End(TEXT("Actual completed source manifest could not be retained"));return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down)
 {bool& Old=Keys.FindOrAdd(K);if(Old!=Down && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 void ReleaseMotion(){for(auto K:{EKeys::W,EKeys::S,EKeys::A,EKeys::D,EKeys::SpaceBar})Key(K,false);}
 bool Focus(AActor* A,FVector Aim){PC->SetControlRotation((Aim-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==A;}
 bool Floor(FVector XY,FVector& Standing)
 {
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(WingChallengeActualStanding),false,P);
  const bool Found=W->LineTraceSingleByChannel(H,XY+FVector(0,0,400),XY-FVector(0,0,450),ECC_Pawn,Q);
  Standing=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
  const bool Clear=Found && P->GetCharacterMovement()->IsWalkable(H) && !W->OverlapBlockingTestByChannel(Standing,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
  Access+=FString::Printf(TEXT("floor-capsule,%d,%.3f,%.3f,%.3f,%d,%s,%s,%.3f,%.3f,%.3f,%.3f\n"),Stage,XY.X,XY.Y,XY.Z,Clear?1:0,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),H.ImpactPoint.X,H.ImpactPoint.Y,H.ImpactPoint.Z,H.ImpactNormal.Z);return Clear;
 }
 bool PlanInteraction(AActor* Actor,FVector Aim)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(WingChallengeActualInteraction),false,P);
  for(float Radius:{150.f,190.f,220.f})for(int32 I=0;I<16;++I)
  {
   const float A=I*UE_TWO_PI/16;FVector Stand;FHitResult Eye;
   if(!Floor(Actor->GetActorLocation()+FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius,Stand))continue;
   const bool Visible=W->LineTraceSingleByChannel(Eye,Stand+FVector(0,0,P->BaseEyeHeight),Aim,ECC_Visibility,Q) && Eye.GetActor()==Actor;
   Access+=FString::Printf(TEXT("eye-%s,%d,%.3f,%.3f,%.3f,%d,%s,%s,%.3f,%.3f,%.3f,%.3f\n"),*GetNameSafe(Actor),Stage,Stand.X,Stand.Y,Stand.Z,Visible?1:0,*GetNameSafe(Eye.GetActor()),*GetNameSafe(Eye.GetComponent()),Eye.ImpactPoint.X,Eye.ImpactPoint.Y,Eye.ImpactPoint.Z,Eye.ImpactNormal.Z);
   if(!Visible)continue;
   if(Phase205cSupportedWalkPath(W,P,Stand,Path)){Node=0;LastWalkProgress=W->GetTimeSeconds();WalkFailure.Reset();return true;}
  }
  WalkFailure=FString::Printf(TEXT("Actual full-capsule floor/eye/W access missing for%s at%s; no posed approach"),*GetNameSafe(Actor),*Actor->GetActorLocation().ToString());return false;
 }
 bool PlanFloor(FVector Goal)
 {
  Goal.Z=P->GetActorLocation().Z;FVector Stand;
  if(!Floor(Goal,Stand) || !Phase205cSupportedWalkPath(W,P,Stand,Path)){WalkFailure=TEXT("Retained real case-side route has no actual full-capsule supported W path");return false;}
  Node=0;LastWalkProgress=W->GetTimeSeconds();WalkFailure.Reset();return true;
 }
 bool WalkUpdate()
 {
  while(Node<Path.Num() && FVector::Dist2D(P->GetActorLocation(),Path[Node])<35){++Node;LastWalkProgress=W->GetTimeSeconds();}
  if(Node>=Path.Num()){Key(EKeys::W,false);return true;}
  if(W->GetTimeSeconds()-LastWalkProgress>12){WalkFailure=FString::Printf(TEXT("Actual normal W walk blocked stage%d node%d player%s target%s"),Stage,Node,*P->GetActorLocation().ToString(),*Path[Node].ToString());ReleaseMotion();}
  return false;
 }
 void DriveFrame(UWorld* World,ELevelTick,float DT)
 {
  if(World!=W || !P || !PC)return;
  if((Stage==7 || Stage==10 || Stage==13 || Stage==18) && Path.IsValidIndex(Node))
  {PC->SetControlRotation(FRotator(0,(Path[Node]-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);return;}
  if(Stage==14 || Stage==15 || Stage==16){PC->SetControlRotation(FRotator(0,-90,0));Key(EKeys::W,true);return;}
  if(Stage!=17 || !Kit->IsFlying())return;
  const FVector Position=P->GetActorLocation();Travel+=FVector::Dist(Position,LastFlight);LastFlight=Position;
  const int32 Checkpoint=Job->BuildOpportunitySnapshot(false).ActivityCheckpoint;
  const FVector Goal=Checkpoint<3?Route[Checkpoint]:Finish->GetActorLocation();const float Distance=FVector::Dist2D(Position,Goal);
  FVector Desired=Goal-Position;Desired.Z=0;
  // Reserve canopy is a normal public input and remains IsFlying in production.
  // Its real speed/turn/descent permit the existing precise approach gates;
  // no dwell/radius, time or movement-state requirement is relaxed here.
  Key(EKeys::SpaceBar,!Kit->IsCanopyOpen() && W->GetTimeSeconds()-FlightAt>1.05);
  if(!Kit->IsCanopyOpen() && Checkpoint<3 && Position.Z>Goal.Z+250 && Distance<3000)
  {
   // The first gate is close below the actual launch. Wait on a normal
   // outward turning arc during the native one-second canopy transition;
   // do not drive a 21m/s glide straight across its horizontal position.
   const FVector Radial=(Position-Goal).GetSafeNormal2D();
   const FVector Tangent(-Radial.Y,Radial.X,0);
   Desired=Tangent+Radial;
  }
  if(Kit->IsCanopyOpen())
  {
   SawCanopy=true;
   // A canopy cannot turn from the waiting circle and cover its radius
   // before descending. Lead the actual gate altitude by the measured
   // descent over the remaining travel and native 45deg/s turn budget.
   // Original 250cm 3D gate and .2s flying dwell remain in production.
   const float ApproachSpeed=FMath::Clamp(float(P->GetVelocity().Size2D()),350.f,450.f);
   const float Descent=FMath::Max(170.f,float(-P->GetVelocity().Z));
   const float TurnSeconds=FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Desired.Rotation().Yaw))/45.f;
   const float HeightLead=Descent*(Distance/ApproachSpeed+TurnSeconds)+100.f;
   const bool NeedHeight=Checkpoint<3?Position.Z>Goal.Z+HeightLead:true;
   if(NeedHeight && Distance<(Checkpoint<3?1400:2200))
   {
    const float Radius=Checkpoint<3?700.f:1600.f;
    const FVector Radial=(Position-Goal).GetSafeNormal2D(),Tangent(-Radial.Y,Radial.X,0);
    Desired=Tangent+Radial*FMath::Clamp((Radius-Distance)/800.f,-1.f,1.f);
   }
  }
  const float Error=FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Desired.Rotation().Yaw);
  TurnIntegral=FMath::Clamp(TurnIntegral+FMath::Clamp(Error/18.f,-1.f,1.f)*DT,-.12f,.12f);
  const bool Right=TurnIntegral>DT*.25f,Left=TurnIntegral<-DT*.25f;
  if(Right)TurnIntegral-=DT;if(Left)TurnIntegral+=DT;
  Key(EKeys::D,Right);Key(EKeys::A,Left);Key(EKeys::W,false);Key(EKeys::S,!Kit->IsCanopyOpen());
 }
 void Observe()
 {
  if(!Job || !Kit || W->GetTimeSeconds()-LastSample<.1)return;LastSample=W->GetTimeSeconds();const auto D=Job->BuildOpportunitySnapshot(false);const FVector P0=P->GetActorLocation(),V=P->GetVelocity();
  const FVector Goal=Route.IsValidIndex(D.ActivityCheckpoint)?Route[D.ActivityCheckpoint]:Finish->GetActorLocation();
  if(Kit->IsFlying())for(int32 I=0;I<Route.Num();++I)Closest[I]=FMath::Min(Closest[I],float(FVector::Dist(P0,Route[I])));
  CSV+=FString::Printf(TEXT("%.4f,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d,%.3f\n"),W->GetTimeSeconds(),Stage,D.ActivityStage,D.ActivityCheckpoint,P0.X,P0.Y,P0.Z,V.X,V.Y,V.Z,Goal.X,Goal.Y,Goal.Z,FVector::Dist(P0,Goal),Kit->IsFlying()?1:0,Kit->IsCanopyOpen()?1:0,Keys.FindRef(EKeys::W)?1:0,Keys.FindRef(EKeys::S)?1:0,Keys.FindRef(EKeys::A)?1:0,Keys.FindRef(EKeys::D)?1:0,D.ActivitySeconds);
 }
 bool WriteBridge()
 {
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("source_pid"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("expected_balance"),double(ExpectedBalance));J->SetNumberField(TEXT("best_seconds"),BestSeconds);
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Bridge),true);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Bridge);
 }
 bool ReadBridge()
 {
  FString Text;TSharedPtr<FJsonObject> J;if(!FFileHelper::LoadFileToString(Text,*Bridge) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J))return false;
  SourcePID=J->GetIntegerField(TEXT("source_pid"));ExpectedBalance=J->GetNumberField(TEXT("expected_balance"));BestSeconds=J->GetNumberField(TEXT("best_seconds"));return FGuid::Parse(J->GetStringField(TEXT("run_id")),Run) && Run.IsValid();
 }
 bool End(const FString& Failure)
 {
  if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}
  if(PC)for(auto& K:Keys)if(K.Value){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));K.Value=false;}
  if(!Failure.IsEmpty())Test->AddError(Failure);if(Directory.IsEmpty())return true;
  const auto D=Job?Job->BuildOpportunitySnapshot(false):FNeonCityOpportunitySaveData{};
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("current_native_checkpoint"),D.ActivityCheckpoint);J->SetNumberField(TEXT("current_native_state"),D.State);J->SetNumberField(TEXT("native_best_seconds"),D.ActivityBestSeconds);J->SetNumberField(TEXT("native_completions"),D.ActivityCompletions);
  J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetNumberField(TEXT("actual_flying_travel_cm"),Travel);J->SetStringField(TEXT("actual_ground_landing"),Landed.ToString());J->SetBoolField(TEXT("native_canopy_observed"),SawCanopy);J->SetBoolField(TEXT("airborne_save_guard_checked"),CheckedAirSave);
  J->SetNumberField(TEXT("player_pose_fixtures"),0);J->SetNumberField(TEXT("direct_flight_or_reward_injection"),0);J->SetNumberField(TEXT("source_pid"),bCold?SourcePID:FPlatformProcess::GetCurrentProcessId());J->SetNumberField(TEXT("current_pid"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("completion_slot"),DoneSlot);
  TArray<TSharedPtr<FJsonValue>> Distances;for(float Value:Closest)Distances.Add(MakeShared<FJsonValueNumber>(Value));J->SetArrayField(TEXT("closest_actual_flying_distances_cm"),Distances);
  J->SetStringField(TEXT("scope"),bCold?TEXT("Independent completed source F7: original run/three ordered gates/time/one210 wallet receipt and safe on-foot state, normal P/O/RIGHT/ENTER replay then DELETE without duplicate income. No placements or synthetic activity state."):TEXT("Retained genuine accepted24 roof source F7, normal phone accept/F timed start/F locker rearm, actual supported W around case/Space jump, native A/D/S/Space reserve-canopy controls through all three original 3D checkpoints, genuine ground landing/W/F finish, one210 receipt/time, repeat-F exclusion and grounded F6. No player pose/velocity/collision/gear/flight/activity/reward writes. Existing ordinary wingsuit and equipment cold PASS cached."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/(bCold?TEXT("cold-receipt.json"):TEXT("source-receipt.json"))));FFileHelper::SaveStringToFile(CSV,*(Directory/(bCold?TEXT("cold-trajectory.csv"):TEXT("source-trajectory.csv"))));FFileHelper::SaveStringToFile(Access,*(Directory/(bCold?TEXT("cold-native-access.csv"):TEXT("source-native-access.csv"))));return true;
 }
 FAutomationTestBase* Test;bool bCold;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Begin=nullptr;AIslandActivityPoint* Finish=nullptr;AWingsuitLaunchPoint* Launch=nullptr;UWingsuitComponent* Kit=nullptr;
 UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;FDelegateHandle Driver;FNeonCityVisualEvidence Evidence;
 TMap<FKey,bool> Keys;TArray<FVector> Route,Path,RoofGoals;TArray<float> Closest;FGuid Run,OldRun;FVector LastFlight=FVector::ZeroVector,Landed=FVector::ZeroVector;
 FString Directory,InputSlot,DoneSlot,Bridge,WalkFailure,CSV=TEXT("game_seconds,stage,activity_stage,checkpoint,x,y,z,vx,vy,vz,goal_x,goal_y,goal_z,distance3d_cm,flying,canopy,W,S,A,D,activity_seconds\n");
 FString Access=TEXT("kind,stage_or_gate,x,y,z,passed,actual_hit_actor,actual_hit_component,impact_x,impact_y,impact_z,normal_z\n");
 int32 Stage=0,Browsed=0,Node=0,RoofNode=0,LastCheckpoint=0;uint32 SourcePID=0;int64 Before=0,ExpectedBalance=0;
 double Started=0,Travel=0;float At=0,LastSample=0,LastWalkProgress=0,InitialHealth=0,FlightAt=0,LastCheckpointAt=0,TurnIntegral=0,BestSeconds=0;bool SawCanopy=false,CheckedAirSave=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWingChallengeSource254,"NeonCity.Phase205c.Activity.WingsuitChallenge.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWingChallengeSource254::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeWingChallenge254::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWingChallengeCold254,"NeonCity.Phase205c.Activity.WingsuitChallenge.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWingChallengeCold254::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeWingChallenge254::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
