#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "Vehicles/Marine/ReferenceBoatGeometry.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/Particle/ParticleUtilities.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/CommandLine.h"
#include "Water/CoastalOceanSurface.h"
#endif
UMarineVehicleComponent::UMarineVehicleComponent() { PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
void UMarineVehicleComponent::Configure(UPrimitiveComponent* Hull,const UMarineVehicleData* Data)
{
 Body=Hull; Profile=Data;
 bReferenceInputReady=false;
#if WITH_DEV_AUTOMATION_TESTS
 bRecordStepDiagnostics=FParse::Param(FCommandLine::Get(),TEXT("NeonCityBoatTelemetry"));StepDiagnostics=FReferenceBoatStepDiagnostics();
#endif
 if(!Body || !Profile || !Profile->IsValidProfile()) { Profile=nullptr; return; }
 Body->SetMassOverrideInKg(NAME_None,Profile->Mass,true);
 if(Profile->bReferenceDisplacementHull)
 {
  Body->SetCenterOfMass(FVector::ZeroVector);
  const FVector ShapeCOM=Body->GetComponentTransform().InverseTransformPosition(Body->GetCenterOfMass());
  Body->SetCenterOfMass(Profile->CenterOfMass-ShapeCOM);
 }
 else Body->SetCenterOfMass(Profile->CenterOfMass);
 Body->SetLinearDamping(Data->bReferenceDisplacementHull?0:.02f); Body->SetAngularDamping(Data->bReferenceDisplacementHull?0:.5f);
 if(Data->bReferenceDisplacementHull) if(auto* Instance=Body->GetBodyInstance())
 {
  Instance->InertiaTensorScale=FVector::OneVector;Instance->UpdateMassProperties();
  const FVector Current=Instance->GetBodyInertiaTensor();
  const FVector Source=NeonCityReferenceBoat::InertiaSource;
  if(Current.GetMin()>UE_SMALL_NUMBER) { Instance->InertiaTensorScale=FVector(Source.Z,Source.X,Source.Y)*10000/Current;Instance->UpdateMassProperties(); }
 }
 if(Profile->bReferenceDisplacementHull)Activate();
 SetAsyncPhysicsTickEnabled(Profile->bReferenceDisplacementHull);
}
void UMarineVehicleComponent::EndPlay(EEndPlayReason::Type Reason)
{
 SetAsyncPhysicsTickEnabled(false);bReferenceInputReady=false;
 Super::EndPlay(Reason);
}
bool UMarineVehicleComponent::StartEngine()
{
 if(!Profile || !Body || !GetOwner()->HasAuthority()) return false;
 if(auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead()) return false;
 bEngineRunning=true;bMoored=false; Body->WakeAllRigidBodies(); return true;
}
void UMarineVehicleComponent::SetMooringBerth(const FTransform& Berth)
{
 if(Berth.ContainsNaN())return;
 MooringBerth=Berth;bHasMooringBerth=true;bMoored=!bEngineRunning;
}
void UMarineVehicleComponent::StopEngine()
{
 bEngineRunning=false;bMoored=false;Drive=CurrentDrive=Steer=BrakeInput=0;
 bHadWaterSample=false;ForwardAcceleration=0;
 // Returning alongside the original berth can tie up again. Stopping at sea
 // never anchors the boat or attracts a restored save back to the island.
 bMoored=IsAtMooringBerth();
}
bool UMarineVehicleComponent::IsAtMooringBerth() const
{
 return bHasMooringBerth && Body && FVector::DistSquared2D(Body->GetComponentLocation(),MooringBerth.GetLocation())<FMath::Square(150.f)
  && Body->GetPhysicsLinearVelocity().Size2D()<80
  && FMath::Abs(FMath::FindDeltaAngleDegrees(Body->GetComponentRotation().Yaw,MooringBerth.Rotator().Yaw))<35;
}
void UMarineVehicleComponent::SetInput(float Throttle,float Steering,float Brake)
{
 Drive=FMath::IsFinite(Throttle)?FMath::Clamp(Throttle,-1.f,1.f):0;
 Steer=FMath::IsFinite(Steering)?FMath::Clamp(Steering,-1.f,1.f):0;
 BrakeInput=FMath::IsFinite(Brake)?FMath::Clamp(Brake,0.f,1.f):0;
}
FMarineDriveTelemetry UMarineVehicleComponent::GetTelemetry() const
{
 FMarineDriveTelemetry T;
 T.EngineRunning=bEngineRunning;T.Afloat=IsAfloat();T.Steering=Steer;
 T.SignedThrust=CurrentDrive;T.Throttle=FMath::Abs(CurrentDrive);
 T.TargetThrottle=bEngineRunning && BrakeInput<.1f?FMath::Abs(Drive):0;
 if(Body)
 {
  const FVector Relative=Body->GetPhysicsLinearVelocity()-WaterVelocity;
  T.SpeedKmh=Relative.Size2D()*.036f;
  T.ForwardSpeedKmh=FVector::DotProduct(Relative,Body->GetForwardVector().GetSafeNormal2D())*.036f;
 }
 if(!bEngineRunning) return T;
 if(BrakeInput>.1f || (Drive*T.ForwardSpeedKmh<0 && FMath::Abs(T.ForwardSpeedKmh)>2)) T.State=EMarineDriveState::Braking;
 else if(CurrentDrive<-.02f) T.State=EMarineDriveState::Reverse;
 else if(FMath::Abs(Drive)<.02f) T.State=T.SpeedKmh>1?EMarineDriveState::Coasting:EMarineDriveState::Idle;
 else T.State=FMath::Abs(CurrentDrive-Drive)>.03f || ForwardAcceleration>15?EMarineDriveState::Accelerating:EMarineDriveState::Cruise;
 return T;
}
void UMarineVehicleComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 if(!Body || !Profile || !GetOwner()->HasAuthority() || !Body->IsSimulatingPhysics()) return;
 if(auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead()) StopEngine();
 if(Profile->bReferenceDisplacementHull) { ApplyReferenceHydrodynamics(Delta);return; }
 const float Target=bEngineRunning && BrakeInput<.1f?Drive:0;
 // Releasing/reversing the throttle unloads the engine before thrust builds in
 // the new direction. Hull velocity is always the result of Chaos forces.
 const bool Reversing=Target*CurrentDrive<0;
 const float Rate=Reversing || FMath::Abs(Target)<FMath::Abs(CurrentDrive)?Profile->ThrottleFallRate:Profile->ThrottleRiseRate;
 CurrentDrive=FMath::FInterpConstantTo(CurrentDrive,Reversing?0.f:Target,FMath::Clamp(Delta,0.f,.1f),Rate);
 auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();
 const auto& D=*Profile; const float Mass=Body->GetMass(),Gravity=FMath::Abs(GetWorld()->GetGravityZ());
 WetPoints=0; WaterVelocity=FVector::ZeroVector;
 // Four separated pontoons give passive pitch/roll restoring torque and point-velocity damping.
 int32 HullIndex=0;
 for(float X:{-1.f,1.f}) for(float Y:{-1.f,1.f})
 {
  const FVector Point=Body->GetComponentTransform().TransformPosition(FVector(X*D.HullExtent.X*.8f,Y*D.HullExtent.Y*.85f,-D.HullExtent.Z));
  FWaterSurfaceSample Sample;
  if(!Water->QueryHullWater(Body,HullIndex++,Point,Sample) || Point.Z>=Sample.SurfacePoint.Z) continue;
  const float Height=Sample.SurfacePoint.Z;const FVector Current=Sample.Flow;
  ++WetPoints; WaterVelocity+=Current;
  const float Depth=FMath::Clamp((Height-Point.Z)/(D.HullExtent.Z*2),0.f,1.f);
  const float VerticalSpeed=(Body->GetPhysicsLinearVelocityAtPoint(Point)-Current).Z;
  const float Force=Mass*.25f*FMath::Clamp(Gravity*D.Buoyancy*Depth-VerticalSpeed*D.VerticalDamping,0.f,Gravity*D.Buoyancy*2);
  Body->AddForceAtLocation(FVector(0,0,Force),Point);
 }
 if(!WetPoints) { bHadWaterSample=false;ForwardAcceleration=0;return; }
 WaterVelocity/=WetPoints;
 const FVector Relative=Body->GetPhysicsLinearVelocity()-WaterVelocity;
 const FVector Forward=Body->GetForwardVector().GetSafeNormal2D(),Right=FVector::CrossProduct(FVector::UpVector,Forward);
 const float Speed=FVector::DotProduct(Relative,Forward),Side=FVector::DotProduct(Relative,Right);
 const float MeasuredAcceleration=bHadWaterSample && Delta>SMALL_NUMBER?(Speed-LastForwardSpeed)/Delta:0;
 ForwardAcceleration=FMath::FInterpTo(ForwardAcceleration,MeasuredAcceleration,FMath::Clamp(Delta,0.f,.1f),5.f);
 LastForwardSpeed=Speed;bHadWaterSample=true;
 const float Limit=D.MaxSpeed/.036f;
 const float Drag=D.WaterDrag*(1+FMath::Abs(Speed)/Limit)+BrakeInput*D.BrakeDrag;
 Body->AddForce(-Mass*(Forward*Speed*Drag+Right*Side*D.LateralDrag));
 if(bEngineRunning && BrakeInput<.1f)
 {
  const float DesiredLimit=CurrentDrive<0?Limit*.25f:Limit;
  const float Governor=Speed*CurrentDrive<0?1.f:FMath::Clamp((DesiredLimit-FMath::Abs(Speed))/200.f,0.f,1.f);
  // kW -> W -> N (at current m/s) -> kg*cm/s^2. Acceleration limits launch force.
  const float PowerForce=D.EnginePower*1000/FMath::Max(FMath::Abs(Speed)/100.f,2.f)*100.f;
  Body->AddForce(Forward*CurrentDrive*FMath::Min(Mass*D.Acceleration*100.f,PowerForce)*Governor);
 }
 const float Rudder=bEngineRunning?Steer*FMath::Clamp(Speed/350.f,-1.f,1.f):0;
 const FVector Omega=Body->GetPhysicsAngularVelocityInRadians();
 const float YawAcceleration=(FMath::DegreesToRadians(D.TurningRate)*Rudder-Omega.Z)*D.TurningResistance;
 // Acceleration-mode torque uses actual hull inertia. Roll/pitch remain fully simulated.
 Body->AddTorqueInRadians(FVector(-Omega.X,-Omega.Y,YawAcceleration),NAME_None,true);
 // Only explicitly authored berths opt in. Existing speedboats have no berth;
 // the reference fishing hull retains its independent solver path above.
 if(bMoored && bHasMooringBerth)
 {
  FVector Offset=MooringBerth.GetLocation()-Body->GetComponentLocation();Offset.Z=0;
  if(Offset.Size2D()>800){bMoored=false;return;}
  FVector Velocity=Body->GetPhysicsLinearVelocity();Velocity.Z=0;
  const FVector Acceleration=(Offset*.35f-Velocity*1.2f).GetClampedToMaxSize(120);
  Body->AddForce(Acceleration*Mass);
  const float Heading=FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Body->GetComponentRotation().Yaw,MooringBerth.Rotator().Yaw));
  Body->AddTorqueInRadians(FVector(0,0,FMath::Clamp(Heading*.6f-Omega.Z*1.4f,-.4f,.4f)),NAME_None,true);
 }
}

