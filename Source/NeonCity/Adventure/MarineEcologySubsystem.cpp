#include "Adventure/MarineEcologySubsystem.h"
#include "Adventure/CoastalWildlife.h"
#include "Adventure/MarineSchool.h"
#include "Engine/World.h"

namespace { constexpr float CellSize=450;
FIntVector Cell(const FVector& P) { return FIntVector(FMath::FloorToInt(P.X/CellSize),FMath::FloorToInt(P.Y/CellSize),FMath::FloorToInt(P.Z/CellSize)); }
}
void UMarineEcologySubsystem::RegisterFish(ACoastalWildlife* Fish) { Members.AddUnique(Fish);NextSnapshot=-1; }
void UMarineEcologySubsystem::UnregisterFish(ACoastalWildlife* Fish) { Members.Remove(Fish);NextSnapshot=-1; }
void UMarineEcologySubsystem::RegisterSchool(AMarineSchool* School) { Schools.AddUnique(School); }
void UMarineEcologySubsystem::UnregisterSchool(AMarineSchool* School) { Schools.Remove(School); }
bool UMarineEcologySubsystem::NearestCreaturePosition(const FVector& P,float Range,FVector& Result)
{
 bool Found=false;float Distance=Range*Range;
 if(const auto* Fish=NearestFish(P,Range))
 {Result=Fish->GetActorLocation();Distance=FVector::DistSquared(P,Result);Found=true;}
 for(const auto& S:Schools)if(S.IsValid() && FVector::DistSquared(P,S->GetActorLocation())<FMath::Square(Range+S->HomeRadius))
  Found=S->NearestCreature(P,Distance,Result)||Found;
 return Found;
}
ACoastalWildlife* UMarineEcologySubsystem::NearestFish(const FVector& Position,float Range)
{
 RefreshSnapshot();ACoastalWildlife* Best=nullptr;float Distance=Range*Range;
 for(const auto& N:Snapshot)if(N.Fish.IsValid())
 {
  const float D=FVector::DistSquared(N.Position,Position);
  if(D<Distance){Distance=D;Best=N.Fish.Get();}
 }
 return Best;
}
void UMarineEcologySubsystem::RefreshSnapshot()
{
 if(GetWorld()->GetTimeSeconds()<NextSnapshot)return;
 NextSnapshot=GetWorld()->GetTimeSeconds()+.1;Snapshot.Reset();Cells.Reset();
 Members.RemoveAllSwap([](const auto& P){return !P.IsValid();});
 for(const auto& P:Members)if(P->GetSpearLife()==EMarineSpearLife::Alive)
 {
  const int32 I=Snapshot.Add({P,P->GetActorLocation(),P->GetSwimHeading(),P->SchoolID});
  Cells.FindOrAdd(Cell(Snapshot[I].Position)).Add(I);
 }
}
FVector UMarineEcologySubsystem::SchoolDirection(const ACoastalWildlife* Fish,const FVector& P,const FVector& Heading)
{
 RefreshSnapshot();FVector Separation=FVector::ZeroVector,Alignment=FVector::ZeroVector,Cohesion=FVector::ZeroVector;
 int32 Count=0,Inspected=0;const FIntVector C=Cell(P);
 for(int32 Z=-1;Z<=1 && Inspected<32;++Z)for(int32 Y=-1;Y<=1 && Inspected<32;++Y)for(int32 X=-1;X<=1 && Inspected<32;++X)
 {
  const auto* Bucket=Cells.Find(C+FIntVector(X,Y,Z));if(!Bucket)continue;
  for(int32 I:*Bucket)
  {
   if(++Inspected>32)break;
   const auto& N=Snapshot[I];if(N.Fish.Get()==Fish || !N.Fish.IsValid())continue;
   const FVector Away=P-N.Position;const float D2=Away.SizeSquared();
   if(D2<1 || D2>FMath::Square(CellSize))continue;
   if(D2<FMath::Square(Fish->SchoolSpacing))Separation+=Away/FMath::Max(D2,25.f)*Fish->SchoolSpacing;
   if(N.School==Fish->SchoolID && !Fish->SchoolID.IsNone()) { Alignment+=N.Heading;Cohesion+=N.Position;++Count; }
   // Bound work even when an authored school is accidentally overcrowded.
   if(Inspected>=32)break;
  }
  if(Inspected>=32)break;
 }
 FVector Steering=Separation*2.4f;
 if(Count)Steering+=(Alignment/Count-Heading)*.7f+(Cohesion/Count-P)/CellSize*.45f;
 return Steering;
}
