#include "Presentation/FullArtReviewSubsystem.h"
#include "Misc/CommandLine.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "VehicleCombat/VehicleCombatTestArea.h"
#include "AI/Police/PoliceVehicle.h"
#include "Economy/CurrencyComponent.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "World/NeonCityDayNightCycle.h"
#include "InputCoreTypes.h"
namespace
{
const TCHAR* Labels[]={TEXT("01 Buildings / walk and use F at Visitor Hall"),TEXT("02 Sedan / F enter, WASD drive"),TEXT("03 Sports / F enter"),TEXT("04 SUV / F enter"),TEXT("05 Pickup / F enter"),TEXT("06 Police visual / response regression is separate"),TEXT("07 Weapons / 1-4 equip, RMB aim, LMB fire, R reload"),TEXT("08 Beach / promenade, sand and shore"),TEXT("09 Swimming / WASD, C dive, Space surface"),TEXT("10 Speedboat / F enter, WASD, Space brake"),TEXT("11 Yacht / visual shell only, no boarding gameplay"),TEXT("12 Helicopter / F enter, Space up, C down"),TEXT("13 Helicopter missile / V arm, Tab twice, RMB lock, LMB fire"),TEXT("14 Fixed wing / F, W throttle, Down pitch up"),TEXT("15 Lighting / T cycles day, sunset, night")};
const FVector Stops[]={FVector(-4300,-800,150),FVector(4475,590,150),FVector(4825,590,150),FVector(5525,590,150),FVector(5875,590,150),FVector(-2500,6580,520),FVector(2050,780,150),FVector(-800,-1800,110),FVector(5200,-3550,60),FVector(5200,-3220,60),FVector(-3300,-2300,110),FVector(-3000,2030,150),FVector(-3000,2030,150),FVector(15000,6450,520),FVector(-4300,-800,150)};
}
bool UFullArtReviewSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{ return FParse::Param(FCommandLine::Get(),TEXT("NeonCityFullArtTour")) && FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) && Super::ShouldCreateSubsystem(Outer); }
bool UFullArtReviewSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::PIE || Type==EWorldType::Game; }
void UFullArtReviewSubsystem::GoToStop()
{
 auto* PC=GetWorld()->GetFirstPlayerController(); auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
 if(!Player) { if(GEngine) GEngine->AddOnScreenDebugMessage(20551,5,FColor::Yellow,TEXT("Land / stop and exit before changing review stop."));return; }
 Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
 Player->SetActorLocation(Stops[Stop],false,nullptr,ETeleportType::TeleportPhysics);
 PC->SetControlRotation(FRotator(-8,Stop==10?-90:90,0));
}
void UFullArtReviewSubsystem::Tick(float DeltaTime)
{
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC || !PC->GetPawn() || GetWorld()->GetTimeSeconds()<2) return;
 if(!Initialized)
 {
  Initialized=true;
  bool HasStrip=false;for(TActorIterator<AFixedWingTestArea> It(GetWorld());It;++It) HasStrip=true;
  if(!HasStrip) GetWorld()->SpawnActor<AFixedWingTestArea>();
  GetWorld()->SpawnActor<AVehicleCombatTestArea>();
  GetWorld()->SpawnActor<APoliceVehicle>(FVector(-2500,7000,481),FRotator::ZeroRotator);
  if(auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn()))
  {
   // Review-session purchases exercise the existing catalogue; disk persistence
   // is disabled by the required launch flag. No normal-game defaults change.
   if(auto* Wallet=Player->FindComponentByClass<UCurrencyComponent>()) Wallet->AddMoney(10000);
   if(auto* Inventory=Player->FindComponentByClass<UWeaponInventoryComponent>()) for(int32 I=1;I<=4;++I) Inventory->PurchaseWeapon(NeonCityWeapons::Slot(I));
  }
  GoToStop();
 }
 if(PC->WasInputKeyJustPressed(EKeys::PageDown)) { if(Cast<ANeonCityPlayerCharacter>(PC->GetPawn())) Stop=(Stop+1)%15;GoToStop(); }
 if(PC->WasInputKeyJustPressed(EKeys::PageUp)) { if(Cast<ANeonCityPlayerCharacter>(PC->GetPawn())) Stop=(Stop+14)%15;GoToStop(); }
 if(PC->WasInputKeyJustPressed(EKeys::T))
 { Lighting=(Lighting+1)%3;for(TActorIterator<ANeonCityDayNightCycle> It(GetWorld());It;++It) { It->bCycleEnabled=false;It->SetTimeOfDay(Lighting==0?10.f:Lighting==1?17.3f:21.f); } }
 if(GetWorld()->GetTimeSeconds()>NextHint && GEngine)
 { NextHint=GetWorld()->GetTimeSeconds()+.5f;GEngine->AddOnScreenDebugMessage(20550,.65f,FColor::White,FString(Labels[Stop])+TEXT("\nPgDn / PgUp: review stop | T: time | transient review session")); }
}