void UMarineVehicleComponent::ApplyReferenceHydrodynamics(float Delta)
{
 using namespace NeonCityReferenceBoat;
 bReferenceInputReady=false;
#if WITH_DEV_AUTOMATION_TESTS
 if(bRecordStepDiagnostics){StepDiagnostics.ValidWaterSamples=0;StepDiagnostics.MaximumIssuedAge=0;StepDiagnostics.MaximumAngularAcceleration=0;StepDiagnostics.MaximumLeaseAge=0;StepDiagnostics.MaximumLeaseFootprint=0;StepDiagnostics.UnavailableLeases=0;StepDiagnostics.ExpiredLeases=0;StepDiagnostics.OutsideLeases=0;}
#endif
 const double DT=FMath::Clamp(double(Delta),0.,.1);
 const double Target=bEngineRunning && BrakeInput<.1f?Drive:0;
 CurrentDrive+=(Target-CurrentDrive)*(1-FMath::Exp(-DT*2.2));
 ReferenceSteer+=((bEngineRunning?Steer:0)-ReferenceSteer)*(1-FMath::Exp(-DT*1.2));
 const double Lever=bEngineRunning?FMath::Abs(CurrentDrive):0,TargetRPM=FMath::Max(Lever,bEngineRunning?.15:0.);
 ReferenceRPM+=(TargetRPM-ReferenceRPM)*(1-FMath::Exp(-DT/(TargetRPM>ReferenceRPM?.9:1.3)));
 FHydroState S;
 S.Position=ToSourcePolar(Body->GetComponentLocation()/100);S.Rotation=ToSourceRotation(Body->GetComponentQuat());
 S.Velocity=ToSourcePolar(Body->GetPhysicsLinearVelocity()/100);S.Angular=ToSourceAxial(Body->GetPhysicsAngularVelocityInRadians());
 S.RPM=ReferenceRPM;S.Throttle=CurrentDrive;S.Steering=-ReferenceSteer;S.Driven=bEngineRunning;
 if(bMoored && FVector::DistSquared2D(Body->GetComponentLocation(),MooringBerth.GetLocation())>FMath::Square(800.f))bMoored=false;
 S.Moored=bMoored;S.MooringAnchor=ToSourcePolar(MooringBerth.GetLocation()/100);
 const FVector BerthForward=ToSourceRotation(MooringBerth.GetRotation()).RotateVector(FVector(0,0,1));
 S.MooringHeading=FMath::Atan2(BerthForward.X,BerthForward.Z);
 S.Gravity=FMath::Abs(GetWorld()->GetGravityZ())/100.;
 auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>();WaterVelocity=FVector::ZeroVector;WetPoints=0;
 ReferenceWaterPoints.Reset();ReferenceWaterNormals.Reset();
 ReferenceWaterStates.Reset();ReferenceInitialEffective.Reset();
 int32 Samples=0,HullIndex=0;
 for(const auto& H:HullSamples)
 {
  const FVector P=Body->GetComponentTransform().TransformPosition(ToUEPolar(H.Position)*100);
  FWaterSurfaceSample Sample;
  FTidewaterWaterSample Snapshot;
#if WITH_DEV_AUTOMATION_TESTS
  if(bRecordStepDiagnostics)
  {
   const auto Lease=Water->QueryHullLeaseDiagnostic(Body,HullIndex,P);
   StepDiagnostics.MaximumLeaseAge=FMath::Max(StepDiagnostics.MaximumLeaseAge,Lease.IssuedAge);
   StepDiagnostics.MaximumLeaseFootprint=FMath::Max(StepDiagnostics.MaximumLeaseFootprint,Lease.FootprintMetres);
   StepDiagnostics.UnavailableLeases+=Lease.State<3?1:0;StepDiagnostics.OutsideLeases+=Lease.State==3?1:0;StepDiagnostics.ExpiredLeases+=Lease.State==4?1:0;
  }
#endif
  if(Water->QueryHullWater(Body,HullIndex++,P,Sample,&Snapshot))
  {
   S.WaterHeight.Add(Sample.SurfacePoint.Z/100);S.WaterVerticalVelocity.Add(Sample.Flow.Z/100);
   ReferenceWaterPoints.Add(P);ReferenceWaterNormals.Add(Sample.Normal);
   WaterVelocity+=Sample.Flow;++Samples;if(P.Z<Sample.SurfacePoint.Z)++WetPoints;
  }
  else { S.WaterHeight.Add(P.Z/100-100);S.WaterVerticalVelocity.Add(0);ReferenceWaterPoints.Add(P);ReferenceWaterNormals.Add(FVector::UpVector); }
  ReferenceWaterStates.Add(Snapshot);ReferenceInitialEffective.Add(Snapshot.Effective);
#if WITH_DEV_AUTOMATION_TESTS
  if(bRecordStepDiagnostics && Snapshot.Valid){++StepDiagnostics.ValidWaterSamples;StepDiagnostics.MaximumIssuedAge=FMath::Max(StepDiagnostics.MaximumIssuedAge,double(GetWorld()->GetTimeSeconds())-Snapshot.Time);}
#endif
 }
 if(!Samples) { bHadWaterSample=false;ForwardAcceleration=0;return; }
 WaterVelocity/=Samples;
 ReferenceInput=MoveTemp(S);bReferenceInputReady=true;
 if(!Body->RigidBodyIsAwake())Body->WakeAllRigidBodies();
 // The native brake input closes the throttle; it never freezes a drifting vessel.
 const float ForwardSpeed=FVector::DotProduct(Body->GetPhysicsLinearVelocity()-WaterVelocity,Body->GetForwardVector());
 ForwardAcceleration=bHadWaterSample && DT>UE_SMALL_NUMBER?(ForwardSpeed-LastForwardSpeed)/DT:0;
 LastForwardSpeed=ForwardSpeed;bHadWaterSample=true;
}

