#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Interaction/InteractableInterface.h"
#include "Interaction/InteractionComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "InputCoreTypes.h"
#include "Vehicles/Marine/NeonCityLamborghini63.h"

namespace NeonCityWestLakeYachtWalk
{
/** Living aboard the Lamborghini 63, the way a player does it: swim to the stern and climb out onto the swim
 * platform (F), walk up the three stern tiers onto the aft deck, take the helm at the wheel (F) and leave it again,
 * step ashore from the stern while alongside the quay (F), and board again from the quay (F). Throughout, the
 * player stands on the boat (walking, based on it), never hanging in the falling state. */
class FWalk:public IAutomationLatentCommand
{
public:
 explicit FWalk(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>200){Test->AddError(FString::Printf(TEXT("Yacht walk timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<5)return false;
  if(!Boat.IsValid())
  {
   ACharacter* Player=Cast<ACharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<ANeonCityLamborghini63> It(W);It;++It)if(!It->ActorHasTag(TEXT("WL.Leisure"))){Boat=*It;break;}      // the player's own 63, not the boat club's
   if(!Test->TestNotNull(TEXT("the Lamborghini 63 at its berth"),Boat.Get()))return true;
   Me=Player;
  }
  const double E=FPlatformTime::Seconds()-At;const FTransform X=Boat->GetActorTransform();
  ACharacter* P=Me.Get();if(!P){Test->AddError(TEXT("player character lost"));return true;}
  const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();auto* M=P->GetCharacterMovement();
  auto Local=[&](){return X.InverseTransformPosition(P->GetActorLocation()-FVector(0,0,Half));};
  auto OnBoat=[&](){return M->IsMovingOnGround() && P->GetMovementBase() && P->GetMovementBase()->GetOwner()==Boat.Get();};
  if(Stage==0)
  {
   // in the lake a few metres astern of the platform
   M->StopMovementImmediately();
   P->SetActorLocation(X.TransformPosition(FVector(-1180,0,-70)),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(1);
  }
  if(Stage==1)
  {
   if(E<2)return false;
   const auto Action=Boat->GetActionFor(P);
   Test->TestEqual(FString::Printf(TEXT("in the water at the stern, F climbs aboard (mode %d)"),int32(M->MovementMode.GetValue())),int32(Action),int32(ENeonCityYachtAction::ClimbAboard));
   Test->TestEqual(TEXT("prompt reads Climb aboard"),Boat->GetInteractionName_Implementation().ToString(),FString(TEXT("Climb aboard")));
   PressF(PC);
   return Next(2);
  }
  if(Stage==2)
  {
   if(E<1.5)return false;
   const FVector L=Local();
   Test->TestTrue(FString::Printf(TEXT("standing on the swim platform (feet local %s, mode %d)"),*L.ToCompactString(),int32(M->MovementMode.GetValue())),OnBoat() && L.X<-930 && FMath::Abs(L.Z-47)<12);
   PC->SetControlRotation(FRotator(0,Boat->GetActorRotation().Yaw,0));
   return Next(3);
  }
  if(Stage==3)
  {
   // walk forward up the stern tiers
   P->AddMovementInput(Boat->GetActorForwardVector(),1.f);
   if(E>.3 && M->IsFalling())Falling+=W->GetDeltaSeconds();
   if(E<3.5)return false;
   const FVector L=Local();
   Test->TestTrue(FString::Printf(TEXT("walked up the tiers onto the aft deck (feet local %s)"),*L.ToCompactString()),L.Z>125 && L.X>-860 && OnBoat());
   Test->TestTrue(FString::Printf(TEXT("on its feet up the steps (%.2f s falling)"),Falling),Falling<.6f);
   // to the wheel
   M->StopMovementImmediately();
   P->SetActorLocation(X.TransformPosition(FVector(-60,-10,102+Half+4)),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(4);
  }
  if(Stage==4)
  {
   if(E<1.5)return false;
   Test->TestTrue(TEXT("standing in the cockpit"),OnBoat());
   Test->TestEqual(TEXT("at the wheel, F takes the helm"),int32(Boat->GetActionFor(P)),int32(ENeonCityYachtAction::TakeHelm));
   PressF(PC);
   return Next(5);
  }
  if(Stage==5)
  {
   if(E<1.5)return false;
   Test->TestTrue(TEXT("driving the 63"),PC->GetPawn()==Boat.Get());
   Test->TestTrue(TEXT("leaves the helm"),Boat->ExitVehicle_Implementation());
   return Next(6);
  }
  if(Stage==6)
  {
   if(E<1.5)return false;
   Test->TestTrue(FString::Printf(TEXT("back on deck after the helm (feet local %s)"),*Local().ToCompactString()),PC->GetPawn()==P && OnBoat());
   M->StopMovementImmediately();
   P->SetActorLocation(X.TransformPosition(FVector(-830,0,131+Half+4)),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(7);
  }
  if(Stage==7)
  {
   if(E<1.5)return false;
   Test->TestTrue(TEXT("standing on the stern tier"),OnBoat());
   Test->TestEqual(TEXT("at the stern alongside, F steps ashore"),int32(Boat->GetActionFor(P)),int32(ENeonCityYachtAction::GoAshore));
   PressF(PC);
   return Next(8);
  }
  if(Stage==8)
  {
   if(E<1.5)return false;
   Test->TestTrue(FString::Printf(TEXT("ashore on the quay (%.0f cm from the hull centre, mode %d)"),FVector::Dist2D(P->GetActorLocation(),Boat->GetActorLocation()),int32(M->MovementMode.GetValue())),
    M->IsMovingOnGround() && (!P->GetMovementBase() || P->GetMovementBase()->GetOwner()!=Boat.Get()));
   Test->TestEqual(TEXT("from the quay, F boards"),int32(Boat->GetActionFor(P)),int32(ENeonCityYachtAction::Board));
   PressF(PC);
   return Next(9);
  }
  if(Stage==9)
  {
   if(E<1.5)return false;
   Test->TestTrue(FString::Printf(TEXT("boarded into the cockpit (feet local %s)"),*Local().ToCompactString()),OnBoat() && Local().Z>90);
   Test->TestEqual(TEXT("in the cockpit after boarding, F takes the helm (no need to squeeze up to the wheel)"),int32(Boat->GetActionFor(P)),int32(ENeonCityYachtAction::TakeHelm));
   Test->TestEqual(TEXT("prompt reads Take the helm"),Boat->GetInteractionName_Implementation().ToString(),FString(TEXT("Take the helm")));
   Key(PC,EKeys::V,IE_Pressed);Key(PC,EKeys::V,IE_Released);
   return Next(10);
  }
  if(Stage==10)
  {
   if(E<.6)return false;
   auto* Hero=Cast<ANeonCityPlayerCharacter>(P);
   Test->TestTrue(TEXT("V switches to first person"),Hero && Hero->IsFirstPerson() && Hero->GetCameraBoom()->TargetArmLength<1);
   if(Hero)
   {
    const float FromEyes=FVector::Dist(Hero->GetFollowCamera()->GetComponentLocation(),P->GetActorLocation()+FVector(0,0,P->BaseEyeHeight));
    Test->TestTrue(FString::Printf(TEXT("camera at the eyes (%.0f cm)"),FromEyes),FromEyes<40);
   }
   Key(PC,EKeys::V,IE_Pressed);Key(PC,EKeys::V,IE_Released);
   return Next(11);
  }
  if(Stage==11)
  {
   if(E<.6)return false;
   auto* Hero=Cast<ANeonCityPlayerCharacter>(P);
   Test->TestTrue(TEXT("V again returns to the third-person camera"),Hero && !Hero->IsFirstPerson() && Hero->GetCameraBoom()->TargetArmLength>100);
   return true;
  }
  return true;
 }
private:
 bool Next(int32 S){Stage=S;At=FPlatformTime::Seconds();return false;}
 // the real input path: look up at the sky (no aiming at the boat), press and release F
 static void Key(APlayerController* PC,FKey K,EInputEvent Ev){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Ev,FPlatformTime::Cycles64()));}
 void PressF(APlayerController* PC)
 {
  PC->SetControlRotation(FRotator(35,PC->GetControlRotation().Yaw+70,0));
  if(auto* I=Me->FindComponentByClass<UInteractionComponent>())
  {
   AActor* F=I->RefreshFocusedInteractable();
   Test->AddInfo(FString::Printf(TEXT("F pressed at stage %d: focus %s, can interact %d, pawn %s"),Stage,*GetNameSafe(F),I->CanInteractWith(Boat.Get()),*GetNameSafe(PC->GetPawn())));
  }
  Key(PC,EKeys::F,IE_Pressed);Key(PC,EKeys::F,IE_Released);
 }
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0;float Falling=0;
 TWeakObjectPtr<ANeonCityLamborghini63> Boat;TWeakObjectPtr<ACharacter> Me;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeYachtWalkPIE,"NeonCity.WestLake.YachtWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeYachtWalkPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeYachtWalk::FWalk(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
