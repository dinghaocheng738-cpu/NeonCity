#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/AutomationEditorCommon.h"
#include "Vehicles/Marine/NeonCityRowboat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NeonCityRowboatTest
{
void Key(APlayerController* PC,FKey Name,EInputEvent Event)
{
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Name,Event,FPlatformTime::Cycles64()));
}

/** West Lake rowboats in the live map: moored afloat at the Hubin landing, board / step ashore,
 * the hired ride (boatman rows out and brings the boat back alongside), then self-rowing:
 * stroke speed, turning, holding water. Writes Saved/WestLake/RowboatTest.json and, with
 * -NeonCityArtStills, stills of the boarding and every stroke phase. */
class FScenario : public IAutomationLatentCommand
{
public:
 explicit FScenario(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>1500)return Finish(TEXT("Scenario exceeded 1500 s."));
  if(Stage==0)return Initialize();
  if(!IsValid(W) || !IsValid(PC) || !IsValid(Player) || !IsValid(Self) || !IsValid(Hired))return Finish(TEXT("World, player or boats became unavailable."));
  const float E=W->GetTimeSeconds()-At;
  for(ANeonCityRowboat* Boat:{Self.Get(),Hired.Get()})
  {MaxRoll=FMath::Max(MaxRoll,FMath::Abs(Boat->GetActorRotation().Roll));MaxPitch=FMath::Max(MaxPitch,FMath::Abs(Boat->GetActorRotation().Pitch));}
  if(IsValid(Close))FollowClose();
  if(Stage==3 || Stage==4)
   if(W->GetTimeSeconds()>NextTrace)
   {
    NextTrace=W->GetTimeSeconds()+5;
    const FVector L=Hired->GetActorLocation();
    Trace+=FString::Printf(TEXT("t%.0f s%d (%.0f,%.0f) yaw%.0f v%.0f wp%d dock%d moored%d tour%d oars%d/%d blk%d/%d rate%.0f dist%.0f | "),
     W->GetTimeSeconds(),Stage,L.X/100,L.Y/100,Hired->GetActorRotation().Yaw,Hired->GetVelocity().Size2D(),Hired->GetTourWaypoint(),
     Hired->IsDocked(),Hired->GetMarineMovement()->IsMoored(),Hired->IsTourActive(),int32(Hired->GetOar(0).Mode),int32(Hired->GetOar(1).Mode),
     Hired->GetOar(0).bBlocked,Hired->GetOar(1).bBlocked,Hired->GetStrokeRate(),FVector::Dist2D(L,HiredBerth)/100);
    Metrics->SetStringField(TEXT("ride_trace"),Trace);
   }
  if(Stage==-1)return UpdatePending();
  switch(Stage)
  {
  case 1: // moored and afloat
   if(E<6)return false;
   for(ANeonCityRowboat* Boat:{Self.Get(),Hired.Get()})
   {
    Metrics->SetNumberField(Boat==Self?TEXT("self_float_z"):TEXT("hired_float_z"),Boat->GetActorLocation().Z);
    TArray<FHitResult> Hits;FCollisionQueryParams Q(SCENE_QUERY_STAT(RowboatProbe),true,Boat);
    W->LineTraceMultiByChannel(Hits,Boat->GetActorLocation()+FVector(0,0,400),Boat->GetActorLocation()-FVector(0,0,400),ECC_Visibility,Q);
    FString Under;for(const auto& H:Hits)Under+=FString::Printf(TEXT("%s/%s@%.0f; "),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),H.ImpactPoint.Z);
    TArray<FOverlapResult> Over;W->OverlapMultiByChannel(Over,Boat->GetActorLocation(),Boat->GetActorQuat(),ECC_Vehicle,FCollisionShape::MakeBox(FVector(330,85,30)),Q);
    for(const auto& O:Over)Under+=FString::Printf(TEXT("[overlap %s/%s] "),*GetNameSafe(O.GetActor()),*GetNameSafe(O.GetComponent()));
    Metrics->SetStringField(Boat==Self?TEXT("self_under"):TEXT("hired_under"),Under);
    if(!Test->TestTrue(TEXT("Rowboat moored at its berth and afloat"),Boat->GetMarineMovement()->IsMoored() && Boat->GetMarineMovement()->IsAfloat()))
     return Finish(TEXT("Rowboat not moored / afloat at the Hubin landing."));
   }
   return Board(Hired,2);
  case 2: return WaitBoarded(Hired,3,TEXT("hired"));
  case 3: // the ride
   if(E<1.5f)return false;
   if(!Captured.Contains(TEXT("ride-passenger")))
   {Capture(TEXT("ride-passenger"));SetClose(Hired,FVector(-250,-330,150),FVector(-272,0,70));}
   if(E>18 && !Captured.Contains(TEXT("ride-boatman"))){Capture(TEXT("ride-boatman"));}
   if(E>19 && E<24)CaptureStroke(Hired,TEXT("boatman"));
   RideSpeed=FMath::Max(RideSpeed,Hired->GetVelocity().Size2D()*.036f);
   if(E<70 || (E<200 && FVector::Dist2D(Hired->GetActorLocation(),HiredBerth)<3500))return false;
   ClearClose();
   Metrics->SetNumberField(TEXT("ride_max_kmh"),RideSpeed);
   Metrics->SetNumberField(TEXT("ride_distance_m"),FVector::Dist2D(Hired->GetActorLocation(),HiredBerth)/100);
   if(!Test->TestTrue(TEXT("Boatman rows the hired boat out"),RideSpeed>2.5f && FVector::Dist2D(Hired->GetActorLocation(),HiredBerth)>2500))
    return Finish(TEXT("The hired boat did not leave the pier under oars."));
   Key(PC,EKeys::F,IE_Pressed);Key(PC,EKeys::F,IE_Released);
   return Next(4);
  case 4: // back alongside
   if(!Hired->IsTourActive() && Hired->IsDocked())
   {
    Metrics->SetNumberField(TEXT("ride_return_s"),E);
    Capture(TEXT("ride-docked"));Key(PC,EKeys::F,IE_Pressed);Key(PC,EKeys::F,IE_Released);return Next(5);
   }
   if(E>360)return Finish(TEXT("The boatman did not bring the hired boat back alongside within 360 s."));
   return false;
  case 5: return WaitAshore(6,TEXT("hired"));
  case 6: return Board(Self,7);
  case 7: return WaitBoarded(Self,8,TEXT("self"));
  case 8: // step ashore again from the self-row boat at the landing, then re-board
   if(E<2)return false;
   Key(PC,EKeys::F,IE_Pressed);Key(PC,EKeys::F,IE_Released);return Next(9);
  case 9: return WaitAshore(10,TEXT("self"));
  case 10: return Board(Self,11);
  case 11: return WaitBoarded(Self,12,TEXT("self2"));
  case 12: // row out
   if(E<1)return false;
   Key(PC,EKeys::W,IE_Pressed);StartYaw=Self->GetActorRotation().Yaw;return Next(13);
  case 13:
   if(E>14 && E<24){Speeds.Add(Self->GetVelocity().Size2D()*.036f);}
   if(E>9 && E<14){if(!IsValid(Close))SetClose(Self,FVector(-180,-300,140),FVector(-262,0,75));CaptureStroke(Self,TEXT("row"));}
   if(E>14 && IsValid(Close))ClearClose();
   if(E>24 && !Captured.Contains(TEXT("row-chase")))Capture(TEXT("row-chase"));
   if(E<25)return false;
   {
    float Sum=0,Peak=0,Low=1000;for(float V:Speeds){Sum+=V;Peak=FMath::Max(Peak,V);Low=FMath::Min(Low,V);}
    const float Mean=Speeds.Num()?Sum/Speeds.Num():0;
    Metrics->SetNumberField(TEXT("row_mean_kmh"),Mean);Metrics->SetNumberField(TEXT("row_surge_kmh"),Peak-Low);
    Metrics->SetNumberField(TEXT("row_rate_spm"),Self->GetStrokeRate());
    Metrics->SetNumberField(TEXT("row_heading_drift_deg"),FMath::FindDeltaAngleDegrees(StartYaw,Self->GetActorRotation().Yaw));
    if(!Test->TestTrue(TEXT("Rowing cruise 3-8 km/h with stroke surge"),Mean>3 && Mean<8.5f && Peak-Low>.15f))
     return Finish(FString::Printf(TEXT("Rowing speed %.2f km/h (surge %.2f) outside the hand-row range."),Mean,Peak-Low));
   }
   Key(PC,EKeys::D,IE_Pressed);StartYaw=Self->GetActorRotation().Yaw;return Next(14);
  case 14: // rowing with a starboard turn
   if(E<10)return false;
   Metrics->SetNumberField(TEXT("turn_rowing_deg"),FMath::FindDeltaAngleDegrees(StartYaw,Self->GetActorRotation().Yaw));
   Key(PC,EKeys::W,IE_Released);StartYaw=Self->GetActorRotation().Yaw;return Next(15);
  case 15: // D only: pivot (port ahead, starboard backing)
   if(E<10)return false;
   Metrics->SetNumberField(TEXT("turn_pivot_deg"),FMath::FindDeltaAngleDegrees(StartYaw,Self->GetActorRotation().Yaw));
   Key(PC,EKeys::D,IE_Released);
   if(!Test->TestTrue(TEXT("Differential strokes turn the boat to starboard"),Metrics->GetNumberField(TEXT("turn_rowing_deg"))>8 && Metrics->GetNumberField(TEXT("turn_pivot_deg"))>8))
    return Finish(TEXT("Rowboat did not turn under differential strokes."));
   Key(PC,EKeys::W,IE_Pressed);return Next(16);
  case 16: // speed up again, then hold water
   if(E<12)return false;
   Key(PC,EKeys::W,IE_Released);HoldFrom=Self->GetVelocity().Size2D();Key(PC,EKeys::SpaceBar,IE_Pressed);return Next(17);
  case 17:
   if(E<1.2f)return false;
   if(!Captured.Contains(TEXT("hold-water")))Capture(TEXT("hold-water"));
   if(E<4)return false;
   Key(PC,EKeys::SpaceBar,IE_Released);
   Metrics->SetNumberField(TEXT("hold_from_kmh"),HoldFrom*.036f);Metrics->SetNumberField(TEXT("hold_after4s_kmh"),Self->GetVelocity().Size2D()*.036f);
   if(!Test->TestTrue(TEXT("Holding water takes the way off"),Self->GetVelocity().Size2D()<HoldFrom*.55f))
    return Finish(TEXT("Holding water did not slow the boat."));
   Metrics->SetNumberField(TEXT("max_roll_deg"),MaxRoll);Metrics->SetNumberField(TEXT("max_pitch_deg"),MaxPitch);
   if(!Test->TestTrue(TEXT("Hull stays upright"),MaxRoll<12 && MaxPitch<10))return Finish(TEXT("Rowboat heeled beyond 12 degrees."));
   return Finish(FString());
  }
  return Finish(TEXT("Unknown stage."));
 }
