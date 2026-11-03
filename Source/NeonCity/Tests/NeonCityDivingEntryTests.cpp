#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DiveLocker.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/SwimmingComponent.h"
#include "Water/SwimmingAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "AudioMixerBlueprintLibrary.h"
#include "HAL/PlatformProcess.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"

namespace {
class FDivingPlayerEntry : public IAutomationLatentCommand
{
public:
 FDivingPlayerEntry(FAutomationTestBase* T,bool Read):Test(T),ReadSession(Read){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>420){Test->AddError(FString::Printf(TEXT("Normal input route timed out at %d, position %s"),Stage,P?*P->GetActorLocation().ToString():TEXT("no pawn")));return true;}
  if(!P || W->GetTimeSeconds()<6)return false;
  auto* Kit=P->FindComponentByClass<UDivingComponent>();auto* E=P->FindComponentByClass<UDiveEquipmentComponent>();auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();
  auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();auto* Move=P->GetCharacterMovement();const float Now=W->GetTimeSeconds(),Age=Now-At;
  auto Key=[&](FKey K,EInputEvent Event){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));};
  auto Next=[&](int32 S){Stage=S;At=Now;Test->AddInfo(FString::Printf(TEXT("ENTRY_STAGE %d at %.3f position %s"),S,Now,*P->GetActorLocation().ToString()));};
  Evidence.Record(W,PC,Stage);
  if(Now>NextMetric)
  {
   NextMetric=Now+.5f;
   Test->AddInfo(FString::Printf(TEXT("ENTRY_STATE s=%d age=%.2f suit=%d changing=%d dpv=%d spool=%.3f speed=%.1f water=%d mode=%d position=%s"),Stage,Age,Kit->IsEquipped(),Kit->IsChangingGear(),E->IsDPVDeployed(),E->GetMotorSpool(),P->GetVelocity().Size(),int(Water->GetWaterState()),int(Move->MovementMode),*P->GetActorLocation().ToString()));
  }
  if(Stage==0)
  {
   const FVector Arrival=NeonCityCoastalLayout::FromLegacyIsland(FVector(55520,-173550,220));auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(Arrival))return false;
   Save->ConfigureSlot(TEXT("NeonCity_Test_R05DPVPlayerEntry_20261003"));
   OriginalMesh=P->GetMesh()->GetSkeletalMeshAsset();DefaultCameraFOV=P->GetFollowCamera()->FieldOfView;
   Test->TestFalse(TEXT("Ordinary spawn has no issued R05"),Kit->OwnsSuit());Test->TestFalse(TEXT("Ordinary clothes cannot deploy DPV"),E->ToggleDPV());
   if(ReadSession)
   {
    if(!Test->TestTrue(TEXT("Prior OS process left an actual disk save"),Save->DoesSaveExist()))return true;
    Key(EKeys::F7,IE_Pressed);Next(90);return false;
   }
   // Only arrival is placed for the fixture. Every interaction, descent, device,
   // propulsion, shore return and save below uses the normal player input path.
   P->SetActorLocation(Arrival,false,nullptr,ETeleportType::TeleportPhysics);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);PC->SetControlRotation(FRotator(0,0,0));Next(1);
  }
  else if(Stage==1 && Age>2)
  {Key(EKeys::W,IE_Pressed);Next(2);}
  else if(Stage==2 && P->GetActorLocation().X>=55780)
  {Key(EKeys::W,IE_Released);PC->SetControlRotation(FRotator(-6,0,0));Next(3);}
  else if(Stage==3 && Age>1)
  {
   auto* Focus=P->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable();
   if(!Test->TestTrue(TEXT("Normal F focus finds authored diving station"),Focus && Focus->GetClass()==ADiveLocker::StaticClass()))return true;
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("01-Station-F.png"));Key(EKeys::F,IE_Pressed);Next(4);
  }
  else if(Stage==4)
  {
   Key(EKeys::F,IE_Released);
   if(Age>.15f && Age<.5f && !TransitionSeen){TransitionSeen=Kit->IsChangingGear();Test->TestTrue(TEXT("F starts a real timed equipment transition"),TransitionSeen);}
   if(Age>4)
   {
    if(!Test->TestTrue(TEXT("F issued R05 and DPV and wore the actual suit"),Kit->IsEquipped() && Kit->OwnsSuit() && Kit->OwnsDPV()))return true;
    Test->TestFalse(TEXT("Transition releases movement input"),PC->IsMoveInputIgnored());CheckComponents(P,true);
    Key(EKeys::F6,IE_Pressed);Next(5);
   }
  }
  else if(Stage==5 && Age>.5f)
  {
   Key(EKeys::F6,IE_Released);if(!Test->TestTrue(TEXT("Normal F6 saved worn gear"),Save->GetLastResult()==ENeonCitySaveResult::Success))return true;
   Key(EKeys::F7,IE_Pressed);Next(6);
  }
  else if(Stage==6 && Age>3)
  {
   Key(EKeys::F7,IE_Released);Test->TestTrue(TEXT("Worn R05 restores once; DPV remains stowed"),Kit->IsEquipped() && !E->IsDPVDeployed());CheckComponents(P,true);
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("02-R05-Restored.png"));PC->SetControlRotation(FRotator(-4,-90,0));Key(EKeys::W,IE_Pressed);Waypoint=0;Next(7);
  }
  else if(Stage==7)
  {
   // Leave the service bay through its actual east opening before swimming
   // around the moored bow; the north edge retains its physical safety rail.
   const FVector Points[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(55780,-173750,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(56110,-173750,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(56110,-173510,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(56250,-173510,-250)),NeonCityCoastalLayout::FromLegacyIsland(FVector(56250,-173000,-250)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57700,-173000,-250))};
   FVector To=Points[Waypoint]-P->GetActorLocation();
   // A swimmer-sized arrival radius clips the cabinet corner on the pier.
   // Finish the first three walking waypoints before turning into the water.
   if(To.Size2D()<(Waypoint<3?15.f:65.f) && Waypoint<UE_ARRAY_COUNT(Points)-1)
   {++Waypoint;To=Points[Waypoint]-P->GetActorLocation();}
   PC->SetControlRotation(FRotator(-4,To.Rotation().Yaw,0));
   if(Move->IsSwimming())Key(EKeys::C,IE_Pressed);
   if(Water->IsFullySubmerged() && Water->GetSample().DepthAt(P->GetActorLocation())>250)
   {Key(EKeys::C,IE_Released);Key(EKeys::W,IE_Released);PC->SetControlRotation(FRotator(0,-90,0));Key(EKeys::Q,IE_Pressed);Next(8);}
   else if(Age>45)
   {
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(DiveEntryRouteBlocker),true,P);
    const auto* Capsule=P->GetCapsuleComponent();
    W->SweepSingleByChannel(Hit,P->GetActorLocation(),FVector(Points[Waypoint].X,Points[Waypoint].Y,P->GetActorLocation().Z),FQuat::Identity,ECC_Pawn,
     FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query);
    Test->AddError(FString::Printf(TEXT("Pier-to-water route blocked at waypoint %d: player %s target %s blocker %s/%s at %s"),Waypoint,*P->GetActorLocation().ToString(),*Points[Waypoint].ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString()));
    Key(EKeys::W,IE_Released);Key(EKeys::C,IE_Released);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("PlayerEntry-PierBlocked.png"));return true;
   }
  }
  else if(Stage==8)
  {
   Key(EKeys::Q,IE_Released);
   if(Age>3)
   {
    if(!Test->TestTrue(TEXT("Enhanced Input Q deploys DPV with suit underwater"),E->IsDPVDeployed()))return true;
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("03-DPV-ThirdPerson.png"));
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))UAudioMixerBlueprintLibrary::StartRecordingOutput(W,40);
    Key(EKeys::LeftMouseButton,IE_Pressed);Next(9);
   }
  }
  else if(Stage==9)
  {
   if(Age>.25f && Age<.65f && !RampSeen){RampSeen=true;Test->TestTrue(TEXT("Held paddle ramps motor rather than instantly reaching full thrust"),E->GetMotorSpool()>.05f && E->GetMotorSpool()<.65f);}
   PeakSpeed=FMath::Max(PeakSpeed,float(P->GetVelocity().Size()));
   PeakCameraFOV=FMath::Max(PeakCameraFOV,P->GetFollowCamera()->FieldOfView);
   if(Age>5){Test->TestTrue(TEXT("Powered DPV exceeds ordinary suited swimming"),PeakSpeed>200);PC->SetControlRotation(FRotator(0,-60,0));Next(10);}
  }
  else if(Stage==10 && Age>3)
  {
   Test->AddInfo(FString::Printf(TEXT("DPV steering actor=%s view=%s controller=%s acceleration=%s velocity=%s orientToMovement=%d mode=%d motor=%.3f"),
    *P->GetActorRotation().ToString(),*P->GetFollowCamera()->GetComponentRotation().ToString(),*PC->GetControlRotation().ToString(),*Move->GetCurrentAcceleration().ToString(),*P->GetVelocity().ToString(),Move->bOrientRotationToMovement,int32(Move->MovementMode),E->GetMotorSpool()));
   Test->TestTrue(TEXT("Powered steering turns the body with the tool"),FMath::Abs(FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,-60))<12);
   Key(EKeys::V,IE_Pressed);Next(11);
  }
  else if(Stage==11)
  {
   Key(EKeys::V,IE_Released);
   if(Age>3)
   {
    Test->TestTrue(TEXT("V retains DPV in first person"),Kit->IsFirstPerson() && E->IsDPVDeployed());
    bool Hands=false,Device=false,Motor=false;
    TInlineComponentArray<USkeletalMeshComponent*> Skins(P);for(auto* S:Skins)if(S->GetName()==TEXT("DrysuitFirstPersonHands"))Hands=S->IsVisible() && S->IsComponentTickEnabled();
    TInlineComponentArray<UStaticMeshComponent*> Parts(P);for(auto* S:Parts)if(S->GetStaticMesh() && S->GetStaticMesh()->GetName()==TEXT("SM_OceanDPV"))Device=S->IsVisible();
    TInlineComponentArray<UAudioComponent*> Audio(P);for(auto* A:Audio)if(A->GetName()==TEXT("DPVSealedMotor"))Motor=A->IsPlaying();
    Test->TestTrue(TEXT("First person draws actual hands and actual DPV"),Hands && Device);Test->TestTrue(TEXT("Underwater motor audio is active"),Motor);
    // The physical maneuver can encounter the seabed, slowing the DPV. Check
    // the observed speed response and bounded lens change, not a fixed fast FOV.
    const float CurrentFOV=P->GetFollowCamera()->FieldOfView;
    Test->AddInfo(FString::Printf(TEXT("DPV lens baseline=%.2f movingPeak=%.2f current=%.2f actualSpeed=%.1f"),DefaultCameraFOV,PeakCameraFOV,CurrentFOV,P->GetVelocity().Size()));
    Test->TestTrue(TEXT("Powered motion produces a restrained camera response"),PeakCameraFOV>DefaultCameraFOV+1 && PeakCameraFOV<=DefaultCameraFOV+5.01f);
    Test->TestTrue(TEXT("First person speed response remains bounded as collisions slow the device"),CurrentFOV>=DefaultCameraFOV && CurrentFOV<=DefaultCameraFOV+5.01f);
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("04-DPV-FirstPerson.png"));ReleaseSpeed=P->GetVelocity().Size();Key(EKeys::LeftMouseButton,IE_Released);Next(12);
   }
  }
  else if(Stage==12)
  {
   if(Age>.15f && Age<.5f && !CoastSeen){CoastSeen=true;Test->TestTrue(TEXT("Paddle release preserves momentum"),P->GetVelocity().Size()>ReleaseSpeed*.45f);}
   if(Age>4)
   {Test->TestTrue(TEXT("Water drag gradually slows coast"),P->GetVelocity().Size()<ReleaseSpeed*.65f);Key(EKeys::Q,IE_Pressed);Next(13);}
  }
  else if(Stage==13)
  {
   Key(EKeys::Q,IE_Released);
   if(Age>2){Test->TestFalse(TEXT("Q stows DPV"),E->IsDPVDeployed());Test->TestFalse(TEXT("DPV stow restores empty hands"),E->IsSpeargunDeployed());Test->TestTrue(TEXT("Stowing DPV restores the original field of view"),FMath::IsNearlyEqual(P->GetFollowCamera()->FieldOfView,DefaultCameraFOV,.2f));Key(EKeys::G,IE_Pressed);Key(EKeys::RightMouseButton,IE_Pressed);Next(14);}
  }
  else if(Stage==14 && Age>1){Key(EKeys::G,IE_Released);Test->TestTrue(TEXT("G deploys the speargun"),E->IsSpeargunDeployed());Key(EKeys::LeftMouseButton,IE_Pressed);Next(15);}
  else if(Stage==15 && Age>.3f)
  {Key(EKeys::LeftMouseButton,IE_Released);Key(EKeys::RightMouseButton,IE_Released);Test->TestEqual(TEXT("Speargun still uses the physical firing loop after stowing DPV"),E->GetFiredCount(),1);Key(EKeys::R,IE_Pressed);Next(16);}
  else if(Stage==16 && Age>12)
  {
   Key(EKeys::R,IE_Released);Test->TestTrue(TEXT("Physical shaft recovered and reloaded"),E->GetSpearPhase()==ESpearPhase::Ready);Key(EKeys::Q,IE_Pressed);Next(17);
  }
  else if(Stage==17)
  {
   Key(EKeys::Q,IE_Released);
   if(Age>2)
   {
    Test->TestTrue(TEXT("DPV is available again after spear recovery"),E->IsDPVDeployed());Key(EKeys::V,IE_Pressed);Key(EKeys::SpaceBar,IE_Pressed);Key(EKeys::W,IE_Pressed);Waypoint=0;Next(18);
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))
    {FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,TEXT("DPV-Normal-Input"),FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("Audio"));}
   }
  }
  else if(Stage==18)
  {
   Key(EKeys::V,IE_Released);
   // Return via the physical beach and the land end of the pier. The visual
   // ladder has no native climbing adapter; do not bypass that with a teleport.
   const FVector Points[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(56000,-163000,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-163000,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-173550,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55780,-173550,180))};
   const FVector To=Points[Waypoint]-P->GetActorLocation();PC->SetControlRotation(FRotator(15,To.Rotation().Yaw,0));
   if(Age<.2f)Key(EKeys::Q,IE_Pressed);else Key(EKeys::Q,IE_Released);
   if(Move->IsMovingOnGround() && Water->GetWaterState()==EWaterInteractionState::Dry)Key(EKeys::SpaceBar,IE_Released);
   if(To.Size2D()<65 && Waypoint<3){++Waypoint;return false;}
   if(Waypoint==3 && Move->IsMovingOnGround() && Water->GetWaterState()==EWaterInteractionState::Dry && To.Size2D()<50)
   {Key(EKeys::W,IE_Released);Key(EKeys::SpaceBar,IE_Released);PC->SetControlRotation(FRotator(-6,0,0));Next(19);}
  }
  else if(Stage==19 && Age>1)
  {
   Test->TestFalse(TEXT("Shore return safely stows propulsion"),E->IsDPVDeployed());
   if(!Test->TestTrue(TEXT("Returned player focuses same gear station"),Cast<ADiveLocker>(P->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable())!=nullptr))return true;
   Key(EKeys::F,IE_Pressed);Next(20);
  }
  else if(Stage==20)
  {
   Key(EKeys::F,IE_Released);
   if(Age>4)
   {
    Test->TestTrue(TEXT("Second station F removes gear but preserves ownership"),!Kit->IsEquipped() && Kit->OwnsSuit() && Kit->OwnsDPV());CheckComponents(P,false);
    Test->TestTrue(TEXT("Normal character mesh restored"),P->GetMesh()->GetSkeletalMeshAsset()==OriginalMesh.Get());
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("05-Gear-Removed.png"));Key(EKeys::F6,IE_Pressed);Next(21);
   }
  }
  else if(Stage==21 && Age>1)
  {
   Key(EKeys::F6,IE_Released);Test->TestTrue(TEXT("Normal F6 commits the final owned and stowed kit"),Save->GetLastResult()==ENeonCitySaveResult::Success);
   Manifest(Save,false);return true;
  }
  else if(Stage==90 && Age>4)
  {
   Key(EKeys::F7,IE_Released);Test->TestTrue(TEXT("F7 in a new OS process restored kit ownership without wearing it"),Kit->OwnsSuit() && Kit->OwnsDPV() && !Kit->IsEquipped() && !E->IsDPVDeployed());CheckComponents(P,false);
   Test->TestTrue(TEXT("Restart retains the ordinary character mesh"),P->GetMesh()->GetSkeletalMeshAsset()==OriginalMesh.Get());Key(EKeys::F7,IE_Pressed);Next(91);
  }
  else if(Stage==91 && Age>3)
  {Key(EKeys::F7,IE_Released);CheckComponents(P,false);Manifest(Save,true);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("06-Restart-Loaded.png"));Key(EKeys::F,IE_Pressed);Next(92);}
  else if(Stage==92)
  {
   Key(EKeys::F,IE_Released);
   if(Age>4){Test->TestTrue(TEXT("Loaded owner can use normal station F again"),Kit->IsEquipped());P->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(15);Next(93);}
  }
  else if(Stage==93 && Age>1)
  {
   Test->TestTrue(TEXT("Downed clears worn and hand-held kit while preserving ownership"),!Kit->IsEquipped() && !E->IsDPVDeployed() && Kit->OwnsSuit());CheckComponents(P,false);
   Key(EKeys::F7,IE_Pressed);Next(94);
  }
  else if(Stage==94 && Age>3){Key(EKeys::F7,IE_Released);Key(EKeys::F,IE_Pressed);Next(95);}
  else if(Stage==95)
  {
   Key(EKeys::F,IE_Released);
   if(Age>4){Test->TestTrue(TEXT("Recovered player can re-equip"),Kit->IsEquipped());P->FindComponentByClass<UHealthComponent>()->Die();Next(96);}
  }
  else if(Stage==96 && Age>1)
  {
   Test->TestTrue(TEXT("Death clears gear and equipment input"),!Kit->IsEquipped() && !E->IsDPVDeployed());CheckComponents(P,false);Key(EKeys::F7,IE_Pressed);Next(97);
  }
  else if(Stage==97 && Age>3){Key(EKeys::F7,IE_Released);Key(EKeys::F,IE_Pressed);Next(98);}
  else if(Stage==98)
  {
   Key(EKeys::F,IE_Released);
   if(Age>4)
   {
    Test->TestTrue(TEXT("Post-death load restores a usable player"),Kit->IsEquipped());
    // Cars and both aircraft families use this same on-foot UnPossessed boundary.
    PC->UnPossess();Test->TestFalse(TEXT("Possession transfer synchronously removes professional hand equipment"),Kit->IsEquipped());CheckComponents(P,false);PC->Possess(P);
    Key(EKeys::F7,IE_Pressed);Next(99);
   }
  }
  else if(Stage==99 && Age>3)
  {Key(EKeys::F7,IE_Released);CheckComponents(P,false);Test->TestTrue(TEXT("Repeated recovery retains both owned items"),Kit->OwnsSuit() && Kit->OwnsDPV());return true;}
  return false;
 }
 void CheckComponents(ANeonCityPlayerCharacter* P,bool Worn)
 {
  int32 Hands=0,DPVs=0;TInlineComponentArray<USkeletalMeshComponent*> Skins(P);for(auto* S:Skins)if(S->GetName().StartsWith(TEXT("DrysuitFirstPersonHands")))++Hands;
  TInlineComponentArray<UStaticMeshComponent*> Parts(P);for(auto* S:Parts)if(S->GetStaticMesh() && S->GetStaticMesh()->GetName()==TEXT("SM_OceanDPV"))++DPVs;
  Test->TestEqual(TEXT("Exactly one or zero equipment hands instance"),Hands,Worn?1:0);Test->TestEqual(TEXT("Exactly one or zero DPV assembly"),DPVs,Worn?1:0);
 }
 void Manifest(UNeonCitySaveSubsystem* S,bool Read)
 {
  auto* M=S->ReadSaveForInspection();if(!Test->TestNotNull(TEXT("Saved kit model is readable"),M))return;
  Test->TestTrue(TEXT("Disk records both owned items and stowed final state"),M->Player.OwnsR05 && M->Player.OwnsDPV && !M->Player.R05Equipped);
  const FString Json=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"ownsR05\":true,\"ownsDPV\":true,\"equipped\":false,\"readSession\":%s}"),FPlatformProcess::GetCurrentProcessId(),*M->SaveID.ToString(),Read?TEXT("true"):TEXT("false"));
  FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("OceanOverhaul")/(Read?TEXT("PlayerEntry-SessionB.json"):TEXT("PlayerEntry-SessionA.json"))));
 }
 FAutomationTestBase* Test;bool ReadSession=false,TransitionSeen=false,RampSeen=false,CoastSeen=false;double Started=0;float At=0,NextMetric=0,PeakSpeed=0,ReleaseSpeed=0,DefaultCameraFOV=0,PeakCameraFOV=0;int32 Stage=0,Waypoint=0;
 TWeakObjectPtr<USkeletalMesh> OriginalMesh;FNeonCityVisualEvidence Evidence{TEXT("DivingPlayerEntry")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivingEntryA,"NeonCity.OceanExperience.PlayerEntry.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivingEntryA::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FDivingPlayerEntry(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivingEntryB,"NeonCity.OceanExperience.PlayerEntry.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivingEntryB::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FDivingPlayerEntry(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
