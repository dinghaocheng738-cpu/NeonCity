#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
ANeonCityTrafficRoute::ANeonCityTrafficRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	Road = CreateDefaultSubobject<USplineComponent>(TEXT("RoadNodes"));
	SetRootComponent(Road);
	Road->SetClosedLoop(true);
}
void ANeonCityTrafficRoute::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || Road->GetNumberOfSplinePoints() < 4) { return; }
	int32 Existing = 0;
	for (TActorIterator<ANeonCityTrafficVehicle> It(GetWorld()); It; ++It) { ++Existing; }
	const int32 Count = FMath::Min(FMath::Clamp(VehicleCount,0,3),3-Existing);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Distance = Road->GetSplineLength()*Index/FMath::Max(Count,1);
		const FTransform Transform = Road->GetTransformAtDistanceAlongSpline(Distance,ESplineCoordinateSpace::World,false);
		ANeonCityTrafficVehicle* Car = GetWorld()->SpawnActorDeferred<ANeonCityTrafficVehicle>(
			ANeonCityTrafficVehicle::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
		if (Car)
		{
			Car->InitializeRoute(this,Distance);
			Car->FinishSpawning(Transform);
			Spawned.Add(Car);
		}
	}
}
void ANeonCityTrafficRoute::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (const TWeakObjectPtr<ANeonCityTrafficVehicle>& Car : Spawned) { if (Car.IsValid()) { Car->Destroy(); } }
	}
	Spawned.Reset();
	Super::EndPlay(EndPlayReason);
}
