#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandGarageControl.h"
#include "Adventure/IslandGarageDoor.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Algo/Reverse.h"

namespace GarageOccupied242
{
FString Cell(FString Value){Value.ReplaceInline(TEXT("\""),TEXT("\"\""));return TEXT("\"")+Value+TEXT("\"");}
FString XYZ(FVector Point){return FString::Printf(TEXT("%.3f,%.3f,%.3f"),Point.X,Point.Y,Point.Z);}
class FFlow:public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* InTest):Test(InTest),Evidence(TEXT("GarageOccupied242")){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>65)return End(TEXT("Focused occupied-door native test timeout"));if(!P || W->GetTimeSeconds()<4)return false;
  Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31830))))return false;
   for(TActorIterator<AIslandGarageControl> I(W);I;++I)if(I->Action==EIslandGarageAction::Door && I->PropertyID==TEXT("Property.Island.Aurelia"))Control=*I;
   if(!Control || !Control->Door)return End(TEXT("Existing normal owned garage-door F control absent"));Door=Control->Door.Get();DoorPose=Door->GetActorTransform();ControlPose=Control->GetActorTransform();
   if(!Door->IsTargetOpen() || Door->GetOpenFraction()<.999 || Door->IsObstructed())return End(TEXT("Test requires unchanged native default fully-open, unobstructed garage; no state fixture used"));
   for(const auto& Leaf:Door->Panels){if(!Leaf || !Leaf->GetStaticMesh() || !Leaf->IsVisible() || Leaf->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || Leaf->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || Leaf->GetCollisionResponseToChannel(ECC_Vehicle)!=ECR_Block)return End(TEXT("Existing physical garage leaves are missing or collision changed"));LeafMeshes.Add(Leaf->GetStaticMesh());}
   const FVector ControlLocal=DoorPose.InverseTransformPosition(Control->GetActorLocation());const float Margin=P->GetSimpleCollisionRadius()+30;
   TrackGoal=DoorPose.TransformPosition(FVector(FMath::Clamp(float(ControlLocal.X),-Door->Width*.5f+Margin,Door->Width*.5f-Margin),0,P->GetSimpleCollisionHalfHeight()+3));
   // One player-only fixture within the unchanged normal F radius, towards the
   // nearest safe part of the actual opening. Floor and full capsule are native.
   // 253 showed that the single radial guess falls beyond the real floor.
   // Survey supported points inside the unchanged F radius and retain the
   // closest actual standing point to the track. This moves no floor/control.
   FVector Stand;FString Why;double BestDistance=TNumericLimits<double>::Max();bool Found=false;
   for(const float Radius:{50.f,100.f,150.f,180.f,200.f,215.f})for(int Angle=0;Angle<360;Angle+=15)
   {
    const float Radians=FMath::DegreesToRadians(float(Angle));FVector Candidate;
    const FVector Around=Control->GetActorLocation()+FVector(FMath::Cos(Radians),FMath::Sin(Radians),0)*Radius;
    if(!Supported(Around,Candidate,Why) || FVector::Dist(Candidate,Control->GetActorLocation())>235)continue;
    // The current real jamb control is near the door. Begin outside the leaf
    // plane so the fixture represents approach, then actual W enters and
    // retreats. Starting already under the track would make withdrawal zero.
    const FVector CandidateLocal=DoorPose.InverseTransformPosition(Candidate);
    // The final safe-retreat gate is radius+100. Allow the same measured
    // walking stop tolerance when selecting its initial destination.
    if(FMath::Abs(CandidateLocal.Y)<P->GetSimpleCollisionRadius()+140)continue;
    FHitResult FocusHit;FCollisionQueryParams FocusQuery(SCENE_QUERY_STAT(OccupiedGarageSupportedStartFocus),false,P);
    if(!W->LineTraceSingleByChannel(FocusHit,Candidate+FVector(0,0,P->BaseEyeHeight),Control->Interaction->GetComponentLocation(),ECC_Visibility,FocusQuery)
     || FocusHit.GetActor()!=Control)continue;
    const double Distance=FVector::Dist2D(Candidate,TrackGoal);if(Distance>=BestDistance)continue;
    Stand=Candidate;BestDistance=Distance;Found=true;
   }
   if(!Found)return End(TEXT("Native floor/full-capsule/F-eye survey found no normal garage-control start: ")+Why);
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();StartPosition=Stand;Health=P->GetHealthComponent()->GetCurrentHealth();return Next(1);
  }
  Record();if(!Door->GetActorTransform().Equals(DoorPose,.001) || !Control->GetActorTransform().Equals(ControlPose,.001))return End(TEXT("Door/control original actor pose changed"));
  if(!P->GetActorEnableCollision() || P->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || P->GetHealthComponent()->GetCurrentHealth()!=Health)return End(TEXT("Player collision or health changed during occupied-door test"));
  if(Stage==1 && Age>.25)
  {
   StartPosition=P->GetActorLocation();if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Declared normal control start did not settle on real floor"));
   if(!Plan(StartPosition,TrackGoal,WalkPath,PathFailure))return End(TEXT("No actual supported full-capsule walking route from unchanged F control to track: ")+PathFailure);
   ApproachLength=0;FVector Previous=StartPosition;for(const auto& Point:WalkPath){ApproachLength+=FVector::Dist2D(Previous,Point);Previous=Point;}
   if(!Focus())return End(TEXT("Unchanged normal garage-door F eye trace/radius unavailable"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("GarageOccupied242-Normal-Close-F.png"));Key(EKeys::F,true);++ClosePresses;return Next(2);
  }
  if(Stage==2)
  {
   if(Door->IsTargetOpen()){if(Age>.6)return End(TEXT("Normal F failed to start closing"));return false;}
   SawClosing=true;ClosingAt=W->GetTimeSeconds();StartFraction=Door->GetOpenFraction();Key(EKeys::F,false);WalkNode=0;return Next(3);
  }
  if(Stage==3)
  {
   MinFraction=FMath::Min(MinFraction,Door->GetOpenFraction());
   if(Door->IsTargetOpen())
   {
    Key(EKeys::W,false);FString Blockers;const bool PlayerBlocked=ProbeClosing(Blockers);const FVector Local=DoorPose.InverseTransformPosition(P->GetActorLocation());
    if(!SawClosing || !Door->IsObstructed() || MinFraction>=StartFraction-.005f || !PlayerBlocked || FMath::Abs(Local.X)>Door->Width*.5f-P->GetSimpleCollisionRadius() || FMath::Abs(Local.Y)>P->GetSimpleCollisionRadius()+12)
     return End(TEXT("Door target reversed without proven physically occupied player track: ")+Blockers);
    Reversal=true;ReversalAt=W->GetTimeSeconds();ReversalPosition=P->GetActorLocation();ReversalFraction=Door->GetOpenFraction();PlayerBlockerEvidence=Blockers;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("GarageOccupied242-Real-Player-Reversal.png"));return Next(4);
   }
   if(Door->GetOpenFraction()<=.001)return End(FString::Printf(TEXT("Player did not reach the physical descending track before full closure: actual elapsed%.3fs path%.3fcm travelled%.3fcm nativeMaxWalk%.3fcm/s at%s; no remote F, speed fixture or pose bypass"),W->GetTimeSeconds()-ClosingAt,ApproachLength,WalkDistance,P->GetCharacterMovement()->MaxWalkSpeed,*P->GetActorLocation().ToString()));
   if(Age>12)return End(TEXT("Actual W walk did not enter occupied door track within focused12s bound: ")+PathFailure);
   Walk(WalkPath,18);return false;
  }
  if(Stage==4 && Age>.15)
  {
   // Actual W retreat returns along the already surveyed route, clearing the
   // track so its automatic opening can finish. No second close/open cycle.
   ReturnPath.Reset();for(int I=FMath::Min(WalkNode-1,WalkPath.Num()-1);I>=0;--I)ReturnPath.Add(WalkPath[I]);ReturnPath.Add(StartPosition);WalkNode=0;return Next(5);
  }
  if(Stage==5)
  {
   if(!Walk(ReturnPath,25)){if(Age>15)return End(TEXT("Actual W retreat from occupied track became blocked"));return false;}
   Key(EKeys::W,false);OpeningProgress=Door->GetOpenFraction();OpeningProgressAt=W->GetTimeSeconds();return Next(6);
  }
  if(Stage==6)
  {
   if(!Door->IsTargetOpen())return End(TEXT("Safety reversal lost opening target while actual player withdrew"));
   if(Door->GetOpenFraction()<.999)
   {
    // Production collision sweeps deliberately cap each articulated-leaf step
    // to50ms. A rendered capture can take longer than a simulation step.
    // Require actual monotonic native opening, with the unchanged65s whole
    // test bound, rather than treating slow rendering as an opening blockage.
    if(Door->GetOpenFraction()>OpeningProgress+.0025f){OpeningProgress=Door->GetOpenFraction();OpeningProgressAt=W->GetTimeSeconds();}
    if(W->GetTimeSeconds()-OpeningProgressAt>6)return End(TEXT("Automatic opening made no native progress after real track clearance"));return false;
   }
   const FVector Local=DoorPose.InverseTransformPosition(P->GetActorLocation());if(!Reversal || FMath::Abs(Local.Y)<P->GetSimpleCollisionRadius()+100 || !P->GetCharacterMovement()->IsMovingOnGround() || !Focus())return End(TEXT("Player did not return normally to safe focused control outside track"));
   for(int I=0;I<Door->Panels.Num();++I)if(Door->Panels[I]->GetStaticMesh()!=LeafMeshes[I] || !Door->Panels[I]->IsVisible() || Door->Panels[I]->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || Door->Panels[I]->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || Door->Panels[I]->GetCollisionResponseToChannel(ECC_Vehicle)!=ECR_Block)return End(TEXT("Leaf physical presentation/collision changed during reversal"));
   Restored=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("GarageOccupied242-Safe-Retreat-Auto-Open.png"));return End(TEXT(""));
  }
  return false;
 }
