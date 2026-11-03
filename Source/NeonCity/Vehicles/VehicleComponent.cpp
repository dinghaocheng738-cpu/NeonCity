#include "Vehicles/VehicleComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Engine/World.h"
#include "Combat/HealthComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "GameFramework/Actor.h"
#include "Vehicles/VehicleData.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkinnedMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

namespace
{
 // Driving assists (all cars). Grip-limited steering: the most front-wheel angle the tyres can turn into lateral
 // grip at this speed (kinematic angle for GripG plus a tyre slip allowance); full lock stays available in town.
 // Stability control: compares the body's yaw rate with what the steering asks for and pushes back with a yaw
 // moment (as brake-based ESC does) when the tail steps out or the body slides, and eases the throttle.
 constexpr float EscYawGain=9.f,EscSlipGain=6.f,EscSlipThresholdDeg=3.f,EscMinKmh=25.f;
 TAutoConsoleVariable<float> CVarSlipAllowance(TEXT("nc.Vehicle.SteerSlipAllowance"),2.6f,TEXT("Front tyre slip angle (deg) allowed above the kinematic grip-limited steering angle"));
 TAutoConsoleVariable<float> CVarEscGain(TEXT("nc.Vehicle.EscGain"),1.f,TEXT("Stability control strength (0 off)"));
 float SteerRate(float Kmh,bool Returning){return Returning?4.5f:FMath::GetMappedRangeValueClamped(FVector2f(20.f,180.f),FVector2f(3.f,1.1f),Kmh);}
 // UE exposes StopThreshold as a reflected tuning property but not a public
 // C++ setter. It controls automatic SERVICE BRAKING, not native sleep.
 const FFloatProperty* AutoBrakeStopThresholdProperty()
 {
  static const auto* Property=FindFProperty<FFloatProperty>(UChaosWheeledVehicleMovementComponent::StaticClass(),TEXT("StopThreshold"));
  return Property;
 }
}

