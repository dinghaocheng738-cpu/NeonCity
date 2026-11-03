#include "Police/PoliceResponseData.h"
UPoliceResponseData::UPoliceResponseData()
{
 const int32 Officers[]={1,2,3,4,5,6},Cars[]={0,0,1,2,3,4},Blocks[]={0,0,0,1,1,2};
 for(int32 I=0;I<6;++I) { FPoliceResponseBudget B; B.Officers=Officers[I]; B.Vehicles=Cars[I]; B.Roadblocks=Blocks[I]; B.SearchRadius=600+I*200; B.ChaseSpeed=1100+I*150; Levels.Add(B); }
 Severity={{EPoliceOffense::Attention,1},{EPoliceOffense::Assault,3},{EPoliceOffense::WeaponAttack,1},{EPoliceOffense::VehicleHit,3},{EPoliceOffense::PropertyDamage,1},{EPoliceOffense::PoliceAttack,6},{EPoliceOffense::VehicleDestruction,4},{EPoliceOffense::PublicDanger,2}};
}
int32 UPoliceResponseData::GetSeverity(EPoliceOffense O,float Damage) const
{
 const int32* Value=Severity.Find(O); const int32 Base=Value?*Value:1;
 // A glancing/minor hit still matters, but an armed injury above 10 HP prompts the full police response.
 return FMath::Clamp(O==EPoliceOffense::PoliceAttack && Damage<10?FMath::Min(Base,3):Base,1,10);
}
bool UPoliceResponseData::IsValidProfile() const
{
 if(RifleMinimumLevel<3 || RifleMinimumLevel>5 || RifleEveryNthOfficer<3 || RifleEveryNthOfficer>8) return false;
 if(Levels.Num()!=6 || !FMath::IsFinite(ReplacementInterval) || ReplacementInterval<2 || ReplacementInterval>30
  || !FMath::IsFinite(KnownVehicleIdentificationSeconds) || KnownVehicleIdentificationSeconds<.1f || KnownVehicleIdentificationSeconds>2
  || !FMath::IsFinite(PersonIdentificationSeconds) || PersonIdentificationSeconds<KnownVehicleIdentificationSeconds || PersonIdentificationSeconds>3
  || !FMath::IsFinite(EvidenceMemorySeconds) || EvidenceMemorySeconds<15 || EvidenceMemorySeconds>120) return false;
 for(int32 I=0;I<6;++I) { const auto& B=Levels[I]; if(B.Officers<1 || B.Officers>6 || B.Vehicles<0 || B.Vehicles>4 || B.Roadblocks<0 || B.Roadblocks>2 || B.Roadblocks>B.Vehicles || B.Roadblocks>B.Officers || !FMath::IsFinite(B.SearchRadius) || B.SearchRadius<100 || B.SearchRadius>3000 || !FMath::IsFinite(B.ChaseSpeed) || B.ChaseSpeed<300 || B.ChaseSpeed>2000) return false; }
 for(const auto& S:Severity) if(S.Value<1 || S.Value>10) return false;
 return Levels[0].Vehicles==0 && Levels[0].Roadblocks==0 && Levels[1].Vehicles==0 && Levels[2].Roadblocks==0;
}
