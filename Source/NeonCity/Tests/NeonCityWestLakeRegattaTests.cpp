#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Leisure/NeonCityCheckpointRace.h"
#include "Leisure/NeonCityActivityHUD.h"
#include "Vehicles/Marine/NeonCityLamborghini63.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineVehicleInterface.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Combat/HealthComponent.h"
#include "Interaction/InteractableInterface.h"
#include "World/NeonCityDayNightCycle.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Engine/GameInstance.h"

namespace NeonCityWestLakeRegatta
{
/** Normal PIE at night: the player takes the helm of the boat club's celadon Lamborghini 63 at the Hubin revetment,
 * accepts the West Lake Night Regatta, stops at the start for the countdown and races the outer-lake course in Corsa
 * on an autopilot that aims at each gate in turn. Checks the club livery, the lantern markers, the gate order, the
 * HUD, a finish with a time and a paid medal, and that the run's markers are cleared. Pass -NeonCityRegattaCapture
 * for stills (Saved/WestLake/Regatta). */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>900){Test->AddError(FString::Printf(TEXT("Regatta check timed out at stage %d gate %d"),Stage,Race.IsValid()?Race->GetNextGate():-1));return true;}
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !W || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   ACharacter* Player=Cast<ACharacter>(PC->GetPawn());
   if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityLamborghini63> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.Leisure")))Boat=*It;
   for(TActorIterator<ANeonCityCheckpointRace> It(W);It;++It)if(It->ActivityID==TEXT("WestLake.NightRegatta"))Race=*It;
   if(!Test->TestNotNull(TEXT("boat club's Lamborghini 63 at the Hubin revetment"),Boat.Get()) || !Test->TestNotNull(TEXT("night regatta placed"),Race.Get()))return true;
   Test->TestTrue(TEXT("club 63 in its own livery"),Boat->bCustomLivery);
   Test->TestTrue(FString::Printf(TEXT("club 63 afloat at the revetment (z %.0f cm)"),Boat->GetActorLocation().Z),FMath::Abs(Boat->GetActorLocation().Z+50)<90);
   Test->TestEqual(TEXT("regatta has 10 gates"),Race->Gates.Num(),10);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(10.f);
   FText Why;Test->TestFalse(TEXT("regatta closed by day"),Race->GetOpportunityData().AcceptanceAvailable);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(21.f);
   Test->TestTrue(TEXT("regatta open at 21:00"),Race->GetOpportunityData().AcceptanceAvailable);
   // aboard in the cockpit (where F at the quay puts a visitor), then F takes the helm
   Player->SetActorLocation(Boat->GetActorTransform().TransformPosition(FVector(-515,0,102+95)),false,nullptr,ETeleportType::TeleportPhysics);
   Cash=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance();
   return Next(1);
  }
  ANeonCityLamborghini63* B=Boat.Get();ANeonCityCheckpointRace* R=Race.Get();if(!B || !R){Test->AddError(TEXT("club 63 or race lost"));return true;}
  if(Stage==1)
  {
   if(PC->GetPawn()!=B)
   {
    if(auto* Person=Cast<ACharacter>(PC->GetPawn());Person && Elapsed()>1 && B->GetActionFor(Person)==ENeonCityYachtAction::TakeHelm)IInteractableInterface::Execute_Interact(B,Person);
    return Elapsed()>12?(Test->AddError(FString::Printf(TEXT("player never took the club 63's helm (action %d)"),Cast<ACharacter>(PC->GetPawn())?int32(B->GetActionFor(Cast<ACharacter>(PC->GetPawn()))):-1)),true):false;
   }
   if(!Test->TestTrue(TEXT("regatta accepted from the phone domain"),W->GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->Choose(R->ActivityID,ENeonCityOpportunityChoice::Accept,PC)))return true;
   Test->TestEqual(TEXT("lantern buoys laid (two per gate)"),R->GetMarkerCount(),20);
   B->SetDriveMode(ENeonCityDriveMode::Corsa);
   Shot(TEXT("A-ClubYacht.png"),B->GetActorLocation(),B->GetActorLocation()+FVector(2600,-1800,900));
   return Next(2);
  }
  if(Stage==2)                                     // motor to the start and stop there
  {
   if(R->GetStage()>=1){Test->AddInfo(FString::Printf(TEXT("countdown began after %.1fs"),Elapsed()));Drive(B,0,0,1);return Next(3);}
   const FVector To=R->StartLocation-B->GetActorLocation();const float Dist=To.Size2D();
   Drive(B,Dist>2500?.4f:0.f,SteerTo(B,R->StartLocation),Dist<2500?1.f:0.f);
   return Elapsed()>90?(Test->AddError(TEXT("never reached the regatta start")),true):false;
  }
  if(Stage==3)                                     // countdown
  {
   Drive(B,0,0,1);
   if(R->GetStage()==2){RaceStart=FPlatformTime::Seconds();LastGate=0;LastGateAt=RaceStart;
    auto* HUD=W->GetSubsystem<UNeonCityActivityHUDSubsystem>();
    Test->TestTrue(TEXT("activity HUD shows the regatta"),HUD && HUD->IsShowing(R->ActivityID));return Next(4);}
   return Elapsed()>8?(Test->AddError(TEXT("countdown never started the race")),true):false;
  }
  if(Stage==4)                                     // race on the autopilot
  {
   if(!R->IsRunning())
   {
    const auto Data=R->BuildOpportunitySnapshot(false);
    Test->TestEqual(TEXT("regatta completed"),int32(Data.State),2);
    Test->TestTrue(FString::Printf(TEXT("finish time recorded (%.1f s)"),R->GetBestSeconds()),R->GetBestSeconds()>60 && R->GetBestSeconds()<R->MaximumSeconds);
    const int64 Paid=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->GetBalance()-Cash;
    const int32 Medal=R->MedalFor(R->GetBestSeconds());
    Test->AddInfo(FString::Printf(TEXT("Regatta finished: %s, medal %d, rank %d of %d, paid $%lld"),*ANeonCityLeisureActivity::FormatTime(R->GetBestSeconds()),Medal,R->RankOf(R->GetBestSeconds()),R->Rivals.Num()+1,Paid));
    Test->TestTrue(TEXT("a medal time is reachable on the autopilot"),Medal>=1);
    Test->TestTrue(TEXT("medal prize paid once into the wallet"),Paid==(Medal==3?R->GoldReward:Medal==2?R->SilverReward:R->BronzeReward));
    Test->TestEqual(TEXT("markers cleared after the run"),R->GetMarkerCount(),0);
    Shot(TEXT("F-Finish.png"),B->GetActorLocation(),B->GetActorLocation()+FVector(-1200,600,700));
    return true;
   }
   const int32 G=R->GetNextGate();
   if(G!=LastGate)
   {
    Test->AddInfo(FString::Printf(TEXT("gate %d (%s) at %.1f s, %.0f km/h"),LastGate+1,*R->Gates[LastGate].Label.ToString(),R->GetRunSeconds(),B->GetVelocity().Size()*.036f));
    if(LastGate%3==1)Shot(*FString::Printf(TEXT("B-Gate%02d.png"),LastGate+1),B->GetActorLocation(),B->GetActorLocation()-B->GetActorForwardVector()*3200+FVector(0,0,1100));
    LastGate=G;LastGateAt=FPlatformTime::Seconds();
   }
   if(FPlatformTime::Seconds()-LastGateAt>90){Test->AddError(FString::Printf(TEXT("stuck before gate %d (%s) at %s"),G+1,*R->Gates[G].Label.ToString(),*B->GetActorLocation().ToCompactString()));return true;}
   // aim at the gate; brake ahead of a sharp turn into the next leg, as a driver would
   const FNeonCityRaceGate& Gate=R->Gates[G];const FVector Here=B->GetActorLocation();
   const float Now=B->GetVelocity().Size()*.036f,Dist=FVector::Dist2D(Here,Gate.Location);
   float Turn=0;
   if(R->Gates.IsValidIndex(G+1))
    Turn=FMath::Abs(FRotator::NormalizeAxis((R->Gates[G+1].Location-Gate.Location).Rotation().Yaw-(Gate.Location-Here).Rotation().Yaw));
   const float Steer=SteerTo(B,Gate.Location);
   const bool Corner=Turn>35 && Dist<FMath::Max(9000.f,Now*Now*Turn/90.f*1.2f);
   const float Safe=Corner?FMath::GetMappedRangeValueClamped(FVector2D(35,120),FVector2D(90,40),Turn):130;
   Drive(B,Now>Safe+8?0.f:(FMath::Abs(Steer)>.6f?.5f:1.f),Steer,Now>Safe+15?1.f:0.f);
   {
    // a sudden stop: what is the hull against, and is the boat still alive
    const float Kmh=B->GetVelocity().Size()*.036f;
    if(PrevKmh-Kmh>1.5f && PrevKmh>20 && Frames<40)
    {
     ++Frames;auto* Move=B->GetMarineMovement();FHitResult Hit;FCollisionQueryParams Q(TEXT("RegattaDecel"),true,B);
     const bool Any=W->SweepSingleByChannel(Hit,B->GetActorLocation(),B->GetActorLocation()+B->GetVelocity().GetSafeNormal()*300,B->GetActorQuat(),ECC_Vehicle,FCollisionShape::MakeBox(B->GetHull()->GetScaledBoxExtent()),Q);
     Test->AddInfo(FString::Printf(TEXT("DECEL %.1f->%.1f km/h t %.2f at %s rot %s sim %d afloat %d engine %d dt %.3f sweep %s %s"),PrevKmh,Kmh,R->GetRunSeconds(),*B->GetActorLocation().ToCompactString(),*B->GetActorRotation().ToCompactString(),
      B->GetHull()->IsSimulatingPhysics(),Move->IsAfloat(),Move->IsEngineRunning(),W->GetDeltaSeconds(),Any?*GetNameSafe(Hit.GetActor()):TEXT("clear"),Any?*GetNameSafe(Hit.GetComponent()):TEXT("")));
    }
    if(PrevKmh>30 && Kmh<10 && !bReportedStop)
    {
     bReportedStop=true;FHitResult Hit;FCollisionQueryParams Q(TEXT("RegattaStop"),true,B);
     const FVector Ext=B->GetHull()->GetScaledBoxExtent();const FVector From=B->GetActorLocation();
     const bool Any=W->SweepSingleByChannel(Hit,From,From+B->GetActorForwardVector()*400,B->GetActorQuat(),ECC_Vehicle,FCollisionShape::MakeBox(Ext),Q);
     auto* Move=B->GetMarineMovement();auto* Health=B->GetHealthComponent();
     Test->AddInfo(FString::Printf(TEXT("SUDDEN STOP %.0f->%.0f km/h at %s: sweep %s (%s / %s) engine %d afloat %d health %.0f dead %d"),PrevKmh,Kmh,*From.ToCompactString(),
      Any?TEXT("hit"):TEXT("clear"),Any?*GetNameSafe(Hit.GetActor()):TEXT("-"),Any?*GetNameSafe(Hit.GetComponent()):TEXT("-"),Move->IsEngineRunning(),Move->IsAfloat(),Health?Health->GetCurrentHealth():-1.f,Health && Health->IsDead()?1:0));
    }
    PrevKmh=Kmh;
   }
   if(FPlatformTime::Seconds()-LastLog>(R->GetRunSeconds()<20?2:10)){LastLog=FPlatformTime::Seconds();Test->AddInfo(FString::Printf(TEXT("  t %.0f s gate %d at %s %.0f km/h rot %s"),R->GetRunSeconds(),G+1,*B->GetActorLocation().ToCompactString(),B->GetVelocity().Size()*.036f,*B->GetActorRotation().ToCompactString()));}
   return false;
  }
  return true;
 }
