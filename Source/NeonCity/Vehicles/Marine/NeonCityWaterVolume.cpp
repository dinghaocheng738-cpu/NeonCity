#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "Water/CoastalOceanSurface.h"
ANeonCityWaterVolume::ANeonCityWaterVolume()
{
 bWaterVolume=true; Priority=10; FluidFriction=.6f;
 GetBrushComponent()->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
 GetBrushComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
}
void ANeonCityWaterVolume::PostInitializeComponents()
{
 // Native brush collision also supplies CMC's immersion-depth trace. No editor-only BSP builder.
 auto* VolumeBrush=GetBrushComponent();
 VolumeBrush->BrushBodySetup=NewObject<UBodySetup>(VolumeBrush);
 VolumeBrush->BrushBodySetup->CollisionTraceFlag=CTF_UseSimpleAsComplex;
 // Broad phase contains the displaced crests; exact membership below uses the
 // shared surface sample. The mean-level brush previously dropped swimmers into
 // falling mode at every crest. This is an overlap envelope, not a higher sea.
 const float CrestEnvelope=bContainedPoolWater?0.f:1000.f;
 FKBoxElem Box; Box.X=Extent.X*2; Box.Y=Extent.Y*2; Box.Z=Extent.Z*2+CrestEnvelope;
 Box.Center.Z=CrestEnvelope*.5f;
 VolumeBrush->BrushBodySetup->AggGeom.BoxElems.Add(Box);
 VolumeBrush->RecreatePhysicsState(); VolumeBrush->UpdateBounds();
 Super::PostInitializeComponents();
}
void ANeonCityWaterVolume::BeginPlay()
{
 Super::BeginPlay(); GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->RegisterWater(this);
}
void ANeonCityWaterVolume::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* Registry=GetWorld()->GetSubsystem<UMarineWorldSubsystem>()) Registry->UnregisterWater(this);
 Super::EndPlay(Reason);
}
bool ANeonCityWaterVolume::Sample(FVector Point,float& SurfaceZ,FVector& Velocity) const
{
 // A surface-height query describes the whole water column, including below the swim volume.
 // A sinking land car must not restart its engine when it crosses the volume bottom.
 FWaterSurfaceSample Result; if(!QueryWaterSurface_Implementation(Point,Result)) return false;
 SurfaceZ=Result.SurfacePoint.Z; Velocity=Result.Flow; return true;
}
bool ANeonCityWaterVolume::QueryWaterSurface_Implementation(FVector Point,FWaterSurfaceSample& Result) const
{
 const FVector Local=GetActorTransform().InverseTransformPosition(Point);
 if(Point.ContainsNaN() || FMath::Abs(Local.X)>Extent.X || FMath::Abs(Local.Y)>Extent.Y ||
    (bContainedPoolWater && Local.Z < -Extent.Z)) return false;
 Result.SurfacePoint=GetActorTransform().TransformPosition(FVector(Local.X,Local.Y,Extent.Z));
 Result.Normal=GetActorUpVector(); Result.Flow=CurrentVelocity;Result.ContainedWater=bContainedPoolWater;
 if(IsValid(OceanSurface) && OceanSurface->IsReady())
 {
  float Height=Result.SurfacePoint.Z;FVector WaveVelocity;
  OceanSurface->Sample(Point,Height,Result.Normal,WaveVelocity);
  Result.SurfacePoint.Z=Height;Result.Flow+=WaveVelocity;
 }
 Result.InsideVolume=Local.Z>=-Extent.Z && Point.Z<=Result.SurfacePoint.Z; return true;
}
bool ANeonCityWaterVolume::IsOverlapInVolume(const USceneComponent& TestComponent) const
{
 FWaterSurfaceSample Result;
 return QueryWaterSurface_Implementation(TestComponent.GetComponentLocation(),Result) && Result.InsideVolume;
}
