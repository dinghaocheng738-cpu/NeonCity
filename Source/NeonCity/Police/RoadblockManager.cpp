#include "Police/RoadblockManager.h"
#include "Police/PoliceObservation.h"
#include "Wanted/SearchSystem.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "AI/Police/PoliceVehicle.h"
#include "AI/Police/PoliceCharacter.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
bool URoadblockManager::TryDeploy(ANeonCityTrafficRoute* Road,int32 Maximum)
{
 auto* Search=GetWorld()->GetSubsystem<USearchSystem>();
 if(!IsValid(Road) || Cars.Num()>=FMath::Clamp(Maximum,0,2) || !Search->GetDescriptor().IsValid()) return false;
 auto* S=Road->GetRoadSpline(); const float Length=S->GetSplineLength(); const FVector Known=Search->GetLastKnownLocation();
 if(Length<1000) return false;
 const float Base=S->GetDistanceAlongSplineAtLocation(Known,ESplineCoordinateSpace::World);
 for(float Ahead:{1800.f,2600.f,3400.f,4500.f,6000.f})
 {
  const float D=FMath::Fmod(Base+Ahead,Length); const FVector P=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
  if(FMath::Abs(P.Z-Known.Z)>500 || FVector::Dist2D(P,Known)<1500 || FVector::DotProduct((P-Known).GetSafeNormal2D(),Search->GetDescriptor().LastSeenDirection)<-.25f || !NeonCityObservation::SafeDeployment(GetWorld(),P,2500)) continue;
  if(Positions.ContainsByPredicate([&](FVector V){return FVector::Dist2D(P,V)<1500;})) continue;
  const FRotator R(0,S->GetRotationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World).Yaw+90,0);
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
  auto* Car=GetWorld()->SpawnActor<APoliceVehicle>(P,R,Params); if(!Car) continue;
  Car->SetRoadblock(true); Car->SetSiren(true); Cars.Add(Car); Positions.Add(P); FixtureStarts.Add(Fixtures.Num());
  const FVector Along=S->GetDirectionAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
  // Block only one lane. The opposite lane and turn-around remain physically usable.
  for(int32 I=0;I<2;++I)
  {
   const FTransform T(R,FVector(P+Along*(I?250:-250))+FVector(0,0,-43));
   auto* Barrier=GetWorld()->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),T);
   if(Barrier) { Barrier->Configure(ECityPhysicsProp::RoadBarrier); Barrier->FinishSpawning(T); Fixtures.Add(Barrier); }
  }
  auto* Officer=GetWorld()->SpawnActor<APoliceCharacter>(P+Along*480+FVector(0,0,15),R,Params);
  if(Officer) { Officer->SetDispatchSlot(7+Cars.Num()); Fixtures.Add(Officer); }
  return true;
 }
 return false;
}
void URoadblockManager::Reset()
{ TrimTo(0); }
void URoadblockManager::TrimTo(int32 Maximum)
{
 while(Cars.Num()>FMath::Max(0,Maximum))
 {
  const int32 Start=FixtureStarts.Pop();
  for(int32 I=Fixtures.Num()-1;I>=Start;--I) { if(IsValid(Fixtures[I])) Fixtures[I]->Destroy(); Fixtures.RemoveAt(I); }
  if(auto* Car=Cars.Pop().Get(); IsValid(Car)) Car->Destroy(); Positions.Pop();
 }
}
