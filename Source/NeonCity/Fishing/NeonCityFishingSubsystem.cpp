#include "Fishing/NeonCityFishingSubsystem.h"
#include "Fishing/FishingStation.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UNeonCityFishingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);Collection.InitializeDependency<UNeonCityEconomySubsystem>();
 for(const TCHAR* ID:{TEXT("Silverfin"),TEXT("CoastBream"),TEXT("CoralSnapper"),TEXT("Barramundi"),TEXT("CoastalShark"),
  TEXT("Sardine"),TEXT("RedNapedWrasse"),TEXT("Flyingfish"),TEXT("Lionfish"),TEXT("Parrotfish"),
  TEXT("Seabream"),TEXT("YellowfinTuna"),TEXT("SpinyLobster"),TEXT("SpearLobster"),
  TEXT("SailfinPoacher"),TEXT("SmoothHound"),TEXT("SmoothHammerhead"),TEXT("Goosefish"),TEXT("GrassPuffer"),
  TEXT("IzuScorpionfish"),TEXT("GizzardShad"),TEXT("PacificHerring"),TEXT("SnootyWrasse"),TEXT("SandSeaStar")})
 {
  const FString Path=FString::Printf(TEXT("/Game/NeonCity/Fishing/Data/DA_Fish_%s.DA_Fish_%s"),ID,ID);
  auto* Profile=LoadObject<UFishData>(nullptr,*Path);
  if(Profile && Profile->IsValidProfile() && !FindSpecies(Profile->SpeciesID))Species.Add(Profile);
 }
}
const UFishData* UNeonCityFishingSubsystem::FindSpecies(FName ID) const
{ for(const UFishData* Profile:Species)if(Profile && Profile->SpeciesID==ID)return Profile;return nullptr; }
const FNeonCityCaughtFish* UNeonCityFishingSubsystem::FindFish(FGuid ID) const
{ return Fish.FindByPredicate([&](const auto& Record){return Record.FishID==ID;}); }
int32 UNeonCityFishingSubsystem::PouchCount() const
{ int32 Count=0;for(const auto& Record:Fish)if(Record.Storage==ENeonCityFishStorage::Pouch)++Count;return Count; }
bool UNeonCityFishingSubsystem::ValidFish(const FNeonCityCaughtFish& Record) const
{
 return Record.FishID.IsValid() && FindSpecies(Record.SpeciesID) && !Record.FishingPointID.IsNone()
  && Record.CaughtAt.GetTicks()>0 && Record.CaughtAt<=FDateTime::MaxValue()
  && FMath::IsFinite(Record.WeightKg) && Record.WeightKg>0 && Record.WeightKg<=250
  && FMath::IsFinite(Record.SizeCm) && Record.SizeCm>0 && Record.SizeCm<=450
  && uint8(Record.Storage)<=uint8(ENeonCityFishStorage::Grill)
  && uint8(Record.GrillStage)<=uint8(ENeonCityGrillStage::Closing)
  && FMath::IsFinite(Record.GrillStageSeconds) && Record.GrillStageSeconds>=0 && Record.GrillStageSeconds<=120
  && FMath::IsFinite(Record.CookFraction) && Record.CookFraction>=0 && Record.CookFraction<=1;
}
FNeonCityCaughtFish UNeonCityFishingSubsystem::SampleCatch(FRandomStream& Random,FName Habitat,float TimeOfDay,FName Weather,FName PointID) const
{
 FNeonCityCaughtFish Result;if(PointID.IsNone())return Result;
 float Total=0;for(const UFishData* Profile:Species)Total+=Profile->EncounterWeight(Habitat,TimeOfDay,Weather);
 if(Total<=0)return Result;
 float Roll=Random.FRand()*Total;const UFishData* Selected=nullptr;
 for(const UFishData* Profile:Species)
 {
  const float Weight=Profile->EncounterWeight(Habitat,TimeOfDay,Weather);if(Weight<=0)continue;
  Selected=Profile;Roll-=Weight;if(Roll<=0)break;
 }
 if(!Selected)return Result;
 const float SizeFactor=Random.FRand();Result.FishID=FGuid::NewGuid();Result.SpeciesID=Selected->SpeciesID;
 Result.WeightKg=FMath::Lerp(float(Selected->WeightKg.X),float(Selected->WeightKg.Y),SizeFactor);
 Result.SizeCm=FMath::Lerp(float(Selected->SizeCm.X),float(Selected->SizeCm.Y),SizeFactor);
 Result.CaughtAt=FDateTime::UtcNow();Result.FishingPointID=PointID;return Result;
}
bool UNeonCityFishingSubsystem::StoreCatch(const FNeonCityCaughtFish& Catch)
{
 if(bChangingInventory || !HasPouchSpace() || !ValidFish(Catch) || FindFish(Catch.FishID)
  || Meals.ContainsByPredicate([&](const auto& M){return M.SourceFishID==Catch.FishID;})
  || Catch.Storage!=ENeonCityFishStorage::Pouch || !Catch.ContainerID.IsNone())return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Fish.Add(Catch);OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::ReleaseFish(FGuid ID)
{
 const int32 Index=Fish.IndexOfByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Pouch;});
 if(bChangingInventory || Index==INDEX_NONE)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Fish.RemoveAt(Index);OnInventoryChanged.Broadcast();return true;
}
int64 UNeonCityFishingSubsystem::GetSaleValue(const FNeonCityCaughtFish& Catch) const
{
 const auto* Profile=FindSpecies(Catch.SpeciesID);if(!Profile || !ValidFish(Catch))return 0;
 return FMath::Max<int64>(1,FMath::FloorToInt64(double(Catch.WeightKg)*Profile->ValuePerKg));
}
bool UNeonCityFishingSubsystem::SellFish(FGuid ID)
{
 const int32 Index=Fish.IndexOfByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Pouch;});
 if(bChangingInventory || Index==INDEX_NONE)return false;
 const auto Record=Fish[Index];const int64 Price=GetSaleValue(Record);if(Price<=0)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);
 // Remove before account notification; failed credit restores the same item, with no inventory notification.
 Fish.RemoveAt(Index);
 if(!GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->AddMoney(Price,TEXT("FishSale"))) { Fish.Insert(Record,Index);return false; }
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::CookFish(FGuid ID)
{
 const int32 Index=Fish.IndexOfByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Pouch;});
 if(bChangingInventory || Index==INDEX_NONE || Meals.Num()>=16)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);
 FNeonCityFishMeal Meal;Meal.MealID=Fish[Index].FishID;Meal.SourceFishID=Fish[Index].FishID;Meal.SpeciesID=Fish[Index].SpeciesID;
 Meal.SourceWeightKg=Fish[Index].WeightKg;Meal.SourceSizeCm=Fish[Index].SizeCm;Meals.Add(Meal);Fish.RemoveAt(Index);
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::ReserveForGrill(FGuid ID,FName GrillID)
{
 auto* Record=Fish.FindByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Pouch;});
 if(bChangingInventory || !Record || GrillID.IsNone() || Meals.Num()>=16
  || Fish.ContainsByPredicate([&](const auto& R){return R.Storage==ENeonCityFishStorage::Grill && R.ContainerID==GrillID;}))return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Record->Storage=ENeonCityFishStorage::Grill;Record->ContainerID=GrillID;
 Record->GrillStage=ENeonCityGrillStage::Opening;Record->GrillStageSeconds=0;
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::UpdateGrillReservation(FGuid ID,FName GrillID,ENeonCityGrillStage Stage,float Seconds,float Fraction)
{
 auto* Record=Fish.FindByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Grill && R.ContainerID==GrillID;});
 if(bChangingInventory || !Record || uint8(Stage)<uint8(ENeonCityGrillStage::Opening) || uint8(Stage)>uint8(ENeonCityGrillStage::Plating)
  || !FMath::IsFinite(Seconds) || Seconds<0 || Seconds>120 || !FMath::IsFinite(Fraction) || Fraction<0 || Fraction>1)return false;
 // No backwards state/fraction writes: cancellation is a separate ownership operation.
 if(uint8(Stage)<uint8(Record->GrillStage) || Fraction+.0001f<Record->CookFraction)return false;
 Record->GrillStage=Stage;Record->GrillStageSeconds=Seconds;Record->CookFraction=Fraction;
 if(Fraction>0)Record->bAlive=false;
 return true;
}
bool UNeonCityFishingSubsystem::CancelGrillReservation(FGuid ID,FName GrillID)
{
 auto* Record=Fish.FindByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Grill && R.ContainerID==GrillID;});
 if(bChangingInventory || !Record || PouchCount()>=64)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Record->Storage=ENeonCityFishStorage::Pouch;Record->ContainerID=NAME_None;
 Record->GrillStage=ENeonCityGrillStage::Idle;Record->GrillStageSeconds=0;
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::CompleteGrillReservation(FGuid ID,FName GrillID,FGuid& MealID)
{
 const int32 Index=Fish.IndexOfByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Grill && R.ContainerID==GrillID;});
 if(bChangingInventory || Index==INDEX_NONE || Fish[Index].GrillStage!=ENeonCityGrillStage::Plating
  || Fish[Index].CookFraction<.999f || Meals.Num()>=16 || Meals.ContainsByPredicate([&](const auto& M){return M.SourceFishID==ID;}))return false;
 TGuardValue<bool> Guard(bChangingInventory,true);FNeonCityFishMeal Meal;Meal.MealID=ID;Meal.SourceFishID=ID;
 Meal.SpeciesID=Fish[Index].SpeciesID;Meal.SourceWeightKg=Fish[Index].WeightKg;Meal.SourceSizeCm=Fish[Index].SizeCm;
 Meals.Add(Meal);Fish.RemoveAt(Index);MealID=Meal.MealID;OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::IsSupportedAquarium(FName ContainerID)
{ return ContainerID==TEXT("Island.Mansion.Aquarium.Main"); }
bool UNeonCityFishingSubsystem::PutInAquarium(FGuid ID,FName ContainerID,bool Saltwater)
{
 auto* Record=Fish.FindByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Pouch;});
 const auto* Profile=Record?FindSpecies(Record->SpeciesID):nullptr;
 const int32 Count=Fish.FilterByPredicate([&](const auto& R){return R.Storage==ENeonCityFishStorage::Aquarium && R.ContainerID==ContainerID;}).Num();
 if(bChangingInventory || !Record || !Profile || !Record->bAlive || Record->CookFraction>0 || !Profile->SuitableForAquarium
  || Profile->SaltwaterSpecies!=Saltwater || Record->SizeCm>60 || !IsSupportedAquarium(ContainerID) || Count>=12)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Record->Storage=ENeonCityFishStorage::Aquarium;Record->ContainerID=ContainerID;
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::RemoveFromAquarium(FGuid ID,FName ContainerID)
{
 auto* Record=Fish.FindByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Aquarium && R.ContainerID==ContainerID;});
 if(bChangingInventory || !Record || PouchCount()>=64)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Record->Storage=ENeonCityFishStorage::Pouch;Record->ContainerID=NAME_None;
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::DeliverFishForOrder(FGuid ID,FName SpeciesID,float MinimumKg,float MaximumKg,int64 Reward,FName Receipt)
{
 const int32 Index=Fish.IndexOfByPredicate([&](const auto& R){return R.FishID==ID && R.Storage==ENeonCityFishStorage::Pouch
  && R.SpeciesID==SpeciesID && R.CookFraction==0 && R.WeightKg>=MinimumKg && R.WeightKg<=MaximumKg;});
 if(bChangingInventory || Index==INDEX_NONE || Reward<=0 || Receipt.IsNone())return false;
 const auto Record=Fish[Index];TGuardValue<bool> Guard(bChangingInventory,true);Fish.RemoveAt(Index);
 if(!GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->CreditOnce(Reward,Receipt)){Fish.Insert(Record,Index);return false;}
 OnInventoryChanged.Broadcast();return true;
}
bool UNeonCityFishingSubsystem::EatMeal(FGuid ID,AActor* Consumer)
{
 auto* Pawn=Cast<APawn>(Consumer);auto* PC=Pawn?Cast<APlayerController>(Pawn->GetController()):nullptr;
 if(bChangingInventory || !PC || !PC->IsLocalController() || PC->GetGameInstance()!=GetGameInstance() || !Pawn->HasAuthority())return false;
 auto* Health=Pawn->FindComponentByClass<UHealthComponent>();auto* Data=Pawn->FindComponentByClass<UNeonCityPlayerDataComponent>();
 auto* Body=Pawn->FindComponentByClass<UDamageableCharacterComponent>();
 const int32 Index=Meals.IndexOfByPredicate([&](const auto& M){return M.MealID==ID;});
 if(Index==INDEX_NONE || !Health || !Data || Health->IsDead() || (Body && !Body->CanAct()))return false;
 if(Health->GetCurrentHealth()>=Health->GetMaxHealth() && Data->GetStamina()>=100)return false;
 TGuardValue<bool> Guard(bChangingInventory,true);Meals.RemoveAt(Index);
 Health->Heal(Health->GetMaxHealth()*.25f);if(Data->GetStamina()<100)Data->SetStamina(FMath::Min(100.f,Data->GetStamina()+20.f));OnInventoryChanged.Broadcast();return true;
}
FNeonCityFishingSaveData UNeonCityFishingSubsystem::BuildPersistenceSnapshot(bool Defaults) const
{ FNeonCityFishingSaveData Result;if(!Defaults) { Result.Fish=Fish;Result.Meals=Meals; }return Result; }
bool UNeonCityFishingSubsystem::ValidateEnvelope(const FNeonCityFishingSaveData& Snapshot,FString& Error)
{ if(Snapshot.Fish.Num()>128 || Snapshot.Meals.Num()>16) { Error=TEXT("Fishing inventory exceeds supported bounds.");return false; }return true; }
void UNeonCityFishingSubsystem::NormalizePersistenceSnapshot(FNeonCityFishingSaveData& Snapshot,TArray<FString>& Warnings) const
{
 TSet<FGuid> FishIDs,MealIDs;TSet<FName> OccupiedGrills;TMap<FName,int32> AquariumCounts;int32 PouchItems=0;
 // A meal wins over a stale fish reservation of the same source identity.
 for(const auto& Meal:Snapshot.Meals)if(Meal.SourceFishID.IsValid())MealIDs.Add(Meal.SourceFishID);
 Snapshot.Fish.RemoveAll([&](const auto& Record)
 {
  const auto* Profile=FindSpecies(Record.SpeciesID);
  const bool Pouch=Record.Storage==ENeonCityFishStorage::Pouch && PouchItems<64;
  const bool Aquarium=Record.Storage==ENeonCityFishStorage::Aquarium && IsSupportedAquarium(Record.ContainerID)
   && Record.bAlive && Record.CookFraction==0 && Record.SizeCm<=60 && Profile && Profile->SuitableForAquarium && Profile->SaltwaterSpecies && AquariumCounts.FindRef(Record.ContainerID)<12;
  const bool Grill=Record.Storage==ENeonCityFishStorage::Grill && !Record.ContainerID.IsNone()
   && uint8(Record.GrillStage)>=uint8(ENeonCityGrillStage::Opening) && uint8(Record.GrillStage)<=uint8(ENeonCityGrillStage::Plating)
   && !OccupiedGrills.Contains(Record.ContainerID);
  if(!ValidFish(Record) || FishIDs.Contains(Record.FishID) || MealIDs.Contains(Record.FishID) || (!Pouch && !Aquarium && !Grill))
  { Warnings.Add(TEXT("Invalid/unknown/duplicate or unsupported stored fish skipped."));return true; }
  FishIDs.Add(Record.FishID);if(Pouch)++PouchItems;else if(Aquarium)++AquariumCounts.FindOrAdd(Record.ContainerID);else OccupiedGrills.Add(Record.ContainerID);return false;
 });
 for(auto& Record:Snapshot.Fish)if(Record.Storage==ENeonCityFishStorage::Pouch)Record.ContainerID=NAME_None;
 MealIDs.Reset();
 Snapshot.Meals.RemoveAll([&](const auto& Meal)
 {
  if(!Meal.MealID.IsValid() || MealIDs.Contains(Meal.MealID) || Meal.RecipeID!=TEXT("Food.GrilledFish") || !FindSpecies(Meal.SpeciesID))
  { Warnings.Add(TEXT("Invalid/unknown/duplicate fish meal skipped."));return true; }
  MealIDs.Add(Meal.MealID);return false;
 });
}
void UNeonCityFishingSubsystem::ApplyPersistenceSnapshot(const FNeonCityFishingSaveData& Snapshot)
{
 FString Error;if(bChangingInventory || !ValidateEnvelope(Snapshot,Error))return;
 auto Safe=Snapshot;TArray<FString> Warnings;NormalizePersistenceSnapshot(Safe,Warnings);
 TGuardValue<bool> Guard(bChangingInventory,true);Fish=MoveTemp(Safe.Fish);Meals=MoveTemp(Safe.Meals);OnInventoryChanged.Broadcast();
}

