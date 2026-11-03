#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Water/SwimmingComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Combat/HealthComponent.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "EngineUtils.h"
namespace {
class FUnderwaterArmsReview : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int32 Stage=0;double Start=0;float At=0;bool Began=false;
 FNeonCityVisualEvidence Evidence{TEXT("UnderwaterArms")};
public:
 explicit FUnderwaterArmsReview(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>220){Test->AddError(TEXT("Arm review timeout"));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Kit=P->FindComponentByClass<UDivingComponent>();auto* E=P->FindComponentByClass<UDiveEquipmentComponent>();
  auto* A=Cast<UCharacterPresentationAnimInstance>(P->GetMesh()->GetAnimInstance());
  auto Key=[&](FKey K,EInputEvent Event){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));};
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  auto Next=[&](int32 N){Stage=N;At=Now;Began=false;};
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,0))))return false;
   if(!Test->TestTrue(TEXT("Accepted R05 equips for animation fixture"),Kit && Kit->Equip()))return true;
   P->SetActorLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,-500)),false,nullptr,ETeleportType::TeleportPhysics);P->SetActorRotation(FRotator::ZeroRotator);
   P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
   P->GetCameraBoom()->TargetArmLength=275;P->GetCameraBoom()->SocketOffset=FVector(0,0,15);PC->SetControlRotation(FRotator(-12,145,0));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(11);}
   // An already injured diver, without introducing a separate stagger/gear
   // cleanup event while establishing the animation fixture.
   auto* Health=P->FindComponentByClass<UHealthComponent>();Health->ApplyPersistenceHealth(Health->GetMaxHealth()*.45f,Health->GetMaxHealth());
   Key(EKeys::B,IE_Pressed);Next(1);return false;
  }
  Evidence.Record(W,PC,Stage);
  if(A && Age>1)
  {
   if(Age>2 && (!Kit->IsEquipped() || A->EquipmentWaterAlpha<.99f))
   {Test->AddError(TEXT("Animation fixture lost R05 or the full ocean layer"));return true;}
   if(A->ArmedAlpha>.01 || A->AimAlpha>.01 || A->FishingAlpha>.01 || A->FlashlightAlpha>.01 || A->SupportHandAlpha>.01 || A->InjuryAlpha>.01)
   {Test->AddError(TEXT("Land upper-body layer contaminated ocean pose"));return true;}
  }
  if(Stage==1)
  {
   Key(EKeys::B,IE_Released);
   // Ten seconds of actual no-input swimming, not a frozen animation preview.
   if(Age>2 && Age<12 && A && A->OceanPose!=19){Test->AddError(FString::Printf(TEXT("No-input submerged body selected %d instead of Neutral"),A->OceanPose));return true;}
   if(Age>12){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("01-Neutral-Rear45.png"));Next(2);}
  }
  else if(Stage>=2 && Stage<=5)
  {
   const float Yaws[]={180,0,90,-90};PC->SetControlRotation(FRotator(-10,Yaws[Stage-2],0));
   if(Age>4){FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Neutral-Angle-%d.png"),Stage));if(Stage==5)P->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(100);Next(Stage+1);}
  }
  else if(Stage==6)
  {
   PC->SetControlRotation(FRotator(-8,0,0));P->AddMovementInput(FVector(1,0,0),.24f);
   if(Age>5){Test->TestTrue(TEXT("Slow swimming uses a distinct cycle"),A && (A->OceanPose==2 || A->OceanPose==3));Next(7);}
  }
  else if(Stage==7)
  {
   P->AddMovementInput(FVector(1,0,0),1);
   if(Age>5){Test->TestEqual(TEXT("Normal R05 max speed reaches fast cycle"),A->OceanPose,4);Next(8);}
  }
  else if(Stage==8)
  {
   P->AddMovementInput(FRotator(0,Age*38,0).Vector(),.7f);
   if(Age>4){Next(9);}
  }
  else if(Stage==9 && Age>5)
  {Test->TestEqual(TEXT("Stopping returns to Neutral"),A->OceanPose,19);Key(EKeys::SpaceBar,IE_Pressed);Next(10);}
  else if(Stage==10)
  {
   if(Age>3){Test->TestEqual(TEXT("Rising uses ascend"),A->OceanPose,7);Key(EKeys::SpaceBar,IE_Released);Key(EKeys::C,IE_Pressed);Next(11);}
  }
  else if(Stage==11)
  {
   if(Age>4){Test->TestEqual(TEXT("Diving uses descend"),A->OceanPose,8);Key(EKeys::C,IE_Released);Next(12);}
  }
  else if(Stage==12 && Age>4){Key(EKeys::G,IE_Pressed);Next(13);}
  else if(Stage==13)
  {
   Key(EKeys::G,IE_Released);
   if(Age>5){Test->TestTrue(TEXT("Normal G deploys third-person spear hold"),E->IsSpeargunDeployed() && A->OceanPose==12);Key(EKeys::G,IE_Pressed);Next(14);}
  }
  else if(Stage==14)
  {
   Key(EKeys::G,IE_Released);
   if(Age>4){Test->TestTrue(TEXT("G stow leaves no spear pose"),!E->IsSpeargunDeployed() && A->OceanPose==19);Key(EKeys::Q,IE_Pressed);Next(15);}
  }
  else if(Stage==15)
  {
   Key(EKeys::Q,IE_Released);
   if(Age>5){Test->TestTrue(TEXT("Q deploys third-person DPV hold"),E->IsDPVDeployed() && A->OceanPose==16);Key(EKeys::Q,IE_Pressed);Next(16);}
  }
  else if(Stage==16)
  {
   Key(EKeys::Q,IE_Released);
   if(Age>4){Test->TestTrue(TEXT("Q stow leaves neutral empty hands"),!E->IsDPVDeployed() && !E->IsSpeargunDeployed() && A->OceanPose==19);return true;}
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityUnderwaterArms,TEXT("NeonCity.OceanExperience.UnderwaterArmsPIE"),EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityUnderwaterArms::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(FUnderwaterArmsReview(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
