#include "Adventure/IslandSupercarPresentationData.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"

float UIslandSupercarPresentationData::Phase(float Progress,FVector2D Bounds)
{
 const float T=FMath::Clamp(float((Progress-Bounds.X)/(Bounds.Y-Bounds.X)),0.f,1.f);
 return T*T*(3-2*T);
}
bool UIslandSupercarPresentationData::IsComplete() const
{
 auto Range=[](FVector2D B){return !B.ContainsNaN() && B.X>=0 && B.Y<=1 && B.Y>B.X;};
 if(Body.IsNull() || Chassis.IsNull() || Wheels.Num()!=4
  || (bRequiresRetractableRoof && (Roof.IsNull() || Tonneau.IsNull() || Backlight.IsNull() || Arm.IsNull()
  || Carrier.IsNull() || FixedGuides.IsNull() || RoofMounts.IsNull() || Anchors.Num()!=4 || Leaves.Num()!=2))
  || !FMath::IsFinite(CycleSeconds) || CycleSeconds<=0 || !FMath::IsFinite(ArmLengthCm) || ArmLengthCm<=0
  || !FMath::IsFinite(SafeOperationSpeedKmh) || SafeOperationSpeedKmh<0 || CoverPivotCm.ContainsNaN()
  || !Range(CoverOpenPhase) || !Range(CoverClosePhase) || !Range(PanelTransferPhase) || !Range(CarrierRaisePhase)
  || !Range(CarrierLowerPhase) || !Range(BacklightLowerPhase) || !Range(BacklightAeroPhase))return false;
 for(const auto& W:Wheels)if(W.Mesh.IsNull() || W.Caliper.IsNull() || W.CentreCm.ContainsNaN() || !FMath::IsFinite(W.RadiusCm) || W.RadiusCm<=0 || !FMath::IsFinite(W.AxleRollDegrees))return false;
 for(const auto& A:Anchors)if(A.BodyPinCm.ContainsNaN() || A.RoofPinCm.ContainsNaN())return false;
 for(const auto& L:Leaves)if(L.Mesh.IsNull() || L.CoverLocalHingeCm.ContainsNaN() || !FMath::IsFinite(L.MaximumAngleDegrees) || !Range(L.OpeningPhase) || !Range(L.ClosingPhase))return false;
 for(const float Value:{ArmClosedDegrees,ArmOpenDegrees,CarrierLiftCm,CoverPeakDegrees,BacklightDropCm,BacklightAeroReturnCm,
                       MaximumEngineRPM,FinalDriveRatio,DriverReclineDegrees})if(!FMath::IsFinite(Value))return false;
 if(DriverReclineDegrees<0 || DriverReclineDegrees>40)return false;
 const FVector ClosedArm(ArmLengthCm*FMath::Sin(FMath::DegreesToRadians(ArmClosedDegrees)),0,ArmLengthCm*FMath::Cos(FMath::DegreesToRadians(ArmClosedDegrees)));
 for(const auto& Anchor:Anchors)if(!(Anchor.RoofPinCm-Anchor.BodyPinCm).Equals(ClosedArm,.01))return false;
 for(const float Ratio:ForwardGearRatios)if(!FMath::IsFinite(Ratio) || Ratio<=0)return false;
 for(const FVector V:{CentreOfMassCm,ChassisDimensionsCm,InteractionHalfExtentsCm,DriverHipCm,LeftHandCm,RightHandCm,
                      LeftFootCm,RightFootCm,LeftHeadlightCm,RightHeadlightCm})if(V.ContainsNaN())return false;
 if(ChassisDimensionsCm.GetMin()<=0 || InteractionHalfExtentsCm.GetMin()<=0 || CollisionBoxes.IsEmpty())return false;
 for(const auto& Box:CollisionBoxes)if(Box.CentreCm.ContainsNaN() || Box.DimensionsCm.ContainsNaN() || Box.DimensionsCm.GetMin()<=0)return false;
 return ForwardGearRatios.Num()>0 && FMath::IsFinite(PeakEngineTorqueNm) && PeakEngineTorqueNm>0
  && MaximumEngineRPM>0 && FinalDriveRatio>0 && CarrierLiftCm>=0 && BacklightDropCm>=0;
}
bool UIslandSupercarPresentationData::ResolveRequiredAssets()
{
 ResolvedAssets.Reset();if(!IsComplete())return false;
 auto Keep=[this](UObject* Object){if(Object)ResolvedAssets.Add(Object);return Object!=nullptr;};
 bool Valid=Keep(Chassis.LoadSynchronous());
 Valid=Keep(Body.LoadSynchronous()) && Valid;
 if(bRequiresRetractableRoof)
  for(const auto& Mesh:{Roof,Tonneau,Backlight,Arm,Carrier,FixedGuides,RoofMounts})Valid=Keep(Mesh.LoadSynchronous()) && Valid;
 for(const auto& Wheel:Wheels){Valid=Keep(Wheel.Mesh.LoadSynchronous()) && Valid;Valid=Keep(Wheel.Caliper.LoadSynchronous()) && Valid;}
 for(const auto& Leaf:Leaves)Valid=Keep(Leaf.Mesh.LoadSynchronous()) && Valid;
 if(!Valid)ResolvedAssets.Reset();return Valid;
}
float UIslandSupercarPresentationData::ArmAngleDegrees(float P) const
{return FMath::Lerp(ArmClosedDegrees,ArmOpenDegrees,Phase(P,PanelTransferPhase));}
float UIslandSupercarPresentationData::CarrierHeightCm(float P) const
{return CarrierLiftCm*Phase(P,CarrierRaisePhase)*(1-Phase(P,CarrierLowerPhase));}
FVector UIslandSupercarPresentationData::PanelTranslationCm(float P) const
{
 const float A=FMath::DegreesToRadians(ArmAngleDegrees(P)),C=FMath::DegreesToRadians(ArmClosedDegrees);
 return FVector(ArmLengthCm*(FMath::Sin(A)-FMath::Sin(C)),0,ArmLengthCm*(FMath::Cos(A)-FMath::Cos(C))+CarrierHeightCm(P));
}