private:
 bool Supported(FVector Around,FVector& Stand,FString& Failure)const
 {
  FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageOccupied242Support),false,P);
  if(!W->LineTraceSingleByChannel(Floor,Around+FVector(0,0,130),Around-FVector(0,0,250),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7){Failure=TEXT("floor ")+GetNameSafe(Floor.GetActor())+TEXT("/")+GetNameSafe(Floor.GetComponent());return false;}
  Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q)){Failure=TEXT("full real capsule overlap");return false;}return true;
 }
 bool Segment(FVector A,FVector B,TArray<FVector>* Out,FString& Failure)
 {
  const int Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/25));FVector Previous=A;FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageOccupied242Walking),false,P);const auto Shape=FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());
  for(int I=1;I<=Steps;++I)
  {
   FVector FloorPoint;const FVector Probe=FMath::Lerp(A,B,float(I)/Steps);FHitResult H;FString Why;const bool Support=Supported(Probe,FloorPoint,Why);
   bool Blocked=false;if(Support)Blocked=W->SweepSingleByChannel(H,Previous,FloorPoint,FQuat::Identity,ECC_Pawn,Shape,Q);
   SegmentEvidence+=XYZ(Previous)+TEXT(",")+XYZ(Support?FloorPoint:Probe)+FString::Printf(TEXT(",%d,%d,"),Support,Blocked)+Cell(GetNameSafe(H.GetActor()))+TEXT(",")+Cell(GetNameSafe(H.GetComponent()))+TEXT(",")+Cell(Why)+TEXT("\n");
   if(!Support || Blocked || FMath::Abs(FloorPoint.Z-Previous.Z)>FVector::Dist2D(FloorPoint,Previous)*.6){Failure=Why.IsEmpty()?TEXT("actual capsule sweep ")+GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent()):Why;return false;}
   if(Out && (Out->IsEmpty() || !Out->Last().Equals(FloorPoint,.01)))Out->Add(FloorPoint);Previous=FloorPoint;
  }
  return true;
 }
 bool Plan(FVector From,FVector Goal,TArray<FVector>& Path,FString& Failure)
 {
  Path.Reset();if(Segment(From,Goal,&Path,Failure)){PathMethod=TEXT("Direct native floor/full-capsule25cm");return true;}Path.Reset();
  constexpr float Grid=100;const FIntPoint End(FMath::RoundToInt((Goal.X-From.X)/Grid),FMath::RoundToInt((Goal.Y-From.Y)/Grid));
  struct FNode{FVector Point;FIntPoint Parent;float Cost=BIG_NUMBER;bool Probed=false,Supported=false;};TMap<FIntPoint,FNode> Nodes;TArray<FIntPoint> Open;TSet<FIntPoint> Closed;
  auto Probe=[&](FIntPoint K){auto& N=Nodes.FindOrAdd(K);if(!N.Probed){N.Probed=true;FString Why;N.Supported=Supported(K==End?Goal:From+FVector(K.X*Grid,K.Y*Grid,0),N.Point,Why);}return N.Supported;};
  const FIntPoint Zero(0,0);auto& First=Nodes.FindOrAdd(Zero);First.Point=From;First.Probed=First.Supported=true;First.Cost=0;Open.Add(Zero);bool Found=false;int Count=0;
  while(!Open.IsEmpty() && ++Count<1500)
  {
   int Best=0;float Score=BIG_NUMBER;for(int I=0;I<Open.Num();++I){const auto K=Open[I];const float S=Nodes[K].Cost+FVector2D(K.X-End.X,K.Y-End.Y).Size()*Grid;if(S<Score){Best=I;Score=S;}}
   const FIntPoint K=Open[Best];Open.RemoveAtSwap(Best);if(Closed.Contains(K))continue;Closed.Add(K);if(K==End){Found=true;break;}const FVector AtPoint=Nodes[K].Point;const float Cost=Nodes[K].Cost;
   for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y){if(!X&&!Y)continue;const FIntPoint Next(K.X+X,K.Y+Y);if(Next.X<FMath::Min(0,End.X)-6 || Next.X>FMath::Max(0,End.X)+6 || Next.Y<FMath::Min(0,End.Y)-6 || Next.Y>FMath::Max(0,End.Y)+6 || Closed.Contains(Next) || !Probe(Next))continue;FString Why;if(!Segment(AtPoint,Nodes[Next].Point,nullptr,Why))continue;const float New=Cost+FVector::Dist2D(AtPoint,Nodes[Next].Point);if(New<Nodes[Next].Cost){Nodes[Next].Cost=New;Nodes[Next].Parent=K;Open.AddUnique(Next);}}
  }
  if(!Found){Failure+=FString::Printf(TEXT("; bounded native full-capsule planner exhausted%d nodes"),Count);return false;}
  TArray<FVector> Raw;FIntPoint K=End;while(K!=Zero){Raw.Add(Nodes[K].Point);K=Nodes[K].Parent;}Raw.Add(From);Algo::Reverse(Raw);
  // Prune grid corners only when the complete existing native Segment test
  // proves real floor, full standing capsule and every25cm sweep. The player
  // still uses ordinary W/500cm/s; the door and F radius are never changed.
  // Steering at sparse proven line endpoints avoids following a stair-step
  // grid detour that260 missed the original closing track by38.4cm.
  int Anchor=0;while(Anchor<Raw.Num()-1)
  {
   int Reach=Raw.Num()-1;FString Why;while(Reach>Anchor+1 && !Segment(Raw[Anchor],Raw[Reach],nullptr,Why))--Reach;
   if(!Segment(Raw[Anchor],Raw[Reach],nullptr,Failure)){Path.Reset();return false;}
   Path.Add(Raw[Reach]);Anchor=Reach;
  }
  PathMethod=TEXT("100cm A* grid, farthest-visible line pruning; native floor/full-capsule25cm and unchanged slope/sweep gates on each complete segment");return !Path.IsEmpty();
 }
 bool Walk(const TArray<FVector>& Path,float Radius)
 {
  if(Path.IsEmpty())return false;if(FVector::Dist2D(P->GetActorLocation(),Path.Last())<Radius){Key(EKeys::W,false);return true;}
  while(WalkNode<Path.Num()-1 && FVector::Dist2D(P->GetActorLocation(),Path[WalkNode])<40)++WalkNode;PC->SetControlRotation(FRotator(0,(Path[WalkNode]-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);return false;
 }
 bool ProbeClosing(FString& Names)const
 {
  bool PlayerBlocks=false;FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageOccupied242ReadOnlyClosingGuard),false,Door);const float Fraction=FMath::Max(0.f,Door->GetOpenFraction()-.05f/FMath::Max(2.f,Door->TravelSeconds));
  for(int I=0;I<Door->Panels.Num();++I){FVector Location;FRotator Rotation;Door->PanelPose(I,Fraction,Location,Rotation);const FTransform LeafPose=FTransform(Rotation,Location)*DoorPose;const auto Shape=FCollisionShape::MakeBox(FVector(FMath::Max(200.f,Door->Width)*.5f-.2f,2.8f,Door->Height/20.f-.2f));FHitResult H;
   if(W->SweepSingleByChannel(H,Door->Panels[I]->GetComponentLocation(),LeafPose.GetLocation(),LeafPose.GetRotation(),ECC_Pawn,Shape,Q)){Names+=FString::Printf(TEXT("leaf%d:"),I)+GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent())+TEXT("|");PlayerBlocks|=H.GetActor()==P;}}
  return PlayerBlocks;
 }
 bool Focus(){PC->SetControlRotation((Control->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Control;}
 void Record()
 {
  const float Now=W->GetTimeSeconds();const FVector Position=P->GetActorLocation();if(LastPositionValid)WalkDistance+=FVector::Dist2D(Position,LastPosition);LastPosition=Position;LastPositionValid=true;
  if(Now-LastRecord<.05f)return;LastRecord=Now;const FVector Local=DoorPose.InverseTransformPosition(Position);FString Names;const bool PlayerBlocks=ProbeClosing(Names);
  Trajectory+=FString::Printf(TEXT("%.3f,%d,"),Now,Stage)+XYZ(Position)+TEXT(",")+XYZ(Local)+FString::Printf(TEXT(",%.5f,%d,%d,%d,%d,%.3f,"),Door->GetOpenFraction(),Door->IsTargetOpen(),Door->IsObstructed(),PlayerBlocks,P->GetCharacterMovement()->IsMovingOnGround(),P->GetVelocity().Size())+Cell(Names)+TEXT("\n");
 }
 bool Next(int NewStage){Stage=NewStage;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Previous=Keys.FindOrAdd(K);if(Previous!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Previous=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("normal_close_F_presses"),ClosePresses);J->SetBoolField(TEXT("observed_normal_closing"),SawClosing);J->SetBoolField(TEXT("real_player_collision_guard_reversal"),Reversal);J->SetBoolField(TEXT("safe_real_walk_retreat_auto_open"),Restored);J->SetNumberField(TEXT("post_setup_pose_writes"),0);J->SetNumberField(TEXT("vehicle_pose_writes"),0);J->SetNumberField(TEXT("door_direct_toggle_calls"),0);
  J->SetStringField(TEXT("path_method"),PathMethod);J->SetNumberField(TEXT("actual_supported_path_cm"),ApproachLength);J->SetNumberField(TEXT("actual_walk_distance_cm"),WalkDistance);J->SetNumberField(TEXT("normal_walk_max_cm_s"),P?P->GetCharacterMovement()->MaxWalkSpeed:0);J->SetNumberField(TEXT("minimum_open_fraction"),MinFraction);J->SetNumberField(TEXT("reversal_open_fraction"),ReversalFraction);J->SetNumberField(TEXT("close_to_reversal_actual_seconds"),Reversal?ReversalAt-ClosingAt:0);J->SetStringField(TEXT("actual_player_collision_guard"),PlayerBlockerEvidence);J->SetStringField(TEXT("reversal_world_position"),ReversalPosition.ToString());
  J->SetStringField(TEXT("scope"),TEXT("Focused new occupied-closing protection only. One supported player-only fixture within unchanged normal existing F control radius. Normal F initiates closure; actual mapped W follows native floor/full-capsule route to original door track. PASS requires genuine target reversal/obstructed flag plus independent next-closing leaf sweep hitting actual player capsule while physically within opening, then actual W withdrawal and automatic restored full open. All original door/control/vehicle poses, leaf meshes and real Pawn/Vehicle collision retained. No remote F, Door.Toggle call, speed/fraction/time fixture, collision bypass or repeated display/full-controls acceptance. If physical control distance prevents timely occupancy, preserve exact FAIL."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("GarageOccupied242");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));FFileHelper::SaveStringToFile(FString(TEXT("game_seconds,stage,x,y,z,door_local_x,door_local_y,door_local_z,open_fraction,target_open,obstructed,actual_player_guard,on_ground,speed_cm_s,guard_blockers\n"))+Trajectory,*(Dir/TEXT("trajectory.csv")));FFileHelper::SaveStringToFile(FString(TEXT("from_x,from_y,from_z,to_x,to_y,to_z,supported,blocked,actor,component,floor_failure\n"))+SegmentEvidence,*(Dir/TEXT("walking-floor-capsule.csv")));return true;
 }
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandGarageControl* Control=nullptr;AIslandGarageDoor* Door=nullptr;
 TArray<UStaticMesh*> LeafMeshes;TArray<FVector> WalkPath,ReturnPath;TMap<FKey,bool> Keys;FTransform DoorPose,ControlPose;FVector TrackGoal=FVector::ZeroVector,StartPosition=FVector::ZeroVector,LastPosition=FVector::ZeroVector,ReversalPosition=FVector::ZeroVector;
 int Stage=0,WalkNode=0,ClosePresses=0;double Started=0;float At=0,LastRecord=0,Health=0,ClosingAt=0,ReversalAt=0,StartFraction=1,MinFraction=1,ReversalFraction=0,ApproachLength=0,WalkDistance=0,OpeningProgress=0,OpeningProgressAt=0;
 bool SawClosing=false,Reversal=false,Restored=false,LastPositionValid=false;FString PathFailure,PathMethod,Trajectory,SegmentEvidence,PlayerBlockerEvidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarageOccupied242,"NeonCity.Phase205c.Garage.OccupiedReversal242PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGarageOccupied242::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(GarageOccupied242::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
