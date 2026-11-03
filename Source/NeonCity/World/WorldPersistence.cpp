#include "World/NeonCityWorldManager.h"
#include "World/NeonCityDayNightCycle.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Water/CoastalOceanSurface.h"
FNeonCityWorldSaveData UNeonCityWorldManager::BuildPersistenceSnapshot(bool Defaults) const
{
 FNeonCityWorldSaveData Data;
 if(UGameplayStatics::GetCurrentLevelName(GetWorld(),true)==TEXT("NeonCity_SouthBeach_Test")) Data.WorldID=TEXT("NeonCity.SouthBeach");
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It) { Data.TimeOfDay=Defaults?It->GetInitialTimeOfDay():It->GetTimeOfDay(); Data.GameDay=Defaults?0:It->GetGameDay(); break; }
 if(!Defaults)for(TActorIterator<ACoastalOceanSurface> It(GetWorld());It;++It){Data.CoastalSeaState=uint8(It->GetSeaState());break;}
 return Data;
}
bool UNeonCityWorldManager::CanApplyPersistenceSnapshot(const FNeonCityWorldSaveData& Data) const
{
 if(Data.WorldID.IsNone() || Data.WorldID!=BuildPersistenceSnapshot().WorldID || !FMath::IsFinite(Data.TimeOfDay) || Data.TimeOfDay<0 || Data.TimeOfDay>=24) return false;
 int32 Count=0; for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It) ++Count; return Count==1;
}
void UNeonCityWorldManager::ApplyPersistenceSnapshot(const FNeonCityWorldSaveData& Data)
{
 for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It){It->SetTimeOfDay(Data.TimeOfDay);It->SetGameDay(Data.GameDay);}
 // An unsupported optional weather value cannot invalidate player/property data.
 for(TActorIterator<ACoastalOceanSurface> It(GetWorld());It;++It)
  It->SetSeaState(Data.CoastalSeaState<=2?ECoastalSeaState(Data.CoastalSeaState):ECoastalSeaState::Light);
}
