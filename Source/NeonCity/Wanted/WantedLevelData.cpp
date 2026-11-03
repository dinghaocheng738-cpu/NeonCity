#include "Wanted/WantedLevelData.h"
UWantedLevelData::UWantedLevelData()
{
 const int32 Thresholds[]={0,1,3,6,10,15}; const int32 Cars[]={0,0,1,2,2,2};
 for (int32 I=0; I<6; ++I) { FNeonCityWantedLevel L; L.EvidenceThreshold=Thresholds[I]; L.VehicleCount=Cars[I]; L.SearchRadius=600+I*100; Levels.Add(L); }
}
bool UWantedLevelData::IsValidProfile() const
{
 if (Levels.Num()!=6 || Levels[0].EvidenceThreshold!=0 || !FMath::IsFinite(LostSightGrace) || LostSightGrace<1
  || !FMath::IsFinite(DecayInterval) || DecayInterval<1) { return false; }
 for (int32 I=0; I<6; ++I)
 {
  if (Levels[I].VehicleCount<0 || Levels[I].VehicleCount>2 || !FMath::IsFinite(Levels[I].SearchRadius)
   || Levels[I].SearchRadius<100 || Levels[I].SearchRadius>3000 || (I>0 && Levels[I].EvidenceThreshold<=Levels[I-1].EvidenceThreshold)) { return false; }
 }
 return Levels[0].VehicleCount==0 && Levels[1].VehicleCount==0 && Levels.Last().EvidenceThreshold<=10000;
}
