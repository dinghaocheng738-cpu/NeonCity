// Adapted from Tidewater src/player/BoatController.js, commit 4811ba48d795197de5621985f404e765c0b7c0ef.
// Copyright (c) 2026 DRG Software Solutions LLC. MIT; full notice in ReferenceBoatGeometry.h and Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "Vehicles/Marine/ReferenceBoatGeometry.h"

namespace NeonCityReferenceBoat
{
FHydroResult EvaluateHydrodynamics(const FHydroState& S)
{
 FHydroResult Out;
 if(S.WaterHeight.Num()!=UE_ARRAY_COUNT(HullSamples) || S.WaterVerticalVelocity.Num()!=S.WaterHeight.Num()
  || S.Position.ContainsNaN() || S.Velocity.ContainsNaN() || S.Angular.ContainsNaN() || S.Rotation.ContainsNaN()) return Out;
 constexpr double Rho=1025;
 auto Smooth=[](double X,double A,double B) { const double T=FMath::Clamp((X-A)/(B-A),0.,1.);return T*T*(3-2*T); };
 auto Sign=[](double V) { return V>0?1.:V<0?-1.:0.; };
 const FVector COM=S.Position+S.Rotation.RotateVector(CenterOfMassSource);
 FVector F(0,-Mass*S.Gravity,0),T=FVector::ZeroVector;
 auto Point=[&](FVector P) { return S.Position+S.Rotation.RotateVector(P); };
 auto ForceAt=[&](FVector Force,FVector P) { F+=Force;T+=FVector::CrossProduct(P-COM,Force); };
 double WetArea=0,Area=0,Immersion=0,PitchK=0;
 for(int32 I=0;I<UE_ARRAY_COUNT(HullSamples);++I)
 {
  const auto& H=HullSamples[I];const FVector P=Point(H.Position);
  const double Depth=S.WaterHeight[I]-P.Y;
  Area+=H.Area;PitchK+=H.Area*FMath::Square(H.Position.Z-CenterOfMassSource.Z);
  if(Depth<=0) continue;
  const double Sub=FMath::Min(Depth,1.6),Wet=FMath::Min(1.,Depth/.3);
  WetArea+=H.Area*Wet;Immersion+=H.Area*FMath::Min(Sub/FMath::Max(-H.Position.Y,.05),1.5);
  const double VY=(FVector::CrossProduct(S.Angular,P-COM)+S.Velocity).Y-S.WaterVerticalVelocity[I]*.6;
  const double WetK=FMath::Min(1.,Sub/.25)*H.Area;
  ForceAt(FVector(0,Rho*S.Gravity*H.Area*Sub-(1800*VY+900*VY*FMath::Abs(VY))*WetK,0),P);
 }
 Out.WetFraction=Area>0?WetArea/Area:0;
 const double Imm=Area>0?FMath::Clamp(Immersion/Area,0.,1.):0,WetD=FMath::Min(1.,Out.WetFraction*1.6);
 const FVector Forward=S.Rotation.RotateVector(FVector(0,0,1)),Side=S.Rotation.RotateVector(FVector(1,0,0)),Up=S.Rotation.RotateVector(FVector(0,1,0));
 const FVector V=S.Rotation.UnrotateVector(S.Velocity),A=S.Rotation.UnrotateVector(S.Angular);
 const double U=V.Z,VS=V.X,AU=FMath::Abs(U);
 const double Resistance=(40*AU+22*AU*AU+3000*Smooth(AU,2.8,5.4)+55*AU*AU*Smooth(AU,7,11))*WetD;
 ForceAt(Forward*(-Resistance*Sign(U)),Point(FVector(0,-.2,CenterOfMassSource.Z)));
 F+=S.Velocity*(-3.3*S.Velocity.Size());
 const double Stations[][2]={{-3.4,.73},{-2.3,.78},{-1.2,.8},{-.1,.77},{1,.62},{2.1,.33},{3.2,.14}};
 for(const auto& Station:Stations)
 {
  const double VK=VS+A.Y*(Station[0]-CenterOfMassSource.Z)-A.Z*(.06-CenterOfMassSource.Y);
  const double Y=-.5*Rho*Station[1]*(.5*AU*VK+1.1*VK*FMath::Abs(VK))*WetD;
  ForceAt(Side*Y,Point(FVector(0,.06,Station[0])));
 }
 const FVector Prop=Point(FVector(0,-.53,-3.3));
 double BestDistance=TNumericLimits<double>::Max(),PropHeight=0;
 for(int32 I=0;I<UE_ARRAY_COUNT(HullSamples);++I)
 {
  const FVector P=Point(HullSamples[I].Position);const double D=FMath::Square(P.X-Prop.X)+FMath::Square(P.Z-Prop.Z);
  if(D<BestDistance) { BestDistance=D;PropHeight=S.WaterHeight[I]; }
 }
 const double PropWet=FMath::Clamp((PropHeight-Prop.Y)/.3+.5,0.,1.);
 const double Dir=S.Driven?Sign(S.Throttle):0,N=S.RPM*(S.Driven && FMath::Abs(S.Throttle)>.02?1:0);
 double Thrust=0;
 if(Dir>0) Thrust=26000*N*N*FMath::Max(0.,1-FMath::Max(U,0.)/(16*FMath::Max(N,.2)));
 else if(Dir<0) Thrust=-.45*26000*N*N*FMath::Max(0.,1-FMath::Max(-U,0.)/(16*.6*FMath::Max(N,.2)));
 Thrust*=PropWet;Out.Thrust=Thrust;ForceAt(Forward*Thrust,Prop);
 if(Thrust<0) ForceAt(Side*(-Thrust*.08),Prop);
 const FVector Rudder=Point(FVector(0,-.15,-3.58));
 const double Race=FMath::Max(Thrust,0.)*2/(Rho*.14),UR2=U*FMath::Abs(U)+.9*Race,UR=FMath::Sqrt(FMath::Abs(UR2))*Sign(UR2);
 const double VRud=VS+A.Y*(-3.58-CenterOfMassSource.Z);
 const double Delta=S.Steering*.6+(FMath::Abs(UR)>.3?FMath::Atan2(VRud,FMath::Abs(UR))*Sign(UR):0),D=FMath::Clamp(Delta,-.7,.7);
 const double Lift=.5*Rho*.12*FMath::Abs(UR2)*2.8*FMath::Sin(D)*FMath::Cos(D)*PropWet*Sign(UR2==0?1:UR2);
 ForceAt(Side*(-Lift),Rudder);
 ForceAt(Forward*(-FMath::Abs(Lift*FMath::Sin(D))*.8*Sign(U==0?1:U)),Rudder);
 const double Trim=FMath::DegreesToRadians(2.8*Smooth(U,2.5,4.8)-.8*Smooth(U,4.8,7.5)-.4*Smooth(U,7.5,10.5));
 T+=Side*(-Rho*S.Gravity*PitchK*Trim*Imm);
 F+=Up*(Mass*S.Gravity*.15*Smooth(U,3.5,9)*Imm);
 T+=S.Rotation.RotateVector(FVector(-A.X*25000,-A.Y*2000,-A.Z*(4500+900*AU))*(.2+WetD));
 // Tidewater's compliant berth: horizontal spring/damper and yaw restraint.
 // Heave, pitch and roll remain entirely driven by the water and hull contacts.
 if(S.Moored && !S.Driven)
 {
  F.X+=(S.MooringAnchor.X-S.Position.X)*5500-S.Velocity.X*4200;
  F.Z+=(S.MooringAnchor.Z-S.Position.Z)*5500-S.Velocity.Z*4200;
  const double Heading=FMath::Atan2(Forward.X,Forward.Z);
  const double Angle=S.MooringHeading-Heading;
  T.Y+=FMath::Atan2(FMath::Sin(Angle),FMath::Cos(Angle))*60000-S.Angular.Y*30000;
 }
 // The source's added-mass response is retained. Chaos still integrates motion and resolves real contacts.
 const FVector FL=S.Rotation.UnrotateVector(F)/(Mass*(FVector(1,1,1)+FVector(.6,.7,.05)*WetD));
 const FVector TL=S.Rotation.UnrotateVector(T)/(InertiaSource*(FVector(1,1,1)+FVector(1,.4,.2)*WetD));
 Out.Acceleration=S.Rotation.RotateVector(FL);Out.AngularAcceleration=S.Rotation.RotateVector(TL);
 return Out;
}
}
