#include "Adventure/IslandBird.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
AIslandBird::AIslandBird()
{
 PrimaryActorTick.bCanEverTick=true;
 Bird=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Bird"));SetRootComponent(Bird);
 Bird->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bird->SetCanEverAffectNavigation(false);
 Bird->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
 Bird->bEnableUpdateRateOptimizations=true;
}
FVector AIslandBird::Orbit(float A)const
{
 return Hub+FVector(FMath::Cos(A)*OrbitRadius,FMath::Sin(A)*OrbitRadius,FlightHeight+FMath::Sin(A*2+Phase)*70);
}
void AIslandBird::ChangeState(EIslandBirdState Next)
{
 State=Next;Clock=0;TransitionStart=GetActorLocation();
}
void AIslandBird::BeginPlay()
{
 Super::BeginPlay();Perch=GetActorLocation();LastPosition=Perch;TransitionStart=Perch;Bird->SetVisibility(false);
 if(auto* Mesh=Bird->GetSkeletalMeshAsset()){const auto Bounds=Mesh->GetBounds();FootOffset=FMath::Max(.2f,float(Bounds.BoxExtent.Z-Bounds.Origin.Z));}
 OrbitRadius=FMath::Clamp(OrbitRadius,600.f,6000.f);FlightHeight=FMath::Clamp(FlightHeight,400.f,3000.f);
 FlightSpeed=FMath::Clamp(FlightSpeed,200.f,1200.f);
 Hub=Perch-FVector(OrbitRadius,0,0);Angle=0;Clock=-FMath::Fmod(Phase,8.f);
 Bird->SetAnimationMode(EAnimationMode::AnimationSingleNode);if(Flight)Bird->PlayAnimation(Flight,true);
 Bird->SetMorphTarget(TEXT("Folded"),1);Bird->SetPlayRate(0);
}
void AIslandBird::Tick(float Delta)
{
 Super::Tick(Delta);if(!FMath::IsFinite(Delta))return;Delta=FMath::Min(Delta,.05f);
 // Wait for streamed collision. A support trace prevents indoor/underwater authored errors.
 if(!bHabitatReady)
 {
  if(auto* PC=GetWorld()->GetFirstPlayerController();PC && PC->PlayerCameraManager && FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(),Perch)<FMath::Square(350.f))return;
  FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdPerch),false,this);
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Perch+FVector(0,0,80),Perch-FVector(0,0,200),ECC_WorldStatic,Q))return;
  FWaterSurfaceSample Water;if(auto* Marine=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();Marine && Marine->QueryWater(Hit.ImpactPoint,Water) && Hit.ImpactPoint.Z<Water.SurfacePoint.Z+10)return;
  FHitResult Roof;if(GetWorld()->LineTraceSingleByChannel(Roof,Hit.ImpactPoint+FVector(0,0,100),Hit.ImpactPoint+FVector(0,0,FlightHeight+200),ECC_WorldStatic,Q))return;
  Perch.Z=Hit.ImpactPoint.Z+FootOffset;Hub.Z=Perch.Z;SetActorLocation(Perch);LastPosition=Perch;bHabitatReady=true;Bird->SetVisibility(true);
 }
 Clock+=Delta;FVector Desired=GetActorLocation();float WantedFold=0;
 auto* PC=GetWorld()->GetFirstPlayerController();const FVector Viewer=PC && PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraLocation():FVector(1e9);
 const bool Disturbed=FVector::DistSquared(Viewer,Perch)<FMath::Square(350.f);
 if(State==EIslandBirdState::Perched)
 {
  WantedFold=1;Desired=Perch;
  if(Clock>RestSeconds || Disturbed)ChangeState(EIslandBirdState::Takeoff);
 }
 else if(State==EIslandBirdState::Takeoff)
 {
  const float T=FMath::Clamp(Clock/4.f,0.f,1.f),S=T*T*(3-2*T);
  Desired=FMath::Lerp(TransitionStart,Orbit(0),S);Desired.Y+=FMath::Sin(T*PI)*180;
  if(T>=1){Angle=0;ChangeState(EIslandBirdState::Cruise);}
 }
 else if(State==EIslandBirdState::Cruise)
 {
  Angle+=Delta*FlightSpeed/OrbitRadius;Desired=Orbit(Angle);
  if(Angle>=2*PI && !Disturbed){Angle=0;ChangeState(EIslandBirdState::Landing);}
 }
 else
 {
  const float T=FMath::Clamp(Clock/5.f,0.f,1.f),S=T*T*(3-2*T);
  Desired=FMath::Lerp(TransitionStart,Perch,S);Desired.Y-=FMath::Sin(T*PI)*180;
  WantedFold=FMath::Clamp((T-.65f)/.35f,0.f,1.f);
  if(Disturbed){ChangeState(EIslandBirdState::Takeoff);}
  else if(T>=1){++CompletedFlights;ChangeState(EIslandBirdState::Perched);}
 }
 // Swept clearance preserves the authored route without ever flying through scenery.
 FHitResult Block;FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdFlight),false,this);
 if(State!=EIslandBirdState::Perched && GetWorld()->SweepSingleByChannel(Block,LastPosition,Desired,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(FMath::Clamp(FootOffset*.45f,.05f,12.f)),Q))
 {Desired=LastPosition;ChangeState(EIslandBirdState::Takeoff);}
 const FVector Velocity=(Desired-LastPosition)/FMath::Max(Delta,.001f);
 if(Velocity.SizeSquared()>100)
 {
  FRotator Facing=Velocity.Rotation();Facing.Pitch=FMath::Clamp(Facing.Pitch,-18.f,22.f);
  Facing.Yaw-=90;Facing.Roll=State==EIslandBirdState::Cruise?-12.f:0;
  SetActorRotation(FMath::RInterpTo(GetActorRotation(),Facing,Delta,3));
 }
 else if(State==EIslandBirdState::Perched)
 {
  // Settle upright after the banked approach; preserve the landing heading.
  SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,GetActorRotation().Yaw,0),Delta,3));
 }
 SetActorLocation(Desired);LastPosition=Desired;
 Fold=FMath::FInterpTo(Fold,WantedFold,Delta,4);Bird->SetMorphTarget(TEXT("Folded"),Fold);
 if(State==EIslandBirdState::Perched)Bird->SetPosition(0,false);
 Bird->SetPlayRate(State==EIslandBirdState::Perched?0:State==EIslandBirdState::Cruise?.8f:1.35f);
}
