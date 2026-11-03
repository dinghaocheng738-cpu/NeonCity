#include "Water/WaterInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Water/CoastalOceanSurface.h"
#include "EngineUtils.h"
#include "Engine/World.h"
UWaterInteractionComponent::UWaterInteractionComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics; PrimaryComponentTick.TickInterval=.05f; }
void UWaterInteractionComponent::BeginPlay()
{
 Super::BeginPlay();
 // Sample the newly published water before buoyancy and character movement.
 for(TActorIterator<ACoastalOceanSurface> It(GetWorld());It;++It)AddTickPrerequisiteActor(*It);
 Refresh();
}
void UWaterInteractionComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{ Super::TickComponent(Delta,Type,Function); Refresh(); }
void UWaterInteractionComponent::Refresh()
{
 auto* Character=Cast<ACharacter>(GetOwner()); auto Next=EWaterInteractionState::Dry; bShallowFloor=false;bSeafloorSupport=false;
 if(Character && !Character->IsHidden() && Character->GetActorEnableCollision())
 {
  const FVector Center=Character->GetActorLocation(); const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Center,Sample) && Sample.DepthAt(Center-FVector(0,0,Half))>0)
  {
   FFindFloorResult Floor;Character->GetCharacterMovement()->K2_FindFloor(Center,Floor);
   const bool Support=Floor.IsWalkableFloor();
   bShallowFloor=Support && Sample.DepthAt(Floor.HitResult.ImpactPoint)<Half*1.15f;
   // Use the same capsule sweep and slope rules as actual walking. A centre
   // ray accepted ledges with no usable footing and snapped a diver down 18cm.
   bSeafloorSupport=Support && !bShallowFloor && Floor.GetDistanceToFloor()<=6
    && Floor.HitResult.GetComponent() && !Floor.HitResult.GetComponent()->IsSimulatingPhysics();
   BreathingLocation=Center+FVector(0,0,Half);
   if(const auto* Mesh=Character->GetMesh(); Mesh && Mesh->DoesSocketExist(TEXT("head")))
   {
    const FVector Head=Mesh->GetSocketLocation(TEXT("head"))+FVector(0,0,8);
    // Ignore stale post-teleport poses until the skeleton catches up. The head
    // point, rather than the upright capsule top, follows the real swimming pose.
    if(FVector::DistSquared(Head,Center)<FMath::Square(180.f)) BreathingLocation=Head;
   }
   const float HeadDepth=Sample.DepthAt(BreathingLocation);
   const double Now=GetWorld()->GetTimeSeconds();
   const bool Reset=LastHeadSample<0 || FVector::DistSquared(Center,PreviousCenter)>FMath::Square(200.f);
   FilteredHeadDepth=Reset?HeadDepth:FMath::FInterpTo(FilteredHeadDepth,HeadDepth,FMath::Clamp(float(Now-LastHeadSample),0.f,.2f),10.f);
   LastHeadSample=Now; PreviousCenter=Center;
   // A short filter plus 11cm hysteresis prevents strokes/breaths flickering
   // oxygen and police visibility. It does not pause underwater oxygen loss.
   const bool Under=FilteredHeadDepth>(State==EWaterInteractionState::Underwater?-5.f:6.f);
   Next=Under?EWaterInteractionState::Underwater:bShallowFloor || Sample.DepthAt(Center)<0?EWaterInteractionState::Shallow:EWaterInteractionState::Surface;
  }
 }
 if(State!=Next) { const auto Before=State; State=Next; OnWaterStateChanged.Broadcast(Before,State); }
}
void UWaterInteractionComponent::ResetForLoad() { State=EWaterInteractionState::Dry; Sample=FWaterSurfaceSample(); bShallowFloor=false; LastHeadSample=-1; Refresh(); }
bool UWaterInteractionComponent::IsSubmergedActor(const AActor* Actor)
{
 if(!IsValid(Actor)) return false;
 if(const auto* Water=Actor->FindComponentByClass<UWaterInteractionComponent>()) return Water->IsFullySubmerged();
 return false;
}
