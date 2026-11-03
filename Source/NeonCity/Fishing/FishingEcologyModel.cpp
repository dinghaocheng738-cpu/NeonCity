// Tidewater MIT adaptation. Full notice: Docs/ThirdParty/Tidewater-LICENSE.txt.
#include "Fishing/FishingEcologyModel.h"
#include <limits>

namespace
{
 double Smooth(double A,double B,double X)
 { const double T=FMath::Clamp((X-A)/(B-A),0.,1.);return T*T*(3.-2.*T); }
 double HourOfDay(double Hour) { return FMath::Fmod(FMath::Fmod(Hour,24.)+24.,24.); }
}
double FNeonCityFishingHabitat::Dot(const FNeonCityFishingHabitat& Other) const
{ return Shallows*Other.Shallows+Reef*Other.Reef+Pier*Other.Pier+Bay*Other.Bay+Deep*Other.Deep; }
FNeonCityFishingHabitat NeonCityFishingEcology::HabitatAt(double DepthM,double ReefEdgeDistanceM,double PierDistanceM)
{
 FNeonCityFishingHabitat H;
 if(!FMath::IsFinite(DepthM) || !FMath::IsFinite(ReefEdgeDistanceM) || !FMath::IsFinite(PierDistanceM) || DepthM<.25)return H;
 H.Shallows=Smooth(3.5,1.,DepthM);
 H.Reef=Smooth(12.,-6.,ReefEdgeDistanceM)*Smooth(.8,2.5,DepthM);
 H.Pier=Smooth(9.,2.,PierDistanceM)*Smooth(.6,2.,DepthM);
 H.Bay=Smooth(1.5,4.,DepthM)*(1.-Smooth(18.,30.,DepthM));
 H.Deep=Smooth(16.,28.,DepthM);
 return H;
}
double NeonCityFishingEcology::Activity(ENeonCityFishActivity Preference,double Hour)
{
 if(!FMath::IsFinite(Hour))return 0;
 Hour=HourOfDay(Hour);
 const double Dawn=FMath::Exp(-FMath::Square(Hour-6.5)/2.5),Dusk=FMath::Exp(-FMath::Square(Hour-18.5)/2.5);
 const double Night=(Hour<5.5 || Hour>19.5)?1.:0.,Day=(Hour>7. && Hour<18.)?1.:.35;
 switch(Preference)
 {
 case ENeonCityFishActivity::Day:return .25+.75*Day*(1.-Night);
 case ENeonCityFishActivity::DawnDusk:return .3+.7*FMath::Max(Dawn,Dusk)+.1*Day;
 case ENeonCityFishActivity::Night:return .15+.85*FMath::Max(Night,Dusk*.8);
 default:return .8+.2*FMath::Max(Dawn,Dusk);
 }
}
double NeonCityFishingEcology::BiteDelay(const FNeonCityFishingHabitat& Habitat,double Hour,double Random01)
{
 double Rich=Habitat.Richness();
 if(!FMath::IsFinite(Rich) || Rich<.05 || !FMath::IsFinite(Hour) || !FMath::IsFinite(Random01))return std::numeric_limits<double>::infinity();
 Rich=FMath::Max(Rich,.35);Hour=HourOfDay(Hour);
 const double Light=(Hour>6. && Hour<19.)?1.:.8;
 const double Mean=8./(FMath::Min(Rich,1.6)*Light);
 return 2.-FMath::Loge(1.-FMath::Clamp(Random01,0.,1.)*.98)*Mean*.6;
}
double NeonCityFishingEcology::RollWeight(double MinimumKg,double MaximumKg,double Random01)
{
 if(!FMath::IsFinite(MinimumKg) || !FMath::IsFinite(MaximumKg) || !FMath::IsFinite(Random01) || MinimumKg<=0 || MaximumKg<MinimumKg)return 0;
 return MinimumKg+(MaximumKg-MinimumKg)*FMath::Pow(FMath::Clamp(Random01,0.,1.),2.2);
}
double NeonCityFishingEcology::LengthCm(double WeightKg,double CoefficientA,double ExponentB)
{
 if(!FMath::IsFinite(WeightKg) || !FMath::IsFinite(CoefficientA) || !FMath::IsFinite(ExponentB) || WeightKg<=0 || CoefficientA<=0 || ExponentB<=0)return 0;
 return FMath::Pow(FMath::Max(WeightKg,.001)*1000./CoefficientA,1./ExponentB);
}
int64 NeonCityFishingEcology::SaleValue(double WeightKg,double MinimumKg,double MaximumKg,double PricePerKg)
{
 if(!FMath::IsFinite(WeightKg) || !FMath::IsFinite(MinimumKg) || !FMath::IsFinite(MaximumKg) || !FMath::IsFinite(PricePerKg)
  || WeightKg<=0 || MinimumKg<=0 || MaximumKg<MinimumKg || PricePerKg<=0)return 0;
 const double T=(WeightKg-MinimumKg)/FMath::Max(MaximumKg-MinimumKg,1e-6);
 const double Value=PricePerKg*WeightKg*(1.+.25*FMath::Max(0.,T-.7)/.3);
 // Keep malformed/untrusted data from overflowing an economy transaction.
 return FMath::IsFinite(Value) && Value<1e12?FMath::Max<int64>(1,FMath::RoundToInt64(Value)):0;
}
