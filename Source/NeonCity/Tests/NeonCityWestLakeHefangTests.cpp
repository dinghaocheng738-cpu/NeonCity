#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Property/NeonCityPropertySign.h"
#include "Property/PropertySubsystem.h"
#include "Property/PropertyData.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandLandCluster.h"
#include "Engine/GameInstance.h"

namespace NeonCityWestLakeHefang
{
/** Normal PIE on the West Lake map: Hefang Street stands at the foot of Wushan (its shop rows, both paifang gates,
 * walkable paving on the cut terrace) with four shops for sale. With enough money the player buys the Xihu Teahouse
 * at its sale board (paid from cash, then the bank), the board turns to "yours", and when the clock passes midnight
 * the teahouse's daily takings are paid once into the wallet (a repeated pay day pays nothing more). */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(FString::Printf(TEXT("Hefang check timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  ACharacter* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<4 || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
  auto* Economy=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();auto* Props=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
  ANeonCityDayNightCycle* Cycle=nullptr;for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)Cycle=*It;
  if(Stage==0)
  {
   int32 Shops=0,Gates=0;
   for(TActorIterator<AIslandLandCluster> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.Hefang")))
    for(auto* C:It->GetComponents())if(auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(C))
    {
     const FString Mesh=GetNameSafe(H->GetStaticMesh());
     if(Mesh.StartsWith(TEXT("SM_WL_HF_Shop_")))Shops+=H->GetInstanceCount();
     if(Mesh==TEXT("SM_WL_HF_Paifang"))Gates+=H->GetInstanceCount();
    }
   Test->TestTrue(FString::Printf(TEXT("Hefang Street shop rows (%d shops)"),Shops),Shops>=60);
   Test->TestEqual(TEXT("a paifang gate at each end"),Gates,2);
   for(TActorIterator<ANeonCityPropertySign> It(W);It;++It){++Signs;if(It->Data && It->Data->PropertyID==TEXT("WestLake.Hefang.Teahouse"))Sign=*It;}
   Test->TestEqual(TEXT("four Hefang shops for sale"),Signs,4);
   if(!Test->TestNotNull(TEXT("Xihu Teahouse sale board"),Sign) || !Cycle)return true;
   Test->TestFalse(TEXT("teahouse not owned at the start"),Props->IsOwned(Sign->Data->PropertyID));
   // the street is a walkable terrace at city ground: stand the player in front of the teahouse
   // the board faces the street along its local +y: stand 1.5 m in front of it
   Player->SetActorLocation(Sign->GetActorLocation()+Sign->GetActorRightVector()*150+FVector(0,0,110),false,nullptr,ETeleportType::TeleportPhysics);
   FText Why;Test->TestFalse(TEXT("cannot buy without the money"),Economy->GetBalance()+Economy->GetBankBalance()>=Sign->Data->Price || Props->Purchase(Sign->Data->PropertyID,Why));
   Economy->AddMoney(Sign->Data->Price+500,TEXT("Test.HefangFunds"));
   Cash=Economy->GetBalance();
   return Next(1);
  }
  if(Stage==1)
  {
   if(Elapsed()<1)return false;
   Test->TestTrue(TEXT("player stands on the Hefang paving"),Player->GetCharacterMovement()->IsMovingOnGround() && FMath::Abs(Player->GetActorLocation().Z-Sign->GetActorLocation().Z-Player->GetDefaultHalfHeight())<60);
   IInteractableInterface::Execute_Interact(Sign,Player);
   Test->TestTrue(TEXT("teahouse bought at its sale board"),Props->IsOwned(Sign->Data->PropertyID));
   Test->TestEqual(TEXT("price paid"),Cash-Economy->GetBalance(),Sign->Data->Price);
   Test->TestTrue(TEXT("board shows it is yours"),Sign->IsOwned());
   Cash=Economy->GetBalance();Day=Cycle->GetGameDay();
   Cycle->SetTimeOfDay(23.99f);
   return Next(2);
  }
  if(Stage==2)
  {
   if(Cycle->GetGameDay()==Day)return Elapsed()>40?(Test->AddError(TEXT("the clock never passed midnight")),true):false;
   const int64 Paid=Economy->GetBalance()-Cash;
   Test->TestEqual(TEXT("one day of teahouse takings paid at midnight"),Paid,Sign->Data->DailyIncome);
   Test->TestEqual(TEXT("a repeated pay day pays nothing more"),Props->SettleBusinessIncome(Cycle->GetGameDay()),int64(0));
   return true;
  }
  return true;
 }
private:
 double Elapsed() const { return FPlatformTime::Seconds()-At; }
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0,Signs=0,Day=0;int64 Cash=0;ANeonCityPropertySign* Sign=nullptr;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeHefangPIE,"NeonCity.WestLake.HefangShopPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeHefangPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeHefang::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
