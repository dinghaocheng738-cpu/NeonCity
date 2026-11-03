#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Presentation/TransportOccupantAnimInstance.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/FixedWing/NeonCityFixedWingAircraft.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"

namespace NeonCityTransportOccupantTests
{
class FVerify final : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>160){Test->AddError(FString::Printf(TEXT("Occupant PIE timeout vehicle %d stage %d"),Index,Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<3)return false;
  const float Now=W->GetTimeSeconds();auto Next=[&](int32 S){Stage=S;Phase=Now;return false;};
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)if(!It->GetController())Vehicles.Add(*It);
   for(TActorIterator<ANeonCityMarineVehicleBase> It(W);It;++It)Vehicles.Add(*It);
   bool bPlane=false;
   for(TActorIterator<ANeonCityAircraftVehicleBase> It(W);It;++It){Vehicles.Add(*It);bPlane|=It->IsA<ANeonCityFixedWingAircraft>();}
   if(!bPlane){auto* Area=W->SpawnActor<AFixedWingTestArea>();if(Area && Area->GetAircraft())Vehicles.Add(Area->GetAircraft());}
   if(!Test->TestTrue(TEXT("Road fleet, marine and both aircraft available"),Vehicles.Num()>=7))return true;
   const auto FirstVehicle=Vehicles[0];
   Vehicles.Add(FirstVehicle); // Copy before array growth; re-entry must reuse one proxy.
   return Next(1);
  }
  auto* Vehicle=Vehicles[Index].Get();if(!Test->TestNotNull(TEXT("Vehicle retained"),Vehicle))return true;
  auto* Occupant=Vehicle->FindComponentByClass<UTransportOccupantComponent>();
  if(!Test->TestNotNull(TEXT("Transport has a generic occupant adapter"),Occupant))return true;
  if(Stage==1)
  {
   FVector Offset=FVector(0,-220,100);
   if(Vehicle->IsA<ANeonCityMarineVehicleBase>())Offset=FVector(0,210,180);
   if(const auto* Boat=Cast<ANeonCityMarineVehicleBase>(Vehicle);Boat && Boat->GetProfile()->bWalkableDeck)Offset=FVector(-75,0,134);
   if(Vehicle->IsA<ANeonCityAircraftVehicleBase>())Offset=FVector(0,-260,55);
   if(Vehicle->IsA<ANeonCityFixedWingAircraft>())Offset=FVector(380,0,35);
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Player->GetCharacterMovement()->StopMovementImmediately();
   Player->SetActorLocation(Vehicle->GetActorTransform().TransformPosition(Offset),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);PC->SetViewTarget(Player);
   return Next(2);
  }
  if(Stage==2 && Now-Phase>.45f)
  {
   FVector Eye;FRotator R;Player->GetActorEyesViewPoint(Eye,R);
   const auto* Boat=Cast<ANeonCityMarineVehicleBase>(Vehicle);
   const FVector Aim=Boat && Boat->GetProfile()->bWalkableDeck?Vehicle->GetActorTransform().TransformPosition(FVector(110,55,120)):Vehicle->GetActorLocation()+FVector(0,0,40);
   PC->SetControlRotation((Aim-Eye).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Vehicle)
   {
    if(Now-Phase<2)return false;
    Test->AddError(FString::Printf(TEXT("Normal F focus failed for %s from %s"),*Vehicle->GetClass()->GetName(),*Player->GetActorLocation().ToString()));return true;
   }
   Key(PC,EKeys::F,IE_Pressed);return Next(3);
  }
  if(Stage==3 && Now-Phase>.4f)
  {
   Key(PC,EKeys::F,IE_Released);auto* Mesh=Occupant->GetDriverMesh();
   if(!Test->TestTrue(TEXT("F possesses vehicle with visible seated driver"),PC->GetPawn()==Vehicle && Occupant->GetRepresentedDriver()==Player && Mesh && Mesh->IsVisible() && !Mesh->bHiddenInGame))return true;
   Test->TestTrue(TEXT("Existing hidden player has no collision while seated"),Player->IsHidden() && !Player->GetActorEnableCollision());
   Test->TestTrue(TEXT("Presentation adds no contact or mass"),!Mesh->IsSimulatingPhysics() && Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Test->TestEqual(TEXT("The seated driver retains the player's actual character skin"),Mesh->GetSkeletalMeshAsset(),Player->GetMesh()->GetSkeletalMeshAsset());
   if(Index==0)FirstDriverMesh=Mesh;
   if(Index==Vehicles.Num()-1)Test->TestTrue(TEXT("Repeated entry reuses the original seated mesh"),Mesh==FirstDriverMesh.Get());
   const FVector Hip=Vehicle->GetActorTransform().InverseTransformPosition(Mesh->GetSocketLocation(TEXT("pelvis")));
   Test->TestTrue(TEXT("Animated pelvis stays aligned with actual seat"),FVector::Dist(Hip,Occupant->SeatHipLocation)<8);
   Test->TestNotNull(TEXT("Seated graph applies control contact IK"),Cast<UTransportOccupantAnimInstance>(Mesh->GetAnimInstance()));
   auto Contact=[&](FName Bone,FVector Expected)
   {
    const FVector Actual=Vehicle->GetActorTransform().InverseTransformPosition(Mesh->GetSocketLocation(Bone));
    Test->TestTrue(FString::Printf(TEXT("%s %s reaches its control or footwell: %s"),*Vehicle->GetClass()->GetName(),*Bone.ToString(),*Actual.ToString()),FVector::Dist(Actual,Expected)<4);
   };
   Contact(TEXT("hand_l"),Occupant->LeftHandTarget);Contact(TEXT("hand_r"),Occupant->RightHandTarget);
   Contact(TEXT("foot_l"),Occupant->LeftFootTarget);Contact(TEXT("foot_r"),Occupant->RightFootTarget);
   if(Vehicle->IsA<ANeonCityFixedWingAircraft>())Test->TestTrue(TEXT("Pilot head fits below closed canopy"),Vehicle->GetActorTransform().InverseTransformPosition(Mesh->GetSocketLocation(TEXT("head"))).Z<80);
   UE_LOG(LogTemp,Display,TEXT("Occupant %s hip=%s head=%s"),*Vehicle->GetClass()->GetName(),*Hip.ToString(),*Vehicle->GetActorTransform().InverseTransformPosition(Mesh->GetSocketLocation(TEXT("head"))).ToString());
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMotion")))
   {
    Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=55;
    const FVector Focus=Vehicle->GetActorTransform().TransformPosition(Occupant->SeatHipLocation+FVector(0,0,40));
    Camera->SetActorLocation(Focus+Vehicle->GetActorRotation().RotateVector(FVector(180,-200,50)));
    Camera->SetActorRotation((Focus-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);
   }
   return Next(4);
  }
  if(Stage==4)
  {
   Evidence.Record(W,PC,Index);
   if(Now-Phase<2)return false;
   PC->SetViewTarget(Vehicle);if(Camera){Camera->Destroy();Camera=nullptr;}
   Key(PC,EKeys::F,IE_Pressed);return Next(5);
  }
  if(Stage==5 && Now-Phase>.4f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("Normal F exit returns original character"),PC->GetPawn()==Player))return true;
   Test->TestTrue(TEXT("Exit clears seated proxy and restores player"),!Player->IsHidden() && Player->GetActorEnableCollision() && !Occupant->GetRepresentedDriver()
    && Occupant->GetDriverMesh() && Occupant->GetDriverMesh()->bHiddenInGame && !Occupant->GetDriverMesh()->GetSkeletalMeshAsset());
   if(++Index==Vehicles.Num())return true;return Next(1);
  }
  return false;
 }
private:
 static void Key(APlayerController* PC,FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 FAutomationTestBase* Test;double Started=0;float Phase=0;int32 Stage=0,Index=0;
 ANeonCityPlayerCharacter* Player=nullptr;ACameraActor* Camera=nullptr;TArray<TWeakObjectPtr<APawn>> Vehicles;
 TWeakObjectPtr<USkeletalMeshComponent> FirstDriverMesh;
 FNeonCityVisualEvidence Evidence{TEXT("TransportOccupants")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransportOccupantPIE,"NeonCity.VerticalSlice.TransportOccupantsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTransportOccupantPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityTransportOccupantTests::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
