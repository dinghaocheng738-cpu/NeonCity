#include "Vehicles/Aircraft/FixedWing/FixedWingFlightComponent.h"
#include "Vehicles/Aircraft/AircraftWaterInteraction.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"
FAircraftFlightTelemetry UFixedWingFlightComponent::GetTelemetry() const
{
 FAircraftFlightTelemetry T;T.SpeedKmh=Airspeed*3.6f;T.AltitudeMeters=Altitude/100;
 T.CurrentPower=T.TargetPower=Throttle;T.EngineRunning=bEngineRunning;T.FixedWing=true;
 T.Phase=StaticEnum<EFixedWingFlightState>()->GetDisplayNameTextByValue(int64(State));return T;
}
void UFixedWingFlightComponent::Configure(UPrimitiveComponent* InBody,const UAircraftVehicleData* Data)
{
 Super::Configure(InBody,Cast<UFixedWingAircraftData>(Data));
 if(!Body || !Profile) return;
 // Fixed gear uses low rolling friction, with explicit longitudinal brake and lateral tire grip.
 auto* Material=NewObject<UPhysicalMaterial>(Body); Material->Friction=.025f; Material->Restitution=0;
 Material->bOverrideFrictionCombineMode=true; Material->FrictionCombineMode=EFrictionCombineMode::Min;
 Body->SetPhysMaterialOverride(Material); Body->SetLinearDamping(0); Body->SetAngularDamping(.1f);
}
bool UFixedWingFlightComponent::CanPower() const
{
 const auto* Water=GetOwner()->FindComponentByClass<UAircraftWaterInteraction>();
 return Body && Profile && GetOwner()->HasAuthority() && GetFuelFraction()>0 && GetCondition()!=EAircraftCondition::Disabled
  && (!Water || Water->GetWaterState()!=EAircraftWaterState::Submerged);
}
void UFixedWingFlightComponent::StopEngine() { Super::StopEngine(); Throttle=Pitch=Roll=Rudder=0; bBrake=false; }
void UFixedWingFlightComponent::ResetAfterLoad()
{ Super::ResetAfterLoad(); bStalled=bPreviouslyAirborne=false; Airspeed=Lift=0; ChangeState(GetCondition()==EAircraftCondition::Disabled?EFixedWingFlightState::Disabled:EFixedWingFlightState::Grounded); }
void UFixedWingFlightComponent::SetInput(FVector Translation,float Yaw)
{ SetControls(Translation.X,Translation.Z,Translation.Y,Yaw,false); }
bool UFixedWingFlightComponent::SetControls(float T,float P,float R,float Y,bool Brake)
{
 if(!GetOwner()->HasAuthority() || !FMath::IsFinite(T) || !FMath::IsFinite(P) || !FMath::IsFinite(R) || !FMath::IsFinite(Y)) return false;
 Throttle=FMath::Clamp(T,0.f,1.f); Pitch=FMath::Clamp(P,-1.f,1.f); Roll=FMath::Clamp(R,-1.f,1.f); Rudder=FMath::Clamp(Y,-1.f,1.f); bBrake=Brake; return true;
}
float UFixedWingFlightComponent::EvaluateLift(const UFixedWingAircraftData& D,float Speed,float AoA,bool Stalled)
{
 const float CL=FMath::Clamp(D.LiftCoefficient+AoA*D.LiftSlope,-.3f,D.MaximumLiftCoefficient);
 const float StallFactor=Stalled?FMath::Clamp(FMath::Square(Speed/(D.StallSpeed/3.6f))*.4f,0.f,.4f):1.f;
 return .5f*D.AirDensity*FMath::Square(FMath::Max(Speed,0.f))*D.WingArea*CL*StallFactor;
}
void UFixedWingFlightComponent::ChangeState(EFixedWingFlightState Next)
{
 if(State!=Next) { const auto Previous=State; State=Next; OnFixedWingStateChanged.Broadcast(Previous,State); }
 SetState(State==EFixedWingFlightState::Disabled?EAircraftFlightState::Disabled:
  bGrounded?EAircraftFlightState::Grounded:State==EFixedWingFlightState::LandingApproach?EAircraftFlightState::Landing:EAircraftFlightState::Flying);
}
void UFixedWingFlightComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 // Intentionally bypass helicopter forces; lifecycle, health, water and ground sampling stay shared.
 UActorComponent::TickComponent(Delta,Type,Function);
 const auto* D=Cast<UFixedWingAircraftData>(Profile);
 if(!D || !Body || !GetOwner()->HasAuthority() || !Body->IsSimulatingPhysics() || Delta<=0) return;
 if(auto* Water=GetOwner()->FindComponentByClass<UAircraftWaterInteraction>()) Water->UpdateImmersion(D->BodyExtent.Z);
 SampleGround(); if(bEngineRunning && !CanPower()) StopEngine();
 const FTransform Transform=Body->GetComponentTransform();
 const FVector Velocity=Body->GetPhysicsLinearVelocity()/100,Local=Transform.InverseTransformVectorNoScale(Velocity);
 const FVector Angular=Transform.InverseTransformVectorNoScale(Body->GetPhysicsAngularVelocityInRadians());
 if(Velocity.ContainsNaN() || Angular.ContainsNaN()) { StopEngine(); return; }
 Airspeed=FMath::Max(0.f,float(Local.X));
 const float AoA=FMath::Atan2(-Local.Z,FMath::Max(1.f,Airspeed));
 const bool Disabled=GetCondition()==EAircraftCondition::Disabled || !CanPower();
 const float StallSpeed=D->StallSpeed/3.6f;
 // Hysteresis keeps a marginal airspeed from alternating states every frame.
 bStalled=!bGrounded && (bStalled?Airspeed<StallSpeed*1.12f:Airspeed<StallSpeed) && (Pitch>0 || Velocity.Z<0 || Airspeed<StallSpeed*.85f);
 Lift=EvaluateLift(*D,Airspeed,AoA,bStalled);
 if(bGrounded && (Airspeed<D->TakeoffSpeed/3.6f || Pitch<=0)) Lift=FMath::Min(Lift,D->Mass*9.81f*.7f);
 if(Disabled) Lift*=.25f;
 const FVector Forward=Body->GetForwardVector(),Right=Body->GetRightVector(),Up=Body->GetUpVector();
 // Lift is perpendicular to airflow so it cannot add kinetic energy. Gravity remains Chaos gravity.
 const FVector LiftDirection=FVector::VectorPlaneProject(Up,Velocity.GetSafeNormal()).GetSafeNormal();
 const float CL=FMath::Clamp(D->LiftCoefficient+AoA*D->LiftSlope,-.3f,D->MaximumLiftCoefficient);
 const float Drag=.5f*D->AirDensity*Velocity.SizeSquared()*D->WingArea*(D->DragCoefficient+D->InducedDrag*CL*CL);
 FVector Force=LiftDirection*Lift-Velocity.GetSafeNormal()*Drag;
 Force-=Right*Local.Y*D->Mass*(bGrounded?5.f:D->SideSlipDamping);
 const float Speed=Velocity.Size(),Limit=D->MaxSpeed/3.6f;
 if(Speed>Limit) Force-=Velocity.GetSafeNormal()*(Speed-Limit)*D->Mass*D->OverspeedDrag;
 if(bEngineRunning && !Disabled) Force+=Forward*D->EngineThrust*Throttle*(GetCondition()==EAircraftCondition::Critical?.45f:1.f);
 if(bGrounded && (bBrake || !bEngineRunning)) Force-=Forward*FMath::Clamp(float(Local.X)/FMath::Max(Delta,.001f),-D->GroundBrake,D->GroundBrake)*D->Mass;
 Body->AddForce(Force*100);
 FVector DesiredRate=FVector::ZeroVector;
 if(!Disabled && bEngineRunning)
 {
  if(bGrounded)
  {
   DesiredRate.Z=FMath::DegreesToRadians(Rudder*D->GroundSteering)*FMath::Clamp(Airspeed/3.f,0.f,1.f);
   if(Airspeed>=D->TakeoffSpeed/3.6f) DesiredRate.Y=-FMath::DegreesToRadians(FMath::Max(Pitch,0.f)*D->PitchAuthority);
  }
  else
  {
   const float Authority=FMath::Clamp(Airspeed/StallSpeed,0.f,1.f)*(bStalled?D->StallControlFraction:1.f);
   DesiredRate=FVector(-Roll*D->RollAuthority,-Pitch*D->PitchAuthority,Rudder*D->YawAuthority)*PI/180*Authority;
   // Weathervane into relative airflow; banking produces the turn through tilted lift.
   DesiredRate.Z+=FMath::Clamp(float(Local.Y)/FMath::Max(Airspeed,5.f),-.4f,.4f);
  }
 }
 if(bStalled && !bGrounded) DesiredRate.Y+=FMath::DegreesToRadians(D->StallNoseDown);
 Body->AddTorqueInRadians(Transform.TransformVectorNoScale((DesiredRate-Angular)*D->ControlResponse),NAME_None,true);
 EFixedWingFlightState Next;
 if(Disabled) Next=EFixedWingFlightState::Disabled;
 else if(bGrounded) Next=bPreviouslyAirborne && Speed>1?EFixedWingFlightState::Landing:Speed<1?EFixedWingFlightState::Grounded:Throttle>.65f?EFixedWingFlightState::TakeoffRoll:EFixedWingFlightState::Taxiing;
 else if(bStalled) Next=EFixedWingFlightState::Stalling;
 else if(Velocity.Z<-.5f && Altitude<2000) Next=EFixedWingFlightState::LandingApproach;
 else Next=FMath::Abs(Velocity.Z)<2 && Airspeed>=D->CruiseSpeed/3.6f*.85f?EFixedWingFlightState::Cruising:EFixedWingFlightState::Airborne;
 if(!bGrounded && Altitude>100) bPreviouslyAirborne=true;
 if(bGrounded && Speed<1) bPreviouslyAirborne=false;
 ChangeState(Next);
}