bool UNeonCityFishingSubsystem::IsRegisteredStation(const AFishingStation* Station) const
{ const auto* Entry=Station?Stations.Find(Station->StationID):nullptr;return Entry && Entry->Get()==Station; }
void UNeonCityFishingSubsystem::RegisterStation(AFishingStation* Station)
{
 if(!IsValid(Station) || Station->StationID.IsNone() || Station->GetGameInstance()!=GetGameInstance())return;
 for(auto It=Stations.CreateIterator();It;++It)if(!It.Value().IsValid())It.RemoveCurrent();
 if(Stations.Num()>=32 || (Stations.Contains(Station->StationID) && Stations[Station->StationID].IsValid() && Stations[Station->StationID].Get()!=Station))return;
 Stations.Add(Station->StationID,Station);
}
void UNeonCityFishingSubsystem::UnregisterStation(AFishingStation* Station)
{
 if(!Station)return;const auto* Existing=Stations.Find(Station->StationID);if(Existing && Existing->Get()==Station)Stations.Remove(Station->StationID);
}
AFishingStation* UNeonCityFishingSubsystem::FindNearestStation(FVector Location,ENeonCityFishingStation Kind) const
{
 if(Location.ContainsNaN())return nullptr;AFishingStation* Nearest=nullptr;double Best=DBL_MAX;
 for(const auto& Pair:Stations)if(auto* Station=Pair.Value.Get();Station && Station->Kind==Kind && Station->GetWorld()==GetGameInstance()->GetWorld())
 { const double Distance=FVector::DistSquared(Location,Station->GetActorLocation());if(Distance<Best) { Best=Distance;Nearest=Station; } }
 return Nearest;
}
