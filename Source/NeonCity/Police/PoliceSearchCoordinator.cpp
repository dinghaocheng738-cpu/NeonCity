#include "Police/PoliceSearchCoordinator.h"
#include "Wanted/SearchSystem.h"
#include "Engine/World.h"
FVector UPoliceSearchCoordinator::GetAssignment(int32 Slot,float Radius,EPoliceSearchPattern Pattern) const
{
 const auto* Search=GetWorld()->GetSubsystem<USearchSystem>(); const FVector Known=Search->GetLastKnownLocation();
 if(Pattern==EPoliceSearchPattern::InvestigatePoint && Slot==0) return Known;
 const float Scale=Pattern==EPoliceSearchPattern::Perimeter?1.f:Pattern==EPoliceSearchPattern::SweepRoute?.7f:.45f;
 const float Angle=FMath::DegreesToRadians((Slot*137.5f)+int32(GetWorld()->GetTimeSeconds()/6)*35.f);
 for(int32 Attempt=0;Attempt<4;++Attempt)
 {
  const FVector Point=Known+FVector(FMath::Cos(Angle+Attempt*.5f),FMath::Sin(Angle+Attempt*.5f),0)*Search->GetDynamicSearchRadius(Radius)*Scale;
  FHitResult Ground; FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceSearchGround));
  if(GetWorld()->LineTraceSingleByChannel(Ground,Point+FVector(0,0,250),Point-FVector(0,0,500),ECC_Visibility,Q) && Ground.ImpactNormal.Z>.65f && FMath::Abs(Ground.ImpactPoint.Z-Known.Z)<200)
  {
   const FVector Goal=Ground.ImpactPoint+FVector(0,0,92);
   if(!GetWorld()->OverlapBlockingTestByChannel(Goal,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(35,88),Q)) return Goal;
  }
 }
 // A blocked assignment remains an evidence-derived waiting point; no teleport to a hidden player.
 return Known+FVector(0,(Slot%2?1:-1)*(80+Slot*55),0);
}