UVehicleComponent::UVehicleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UVehicleComponent::Configure(UChaosWheeledVehicleMovementComponent* Movement, const UVehicleData* Data, TConstArrayView<float> FittedWheelRadii)
{
	RestoreNativeAutoBrake();
	if(auto* Previous=ChaosMovement.Get();Previous && bNativeParkingBrake)Previous->SetParked(false);
	ChaosMovement = Movement;
	if (!Movement || !Data) { return; }
	auto Safe = [](float Value, float Default, float Min, float Max)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Default;
	};
	const float MassKg = Safe(Data->Weight, 1450.f, 500.f, 3000.f);
	const bool bSportsCar = Data->VehicleType == ENeonCityVehicleType::SportsCar;
 bComfortSteering=bSportsCar;
	const float Acceleration = Safe(Data->Acceleration, 3.5f, 0.5f, bSportsCar ? 12.f : 8.f);
	const float BrakeForce = Safe(Data->BrakeForce, 14000.f, 1000.f, 30000.f);
	const float Grip = Safe(Data->Grip, 2.f, 0.2f, bSportsCar?3.1f:4.f);
	const float Steering = bSportsCar?FMath::Clamp(Safe(Data->Handling,38.f,5.f,45.f),38.f,42.f):Safe(Data->Handling,32.f,5.f,45.f);
	SpeedLimitCmPerSecond = Safe(Data->MaxSpeed, 100.f, 10.f, bSportsCar ? 320.f : 160.f) / 0.036f;
	Movement->Mass = MassKg;
 MaxSteerDeg=Steering;GripG=bSportsCar?1.1f:.9f;WheelbaseCm=0.f;
 // Speed-dependent steering is the grip limit below; the native curve stays flat so the two never stack.
 Movement->SteeringSetup.SteeringCurve.GetRichCurve()->Reset();Movement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(0.f,1.f);Movement->SteeringSetup.SteeringCurve.GetRichCurve()->AddKey(250.f,1.f);   // two keys: Chaos sizes its table from the curve range
 // Linear wheel angle: the Ackermann linkage model barely moves the wheels for small inputs (10 % input gave
 // 0.25 deg), which made fine motorway steering impossible and full lock the only way to turn.
 Movement->SteeringSetup.SteeringType=ESteeringType::SingleAngle;
 // Chaos squares the steering input by default (10 % became 1 %): a light touch did nothing at speed and the only
 // response came from a big input past the grip limit, which spun the car. Linear input; this component slews it.
 Movement->SteeringInputRate.InputCurveFunction=EInputFunctionType::LinearFunction;Movement->SteeringInputRate.RiseRate=10.f;Movement->SteeringInputRate.FallRate=10.f;
	// Force = mass * acceleration. Peak engine torque accounts for first/final gearing.
	const float FirstRatio = Movement->TransmissionSetup.ForwardGearRatios.Num()
		? Movement->TransmissionSetup.ForwardGearRatios[0] : 2.85f;
	const float Reduction = FMath::Max(FirstRatio * Movement->TransmissionSetup.FinalRatio
		* Movement->TransmissionSetup.TransmissionEfficiency, 1.f);
	const auto RadiusCm = [&](int32 Index)
	{
		if (FittedWheelRadii.IsValidIndex(Index) && FMath::IsFinite(FittedWheelRadii[Index]) && FittedWheelRadii[Index] >= 10.f && FittedWheelRadii[Index] <= 100.f)
			return FittedWheelRadii[Index];
		return Movement->Wheels.IsValidIndex(Index) && Movement->Wheels[Index] ? Movement->Wheels[Index]->WheelRadius : 32.f;
	};
	const float RadiusMeters = RadiusCm(0) / 100.f;
	Movement->EngineSetup.MaxTorque = MassKg * Acceleration * RadiusMeters / Reduction;
	// Rebuild once at initialization so Chaos recalculates sprung mass and damping.
	Movement->RecreatePhysicsState();
	Movement->SetMaxEngineTorque(Movement->EngineSetup.MaxTorque);
	for (int32 Index = 0; Index < Movement->Wheels.Num(); ++Index)
	{
		const float ActualRadiusCm = RadiusCm(Index);
		Movement->SetWheelRadius(Index, ActualRadiusCm);
		if (Movement->Wheels[Index]) Movement->Wheels[Index]->WheelRadius = ActualRadiusCm;
  // The Chaos runtime setter updates simulation, not the public wheel descriptor.
  // Keep the descriptor truthful for assistance and diagnostics.
  if(Movement->Wheels[Index])Movement->Wheels[Index]->MaxSteerAngle=Index<2?Steering:0.f;
		Movement->SetWheelFrictionMultiplier(Index, Grip);
		Movement->SetWheelMaxBrakeTorque(Index, BrakeForce * ActualRadiusCm / 400.f);
		Movement->SetWheelHandbrakeTorque(Index, BrakeForce * ActualRadiusCm / 200.f);
		Movement->SetWheelMaxSteerAngle(Index, Index < 2 ? Steering : 0.f);
	}
}

void UVehicleComponent::SetDriveInput(float Value) { DriveInput = FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.f, 1.f) : 0.f; }
void UVehicleComponent::SetSteerInput(float Value) { SteerInput = FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.f, 1.f) : 0.f; }
void UVehicleComponent::SetBrakeInput(float Value) { BrakeInput = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 0.f; }

bool UVehicleComponent::CanUseNativeParking() const
{
 const auto* Owner=GetOwner();if(!Owner)return false;
 const auto* Health=Owner->FindComponentByClass<UHealthComponent>();
 const auto* Damage=Owner->FindComponentByClass<UVehicleDamageComponent>();
 return bNativeParkingBrake && !bDrivingEnabled && !bMechanicalMotionInterlock && !bDeepWaterDisabled
  && (!Health || !Health->IsDead()) && (!Damage || Damage->GetDrivePowerMultiplier()>0);
}

