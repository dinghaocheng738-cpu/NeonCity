#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Leisure/NeonCityAerialTour.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "Engine/GameInstance.h"

namespace NeonCityWestLakeAirTour
{
/** Normal PIE by day: the player boards the helicopter on the residence helipad and flies both helicopter jobs.
 * Island Supply Drop: three crates hook on under the helicopter; held in a still hover 25 m over each island's
 * target (the test holds the helicopter there), the load steadies, the release fires, the crate falls with physics
 * onto the island and is scored by its rings; the run pays by score. West Lake from the Air: held 150 m off each
 * sight with the camera turned onto it, the viewfinder fills, the shutter fires and a photograph is saved; back on
 * the helipad the tour completes and pays per photograph plus the return bonus. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>420){Test->AddError(FString::Printf(TEXT("Air tour check timed out at stage %d stop %d"),Stage,Tour.IsValid()?Tour->GetStopIndex():-1));return true;}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !W || W->GetTimeSeconds()<4)return false;
  auto* Economy=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
  if(Stage==0)
  {
   ACharacter* Player=Cast<ACharacter>(PC->GetPawn());
   if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.Helipad")))Heli=*It;
   for(TActorIterator<ANeonCityAerialTour> It(W);It;++It)
   {
    if(It->ActivityID==TEXT("WestLake.AirTour"))Sights=*It;
    if(It->ActivityID==TEXT("WestLake.SupplyDrop"))Drops=*It;
   }
   if(!Test->TestNotNull(TEXT("helicopter on the residence helipad"),Heli.Get()) || !Test->TestNotNull(TEXT("sightseeing tour placed"),Sights.Get())
    || !Test->TestNotNull(TEXT("supply drop placed"),Drops.Get()))return true;
   Test->TestEqual(TEXT("seven sights"),Sights->Stops.Num(),7);
   Test->TestEqual(TEXT("three drop zones"),Drops->Stops.Num(),3);
   Test->TestTrue(TEXT("tours start at the helipad"),FVector::Dist2D(Drops->StartLocation,Heli->GetActorLocation())<Drops->StartRadius);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(11.f);
   if(Heli->GetHull()->GetPhysicsLinearVelocity().Size()>5 && W->GetTimeSeconds()<12)return false;
   Player->SetActorLocation(Heli->GetActorLocation()+Heli->GetActorRightVector()*260+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(1);
  }
  ANeonCityHelicopterPrototype* H=Heli.Get();if(!H){Test->AddError(TEXT("helicopter lost"));return true;}
  if(Stage==1)
  {
   if(Elapsed()<1)return false;
   if(!Test->TestTrue(TEXT("helicopter boarded"),H->EnterAircraft_Implementation(PC->GetPawn())))return true;
   Cash=Economy->GetBalance();
   if(!Test->TestTrue(TEXT("supply drop accepted from the phone"),W->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->Choose(Drops->ActivityID,ENeonCityOpportunityChoice::Accept,PC)))return true;
   Tour=Drops;return Next(2);
  }
  ANeonCityAerialTour* T=Tour.Get();if(!T){Test->AddError(TEXT("tour lost"));return true;}
  if(Stage==2)
  {
   // the helicopter is at the pad: the run starts and the crates hook on
   if(T->GetStage()<2)return Elapsed()>8?(Test->AddError(TEXT("supply drop never started at the pad")),true):false;
   if(!T->IsRunning())return Elapsed()>8?(Test->AddError(TEXT("supply drop not running")),true):false;
   Stop=-1;return Next(3);
  }
  if(Stage==3)
  {
   // airdrop: hold a still hover 25 m over the current target until the crate is released, then let it land
   if(!T->IsRunning())
   {
    const int64 Paid=Economy->GetBalance()-Cash;
    Test->TestTrue(FString::Printf(TEXT("drops scored (%d / 9)"),T->GetScore()),T->GetScore()>=3);
    Test->TestEqual(TEXT("paid by score"),Paid,T->RewardPerStop*T->GetScore()+T->FinishBonus);
    Cash=Economy->GetBalance();
    if(!Test->TestTrue(TEXT("sightseeing tour accepted"),W->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->Choose(Sights->ActivityID,ENeonCityOpportunityChoice::Accept,PC)))return true;
    Tour=Sights;Hold(H,Sights->PadLocation+FVector(0,0,400),H->GetActorRotation());
    return Next(4);
   }
   if(T->GetStopIndex()!=Stop)
   {
    if(Stop>=0)UE_LOG(LogTemp,Display,TEXT("AirTour: drop %d scored, total %d"),Stop,T->GetScore());
    Stop=T->GetStopIndex();At=FPlatformTime::Seconds();
   }
   if(!T->Stops.IsValidIndex(Stop))return false;
   Hold(H,T->Stops[Stop].Location+FVector(0,0,2500),H->GetActorRotation());
   if(!T->GetFallingCrate() && Elapsed()>20){Test->AddError(FString::Printf(TEXT("crate %d never released over %s"),Stop,*T->Stops[Stop].Label.ToString()));return true;}
   return false;
  }
  if(Stage==4)
  {
   if(T->GetStage()<2)
   {
    Hold(H,Sights->PadLocation+FVector(0,0,400),H->GetActorRotation());
    return Elapsed()>8?(Test->AddError(TEXT("sightseeing never started at the pad")),true):false;
   }
   Stop=-1;return Next(5);
  }
  if(Stage==5)
  {
   // sightseeing: hold 150 m off each sight, toward the open lake and 60 m above it, and turn the camera onto it
   if(T->GetStopIndex()!=Stop)
   {
    if(Stop>=0)++Photos;
    Stop=T->GetStopIndex();At=FPlatformTime::Seconds();
   }
   if(!T->Stops.IsValidIndex(Stop))
   {
    Test->TestEqual(TEXT("seven photographs taken"),Photos,7);
    for(const FString& F:T->GetPhotos())Test->TestTrue(FString::Printf(TEXT("photo saved %s"),*FPaths::GetCleanFilename(F)),IFileManager::Get().FileExists(*F));
    LandAt=FPlatformTime::Seconds();return Next(6);
   }
   const FVector S=T->Stops[Stop].Location;
   const FVector Away=(FVector(60000,5000,S.Z)-S).GetSafeNormal2D();
   const FVector Spot=S+Away*15000+FVector(0,0,6000);
   Hold(H,Spot,(S-Spot).GetSafeNormal2D().Rotation());
   FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
   PC->SetControlRotation((S-Eye).Rotation());
   if(Elapsed()>25){Test->AddError(FString::Printf(TEXT("never photographed %s (framed %d)"),*T->Stops[Stop].Label.ToString(),T->IsFramed(Stop,Eye,View,H)));return true;}
   return false;
  }
  if(Stage==6)
  {
   // set down on the pad: the tour completes once the helicopter rests on its skids
   if(T->IsRunning())
   {
    if(FPlatformTime::Seconds()-LandAt<2)Hold(H,T->PadLocation+FVector(0,0,300),H->GetActorRotation());
    else {H->GetHull()->SetPhysicsLinearVelocity(FVector(0,0,FMath::Min(H->GetHull()->GetPhysicsLinearVelocity().Z,0.f)));H->GetHull()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);}
    return Elapsed()>30?(Test->AddError(TEXT("tour never completed on the helipad")),true):false;
   }
   const int64 Paid=Economy->GetBalance()-Cash;
   Test->TestEqual(TEXT("tour paid per photograph plus the return bonus"),Paid,T->RewardPerStop*T->Stops.Num()+T->FinishBonus);
   return true;
  }
  return true;
 }
private:
 /** Keep the helicopter still at a point (the test's hands on the controls). */
 void Hold(ANeonCityHelicopterPrototype* H,const FVector& Where,const FRotator& Facing)
 {
  H->SetActorLocationAndRotation(Where,FRotator(0,Facing.Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
  H->GetHull()->SetPhysicsLinearVelocity(FVector::ZeroVector);H->GetHull()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
 }
 double Elapsed() const { return FPlatformTime::Seconds()-At; }
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;
 TWeakObjectPtr<ANeonCityHelicopterPrototype> Heli;TWeakObjectPtr<ANeonCityAerialTour> Sights,Drops,Tour;
 double Started=0,At=0,LandAt=0;int32 Stage=0,Stop=-1,Photos=0;int64 Cash=0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeAirTourPIE,"NeonCity.WestLake.AirTourPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeAirTourPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeAirTour::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
