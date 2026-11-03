#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/NeonCityMainlandSite.h"
#include "Property/PropertySubsystem.h"
#include "Property/PropertyComponent.h"
#include "Property/NeonCityStarterApartment.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Components/CapsuleComponent.h"

namespace MainlandPIE920
{
class FIntegration : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Start=0;bool Checked=false;
public:
 explicit FIntegration(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(!Start)Start=Now;
  if(Now-Start>65){Test->AddError(TEXT("Actual editor PIE integration exceeded65s"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<8)return false;
  if(!Checked)
  {
   if(!Test->TestNotNull(TEXT("Replacement site in actual PIE world"),ANeonCityMainlandSite::Find(W)))return true;
   auto* Registry=W->GetGameInstance()->GetSubsystem<UPropertySubsystem>();
   ANeonCityStarterApartment* Home=nullptr;
   for(TActorIterator<ANeonCityStarterApartment> I(W);I;++I)
   {if(Home){Test->AddError(TEXT("PIE has duplicate starter home"));return true;}Home=*I;}
   if(!Test->TestNotNull(TEXT("Actual fitted starter home"),Home))return true;
   FNeonCityPropertyRecord Record;FTransform Spawn;
   if(!Test->TestTrue(TEXT("Fitted starter home is live and owned"),Registry->GetProperty(Home->GetPropertyComponent()->GetPropertyID(),Record)&&Record.Owned))return true;
   Test->TestEqual(TEXT("PIE property navigation uses new address"),Record.Address.ToString(),FString(TEXT("3 Harbour Gallery")));
   // the lakeside residence is the preferred home now; the starter flat stays owned with its own spawn
   Test->TestTrue(TEXT("PIE home spawn resolves"),Registry->GetHomeSpawnTransform(Spawn)&&!Spawn.ContainsNaN());
   Test->TestFalse(TEXT("PIE starter home spawn is valid"),Home->GetPropertyComponent()->GetHomeSpawnTransform().ContainsNaN());
   Test->TestTrue(TEXT("PIE garage catalogue uses the actual interior bay"),Record.GarageSlots.Num()==1&&Record.GarageSlots[0].Location.Equals(FVector(3799.5,-830,55),.1f));
   // The authored PlayerStart is the lakeside residence now (NeonCity.WestLake.ResidencePIE checks it); the
   // promenade walk starts from its old spot, put down on the ground there.
   APlayerStart* Authored=nullptr;for(TActorIterator<APlayerStart> I(W);I;++I)Authored=*I;
   if(!Authored || FVector::Dist2D(Player->GetActorLocation(),Authored->GetActorLocation())>200)
   {Test->AddError(TEXT("PIE did not start at the authored normal PlayerStart"));return true;}
   FHitResult Ground;FCollisionQueryParams Q(TEXT("PromenadeStart"),true,Player);
   if(!W->LineTraceSingleByChannel(Ground,FVector(-7000,-2400,2000),FVector(-7000,-2400,-500),ECC_Visibility,Q))
   {Test->AddError(TEXT("No promenade ground at the walk start"));return true;}
   Player->SetActorLocation(Ground.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);
   Checked=true;return false;
  }
  const FVector P=Player->GetActorLocation(),Goal(-5800,-2400,0);
  if(P.Z<100 || P.Z>300){Test->AddError(TEXT("PIE player lost real promenade support"));return true;}
  if(FVector::Dist2D(P,Goal)<60)
  {
   Player->GetCharacterMovement()->StopMovementImmediately();
   Test->TestTrue(TEXT("PIE normal player remains grounded after continuous12m walk"),Player->GetCharacterMovement()->IsMovingOnGround());
   Test->AddInfo(TEXT("Actual editor PIE: new live property/address/garage and normal12m character walk; no pose fallback or collision changes."));
   return true;
  }
  PC->SetControlRotation((Goal-P).Rotation());Player->AddMovementInput((Goal-P).GetSafeNormal2D(),1);return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPIEIntegration920,"NeonCity.Phase205c.MainlandC920.PIEIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPIEIntegration920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("Use isolated PIE without the user's save slot"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(MainlandPIE920::FIntegration(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