void UVehicleComponent::RestoreNativeAutoBrake()
{
 if(bAutoBrakeSuppressed)if(auto* Movement=AutoBrakeMovement.Get())if(const auto* Property=AutoBrakeStopThresholdProperty())
  Property->SetPropertyValue_InContainer(Movement,OriginalAutoBrakeStopThreshold);
 bAutoBrakeSuppressed=false;AutoBrakeMovement.Reset();
}

void UVehicleComponent::ApplyNativeParkingPolicy(bool Parked)
{
 auto* Movement=ChaosMovement.Get();
 if(!Movement || !Parked){RestoreNativeAutoBrake();if(Movement && bNativeParkingBrake)Movement->SetParked(false);return;}
 if(!bAutoBrakeSuppressed)
 {
  const auto* Property=AutoBrakeStopThresholdProperty();if(!Property)return;
  const float Previous=Property->GetPropertyValue_InContainer(Movement);
  if(!FMath::IsFinite(Previous) || Previous<0)return;
  OriginalAutoBrakeStopThreshold=Previous;AutoBrakeMovement=Movement;bAutoBrakeSuppressed=true;
  // CalcThrottleBrakeInput otherwise synthesizes BrakeOut=1 at rest even
  // with rawBrake=0. Its strict speed<0 && speed>0 predicate is impossible.
  // Handbrake/native parking, wheel contacts, slope and sleep thresholds stay
  // unchanged. Preserve and restore the exact authored value on every exit.
  Property->SetPropertyValue_InContainer(Movement,0.f);
 }
 Movement->SetParked(true);
}

void UVehicleComponent::SetNativeParkingBrake(bool Enabled)
{
 if(!Enabled)
 {
  RestoreNativeAutoBrake();if(auto* Movement=ChaosMovement.Get())Movement->SetParked(false);
 }
 bNativeParkingBrake=Enabled;
 if(Enabled){RefreshEngineState();ApplyNativeParkingPolicy(CanUseNativeParking());}
}

void UVehicleComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 RestoreNativeAutoBrake();Super::EndPlay(Reason);
}

void UVehicleComponent::SetMechanicalMotionInterlock(bool bLocked)
{
	bMechanicalMotionInterlock = bLocked;
	RefreshEngineState();ApplyNativeParkingPolicy(CanUseNativeParking());
	// A request/load can occur after this component's tick. Hold immediately as
	// well as at consumption, without introducing a dependency on attached meshes.
	if (bLocked) if (auto* Movement = ChaosMovement.Get())
	{
		Movement->SetThrottleInput(0.f);
		Movement->SetBrakeInput(1.f);
		Movement->SetHandbrakeInput(true);
	}
}

void UVehicleComponent::SetDrivingEnabled(bool bEnabled)
{
	bDrivingEnabled = bEnabled;
 RefreshEngineState();
	DriveInput = SteerInput = BrakeInput = AppliedSteerInput = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Movement = ChaosMovement.Get())
	{
		const bool Parked=CanUseNativeParking();ApplyNativeParkingPolicy(Parked);
		Movement->SetThrottleInput(0.f);
		Movement->SetSteeringInput(0.f);
		Movement->SetBrakeInput(!bMechanicalMotionInterlock && (bEnabled || Parked) ? 0.f : 1.f);
		Movement->SetHandbrakeInput(!bEnabled || bMechanicalMotionInterlock);
	}
}

float UVehicleComponent::GetSpeedKmh() const
{
	return ChaosMovement.IsValid() ? ChaosMovement->GetForwardSpeed() * 0.036f : 0.f;
}

void UVehicleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	UChaosWheeledVehicleMovementComponent* Movement = ChaosMovement.Get();
	if (!Movement) { return; }
	if (auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead()) { bDrivingEnabled=false; }
	RefreshEngineState();
	if (bMechanicalMotionInterlock || bDeepWaterDisabled || !bDrivingEnabled || (GetOwner()->FindComponentByClass<UVehicleDamageComponent>() && GetOwner()->FindComponentByClass<UVehicleDamageComponent>()->GetDrivePowerMultiplier()==0))
	{
		Movement->SetThrottleInput(0.f);
		// A held service brake is a wake input in Chaos ProcessSleeping.
		// Only an unoccupied, healthy opt-in car uses native parking instead.
		// Roof interlocks, deep water and damage retain full service braking.
  const bool Parked=CanUseNativeParking();ApplyNativeParkingPolicy(Parked);
		Movement->SetBrakeInput(Parked?0.f:1.f);
		Movement->SetHandbrakeInput(true);
		return;
	}
 ApplyNativeParkingPolicy(false);
	const float Speed = Movement->GetForwardSpeed();
	const bool bReverse = DriveInput < -KINDA_SMALL_NUMBER;
	const bool bOpposingMotion = (bReverse && Speed > 40.f) || (DriveInput > 0.f && Speed < -40.f);
	float Brake = BrakeInput;
	float Throttle = FMath::Abs(DriveInput);
	if (bOpposingMotion) { Brake = 1.f; Throttle = 0.f; }
	else if (Throttle > 0.f)
	{
		if (bReverse && Movement->GetTargetGear() >= 0) { Movement->SetTargetGear(-1, true); }
		else if (!bReverse && Movement->GetTargetGear() <= 0) { Movement->SetTargetGear(1, true); }
	}
	const float Limit = bReverse ? FMath::Min(SpeedLimitCmPerSecond, 600.f) : SpeedLimitCmPerSecond;
	// Soft governor via throttle/brakes, preserving actual suspension/tire simulation.
	Throttle *= FMath::Clamp((Limit - FMath::Abs(Speed)) / 150.f, 0.f, 1.f);
	if (FMath::Abs(Speed) > Limit) { Brake = FMath::Max(Brake, FMath::Clamp((FMath::Abs(Speed) - Limit) / 200.f, 0.f, 1.f)); }
	if (Brake > 0.f) { Throttle = 0.f; }
	if(auto* Damage=GetOwner()->FindComponentByClass<UVehicleDamageComponent>()) Throttle*=Damage->GetDrivePowerMultiplier();
	StabilityAssist(*Movement,Speed,DeltaTime,Throttle);
	Movement->SetThrottleInput(Throttle);
	Movement->SetBrakeInput(Brake);
	Movement->SetHandbrakeInput(false);
 // Keyboard steering slews instead of snapping to full lock (slower at speed) and never asks the front tyres
 // for more angle than they can hold at this speed. Chassis, suspension and tyre forces stay with Chaos.
 const float SpeedKmh=FMath::Abs(Speed)*.036f;
 const bool Returning=FMath::Abs(SteerInput)<FMath::Abs(AppliedSteerInput) || SteerInput*AppliedSteerInput<0;
 AppliedSteerInput=FMath::FInterpConstantTo(AppliedSteerInput,SteerInput,DeltaTime,SteerRate(SpeedKmh,Returning));
 const float GripLimit=SteeringLimit(Speed);
	Movement->SetSteeringInput(FMath::Clamp(AppliedSteerInput,-GripLimit,GripLimit));
}

float UVehicleComponent::SteeringLimit(float SpeedCm) const
{
 const float V=FMath::Abs(SpeedCm)/100.f;
 if(V<1.f || WheelbaseCm<=0.f || MaxSteerDeg<=1.f)return 1.f;
 const float Kinematic=FMath::RadiansToDegrees(FMath::Atan(WheelbaseCm/100.f*GripG*9.81f/(V*V)));
 return FMath::Clamp((Kinematic+CVarSlipAllowance.GetValueOnGameThread())/MaxSteerDeg,.06f,1.f);
}

