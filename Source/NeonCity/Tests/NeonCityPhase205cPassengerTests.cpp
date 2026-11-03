#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityPhase205cVehicleTurnSurvey.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/IslandSupercar.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeIslandTaxi
{
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,int32 S):Test(T),Session(S),Evidence(S==0?TEXT("TaxiSource"):S==1?TEXT("TaxiResume"):TEXT("TaxiCold")){}
 ~FFlow(){if(Driver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Driver);}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>230)return End(TEXT("Native taxi timed out"));if(!WalkFailure.IsEmpty())return End(WalkFailure);
  if(!PC || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(57700,-159000,300))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Taxi.FishingHouse"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job){if(It->Index==0)Pickup=*It;else if(It->Index==1)Dropoff=*It;}
   auto* GI=W->GetGameInstance();Phone=PC->FindComponentByClass<UPhoneComponent>();Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=GI->GetSubsystem<UNeonCityEconomySubsystem>();Ownership=GI->GetSubsystem<UVehicleOwnershipSubsystem>();
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityTaxiRun="),Suffix);Suffix=FPaths::MakeValidFileName(Suffix.IsEmpty()?TEXT("01"):Suffix);MidSlot=TEXT("NeonCity_Test_TaxiMid")+Suffix;DoneSlot=TEXT("NeonCity_Test_TaxiDone")+Suffix;Bridge=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("Taxi-")+Suffix);
   if(!P || !Job || !Pickup || !Dropoff || !Phone || !Nav || !Wallet || !Ownership || !Save || !Save->ConfigureSlot(Session==2?DoneSlot:MidSlot,false))return End(TEXT("Actual taxi/player/phone/navigation/save domains missing"));
   if(Session>0){if(!Save->DoesSaveExist() || !ReadBridge(Session==1?TEXT("Source.json"):TEXT("Completed.json")))return End(TEXT("Independent genuine prior taxi source/completion required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*(Bridge/TEXT("Source.json"))))return End(TEXT("Preserve prior genuine taxi evidence; use a fresh explicit run suffix after changes"));
   double Best=BIG_NUMBER;for(TActorIterator<AIslandSupercar> It(W);It;++It)if(Ownership->IsOwned(It->VehicleID) && It->GetVelocity().Size()<80){const double D=FVector::DistSquared2D(It->GetActorLocation(),Pickup->GetActorLocation());if(D<Best){Best=D;Car=*It;}}
   if(!Car)return End(TEXT("No original owned stock taxi car available; no vehicle was invented"));CarID=Car->VehicleID;
   FVector Stand;if(!Phase205cVehicleApproach(W,P,Car,Car->GetActorLocation()-Car->GetActorRightVector()*215,Stand))return End(TEXT("Original taxi car has no supported visible collision-clear on-foot approach"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();PhoneGoal=1;Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);const auto* M=Save->ReadSaveForInspection();const auto D=Job->BuildOpportunitySnapshot(false);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || D.ActivityRunID!=Run || Wallet->GetBalance()!=ExpectedBalance || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>20)return End(TEXT("Independent F7 lost genuine taxi run/wallet/safe player"));Before=Wallet->GetBalance();
   if(Session==2){if(D.State!=2 || D.ActivityCompletions!=1 || Job->GetActivityNPC() || Wallet->HasCredit(Job->GetReceiptID())==false)return End(TEXT("Cold taxi completion lost one receipt/count or left a guest"));PhoneGoal=3;Key(EKeys::P,true);return Next(1);}
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->VehicleID==CarID)Car=*It;
   const auto* V=M->Vehicles.FindByPredicate([&](const auto& Value){return Value.VehicleID==CarID;});
   if(!Car || !V || !Ownership->IsOwned(CarID) || FVector::Dist(Car->GetActorLocation(),V->Transform.GetLocation())>35 || D.State!=1 || D.ActivityStage!=2 || D.VehicleID!=CarID || !D.ActivityTransportPickupValid || FVector::Dist(D.ActivityTransportPickup,BoardingOrigin)>5 || D.ActivityPassengerExiting || !Seated() || Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Independent F7 reset passenger pickup or lost actual saved transport/boarding origin/visible seated guest"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Taxi-Cold-Actual-Seated-Guest.png"));return Next(6);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P phone input failed"));if(Phone->GetActiveApp()==ENeonCityPhoneApp::Opportunities){Browsed=0;return Next(3);}Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);Browsed=0;return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Actual taxi phone offer/action undiscoverable"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);
   if(PhoneGoal==2){if(D.State!=1 || D.ActivityStage<1 || !Job->GetActivityNPC())return End(TEXT("Native phone action did not start real passenger approach"));Key(EKeys::P,true);return Next(12);}
   if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run || !Job->GetActivityNPC())return End(TEXT("Native ENTER did not create a fresh real visitor run"));
   if(PhoneGoal==3){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Native taxi replay duplicated prior income"));Key(EKeys::Delete,true);return Next(30);}
   Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==6 && Age>.2)
  {
   Key(EKeys::P,false);FVector Door;if(!Phase205cVehicleApproach(W,P,Car,P->GetActorLocation(),Door))return End(TEXT("Original taxi car approach became obstructed"));if(!WalkTo(Door,50))return false;Key(EKeys::W,false);PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Original taxi vehicle normal F eye-trace blocked"));Key(EKeys::F,true);return Next(7);
  }
  if(Stage==7 && Age>.25)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Native F did not board original taxi car"));
   if(!BuildPath(Session==0?Pickup->GetActorLocation():Dropoff->GetActorLocation()))return End(TEXT("Actual shared road guide has no connected taxi route"));return Next(Session==0?8:22);
  }
  if(Stage==8 || Stage==14 || Stage==22)
  {
   if(Job->BuildOpportunitySnapshot(false).State!=1)return End(TEXT("Taxi failed during actual transport"));if(Stage!=8 && !Seated())return End(TEXT("Real seated passenger disappeared during physical taxi movement"));
   if(FVector::Dist2D(Car->GetActorLocation(),Marker)>20){Marker=Car->GetActorLocation();LastProgress=W->GetTimeSeconds();}
   const bool Reached=Stage==8?FVector::Dist2D(Car->GetActorLocation(),Pickup->GetActorLocation())<330:Stage==14?FVector::Dist2D(Car->GetActorLocation(),BoardingOrigin)>1800:FVector::Dist2D(Car->GetActorLocation(),Dropoff->GetActorLocation())<600;
   if(Reached){StopDrive();return Next(Stage==8?9:Stage==14?15:23);}
   if(W->GetTimeSeconds()-LastProgress>12)return End(FString::Printf(TEXT("Actual taxi path blocked at%s node%d; no vehicle correction pose"),*Car->GetActorLocation().ToString(),PathNode));
  }
  if(Stage==9 && Age>.6 && Car->GetVelocity().Size()<25){PhoneGoal=2;Key(EKeys::P,true);return Next(1);}
  if(Stage==12)
  {
   if(Age>.2)Key(EKeys::P,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1)return End(TEXT("Actual passenger boarding failed"));
   if(D.ActivityStage==2){if(!Seated() || Wallet->GetBalance()!=Before)return End(TEXT("Actual passenger seat or unpaid state missing"));BoardingOrigin=D.ActivityTransportPickup;if(!D.ActivityTransportPickupValid || D.VehicleID!=CarID || FVector::Dist2D(BoardingOrigin,Pickup->GetActorLocation())>450)return End(TEXT("Actual passenger boarding snapshot lost original car/pickup"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Taxi-Native-Seated-Guest.png"));if(!BuildPath(Dropoff->GetActorLocation()))return End(TEXT("Actual dropoff road route missing"));return Next(14);}
   if(Age>25)return End(TEXT("Real visitor could not physically reach the stopped passenger door"));
  }
  if(Stage==15 && Age>.6 && Car->GetVelocity().Size()<25){if(!Seated() || Wallet->GetBalance()!=Before)return End(TEXT("Unpaid taxi lost real passenger before mid-save"));Key(EKeys::F6,true);return Next(16);}
  if(Stage==16 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();const auto* D=M?M->Phone.Opportunities.FindByPredicate([&](const auto& Value){return Value.OpportunityID==Job->ActivityID;}):nullptr;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !D || D->State!=1 || D->ActivityStage!=2 || D->ActivityRunID!=Run || D->VehicleID!=CarID || !D->ActivityTransportPickupValid || D->ActivityPassengerExiting || FVector::Dist(D->ActivityTransportPickup,BoardingOrigin)>5)return End(TEXT("Genuine F6 lost unpaid actual passenger/transport/pickup stage"));
   if(!WriteBridge(TEXT("Source.json")))return End(TEXT("Could not preserve genuine taxi source manifest"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Taxi-Native-Mid-Transport-Saved.png"));return End(TEXT(""));
  }
  if(Stage==23 && Age>.6 && Car->GetVelocity().Size()<25){Key(EKeys::F,true);return Next(24);}
  if(Stage==24 && Age>.25){Key(EKeys::F,false);if(PC->GetPawn()!=P || P->IsHidden() || !P->GetCharacterMovement()->IsMovingOnGround() || !Seated())return End(TEXT("Native taxi arrival F exit lost real player or passenger"));return Next(25);}
  if(Stage==25)
  {
   if(!WalkTo(Dropoff->GetActorLocation()+FVector(-150,-100,0),45))return false;Key(EKeys::W,false);PC->SetControlRotation((Dropoff->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Dropoff)return End(TEXT("Native taxi dropoff F prompt blocked after genuine on-foot arrival"));Key(EKeys::F,true);return Next(26);
  }
  if(Stage==26 && Age>.2)
  {
   Key(EKeys::F,false);auto* Guest=Job->GetActivityNPC();const auto D=Job->BuildOpportunitySnapshot(false);
   if(D.State==2)return End(TEXT("Taxi completed before its guest actually walked away"));
   if(!Guest || Guest->IsHidden() || !Guest->GetActorEnableCollision() || !Guest->GetCharacterMovement()->IsMovingOnGround() || !D.ActivityPassengerExiting)return End(TEXT("Native dropoff F did not return the actual visitor to safe walking"));LastGuest=Guest->GetActorLocation();LastGuestAt=W->GetTimeSeconds();return Next(27);
  }
  if(Stage==27)
  {
   const auto D=Job->BuildOpportunitySnapshot(false);auto* Guest=Job->GetActivityNPC();
   if(Guest){const float Now=W->GetTimeSeconds();const float Delta=FVector::Dist2D(Guest->GetActorLocation(),LastGuest);if(Now>LastGuestAt && Delta/(Now-LastGuestAt)>350)return End(TEXT("Guest exit moved faster than actual walking"));GuestWalk+=Delta;LastGuest=Guest->GetActorLocation();LastGuestAt=Now;}
   if(D.State==2){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()) || Guest || GuestWalk<50)return End(TEXT("Actual guest walk-away/cleanup/single payment failed"));if(!Save->ConfigureSlot(DoneSlot,false) || Save->DoesSaveExist())return End(TEXT("Preserve prior genuine completed-taxi save"));return Next(28);}
   if(D.State!=1 || Age>20)return End(TEXT("Real visitor walk-away failed or blocked at dropoff"));
  }
  if(Stage==28 && Age>.2){Key(EKeys::F6,true);return Next(29);}
  if(Stage==29 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityCompletions==1 && D.ActivityRunID==Run;}))return End(TEXT("Genuine taxi completion F6 failed"));
   if(!WriteBridge(TEXT("Completed.json")))return End(TEXT("Could not preserve actual taxi completion manifest"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Taxi-Native-Completed-Saved.png"));return End(TEXT(""));
  }
  if(Stage==30 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Job->GetActivityNPC() || Wallet->GetBalance()!=Before)return End(TEXT("Native taxi replay/cancel left guest or duplicated reward"));return End(TEXT(""));}
  return false;
 }
