#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Combat/HealthComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

namespace NeonCityNavigationRecovery
{
/** State fault injection, deliberately separate from the normal driving route. */
class FRecovery : public IAutomationLatentCommand
{
public:
 explicit FRecovery(FAutomationTestBase* T):Test(T) {}
 bool Next(int32 Value) { Stage=Value;At=W->GetTimeSeconds();return false; }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>45) { Test->AddError(FString::Printf(TEXT("Navigation recovery timed out: %d"),Stage));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  auto* Nav=PC->GetNavigationComponent();auto* GI=W->GetGameInstance();
  auto* Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();auto* Economy=GI->GetSubsystem<UNeonCityEconomySubsystem>();
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityTransportTest> It(W);It;++It)Contract=*It;
   if(!Test->TestTrue(TEXT("Recovery fixture has actual assigned car"),Contract && Contract->DeliveryVehicle))return true;
   Car=Contract->DeliveryVehicle;
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)if(*It!=Car)OtherCar=*It;
   if(!Test->TestNotNull(TEXT("Other car exists for wrong-vehicle regression"),OtherCar))return true;
   FNeonCityNavigationDestination D;D.OwnerID=TEXT("Test.Recovery");D.DestinationID=TEXT("Test.TransientTarget");D.Label=FText::FromString(TEXT("Transient target"));
   AActor* Target=W->SpawnActor<AActor>();D.MovingTarget=Target;
   Test->TestTrue(TEXT("Generic destination accepts a live actor"),Nav->SetDestination(D));
   Nav->ClearDestination(TEXT("Different.Owner"));
   Test->TestEqual(TEXT("Another publisher cannot clear the destination"),Nav->GetDestination().DestinationID,D.DestinationID);
   D.OwnerID=NAME_None;Test->TestFalse(TEXT("Destination without stable publisher rejected"),Nav->SetDestination(D));
   Target->Destroy();return Next(1);
  }
  if(W->GetTimeSeconds()-At<.4f)return false;
  if(Stage==1)
  {
   Test->TestFalse(TEXT("Expired actor clears stale marker without a raw pointer"),Nav->GetDisplay().Active);
   if(!Test->TestTrue(TEXT("Accept actual Marcus opportunity"),GI->GetSubsystem<UPhoneSubsystem>()->ExecuteAction(Contract->ReceiptKey(),PC)))return true;
   Save->ConfigureSlot(TEXT("NeonCity_Test_NavigationRecovery"));Save->DeleteSave();
   if(!Test->TestTrue(TEXT("Healthy accepted contract snapshot saves"),Save->SaveGame()))return true;
   return Next(2);
  }
  if(Stage==2)
  {
   Test->TestTrue(TEXT("Accepted contract has pickup marker"),Nav->GetDisplay().Active);
   Car->GetHealthComponent()->Die();return Next(3);
  }
  if(Stage==3)
  {
   Test->TestTrue(TEXT("Damage produces actual retained dead car"),Car->GetHealthComponent()->IsDead());
   Test->TestFalse(TEXT("Dead assigned car has no misleading route"),Nav->GetDisplay().Active);
   Test->TestEqual(TEXT("Failure cannot pay"),Economy->GetBalance(),int64(1850));
   Test->TestTrue(TEXT("Player receives actionable failure feedback"),Nav->GetDisplay().Notice.ToString().Contains(TEXT("paused")));
   if(!Test->TestTrue(TEXT("Load recovers from failed delivery"),Save->LoadGame()))return true;
   return Next(4);
  }
  if(Stage==4)
  {
   Test->TestFalse(TEXT("Saved healthy condition restores"),Car->GetHealthComponent()->IsDead());
   Test->TestEqual(TEXT("Accepted state restored"),Contract->GetState(),ENeonCityTransportState::Accepted);
   Test->TestTrue(TEXT("Pickup target rebuilt with live saved car"),Nav->GetDestination().MovingTarget.Get()==Car);
   Test->TestFalse(TEXT("Reload clears stale failure notice"),Nav->GetDisplay().Notice.ToString().Contains(TEXT("paused")));
   PC->Possess(OtherCar);return Next(5);
  }
  if(Stage==5)
  {
   Test->TestEqual(TEXT("Wrong car still guides to assigned sedan"),Nav->GetDestination().DestinationID,FName(TEXT("SouthBeach.Marcus.AssignedSedan")));
   Test->TestEqual(TEXT("Wrong car gives no reward"),Economy->GetBalance(),int64(1850));
   PC->Possess(Player);Contract->DeliveryVehicle=nullptr;return Next(6);
  }
  if(Stage==6)
  {
   Test->TestFalse(TEXT("Missing assigned actor fails safely"),Nav->GetDisplay().Active);
   Test->TestEqual(TEXT("Missing car cannot pay"),Economy->GetBalance(),int64(1850));
   Contract->DeliveryVehicle=Car;return Next(7);
  }
  if(Stage==7)
  {
   Test->TestTrue(TEXT("Recovered valid actor resumes pickup marker"),Nav->GetDisplay().Active && Nav->GetDestination().MovingTarget.Get()==Car);
   const int32 Count=GI->GetSubsystem<UMessageSystem>()->GetMessages().Num();
   Test->TestTrue(TEXT("Failure messages are bounded"),Count<=5);
   Save->DeleteSave();return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerCharacter* Player=nullptr;
 ANeonCityTransportTest* Contract=nullptr;ANeonCityBaseVehicle* Car=nullptr;ANeonCityBaseVehicle* OtherCar=nullptr;
 int32 Stage=0;float At=0;double Started=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityNavigationRecoveryTest,"NeonCity.Navigation.DestinationRecoveryPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityNavigationRecoveryTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityNavigationRecovery::FRecovery>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
