#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Fishing/FishingStation.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Components/BoxComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
namespace
{
class FPolishVillaWalk : public IAutomationLatentCommand
{
public:
 explicit FPolishVillaWalk(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(FString::Printf(TEXT("Villa route timed out at %d"),Stage));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();auto* Fishing=PC->FindComponentByClass<UFishingComponent>();
  auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  const FVector Origin=NeonCityCoastalLayout::FromLegacyIsland(FVector(64000,-160000,400));
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(Origin))return false;
   for(TActorIterator<AFishingStation> It(W);It;++It)if(It->StationID==TEXT("Island.Villa.TerraceBBQ"))Grill=*It;
   if(!Test->TestNotNull(TEXT("Authored terrace BBQ"),Grill))return true;
   Flip=Grill->GetActorLocation().X<Origin.X?-1:1;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   P->SetActorLocation(Origin+FVector(0,2320,20),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);P->GetCharacterMovement()->StopMovementImmediately();
   Stage=1;At=W->GetTimeSeconds();LastProgress=At;Last=P->GetActorLocation();return false;
  }
  const FVector2D Points[]={FVector2D(0,1300),FVector2D(0,1050),FVector2D(0,750),FVector2D(0,400),FVector2D(100,150),FVector2D(0,150),FVector2D(0,-120),FVector2D(0,-530),FVector2D(600,-580)};
  if(Stage==1)
  {
   if(W->GetTimeSeconds()-At<1)return false;
   const FVector Target=Origin+FVector(Points[Point].X*Flip,Points[Point].Y,0);const FVector Direction=Target-P->GetActorLocation();
   if(FVector::Dist2D(Last,P->GetActorLocation())>20){Last=P->GetActorLocation();LastProgress=W->GetTimeSeconds();}
   if(W->GetTimeSeconds()-LastProgress>6){Test->AddError(FString::Printf(TEXT("Villa walking blocked at point %d (%s)"),Point,*P->GetActorLocation().ToString()));return true;}
   if(Direction.Size2D()>35){PC->SetControlRotation(FRotator(-5,Direction.Rotation().Yaw,0));P->AddMovementInput(Direction.GetSafeNormal2D(),.55);return false;}
   if(Point==3)FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Villa-Living.png"));
   if(Point==6)FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Villa-Terrace.png"));
   if(++Point<UE_ARRAY_COUNT(Points))return false;
   P->GetCharacterMovement()->StopMovementImmediately();Stage=2;At=W->GetTimeSeconds();return false;
  }
  if(Stage==2)
  {
   FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);PC->SetControlRotation((Grill->InteractionBounds->GetComponentLocation()-Eye).Rotation());
   if(W->GetTimeSeconds()-At<.5)return false;
   FRandomStream Random(205);auto Catch=Inventory->SampleCatch(Random,TEXT("Coastal"),12,TEXT("Clear"),TEXT("Polish.GrillTestFixture"));
   Test->TestTrue(TEXT("Explicit inventory fixture for BBQ UI test"),Inventory->StoreCatch(Catch));Meals=Inventory->GetMeals().Num();Key(EKeys::F,IE_Pressed);Stage=3;At=W->GetTimeSeconds();return false;
  }
  if(Stage==3 && W->GetTimeSeconds()-At>.3)
  {
   Key(EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("F opens actual BBQ session after continuous walking"),Fishing->IsActive()))return true;
   Key(EKeys::Enter,IE_Pressed);Stage=4;At=W->GetTimeSeconds();return false;
  }
  if(Stage==4 && W->GetTimeSeconds()-At>.25)
  {
   Key(EKeys::Enter,IE_Released);Test->TestEqual(TEXT("Cooking takes time"),Inventory->GetMeals().Num(),Meals);Stage=5;return false;
  }
  if(Stage==5 && W->GetTimeSeconds()-At>4)
  {
   Test->TestEqual(TEXT("Terrace BBQ produces meal through actual input"),Inventory->GetMeals().Num(),Meals+1);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Villa-BBQ.png"));Fishing->Stop();
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(22);
   PC->SetControlRotation(FRotator(-5,90,0));At=W->GetTimeSeconds();Stage=6;return false;
  }
  if(Stage==6 && W->GetTimeSeconds()-At>4){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Villa-Night.png"));return true;}
  return false;
 }
private:
 FAutomationTestBase* Test;AFishingStation* Grill=nullptr;double Started=0;float At=0,LastProgress=0;FVector Last;
 int32 Stage=0,Point=0,Flip=1,Meals=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishVillaPIE,"NeonCity.Polish.VillaPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPolishVillaPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FPolishVillaWalk(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
