#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Vehicles/NeonCitySedanVisuals.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ANeonCityTrafficVehicle::ANeonCityTrafficVehicle()
{
	DamageHealth=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Combat=CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	CreateDefaultSubobject<UVehicleDamageComponent>(TEXT("VehicleDamageState"));
 CreateDefaultSubobject<UPhysicalImpactComponent>(TEXT("PhysicalImpact"));
	PrimaryActorTick.bCanEverTick = true;
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("TrafficBody"));
	SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(235,96,63));
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionObjectType(ECC_Vehicle);
	USceneComponent* VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TrafficVisualRoot"));
	VisualRoot->SetupAttachment(Collision);
	VisualRoot->SetRelativeLocation(FVector(0,0,-60));
	Wheels = FNeonCitySedanVisuals::Create(*this,VisualRoot);
	const FVector Positions[] = {FVector(135,-90,11),FVector(135,90,11),FVector(-129.2,-90,11),FVector(-129.2,90,11)};
 auto* Fitted=FNeonCitySedanVisuals::GetFleetPresentation();
 if(Fitted && Fitted->IsComplete())
 {
  // Route actors sit 81cm above the road contact plane.
  VisualRoot->SetRelativeLocation(FVector(0,0,Fitted->Wheels[0].RadiusCm-81.f));
  PresentationWheelRadiusCm=Fitted->Wheels[0].RadiusCm;
  Collision->SetBoxExtent(FVector(250,105,63));
 }
	for (int32 Index = 0; Index < Wheels.Num(); ++Index)
	{
		Wheels[Index]->SetRelativeLocationAndRotation(Fitted && Fitted->IsComplete()?Fitted->Wheels[Index].CentreCm:Positions[Index],FRotator(0,0,90));
	}
}
void ANeonCityTrafficVehicle::InitializeRoute(ANeonCityTrafficRoute* InRoute, float InDistance)
{
	Route = InRoute; Distance = InDistance;
}
void ANeonCityTrafficVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CurrentVelocity = FVector::ZeroVector;
	if (DamageHealth->IsDead() || FindComponentByClass<UVehicleDamageComponent>()->GetDrivePowerMultiplier()==0) { return; }
	BlockingActor.Reset();
	if (!Route.IsValid() || !FMath::IsFinite(CruiseSpeed) || DeltaSeconds <= 0.f) { return; }
	USplineComponent* Spline = Route->GetRoadSpline();
	const float Length = Spline->GetSplineLength();
	if (Length < 100.f) { return; }
	const float Speed = FMath::Clamp(CruiseSpeed,100.f,800.f);
	const FVector Start = GetActorLocation();
	const FVector Ahead = Spline->GetLocationAtDistanceAlongSpline(FMath::Fmod(Distance+360.f,Length),ESplineCoordinateSpace::World);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TrafficYield),false,this);
	FHitResult Obstacle;
	bYielding = GetWorld()->SweepSingleByChannel(Obstacle,Start,Ahead,GetActorQuat(),ECC_Vehicle,
		FCollisionShape::MakeBox(FVector(Collision->GetUnscaledBoxExtent().X,Collision->GetUnscaledBoxExtent().Y,60)),Query);
	if (bYielding) { BlockingActor = Obstacle.GetActor(); if(Obstacle.Distance>Speed*.1f || Obstacle.bStartPenetrating) return; }
	const float Step = Speed*FMath::Min(DeltaSeconds,0.1f);
	const float NextDistance = FMath::Fmod(Distance+Step,Length);
	const FVector NextLocation = Spline->GetLocationAtDistanceAlongSpline(NextDistance,ESplineCoordinateSpace::World);
	const FRotator NextRotation = Spline->GetRotationAtDistanceAlongSpline(NextDistance,ESplineCoordinateSpace::World);
	FHitResult Hit;
 FindComponentByClass<UPhysicalImpactComponent>()->SetKinematicVelocity((NextLocation-Start)/DeltaSeconds);
	SetActorLocationAndRotation(NextLocation,FRotator(0,NextRotation.Yaw,0),true,&Hit);
 if(Hit.bBlockingHit) FindComponentByClass<UPhysicalImpactComponent>()->SubmitSweep(Hit.GetActor(),Hit);
	Distance = FMath::Fmod(Distance+Step*(Hit.bBlockingHit ? Hit.Time : 1.f),Length);
	bYielding = Hit.bBlockingHit;
	if (bYielding) { BlockingActor = Hit.GetActor(); }
	CurrentVelocity = (GetActorLocation()-Start)/DeltaSeconds;
	WheelAngle = FMath::Fmod(WheelAngle+FMath::RadiansToDegrees(CurrentVelocity.Size()*DeltaSeconds/PresentationWheelRadiusCm),360.f);
	const FQuat Rotation = FRotator(0,0,90).Quaternion()*FQuat(FVector::UpVector,FMath::DegreesToRadians(WheelAngle));
	for (UStaticMeshComponent* Wheel : Wheels) { Wheel->SetRelativeRotation(Rotation); }
}

bool ANeonCityTrafficVehicle::Attack_Implementation(const FCombatAttackRequest& Request) { return Combat->Attack(Request); }
float ANeonCityTrafficVehicle::TakeCombatDamage_Implementation(const FNeonCityDamageEvent& Event) { return GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(this,Event); }
ENeonCityCombatState ANeonCityTrafficVehicle::GetCombatState_Implementation() const { return Combat->GetCombatState(); }
