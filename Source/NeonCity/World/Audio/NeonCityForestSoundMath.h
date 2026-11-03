#pragma once
#include "CoreMinimal.h"
#include "World/Audio/NeonCityReferenceCoast.h"

// Deterministic section of Tidewater SoundScape.js (MIT, DRG 2026).
// Full notice: Docs/ThirdParty/Tidewater-LICENSE.txt. Raw target gains precede
// each source recording's measured LUFS/channel correction and master volume.
namespace NeonCityForestSound
{
 struct FEnvironment
 {
  double Hour=12,Day=1,DistanceInland=30,Wind=7;
  bool bOnLand=true;
 };
 struct FMix
 {
  double Wind=0,Palms=0,Crickets=0,Dawn=0,WindLowPass=400;
  double BirdActivity=0,Lull=0,Habitat=0;
 };
 inline double Db(double X){return FMath::Pow(10.,X/20);}
 inline double SourceGain(double TargetLufs,double RecordedLufs,int32 Channels)
 {return Db(FMath::Clamp(TargetLufs-RecordedLufs,-80.,24.))*(Channels==1?Db(-3):1);}
 inline FMix Evaluate(const FEnvironment& E,double Gust)
 {
  using NeonCityReferenceCoast::Smooth;
  FMix M;const double D=E.DistanceInland;
  M.Wind=Db(-36)*FMath::Clamp(E.Wind/7,0.,2.5)*Gust;
  M.WindLowPass=400+(80+60*Gust)*E.Wind;
  M.Palms=Db(-39)*(E.bOnLand?Smooth(6,30,D):0)*FMath::Clamp(E.Wind/7,.2,1.8)*Gust*(.3+.7*E.Day);
  M.Crickets=Db(-33)*Smooth(.3,.12,E.Day)*(E.bOnLand?.35+.65*Smooth(5,40,D):.1);
  const double H=E.Hour;
  const double On=Smooth(4.9,5.7,H)*(1-Smooth(18.2,19.3,H));
  const double Chorus=Smooth(5,5.8,H)*(1-Smooth(6.6,7.9,H));
  M.Lull=Smooth(10.5,12,H)*(1-Smooth(14.5,16,H));
  M.BirdActivity=On*FMath::Max(0.,1+2.5*Chorus-.6*M.Lull-.55*Smooth(17.2,18.6,H));
  const double Inland=E.bOnLand?Smooth(0,45,D):0;
  M.Habitat=E.bOnLand?.25+.75*Inland:.25*(1-Smooth(20,90,D));
  M.Dawn=Db(-35)*On*Chorus*(E.bOnLand?.3+.7*Inland:.3*(1-Smooth(20,150,D)));
  return M;
 }
 inline double AdvanceGust(double Value,double Target,double Delta)
 {return Value+(Target-Value)*(1-FMath::Exp(-FMath::Clamp(Delta,0.,.25)/1.4));}
}
