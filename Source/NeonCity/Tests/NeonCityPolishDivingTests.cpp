#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/CoastalWildlife.h"
#include "Adventure/DiveLocker.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Water/UnderwaterViewComponent.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"

namespace
{
class FPolishDiveCheck : public IAutomationLatentCommand
{
public:
 explicit FPolishDiveCheck(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100){Test->AddError(FString::Printf(TEXT("Diving PIE timeout at stage %d"),Stage));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Dive=P->FindComponentByClass<UDivingComponent>();auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-174000,0))))return false;
   Test->TestNotNull(TEXT("Diving adapter belongs to ordinary player"),Dive);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   // Verify the ordinary authored habitat, without installing test-only fish.
   auto* Candidate=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Marine/SK_AdventureReefFish"));
   auto* Swim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Marine/SK_AdventureReefFish_Swim"));
   if(!Test->TestNotNull(TEXT("Reviewed fish candidate"),Candidate) || !Test->TestNotNull(TEXT("Retained swimming animation"),Swim))return true;
   Test->TestTrue(TEXT("Fish close-view length remains below 1 metre"),Candidate->GetBounds().BoxExtent.GetMax()<50);
   TSet<USkeletalMesh*> Presentations;TSet<FName> Schools;
   for(TActorIterator<ACoastalWildlife> It(W);It;++It)
   {
    if(It->bBird)continue;
    ++Count;Presentations.Add(It->AnimatedBody->GetSkeletalMeshAsset());Schools.Add(It->SchoolID);
    if(It->bCapturable && FVector::Dist2D(It->GetActorLocation(),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-175000,0)))<1600 && !Fish.IsValid())
    {
     // Select an accessible ordinary fish, without moving or freezing the school.
     const FVector Start=It->GetActorLocation()+FVector(-195,0,-30);
     FCollisionQueryParams Q(SCENE_QUERY_STAT(DiveApproach),false,P);FHitResult H;
     const bool Obstructed=W->SweepSingleByObjectType(H,Start,Start+FVector(0,0,1),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(45,95),Q)
      || W->LineTraceSingleByObjectType(H,Start+FVector(0,0,64),It->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
     if(!Obstructed){Fish=*It;ApproachStart=Start;}
    }
    Test->TestTrue(TEXT("Authored fish has a skeletal presentation and swimming animation"),It->AnimatedBody->GetSkeletalMeshAsset() && It->SwimAnimation);
   }
   if(!Test->TestEqual(TEXT("162 fish distributed in ordinary world"),Count,162))return true;
   Test->TestEqual(TEXT("Three genuinely distinct skeletal presentations"),Presentations.Num(),3);
   Test->TestTrue(TEXT("Independent ecological schools"),Schools.Num()>=20);
   if(!Test->TestTrue(TEXT("Accessible capturable fish found"),Fish.IsValid()))return true;
   LandTransform=P->GetActorTransform();
   if(!Test->TestTrue(TEXT("Equip on land"),Dive->Equip()))return true;
   Test->TestFalse(TEXT("Equipment initially visible in third person"),Dive->IsFirstPerson());
   PC->SetControlRotation(FRotator(-8,P->GetActorRotation().Yaw+180,0));At=W->GetTimeSeconds();Stage=10;return false;
  }
  if(Stage==10)
  {
   if(W->GetTimeSeconds()-At<3)return false;
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Dive-Suit-ThirdPerson.png"));
   // Schools keep swimming during the suit shot. Re-evaluate the approach
   // against their current position and authored collision before entering.
   Fish.Reset();
   for(TActorIterator<ACoastalWildlife> It(W);It;++It)
   {
    if(It->bBird || !It->bCapturable || FVector::Dist2D(It->GetActorLocation(),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-175000,0)))>=1600)continue;
    const FVector Start=It->GetActorLocation()+FVector(-195,0,-30);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(DiveLiveApproach),false,P);FHitResult H;
    if(W->SweepSingleByObjectType(H,Start,Start+FVector(0,0,1),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(45,95),Q)
     || W->LineTraceSingleByObjectType(H,Start+FVector(0,0,64),It->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Q))continue;
    Fish=*It;ApproachStart=Start;break;
   }
   if(!Test->TestTrue(TEXT("Moving school still has an accessible approach"),Fish.IsValid()))return true;
   Dive->SetFirstPerson(true);
   Before=Inventory->GetFish().Num();OldDrain=P->FindComponentByClass<UOxygenComponent>()->DrainRate;
   P->SetActorLocation(ApproachStart,false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();At=W->GetTimeSeconds();Stage=1;return false;
  }
  if(!Fish.IsValid() && Stage<3){Test->AddError(TEXT("Fish vanished before collection"));return true;}
  if(Stage<4 && Fish.IsValid())PC->SetControlRotation((Fish->GetActorLocation()-P->GetFollowCamera()->GetComponentLocation()).Rotation());
  if(Stage==1 && W->GetTimeSeconds()-At>1)
  {
   if(!Test->TestTrue(TEXT("Real water volume reports underwater"),P->FindComponentByClass<UWaterInteractionComponent>()->IsFullySubmerged()))return true;
   Test->TestTrue(TEXT("Actual first-person camera is inside optical water"),P->FindComponentByClass<UUnderwaterViewComponent>()->GetImmersionWeight()>.99f);
   Key(EKeys::F,IE_Pressed);At=W->GetTimeSeconds();Stage=2;return false;
  }
  if(Stage==2 && W->GetTimeSeconds()-At>.25)
  {
   Key(EKeys::F,IE_Released);Test->TestEqual(TEXT("F cannot award fish"),Inventory->GetFish().Num(),Before);
   Test->TestTrue(TEXT("Underwater school and first-person net captured"),FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Dive-Before.png")));
   Key(EKeys::RightMouseButton,IE_Pressed);At=W->GetTimeSeconds();Stage=3;return false;
  }
  if(Stage==3)
  {
   if(Fish.IsValid())
   {
    const FVector Approach=Fish->GetActorLocation()-P->GetFollowCamera()->GetComponentLocation();
    if(Approach.Size()>180)P->AddMovementInput(Approach.GetSafeNormal(),.35f);
   }
   if(!bPressedSweep && Dive->CanStartSweep()){Key(EKeys::LeftMouseButton,IE_Pressed);bPressedSweep=true;}
   if(bPressedSweep && Dive->GetCaptureStage()==EDiveCaptureStage::Recovering){Key(EKeys::LeftMouseButton,IE_Released);bPressedSweep=false;}
   if(Inventory->GetFish().Num()==Before+1){Stage=4;At=W->GetTimeSeconds();return false;}
   if(W->GetTimeSeconds()-At>12){Test->AddError(FString::Printf(TEXT("Held tool did not catch: progress %.2f stage %d target distance %.1f"),Dive->GetCaptureProgress(),int32(Dive->GetCaptureStage()),Fish.IsValid()?FVector::Distance(Fish->GetActorLocation(),P->GetFollowCamera()->GetComponentLocation()):-1));return true;}
  }
  if(Stage==4)
  {
   Key(EKeys::RightMouseButton,IE_Released);Key(EKeys::LeftMouseButton,IE_Released);
   Test->TestFalse(TEXT("Captured creature removed from habitat"),Fish.IsValid());
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Dive-Caught.png"));
   const auto ID=Inventory->GetFish().Last().FishID;const int32 Meals=Inventory->GetMeals().Num();
   Test->TestTrue(TEXT("Captured fish uses existing cookable inventory"),Inventory->CookFish(ID));
   Test->TestEqual(TEXT("Exactly one meal"),Inventory->GetMeals().Num(),Meals+1);
   Test->TestFalse(TEXT("Cannot cook the same catch twice"),Inventory->CookFish(ID));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(22);
   Stage=5;At=W->GetTimeSeconds();return false;
  }
  if(Stage==5 && W->GetTimeSeconds()-At>18)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Polish-Dive-Night.png"));
   Dive->Unequip();Test->TestTrue(TEXT("Oxygen consumption restored"),P->FindComponentByClass<UOxygenComponent>()->DrainRate>OldDrain);
   P->SetActorTransform(LandTransform,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Stage=6;At=W->GetTimeSeconds();return false;
  }
  if(Stage==6 && W->GetTimeSeconds()-At>1)
  {
   Test->TestEqual(TEXT("Leaving water removes the camera effect"),P->FindComponentByClass<UUnderwaterViewComponent>()->GetImmersionWeight(),0.f);return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;TWeakObjectPtr<ACoastalWildlife> Fish;
 double Started=0;float At=0,OldDrain=0;int32 Stage=0,Count=0,Before=0;FTransform LandTransform;FVector ApproachStart;bool bPressedSweep=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishDivePIE,"NeonCity.Polish.DivingPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPolishDivePIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(FPolishDiveCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
