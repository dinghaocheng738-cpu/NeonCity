#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftWaterInteraction.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
UAircraftVehicleComponent::UAircraftVehicleComponent() { PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PrePhysics; }
void UAircraftVehicleComponent::Configure(UPrimitiveComponent* InBody,const UAircraftVehicleData* Data)
{
 Body=InBody; Profile=Data;
 if(!Body || !Profile || !Profile->IsValidProfile()) { Profile=nullptr; return; }
 Body->SetMassOverrideInKg(NAME_None,Profile->Mass,true); Body->SetCenterOfMass(Profile->CenterOfMass);
 Body->SetLinearDamping(.05f); Body->SetAngularDamping(.4f); ResetAfterLoad();
}
EAircraftCondition UAircraftVehicleComponent::GetCondition() const
{
 const auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 const float Ratio=Health?Health->GetCurrentHealth()/FMath::Max(1.f,Health->GetMaxHealth()):1;
 if(Ratio<=(Profile?Profile->DisabledFraction:.1f)) return EAircraftCondition::Disabled;
 if(Ratio<=(Profile?Profile->CriticalFraction:.25f)) return EAircraftCondition::Critical;
 return Ratio<.65f?EAircraftCondition::Damaged:EAircraftCondition::Healthy;
}
bool UAircraftVehicleComponent::CanPower() const
{
 const auto* Water=GetOwner()->FindComponentByClass<UAircraftWaterInteraction>();
 return Body && Profile && GetOwner()->HasAuthority() && FuelFraction>0 && GetCondition()!=EAircraftCondition::Disabled
  && (!Water || Water->GetWaterState()!=EAircraftWaterState::Submerged) && Body->GetUpVector().Z>.35f;
}
bool UAircraftVehicleComponent::StartEngine()
{
 if(!CanPower()) return false;
 if(bEngineRunning)return true;
 bEngineRunning=true;CurrentPower=TargetPower=Profile->HoverPower;PowerInput=0;PreviousSpeed=Body->GetPhysicsLinearVelocity().Size2D();HorizontalAcceleration=0;
 HoverHeight=Body->GetComponentLocation().Z; bWasClimbing=false; Body->WakeAllRigidBodies(); return true;
}
void UAircraftVehicleComponent::StopEngine()
{ bEngineRunning=false; Input=FVector::ZeroVector; YawInput=0; bWasClimbing=false;CurrentPower=TargetPower=PowerInput=HorizontalAcceleration=PreviousSpeed=0; }
bool UAircraftVehicleComponent::SetPowerInput(float Adjustment)
{
 if(!GetOwner()->HasAuthority() || !FMath::IsFinite(Adjustment))return false;
 PowerInput=FMath::Clamp(Adjustment,-1.f,1.f);return true;
}
FAircraftFlightTelemetry UAircraftVehicleComponent::GetTelemetry() const
{
 FAircraftFlightTelemetry T;T.SpeedKmh=Body?Body->GetPhysicsLinearVelocity().Size2D()*.036f:0;
 T.AltitudeMeters=Altitude/100;T.CurrentPower=CurrentPower;T.TargetPower=TargetPower;T.EngineRunning=bEngineRunning;
 if(!bEngineRunning)T.Phase=NSLOCTEXT("NeonCity","FlightOff","Engine off");
 else if(bGrounded)T.Phase=NSLOCTEXT("NeonCity","FlightGrounded","Grounded");
 else if(PowerInput<-.05f)T.Phase=NSLOCTEXT("NeonCity","FlightDecelerating","Decelerating");
 else if(FlightState==EAircraftFlightState::TakingOff)T.Phase=NSLOCTEXT("NeonCity","FlightClimbing","Climbing");
 else if(FlightState==EAircraftFlightState::Landing)T.Phase=NSLOCTEXT("NeonCity","FlightDescending","Descending");
 else if(T.SpeedKmh<5 && Input.IsNearlyZero(.05f))T.Phase=NSLOCTEXT("NeonCity","FlightHover","Hover");
 else if(CurrentPower>.9f && T.SpeedKmh>70)T.Phase=NSLOCTEXT("NeonCity","FlightFast","Fast flight");
 else if(HorizontalAcceleration>30)T.Phase=NSLOCTEXT("NeonCity","FlightAccelerating","Accelerating");
 else T.Phase=NSLOCTEXT("NeonCity","FlightCruise","Cruise");
 return T;
}
bool UAircraftVehicleComponent::SetFuelFraction(float Value)
{
 if(!GetOwner()->HasAuthority() || !FMath::IsFinite(Value) || Value<0 || Value>1) return false;
 FuelFraction=Value; if(Value==0) StopEngine(); return true;
}
void UAircraftVehicleComponent::SetInput(FVector Translation,float Yaw)
{
 if(!GetOwner()->HasAuthority()) return;
 Input=Translation.ContainsNaN()?FVector::ZeroVector:Translation.BoundToCube(1);
 const FVector Horizontal=FVector(Input.X,Input.Y,0).GetClampedToMaxSize(1); Input.X=Horizontal.X; Input.Y=Horizontal.Y;
 YawInput=FMath::IsFinite(Yaw)?FMath::Clamp(Yaw,-1.f,1.f):0;
}
void UAircraftVehicleComponent::SetState(EAircraftFlightState Value)
{ if(Value!=FlightState) { auto Previous=FlightState; FlightState=Value; OnFlightStateChanged.Broadcast(Previous,Value); } }
void UAircraftVehicleComponent::SampleGround()
{
 FHitResult Floor; FCollisionQueryParams Query(SCENE_QUERY_STAT(AircraftAltitude),false,GetOwner());
 const FVector Center=Body->GetComponentLocation(); float Surface=Center.Z-100000;
 if(GetWorld()->LineTraceSingleByChannel(Floor,Center,Center-FVector(0,0,100000),ECC_WorldStatic,Query)) Surface=Floor.ImpactPoint.Z;
 float WaterHeight; FVector Current;
 if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Center,WaterHeight,Current)) Surface=FMath::Max(Surface,WaterHeight);
 Altitude=FMath::Max(0.f,float(Center.Z-Surface-Profile->BodyExtent.Z));
 bGrounded=Floor.bBlockingHit && Floor.ImpactNormal.Z>.7f && Center.Z-Floor.ImpactPoint.Z<Profile->BodyExtent.Z+25 && Body->GetUpVector().Z>.9f;
 if(bGrounded)
 {
  // a broad footing, not the head of a lamp post or a bollard under the belly's centre: two corners must be on it too
  const FVector E=Profile->BodyExtent;const FTransform& X=Body->GetComponentTransform();int32 Feet=0;
  for(const FVector2D Corner:{FVector2D(.7f,.9f),FVector2D(.7f,-.9f),FVector2D(-.7f,.9f),FVector2D(-.7f,-.9f)})
  {
   const FVector Foot=X.TransformPositionNoScale(FVector(E.X*Corner.X,E.Y*Corner.Y,-E.Z));FHitResult Contact;
   Feet+=GetWorld()->LineTraceSingleByChannel(Contact,Foot+FVector(0,0,40),Foot-FVector(0,0,70),ECC_WorldStatic,Query);
  }
  bGrounded=Feet>=2;
 }
 // Resting on skids / gear over uneven ground (a kerb, a lawn edge, a gentle slope): the centre probe can miss the
 // ground by more than the margin while the aircraft is sitting still. Count it as landed when it has settled,
 // is not steeply tilted and at least two corners of its belly touch walkable ground.
 if(!bGrounded && Body->GetUpVector().Z>.8f && FMath::Abs(Body->GetPhysicsLinearVelocity().Z)<80)
 {
  const FVector E=Profile->BodyExtent;const FTransform& X=Body->GetComponentTransform();int32 Feet=0;
  for(const FVector2D Corner:{FVector2D(.7f,.9f),FVector2D(.7f,-.9f),FVector2D(-.7f,.9f),FVector2D(-.7f,-.9f)})
  {
   const FVector Foot=X.TransformPositionNoScale(FVector(E.X*Corner.X,E.Y*Corner.Y,-E.Z));FHitResult Contact;
   if(GetWorld()->LineTraceSingleByChannel(Contact,Foot+FVector(0,0,20),Foot-FVector(0,0,40),ECC_WorldStatic,Query) && Contact.ImpactNormal.Z>.6f)++Feet;
  }
  bGrounded=Feet>=2;
 }
 // Settled on something the probes cannot call ground (a planter, a wall top, a parked roof): still, upright and
 // not climbing for a moment means it is supported, so the pilot can shut down and step out.
 SlipDir=FVector::ZeroVector;
 if(!bGrounded && RestTime>.8f && Body->GetUpVector().Z>.8f)
 {
  // Supported by something under the belly. Broad support (a wall top, a colonnade roof, a hedge: at least two belly
  // corners on it) is a landing; a narrow one (a lamp post, a bollard, a sign) is not - it slides off beside it.
  const FVector E=Profile->BodyExtent;FHitResult Under;const FTransform& X=Body->GetComponentTransform();
  const FVector Belly=Center-Body->GetUpVector()*(E.Z-10);
  const bool bSupported=GetWorld()->SweepSingleByChannel(Under,Belly,Belly-FVector(0,0,45),Body->GetComponentQuat(),ECC_WorldStatic,
   FCollisionShape::MakeBox(FVector(E.X*.8f,E.Y*.8f,5)),Query);
  int32 Corners=0;
  for(const FVector2D Corner:{FVector2D(.7f,.9f),FVector2D(.7f,-.9f),FVector2D(-.7f,.9f),FVector2D(-.7f,-.9f)})
  {
   const FVector Foot=X.TransformPositionNoScale(FVector(E.X*Corner.X,E.Y*Corner.Y,-E.Z));FHitResult Contact;
   Corners+=GetWorld()->LineTraceSingleByChannel(Contact,Foot+FVector(0,0,40),Foot-FVector(0,0,60),ECC_WorldStatic,Query);
  }
  if(Corners>=2 && (Altitude<120 || bSupported))bGrounded=true;
  else if(bSupported)
  {
   // toward open, dry, level ground beside it (away from the post first, then round the compass)
   const FVector Away=Under.bStartPenetrating?FVector::ZeroVector:(Center-Under.ImpactPoint).GetSafeNormal2D();
   TArray<FVector> Ways;if(!Away.IsNearlyZero())Ways.Add(Away);
   for(int32 K=0;K<8;++K)Ways.Add(FRotator(0,K*45.f,0).RotateVector(X.GetUnitAxis(EAxis::Y)).GetSafeNormal2D());
   for(const FVector& Way:Ways)
   {
    const FVector To=Center+Way*450;FHitResult Floor2;float WaterZ;FVector Cur;
    if(!GetWorld()->LineTraceSingleByChannel(Floor2,To,To-FVector(0,0,2000),ECC_WorldStatic,Query) || Floor2.ImpactNormal.Z<.9f)continue;
    if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Floor2.ImpactPoint,WaterZ,Cur) && WaterZ>Floor2.ImpactPoint.Z-30)continue;
    SlipDir=Way;break;
   }
  }
 }
}
void UAircraftVehicleComponent::ResetAfterLoad()
{
 StopEngine(); if(!Body || !Profile) return;
 if(auto* Water=GetOwner()->FindComponentByClass<UAircraftWaterInteraction>()) Water->UpdateImmersion(Profile->BodyExtent.Z);
 HoverHeight=Body->GetComponentLocation().Z; SampleGround(); SetState(bGrounded && GetCondition()!=EAircraftCondition::Disabled?EAircraftFlightState::Grounded:EAircraftFlightState::Disabled);
}
void UAircraftVehicleComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 if(!Body || !Profile || !GetOwner()->HasAuthority() || !Body->IsSimulatingPhysics() || Delta<=0) return;
 auto* Water=GetOwner()->FindComponentByClass<UAircraftWaterInteraction>(); if(Water) Water->UpdateImmersion(Profile->BodyExtent.Z);
 {
  const FVector V=Body->GetPhysicsLinearVelocity(),A=Body->GetPhysicsAngularVelocityInRadians();
  // only while the pilot is setting it down (collective held down) or with the engine off: a still, low hover is not a landing
  RestTime=V.Size()<25 && A.Size()<.3f && (Input.Z<-.05f || !bEngineRunning)?RestTime+Delta:0;
 }
 SampleGround(); if(bEngineRunning && !CanPower()) StopEngine();
 const FVector Velocity=Body->GetPhysicsLinearVelocity(),Angular=Body->GetPhysicsAngularVelocityInRadians();
 if(Velocity.ContainsNaN() || Angular.ContainsNaN()) { StopEngine(); SetState(EAircraftFlightState::Disabled); return; }
 if(!bEngineRunning)
 {
  SetState(GetCondition()==EAircraftCondition::Disabled || !bGrounded?EAircraftFlightState::Disabled:EAircraftFlightState::Grounded);
  // Drag prevents runaway descent; gravity and contacts stay active after shutdown.
  Body->AddForce(-Velocity*Profile->AirResistance*Profile->Mass); return;
 }
 float VerticalInput=Input.Z;
 // perched on a post with the collective down: hop up clear of it and across, then settle beside it
 if(!SlipDir.IsZero() && VerticalInput<-.05f && SlipTime<=0){SlipTime=2.4f;SlipAway=SlipDir;}
 if(SlipTime>0){SlipTime-=Delta;VerticalInput=SlipTime>1.3f?.8f:0.f;}
 const float Step=FMath::Clamp(Delta,0.f,.1f);
 TargetPower=FMath::Clamp(TargetPower+PowerInput*Profile->PowerAdjustmentRate*Step,Profile->HoverPower,1.f);
 CurrentPower=FMath::FInterpTo(CurrentPower,TargetPower,Step,Profile->PowerResponse);
 const float Boost=FMath::Clamp((CurrentPower-Profile->HoverPower)/(1-Profile->HoverPower),0.f,1.f);
 const bool Climbing=FMath::Abs(VerticalInput)>.05f;
 if(Climbing || bWasClimbing || bGrounded) HoverHeight=Body->GetComponentLocation().Z;
 bWasClimbing=Climbing;
 float DesiredZ=Climbing?VerticalInput*Profile->VerticalSpeed*100:
  FMath::Clamp((HoverHeight-Body->GetComponentLocation().Z)*Profile->HeightResponse,-Profile->VerticalSpeed*100,Profile->VerticalSpeed*100);
 if(DesiredZ<0 && Altitude<250) DesiredZ=FMath::Max(DesiredZ,-Profile->LandingSpeed*100);
 const float Gravity=-GetWorld()->GetGravityZ();
 float LiftAcceleration=FMath::Clamp(Gravity+(DesiredZ-Velocity.Z)*Profile->VerticalResponse,0.f,Profile->LiftForce*100/Profile->Mass);
 if(bGrounded && VerticalInput<=.05f) LiftAcceleration=0;
 // World-up lift is the arcade stability policy. Rotor attitude is still physically simulated.
 Body->AddForce(FVector(0,0,LiftAcceleration*Profile->Mass));
 const FRotator Heading(0,Body->GetComponentRotation().Yaw,0);
 const bool Decelerating=PowerInput<-.05f;
 const FVector Command=SlipTime>0?SlipAway*.45f:Decelerating?FVector::ZeroVector:Heading.RotateVector(FVector(Input.X,Input.Y,0));
 const FVector Horizontal(Velocity.X,Velocity.Y,0);
 HorizontalAcceleration=FMath::FInterpTo(HorizontalAcceleration,(Horizontal.Size()-PreviousSpeed)/FMath::Max(Delta,.001f),Step,4.f);PreviousSpeed=Horizontal.Size();
 FVector Accel=Command*Profile->Acceleration*100*FMath::Lerp(1.f,Profile->FastFlightAccelerationScale,Boost)-Horizontal*Profile->AirResistance;
 if(Command.IsNearlyZero() && !Decelerating) Accel-=Horizontal*Profile->HoverBrake;
 // Controlled deceleration has its own bounded force; do not stack the stronger
 // automatic hover brake on top or reverse velocity when the hull reaches rest.
 if(Decelerating)Accel=-Horizontal.GetSafeNormal()*FMath::Min(Profile->Deceleration*100+Horizontal.Size()*Profile->AirResistance,Horizontal.Size()/FMath::Max(Delta,.001f));
 const float Limit=Profile->MaxSpeed/.036f;
 if(Horizontal.Size()>Limit) Accel-=Horizontal.GetSafeNormal()*(Horizontal.Size()-Limit)*3;
 if(!bGrounded || VerticalInput>.05f) Body->AddForce(Accel*Profile->Mass);
 const FRotator Desired(Decelerating?0:-Input.X*Profile->MaximumBank,Heading.Yaw,Decelerating?0:Input.Y*Profile->MaximumBank);
 // RotationMatrix supplies the target up vector without accumulating rotator integration drift.
 const FVector Up=FRotationMatrix(Desired).GetUnitAxis(EAxis::Z);
 FVector Torque=FVector::CrossProduct(Body->GetUpVector(),Up)*Profile->AttitudeResponse-Angular*Profile->AngularResistance;
 Torque.Z=(FMath::DegreesToRadians(YawInput*Profile->RotationSpeed)-Angular.Z)*Profile->AngularResistance;
 Body->AddTorqueInRadians(Torque,NAME_None,true);
 if(bGrounded && VerticalInput<=.05f) SetState(EAircraftFlightState::Grounded);
 else if(VerticalInput>.05f) SetState(EAircraftFlightState::TakingOff);
 else if(VerticalInput<-.05f) SetState(EAircraftFlightState::Landing);
 else SetState(Horizontal.Size()>120 || Command.SizeSquared()>.01f?EAircraftFlightState::Flying:EAircraftFlightState::Hovering);
}