private:
 bool Initialize()
 {
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<3)return false;
  // the Hubin landing pair (the residence's own boat at its jetty is another self-row boat)
  for(TActorIterator<ANeonCityRowboat> It(W);It;++It)
   if(It->ActorHasTag(TEXT("WL.Boats")))(It->Service==ERowboatService::Hired?Hired:Self)=*It;
  if(!Self || !Hired)return Finish(TEXT("Hubin landing rowboats (self-row + hired) are missing from the map."));
  HiredBerth=Hired->GetActorLocation();Metrics=MakeShared<FJsonObject>();
  return Next(1);
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 bool Board(ANeonCityRowboat* Boat,int32 S)
 {
  // Stand on the landing ledge beside the boat (the ledge is on the boat's port or starboard side).
  const float Half=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  FString Seen;
  for(float Side:{1.f,-1.f})
  {
   const FVector Spot=Boat->GetActorTransform().TransformPosition(FVector(Boat==Self?-240:-40,Side*150,0));
   FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(RowboatTest),false,Player);Q.AddIgnoredActor(Boat);
   const bool bHit=W->LineTraceSingleByChannel(Floor,Spot+FVector(0,0,300),Spot-FVector(0,0,200),ECC_Pawn,Q);
   Seen+=FString::Printf(TEXT("[side %.0f spot %s hit %d %s/%s z %.0f] "),Side,*Spot.ToCompactString(),bHit,*GetNameSafe(Floor.GetActor()),*GetNameSafe(Floor.GetComponent()),Floor.ImpactPoint.Z);
   if(bHit && Floor.ImpactPoint.Z>Boat->GetActorLocation().Z+5)
   {
    Player->SetActorLocationAndRotation(Floor.ImpactPoint+FVector(0,0,Half+2),FRotator(0,(Boat->GetActorLocation()-Floor.ImpactPoint).Rotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    BoardStage=S;Stage=-1;At=W->GetTimeSeconds();Pending=Boat;return false;
   }
  }
  return Finish(TEXT("No landing ledge beside the rowboat: ")+Seen);
 }
 bool WaitBoarded(ANeonCityRowboat* Boat,int32 S,const TCHAR* Name)
 {
  const float E=W->GetTimeSeconds()-At;
  if(E>.3f && E<.5f && !Captured.Contains(FString(Name)+TEXT("-stepping")))Capture(FString(Name)+TEXT("-stepping"));
  if(Boat->GetPhase()==ANeonCityRowboat::EPhase::Seated && PC->GetPawn()==Boat && E>1.8f)
  {Capture(FString(Name)+TEXT("-seated"));Metrics->SetNumberField(FString(Name)+TEXT("_board_s"),E);return Next(S);}
  if(E>8)return Finish(FString::Printf(TEXT("Boarding the %s boat did not complete."),Name));
  return false;
 }
 bool WaitAshore(int32 S,const TCHAR* Name)
 {
  const float E=W->GetTimeSeconds()-At;
  if(E>.5f && E<.7f && !Captured.Contains(FString(Name)+TEXT("-standing")))Capture(FString(Name)+TEXT("-standing"));
  if(PC->GetPawn()==Player && Player->GetCharacterMovement()->IsMovingOnGround() && E>1.5f)
  {Capture(FString(Name)+TEXT("-ashore"));Metrics->SetNumberField(FString(Name)+TEXT("_ashore_s"),E);return Next(S);}
  if(E>10)return Finish(FString::Printf(TEXT("Stepping ashore from the %s boat did not complete."),Name));
  return false;
 }
 void Capture(const FString& Name){Captured.Add(Name);FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("WL-Rowboat-%s.jpg"),*Name));}
 void CaptureStroke(ANeonCityRowboat* Boat,const TCHAR* Prefix)
 {
  const int32 Slot=FMath::FloorToInt(Boat->GetStrokePhase()*8)%8;
  const FString Name=FString::Printf(TEXT("%s-phase%d"),Prefix,Slot);
  if(!Captured.Contains(Name))Capture(Name);
 }
 void SetClose(ANeonCityRowboat* Boat,FVector Eye,FVector Aim)
 {
  Close=W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FTransform::Identity);CloseBoat=Boat;CloseEye=Eye;CloseAim=Aim;
  Close->GetCameraComponent()->SetFieldOfView(50);FollowClose();PC->SetViewTarget(Close);
 }
 void FollowClose()
 {
  if(!CloseBoat.IsValid())return;
  const FTransform T=CloseBoat->GetActorTransform();const FVector Eye=T.TransformPosition(CloseEye),Aim=T.TransformPosition(CloseAim);
  Close->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
 }
 void ClearClose(){if(IsValid(Close)){PC->SetViewTarget(PC->GetPawn());Close->Destroy();}Close=nullptr;CloseBoat.Reset();}
 bool Finish(const FString& Error)
 {
  for(FKey K:{EKeys::W,EKeys::D,EKeys::SpaceBar})if(IsValid(PC))Key(PC,K,IE_Released);
  if(Metrics)
  {
   Metrics->SetStringField(TEXT("error"),Error);
   FString Json;auto Writer=TJsonWriterFactory<>::Create(&Json);FJsonSerializer::Serialize(Metrics.ToSharedRef(),Writer);
   FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("WestLake/RowboatTest.json")));
  }
  if(!Error.IsEmpty())Test->AddError(Error);
  return true;
 }
