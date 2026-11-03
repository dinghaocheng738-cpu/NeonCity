#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
namespace
{
class FShoreWalk:public IAutomationLatentCommand
{
public:
 explicit FShoreWalk(FAutomationTestBase* T):Test(T),Evidence(TEXT("I-ShoreWalkSwimReturn")) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>45) { Test->AddError(FString::Printf(TEXT("Shore route timed out at stage %d, position %s, movement mode %d"),Stage,*LastPosition.ToString(),LastMode));return true; }
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<3) return false;
  auto* Move=P->GetCharacterMovement();
  LastPosition=P->GetActorLocation();LastMode=int32(Move->MovementMode);
  if(Stage==0)
  {
   // Start fixture only. The slope, water entry and return use native movement.
   P->SetActorLocation(FVector(1000,-2200,130),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);At=W->GetTimeSeconds();Stage=1;return false;
  }
  Evidence.Record(W,PC,Stage);const float E=W->GetTimeSeconds()-At;
  PC->SetControlRotation(FRotator(-12,Stage<4?-90:90,0));
  if(Stage==1 && E>.6f) { if(!Test->TestTrue(TEXT("Player starts on beach support"),Move->IsMovingOnGround())) return true;Stage=2; }
  if(Stage==2)
  {
   P->AddMovementInput(FVector(0,-1,0));if(P->GetActorLocation().Y>-3210) return false;
   Test->TestTrue(TEXT("Walkable slope retains floor"),Move->IsMovingOnGround());
   Test->TestTrue(TEXT("Actual floor descends with rendered shore"),Move->CurrentFloor.HitResult.ImpactPoint.Z< -110);
   const float Feet=P->GetActorLocation().Z-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   Test->TestTrue(TEXT("Feet stay on slope, not inside visual sand"),FMath::Abs(Feet-Move->CurrentFloor.HitResult.ImpactPoint.Z)<8);
   Stage=3;
  }
  if(Stage==3)
  {
   // The authored seabed slopes to Y=-4300. Reach water beyond that support
   // before asserting swimming; shallow-water walking is valid on the slope.
   P->AddMovementInput(FVector(0,-1,0));if(P->GetActorLocation().Y>-4650) return false;
   Test->AddInfo(FString::Printf(TEXT("Deep-water checkpoint: position=%s mode=%d floor=%s at=%s"),
    *P->GetActorLocation().ToString(),int32(Move->MovementMode),*GetNameSafe(Move->CurrentFloor.HitResult.GetActor()),*Move->CurrentFloor.HitResult.ImpactPoint.ToString()));
   if(!Test->TestTrue(TEXT("Beyond shore enters existing swimming"),Move->IsSwimming())) return true;
   Stage=4;
  }
  if(Stage==4)
  {
   P->AddMovementInput(FVector(0,1,0));if(P->GetActorLocation().Y< -2780) return false;
   Test->TestTrue(TEXT("Returning up shore restores walking"),Move->IsMovingOnGround());return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;int32 Stage=0,LastMode=0;FVector LastPosition=FVector::ZeroVector;float At=0;double Started=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullArtShore,"NeonCity.FullArt.ShoreWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFullArtShore::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FShoreWalk>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
