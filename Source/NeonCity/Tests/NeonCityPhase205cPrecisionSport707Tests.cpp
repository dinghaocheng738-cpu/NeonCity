#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandEstatePrecisionSport.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"

namespace PrecisionSport707
{
/** One supported fixture start. Every stroke thereafter uses normal F and
 * real enhanced LMB/Enter input; no ball/pin transform, score, clock or
 * gameplay state is injected. Actual rigid body telemetry is retained. */
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,EIslandPrecisionSport K,bool ContactOnly=false):Test(T),Kind(K),bContactOnly(ContactOnly),Evidence(K==EIslandPrecisionSport::Bowling?TEXT("Bowling707"):TEXT("Putting707")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>150)return End(TEXT("Finite real-physics practice timed out | ")+StateSummary());if(!P || W->GetTimeSeconds()<4)return false;
  Evidence.Record(W,PC,Stage);if(Facility)Record();const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
   for(TActorIterator<AIslandEstatePrecisionSport> I(W);I;++I)if(I->Kind==Kind && (Kind==EIslandPrecisionSport::Putting || I->StationID==TEXT("Estate707.Bowling.1")))Facility=*I;
   if(!Facility || !Facility->HasPresentation())return End(TEXT("Native retained lane/green playable actor or source assets missing"));
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_Precision707_")+FString(Kind==EIslandPrecisionSport::Bowling?TEXT("Bowling_"):TEXT("Putting_"))+FPaths::MakeValidFileName(Run.Left(22)),false) || Save->DoesSaveExist())return End(TEXT("Fresh precision test checkpoint required"));
   const FVector Ground=Facility->GetActorTransform().TransformPosition(Facility->ApproachLocal-FVector(0,0,92));FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(Precision707SupportedStart),false,P);Q.AddIgnoredActor(Facility);
   if(!W->LineTraceSingleByChannel(H,Ground+FVector(0,0,180),Ground-FVector(0,0,90),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Precision practice approach lacks genuine floor support"));
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Precision approach is physically blocked"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Entry=Stand;BeforeOwnerNoSee=P->GetMesh()->bOwnerNoSee;return Next(1);
  }
  if(Stage==1 && Age>.4f)
  {
   PC->SetControlRotation((Facility->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Facility)return End(TEXT("Normal F prompt cannot reach the physical lane/green"));Capture(TEXT("Prompt"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.25f)
  {
   Key(EKeys::F,false);if(!Facility->IsPlaying() || Facility->GetStage()!=EIslandPrecisionStage::Aiming)return End(TEXT("Actual normal F did not install scoped practice input"));
   if(Kind==EIslandPrecisionSport::Bowling && (PC->GetViewTarget()!=Facility || !PC->IsMoveInputIgnored()))return End(TEXT("Bowling lacks finite address camera/movement ownership"));
   if(Kind==EIslandPrecisionSport::Putting && (PC->GetViewTarget()!=P || PC->IsMoveInputIgnored()))return End(TEXT("Putting must allow real walking between actual lies"));
   if(Kind==EIslandPrecisionSport::Bowling)
   {
    int32 Lanes=0,Pins=0;for(TActorIterator<AIslandEstatePrecisionSport> I(W);I;++I)if(I->Kind==Kind){++Lanes;Pins+=I->Pins.Num();if(*I!=Facility && I->IsPlaying())return End(TEXT("More than one physical bowling lane owns the player"));}
    if(Lanes!=4 || Pins!=40)return End(TEXT("Retained four lanes must have exactly40 playable pins"));
    for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetStaticMeshComponent()->GetStaticMesh() && I->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_Room623_05") && !I->IsHidden())return End(TEXT("Original static40 pins remain stacked under dynamic pins"));
   }
   else
   {
    // A true unobstructed aperture is distinct from a shallow black cap.
    const FVector Cup=Facility->GetActorTransform().TransformPosition(Facility->HolePositions[0]);FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(Precision707ActualCupOpening),true,P);Q.AddIgnoredActor(Facility);
    if(!W->LineTraceSingleByChannel(H,Cup+FVector(0,0,15),Cup-FVector(0,0,28),ECC_Visibility,Q) || H.ImpactPoint.Z>Cup.Z-18 || H.ImpactPoint.Z<Cup.Z-26)return End(FString::Printf(TEXT("Actual cup lacks open collision and recessed physical base: %s"),*H.ImpactPoint.ToString()));
   }
   Capture(TEXT("Aiming"));return Next(3);
  }
  if(Stage==3)
  {
   if(!Facility->IsPlaying())return End(TEXT("Precision session exited while driver awaited the next lie | ")+StateSummary());
   if(Facility->GetStage()==EIslandPrecisionStage::Aiming)
   {
    if(Kind==EIslandPrecisionSport::Putting)
    {
     if(PC->IsMoveInputIgnored() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Current putting lie has no ordinary supported player movement | ")+StateSummary());
     const FVector To=Facility->GetShotStandLocation()-P->GetActorLocation();
     if(To.Size2D()>8){PC->SetControlRotation(To.Rotation());P->AddMovementInput(To.GetSafeNormal2D(),FMath::Clamp(To.Size2D()/100.f,.08f,1.f));Walked+=P->GetVelocity().Size2D()*FMath::Min(W->GetDeltaSeconds(),.1f);return false;}
     if(P->GetVelocity().Size2D()>4)return false;
    }
    ShotStart=Facility->GetBallPosition();MaxTravel=0;InitialShots=Facility->GetShotCount();
    // Wait on the actual gameplay meter. Request-to-Enhanced-Started latency
    // must not be counted as already-charged time.
    if(Kind==EIslandPrecisionSport::Putting)
    {const FVector Cup=Facility->GetActorTransform().TransformPosition(Facility->HolePositions[Facility->GetCompletedHoles()]);const float Speed=FVector::Dist2D(ShotStart,Cup)*Facility->RollingDamping+18;TargetPower=FMath::Clamp((Speed-Facility->MinimumSpeed)/(Facility->MaximumSpeed-Facility->MinimumSpeed),.02f,1.f);}
    else TargetPower=.78f;
    Hold=TargetPower*Facility->ChargeSeconds;Key(EKeys::LeftMouseButton,true);return Next(4);
   }
   if(Facility->GetStage()==EIslandPrecisionStage::Result)
   {
    if(MaxTravel<120 || Facility->GetShotCount()<=InitialShots)return End(TEXT("Real input stroke did not physically move the ball"));
    if(Kind==EIslandPrecisionSport::Putting && Facility->GetCompletedHoles()!=Completed+1)return End(TEXT("Put did not fall into its actual active cup"));
    Completed=Facility->GetCompletedHoles();Capture(TEXT("Actual-Result"));
    if(bContactOnly && Kind==EIslandPrecisionSport::Bowling){if(Facility->GetScore()<1 || Facility->GetPhysicsHitCount()<1)return End(TEXT("Actual hand release failed a real physical pin hit"));Key(EKeys::F6,true);return Next(7);}
    Key(EKeys::Enter,true);return Next(6);
   }
   if(Facility->GetStage()==EIslandPrecisionStage::Finished)
   {
    if(Kind==EIslandPrecisionSport::Bowling && (Facility->GetScore()<1 || Facility->GetScore()>30 || Facility->GetPhysicsHitCount()<1))return End(TEXT("Three-frame practice lacks actual physical pin hit/scoring evidence"));
    if(Kind==EIslandPrecisionSport::Putting && (Facility->GetCompletedHoles()!=3 || Facility->GetShotCount()!=3 || Walked<600))return End(TEXT("Three actual holes were not completed by three real key strokes"));
    Capture(TEXT("Finished"));Key(EKeys::F6,true);return Next(7);
   }
  }
  if(Stage==4)
  {
   if(Age>5)return End(TEXT("Real hold input failed to reach the actual charge meter"));
   if(Facility->GetStage()==EIslandPrecisionStage::Charging && Facility->GetPower()>=TargetPower)
   {
    auto* Pose=P->FindComponentByClass<UIslandPrecisionParticipant>();FTransform Right,Left;
    if(!Pose || !Pose->GetHandTargets(Right,Left) || P->GetMesh()->bOwnerNoSee)return End(TEXT("Finite actual player hand presentation is absent"));
    const auto* Prop=Kind==EIslandPrecisionSport::Bowling?Facility->BallVisual.Get():Facility->Putter.Get();
    const FVector Grip=Prop->GetComponentTransform().TransformPosition(Kind==EIslandPrecisionSport::Bowling?FVector(2.6,-1.5,10.1):FVector(-17.2,0,82));
    const FVector Support=Prop->GetComponentTransform().TransformPosition(Kind==EIslandPrecisionSport::Bowling?FVector(0,-9.5,0):FVector(-18,0,87));
    const float Contact=FVector::Dist(Grip,P->GetMesh()->GetSocketLocation(TEXT("hand_r"))),SupportContact=FVector::Dist(Support,P->GetMesh()->GetSocketLocation(TEXT("hand_l")));
    if(Contact>4 || SupportContact>15 || FVector::Dist(Right.GetLocation(),P->GetMesh()->GetSocketLocation(TEXT("hand_r")))>14)return End(FString::Printf(TEXT("Actual player grip/contact is invalid: right%.2f left%.2f"),Contact,SupportContact));
    ActualPower=Facility->GetPower();Key(EKeys::LeftMouseButton,false);Capture(TEXT("Real-Release"));return Next(5);
   }
  }
  if(Stage==5)
  {
   MaxTravel=FMath::Max(MaxTravel,float(FVector::Dist2D(ShotStart,Facility->GetBallPosition())));
   if(Facility->GetStage()==EIslandPrecisionStage::Aiming)return End(FString::Printf(TEXT("Real putting stroke rested before entering cup; travel%.2f hold%.3f power%.3f launch%.3f current%s"),MaxTravel,Hold,ActualPower,Facility->GetLaunchSpeed(),*Facility->GetBallPosition().ToString()));
   if(Facility->GetStage()==EIslandPrecisionStage::Result)return Next(3);
  }
  if(Stage==6 && Age>.2f){Key(EKeys::Enter,false);return Next(3);}
  if(Stage==7 && Age>.7f)
  {
   Key(EKeys::F6,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Save->ReadSaveForInspection())return End(TEXT("Scoped sport input swallowed normal F6 checkpoint"));
   Key(EKeys::F7,true);return Next(8);
  }
  if(Stage==8 && Age>.8f)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || Facility->IsPlaying() || PC->IsMoveInputIgnored() || PC->GetViewTarget()!=P || P->GetMesh()->bOwnerNoSee!=BeforeOwnerNoSee)return End(TEXT("Normal F7 failed finite sport view/input/owner visibility cleanup"));
   Capture(TEXT("F7-Walking-Restored"));return End(TEXT(""));
  }
  return false;
 }
 void Record()
 {
  if(W->GetTimeSeconds()-LastSample<.05f)return;LastSample=W->GetTimeSeconds();const FVector B=Facility->GetBallPosition(),PlayerPosition=P->GetActorLocation();
  const float CupDistance=Facility->Kind==EIslandPrecisionSport::Putting?FVector::Dist2D(B,Facility->GetActorTransform().TransformPosition(Facility->HolePositions[FMath::Min(Facility->GetCompletedHoles(),2)])):0;
  Telemetry+=FString::Printf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f\n"),W->GetTimeSeconds(),int(Facility->GetStage()),Facility->GetShotCount(),B.X,B.Y,B.Z,Facility->Ball->GetPhysicsLinearVelocity().Size(),Facility->GetFallenPins(),Facility->GetCompletedHoles(),Facility->GetPhysicsHitCount(),ActualPower,Facility->GetLaunchSpeed(),Walked,Stage,Facility->IsPlaying(),PlayerPosition.X,PlayerPosition.Y,PlayerPosition.Z,P->GetVelocity().Size2D(),int(P->GetCharacterMovement()->MovementMode),PC->IsMoveInputIgnored(),FVector::Dist2D(PlayerPosition,Facility->GetShotStandLocation()),Facility->GetPower(),CupDistance);
 }
 FString StateSummary()const
 {
  if(!Facility || !P || !PC)return TEXT("player/facility unavailable");
  FHitResult Block;FCollisionQueryParams Q(SCENE_QUERY_STAT(Precision707WalkDiagnostic),false,P);const FVector To=Facility->GetShotStandLocation();
  const bool Blocked=W->SweepSingleByChannel(Block,P->GetActorLocation(),To,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
  return FString::Printf(TEXT("driver=%d activity=%d playing=%d shots=%d cups=%d power=%.3f ball=%s player=%s speed=%.3f mode=%d ignored=%d stance=%.3f base=%s sweepBlock=%d %s/%s"),Stage,int(Facility->GetStage()),Facility->IsPlaying(),Facility->GetShotCount(),Facility->GetCompletedHoles(),Facility->GetPower(),*Facility->GetBallPosition().ToString(),*P->GetActorLocation().ToString(),P->GetVelocity().Size2D(),int(P->GetCharacterMovement()->MovementMode),PC->IsMoveInputIgnored(),FVector::Dist2D(P->GetActorLocation(),To),*GetNameSafe(P->GetMovementBase()),Blocked,*GetNameSafe(Block.GetActor()),*GetNameSafe(Block.GetComponent()));
 }
 void Capture(const TCHAR* Label){FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Precision707-%s-%s.png"),Kind==EIslandPrecisionSport::Bowling?TEXT("Bowling"):TEXT("Putting"),Label));}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& H=Held.FindOrAdd(K);if(H!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));H=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(const auto& K:Held)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(Facility)Facility->FinishSession();if(!Error.IsEmpty())Test->AddError(Error);
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/(Kind==EIslandPrecisionSport::Bowling?TEXT("Bowling707"):TEXT("Putting707"));IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(Telemetry,*(Dir/TEXT("actual-rigid-body.csv")));FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nstage=%d\nfailure=%s\nOne declared supported player start. Normal F + scoped enhanced LMB/Enter, real supported player movement toward current lie; no ball/pin pose, velocity, score, timing or hole capture injection. Continuous actual rigid-body telemetry, normal F6 and F7 restore. User handling/audio review remains separate.\n"),Error.IsEmpty(),Stage,*Error),*(Dir/TEXT("scope.txt")));return true;
 }
 FAutomationTestBase* Test;EIslandPrecisionSport Kind;bool bContactOnly=false;FNeonCityVisualEvidence Evidence;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandEstatePrecisionSport* Facility=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 int32 Stage=0,InitialShots=0,Completed=0;double Start=0;float At=0,Hold=0,MaxTravel=0,LastSample=0,TargetPower=0,ActualPower=0,Walked=0;bool BeforeOwnerNoSee=false;FVector Entry,ShotStart;TMap<FKey,bool> Held;
 FString Telemetry=TEXT("seconds,stage,shots,ball_x,ball_y,ball_z,speed_cm_s,fallen_pins,completed_cups,physics_hits,actual_charge,launch_cm_s,walked_cm,driver_stage,playing,player_x,player_y,player_z,player_speed_cm_s,movement_mode,move_ignored,stance_distance_cm,current_charge,cup_distance_cm\n");
};
}
#define PRECISION707_TEST(Class,Name,Kind) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PrecisionSport707::FFlow(this,EIslandPrecisionSport::Kind));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
PRECISION707_TEST(FBowling707Native,"NeonCity.Phase205c.PrecisionSport707.BowlingPIE",Bowling)
PRECISION707_TEST(FPutting707Native,"NeonCity.Phase205c.PrecisionSport707.PuttingPIE",Putting)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBowling707Hands,"NeonCity.Phase205c.PrecisionSport707.BowlingHandContactPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBowling707Hands::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PrecisionSport707::FFlow(this,EIslandPrecisionSport::Bowling,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#undef PRECISION707_TEST
#endif