private:
 float SteerTo(const APawn* B,const FVector& Target) const
 {
  const float Want=(Target-B->GetActorLocation()).Rotation().Yaw;const float Err=FRotator::NormalizeAxis(Want-B->GetActorRotation().Yaw);
  return FMath::Clamp(Err/25.f,-1.f,1.f);
 }
 void Drive(ANeonCityLamborghini63* B,float Throttle,float Steer,float Brake)
 {IMarineVehicleInterface::Execute_SubmitDriverInput(B,PC,Throttle,Steer,Brake);}
 double Elapsed() const { return FPlatformTime::Seconds()-At; }
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 void Shot(const TCHAR* Name,FVector Focus,FVector Eye)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityRegattaCapture")) || !W || !PC)return;
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation());AActor* Previous=PC->GetViewTarget();
  PC->SetViewTarget(Camera);PC->PlayerCameraManager->SetGameCameraCutThisFrame();PC->PlayerCameraManager->UpdateCamera(0);
  if(auto* V=W->GetGameViewport();V && V->Viewport)
  {
   V->Viewport->Draw(false);TArray<FColor> Pixels;
   if(V->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pxl:Pixels)Pxl.A=255;TArray64<uint8> PNG;const auto Size=V->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/Regatta");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveArrayToFile(PNG,*(Dir/Name));
   }
  }
  PC->SetViewTarget(Previous);Camera->Destroy();
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;
 double Started=0,At=0,RaceStart=0,LastGateAt=0,LastLog=0;int32 Stage=0,LastGate=0;int64 Cash=0;float PrevKmh=0;bool bReportedStop=false;int32 Frames=0;
 struct FWaypoint{FVector At;int32 Gate;bool bLineUp;};TArray<FWaypoint> Path;int32 Wp=0;
 TWeakObjectPtr<ANeonCityLamborghini63> Boat;TWeakObjectPtr<ANeonCityCheckpointRace> Race;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeRegattaPIE,"NeonCity.WestLake.NightRegattaPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeRegattaPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeRegatta::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
