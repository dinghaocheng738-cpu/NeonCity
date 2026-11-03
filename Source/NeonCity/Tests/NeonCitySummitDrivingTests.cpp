#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"

namespace NeonCitySummitTests
{
void Input(APlayerController* PC,FKey K,bool Down)
{PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}

/** The automatic keyboard driver uses actual W/Space/A/D mappings. It never
 * sets a vehicle transform or directly writes driving-component commands after T0. */
class FTripleSummit : public IAutomationLatentCommand
{
public:
 explicit FTripleSummit(FAutomationTestBase* T,bool Single=false,bool ExitOnly=false,bool DownOnly=false,bool Remaining=false,int32 Focus=0,bool BlockedOnly=false):Test(T),Evidence(TEXT("TripleSummitForecourt-")+T->GetTestName()),bSingleKestrel(Single || ExitOnly || DownOnly || BlockedOnly),bExitOnly(ExitOnly),bDownOnly(DownOnly || BlockedOnly),bRemainingCars(Remaining),FocusCar(Focus),bBlockedOnly(BlockedOnly){}
 virtual ~FTripleSummit(){if(FrameDriver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(FrameDriver);}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down)
 {bool& Was=Keys.FindOrAdd(K);if(Was!=Down){Input(PC,K,Down);Was=Down;}}
 void Release(){for(auto& K:Keys)if(K.Value){Input(PC,K.Key,false);K.Value=false;}}
 bool Fail(const FString& Why){Test->AddError(Why);return Finish(false,Why);}
 bool Finish(bool Passed,const FString& Why)
 {
  if(FrameDriver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(FrameDriver);FrameDriver.Reset();}
  Release();if(Car && Car->GetRoadCruise()->IsActive())Car->GetRoadCruise()->Cancel(TEXT("Summit fixture ended"));
  if(!Directory.IsEmpty())
  {
   FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Why);
   J->SetStringField(TEXT("scope"),bSingleKestrel?TEXT("Fix-specific Kestrel summit/garage fixture; actual F/W/Space/A/D/C mappings. One setup pose, zero vehicle pose writes after T0. Not full Dock continuity."):TEXT("All-three focused summit/garage fixture; actual mapped keyboard and C controller. One setup pose per car, zero pose writes after T0. Not full Dock continuity."));
   if(bExitOnly)J->SetStringField(TEXT("scope"),TEXT("Smallest new forecourt exit blocker: one initial Kestrel/player fixture in genuine parking aisle. Actual F and W/Space/A/D manual controls physically traverse turning court and merge with real descending road. Zero pose writes after T0. Previously completed ascent/parking are cached; this does not repeat or certify full descent."));
   if(bDownOnly)J->SetStringField(TEXT("scope"),TEXT("Only Kestrel's previously uncompleted descent: one initial road-supported car/player fixture after the cached genuine court merge. Actual F/C cruise and W/Space/A/D manual descending controls, native stop and on-foot exit. Zero vehicle pose writes after T0. Cached ascent/parking/turning facets are not rerun; this is not a new Dock continuity result."));
   if(bRemainingCars)J->SetStringField(TEXT("scope"),TEXT("Previously uncompleted Aster and Nacre summit/garage fixtures; native F/W/Space/A/D/C drive actual Chaos ascent, forecourt park/Uturn and descent. One initial fixture per car, zero route pose corrections. Kestrel completed facets are cached and skipped. Not whole Dock continuity."));
   if(bSingleKestrel && FocusCar==2 && !bDownOnly)J->SetStringField(TEXT("scope"),TEXT("Only previously untested Nacre: one setup pose followed by actual F/W/Space/A/D/C manual+cruise ascent, garage park, physical Uturn and descent. Kestrel/Aster ascent/parking evidence is cached. No post-T0 pose writes; not whole Dock continuity."));
   if(bBlockedOnly)J->SetStringField(TEXT("scope"),TEXT("Smallest proved old-bollard obstruction reproduction for the selected genuine car. One initial supported fixture eight source nodes before the real retained failure pose; actual F/W/Space/A/D physically pass the repaired light, complete remaining manual descent and stop/exit. Previously completed ascend/park/turn/cruise-down are cached. No post-T0 vehicle pose corrections."));
   J->SetNumberField(TEXT("expected_cars"),bSingleKestrel?1:bRemainingCars?2:3);
   J->SetNumberField(TEXT("initial_car_placements"),Placements);J->SetNumberField(TEXT("post_t0_vehicle_pose_writes"),0);
   J->SetNumberField(TEXT("cars_completed"),Index);J->SetStringField(TEXT("source_survey"),TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"));
   J->SetNumberField(TEXT("source_points"),Source.Num());J->SetArrayField(TEXT("completed_vehicle_guids"),Completed);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json")));
  }
  return true;
 }
 bool BuildPath()
 {
  FString Text;TSharedPtr<FJsonObject> Data;
  if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json")))
   || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || Data->GetIntegerField(TEXT("retained_stations"))!=735)return false;
  for(const auto& R:Data->GetArrayField(TEXT("points")))
  {const auto V=R->AsArray();if(V.Num()!=3)return false;Source.Add(FVector(NeonCityCoastalLayout::IslandOrigin.X+100*V[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*V[1]->AsNumber(),-130+100*V[2]->AsNumber()+6));}
  USplineComponent* Court=nullptr;
  for(TActorIterator<ANeonCityTrafficRoute> It(W);It;++It)if(It->RoadID==TEXT("Island.Mansion.Forecourt"))Court=It->GetRoadSpline();
  if(!Court || Court->GetNumberOfSplinePoints()<8)return false;
  const FVector Exit=Court->GetLocationAtSplinePoint(Court->GetNumberOfSplinePoints()-1,ESplineCoordinateSpace::World);
  int32 Rejoin=Source.Num()-1;double Best=TNumericLimits<double>::Max();
  for(int32 I=FMath::Max(735,Source.Num()-15);I<Source.Num();++I){const double D=FVector::DistSquared(Source[I],Exit);if(D<Best){Best=D;Rejoin=I;}}
  if(Best>FMath::Square(250.f))return false;
  auto Lane=[&](int32 I,float Sign,float Offset){const FVector T=(Source[FMath::Min(I+1,Source.Num()-1)]-Source[FMath::Max(I-1,0)]).GetSafeNormal2D();return Source[I]+FVector(-T.Y,T.X,0)*Sign*Offset;};
  for(int32 I=725;I<Source.Num();++I)
  {
   // The final driveway is a centred single entry. Taper while still on the actual road.
   const float ArrivalDistance=FVector::Dist2D(Source[I],Source.Last());
   Path.Add(Lane(I,1,150*FMath::Clamp((ArrivalDistance-300)/1500.f,0.f,1.f)));
  }
  SummitEnd=Path.Num()-1;UpCruiseAt=10+(Source.Num()-735)/2;
  const FVector Parking=NeonCityCoastalLayout::FromLegacyIsland(FVector(47600,-119450,30486));double ParkingBest=TNumericLimits<double>::Max();
  for(int32 I=0;I<Court->GetNumberOfSplinePoints();++I)
  {
   const FVector P=Court->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World);if(!Path.Last().Equals(P,10))Path.Add(P);
   const double D=FVector::DistSquared(P,Parking);if(D<ParkingBest){ParkingBest=D;Park=Path.Num()-1;}
  }
  if(ParkingBest>FMath::Square(300.f))return false;
  CourtExit=Path.Num()-1;
  for(int32 I=Rejoin;I>=725;--I)Path.Add(Lane(I,-1,150));
  DownManualAt=CourtExit+(Path.Num()-1-CourtExit)/2;
  Distance.Add(0);for(int32 I=1;I<Path.Num();++I)Distance.Add(Distance.Last()+FVector::Distance(Path[I-1],Path[I]));
  return true;
 }
 FVector Point(float D) const
 {
  int32 I=FMath::Clamp(Node,0,Path.Num()-2);while(I>0 && Distance[I]>D)--I;while(I<Path.Num()-2 && Distance[I+1]<D)++I;
  return FMath::Lerp(Path[I],Path[I+1],FMath::Clamp((D-Distance[I])/FMath::Max(Distance[I+1]-Distance[I],1.f),0.f,1.f));
 }
 bool Progress()
 {
  const FVector Here=Car->GetActorLocation();float Best=BIG_NUMBER;int32 Chosen=Node;float D=Along;FVector P=Point(Along);
  for(int32 I=FMath::Max(0,Node-3);I<Path.Num()-1 && Distance[I]<=Along+1200;++I)
  {
   const FVector AB=Path[I+1]-Path[I];const float A=FMath::Clamp(float(FVector::DotProduct(Here-Path[I],AB)/FMath::Max(AB.SizeSquared(),1.)),0.f,1.f);
   const FVector Q=Path[I]+AB*A;const float E=FVector::DistSquared(Here,Q);
   if(E<Best){Best=E;Chosen=I;D=Distance[I]+AB.Size()*A;P=Q;}
  }
  LaneError=FVector::Dist2D(Here,P);const float Z=Here.Z-P.Z;
  // Accumulate real progress across native frame ticks. Comparing each
  // 60 Hz step with a 15 cm threshold falsely times out a moving car.
  if(D>ProgressMarker+15){LastProgress=W->GetTimeSeconds();ProgressMarker=D;}
  Node=Chosen;Along=FMath::Max(Along-30,D);
  return LaneError<310 && Z>-70 && Z<180 && FMath::Abs(Car->GetActorRotation().Roll)<28;
 }
 void Manual(float DesiredKmh)
 {
  auto* M=Car->GetChaosMovement();const float Speed=FMath::Max(0.f,M->GetForwardSpeed());
  const FVector Rear=Car->GetMesh()->GetComponentTransform().TransformPosition(RearAxleLocal);
  const FVector V=Car->GetActorTransform().InverseTransformVectorNoScale(Point(Along+FMath::Clamp(420.f+Speed*.4f,450.f,850.f))-Rear);
  const float Scale=FMath::Max(.1f,M->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(Speed*.0223694f));
  const float AxleRatio=M->SteeringSetup.SteeringType==ESteeringType::AngleRatio?.5f*(1.f+M->SteeringSetup.AngleRatio):1.f;
  const float Lock=M->Wheels[0]->MaxSteerAngle*Scale*AxleRatio;
  const float CrossTrack=Car->GetActorTransform().InverseTransformVectorNoScale(Point(Along)-Car->GetActorLocation()).Y;
  const float Correction=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(CrossTrack*.75f,Speed+300.f)),-8.f,8.f);
  const float Needed=FMath::Clamp((FMath::RadiansToDegrees(FMath::Atan2(2*Wheelbase*V.Y,FMath::Max(V.X*V.X+V.Y*V.Y,1.)))+Correction)/Lock,-1.f,1.f);
  const float Actual=M->GetSteeringInput();
  const float DT=FMath::Clamp(W->GetTimeSeconds()-LastManualTime,0.f,.1f);LastManualTime=W->GetTimeSeconds();
  // Close the keyboard pulse duty around actual steering, including the
  // normal return-rate asymmetry. This only selects mapped A/D key events.
  // The previous instantaneous comparator biased short key pulses toward
  // centre when frame readback made the native input interval coarse.
  SteeringIntegral=FMath::Clamp(SteeringIntegral+(Needed-Actual)*DT,-.35f,.35f);
  const float Command=FMath::Clamp(Needed+2.f*SteeringIntegral,-1.f,1.f);
  RequestedSteering=Needed;
  // Feedback pulses are genuine mapped keyboard commands, not direct analog component writes.
  Key(EKeys::D,Command>Actual+.02f);Key(EKeys::A,Command<Actual-.02f);
  Key(EKeys::W,Speed*.036f<DesiredKmh-.6f);Key(EKeys::S,false);Key(EKeys::SpaceBar,Speed*.036f>DesiredKmh+1.0f || DesiredKmh<.2f);
 }
 void DriveFrame(UWorld* World,ELevelTick Tick,float Delta)
 {
  // Latent automation updates at about 10 Hz. Keyboard pulses issued there
  // become 100 ms full-lock commands, unlike normal player input. Feed the
  // existing mapping at the actual game-frame boundary; no gameplay control
  // component, actor transform or tire force is changed by this harness.
  if(World!=W || !Car || !PC || PC->GetPawn()!=Car || (Stage!=3 && Stage!=8 && Stage!=13))return;
  Progress();
  if(Stage==3)Manual(18);
  else if(Stage==8)Manual(10);
  else Manual(FMath::Min(18.f,FMath::Sqrt(500.f*FMath::Max(0.f,Distance.Last()-Along-200.f))*.036f));
 }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>900)return Fail(FString::Printf(TEXT("Triple summit timed out car%d stage%d node%d"),Index,Stage,Node));
  if(!W || !PC || W->GetTimeSeconds()<3)return false;const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30580))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   if(!BuildPath())return Fail(TEXT("Final summit survey or physically connected forecourt/parking route is missing"));
   for(TActorIterator<AIslandSupercar> It(W);It;++It)Cars.Add(*It);
   if(Cars.Num()!=3)return Fail(TEXT("Expected exact three default island cars for the focused fixture"));
   Cars.Sort([](const AIslandSupercar& A,const AIslandSupercar& B){return A.VehicleID.ToString()<B.VehicleID.ToString();});
   if(bSingleKestrel && FocusCar>0){AIslandSupercar* Selected=Cars[FocusCar];Cars.Empty();Cars.Add(Selected);}
   if(bRemainingCars)Cars.RemoveAt(0); // Kestrel's distinct cached GUID sorts first.
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   FString ReceiptName=Test->GetTestName();int32 Dot=0;if(ReceiptName.FindLastChar('.',Dot))ReceiptName.RightChopInline(Dot+1);
   Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/FPaths::MakeValidFileName(Label)/TEXT("TripleSummit")/FPaths::MakeValidFileName(ReceiptName);IFileManager::Get().MakeDirectory(*Directory,true);
   CSV=TEXT("game_seconds,car,mode,node,x,y,z,yaw,pitch,roll,speed_kmh,lane_error_cm,wheel_contacts,condition,cruise,cruise_reason,steering,requested_steering,throttle,brake,key_A,key_D\n");
   FrameDriver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FTripleSummit::DriveFrame);return Next(1);
  }
  if(Stage==1)
  {
   Release();Car=Cars[Index];Node=bDownOnly?CourtExit+3:bExitOnly?Park:0;Along=ProgressMarker=Distance[Node];SteeringIntegral=0;LastManualTime=W->GetTimeSeconds();LastProgress=W->GetTimeSeconds();
   if(bBlockedOnly)
   {
    const FVector ProvedStop=NeonCityCoastalLayout::FromLegacyIsland(FVector(43505.4,-124233.2,26095.5));double Best=BIG_NUMBER;int32 BlockedNode=CourtExit+3;
    for(int32 I=CourtExit+3;I<Path.Num()-1;++I){const double D=FVector::DistSquared(Path[I],ProvedStop);if(D<Best){Best=D;BlockedNode=I;}}
    if(Best>FMath::Square(350.f))return Fail(TEXT("Retained real failure pose does not belong to this authored descending path"));
    Node=FMath::Max(CourtExit+3,BlockedNode-8);Along=ProgressMarker=Distance[Node];
   }
   const FRotator R=(Path[Node+1]-Path[Node]).Rotation();Car->SetActorTransform(FTransform(R,Path[Node]+FVector(0,0,55)),false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->GetMesh()->WakeAllRigidBodies();
   Player->SetActorLocation(Path[Node]-R.RotateVector(FVector(0,bDownOnly?210:260,0))+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();++Placements;return Next(2);
  }
  if(Stage==2 && Age>1.2f)
  {
   auto* M=Car->GetChaosMovement();if(M->Wheels.Num()!=4 || !M->Wheels[0] || !M->Wheels[1] || !M->Wheels[2] || !M->Wheels[3])return Fail(TEXT("Imported car must expose four actual Chaos wheels"));
   const auto* Rig=Car->GetMesh()->GetSkeletalMeshAsset();if(!Rig)return Fail(TEXT("Actual physical wheel skeleton missing"));
   const auto& Skeleton=Rig->GetRefSkeleton();FVector Rest[4];
   for(int32 I=0;I<4;++I)
   {
    const auto& Setup=M->WheelSetups[I];int32 Bone=Skeleton.FindBoneIndex(Setup.BoneName);
    if(Bone==INDEX_NONE || !Setup.WheelClass)return Fail(TEXT("Authored wheel setup missing"));
    FTransform Pose=Skeleton.GetRefBonePose()[Bone];while((Bone=Skeleton.GetParentIndex(Bone))!=INDEX_NONE)Pose*=Skeleton.GetRefBonePose()[Bone];
    Rest[I]=Pose.GetLocation()+Setup.AdditionalOffset+Setup.WheelClass.GetDefaultObject()->Offset;
   }
   RearAxleLocal=(Rest[2]+Rest[3])*.5;Wheelbase=FVector::Dist2D((Rest[0]+Rest[1])*.5,RearAxleLocal);
   if(Wheelbase<180 || Wheelbase>400)return Fail(TEXT("Physical axle measurement invalid"));
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,25)-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return Fail(TEXT("Normal summit car entry trace failed"));
   Key(EKeys::F,true);return Next(20);
  }
  if(Stage==20 && Age>.2f)
  {Key(EKeys::F,false);if(PC->GetPawn()!=Car)return Fail(TEXT("Actual F did not board the summit car"));Condition=Car->GetDamageState()->GetConditionFraction();LastProgress=W->GetTimeSeconds();if(bBlockedOnly)return Next(13);if(bDownOnly){SetNavigationGoal(Path.Last(),TEXT("Island.Mountain.Return"));Key(EKeys::C,true);return Next(9);}return Next(bExitOnly?8:3);}
  if(Stage==20)return false;
  if(!Car || Stage<3)return false;Evidence.Record(W,PC,100*Index+Stage);
  if(!Progress())return Fail(FString::Printf(TEXT("Physical lane/support failure car%d stage%d node%d error%.1f pose%s"),Index,Stage,Node,LaneError,*Car->GetActorTransform().ToHumanReadableString()));
  if(W->GetTimeSeconds()-LastProgress>12 && Stage!=6 && Stage!=11 && Stage!=31)
  {
   FCollisionQueryParams Q(SCENE_QUERY_STAT(SummitStuckContact),false,Car);Q.AddIgnoredActor(Player);
   for(float Side:{-82.f,0.f,82.f})for(float Height:{-25.f,0.f,25.f})
   {
    const FVector From=Car->GetActorTransform().TransformPosition(FVector(150,Side,Height));FHitResult H;
    if(W->LineTraceSingleByChannel(H,From,From+Car->GetActorForwardVector()*180,ECC_Pawn,Q))
     UE_LOG(LogTemp,Warning,TEXT("Summit stuck contact: side=%.0f height=%.0f actor=%s component=%s mesh=%s point=%s normal=%s"),Side,Height,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*GetNameSafe(Cast<UStaticMeshComponent>(H.GetComponent())?Cast<UStaticMeshComponent>(H.GetComponent())->GetStaticMesh():nullptr),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString());
   }
   return Fail(FString::Printf(TEXT("No physical progress car%d stage%d node%d reason%s"),Index,Stage,Node,*Car->GetRoadCruise()->GetReason()));
  }
  if(W->GetTimeSeconds()-LastSample>.1f)
  {
   LastSample=W->GetTimeSeconds();const FVector P=Car->GetActorLocation();const FRotator R=Car->GetActorRotation();int32 Contacts=0;
   auto* M=Car->GetChaosMovement();for(int32 I=0;I<M->Wheels.Num();++I)Contacts+=M->GetWheelState(I).bInContact?1:0;
   CSV+=FString::Printf(TEXT("%.4f,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.4f,%d,%s,%.4f,%.4f,%.4f,%.4f,%d,%d\n"),W->GetTimeSeconds(),*Car->VehicleID.ToString(),Stage,Node,P.X,P.Y,P.Z,R.Yaw,R.Pitch,R.Roll,Car->GetDrivingComponent()->GetSpeedKmh(),LaneError,Contacts,Car->GetDamageState()->GetConditionFraction(),Car->GetRoadCruise()->IsActive()?1:0,*Car->GetRoadCruise()->GetReason(),M->GetSteeringInput(),RequestedSteering,M->GetThrottleInput(),M->GetBrakeInput(),Keys.FindRef(EKeys::A),Keys.FindRef(EKeys::D));
  }
  if(Stage==3){if(Node>=UpCruiseAt){Release();SetNavigationGoal(Path[Park],TEXT("Island.Mansion.Garage"));Key(EKeys::C,true);return Next(4);}return false;}
  if(Stage==4 && Age>.15f){Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return Fail(TEXT("Real C did not engage cruise on final ascending road"));return Next(5);}
  if(Stage==5)
  {
   if(!Car->GetRoadCruise()->IsActive())return Fail(TEXT("Ascending cruise exited without a manual request"));
   if(Along>Distance[Park]-350){Key(EKeys::S,true);Key(EKeys::SpaceBar,true);return Next(6);}return false;
  }
  if(Stage==6 && Age>2 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1)
  {
   if(FVector::Dist2D(Car->GetActorLocation(),Path[Park])>450)return Fail(TEXT("Garage parking did not stop inside the authored bay"));
   Test->TestTrue(TEXT("S parks and retains cruise; real garage bay reached"),Car->GetRoadCruise()->IsActive());
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Summit-%s-GaragePark.png"),*Car->VehicleID.ToString()));Release();Key(EKeys::C,true);return Next(7);
  }
  if(Stage==7 && Age>.15f){Key(EKeys::C,false);LastProgress=W->GetTimeSeconds();return Next(8);}
  if(Stage==8)
  {if(Node>=CourtExit+3){Release();if(bExitOnly){Key(EKeys::SpaceBar,true);return Next(31);}SetNavigationGoal(Path.Last(),TEXT("Island.Mountain.Return"));Key(EKeys::C,true);return Next(9);}return false;}
  if(Stage==31 && Age>1.5 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1)
  {if(Car->GetDamageState()->GetConditionFraction()<Condition-.03f)return Fail(TEXT("Local physical court exit materially damaged the chassis"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Garage-Exit-Native-Merge.png"));Completed.Add(MakeShared<FJsonValueString>(Car->VehicleID.ToString()));Index=1;return Finish(true,TEXT(""));}
  if(Stage==9 && Age>.15f){Key(EKeys::C,false);if(!Car->GetRoadCruise()->IsActive())return Fail(TEXT("Real C did not engage on the descending lane after garage Uturn"));return Next(10);}
  if(Stage==10)
  {
   if(!Car->GetRoadCruise()->IsActive())return Fail(TEXT("Descending cruise exited without a manual request"));
   if(Node>=DownManualAt){Key(EKeys::C,true);return Next(12);}return false;
  }
  if(Stage==12 && Age>.15f){Key(EKeys::C,false);LastProgress=W->GetTimeSeconds();return Next(13);}
  if(Stage==13)
  {
   const float Remaining=Distance.Last()-Along;
   if(Remaining<300){Release();Key(EKeys::SpaceBar,true);return Next(11);}return false;
  }
  if(Stage==11 && Age>1.5f && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1)
  {
   if(Car->GetDamageState()->GetConditionFraction()<Condition-.03f)return Fail(TEXT("Summit/garage fixture produced material vehicle damage"));
   Test->AddInfo(FString::Printf(TEXT("Car%s completed the declared %s scope, zero route pose corrections"),*Car->VehicleID.ToString(),bBlockedOnly?TEXT("lower obstruction/manual-descent"):bDownOnly?TEXT("cruise/manual descent"):TEXT("manual+cruise ascent/descent, garage park and physical turning aisle")));
   if(bBlockedOnly)Test->AddInfo(TEXT("The completed scope is only the declared lower obstruction/manual-descent reproduction; other successful facets are cached, not performed in this test."));
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Summit-%s-DescentFinish.png"),*Car->VehicleID.ToString()));
   Completed.Add(MakeShared<FJsonValueString>(Car->VehicleID.ToString()));Release();
   if(!Car->GetVehicleInteraction()->Exit())return Fail(TEXT("Native player exit failed after descending parking"));
   if(++Index<Cars.Num() && !bSingleKestrel)return Next(1);return Finish(true,TEXT(""));
  }
  return false;
 }
private:
 void SetNavigationGoal(FVector Location,FName Destination)
 {
  if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())
  {FNeonCityNavigationDestination Goal;Goal.OwnerID=TEXT("Test.SummitNavigation");Goal.DestinationID=Destination;Goal.Label=FText::FromString(TEXT("Summit route destination"));Goal.Location=Location;Goal.UseRoadRoute=true;Nav->SetDestination(Goal);}
 }
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;
 ANeonCityPlayerCharacter* Player=nullptr;AIslandSupercar* Car=nullptr;TArray<AIslandSupercar*> Cars;
 TArray<FVector> Source,Path;TArray<float> Distance;TArray<TSharedPtr<FJsonValue>> Completed;TMap<FKey,bool> Keys;
 int32 Stage=0,Index=0,Placements=0,Node=0,SummitEnd=0,UpCruiseAt=0,Park=0,CourtExit=0,DownManualAt=0;
 float At=0,Along=0,ProgressMarker=0,LaneError=0,LastProgress=0,LastSample=0,Condition=0,Wheelbase=280,SteeringIntegral=0,LastManualTime=0,RequestedSteering=0;FVector RearAxleLocal=FVector::ZeroVector;double Started=0;FString Directory,CSV;bool bSingleKestrel=false,bExitOnly=false,bDownOnly=false,bRemainingCars=false;int32 FocusCar=0;bool bBlockedOnly=false;FDelegateHandle FrameDriver;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityTripleSummitTest,"NeonCity.Phase205c.Cruise.TripleSummitForecourtMapped",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityTripleSummitTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityKestrelSummitTest,"NeonCity.Phase205c.Cruise.KestrelSummitMapped",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityKestrelSummitTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityGarageExitTest,"NeonCity.Phase205c.Cruise.GarageExitPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityGarageExitTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityKestrelDescentTest,"NeonCity.Phase205c.Cruise.KestrelDescentOnlyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityKestrelDescentTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,true,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityRemainingSummitTest,"NeonCity.Phase205c.Cruise.RemainingSummitForecourtPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityRemainingSummitTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,false,false,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityKestrelLowerBlockTest,"NeonCity.Phase205c.Cruise.KestrelLowerObstaclePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityKestrelLowerBlockTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,true,false,false,false,0,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityAsterLowerBlockTest,"NeonCity.Phase205c.Cruise.AsterLowerObstaclePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityAsterLowerBlockTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,true,false,false,false,1,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityNacreSummitTest,"NeonCity.Phase205c.Cruise.NacreSummitForecourtPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityNacreSummitTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCitySummitTests::FTripleSummit(this,true,false,false,false,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