public:
 bool UpdatePending()
 {
  // Stage -1: let the player settle on the ledge, then press F (interact) on the boat.
  const float E=W->GetTimeSeconds()-At;
  if(E<1.5f || !Player->GetCharacterMovement()->IsMovingOnGround())return E>6?Finish(TEXT("Player did not settle on the landing ledge.")):false;
  if(!Pending->CanInteract_Implementation(Player))
   return Finish(FString::Printf(TEXT("Rowboat refused boarding: phase %d ground %d docked %d speed %.0f canEnter %d dist %.0f moored %d berthDist %.0f"),
    int32(Pending->GetPhase()),Player->GetCharacterMovement()->IsMovingOnGround(),Pending->IsDocked(),Pending->GetVelocity().Size2D(),
    Pending->GetMarineInteraction()->CanEnter(Player),FVector::Dist(Pending->GetActorLocation(),Player->GetActorLocation()),
    Pending->GetMarineMovement()->IsMoored(),Pending==Hired?FVector::Dist2D(Pending->GetActorLocation(),HiredBerth):-1.));
  Pending->Interact_Implementation(Player);return Next(BoardStage);
 }
private:
 FAutomationTestBase* Test;
 double Started=0;int32 Stage=0,BoardStage=0;float At=0;
 TObjectPtr<UWorld> W;TObjectPtr<APlayerController> PC;TObjectPtr<ACharacter> Player;
 TObjectPtr<ANeonCityRowboat> Self,Hired,Pending;TObjectPtr<ACameraActor> Close;TWeakObjectPtr<ANeonCityRowboat> CloseBoat;
 FVector CloseEye,CloseAim,HiredBerth;TSharedPtr<FJsonObject> Metrics;TSet<FString> Captured;TArray<float> Speeds;
 float MaxRoll=0,MaxPitch=0,RideSpeed=0,StartYaw=0,HoldFrom=0;double NextTrace=0;FString Trace;
};