void UVehicleComponent::StabilityAssist(UChaosWheeledVehicleMovementComponent& Movement,float SpeedCm,float DeltaTime,float& Throttle)
{
 StabilityLevel=FMath::FInterpTo(StabilityLevel,0.f,DeltaTime,6.f);
 auto* Body=Cast<UPrimitiveComponent>(Movement.UpdatedPrimitive);
 if(!Body || !Body->IsSimulatingPhysics())return;
 if(WheelbaseCm<=0.f)
 {
  // wheelbase from the wheel bones (front pair to rear pair), once
  if(auto* Mesh=Cast<USkinnedMeshComponent>(Body);Mesh && Movement.WheelSetups.Num()>=4)
  {
   auto X=[&](int32 I){return Mesh->GetBoneLocation(Movement.WheelSetups[I].BoneName,EBoneSpaces::ComponentSpace).X;};
   WheelbaseCm=FMath::Abs((X(0)+X(1))*.5f-(X(2)+X(3))*.5f);
  }
  if(WheelbaseCm<200.f || WheelbaseCm>350.f)WheelbaseCm=270.f;
 }
 int32 Contact=0;for(int32 I=0;I<Movement.Wheels.Num();++I)if(Movement.GetWheelState(I).bInContact)++Contact;
 if(FMath::Abs(SpeedCm)*.036f<EscMinKmh || Contact<2)return;                              // airborne: nothing to push against
 const FTransform T=Body->GetComponentTransform();
 const FVector V=T.InverseTransformVectorNoScale(Body->GetPhysicsLinearVelocity());
 const float Forward=FMath::Max(100.f,float(FMath::Abs(V.X)));
 const float Slip=FMath::RadiansToDegrees(FMath::Atan2(float(V.Y),Forward));               // body slip (+ = sliding right)
 const float Yaw=T.InverseTransformVectorNoScale(Body->GetPhysicsAngularVelocityInRadians()).Z;   // rad/s (+ = nose right)
 // the yaw rate the driver asks for: the wheel angle on a neutral car, capped by the grip available
 const float Delta=FMath::DegreesToRadians(Movement.GetSteeringInput()*MaxSteerDeg)*(V.X>=0?1.f:-1.f);
 const float Vm=Forward/100.f,MaxYaw=GripG*9.81f/Vm;
 const float Wanted=FMath::Clamp(Vm*FMath::Tan(Delta)/(WheelbaseCm/100.f),-MaxYaw,MaxYaw);
 float Excess=0.f;
 if(Yaw*Wanted<0.f && FMath::Abs(Yaw)>.05f)Excess=Yaw;                                   // rotating against the steering: a spin starting
 else if(FMath::Abs(Yaw)>FMath::Abs(Wanted)+.03f)Excess=Yaw-Wanted;                       // rotating faster than asked: oversteer
 // a sliding body: turn the nose back toward where the car is actually going
 const float SlipExcess=FMath::Sign(Slip)*FMath::Max(0.f,FMath::Abs(Slip)-EscSlipThresholdDeg);
 const float YawAccel=FMath::Clamp((-EscYawGain*Excess+EscSlipGain*FMath::DegreesToRadians(SlipExcess))*CVarEscGain.GetValueOnGameThread(),-6.f,6.f);
 if(FMath::Abs(YawAccel)<1e-3f)return;
 Body->AddTorqueInRadians(T.TransformVectorNoScale(FVector(0,0,YawAccel)),NAME_None,true);
 StabilityLevel=FMath::Max(StabilityLevel,FMath::Clamp(FMath::Abs(YawAccel)/3.f,0.f,1.f));
 Throttle*=1.f-.7f*StabilityLevel;                                                         // and ease the power
}

float UVehicleComponent::GetEngineRPM() const { return ChaosMovement.IsValid() && EngineState==ENeonCityEngineState::Running?ChaosMovement->GetEngineRotationSpeed():0; }
void UVehicleComponent::RefreshEngineState()
{
 const auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 const auto* Damage=GetOwner()->FindComponentByClass<UVehicleDamageComponent>();
 bDeepWaterDisabled=GetWorld() && GetWorld()->GetSubsystem<UMarineWorldSubsystem>()
  && GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->IsDeepWater(GetOwner()->GetActorLocation());
 const auto Next=bDeepWaterDisabled || (Health && Health->IsDead()) || (Damage && Damage->GetDrivePowerMultiplier()==0)?ENeonCityEngineState::Stalled:bDrivingEnabled?ENeonCityEngineState::Running:ENeonCityEngineState::Off;
 if(Next!=EngineState) { EngineState=Next; OnEngineStateChanged.Broadcast(Next); }
}
