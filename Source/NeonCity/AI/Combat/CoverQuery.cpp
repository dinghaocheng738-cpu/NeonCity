#include "AI/Combat/CoverQuery.h"
#include "AI/Combat/CoverPoint.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
ACoverPoint::ACoverPoint() { PrimaryActorTick.bCanEverTick=false; SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"))); }
bool ACoverPoint::IsAvailableFor(const AActor* User) const
{ return !Occupant.IsValid() || Occupant.Get()==User || !UCombatTargetComponent::IsAliveTarget(Occupant.Get()); }
bool NeonCityCover::ClearSight(UWorld* World,AActor* Observer,FVector Start,FVector End,AActor* Target)
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CombatSight),false,Observer); FHitResult Hit;
 return !World->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q) || (Target && Hit.GetActor()==Target);
}
bool NeonCityCover::SafeSegment(ACharacter* User,FVector Start,FVector End)
{
 if(!IsValid(User) || Start.ContainsNaN() || End.ContainsNaN() || FVector::Dist(Start,End)>2500) return false;
 auto* World=User->GetWorld(); const auto* Capsule=User->GetCapsuleComponent();
 const float Half=User->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 // Planning uses the standing capsule even when leaving crouched cover; movement performs the actual uncrouch.
 if(User->bIsCrouched && Start.Equals(User->GetActorLocation(),.1f)) Start.Z+=Half-Capsule->GetScaledCapsuleHalfHeight();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CombatPath),false,User); FHitResult Hit;
 const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Half-3);
 if(World->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,Shape,Q)
  || World->OverlapBlockingTestByChannel(End,FQuat::Identity,ECC_Pawn,Shape,Q)) return false;
 const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,End)/90));
 for(int32 I=1;I<=Steps;++I)
 {
  const FVector Sample=FMath::Lerp(Start,End,float(I)/Steps);
  if(!World->LineTraceSingleByChannel(Hit,Sample,Sample-FVector(0,0,Half+30),ECC_Visibility,Q)
   || Hit.ImpactNormal.Z<User->GetCharacterMovement()->GetWalkableFloorZ() || FMath::Abs(Hit.ImpactPoint.Z+Half-Sample.Z)>20) return false;
 }
 return true;
}
bool ACoverPoint::ProvidesCover(ACharacter* User,FVector ThreatAim) const
{
 if(!IsValid(User) || ThreatAim.ContainsNaN() || FVector::Dist2D(ThreatAim,GetActorLocation())<220
  || FVector::DotProduct(GetActorForwardVector(),(ThreatAim-GetActorLocation()).GetSafeNormal2D())<.2f) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CoverProtection),false,User); FHitResult Hit;
 const FVector Hidden=GetActorLocation()+FVector(0,0,Stance==ENeonCityCoverStance::Standing?60:0);
 if(!GetWorld()->LineTraceSingleByChannel(Hit,ThreatAim,Hidden,ECC_Visibility,Q) || !Hit.GetActor()) return false;
 // Another pawn is not stable cover; a wall between anchor and observer must actually occlude it.
 if(Cast<APawn>(Hit.GetActor()) || FVector::Dist(Hit.ImpactPoint,Hidden)>300) return false;
 return NeonCityCover::SafeSegment(User,GetActorLocation(),GetActorLocation());
}
bool ACoverPoint::Reserve(ACharacter* User,FVector ThreatAim)
{ if(!IsAvailableFor(User) || !ProvidesCover(User,ThreatAim)) return false; Occupant=User; return true; }
void ACoverPoint::Release(const AActor* User) { if(Occupant.Get()==User) Occupant.Reset(); }
bool ACoverPoint::FindPeek(ACharacter* User,FVector ThreatAim,FVector& OutPoint) const
{
 for(float Sign:{1.f,-1.f})
 {
  const FVector Point=GetActorLocation()+GetActorRightVector()*FMath::Clamp(PeekDistance,100.f,300.f)*Sign;
  if(NeonCityCover::SafeSegment(User,GetActorLocation(),Point)
   && NeonCityCover::ClearSight(GetWorld(),User,Point+FVector(0,0,35),ThreatAim)) { OutPoint=Point; return true; }
 }
 return false;
}
ACoverPoint* NeonCityCover::FindAndReserve(ACharacter* User,FVector ThreatAim,float Radius,ACoverPoint* Exclude)
{
 ACoverPoint* Best=nullptr; float Score=FMath::Square(Radius); int32 Examined=0;
 for(TActorIterator<ACoverPoint> It(User->GetWorld());It && Examined<64;++It,++Examined)
 {
  const float Travel=FVector::DistSquared(User->GetActorLocation(),It->GetActorLocation());
  if(*It==Exclude || Travel>=Score || !It->IsAvailableFor(User) || !It->ProvidesCover(User,ThreatAim)) continue;
  if(!SafeSegment(User,User->GetActorLocation(),It->GetActorLocation())) continue;
  FVector Peek; if(!It->FindPeek(User,ThreatAim,Peek)) continue;
  Best=*It; Score=Travel;
 }
 return Best && Best->Reserve(User,ThreatAim)?Best:nullptr;
}
