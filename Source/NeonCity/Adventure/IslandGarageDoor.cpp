#include "Adventure/IslandGarageDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
AIslandGarageDoor::AIslandGarageDoor()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.TickGroup=TG_PrePhysics;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GarageTrackOrigin")));
 for(int32 I=0;I<10;++I)
 {auto* P=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("GarageLeaf%d"),I));P->SetupAttachment(RootComponent);P->SetMobility(EComponentMobility::Movable);P->SetCollisionProfileName(TEXT("BlockAll"));P->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);P->SetGenerateOverlapEvents(false);P->SetCanEverAffectNavigation(false);Panels.Add(P);}
}
void AIslandGarageDoor::PanelPose(int32 Index,float Fraction,FVector& Location,FRotator& Rotation)const
{
 const float Radius=FMath::Max(20.f,TrackRadius),H=FMath::Max(200.f,Height);
 const float Arc=Radius*HALF_PI,Vertical=H-Radius;
 const float Distance=(Index+.5f)*(H/10.f)+FMath::Clamp(Fraction,0.f,1.f)*(H+Arc-Radius);
 float Y=0,Z=Distance,Angle=0;
 if(Distance>Vertical && Distance<Vertical+Arc){const float Theta=(Distance-Vertical)/Radius;Y=Radius*(1-FMath::Cos(Theta));Z=Vertical+Radius*FMath::Sin(Theta);Angle=FMath::RadiansToDegrees(Theta);}
 else if(Distance>=Vertical+Arc){Y=Radius+Distance-Vertical-Arc;Z=H;Angle=90;}
 // Roll tilts the vertical leaf's Z axis toward local +Y; X remains the full
 // entrance width. No leaf disappears or crosses the upstairs floor slab.
 Location=FVector(0,Y,Z);Rotation=FRotator(0,0,Angle);
}
void AIslandGarageDoor::Present(){for(int32 I=0;I<Panels.Num();++I){FVector P;FRotator R;PanelPose(I,OpenFraction,P,R);Panels[I]->SetRelativeLocationAndRotation(P,R);}}
void AIslandGarageDoor::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);Present();}
void AIslandGarageDoor::BeginPlay(){Super::BeginPlay();OpenFraction=1;bTargetOpen=true;bObstructed=false;Present();}
void AIslandGarageDoor::Toggle(){if(!HasAuthority())return;bTargetOpen=!bTargetOpen;bObstructed=false;SetActorTickEnabled(true);}
namespace{FString BlockerName(const AActor* A)
{
#if WITH_EDITOR
 return A?A->GetActorLabel():TEXT("none");
#else
 return GetNameSafe(A);
#endif
}}
bool AIslandGarageDoor::CanMoveTo(float Fraction)const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(GarageArticulatedLeaf),false,this);
 for(int32 I=0;I<Panels.Num();++I)
 {
  FVector P;FRotator R;PanelPose(I,Fraction,P,R);const FTransform T=FTransform(R,P)*GetActorTransform();
  const FVector Half(FMath::Max(200.f,Width)*.5f-.2f,2.8f,Height/20.f-.2f);
  // A sweep guards translation; an oriented end query guards the changing
  // orientation. Tiny endpoint inset avoids floor-contact false positives.
  FHitResult Hit;if(GetWorld()->SweepSingleByChannel(Hit,Panels[I]->GetComponentLocation(),T.GetLocation(),T.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(Half),Q))
  {LastBlocker=FString::Printf(TEXT("leaf %d at %.2f: %s / %s"),I,Fraction,*BlockerName(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));return false;}
  TArray<FOverlapResult> Over;
  if(GetWorld()->OverlapMultiByChannel(Over,T.GetLocation(),T.GetRotation(),ECC_Pawn,FCollisionShape::MakeBox(Half),Q))
   for(const FOverlapResult& O:Over)if(O.bBlockingHit)
   {LastBlocker=FString::Printf(TEXT("leaf %d at %.2f: %s / %s"),I,Fraction,*BlockerName(O.GetActor()),*GetNameSafe(O.GetComponent()));return false;}
 }
 return true;
}
void AIslandGarageDoor::Tick(float Delta)
{
 Super::Tick(Delta);const float Next=FMath::FInterpConstantTo(OpenFraction,bTargetOpen?1.f:0.f,FMath::Clamp(Delta,0.f,.05f),1/FMath::Max(2.f,TravelSeconds));
 if(!CanMoveTo(Next)){bObstructed=true;if(!bTargetOpen)bTargetOpen=true;return;}
 OpenFraction=Next;Present();if(FMath::IsNearlyEqual(OpenFraction,bTargetOpen?1.f:0.f,.001f))SetActorTickEnabled(false);
}
