#include "Adventure/MarineEcologySubsystem.h"
#include "Adventure/MarineSchool.h"
#include "Adventure/CoastalWildlife.h"
#include "Fishing/FishingEcologyModel.h"
#include "Fishing/FishData.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Engine/World.h"

bool UMarineEcologySubsystem::ReserveFishingEncounter(FVector Bobber,float Hour,bool DeepRig,FRandomStream& Random,
 FNeonCityCaughtFish& Catch,FVector& Bait,float& WaitSeconds,FString& Habitat)
{
 Catch=FNeonCityCaughtFish();WaitSeconds=MAX_flt;Habitat=TEXT("No nearby activity");
 if(Bobber.ContainsNaN() || !FMath::IsFinite(Hour) || Hooks.Num()>=16)return false;
 FWaterSurfaceSample Surface;FHitResult Floor;
 if(!GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Bobber,Surface))return false;
 if(!GetWorld()->LineTraceSingleByObjectType(Floor,Surface.SurfacePoint-FVector(0,0,35),Surface.SurfacePoint-FVector(0,0,20000),
  FCollisionObjectQueryParams(ECC_WorldStatic)))return false;
 const float Depth=(Surface.SurfacePoint.Z-Floor.ImpactPoint.Z)/100;
 if(Depth<.35f)return false;
 float Reef=10000,Pier=10000;
 for(const auto& W:Schools)if(const auto* S=W.Get();S && (S->Behaviour==EMarineBehaviour::Forage || S->Behaviour==EMarineBehaviour::Hover))
  Reef=FMath::Min(Reef,float(FVector::Dist2D(Bobber,S->GetActorLocation())-S->HomeRadius)/100);
 if(const auto* Coast=ANeonCityCoastalRegion::Find(GetWorld()))Pier=FVector::Dist2D(Bobber,Coast->GetFishingBerth().GetLocation())/100;
 const auto WaterType=NeonCityFishingEcology::HabitatAt(Depth,Reef,Pier);
 Habitat=FString::Printf(TEXT("%s / %.1f m deep"),Depth>18?TEXT("Deep water"):Reef<6?TEXT("Reef edge"):Pier<9?TEXT("Pier water"):Depth<3.5?TEXT("Shallows"):TEXT("Open bay"),Depth);
 Bait=Surface.SurfacePoint-FVector(0,0,DeepRig?FMath::Min(2800.f,Depth*65):FMath::Min(220.f,Depth*45));
 struct FCandidate {FHook Hook;const UFishData* Species;FVector Position;float Weight,Speed;};
 TArray<FCandidate> Candidates;float Total=0;
 const float Range=DeepRig?2600.f:2000.f;
 const auto Add=[&](const FHook& Hook,const UFishData* Species,FVector At,float Speed)
 {
  if(!Species || !Species->IsValidProfile())return;
  if(const auto* School=Hook.School.Get())
  {
   const FVector Local=School->GetActorTransform().InverseTransformPosition(Bait);
   if(FMath::Abs(Local.X)>School->FloorExtent-50 || FMath::Abs(Local.Y)>School->FloorExtent-50
    || Local.Z<School->FloorAt(Local)+School->FloorClearance || Bait.Z>School->MeanSeaLevel-School->SurfaceClearance)return;
  }
  const float Distance=FVector::Dist2D(At,Bobber),Vertical=FMath::Abs(At.Z-Bait.Z);
  if(Distance>=Range || Vertical>(DeepRig?2600:850))return;
  // Reject solid barriers between the actual animal and the lure, not merely a habitat label.
  FHitResult Obstruction;
  if(GetWorld()->LineTraceSingleByObjectType(Obstruction,At,Bait,FCollisionObjectQueryParams(ECC_WorldStatic)))return;
  const auto Preference=Species->TimePreference==ENeonCityFishTime::Day?ENeonCityFishActivity::Day:
   Species->TimePreference==ENeonCityFishTime::Dusk?ENeonCityFishActivity::DawnDusk:
   Species->TimePreference==ENeonCityFishTime::Night?ENeonCityFishActivity::Night:ENeonCityFishActivity::Any;
  const float Rarity=Species->Rarity==ENeonCityFishRarity::Rare?.2f:Species->Rarity==ENeonCityFishRarity::Uncommon?.6f:1.f;
  const float Weight=NeonCityFishingEcology::Activity(Preference,Hour)*Rarity*FMath::Square(1-Distance/Range)/(1+Vertical/450);
  if(Weight>0){Candidates.Add({Hook,Species,At,Weight,Speed});Total+=Weight;}
 };
 for(const auto& W:Schools)if(auto* S=W.Get();S && S->Behaviour!=EMarineBehaviour::Benthic
  && FVector::Dist2D(S->GetActorLocation(),Bobber)<Range+S->HomeRadius)
  for(int32 I=0;I<S->Starts.Num();++I)if(S->CanHook(I))Add({S,nullptr,I},S->Species,S->CreaturePosition(I),S->CruiseSpeed);
 for(const auto& W:Members)if(auto* F=W.Get();F && F->CanHook())Add({nullptr,F,INDEX_NONE},F->Species,F->GetActorLocation(),F->Speed);
 if(Candidates.IsEmpty())return false;
 float Roll=Random.FRand()*Total;const FCandidate* Chosen=&Candidates.Last();
 for(const auto& C:Candidates){Roll-=C.Weight;if(Roll<=0){Chosen=&C;break;}}
 const FGuid Ticket=FGuid::NewGuid();const FHook Hook=Chosen->Hook;
 const bool Reserved=Hook.School.IsValid()?Hook.School->ReserveHook(Hook.Index,Ticket,Catch):Hook.Fish.IsValid() && Hook.Fish->ReserveHook(Ticket,Catch);
 if(!Reserved)return false;
 Hooks.Add(Ticket,Hook);
 const float Density=FMath::Clamp(Total/8.f,.4f,1.6f);
 const float Delay=NeonCityFishingEcology::BiteDelay(WaterType,Hour,Random.FRand())/FMath::Sqrt(Density);
 // Give this individual time to swim toward the bait. A bite still waits for physical arrival.
 WaitSeconds=FMath::Max(Delay,float(FVector::Dist(Chosen->Position,Bait))/FMath::Max(25.f,Chosen->Speed*1.6f));
 return true;
}
bool UMarineEcologySubsystem::MoveFishingEncounter(FGuid Ticket,FVector Target,bool Hooked,bool Landed)
{
 const auto* H=Hooks.Find(Ticket);if(!H)return false;
 return H->School.IsValid()?H->School->MoveHook(H->Index,Ticket,Target,Hooked,Landed):H->Fish.IsValid() && H->Fish->MoveHook(Ticket,Target,Hooked,Landed);
}
bool UMarineEcologySubsystem::FishingEncounterPosition(FGuid Ticket,FVector& Position)const
{
 const auto* H=Hooks.Find(Ticket);if(!H)return false;
 if(H->School.IsValid() && H->School->IsAlive(H->Index)){Position=H->School->CreaturePosition(H->Index);return true;}
 if(H->Fish.IsValid()){Position=H->Fish->GetActorLocation();return true;}
 return false;
}
bool UMarineEcologySubsystem::KeepFishingEncounter(FGuid Ticket,UNeonCityFishingSubsystem* Inventory,const FNeonCityCaughtFish& Catch)
{
 const auto* H=Hooks.Find(Ticket);if(!H || !Inventory)return false;
 const bool Kept=H->School.IsValid()?H->School->FinishHook(H->Index,Ticket,Inventory,Catch,true):H->Fish.IsValid() && H->Fish->FinishHook(Ticket,Inventory,Catch,true);
 if(Kept)Hooks.Remove(Ticket);return Kept;
}
void UMarineEcologySubsystem::ReleaseFishingEncounter(FGuid Ticket)
{
 FHook H;if(!Hooks.RemoveAndCopyValue(Ticket,H))return;
 if(H.School.IsValid())H.School->FinishHook(H.Index,Ticket,nullptr,FNeonCityCaughtFish(),false);
 else if(H.Fish.IsValid())H.Fish->FinishHook(Ticket,nullptr,FNeonCityCaughtFish(),false);
}
