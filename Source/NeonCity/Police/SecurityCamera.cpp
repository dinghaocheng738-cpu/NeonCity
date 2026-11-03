#include "Police/SecurityCamera.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
ASecurityCamera::ASecurityCamera()
{
 PrimaryActorTick.bCanEverTick=false;
 auto* Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraHousing")); SetRootComponent(Body); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); if(Cube.Succeeded()) Body->SetStaticMesh(Cube.Object); Body->SetRelativeScale3D(FVector(.35,.2,.2));
 Sensor=CreateDefaultSubobject<UPoliceSensorComponent>(TEXT("Surveillance")); Sensor->bSurveillance=true; Sensor->SightRange=2200; Sensor->HalfAngleDegrees=40; Sensor->EyeOffset=FVector::ZeroVector;
}
