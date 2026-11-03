// Tidewater MIT adaptation. Full notice: Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "Fishing/FishingFightModel.h"

bool FNeonCityFishFightConfig::IsValid() const
{
 return FMath::IsFinite(WeightKg) && WeightKg>0 && WeightKg<=1000
  && FMath::IsFinite(SpeciesMaximumKg) && SpeciesMaximumKg>0 && WeightKg<=SpeciesMaximumKg
  && FMath::IsFinite(SpeciesFight) && SpeciesFight>=0 && SpeciesFight<=1
  && FMath::IsFinite(SpeciesStaminaSeconds) && SpeciesStaminaSeconds>0 && SpeciesStaminaSeconds<=600
  && FMath::IsFinite(LineRatingKg) && LineRatingKg>0 && LineRatingKg<=1000
  && FMath::IsFinite(ReelSpeedMps) && ReelSpeedMps>0 && ReelSpeedMps<=100
  && FMath::IsFinite(DistanceM) && DistanceM>=0 && DistanceM<=10000;
}
double FNeonCityFishFight::Random01()
{ RandomState=RandomState*1664525u+1013904223u;return double(RandomState)/4294967296.; }
bool FNeonCityFishFight::Reset(const FNeonCityFishFightConfig& Config,uint32 Seed)
{
 *this=FNeonCityFishFight();if(!Config.IsValid())return false;
 RandomState=Seed;ReelSpeedMps=Config.ReelSpeedMps;
 Power=FMath::Min(2.2,(.25+.75*Config.SpeciesFight)*FMath::Pow(Config.WeightKg/FMath::Max(Config.LineRatingKg*.5,.2),.55));
 StaminaMax=Config.SpeciesStaminaSeconds*FMath::Pow(FMath::Max(Config.WeightKg/Config.SpeciesMaximumKg,.15),.35);
 DistanceM=Config.DistanceM;MaximumDistanceM=FMath::Max(60.,DistanceM+40.);
 NextSurge=1.2+Random01()*2.;Result=ENeonCityFishFightResult::Fighting;return true;
}
ENeonCityFishFightResult FNeonCityFishFight::Step(double Seconds,bool Reeling)
{
 if(Result!=ENeonCityFishFightResult::Fighting || !FMath::IsFinite(Seconds) || Seconds<=0)return Result;
 // An application stall must not skip player reaction. Normal fixed steps match upstream exactly.
 const double Dt=FMath::Min(Seconds,.1);Elapsed+=Dt;
 const double Tired=1.-Stamina;
 NextSurge-=Dt;
 if(NextSurge<=0 && SurgeRemaining<=0)
 {
  SurgeRemaining=.8+Random01()*1.2*(1.-Tired*.6);
  NextSurge=(2.+Random01()*3.)*(1.+Tired);
 }
 SurgeRemaining-=Dt;
 const double SurgeTarget=SurgeRemaining>0?1.:0.;
 Surge+=(SurgeTarget-Surge)*(1.-FMath::Exp(-Dt*6.));
 const double Pull=Power*(.35+.65*Surge)*(1.-.65*Tired);
 const double Target=Reeling?.25+Pull*.95+.25:Pull*.72;
 const double Rate=Reeling?2.2:3.;
 Tension+=(Target-Tension)*(1.-FMath::Exp(-Dt*Rate));
 if(Reeling)DistanceM-=ReelSpeedMps*Dt*(1.15-.6*Surge*(1.-Tired));
 else DistanceM+=Pull*1.4*Dt;
 DistanceM=FMath::Max(0.,DistanceM);
 const bool InBand=Tension>=BandLow && Tension<=BandHigh;
 Stamina=FMath::Max(0.,Stamina-Dt/StaminaMax*(InBand?1.:.3));
 OverloadSeconds=Tension>1.?OverloadSeconds+Dt:FMath::Max(0.,OverloadSeconds-Dt*2.);
 if(OverloadSeconds>.45)Result=ENeonCityFishFightResult::Snapped;
 else
 {
  SlackSeconds=Tension<.12?SlackSeconds+Dt:FMath::Max(0.,SlackSeconds-Dt*2.);
  if(SlackSeconds>4. || DistanceM>MaximumDistanceM)Result=ENeonCityFishFightResult::Escaped;
  else if(DistanceM<1.2)Result=ENeonCityFishFightResult::Caught;
 }
 return Result;
}
