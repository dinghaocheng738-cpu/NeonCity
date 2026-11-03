#include "Navigation/NeonCityRoadGuide.h"
#include "Algo/Reverse.h"
namespace
{
struct FProjection
{
 int32 Segment=INDEX_NONE;
 double Alpha=0,DistanceSquared=TNumericLimits<double>::Max();
 FVector Point=FVector::ZeroVector;
};
FProjection Project(const FVector& Position,TConstArrayView<FVector> Points,bool Closed=false)
{
 FProjection Best;
 const int32 Count=Closed?Points.Num():Points.Num()-1;
 for(int32 I=0;I<Count;++I)
 {
  const FVector A=Points[I],B=Points[(I+1)%Points.Num()];
  const FVector AB(B-A),AP(Position-A);
  const double Alpha=AB.SizeSquared()>1?FMath::Clamp(FVector::DotProduct(AP,AB)/AB.SizeSquared(),0.,1.):0;
  const FVector Point=FMath::Lerp(A,B,Alpha);
  const double Distance=FVector::DistSquared(Position,Point);
  if(Distance<Best.DistanceSquared)Best={I,Alpha,Distance,Point};
 }
 return Best;
}
FProjection Project(const FVector& Position,const FNeonCityGuideRoad& Road)
{return Project(Position,Road.Points,Road.Closed);}
}
bool FNeonCityRoadGuide::AddRoad(FNeonCityGuideRoad Road)
{
 if(Roads.Num()>=MaxRoadCount || Road.Points.Num()<2 || Road.Points.Num()>4096 || PointCount+Road.Points.Num()>16384)return false;
 double Length=0;
 for(int32 I=0;I<Road.Points.Num();++I)
 {
  if(Road.Points[I].ContainsNaN())return false;
  if(I>0)Length+=FVector::Dist2D(Road.Points[I-1],Road.Points[I]);
 }
 if(Length<1)return false;
 PointCount+=Road.Points.Num();Roads.Add(MoveTemp(Road));return true;
}
bool FNeonCityRoadGuide::FindRoute(const FVector& Origin,const FVector& Goal,TArray<FVector>& Route) const
{
 Route.Reset();if(Origin.ContainsNaN() || Goal.ContainsNaN())return false;
 // Centreline road graphs share the same connected route for phone guidance
 // and driving. Legacy one-way lane presentation retains its wider parking connector.
 if(Roads.ContainsByPredicate([](const auto& R){return R.Bidirectional;}))
 {
  TArray<FVector> BestRoute;double BestLength=TNumericLimits<double>::Max();
  for(const auto& R:Roads)
  {
   const auto Q=Project(Origin,R);if(Q.Segment==INDEX_NONE || FVector::Dist2D(Origin,Q.Point)>400 || FMath::Abs(Origin.Z-Q.Point.Z)>180)continue;
   const FVector Direction=(R.Points[(Q.Segment+1)%R.Points.Num()]-R.Points[Q.Segment]).GetSafeNormal();
   for(float Sign:{1.f,-1.f})
   {
    TArray<FVector> Candidate;float Limit;bool Loop;
    if(!FindDriveRoute(Origin,Direction*Sign,&Goal,Candidate,Limit,Loop))continue;
    double Length=0;for(int32 I=1;I<Candidate.Num();++I)Length+=FVector::Distance(Candidate[I-1],Candidate[I]);
    if(Length<BestLength){BestLength=Length;BestRoute=MoveTemp(Candidate);}
   }
  }
  if(!BestRoute.IsEmpty()){Route=MoveTemp(BestRoute);Route.Insert(Origin,0);Route.Add(Goal);return true;}
 }
 const FNeonCityGuideRoad* Chosen=nullptr;FProjection Start,End;
 double Best=TNumericLimits<double>::Max();
 for(const auto& Road:Roads)
 {
  const auto A=Project(Origin,Road),B=Project(Goal,Road);
  // Never invent an off-road connection across distant blocks or water.
  if(A.Segment==INDEX_NONE || B.Segment==INDEX_NONE || A.DistanceSquared>FMath::Square(1800.) || B.DistanceSquared>FMath::Square(1800.))continue;
  const bool Ahead=B.Segment>A.Segment || (B.Segment==A.Segment && B.Alpha>=A.Alpha);
  if(!Road.Closed && !Ahead)continue;
  const double Score=A.DistanceSquared+B.DistanceSquared;
  if(Score<Best) { Best=Score;Chosen=&Road;Start=A;End=B; }
 }
 if(!Chosen)return false;
 auto Add=[&](const FVector& Point){ if(Route.IsEmpty() || !Route.Last().Equals(Point,1))Route.Add(Point); };
 Add(Origin);Add(Start.Point);
 if(Start.Segment!=End.Segment || End.Alpha<Start.Alpha)
 {
  const int32 N=Chosen->Points.Num();int32 Segment=Start.Segment;
  for(int32 Step=0;Step<N;++Step)
  {
   Add(Chosen->Points[(Segment+1)%N]);Segment=(Segment+1)%N;
   if(Segment==End.Segment)break;
  }
 }
 Add(End.Point);Add(Goal);return Route.Num()>=2;
}

