#pragma once
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Math/RotationMatrix.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"

/** Read-only forecast of mapped throttle/steering. Query every actual PhysicsAsset
 * body instead of a larger invented solid box bridging chassis/roof empty space.
 * Full vehicle footprint support, water, slope and all blocking collision remain.
 * Native measured ride height and suspension attitude seed each short forecast.
 * This helper never writes a pose, velocity, physics, road, collision or input. */
inline bool Phase205cNativeVehicleTurnSurvey(UWorld* World,APawn* Vehicle,USkeletalMeshComponent* Mesh,
 bool Reverse,float Steer,FString& Failure,float TravelCm=200.f)
{
 if(!World || !Vehicle || !Mesh || !Mesh->GetPhysicsAsset() || Mesh->GetPhysicsAsset()->SkeletalBodySetups.IsEmpty())
 {Failure=TEXT("Native physical vehicle body unavailable");return false;}
 FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(PhysicalVehicleTurnSupport),false,Vehicle);
 const FTransform MeshRelative=Mesh->GetComponentTransform().GetRelativeTransform(Vehicle->GetActorTransform());
 FVector Position=Vehicle->GetActorLocation();float Yaw=Vehicle->GetActorRotation().Yaw;
 TArray<FVector> PreviousCenters;
 FQuat SuspensionResidual=FQuat::Identity;double RideHeight=0;bool First=true;
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(TravelCm/25.f),1,8);
 for(int StepIndex=0;StepIndex<=Steps;++StepIndex)
 {
  if(StepIndex){const float Distance=Reverse?-25.f:25.f;const float YawDelta=Distance*FMath::Tan(FMath::DegreesToRadians(Steer*38))/350;
   Position+=FRotator(0,Yaw+FMath::RadiansToDegrees(YawDelta)*.5f,0).Vector()*Distance;Yaw+=FMath::RadiansToDegrees(YawDelta);}
  const FVector Forward=FRotator(0,Yaw,0).Vector(),Right=FRotationMatrix(FRotator(0,Yaw,0)).GetUnitAxis(EAxis::Y);
  const FVector Offsets[]={FVector::ZeroVector,FVector(245,0,0),FVector(-245,0,0),FVector(0,125,0),FVector(0,-125,0),FVector(245,125,0),FVector(245,-125,0),FVector(-245,125,0),FVector(-245,-125,0)};
  FVector Ground[UE_ARRAY_COUNT(Offsets)];double MeanHeight=0;float Low=BIG_NUMBER,High=-BIG_NUMBER;
  for(int ProbeIndex=0;ProbeIndex<UE_ARRAY_COUNT(Offsets);++ProbeIndex)
  {
   const FVector XY=Position+Forward*Offsets[ProbeIndex].X+Right*Offsets[ProbeIndex].Y;FHitResult Hit;
   if(!World->LineTraceSingleByChannel(Hit,XY+FVector(0,0,250),XY-FVector(0,0,400),ECC_Vehicle,FloorQuery) || Hit.ImpactNormal.Z<.94)
   {Failure=FString::Printf(TEXT("step%d full-footprint support%d missing/steep at%s; hit%s"),StepIndex,ProbeIndex,*XY.ToString(),*GetPathNameSafe(Hit.GetComponent()));return false;}
   Ground[ProbeIndex]=Hit.ImpactPoint;MeanHeight+=Hit.ImpactPoint.Z;Low=FMath::Min(Low,float(Hit.ImpactPoint.Z));High=FMath::Max(High,float(Hit.ImpactPoint.Z));
  }
  MeanHeight/=UE_ARRAY_COUNT(Offsets);float Water=0;FVector Current;
  if(High-Low>65 || (World->GetSubsystem<UMarineWorldSubsystem>() && World->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(FVector(Position.X,Position.Y,Low),Water,Current) && Low<Water+15))
  {Failure=FString::Printf(TEXT("step%d full footprint unsupported height/water Low%.3f High%.3f"),StepIndex,Low,High);return false;}
  const double Along=(Ground[1].Z-Ground[2].Z)/490.,Across=(Ground[3].Z-Ground[4].Z)/250.;
  const FVector Normal=(-Forward*Along-Right*Across+FVector::UpVector).GetSafeNormal();
  if(Normal.Z<.94){Failure=TEXT("Full support plane exceeds native slope gate");return false;}
  const FQuat PlaneRotation=FRotationMatrix::MakeFromZX(Normal,Forward).ToQuat();
  if(First){RideHeight=Position.Z-MeanHeight;SuspensionResidual=PlaneRotation.Inverse()*Vehicle->GetActorQuat();}
  const FTransform Forecast(PlaneRotation*SuspensionResidual,FVector(Position.X,Position.Y,MeanHeight+RideHeight),Vehicle->GetActorScale3D());
  const FTransform MeshForecast=MeshRelative*Forecast;int32 ShapeIndex=0;
  for(const auto& Setup:Mesh->GetPhysicsAsset()->SkeletalBodySetups)
  {
   if(!Setup){Failure=TEXT("Native PhysicsAsset contains a null body");return false;}
   const FBodyInstance* NativeBody=Mesh->GetBodyInstance(Setup->BoneName);
   // Survey all live native body shapes; disabled/null bodies have no collider.
   if(!NativeBody || NativeBody->GetCollisionEnabled()==ECollisionEnabled::NoCollision)continue;
   if(Setup->AggGeom.GetElementCount()!=Setup->AggGeom.BoxElems.Num()+Setup->AggGeom.SphereElems.Num()+Setup->AggGeom.SphylElems.Num())
   {Failure=FString::Printf(TEXT("Native active body%s includes unsupported shapes: boxes%d total%d collision%d; cannot omit them"),*Setup->BoneName.ToString(),Setup->AggGeom.BoxElems.Num(),Setup->AggGeom.GetElementCount(),int32(NativeBody->GetCollisionEnabled()));return false;}
   const FTransform BoneRelative=NativeBody->GetUnrealWorldTransform().GetRelativeTransform(Mesh->GetComponentTransform());
   auto SurveyShape=[&](const FTransform& ShapeForecast,const FCollisionShape& Shape,const FString& Description)->bool
   {
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByChannel(Overlaps,ShapeForecast.GetLocation(),ShapeForecast.GetRotation(),ECC_Vehicle,Shape,FloorQuery);
    for(const auto& Overlap:Overlaps)if(Overlap.bBlockingHit)
    {Failure=FString::Printf(TEXT("step%d PA shape%d bone%s %s overlap%s; transform%s nativeRideHeight%.3f supportLow%.3f High%.3f"),StepIndex,ShapeIndex,*Setup->BoneName.ToString(),*Description,*GetPathNameSafe(Overlap.GetComponent()),*ShapeForecast.ToString(),RideHeight,Low,High);return false;}
    if(!First)
    {
     FHitResult Hit;if(World->SweepSingleByChannel(Hit,PreviousCenters[ShapeIndex],ShapeForecast.GetLocation(),ShapeForecast.GetRotation(),ECC_Vehicle,Shape,FloorQuery))
     {Failure=FString::Printf(TEXT("step%d PA shape%d bone%s %s sweep%s; transform%s hit%s normal%s penetration%d"),StepIndex,ShapeIndex,*Setup->BoneName.ToString(),*Description,*GetPathNameSafe(Hit.GetComponent()),*ShapeForecast.ToString(),*Hit.ImpactPoint.ToString(),*Hit.ImpactNormal.ToString(),Hit.bStartPenetrating);return false;}
    }
    if(First)PreviousCenters.Add(ShapeForecast.GetLocation());else PreviousCenters[ShapeIndex]=ShapeForecast.GetLocation();++ShapeIndex;return true;
   };
   for(const auto& Box:Setup->AggGeom.BoxElems)
   {
    const FTransform T=FTransform(Box.Rotation,Box.Center)*BoneRelative*MeshForecast;
    const FVector Half=FVector(Box.X,Box.Y,Box.Z)*T.GetScale3D().GetAbs()*.5;
    if(!SurveyShape(T,FCollisionShape::MakeBox(Half),TEXT("box half")+Half.ToString()))return false;
   }
   for(const auto& Sphere:Setup->AggGeom.SphereElems)
   {
    const FTransform T=FTransform(FQuat::Identity,Sphere.Center)*BoneRelative*MeshForecast;
    const float Radius=Sphere.Radius*T.GetScale3D().GetAbs().GetMax();
    if(!SurveyShape(T,FCollisionShape::MakeSphere(Radius),FString::Printf(TEXT("sphere radius%.3f"),Radius)))return false;
   }
   for(const auto& Capsule:Setup->AggGeom.SphylElems)
   {
    const FTransform T=FTransform(Capsule.Rotation,Capsule.Center)*BoneRelative*MeshForecast;
    const FVector Scale=T.GetScale3D().GetAbs();const float Radius=Capsule.Radius*FMath::Max(Scale.X,Scale.Y),Half=Capsule.Length*Scale.Z*.5f+Radius;
    if(!SurveyShape(T,FCollisionShape::MakeCapsule(Radius,Half),FString::Printf(TEXT("capsule radius%.3f half%.3f"),Radius,Half)))return false;
   }
  }
  if(!ShapeIndex){Failure=TEXT("No native physical boxes surveyed");return false;}First=false;
 }
 return true;
}
