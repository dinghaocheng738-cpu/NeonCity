#include "Vehicles/NeonCityVehicleWheel.h"

UNeonCityVehicleWheel::UNeonCityVehicleWheel()
{
	WheelRadius = 32.f;
	WheelWidth = 22.f;
	WheelMass = 20.f;
	AxleType = EAxleType::Rear;
	bAffectedBySteering = false;
	bAffectedByHandbrake = true;
	bABSEnabled = true;
	bTractionControlEnabled = true;
	FrictionForceMultiplier = 2.f;
	CorneringStiffness = 1000.f;
	SuspensionMaxRaise = 10.f;
	SuspensionMaxDrop = 15.f;
	SpringRate = 35000.f;
	SpringPreload = 3500.f;
	SuspensionDampingRatio = 0.5f;
	WheelLoadRatio = 0.65f;
	RollbarScaling = 0.3f;
	MaxBrakeTorque = 1120.f;
	MaxHandBrakeTorque = 2240.f;
	SweepShape = ESweepShape::Spherecast;
}

UNeonCityVehicleFrontWheel::UNeonCityVehicleFrontWheel()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	bAffectedByHandbrake = false;
	MaxSteerAngle = 32.f;
}
