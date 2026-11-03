#include "Adventure/WingsuitLaunchPoint.h"
#include "Adventure/WingsuitComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
AWingsuitLaunchPoint::AWingsuitLaunchPoint()
{
 SafetyGate=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoofSafetyGate"));SafetyGate->SetupAttachment(RootComponent);
 SafetyGate->SetMobility(EComponentMobility::Movable);SafetyGate->SetCanEverAffectNavigation(false);
 SafetyGate->SetRelativeLocation(FVector(-110,-240,0));SafetyGate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 SafetyGate->SetVisibility(false);
}
void AWingsuitLaunchPoint::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 SafetyGate->SetVisibility(bRoofSafetyGate);
 SafetyGate->SetCollisionProfileName(bRoofSafetyGate?TEXT("BlockAll"):TEXT("NoCollision"));
 SafetyGate->SetRelativeRotation(FRotator::ZeroRotator);
}
void AWingsuitLaunchPoint::BeginPlay()
{
 Super::BeginPlay();SetActorTickEnabled(bRoofSafetyGate);PrimaryActorTick.TickInterval=.02f;
 // Session interaction must be explicitly rearmed after a load. No open gate
 // state can expose a restored ordinary pedestrian to the launch edge.
 ArmedVisitor.Reset();GateFraction=0;
 SafetyGate->SetRelativeRotation(FRotator::ZeroRotator);
}
void AWingsuitLaunchPoint::Tick(float Delta)
{
 Super::Tick(Delta);if(!bRoofSafetyGate)return;
 const auto* Suit=ArmedVisitor.IsValid()?ArmedVisitor->FindComponentByClass<UWingsuitComponent>():nullptr;
 const bool Armed=Suit && Suit->IsEquipped() && !Suit->IsFlying() && FVector::DistSquared(ArmedVisitor->GetActorLocation(),GetActorLocation())<FMath::Square(1000.);
 if(!Armed)ArmedVisitor.Reset();
 if(!Armed && GateFraction>0)
 {
  // Do not close through a pedestrian at the physical edge. The same geometry
  // still blocks ordinary access before the explicit equip interaction.
  TArray<FOverlapResult> Overlaps;FCollisionQueryParams Params;Params.AddIgnoredActor(this);
  const FVector Centre=GetActorTransform().TransformPosition(SafetyGate->GetRelativeLocation()+FVector(110,0,55));
  if(GetWorld()->OverlapMultiByObjectType(Overlaps,Centre,GetActorQuat(),FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeBox(FVector(135,55,65)),Params))return;
 }
 GateFraction=FMath::FInterpConstantTo(GateFraction,Armed?1.f:0.f,Delta,1.25f);
 SafetyGate->SetRelativeRotation(FRotator(0,-95*GateFraction,0));
}
bool AWingsuitLaunchPoint::CanInteract_Implementation(AActor* V)const{return V && FVector::Dist(V->GetActorLocation(),GetActorLocation())<260 && V->FindComponentByClass<UWingsuitComponent>();}
FText AWingsuitLaunchPoint::GetInteractionName_Implementation()const{return NSLOCTEXT("NeonCity","WingsuitEquip","Fit wingsuit, helmet and reserve canopy");}
void AWingsuitLaunchPoint::Interact_Implementation(AActor* V)
{
 if(!CanInteract_Implementation(V))return;
 const auto* Character=Cast<ACharacter>(V);
 if(bRoofSafetyGate && (!Character || !Character->GetCharacterMovement()->IsMovingOnGround()))return;
 auto* Suit=V->FindComponentByClass<UWingsuitComponent>();
 if(Suit->Equip() && bRoofSafetyGate)ArmedVisitor=V;
}
