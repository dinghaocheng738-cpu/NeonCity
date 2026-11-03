#pragma once
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Algo/Reverse.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
/** A read-only native floor/capsule/eye survey for the one declared fixture
 * start or a real W walking destination. Never moves a car or clears collision. */
inline bool Phase205cVehicleApproach(UWorld* W,ACharacter* Player,APawn* Car,FVector From,FVector& Stand)
{
 const FVector Center=Car->GetActorLocation(),Near=(From-Center).GetSafeNormal2D();
 const FVector Directions[]={Near.IsNearlyZero()?-Car->GetActorRightVector():Near,-Car->GetActorRightVector(),Car->GetActorRightVector(),Car->GetActorForwardVector(),-Car->GetActorForwardVector(),FVector(1,0,0),FVector(0,1,0),FVector(-1,0,0),FVector(0,-1,0)};
 FCollisionQueryParams Ground(SCENE_QUERY_STAT(ActualCarApproachFloor),false,Player);Ground.AddIgnoredActor(Car);FCollisionQueryParams Q(SCENE_QUERY_STAT(ActualCarApproachClearance),false,Player);
 for(const float Radius:{215.f,245.f})for(const auto& Direction:Directions)
 {
  FHitResult H,Eye;const FVector At=Center+Direction*Radius;
  if(!W->LineTraceSingleByChannel(H,At+FVector(0,0,200),At-FVector(0,0,250),ECC_Pawn,Ground) || H.ImpactNormal.Z<.7 || FMath::Abs(H.ImpactPoint.Z-Center.Z)>150)continue;
  const FVector Candidate=H.ImpactPoint+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3);
  if(W->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight()),Q))continue;
  W->LineTraceSingleByChannel(Eye,Candidate+FVector(0,0,Player->BaseEyeHeight),Center+FVector(0,0,45),ECC_Visibility,Q);if(Eye.GetActor()!=Car)continue;
  Stand=Candidate;return true;
 }
 return false;
}

/** Test-only walking planner over actual native floor/capsule sweeps. Returned
 * points steer mapped W input; no actor, collision, nav graph or game state is
 * written. Bounded A* avoids asking the real character to walk through walls. */