/** Wraps the scenario so stage -1 (settling before interaction) is dispatched. */
class FScenarioDriver : public IAutomationLatentCommand
{
public:
 explicit FScenarioDriver(FAutomationTestBase* In):Scenario(MakeShared<FScenario>(In)){}
 bool Update() override { return Scenario->Update(); }
 TSharedRef<FScenario> Scenario;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWestLakeRowboatPIE,"NeonCity.WestLake.RowboatPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWestLakeRowboatPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityRowboatTest::FScenarioDriver>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
namespace NeonCityWestLakeHomeBoat
{
void Key(APlayerController* PC,FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
/** The residence's own rowboat: it is moored at the home jetty, the player stands on the jetty deck beside it, boards
 * it the normal way (interaction), is seated, and rowing (W held) takes it out onto the lake. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(FString::Printf(TEXT("Home boat check timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(W && !Boat.IsValid())for(TActorIterator<ANeonCityRowboat> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.HomeBoat")))Boat=*It;
  if(Boat.IsValid() && W->GetTimeSeconds()-LastLog>=.5)
  {
   LastLog=W->GetTimeSeconds();
   Test->AddInfo(FString::Printf(TEXT("side t %.1f: boat at %s v %s moored %d docked %d"),LastLog,*Boat->GetActorLocation().ToCompactString(),
    *Boat->GetVelocity().ToCompactString(),Boat->GetMarineMovement()->IsMoored(),Boat->IsDocked()));
  }
  if(!PC || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   Player=Cast<ACharacter>(PC->GetPawn());if(!Player.IsValid())return false;
   for(TActorIterator<ANeonCityRowboat> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.HomeBoat")))Boat=*It;
   if(!Test->TestNotNull(TEXT("home rowboat at the residence jetty"),Boat.Get()))return true;
   // stand on the jetty deck beside the boat
   const float Half=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();bool bPlaced=false;
   for(float Side:{1.f,-1.f})
   {
    const FVector Spot=Boat->GetActorTransform().TransformPosition(FVector(-60,Side*150,0));FHitResult Floor;
    FCollisionQueryParams Q(TEXT("HomeBoatJetty"),false,Player.Get());Q.AddIgnoredActor(Boat.Get());
    const bool bHit=W->LineTraceSingleByChannel(Floor,Spot+FVector(0,0,300),Spot-FVector(0,0,200),ECC_Pawn,Q);
    Test->AddInfo(FString::Printf(TEXT("side %.0f spot %s: hit %d %s z %.0f (boat z %.0f)"),Side,*Spot.ToCompactString(),bHit,*GetNameSafe(Floor.GetActor()),Floor.ImpactPoint.Z,Boat->GetActorLocation().Z));
    if(bHit && Floor.ImpactPoint.Z>Boat->GetActorLocation().Z+20 && Floor.GetActor() && Floor.GetActor()->ActorHasTag(TEXT("WL.HomeBoat")))
    {
     Player->SetActorLocationAndRotation(Floor.ImpactPoint+FVector(0,0,Half+2),FRotator(0,(Boat->GetActorLocation()-Floor.ImpactPoint).Rotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
     bPlaced=true;break;
    }
   }
   if(!Test->TestTrue(TEXT("the jetty deck runs alongside the boat"),bPlaced))return true;
   return Next(1);
  }
  const double E=FPlatformTime::Seconds()-At;
  if(Stage==1)
  {
   if(E<1.5)return false;
   if(!Moored.IsSet())Moored=Boat->GetActorLocation();
   // a moored boat settles on the water; it must not drift away from its jetty
   if(Boat->GetVelocity().Size2D()>=40 && E<20)return false;
   Test->AddInfo(FString::Printf(TEXT("side drift: %.0f cm over %.1f s"),FVector::Dist2D(Boat->GetActorLocation(),Moored.GetValue()),E));
   Test->TestTrue(TEXT("the moored boat stays at its jetty"),FVector::Dist2D(Boat->GetActorLocation(),Moored.GetValue())<150);
   Test->AddInfo(FString::Printf(TEXT("side check: phase %d, boat speed %.0f, controller %s, on ground %d, base %s, %.0f cm from the boat"),int32(Boat->GetPhase()),
    Boat->GetVelocity().Size(),*GetNameSafe(Boat->GetController()),Player->GetCharacterMovement()->IsMovingOnGround(),*GetNameSafe(Player->GetMovementBaseObject()),
    FVector::Dist(Boat->GetActorLocation(),Player->GetActorLocation())));
   if(!Test->TestTrue(TEXT("the boat can be boarded from the jetty"),Boat->CanInteract_Implementation(Player.Get())))return true;
   Boat->Interact_Implementation(Player.Get());return Next(2);
  }
  if(Stage==2)
  {
   if(!(Boat->GetPhase()==ANeonCityRowboat::EPhase::Seated && PC->GetPawn()==Boat.Get())){if(E>8){Test->AddError(TEXT("boarding the home boat did not complete"));return true;}return false;}
   Start=Boat->GetActorLocation();Key(PC,EKeys::W,IE_Pressed);return Next(3);
  }
  if(Stage==3)
  {
   Key(PC,EKeys::W,IE_Repeat);
   if(E<12)return false;                                  // hand rowing builds up over several strokes
   Key(PC,EKeys::W,IE_Released);
   const float Moved=FVector::Dist2D(Boat->GetActorLocation(),Start);
   Test->TestTrue(FString::Printf(TEXT("rowing takes the home boat out (%.0f cm in 12 s)"),Moved),Moved>600);
   return true;
  }
  return true;
 }
private:
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0;FVector Start=FVector::ZeroVector;TOptional<FVector> Moored;double LastLog=-1;
 TWeakObjectPtr<ACharacter> Player;TWeakObjectPtr<ANeonCityRowboat> Boat;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeHomeBoatPIE,"NeonCity.WestLake.HomeBoatPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeHomeBoatPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeHomeBoat::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
