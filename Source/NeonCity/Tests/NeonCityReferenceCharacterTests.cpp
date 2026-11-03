#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Presentation/CharacterAppearanceComponent.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Combat/HealthComponent.h"

namespace NeonCityReferenceCharacterTests
{
class FVerify final : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>55) {Test->AddError(TEXT("Reference character movement timeout"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<3) return false;
  const float Now=W->GetTimeSeconds();
  auto Next=[&](int32 S){Stage=S;Phase=Now;return false;};
  auto Capture=[&](const TCHAR* Name){Test->TestTrue(TEXT("Normal gameplay view captured"),FNeonCityVisualEvidence::CaptureHUD(W,Name));};
  if(Stage==0)
  {
   auto* Mesh=P->GetMesh();
   if(!Test->TestTrue(TEXT("Player uses the licensed website male skin"),Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetName()==TEXT("SK_NCTW_Wood_Male_01_Gameplay"))) return true;
   Test->TestNotNull(TEXT("Existing locomotion/combat animation wrapper retained"),Cast<UCharacterPresentationAnimInstance>(Mesh->GetAnimInstance()));
   Test->TestNotNull(TEXT("Existing ragdoll asset retained"),Mesh->GetPhysicsAsset());
   Test->TestTrue(TEXT("Full-scale human render bounds"),Mesh->Bounds.BoxExtent.Z>75 && Mesh->Bounds.BoxExtent.Z<125);
   Test->TestTrue(TEXT("No extra animation or presentation tick"),!P->FindComponentByClass<UCharacterAppearanceComponent>()->PrimaryComponentTick.bCanEverTick);
   TSet<FName> Skins;
   for(TActorIterator<ANeonCityNPCCharacter> It(W);It;++It)
   {
    if(const auto* Skin=It->GetMesh()->GetSkeletalMeshAsset()) Skins.Add(Skin->GetFName());
    Test->TestNotNull(TEXT("Pedestrians keep their existing animation layer"),Cast<UCharacterPresentationAnimInstance>(It->GetMesh()->GetAnimInstance()));
   }
   Test->TestTrue(TEXT("Stable pedestrian identities use both reviewed people"),Skins.Contains(TEXT("SK_NCTW_Wood_Male_01_Gameplay")) && Skins.Contains(TEXT("SK_NCTW_Female_Adult_04_Gameplay")));
   P->SetActorLocationAndRotation(FVector(2200,-800,136),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation(FRotator(-8,0,0));
   InitialHealth=P->GetHealthComponent()->GetCurrentHealth(); return Next(1);
  }
  Evidence.Record(W,PC,Stage);
  if(Stage==1 && Now-Phase>1.2f) {Capture(TEXT("ReferencePerson-Back"));PC->SetControlRotation(FRotator(-8,180,0));return Next(2);}
  if(Stage==2 && Now-Phase>1.2f)
  {
   Capture(TEXT("ReferencePerson-Front"));PC->SetControlRotation(FRotator(-8,0,0));Start=P->GetActorLocation();return Next(3);
  }
  if(Stage==3)
  {
   P->DoMove(0,1);
   if(Now-Phase<1.f) return false;
   Test->TestTrue(TEXT("Original template movement carries the source skin"),FVector::Dist2D(Start,P->GetActorLocation())>180);
   Capture(TEXT("ReferencePerson-Walk"));P->DoJumpStart();return Next(4);
  }
  if(Stage==4)
  {
   bFell|=P->GetCharacterMovement()->IsFalling();
   if(Now-Phase<.35f) return false;
   Capture(TEXT("ReferencePerson-Jump"));P->DoJumpEnd();return Next(5);
  }
  if(Stage==5 && Now-Phase>1.5f)
  {
   Test->TestTrue(TEXT("Template jump and landing remain functional"),bFell && P->GetCharacterMovement()->IsMovingOnGround());
   Test->TestEqual(TEXT("Presentation does not change player health"),P->GetHealthComponent()->GetCurrentHealth(),InitialHealth);
   Capture(TEXT("ReferencePerson-Landed"));return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Started=0;float Phase=0,InitialHealth=0;int32 Stage=0;bool bFell=false;
 FVector Start=FVector::ZeroVector;FNeonCityVisualEvidence Evidence{TEXT("ReferenceCharacter")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityReferenceCharacter,"NeonCity.VerticalSlice.ReferenceCharacterPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityReferenceCharacter::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityReferenceCharacterTests::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