bool FNeonCityRoadGuide::FindDriveRoute(const FVector& Origin,const FVector& Forward,const FVector* Goal,
 TArray<FVector>& Route,float& SpeedLimitKmh,bool& Looped,TArray<float>* PointSpeedLimits) const
{
 Route.Reset();if(PointSpeedLimits)PointSpeedLimits->Reset();Looped=false;SpeedLimitKmh=0;
 if(Origin.ContainsNaN() || Forward.ContainsNaN() || (Goal && Goal->ContainsNaN()))return false;
 struct FArc { int32 Road;int32 Part=0,PartCount=1;bool Reverse,WholeLoop=false;TArray<FVector> P;float Length=0; };
 TArray<FArc> Arcs;
 // Stable IDs make equally valid junction choices reproducible between processes.
 TArray<int32> Order;for(int32 I=0;I<Roads.Num();++I)Order.Add(I);
 Order.StableSort([&](int32 A,int32 B){return Roads[A].RoadID.LexicalLess(Roads[B].RoadID);});
 for(int32 I:Order)
 {
  const auto& R=Roads[I];if(!R.CruiseEligible)continue;
  TArray<TArray<FVector>> Parts;
  // Closed streets also need explicit junction ports. Treat their closing
  // edge as an ordinary final segment, retaining a whole-loop arc when no
  // measured connector splits it. This preserves legacy loop behaviour.
  {
   // Preserve every original road sample. A measured parking connector can
   // end at an interior segment of the main road, so split only this query's
   // directed arcs there. Never rewrite a road actor, bend a road towards a
   // connector, or join an adjacent hairpin at another elevation.
   struct FNode { double Along;FVector Point;bool Cut; };
   TArray<FNode> Nodes;double Along=0;
   const int32 Count=R.Points.Num()+(R.Closed?1:0);
   for(int32 N=0;N<Count;++N)
   {const FVector Point=R.Points[N%R.Points.Num()];if(N>0)Along+=FVector::Distance(R.Points[N-1],Point);Nodes.Add({Along,Point,N==0 || N==Count-1});}
   for(int32 Other=0;Other<Roads.Num();++Other)
   {
    const auto& Connector=Roads[Other];if(Other==I || !Connector.CruiseEligible || !Connector.NativeMeasured || Connector.Closed)continue;
    for(const FVector Endpoint:{Connector.Points[0],Connector.Points.Last()})
    {
     const auto Q=Project(Endpoint,R);
     if(Q.Segment==INDEX_NONE || FVector::Dist2D(Endpoint,Q.Point)>200 || FMath::Abs(Endpoint.Z-Q.Point.Z)>70)continue;
     double CutDistance=0;for(int32 N=0;N<Q.Segment;++N)CutDistance+=FVector::Distance(R.Points[N],R.Points[N+1]);
     CutDistance+=FVector::Distance(R.Points[Q.Segment],Q.Point);
     if(CutDistance<=1 || CutDistance>=Along-1)continue;
     bool Existing=false;for(auto& Node:Nodes)if(FMath::Abs(Node.Along-CutDistance)<=1){Node.Cut=true;Existing=true;break;}
     if(!Existing)Nodes.Add({CutDistance,Q.Point,true});
    }
   }
   Nodes.StableSort([](const FNode& A,const FNode& B){return A.Along<B.Along;});
   TArray<FVector> Piece;
   for(const auto& Node:Nodes)
   {
    Piece.Add(Node.Point);
    if(Node.Cut && Piece.Num()>1){Parts.Add(MoveTemp(Piece));Piece.Reset();Piece.Add(Node.Point);}
   }
  }
  const bool WholeLoop=R.Closed && Parts.Num()==1;
  if(WholeLoop)Parts[0].Pop(); // The legacy whole-loop arc closes below.
  for(int32 Direction=0;Direction<(R.Bidirectional && !WholeLoop?2:1);++Direction)
  {
   for(int32 Part=0;Part<Parts.Num();++Part)
   {
   FArc A;A.Road=I;A.Part=Part;A.PartCount=Parts.Num();A.WholeLoop=WholeLoop;A.Reverse=Direction!=0;A.P=Parts[Part];
   if(A.Reverse)Algo::Reverse(A.P);
   // Add may reallocate: never pass a reference into the same array.
   if(WholeLoop){const FVector FirstPoint=A.P[0];A.P.Add(FirstPoint);}
   for(int32 J=1;J<A.P.Num();++J)A.Length+=FVector::Distance(A.P[J-1],A.P[J]);
   Arcs.Add(MoveTemp(A));
   }
  }
 }
 int32 Start=INDEX_NONE;FProjection S;double Score=TNumericLimits<double>::Max();
 auto ArcProjection=[&](const FVector& P,const FArc& A){return Project(P,A.P);};
 const FVector F=Forward.GetSafeNormal2D();
 for(int32 I=0;I<Arcs.Num();++I)
 {
  const auto Q=ArcProjection(Origin,Arcs[I]);if(Q.Segment==INDEX_NONE)continue;
  const FVector Tangent=(Arcs[I].P[Q.Segment+1]-Arcs[I].P[Q.Segment]).GetSafeNormal2D();
  const float Alignment=FVector::DotProduct(F,Tangent);
  if(FVector::Dist2D(Origin,Q.Point)>400 || FMath::Abs(Origin.Z-Q.Point.Z)>180 || Alignment<.15f)continue;
  const double Candidate=Q.DistanceSquared+FMath::Square((1-Alignment)*180.);
  if(Candidate<Score){Score=Candidate;Start=I;S=Q;}
 }
 if(Start==INDEX_NONE)return false;
 auto Connected=[&](int32 A,int32 B)
 {
  if(A==B)return false;
  int32 NextPart=Arcs[A].Part+(Arcs[A].Reverse?-1:1);
  if(Roads[Arcs[A].Road].Closed)NextPart=(NextPart+Arcs[A].PartCount)%Arcs[A].PartCount;
  if(Arcs[A].Road==Arcs[B].Road && (Arcs[A].Reverse!=Arcs[B].Reverse || Arcs[B].Part!=NextPart))return false;
  const FVector Exit=Arcs[A].P.Last(),Entry=Arcs[B].P[0];
  return FVector::Dist2D(Exit,Entry)<=200 && FMath::Abs(Exit.Z-Entry.Z)<=70;
 };
 TArray<int32> Chain;Chain.Add(Start);FProjection End;int32 EndArc=INDEX_NONE,EndPredecessor=INDEX_NONE;bool ReturnToStart=false;
 if(Goal)
 {
  TArray<double> Cost;Cost.Init(TNumericLimits<double>::Max(),Arcs.Num());
  TArray<int32> Previous;Previous.Init(INDEX_NONE,Arcs.Num());TArray<bool> Visited;Visited.Init(false,Arcs.Num());
  Cost[Start]=0;double Best=TNumericLimits<double>::Max(),BestGoalDistance=TNumericLimits<double>::Max();
  for(int32 Step=0;Step<Arcs.Num();++Step)
  {
   int32 Current=INDEX_NONE;double Lowest=TNumericLimits<double>::Max();
   for(int32 I=0;I<Arcs.Num();++I)if(!Visited[I] && Cost[I]<Lowest){Lowest=Cost[I];Current=I;}
   if(Current==INDEX_NONE)break;Visited[Current]=true;
   const auto Q=ArcProjection(*Goal,Arcs[Current]);
   const bool Ahead=Current!=Start || Arcs[Start].WholeLoop || Q.Segment>S.Segment || (Q.Segment==S.Segment && Q.Alpha>=S.Alpha);
   if(Ahead && FVector::Dist2D(*Goal,Q.Point)<=400 && FMath::Abs(Goal->Z-Q.Point.Z)<=180)
   {
    float Partial=0;for(int32 J=0;J<Q.Segment;++J)Partial+=FVector::Distance(Arcs[Current].P[J],Arcs[Current].P[J+1]);
    Partial+=FVector::Distance(Arcs[Current].P[Q.Segment],Q.Point);
    const double Total=Cost[Current]+Partial+FMath::Sqrt(Q.DistanceSquared);
    // Reach the closest connected road projection before comparing travel cost.
    // Otherwise a shorter neighbouring road can steal an exact garage destination,
    // causing the safety stop to occur before the actual entrance. Keep endpoints
    // on real authored arcs, and retain the existing vertical/connectivity checks.
    const double GoalDistance=FMath::Sqrt(Q.DistanceSquared);
    if(GoalDistance<BestGoalDistance-1. || (FMath::Abs(GoalDistance-BestGoalDistance)<=1. && Total<Best))
    {BestGoalDistance=GoalDistance;Best=Total;EndArc=Current;EndPredecessor=INDEX_NONE;End=Q;}
   }
   // A split cycle may return to the starting arc before reaching a goal
   // behind its initial projection. Dijkstra must not reopen Start, but its
   // final prefix is still a valid terminal leg after a complete cycle.
   if(Current!=Start && Roads[Arcs[Start].Road].Closed && !Arcs[Start].WholeLoop && Connected(Current,Start))
   {
    const auto Back=ArcProjection(*Goal,Arcs[Start]);const bool Behind=Back.Segment<S.Segment || (Back.Segment==S.Segment && Back.Alpha<S.Alpha);
    const double GoalDistance=FMath::Sqrt(Back.DistanceSquared);
    if(Behind && FVector::Dist2D(*Goal,Back.Point)<=400 && FMath::Abs(Goal->Z-Back.Point.Z)<=180)
    {
     double Prefix=0;for(int32 J=0;J<Back.Segment;++J)Prefix+=FVector::Distance(Arcs[Start].P[J],Arcs[Start].P[J+1]);Prefix+=FVector::Distance(Arcs[Start].P[Back.Segment],Back.Point);
     const double Total=Cost[Current]+Arcs[Current].Length+FVector::Distance(Arcs[Current].P.Last(),Arcs[Start].P[0])+Prefix+GoalDistance;
     if(GoalDistance<BestGoalDistance-1. || (FMath::Abs(GoalDistance-BestGoalDistance)<=1. && Total<Best))
     {BestGoalDistance=GoalDistance;Best=Total;EndArc=Start;EndPredecessor=Current;End=Back;}
    }
   }
   for(int32 I=0;I<Arcs.Num();++I)if(Connected(Current,I))
   {
    const double Candidate=Cost[Current]+Arcs[Current].Length+FVector::Distance(Arcs[Current].P.Last(),Arcs[I].P[0]);
    if(Candidate<Cost[I]){Cost[I]=Candidate;Previous[I]=Current;}
   }
  }
  if(EndArc==INDEX_NONE)return false;
  Chain.Reset();for(int32 I=EndPredecessor!=INDEX_NONE?EndPredecessor:EndArc;I!=INDEX_NONE;I=Previous[I]){Chain.Insert(I,0);if(I==Start)break;}
  if(EndPredecessor!=INDEX_NONE)Chain.Add(Start);
  if(Chain.IsEmpty() || Chain[0]!=Start)return false;
 }
 else if(!Arcs[Start].WholeLoop)
 {
  // Follow a deterministic connected branch. A terminal road is a safe stop, never a U-turn teleport.
  for(int32 Step=0;Step<Arcs.Num()-1;++Step)
  {
   const int32 Current=Chain.Last();const FVector Direction=(Arcs[Current].P.Last()-Arcs[Current].P[Arcs[Current].P.Num()-2]).GetSafeNormal();
   int32 Next=INDEX_NONE;float Alignment=-2;
   for(int32 I=0;I<Arcs.Num();++I)if(!Chain.Contains(I) && Connected(Current,I))
   {
    const float Dot=FVector::DotProduct(Direction,(Arcs[I].P[1]-Arcs[I].P[0]).GetSafeNormal());
    if(Dot>Alignment+.001f){Alignment=Dot;Next=I;}
   }
   if(Next==INDEX_NONE){ReturnToStart=Connected(Current,Start);break;}Chain.Add(Next);
  }
  // A cycle can consume every arc, exhausting the bounded loop immediately
  // after appending its final road. Still recognise its authored closing port.
  if(!ReturnToStart && Chain.Num()>1)ReturnToStart=Connected(Chain.Last(),Start);
 }
 float CurrentSpeedLimit=45;
 auto Add=[&](const FVector& P,const FVector& Tangent,float Offset)
 {
  const FVector Right=FVector::CrossProduct(FVector::UpVector,Tangent.GetSafeNormal2D());
  const FVector Lane=P+Right*Offset;
  if(Route.IsEmpty() || !Route.Last().Equals(Lane,1)){Route.Add(Lane);if(PointSpeedLimits)PointSpeedLimits->Add(CurrentSpeedLimit);}
  else if(PointSpeedLimits && !PointSpeedLimits->IsEmpty())PointSpeedLimits->Last()=FMath::Min(PointSpeedLimits->Last(),CurrentSpeedLimit);
 };
 auto LaneOffsetAt=[&](int32 ChainIndex,float Distance)
 {
  const auto& Arc=Arcs[Chain[ChainIndex]];const float Offset=Roads[Arc.Road].LaneOffsetCm;
  const int32 Previous=ChainIndex>0?ChainIndex-1:(ReturnToStart?Chain.Num()-1:INDEX_NONE);
  const int32 Next=ChainIndex+1<Chain.Num()?ChainIndex+1:(ReturnToStart?0:INDEX_NONE);
  const float BlendLength=FMath::Max(600.f,FMath::Abs(Offset)*8.f);float Weight=1;
  auto NeedsJoinBlend=[&](int32 Before,int32 After)
  {
   const auto& Incoming=Arcs[Chain[Before]];const auto& Outgoing=Arcs[Chain[After]];
   const FVector In=(Incoming.P.Last()-Incoming.P[Incoming.P.Num()-2]).GetSafeNormal2D();
   const FVector Out=(Outgoing.P[1]-Outgoing.P[0]).GetSafeNormal2D();
   // Equal lane widths still produce a backward diagonal connector at a
   // perpendicular junction. Ease both offset lanes to their shared port.
   return !FMath::IsNearlyEqual(Roads[Incoming.Road].LaneOffsetCm,Roads[Outgoing.Road].LaneOffsetCm)
       || FVector::DotProduct(In,Out)<.98f;
  };
  if(Previous!=INDEX_NONE && NeedsJoinBlend(Previous,ChainIndex))
   Weight=FMath::Min(Weight,FMath::SmoothStep(0.f,BlendLength,Distance));
  if(Next!=INDEX_NONE && NeedsJoinBlend(ChainIndex,Next))
   Weight=FMath::Min(Weight,FMath::SmoothStep(0.f,BlendLength,Arc.Length-Distance));
  return Offset*Weight;
 };
 for(int32 C=0;C<Chain.Num();++C)
 {
  const auto& A=Arcs[Chain[C]];const auto& R=Roads[A.Road];CurrentSpeedLimit=FMath::Clamp(R.SpeedLimitKmh,5.f,130.f);SpeedLimitKmh=FMath::Max(SpeedLimitKmh,CurrentSpeedLimit);
  // A lane-offset change must not append a sideways step at a shared road
  // port. That step rotated the full-chassis safety sweep into the garage
  // wall and could leave a slow car permanently braking before the door.
  // Ease the lane to the authored centreline on both sides of such a join;
  // retain the original elevation, road samples and obstruction checks.
  TArray<float> Along;Along.Add(0);
  for(int32 J=1;J<A.P.Num();++J)Along.Add(Along.Last()+FVector::Distance(A.P[J-1],A.P[J]));
  auto AppendLane=[&](const FVector& P,const FVector& Tangent,float Distance)
  {
   Add(P,Tangent,LaneOffsetAt(C,Distance));
  };
  const int32 First=C==0?S.Segment:0;const int32 Last=Goal && C==Chain.Num()-1?End.Segment:A.P.Num()-2;
  if(C==0)AppendLane(S.Point,A.P[First+1]-A.P[First],Along[First]+FVector::Distance(A.P[First],S.Point));
  else AppendLane(A.P[0],A.P[1]-A.P[0],0);
  if(Goal && C==Chain.Num()-1 && A.WholeLoop && (End.Segment<First || (End.Segment==First && End.Alpha<S.Alpha)))
  {
   for(int32 J=First+1;J<A.P.Num()-1;++J)AppendLane(A.P[J],A.P[FMath::Min(J+1,A.P.Num()-1)]-A.P[J-1],Along[J]);
   for(int32 J=0;J<=End.Segment;++J)AppendLane(A.P[J],A.P[J+1]-A.P[J],Along[J]);
  }
  else for(int32 J=First+1;J<=Last;++J)AppendLane(A.P[J],A.P[J+1]-A.P[J-1],Along[J]);
  if(Goal && C==Chain.Num()-1)AppendLane(End.Point,A.P[End.Segment+1]-A.P[End.Segment],Along[End.Segment]+FVector::Distance(A.P[End.Segment],End.Point));
  else if(!Goal && C==0 && A.WholeLoop)
  {
   for(int32 J=0;J<=First;++J)AppendLane(A.P[J],A.P[J+1]-A.P[J],Along[J]);
   AppendLane(S.Point,A.P[First+1]-A.P[First],Along[First]+FVector::Distance(A.P[First],S.Point));Looped=true;
  }
  else AppendLane(A.P.Last(),A.P.Last()-A.P[A.P.Num()-2],Along.Last());
 }
 if(ReturnToStart)
 {
  const auto& A=Arcs[Start];const auto& R=Roads[A.Road];CurrentSpeedLimit=FMath::Clamp(R.SpeedLimitKmh,5.f,130.f);
  float Along=0;
  for(int32 J=0;J<=S.Segment;++J)
  {if(J>0)Along+=FVector::Distance(A.P[J-1],A.P[J]);Add(A.P[J],A.P[J+1]-A.P[FMath::Max(0,J-1)],LaneOffsetAt(0,Along));}
  Add(S.Point,A.P[S.Segment+1]-A.P[S.Segment],LaneOffsetAt(0,Along+FVector::Distance(A.P[S.Segment],S.Point)));Looped=true;
 }
 return Route.Num()>=2;
}