struct FPhase205cWalkSurvey
{
 int32 Expanded=0,Probed=0,OpenAtExit=0;FString Failure;TMap<FString,int32> Rejections;TArray<FVector> Explored;
 void Reject(const TCHAR* Kind,const FHitResult& Hit){const FString Name=Hit.GetActor()?Hit.GetActor()->GetName():TEXT("None");++Rejections.FindOrAdd(FString(Kind)+TEXT(":")+Name);}
};
inline bool Phase205cSupportedWalkPath(UWorld* W,ACharacter* P,FVector Goal,TArray<FVector>& Path,float GroundProbeAboveCenter=350.f,float GridCellCm=100.f,FPhase205cWalkSurvey* Survey=nullptr,int32 NodeLimit=5000)
{
 Path.Reset();if(!IsValid(W) || !IsValid(P) || Goal.ContainsNaN() || !FMath::IsFinite(GroundProbeAboveCenter) || GroundProbeAboveCenter<=0 || !FMath::IsFinite(GridCellCm) || GridCellCm<20 || GridCellCm>100)return false;
 const FVector From=P->GetActorLocation();if(From.ContainsNaN())return false;
 const auto* Move=P->GetCharacterMovement();auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();if(!Move || !Marine)return false;
 if(FVector::Dist2D(From,Goal)<60){Path.Add(Goal);return true;}
 // Bound before float-to-grid conversion as well as after rounding.
 if(FMath::Max(FMath::Abs(Goal.X-From.X),FMath::Abs(Goal.Y-From.Y))>10050){if(Survey)Survey->Failure=TEXT("Physical100.5m bound exceeded");return false;}
 const float Cell=GridCellCm;const FIntPoint Finish(FMath::RoundToInt((Goal.X-From.X)/Cell),FMath::RoundToInt((Goal.Y-From.Y)/Cell));
 // Preserve the100.5m physical bound for every supported cell size. A fixed
 //100-cell cap incorrectly limited50cm surveys to50m after the BBQ reflow.
 const int32 GridBound=FMath::CeilToInt(10050.f/Cell);
 if(FMath::Max(FMath::Abs(Finish.X),FMath::Abs(Finish.Y))>GridBound)return false;
 struct FNode{FVector Position;FIntPoint Parent;float Cost=BIG_NUMBER;bool Probed=false,Walkable=false;};TMap<FIntPoint,FNode> Nodes;TSet<FIntPoint> Closed;TArray<FIntPoint> Open;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(ActualWalkPath),false,P);const float R=P->GetSimpleCollisionRadius(),Half=P->GetSimpleCollisionHalfHeight();
 if(!FMath::IsFinite(R) || !FMath::IsFinite(Half) || R<=0 || Half<R)return false;
 const auto Shape=FCollisionShape::MakeCapsule(R,Half);
 const float Length=FVector::Dist2D(From,Goal);const int Margin=FMath::CeilToInt(1200.f/Cell);
 const int MinX=FMath::Min(0,Finish.X)-Margin,MaxX=FMath::Max(0,Finish.X)+Margin,MinY=FMath::Min(0,Finish.Y)-Margin,MaxY=FMath::Max(0,Finish.Y)+Margin;
 // Query the native StepUp sequence without moving the character. A diagonal
 // centre-to-centre sweep hits a low curb's vertical face even when ordinary
 // CharacterMovement can step onto it. Every leg retains the complete capsule,
 // all blocking geometry, the character's actual step limit and native floor
 // eligibility. Failure on any leg leaves this edge blocked.
 auto SupportedStep=[&](const FVector& Position,const FVector& To,const FHitResult& Obstacle)->bool
 {
  const float StepHeight=Move->MaxStepHeight;
  if(!FMath::IsFinite(StepHeight) || StepHeight<=0 || !Obstacle.IsValidBlockingHit() ||
     Obstacle.ImpactPoint.ContainsNaN() || Obstacle.ImpactNormal.ContainsNaN() || !Move->CanStepUp(Obstacle))return false;
  FHitResult SourceFloor;
  const FVector Feet=Position-FVector(0,0,Half);
  if(!W->LineTraceSingleByChannel(SourceFloor,Feet+FVector(0,0,5),Feet-FVector(0,0,10),ECC_Pawn,Q) ||
     !SourceFloor.IsValidBlockingHit() || SourceFloor.ImpactPoint.ContainsNaN() || SourceFloor.ImpactNormal.ContainsNaN() ||
     SourceFloor.ImpactNormal.Z<.7 || !Move->IsWalkable(SourceFloor))return false;
  const float FloorZ=SourceFloor.ImpactPoint.Z,Gap=Feet.Z-FloorZ;
  const float TargetFloorZ=To.Z-Half-3;
  if(Gap<0 || Gap>5 || TargetFloorZ-FloorZ>StepHeight ||
     Obstacle.ImpactPoint.Z<=FloorZ || Obstacle.ImpactPoint.Z>Position.Z+Half-R)return false;
  // Native StepUp subtracts the current floor gap from its upward travel.
  // Consequently neither the capsule nor the intended landing can be lifted
  // beyond MaxStepHeight measured from the actual supporting floor.
  const float Lift=FMath::Max(0.f,StepHeight-Gap);if(Lift<=0)return false;
  const FVector Raised=Position+FVector(0,0,Lift),Across(To.X,To.Y,Raised.Z);
  FHitResult H;
  if(W->SweepSingleByChannel(H,Position,Raised,FQuat::Identity,ECC_Pawn,Shape,Q) ||
     W->OverlapBlockingTestByChannel(Raised,FQuat::Identity,ECC_Pawn,Shape,Q))return false;
  if(W->SweepSingleByChannel(H,Raised,Across,FQuat::Identity,ECC_Pawn,Shape,Q) ||
     W->OverlapBlockingTestByChannel(Across,FQuat::Identity,ECC_Pawn,Shape,Q))return false;
  FHitResult Landing;
  const FVector Down=Across-FVector(0,0,StepHeight+UCharacterMovementComponent::MAX_FLOOR_DIST*2);
  if(!W->SweepSingleByChannel(Landing,Across,Down,FQuat::Identity,ECC_Pawn,Shape,Q) ||
     !Landing.IsValidBlockingHit() || Landing.Location.ContainsNaN() || Landing.ImpactPoint.ContainsNaN() || Landing.ImpactNormal.ContainsNaN() ||
     Landing.ImpactNormal.Z<.7 || !Move->IsWalkable(Landing) ||
     !Move->IsWithinEdgeTolerance(Landing.Location,Landing.ImpactPoint,R) ||
     Landing.ImpactPoint.Z-FloorZ>StepHeight ||
     (Landing.ImpactPoint.Z>FloorZ && !Move->CanStepUp(Landing)))return false;
  // The capsule must land on the same supported height surveyed for this node,
  // rather than perch on a side face, stop on an intervening object or float.
  if(FMath::Abs(Landing.ImpactPoint.Z-TargetFloorZ)>3 ||
     FMath::Abs(Landing.Location.Z+3-To.Z)>3)return false;
  float Water=0;FVector Current;
  if(Marine->SampleWater(Landing.ImpactPoint,Water,Current) && (!FMath::IsFinite(Water) || Landing.ImpactPoint.Z<Water+15))return false;
  return !W->OverlapBlockingTestByChannel(To,FQuat::Identity,ECC_Pawn,Shape,Q);
 };
 auto Probe=[&](FIntPoint Key)->bool
 {
  auto& N=Nodes.FindOrAdd(Key);if(N.Probed)return N.Walkable;N.Probed=true;if(Survey)++Survey->Probed;const FVector XY=Key==Finish?Goal:From+FVector(Key.X*Cell,Key.Y*Cell,0);
  const float T=FMath::Clamp(FVector::Dist2D(From,XY)/Length,0.f,1.f),Z=FMath::Lerp(From.Z,Goal.Z,T);FHitResult H;
  if(!W->LineTraceSingleByChannel(H,FVector(XY.X,XY.Y,Z+GroundProbeAboveCenter),FVector(XY.X,XY.Y,Z-450),ECC_Pawn,Q) || !H.IsValidBlockingHit() || H.ImpactPoint.ContainsNaN() || H.ImpactNormal.ContainsNaN() || H.ImpactNormal.Z<.7){if(Survey)Survey->Reject(TEXT("Floor"),H);return false;}
  float Water=0;FVector Current;if(Marine->SampleWater(H.ImpactPoint,Water,Current) && (!FMath::IsFinite(Water) || H.ImpactPoint.Z<Water+15))return false;
  N.Position=H.ImpactPoint+FVector(0,0,Half+3);N.Walkable=!W->OverlapBlockingTestByChannel(N.Position,FQuat::Identity,ECC_Pawn,Shape,Q);if(!N.Walkable && Survey){TArray<FOverlapResult> Hits;W->OverlapMultiByChannel(Hits,N.Position,FQuat::Identity,ECC_Pawn,Shape,Q);for(const auto& O:Hits)if(O.bBlockingHit && O.GetActor())++Survey->Rejections.FindOrAdd(TEXT("Capsule:")+O.GetActor()->GetName());}return N.Walkable;
 };
 // An occupied or unsupported destination cannot be reached. Report it before
 // expanding thousands of cells, keeping failed fixture diagnosis bounded.
 if(!Probe(Finish)){if(Survey)Survey->Failure=TEXT("Destination unsupported or capsule blocked");return false;}
 const FIntPoint Zero(0,0);auto& First=Nodes.FindOrAdd(Zero);First.Position=From;First.Probed=First.Walkable=true;First.Cost=0;Open.Add(Zero);int Count=0;
 while(!Open.IsEmpty() && ++Count<FMath::Clamp(NodeLimit,5000,50000))
 {
  int Best=0;float Score=BIG_NUMBER;for(int I=0;I<Open.Num();++I){const auto K=Open[I];const float S=Nodes[K].Cost+FVector2D(K.X-Finish.X,K.Y-Finish.Y).Size()*Cell;if(S<Score){Score=S;Best=I;}}
  const FIntPoint Key=Open[Best];Open.RemoveAtSwap(Best);if(Closed.Contains(Key))continue;Closed.Add(Key);
  if(Key==Finish){FIntPoint At=Key;while(At!=Zero){Path.Add(Nodes[At].Position);At=Nodes[At].Parent;}Algo::Reverse(Path);return !Path.IsEmpty();}
  const FVector Position=Nodes[Key].Position;const float Cost=Nodes[Key].Cost;if(Survey)Survey->Explored.Add(Position);
  for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y)
  {
   if(!X && !Y)continue;const FIntPoint Next(Key.X+X,Key.Y+Y);if(Next.X<MinX || Next.X>MaxX || Next.Y<MinY || Next.Y>MaxY || Closed.Contains(Next) || !Probe(Next))continue;
   const FVector To=Nodes[Next].Position;
   // Fine cells resolve individual stair treads. A single legal step can be
   // higher than 0.6 * cell length; the native full-capsule StepUp below still
   // owns admissibility. Legacy callers retain their original ramp cutoff.
   const float RiseLimit=Cell<100?FMath::Max(FVector::Dist2D(To,Position)*.6f,Move->MaxStepHeight):FVector::Dist2D(To,Position)*.6f;
   if(FMath::Abs(To.Z-Position.Z)>RiseLimit)continue;FHitResult H;
   if(W->SweepSingleByChannel(H,Position,To,FQuat::Identity,ECC_Pawn,Shape,Q) && !SupportedStep(Position,To,H)){if(Survey)Survey->Reject(TEXT("Sweep"),H);continue;}
   const float New=Cost+FVector::Dist2D(Position,To);if(New<Nodes[Next].Cost){Nodes[Next].Cost=New;Nodes[Next].Parent=Key;Open.AddUnique(Next);}
  }
 }
 if(Survey){Survey->Expanded=Count;Survey->OpenAtExit=Open.Num();Survey->Failure=Open.IsEmpty()?TEXT("No connected supported cells"):TEXT("Node budget exhausted");}
 return false;
}
