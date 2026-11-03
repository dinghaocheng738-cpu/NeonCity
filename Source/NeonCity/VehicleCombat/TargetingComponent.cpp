#include "VehicleCombat/TargetingComponent.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Combat/DamageInterface.h"
#include "Vehicles/VehicleInterface.h"
#include "Combat/CombatCollision.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
UTargetingComponent::UTargetingComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool UTargetingComponent::HasLineOfSight(UWorld* World,AActor* Observer,AActor* Target,FVector Origin)
{
 if(!World || !IsValid(Target) || Origin.ContainsNaN()) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(VehicleWeaponLOS),false,Observer); FHitResult Hit;
 // the observer's own weapon mounts (separate attached actors) never block its sight
 if(Observer)for(AActor* Child:Observer->Children)Q.AddIgnoredActor(Child);
 if(Observer)if(AActor* Mount=Observer->GetOwner())Q.AddIgnoredActor(Mount);
 // a sight line that reaches the target's immediate surroundings (the ground it stands on) is a clear one
 return !World->LineTraceSingleByChannel(Hit,Origin,Target->GetActorLocation(),NeonCityCombat::WeaponTrace,Q) || Hit.GetActor()==Target
  || FVector::DistSquared(Hit.ImpactPoint,Target->GetActorLocation())<FMath::Square(250.f);
}
FVehicleTargetInfo UTargetingComponent::Inspect(AActor* Target,FVector Origin,FVector Direction,float Range,float Angle)
{
 FVehicleTargetInfo Info;
 if(!IsValid(Target) || Target==GetOwner() || Target->GetWorld()!=GetWorld() || !Target->Implements<UDamageInterface>()
  || !UCombatTargetComponent::IsAliveTarget(Target) || Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero()
  || !FMath::IsFinite(Range) || !FMath::IsFinite(Angle)) return Info;
 const FVector Delta=Target->GetActorLocation()-Origin; Info.Distance=Delta.Size(); Info.Direction=Delta.GetSafeNormal();
 if(Info.Distance>FMath::Clamp(Range,1.f,50000.f) || FVector::DotProduct(Direction.GetSafeNormal(),Info.Direction)<FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Angle,1.f,90.f)))) return Info;
 Info.Actor=Target; Info.Visible=HasLineOfSight(GetWorld(),GetOwner(),Target,Origin);
 if(Target->Implements<UVehicleInterface>()) Info.TargetID=IVehicleInterface::Execute_GetVehicleId(Target);
 if(Info.TargetID.IsNone())
 {
  if(RuntimeIDs.Num()>=256) for(auto It=RuntimeIDs.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
  auto* ID=RuntimeIDs.Find(Target);
  if(!ID && RuntimeIDs.Num()<256) ID=&RuntimeIDs.Add(Target,FGuid::NewGuid());
  if(ID) Info.TargetID=FName(*ID->ToString());
 }
 return Info;
}
FVehicleTargetInfo UTargetingComponent::FindTarget(FVector Origin,FVector Direction,float Range,float Angle)
{
 FVehicleTargetInfo Best; float BestDot=-1;
 if(Origin.ContainsNaN() || Direction.ContainsNaN() || !FMath::IsFinite(Range)) return Best;
 TArray<FOverlapResult> Results; FCollisionQueryParams Q(SCENE_QUERY_STAT(VehicleTargets),false,GetOwner());
 GetWorld()->OverlapMultiByObjectType(Results,Origin,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeSphere(FMath::Clamp(Range,1.f,50000.f)),Q);
 TSet<AActor*> Seen;
 for(const auto& Result:Results)
 {
  AActor* A=Result.GetActor(); if(!A || Seen.Contains(A)) continue; Seen.Add(A);
  auto Info=Inspect(A,Origin,Direction,Range,Angle); if(!Info.Visible) continue;
  const float Dot=FVector::DotProduct(Direction.GetSafeNormal(),Info.Direction);
  if(Dot>BestDot) { Best=Info; BestDot=Dot; }
 }
 return Best;
}