void UMarineVehicleComponent::AsyncPhysicsTickComponent(float DeltaTime,float SimTime)
{
 using namespace NeonCityReferenceBoat;
 if(!bReferenceInputReady || !Body || !Body->GetBodyInstance())return;
 const auto Proxy=Body->GetBodyInstance()->GetPhysicsActorHandle();
 auto* Particle=Proxy?Proxy->GetPhysicsThreadAPI():nullptr;
 if(!Particle || Particle->ObjectState()!=Chaos::EObjectStateType::Dynamic)return;
 const FTransform Pose=Chaos::FParticleUtilitiesXR::GetActorWorldTransform(Particle);
 FHydroState S=ReferenceInput;
 S.Position=ToSourcePolar(Pose.GetLocation()/100);S.Rotation=ToSourceRotation(Pose.GetRotation());
 S.Velocity=ToSourcePolar(Particle->V()/100);S.Angular=ToSourceAxial(Particle->W());
 int32 I=0;
 for(const auto& H:HullSamples)
 {
  const FVector P=Pose.TransformPosition(ToUEPolar(H.Position)*100);
  const FVector Shift=P-ReferenceWaterPoints[I],N=ReferenceWaterNormals[I];
  const double Tangent=-(N.X*Shift.X+N.Y*Shift.Y)/FMath::Max(.1,double(N.Z));
  // Continue the same bounded sample age and 60 ms readback blend. Starting a
  // second prediction clock here would extrapolate old water beyond 120 ms.
  auto& WaterState=ReferenceWaterStates[I];WaterState.Advance(DeltaTime);
  S.WaterHeight[I]+=FMath::Clamp(Tangent/100,-.5,.5)+WaterState.Effective-ReferenceInitialEffective[I];
  ++I;
 }
 const auto Result=EvaluateHydrodynamics(S);
 if(Result.Acceleration.ContainsNaN() || Result.AngularAcceleration.ContainsNaN())return;
#if WITH_DEV_AUTOMATION_TESTS
 if(bRecordStepDiagnostics)
 {
  ++StepDiagnostics.Steps;StepDiagnostics.LastDelta=DeltaTime;StepDiagnostics.MaximumDelta=FMath::Max(StepDiagnostics.MaximumDelta,double(DeltaTime));
  StepDiagnostics.Acceleration=ToUEPolar(Result.Acceleration);StepDiagnostics.AngularAcceleration=ToUEAxial(Result.AngularAcceleration);
  StepDiagnostics.MaximumAngularAcceleration=FMath::Max(StepDiagnostics.MaximumAngularAcceleration,Result.AngularAcceleration.Size());
  StepDiagnostics.MinimumWater=StepDiagnostics.MaximumWater=S.WaterHeight[0];StepDiagnostics.MaximumSampleAge=0;
  for(int32 J=0;J<S.WaterHeight.Num();++J)
  {
   StepDiagnostics.MinimumWater=FMath::Min(StepDiagnostics.MinimumWater,S.WaterHeight[J]);StepDiagnostics.MaximumWater=FMath::Max(StepDiagnostics.MaximumWater,S.WaterHeight[J]);
   StepDiagnostics.MaximumSampleAge=FMath::Max(StepDiagnostics.MaximumSampleAge,ReferenceWaterStates[J].Age);
  }
 }
#endif
 // Cancel only Chaos gravity already included by the source evaluator. All
 // movement, buoyancy, roll and contacts remain integrated by the live solver.
 const FVector Acceleration=ToUEPolar(Result.Acceleration)*100+FVector(0,0,S.Gravity*100);
 Particle->AddForce(Acceleration*Particle->M());
 Particle->AddTorque(Chaos::FParticleUtilitiesXR::GetWorldInertia(Particle)*ToUEAxial(Result.AngularAcceleration));
}