private:
 bool Seated() const
 {
  auto* Guest=Job?Job->GetActivityNPC():nullptr;if(!Guest || !Car || !Guest->IsHidden() || Guest->GetActorEnableCollision() || Guest->GetAttachParentActor()!=Car)return false;
  TArray<UTransportOccupantComponent*> Seats;Car->GetComponents(Seats);int32 Count=0;for(auto* Seat:Seats)if(Seat && Seat->GetRepresentedDriver()==Guest && Seat->GetDriverMesh() && Seat->GetDriverMesh()->IsVisible())++Count;return Count==1;
 }
 bool WalkTo(FVector Goal,float Radius)
 {
  if(FVector::Dist2D(P->GetActorLocation(),Goal)<Radius){Key(EKeys::W,false);return true;}
  if(WalkPath.IsEmpty() || FVector::Dist2D(WalkGoal,Goal)>120){WalkGoal=Goal;WalkNode=0;if(!Phase205cSupportedWalkPath(W,P,Goal,WalkPath)){WalkFailure=TEXT("No actual supported collision-clear taxi walking path");return false;}}
  while(WalkNode<WalkPath.Num()-1 && FVector::Dist2D(P->GetActorLocation(),WalkPath[WalkNode])<40)++WalkNode;const FVector Aim=WalkNode==WalkPath.Num()-1?Goal:WalkPath[WalkNode];PC->SetControlRotation(FRotator(0,(Aim-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);
  if(W->GetTimeSeconds()-At>30){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TaxiWalkBlocker),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+P->GetActorForwardVector()*120,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);WalkFailure=FString::Printf(TEXT("Native taxi held-W walk blocked by%s/%s at%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*P->GetActorLocation().ToString());}return false;
 }
 bool BuildPath(FVector Goal){Nav->RefreshRoadGuide();if(!Nav->GetRoadGuide().FindRoute(Car->GetActorLocation(),Goal,Path) || Path.Num()<2)return false;PathNode=1;Turning=false;TurnReverse=false;TurnAllowed=false;NextTurnProbe=0;Marker=Car->GetActorLocation();LastProgress=W->GetTimeSeconds();if(!Driver.IsValid())Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FFlow::DriveFrame);return true;}
 bool TurnClear(bool Reverse,float Steer,FString& Failure)const
 {
  // Confined three-point primitive:50cm actual leg plus50cm braking margin.
  // Full native body shapes and25cm support/sweep gates are unchanged.
  return Phase205cNativeVehicleTurnSurvey(W,Car,Car->GetMesh(),Reverse,Steer,Failure,100.f);
 }
 void DriveFrame(UWorld* World,ELevelTick,float)
 {
  if(World!=W || !PC || !Car || PC->GetPawn()!=Car || (Stage!=8 && Stage!=14 && Stage!=22))return;
  while(PathNode<Path.Num()-1 && FVector::Dist2D(Car->GetActorLocation(),Path[PathNode])<170)++PathNode;
  FVector Target=Path[PathNode];const float D=FVector::Dist2D(Car->GetActorLocation(),Target);const FVector RawLocal=Car->GetActorTransform().InverseTransformVectorNoScale(Target-Car->GetActorLocation());
  if(!Turning && RawLocal.X>0 && FMath::Abs(FMath::Atan2(RawLocal.Y,RawLocal.X))<FMath::DegreesToRadians(65.f) && D<600 && PathNode<Path.Num()-1)
  {
   // Keep the same native input controller on the next measured segment.
   // Extending600cm past a150cm corner cuts through the village buildings;
   // this is the same bounded-lookahead defect observed in Delivery.B256.
   const FVector Segment=Path[PathNode+1]-Target;
   Target+=Segment.GetSafeNormal2D()*FMath::Min(double(600-D),Segment.Size2D());
  }
  const FVector Local=Car->GetActorTransform().InverseTransformVectorNoScale(Target-Car->GetActorLocation());const float Speed=FVector::DotProduct(Car->GetVelocity(),Car->GetActorForwardVector());const float Bearing=FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)),Angle=FMath::Abs(Bearing);float Desired=Angle>20?250.f:400.f;
  const FVector Goal=Stage==8?Pickup->GetActorLocation():Dropoff->GetActorLocation();if(FVector::Dist2D(Car->GetActorLocation(),Goal)<1200)Desired=FMath::Min(Desired,250.f);
  float Steer=FMath::Clamp(float(FMath::RadiansToDegrees(FMath::Atan2(560*Local.Y,FMath::Max(1.,Local.SizeSquared2D())))/32),-1.f,1.f),Drive=0,Brake=0;FString Mode=TEXT("forward"),Clearance;
  if(Local.X<-40 && FMath::Abs(Local.Y)<80 && D<260 && !Turning)
  {
   Mode=TEXT("short_reverse");Desired=120;if(!TurnClear(true,Steer,Clearance)){Brake=1;Mode=TEXT("reverse_clearance_stop");}else {Drive=-FMath::Clamp((Desired+Speed)/120.f,0.f,1.f);Brake=Speed<-Desired-20?.5f:0.f;}
  }
  else
  {
   if(!Turning && Angle>70){Turning=true;TurnSide=FMath::Abs(Local.Y)>10?FMath::Sign(Local.Y):1.f;TurnReverse=false;TurnLegOrigin=Car->GetActorLocation();NextTurnProbe=0;}
   if(Turning && Angle<55 && Local.X>100){Turning=false;TurnReverse=false;}
   if(Turning)
   {
    Mode=TurnReverse?TEXT("turn_reverse"):TEXT("turn_forward");Desired=75;
    if(FMath::Abs(Local.Y)>80 && FMath::Sign(Local.Y)!=TurnSide)
    {if(FMath::Abs(Speed)>25){Brake=1;Mode=TEXT("waypoint_side_braking");}else{TurnSide=FMath::Sign(Local.Y);TurnAllowed=false;NextTurnProbe=0;TurnLegOrigin=Car->GetActorLocation();}}
    const float Now=W->GetTimeSeconds();if(Now>=NextTurnProbe)
    {
     NextTurnProbe=Now+.2f;FString Why;const bool CurrentClear=TurnClear(TurnReverse,TurnReverse?-TurnSide:TurnSide,Why);
     if(!CurrentClear || FVector::Dist2D(Car->GetActorLocation(),TurnLegOrigin)>50)
     {
      FString OtherWhy;if(FMath::Abs(Speed)>25){Brake=1;Mode=TEXT("turn_leg_braking");NextTurnProbe=Now;}
      else if(TurnClear(!TurnReverse,TurnReverse?TurnSide:-TurnSide,OtherWhy)){TurnReverse=!TurnReverse;TurnLegOrigin=Car->GetActorLocation();TurnAllowed=true;}
      // A completed50cm leg requests a direction change; it is not a
      // collision. If the alternate arc is blocked but this arc still passes
      // the unchanged full-body/support sweep, start another bounded leg in
      // the safe current direction after the same native stop-speed check.
      else if(CurrentClear){TurnLegOrigin=Car->GetActorLocation();TurnAllowed=true;Clearance=TEXT("retained clear current leg; alternate blocked: ")+OtherWhy;}
      else {TurnAllowed=false;Clearance=Why+TEXT(" | ")+OtherWhy;}
     }
     else TurnAllowed=true;
    }
    Steer=TurnReverse?-TurnSide:TurnSide;if(!TurnAllowed){Brake=1;Mode=TEXT("turn_clearance_stop");}
    if(Brake==0){Drive=TurnReverse?-FMath::Clamp((Desired+Speed)/100.f,0.f,1.f):FMath::Clamp((Desired-Speed)/100.f,0.f,1.f);Brake=(TurnReverse?-Speed:Speed)>Desired+20?.5f:0.f;}
   }
   else {Drive=FMath::Clamp((Desired-Speed)/180.f,0.f,1.f);Brake=Speed>Desired+30?.4f:0.f;}
  }
  if(Brake>0)Drive=0;auto* Input=Car->GetDrivingComponent();Input->SetSteerInput(Steer);Input->SetDriveInput(Drive);Input->SetBrakeInput(Brake);
  if(W->GetTimeSeconds()-LastDriverRecord>.1f){LastDriverRecord=W->GetTimeSeconds();DriverEvidence+=FString::Printf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%.3f,%.3f,%.3f,"),LastDriverRecord,Stage,PathNode,Car->GetActorLocation().X,Car->GetActorLocation().Y,Car->GetActorLocation().Z,Local.X,Local.Y,Speed,*Mode,Steer,Drive,Brake)+FString(TEXT("\""))+Clearance.Replace(TEXT("\""),TEXT("\"\""))+TEXT("\"\n");}
 }
 void StopDrive(){if(Car){auto* I=Car->GetDrivingComponent();I->SetDriveInput(0);I->SetSteerInput(0);I->SetBrakeInput(1);}}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool ReadBridge(const TCHAR* Name)
 {
  FString Text;TSharedPtr<FJsonObject> J;if(!FFileHelper::LoadFileToString(Text,*(Bridge/Name)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid())return false;double PID=0,X=0,Y=0,Z=0;FString R,V;
  if(!J->TryGetNumberField(TEXT("process_id"),PID) || uint32(PID)==FPlatformProcess::GetCurrentProcessId() || !J->TryGetStringField(TEXT("run_id"),R) || !J->TryGetStringField(TEXT("car_id"),V) || !J->TryGetNumberField(TEXT("balance"),ExpectedBalance) || !J->TryGetNumberField(TEXT("pickup_x"),X) || !J->TryGetNumberField(TEXT("pickup_y"),Y) || !J->TryGetNumberField(TEXT("pickup_z"),Z))return false;BoardingOrigin=FVector(X,Y,Z);return FGuid::Parse(R,Run) && FGuid::Parse(V,CarID);
 }
 bool WriteBridge(const TCHAR* Name)
 {
  IFileManager::Get().MakeDirectory(*Bridge,true);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("car_id"),CarID.ToString());J->SetNumberField(TEXT("balance"),double(Wallet->GetBalance()));J->SetNumberField(TEXT("pickup_x"),BoardingOrigin.X);J->SetNumberField(TEXT("pickup_y"),BoardingOrigin.Y);J->SetNumberField(TEXT("pickup_z"),BoardingOrigin.Z);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*(Bridge/Name));
 }
 bool End(const FString& Error)
 {
  if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}StopDrive();if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("session"),Session);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("car_id"),CarID.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetNumberField(TEXT("actual_guest_exit_walk_cm"),GuestWalk);J->SetNumberField(TEXT("post_setup_pose_writes"),0);
  J->SetStringField(TEXT("scope"),Session==0?TEXT("One supported on-foot start beside an original owned stock car. Native phone acceptance, real F board, actual per-frame existing gameplay driver ports follow shared navigation to pickup and stop; phone P/O/ENTER causes visible actual NPC approach/boarding. Seated real guest then travels over18m; stop and genuine F6 retain original run/car/boarding point without reward. No vehicle/NPC poses, state, ownership, reward or completion fixtures."):Session==1?TEXT("Independent actual source process check; genuine F7 must retain Stage2/original car/boarding origin and exactly one visible represented passenger, with zero pose/state fixtures. Real F board and gameplay input ports drive to the actual dropoff; native F exit/W/F dropoff, actual visible collision walking guest trajectory, cleanup/one shared180credit/F6. Whole guest appearance, village circulation and handling remain separate."):TEXT("Independent genuine completed-taxi F7 preserves single income/receipt/completion and no orphan guest; actual phone replay/DELETE cancellation creates/removes fresh guest without another payment. Zero pose/state/reward fixtures."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(Session==0?TEXT("TaxiSource"):Session==1?TEXT("TaxiResume"):TEXT("TaxiCold"));IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,node,x,y,z,goal_local_x,goal_local_y,signed_speed,driver_mode,steer,drive,brake,turn_clearance\n")+DriverEvidence,*(Dir/TEXT("physical-driver.csv")));return true;
 }
 FAutomationTestBase* Test;int32 Session;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Pickup=nullptr;AIslandActivityPoint* Dropoff=nullptr;AIslandSupercar* Car=nullptr;UPhoneComponent* Phone=nullptr;UNeonCityNavigationComponent* Nav=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;UVehicleOwnershipSubsystem* Ownership=nullptr;TMap<FKey,bool> Keys;TArray<FVector> Path,WalkPath;int32 WalkNode=0;FVector WalkGoal=FVector::ZeroVector;FDelegateHandle Driver;FNeonCityVisualEvidence Evidence;int32 Stage=0,Browsed=0,PhoneGoal=1,PathNode=1;double Started=0,ExpectedBalance=0;float At=0,LastProgress=0,GuestWalk=0,LastGuestAt=0;int64 Before=0;FGuid Run,CarID;FVector Marker=FVector::ZeroVector,BoardingOrigin=FVector::ZeroVector,LastGuest=FVector::ZeroVector;FString MidSlot,DoneSlot,Bridge,WalkFailure;
 bool Turning=false,TurnReverse=false,TurnAllowed=false;float TurnSide=1,NextTurnProbe=0,LastDriverRecord=0;FVector TurnLegOrigin=FVector::ZeroVector;FString DriverEvidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTaxiSource,"NeonCity.Phase205c.Activity.Taxi.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTaxiSource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTaxi::FFlow(this,0));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTaxiResume,"NeonCity.Phase205c.Activity.Taxi.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTaxiResume::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTaxi::FFlow(this,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandTaxiResultCold,"NeonCity.Phase205c.Activity.Taxi.C",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandTaxiResultCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandTaxi::FFlow(this,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
