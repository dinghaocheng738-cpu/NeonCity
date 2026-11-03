#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/IslandSupercar.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"

namespace DeliveryWalkingDiagnosis236
{
FString Cell(FString S){S.ReplaceInline(TEXT("\""),TEXT("\"\""));return TEXT("\"")+S+TEXT("\"");}
FString XYZ(FVector P){return FString::Printf(TEXT("%.3f,%.3f,%.3f"),P.X,P.Y,P.Z);}
FString Identity(const FHitResult& H){auto* S=Cast<UStaticMeshComponent>(H.GetComponent());return Cell(GetNameSafe(H.GetActor()))+TEXT(",")+Cell(GetNameSafe(H.GetComponent()))+TEXT(",")+Cell(S?GetPathNameSafe(S->GetStaticMesh()):TEXT(""));}
FString Overlaps(UWorld* W,FVector At,const FCollisionShape& Shape,const FCollisionQueryParams& Q)
{
 TArray<FOverlapResult> Found;W->OverlapMultiByChannel(Found,At,FQuat::Identity,ECC_Pawn,Shape,Q);FString Names;
 for(const auto& O:Found)if(O.bBlockingHit){auto* S=Cast<UStaticMeshComponent>(O.GetComponent());Names+=GetNameSafe(O.GetActor())+TEXT("/")+GetNameSafe(O.GetComponent())+TEXT("/")+(S?GetPathNameSafe(S->GetStaticMesh()):TEXT(""))+TEXT("|");}return Names;
}
// Exact current helper search/grid/height/overlap/sweep gates, with read-only
// evidence. Invoke this diagnostic planner once; do not rerun core planner too.
bool Plan(UWorld* W,ACharacter* P,FVector Goal,TArray<FVector>& Path,FString& NodesCSV,FString& EdgesCSV,FString& Summary)
{
 Path.Reset();const FVector From=P->GetActorLocation();if(FVector::Dist2D(From,Goal)<60){Path.Add(Goal);Summary=TEXT("Goal already within60cm");return true;}
 constexpr float CellSize=100;const FIntPoint Finish(FMath::RoundToInt((Goal.X-From.X)/CellSize),FMath::RoundToInt((Goal.Y-From.Y)/CellSize));
 if(FMath::Max(FMath::Abs(Finish.X),FMath::Abs(Finish.Y))>100){Summary=TEXT("Existing100-cell distance gate rejected");return false;}
 struct FNode{FVector Position=FVector::ZeroVector;FIntPoint Parent;float Cost=BIG_NUMBER;bool Probed=false,Walkable=false;};TMap<FIntPoint,FNode> Nodes;TSet<FIntPoint> Closed;TArray<FIntPoint> Open;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(DeliveryReadOnlyWalking236),false,P);const float R=P->GetSimpleCollisionRadius(),Half=P->GetSimpleCollisionHalfHeight();const auto Shape=FCollisionShape::MakeCapsule(R,Half);
 const float Length=FVector::Dist2D(From,Goal);const int MinX=FMath::Min(0,Finish.X)-12,MaxX=FMath::Max(0,Finish.X)+12,MinY=FMath::Min(0,Finish.Y)-12,MaxY=FMath::Max(0,Finish.Y)+12;
 TMap<FString,int32> Reasons;int EdgeBlocked=0,GradeRejected=0;
 auto Probe=[&](FIntPoint Key)->bool
 {
  auto& N=Nodes.FindOrAdd(Key);if(N.Probed)return N.Walkable;N.Probed=true;const FVector XY=Key==Finish?Goal:From+FVector(Key.X*CellSize,Key.Y*CellSize,0);
  const float T=FMath::Clamp(FVector::Dist2D(From,XY)/Length,0.f,1.f),Z=FMath::Lerp(From.Z,Goal.Z,T);FHitResult H;
  const FVector Top(XY.X,XY.Y,Z+350),Bottom(XY.X,XY.Y,Z-450);const bool Hit=W->LineTraceSingleByChannel(H,Top,Bottom,ECC_Pawn,Q);float Water=0;FVector Current;FString Reason,Blocking;
  if(!Hit)Reason=TEXT("no_floor");else if(H.ImpactNormal.Z<.7)Reason=TEXT("floor_normal");
  else if(W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(H.ImpactPoint,Water,Current) && H.ImpactPoint.Z<Water+15)Reason=TEXT("water_clearance");
  else{N.Position=H.ImpactPoint+FVector(0,0,Half+3);Blocking=Overlaps(W,N.Position,Shape,Q);N.Walkable=Blocking.IsEmpty();Reason=N.Walkable?TEXT("clear"):TEXT("capsule_overlap");}
  ++Reasons.FindOrAdd(Reason);NodesCSV+=FString::Printf(TEXT("%d,%d,"),Key.X,Key.Y)+XYZ(Top)+TEXT(",")+XYZ(Bottom)+FString::Printf(TEXT(",%d,"),Hit)+Identity(H)+TEXT(",")+XYZ(H.ImpactPoint)+FString::Printf(TEXT(",%.4f,%.3f,"),H.ImpactNormal.Z,Water)+XYZ(N.Position)+TEXT(",")+Cell(Blocking)+TEXT(",")+Reason+TEXT("\n");return N.Walkable;
 };
 const FIntPoint Zero(0,0);auto& First=Nodes.FindOrAdd(Zero);First.Position=From;First.Probed=First.Walkable=true;First.Cost=0;Open.Add(Zero);int Count=0;bool Pass=false;
 while(!Open.IsEmpty() && ++Count<5000)
 {
  int Best=0;float Score=BIG_NUMBER;for(int I=0;I<Open.Num();++I){const auto K=Open[I];const float S=Nodes[K].Cost+FVector2D(K.X-Finish.X,K.Y-Finish.Y).Size()*CellSize;if(S<Score){Score=S;Best=I;}}
  const FIntPoint Key=Open[Best];Open.RemoveAtSwap(Best);if(Closed.Contains(Key))continue;Closed.Add(Key);
  if(Key==Finish){FIntPoint At=Key;while(At!=Zero){Path.Add(Nodes[At].Position);At=Nodes[At].Parent;}Algo::Reverse(Path);Pass=!Path.IsEmpty();break;}
  const FVector Position=Nodes[Key].Position;const float Cost=Nodes[Key].Cost;
  for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y)
  {
   if(!X && !Y)continue;const FIntPoint Next(Key.X+X,Key.Y+Y);if(Next.X<MinX || Next.X>MaxX || Next.Y<MinY || Next.Y>MaxY || Closed.Contains(Next) || !Probe(Next))continue;
   const FVector To=Nodes[Next].Position;const float Grade=FMath::Abs(To.Z-Position.Z)/FMath::Max(.01f,float(FVector::Dist2D(To,Position)));FHitResult H;bool Blocked=false;FString Reason;
   if(Grade>.6f){++GradeRejected;Reason=TEXT("existing_height_gate");}else {Blocked=W->SweepSingleByChannel(H,Position,To,FQuat::Identity,ECC_Pawn,Shape,Q);if(Blocked){++EdgeBlocked;Reason=TEXT("capsule_sweep");}else Reason=TEXT("clear");}
   EdgesCSV+=FString::Printf(TEXT("%d,%d,%d,%d,"),Key.X,Key.Y,Next.X,Next.Y)+XYZ(Position)+TEXT(",")+XYZ(To)+FString::Printf(TEXT(",%.4f,%d,%d,"),Grade,Blocked,H.bStartPenetrating)+Identity(H)+TEXT(",")+XYZ(H.ImpactPoint)+FString::Printf(TEXT(",%.4f,"),H.ImpactNormal.Z)+Reason+TEXT("\n");
   if(Grade>.6f || Blocked)continue;const float New=Cost+FVector::Dist2D(Position,To);if(New<Nodes[Next].Cost){Nodes[Next].Cost=New;Nodes[Next].Parent=Key;Open.AddUnique(Next);}
  }
 }
 Summary=FString::Printf(TEXT("planner_found=%d nodes%d closed%d iterations%d remaining_open%d finish%d,%d full_capsule_radius%.3f halfheight%.3f sweep_blocked%d grade_rejected%d\n"),Pass,Nodes.Num(),Closed.Num(),Count,Open.Num(),Finish.X,Finish.Y,R,Half,EdgeBlocked,GradeRejected);for(const auto& V:Reasons)Summary+=V.Key+FString::Printf(TEXT("=%d\n"),V.Value);return Pass;
}
class FInspect:public IAutomationLatentCommand
{
public:explicit FInspect(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Delivery walking diagnosis initialization timeout"));return true;}if(!P || W->GetTimeSeconds()<4)return false;
  if(!Setup)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(58100,-159350,300))))return false;
   for(TActorIterator<AIslandActivityPoint> I(W);I;++I)if(I->Index==0 && I->Job && I->Job->ActivityID==TEXT("Island.Activity.Delivery.Provisions"))Pickup=*I;
   FGuid ID;FGuid::Parse(TEXT("7F242340469FEA4D1A7D21A5D5DED892"),ID);for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->VehicleID==ID)Car=*I;
   if(!Pickup || !Car){Test->AddError(TEXT("Actual delivery pickup/original235 selected stock car absent; no substitute fixture made"));return true;}
   const FVector Around=Pickup->GetActorLocation()+FVector(125,0,0);FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(DeliveryDiagnosisDeclaredStart),false,P);
   if(!W->LineTraceSingleByChannel(Floor,Around+FVector(0,0,650),Around-FVector(0,0,900),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7){Test->AddError(TEXT("Same actual delivery start floor unavailable"));return true;}
   const FVector At=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);if(W->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q)){Test->AddError(TEXT("Declared actual delivery start is blocked"));return true;}
   // The only declared pose fixture, identical to Delivery.A supported start.
   // No pickup/activity/cargo/money/input/vehicle state is created or altered.
   P->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Setup=true;AtTime=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-AtTime<.4f)return false;
  const FVector From=P->GetActorLocation();FVector Goal=Car->GetActorLocation();const bool Approach=Phase205cVehicleApproach(W,P,Car,From,Goal);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("DeliveryWalkingDiagnosis236");IFileManager::Get().MakeDirectory(*Dir,true);
  const auto Shape=FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());FCollisionQueryParams Q(SCENE_QUERY_STAT(DeliveryStartGoalEvidence236),false,P);
  FString Approaches=TEXT("radius,direction,probe_x,probe_y,probe_z,floor_hit,actor,component,mesh,floor_x,floor_y,floor_z,normal_z,stand_x,stand_y,stand_z,capsule_blockers,eye_actor,eye_component,valid\n");
  const FVector Center=Car->GetActorLocation(),Near=(From-Center).GetSafeNormal2D();const FVector Directions[]={Near.IsNearlyZero()?-Car->GetActorRightVector():Near,-Car->GetActorRightVector(),Car->GetActorRightVector(),Car->GetActorForwardVector(),-Car->GetActorForwardVector(),FVector(1,0,0),FVector(0,1,0),FVector(-1,0,0),FVector(0,-1,0)};FCollisionQueryParams Ground=Q;Ground.AddIgnoredActor(Car);
  for(float Radius:{215.f,245.f})for(int D=0;D<9;++D)
  {
   const FVector XY=Center+Directions[D]*Radius;FHitResult F,Eye;const bool Hit=W->LineTraceSingleByChannel(F,XY+FVector(0,0,200),XY-FVector(0,0,250),ECC_Pawn,Ground);const FVector Stand=Hit?F.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3):XY;const FString Blocking=Overlaps(W,Stand,Shape,Q);W->LineTraceSingleByChannel(Eye,Stand+FVector(0,0,P->BaseEyeHeight),Center+FVector(0,0,45),ECC_Visibility,Q);
   const bool Valid=Hit && F.ImpactNormal.Z>=.7 && FMath::Abs(F.ImpactPoint.Z-Center.Z)<=150 && Blocking.IsEmpty() && Eye.GetActor()==Car;Approaches+=FString::Printf(TEXT("%.3f,%d,"),Radius,D)+XYZ(XY)+FString::Printf(TEXT(",%d,"),Hit)+Identity(F)+TEXT(",")+XYZ(F.ImpactPoint)+FString::Printf(TEXT(",%.4f,"),F.ImpactNormal.Z)+XYZ(Stand)+TEXT(",")+Cell(Blocking)+TEXT(",")+Cell(GetNameSafe(Eye.GetActor()))+TEXT(",")+Cell(GetNameSafe(Eye.GetComponent()))+FString::Printf(TEXT(",%d\n"),Valid);
  }
  FString Nodes=TEXT("key_x,key_y,ray_from_x,ray_from_y,ray_from_z,ray_to_x,ray_to_y,ray_to_z,hit,actor,component,mesh,impact_x,impact_y,impact_z,normal_z,water_z,stand_x,stand_y,stand_z,capsule_blockers,reason\n"),Edges=TEXT("from_key_x,from_key_y,to_key_x,to_key_y,from_x,from_y,from_z,to_x,to_y,to_z,grade,blocked,start_penetrating,actor,component,mesh,impact_x,impact_y,impact_z,normal_z,reason\n"),Summary;TArray<FVector> Path;bool Found=false;if(Approach)Found=Plan(W,P,Goal,Path,Nodes,Edges,Summary);
  FString Forward=TEXT("direction,from_x,from_y,from_z,to_x,to_y,to_z,blocked,start_penetrating,actor,component,mesh,impact_x,impact_y,impact_z,normal_z\n");const FVector Toward=(Goal-From).GetSafeNormal2D();for(int D=0;D<8;++D){const FVector To=From+Toward.RotateAngleAxis(D*45,FVector::UpVector)*100;FHitResult H;const bool Blocked=W->SweepSingleByChannel(H,From,To,FQuat::Identity,ECC_Pawn,Shape,Q);Forward+=FString::Printf(TEXT("%d,"),D)+XYZ(From)+TEXT(",")+XYZ(To)+FString::Printf(TEXT(",%d,%d,"),Blocked,H.bStartPenetrating)+Identity(H)+TEXT(",")+XYZ(H.ImpactPoint)+FString::Printf(TEXT(",%.4f\n"),H.ImpactNormal.Z);}
  FString Bounds=TEXT("actor,component,mesh,min_x,min_y,min_z,max_x,max_y,max_z,pawn_response,profile\n");
  for(TActorIterator<AActor> I(W);I;++I){TInlineComponentArray<UPrimitiveComponent*> Cs(*I);for(auto* C:Cs)if(C->IsCollisionEnabled() && C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block){const FVector AB=Goal-From;const double T=AB.SizeSquared2D()>1?FMath::Clamp(FVector::DotProduct(C->Bounds.Origin-From,FVector(AB.X,AB.Y,0))/AB.SizeSquared2D(),0.,1.):0;const FVector On=From+AB*T;if(FVector::Dist2D(C->Bounds.Origin,On)>900 || C->Bounds.GetBox().Min.Z>1300 || C->Bounds.GetBox().Max.Z<-600)continue;auto* S=Cast<UStaticMeshComponent>(C);const FBox B=C->Bounds.GetBox();Bounds+=Cell(I->GetName())+TEXT(",")+Cell(C->GetName())+TEXT(",")+Cell(S?GetPathNameSafe(S->GetStaticMesh()):C->GetDetailedInfo())+TEXT(",")+XYZ(B.Min)+TEXT(",")+XYZ(B.Max)+FString::Printf(TEXT(",%d,"),C->GetCollisionResponseToChannel(ECC_Pawn))+Cell(C->GetCollisionProfileName().ToString())+TEXT("\n");}}
  const FString Scope=FString::Printf(TEXT("READ ONLY DELIVERY WALKING DIAGNOSIS, one declared physically supported same235 pickup start only. No W/F/pickup/cargo/activity/car/ownership/reward/nav/collision writes. Settled actual start%s goal%s originalCar%s ID%s approach%d copied_helper_path%d onground%d current_floor%d\n"),*From.ToString(),*Goal.ToString(),*Car->GetName(),*Car->VehicleID.ToString(),Approach,Found,P->GetCharacterMovement()->IsMovingOnGround(),P->GetCharacterMovement()->CurrentFloor.IsWalkableFloor())+Summary+TEXT("Diagnostic copied current PhysicalAccess100cm grid/5000 nodes/normal.7/water15/height.6/full physical capsule, unchanged gates. Start is marked walkable exactly as helper; nodes/edges retain real blockers and limit outcome. Not actual walking or gameplay PASS.\n");
  FString Route=TEXT("x,y,z\n");for(const auto Point:Path)Route+=XYZ(Point)+TEXT("\n");FFileHelper::SaveStringToFile(Route,*(Dir/TEXT("planned-path.csv")));FFileHelper::SaveStringToFile(Nodes,*(Dir/TEXT("planner-nodes.csv")));FFileHelper::SaveStringToFile(Edges,*(Dir/TEXT("planner-edges.csv")));FFileHelper::SaveStringToFile(Approaches,*(Dir/TEXT("car-approaches.csv")));FFileHelper::SaveStringToFile(Forward,*(Dir/TEXT("start-forward-capsule.csv")));FFileHelper::SaveStringToFile(Bounds,*(Dir/TEXT("nearby-pawn-collision.csv")));FFileHelper::SaveStringToFile(Scope,*(Dir/TEXT("scope.txt")));Test->AddInfo(TEXT("Actual walking diagnosis retained; no path/walking/gameplay PASS granted"));return true;
 }
 FAutomationTestBase* Test;double Started=0;float AtTime=0;bool Setup=false;AIslandActivityPoint* Pickup=nullptr;AIslandSupercar* Car=nullptr;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryWalkingDiagnosis236,"NeonCity.Phase205c.Activity.Delivery.WalkingDiagnosis236PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryWalkingDiagnosis236::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(DeliveryWalkingDiagnosis236::FInspect(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
