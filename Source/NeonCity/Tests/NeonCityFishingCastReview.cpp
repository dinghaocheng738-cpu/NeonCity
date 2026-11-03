#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/FishingStation.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Interaction/InteractionComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
namespace
{
class FShoreCastReview : public IAutomationLatentCommand
{
public:
 explicit FShoreCastReview(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150){Test->AddError(FString::Printf(TEXT("Shore cast review timed out at stage %d"),Stage));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Fishing=PC->FindComponentByClass<UFishingComponent>();
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  const auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  const auto Next=[&](int32 S){Stage=S;At=Now;};
  const auto Shot=[&](const TCHAR* N){FNeonCityVisualEvidence::CaptureHUD(W,N);};
  Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   for(TActorIterator<AFishingStation> It(W);It;++It)if(It->StationID==TEXT("SouthBeach.Fishing.Pier"))Point=*It;
   if(!Test->TestNotNull(TEXT("Existing authored shore fishing point"),Point))return true;
   P->GetCombatAdapter()->StowForUtility();
   P->SetActorLocation(Point->GetActorTransform().TransformPosition(FVector(-140,-160,99)),false,nullptr,ETeleportType::TeleportPhysics);
   P->SetActorRotation(Point->GetActorRotation());P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
   PC->SetControlRotation((Point->InteractionBounds->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   Next(1);return false;
  }
  if(Stage==1 && Age>1)
  {
   if(!Test->TestTrue(TEXT("Actual pier is reachable without placing a test platform"),P->GetInteractionComponent()->RefreshFocusedInteractable()==Point && P->GetCharacterMovement()->IsMovingOnGround()))return true;
   Key(EKeys::F,IE_Pressed);Next(2);
  }
  else if(Stage==2 && Age>.3f)
  {
   Key(EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("Ordinary F enters shore fishing"),Fishing->IsActive()))return true;
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(60);
   const FVector Look=P->GetActorTransform().TransformPosition(FVector(35,0,85));
   const FVector From=P->GetActorTransform().TransformPosition(FVector(100,-430,140));
   Camera->SetActorLocationAndRotation(From,(Look-From).Rotation());PC->SetViewTarget(Camera);
   Next(3);
  }
  else if(Stage==3 && Age>1){Shot(TEXT("Cast-R3-Ready.png"));Key(EKeys::LeftMouseButton,IE_Pressed);Next(4);}
  else if(Stage==4 && Fishing->GetCastMotionCurve(TEXT("CastReady"))>=.99f && Fishing->GetCastPoseTime()>1.2f)
  {Shot(TEXT("Cast-R3-Perfect.png"));Key(EKeys::LeftMouseButton,IE_Released);Next(5);}
  else if(Stage==5)
  {
   if(!Test->TestTrue(TEXT("Shore cast remains active"),Fishing->IsActive()))return true;
   Test->TestTrue(TEXT("Actual animation grants Perfect"),Fishing->GetSession().PerfectCast);
   if(Fishing->GetCastMotionCurve(TEXT("LineReleased"))>=.99f && !Released){Released=true;Shot(TEXT("Cast-R3-LineRelease.png"));}
   if(Fishing->GetSession().State==ENeonCityFishingState::Waiting){Shot(TEXT("Cast-R3-Landed.png"));Next(6);}
  }
  else if(Stage==6 && Age>2)
  {
   Test->TestTrue(TEXT("Actual shore cast completes its follow-through without cancellation"),Fishing->IsActive() && Fishing->GetCastPoseIndex()==0);
   Test->TestTrue(TEXT("Forward stroke released the actual line"),Released);
   // This fixed legacy pier is currently quiet water. Do not inject a fish to
   // manufacture a bite. ReferenceBoatLoopPIE owns the real catch/reel check.
   Test->AddInfo(TEXT("Shore animation/landing review only; complete ecological bite/reel/keep remains required in ReferenceBoatLoopPIE."));
   Shot(TEXT("Cast-R3-RecoveredPose.png"));Fishing->Stop();PC->SetViewTarget(P);Camera->Destroy();return true;
  }
  return false;
 }
 FAutomationTestBase* Test;AFishingStation* Point=nullptr;ACameraActor* Camera=nullptr;
 FNeonCityVisualEvidence Evidence{TEXT("ShoreCast")};double Started=0;float At=0;int32 Stage=0;bool Released=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShoreCastPIE,"NeonCity.Fishing.ShoreCastPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShoreCastPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FShoreCastReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
