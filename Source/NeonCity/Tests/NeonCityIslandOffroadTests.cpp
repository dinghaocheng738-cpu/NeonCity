#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Misc/AutomationTest.h"
#include "Adventure/IslandSupercar.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleData.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Components/AudioComponent.h"
#include "Misc/App.h"
namespace
{
class FIslandFleetAudit : public IAutomationLatentCommand
{
public:
 explicit FIslandFleetAudit(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>700){Test->AddError(FString::Printf(TEXT("Island fleet timeout stage=%d index=%d"),Stage,Index));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds();auto Next=[&](int32 S){Stage=S;At=Now;return false;};
  // The isolated speed straight is deliberately outside the playable island.
  // Keep the source level resident while its production cars are on the rig.
  if(Stage>=6 && Stage<=11)
   if(auto* Region=ANeonCityCoastalRegion::Find(W))Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-155000,700)));
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-155000,700))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)Cars.Add(*It);
   Cars.Sort([](const auto& A,const auto& B){return A.GetName()<B.GetName();});
   if(!Test->TestEqual(TEXT("Four authored hero cars on island"),Cars.Num(),4))return true;
   TSet<FGuid> IDs;int32 Pickups=0;
   for(auto* Car:Cars)
   {
    IDs.Add(Car->VehicleID);Pickups+=Car->bRoadster?1:0;Parking.Add(Car->GetActorTransform());
    Test->TestTrue(TEXT("Island placement is in the correct streamed level"),Car->GetLevel()!=W->PersistentLevel);
    Test->TestTrue(TEXT("Physical AWD is enabled"),Car->GetChaosMovement()->DifferentialSetup.DifferentialType==EVehicleDifferential::AllWheelDrive);
    Test->TestTrue(TEXT("Distinct hero speed profile"),Car->GetVehicleProfile()->MaxSpeed>=280);
    Test->TestTrue(TEXT("Identity grants accessible owned vehicle"),W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>()->IsOwned(Car->VehicleID));
    TInlineComponentArray<USpotLightComponent*> Headlights(Car);
    Test->TestEqual(TEXT("Each production body has two headlights"),Headlights.Num(),2);
    for(auto* Lamp:Headlights)
    {
     Test->TestTrue(TEXT("Headlight optical origin clears the production nose"),FMath::IsNearlyEqual(Lamp->GetRelativeLocation().X,Car->bRoadster?226.f:256.f));
     Test->TestTrue(TEXT("Headlight preserves shadows and authored photometry"),Lamp->CastShadows && Lamp->IntensityUnits==ELightUnits::Lumens && FMath::IsNearlyEqual(Lamp->Intensity,900.f));
    }
    TInlineComponentArray<UStaticMeshComponent*> Parts(Car);for(auto* P:Parts)
     if(P->GetFName()==TEXT("ArtVisual"))Test->TestEqual(TEXT("Distinct hero body loaded"),P->GetStaticMesh()->GetName(),FString(Car->bRoadster?TEXT("SM_HeroKestrel"):TEXT("SM_HeroAster")));
   }
   Test->TestEqual(TEXT("Unique persistent identities"),IDs.Num(),4);Test->TestEqual(TEXT("Two Aster and two Kestrel cars"),Pickups,2);
   auto* Registry=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();auto Snapshot=Registry->BuildPersistenceSnapshot(false);TArray<FString> Warnings;Registry->NormalizePersistenceSnapshot(Snapshot,Warnings);
   Test->TestEqual(TEXT("Supercar profile allowlist accepts saved fleet"),Warnings.Num(),0);
   auto Legacy=Snapshot;
   for(auto& Row:Legacy)for(auto* Car:Cars)if(Row.VehicleID==Car->VehicleID)
   {Row.VehicleType=uint8(Car->bRoadster?ENeonCityVehicleType::Pickup:ENeonCityVehicleType::SUV);Row.DataAssetID=Car->bRoadster?TEXT("DA_ExpeditionPickup"):TEXT("DA_ExpeditionSUV");}
   Warnings.Reset();Registry->NormalizePersistenceSnapshot(Legacy,Warnings);
   Test->TestEqual(TEXT("Four legacy island identities migrate without warning"),Warnings.Num(),0);
   for(const auto& Row:Legacy)for(auto* Car:Cars)if(Row.VehicleID==Car->VehicleID)
   {Test->TestEqual(TEXT("Legacy save now uses matching hero profile"),Row.DataAssetID,Car->GetVehicleProfile()->GetFName());
    const auto* Original=Snapshot.FindByPredicate([&](const auto& Other){return Other.VehicleID==Row.VehicleID;});
    Test->TestTrue(TEXT("Migration preserves parking, condition and ownership"),Original && Row.Transform.Equals(Original->Transform) && Row.Condition==Original->Condition && Row.Owned==Original->Owned);}
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);
   const FVector Focus=Cars[0]->GetActorLocation();const FVector Eye=Focus+FVector(1000,-1100,650);Camera->SetActorLocation(Eye);Camera->SetActorRotation((Focus+FVector(0,0,80)-Eye).Rotation());
   return Next(1);
  }
  if(Stage==1 && Now-At>8){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Island-Supercar-Day.png"));return Next(2);}
  if(Stage==2)
  {
   auto* Car=Cars[Index];FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(IslandCarGround),true,Car);
   const FVector P=Car->GetActorLocation();const bool Ground=W->LineTraceSingleByChannel(Hit,P+FVector(0,0,80),P-FVector(0,0,200),ECC_WorldStatic,Query);
   Test->TestTrue(TEXT("Authored car settled on dry island ground"),Ground && Hit.ImpactPoint.Z>100 && P.Z-Hit.ImpactPoint.Z<130);
   Test->TestTrue(TEXT("Parked chassis remains upright"),Car->GetActorUpVector().Z>.92);
   Player->SetActorLocation(P-Car->GetActorRightVector()*255+FVector(0,0,95),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();PC->SetViewTarget(Player);return Next(3);
  }
  if(Stage==3 && Now-At>.8f)
  {
   if(!Test->TestTrue(TEXT("Ordinary player enters parked supercar"),Cars[Index]->GetVehicleInteraction()->Enter(Player)))return true;
   Origin=Cars[Index]->GetActorLocation();Cars[Index]->GetDrivingComponent()->SetDriveInput(1);return Next(4);
  }
  if(Stage==4)Cars[Index]->GetDrivingComponent()->SetDriveInput(1);
  if(Stage==4 && Now-At>1.2f)
  {
   auto* Car=Cars[Index];const float Distance=FVector::Dist2D(Origin,Car->GetActorLocation());
   Test->AddInfo(FString::Printf(TEXT("Island car %d %s: actual launch %.1f km/h, moved %.1f cm"),Index,*Car->GetVehicleProfile()->DisplayName.ToString(),Car->GetDrivingComponent()->GetSpeedKmh(),Distance));
   Test->TestTrue(TEXT("Actual Chaos engine moves car from island parking"),Distance>65 && Car->GetDrivingComponent()->GetSpeedKmh()>5);
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Island-Supercar-Driving-%d.png"),Index));
   Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);return Next(5);
  }
  if(Stage==5 && Now-At>1.8f)
  {
   FTransform Exit;Test->AddInfo(FString::Printf(TEXT("Parking exit %d: velocity %.1f cm/s, rotation %s, clear route %d"),Index,Cars[Index]->GetVelocity().Size(),*Cars[Index]->GetActorRotation().ToString(),Cars[Index]->GetVehicleInteraction()->GetPersistenceExitTransform(Exit)));
   if(!Test->TestTrue(TEXT("Braking permits safe ordinary exit"),Cars[Index]->GetVehicleInteraction()->Exit()))return true;
   if(++Index<4)return Next(2);Index=0;
   // Explicit test-only calibrated straight. Never saved or shipped as art.
   // It isolates top speed, braking and steering from finite island road length.
   Track=W->SpawnActor<AStaticMeshActor>();Track->SetActorLabel(TEXT("Temporary Prototype - supercar physics test straight"));Track->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Track->SetActorLocation(FVector(500000,0,10000));Track->SetActorScale3D(FVector(6000,80,.3));Track->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));Track->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));Track->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
   for(auto* Car:Cars){if(!SUV && !Car->bRoadster)SUV=Car;if(!Pickup && Car->bRoadster)Pickup=Car;}Runs={SUV,Pickup};return Next(6);
  }
  if(Stage==6)
  {
   auto* Car=Runs[Index];Car->GetChaosMovement()->ResetVehicleState();Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector(210000,0,10110)),false,nullptr,ETeleportType::TeleportPhysics);
   Player->SetActorLocation(Car->GetActorLocation()+FVector(0,-260,100),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();Peak=0;LastTelemetry=-1;return Next(7);
  }
  if(Stage==7 && Now-At>1)
  {
   if(!Test->TestTrue(TEXT("Production vehicle enters speed circuit"),Runs[Index]->GetVehicleInteraction()->Enter(Player)))return true;
   Runs[Index]->GetDrivingComponent()->SetDriveInput(1);
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))
   {
    if(auto Device=W->GetAudioDevice())
    {
     Test->AddInfo(FString::Printf(TEXT("PIE recording device %u muted=%d primary=%.3f app=%.3f background=%.3f allowsound=%d"),Device->DeviceID,Device->IsAudioDeviceMuted(),Device->GetPrimaryVolume(),FApp::GetVolumeMultiplier(),FApp::GetUnfocusedVolumeMultiplier(),W->bAllowAudioPlayback));
     // Offscreen PIE has no focused native viewport. Explicitly solo this
     // session's real mixer for recording, just as interactive PIE focus does.
     GEngine->GetAudioDeviceManager()->SetSoloDevice(Device->DeviceID);
     Device->SetTransientPrimaryVolume(1);
    }
    UAudioMixerBlueprintLibrary::StartRecordingOutput(W,60);
   }
   return Next(8);
  }
  if(Stage==8)
  {
   auto* Car=Runs[Index];Car->GetDrivingComponent()->SetDriveInput(1);Peak=FMath::Max(Peak,Car->GetDrivingComponent()->GetSpeedKmh());
   HoldStraight(Car);
   if(int32((Now-At)/5)>LastTelemetry)
   {
    LastTelemetry=int32((Now-At)/5);auto* M=Car->GetChaosMovement();int32 Grounded=0;for(int32 I=0;I<4;++I)Grounded+=M->GetWheelState(I).bInContact?1:0;
    Test->AddInfo(FString::Printf(TEXT("Speed telemetry %d t=%.1f speed=%.1f position=%s rotation=%s steer=%.3f contacts=%d velocity=%s"),Index,Now-At,Car->GetDrivingComponent()->GetSpeedKmh(),*Car->GetActorLocation().ToString(),*Car->GetActorRotation().ToString(),M->GetSteeringInput(),Grounded,*Car->GetVelocity().ToString()));
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")) && LastTelemetry==1)
    {
     TInlineComponentArray<UAudioComponent*> Sounds(Car);
     for(auto* Sound:Sounds)Test->AddInfo(FString::Printf(TEXT("Audio source %s asset=%s playing=%d volume=%.3f pitch=%.3f"),*Sound->GetName(),*GetNameSafe(Sound->Sound),Sound->IsPlaying(),Sound->VolumeMultiplier,Sound->PitchMultiplier));
     if(auto Device=W->GetAudioDevice()){FVector Listener;Device->GetListenerPosition(0,Listener,false);Test->AddInfo(FString::Printf(TEXT("Recording device muted=%d primary=%.3f listener=%s"),Device->IsAudioDeviceMuted(),Device->GetPrimaryVolume(),*Listener.ToString()));}
    }
   }
   if(Now-At<50 && Peak<Car->GetVehicleProfile()->MaxSpeed-5)return false;
   Test->AddInfo(FString::Printf(TEXT("%s achieved %.2f km/h with actual AWD and production torque/gearing in %.2f seconds"),*Car->GetVehicleProfile()->DisplayName.ToString(),Peak,Now-At));
   Test->TestTrue(TEXT("High speed exceeds 200 km/h using actual physics"),Peak>200);
   Test->TestTrue(TEXT("Speed governor respected"),Peak<Car->GetVehicleProfile()->MaxSpeed+3);
   Test->TestTrue(TEXT("High speed remains upright"),Car->GetActorUpVector().Z>.92);
   int32 Contacts=0;for(int32 I=0;I<4;++I)Contacts+=Car->GetChaosMovement()->GetWheelState(I).bInContact?1:0;
   Test->TestTrue(TEXT("Speed achieved on the calibrated road, not while falling"),Contacts>=3 && FMath::Abs(Car->GetActorLocation().Z-10100)<150);
   Test->AddInfo(FString::Printf(TEXT("Top-speed chassis location %s, wheel contacts %d"),*Car->GetActorLocation().ToString(),Contacts));
   Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);Origin=Car->GetActorLocation();return Next(9);
  }
  if(Stage==9)
  {
   auto* Car=Runs[Index];Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);HoldStraight(Car);
   // These production cars start this trial near 280 km/h. Observe the actual
   // stop with a bounded distance/time budget before beginning the turn trial.
   if(Now-At<12 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())>2)return false;
   Test->AddInfo(FString::Printf(TEXT("Braking %s: %.1f m, %.1f seconds, final %.1f km/h"),*Car->GetName(),FVector::Dist2D(Origin,Car->GetActorLocation())*.01,Now-At,Car->GetDrivingComponent()->GetSpeedKmh()));
   Test->TestTrue(TEXT("Full brake brings fast car to stop"),FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<2);
   Test->TestTrue(TEXT("High-speed stopping distance remains below 400 metres"),FVector::Dist2D(Origin,Car->GetActorLocation())<40000);
   Test->TestTrue(TEXT("Braking preserves road contact and upright chassis"),Car->GetActorUpVector().Z>.92 && Car->GetChaosMovement()->GetWheelState(0).bInContact && Car->GetChaosMovement()->GetWheelState(1).bInContact);
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck")))UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,Index==0?TEXT("Hero-Aster-PIE"):TEXT("Hero-Kestrel-PIE"),FPaths::ProjectSavedDir()/TEXT("BouncedWavFiles"));
   Car->GetDrivingComponent()->SetBrakeInput(0);Car->GetDrivingComponent()->SetDriveInput(.7);Car->GetDrivingComponent()->SetSteerInput(.65);StartYaw=Car->GetActorRotation().Yaw;return Next(10);
  }
  if(Stage==10){Runs[Index]->GetDrivingComponent()->SetDriveInput(.7);Runs[Index]->GetDrivingComponent()->SetSteerInput(.65);}
  if(Stage==10 && Now-At>3)
  {
   auto* Car=Runs[Index];Test->TestTrue(TEXT("Real steering turns chassis"),FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw,Car->GetActorRotation().Yaw))>8);
   Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);return Next(11);
  }
  if(Stage==11 && Now-At>2)
  {
   auto* StoppingCar=Runs[Index];StoppingCar->GetDrivingComponent()->SetBrakeInput(1);
   if(StoppingCar->GetVelocity().Size()>100 && Now-At<8)return false;
   FTransform Exit;Test->AddInfo(FString::Printf(TEXT("Circuit exit %d: velocity %.1f cm/s, position %s, rotation %s, clear route %d"),Index,StoppingCar->GetVelocity().Size(),*StoppingCar->GetActorLocation().ToString(),*StoppingCar->GetActorRotation().ToString(),StoppingCar->GetVehicleInteraction()->GetPersistenceExitTransform(Exit)));
   Test->TestTrue(TEXT("Circuit braking settles the chassis before safe exit"),StoppingCar->GetVelocity().Size()<=100);
   if(!Test->TestTrue(TEXT("Ordinary safe circuit exit"),Runs[Index]->GetVehicleInteraction()->Exit()))return true;
   if(++Index<2)return Next(6);
   // Restore authored parking for the game-camera light review, not driving.
   for(int32 I=0;I<4;++I){auto* Car=Cars[I];Car->GetChaosMovement()->ResetVehicleState();Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->SetActorTransform(Parking[I],false,nullptr,ETeleportType::TeleportPhysics);}
   Player->SetActorLocation(Cars[0]->GetActorLocation()+FVector(-300,-300,200),false,nullptr,ETeleportType::TeleportPhysics);PC->SetViewTarget(Camera);Track->Destroy();Index=0;return Next(12);
  }
  if(Stage==12)
  {
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)It->SetTimeOfDay(Index==0?17.5f:22.f);
   if(Index==1)
   {
    Player->SetActorLocation(Cars[0]->GetActorLocation()-Cars[0]->GetActorRightVector()*255+FVector(0,0,90),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
    Test->TestTrue(TEXT("Night car entry"),Cars[0]->GetVehicleInteraction()->Enter(Player));PC->SetViewTarget(Camera);
   }
   return Next(13);
  }
  if(Stage==13 && Now-At>8)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,Index==0?TEXT("Island-Supercar-Sunset.png"):TEXT("Island-Supercar-Night.png"));
   if(++Index<2)return Next(12);
   TInlineComponentArray<USpotLightComponent*> Lamps(Cars[0]);Test->TestEqual(TEXT("Two real headlights"),Lamps.Num(),2);
   // Road illumination is checked in normal PIE. This assertion checks the
   // repaired native policy without requiring the old overexposed 3600 lm.
   for(auto* Lamp:Lamps)Test->TestTrue(TEXT("Occupied night car enables authored 900 lm lamps"),Lamp->IsVisible() && FMath::IsNearlyEqual(Lamp->Intensity,900.f));
   Cars[0]->GetVehicleInteraction()->Exit();
   auto* Registry=W->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();auto Snapshot=Registry->BuildPersistenceSnapshot(false);
   const FGuid SavedID=Pickup->VehicleID;Pickup->Destroy();Registry->ApplyPersistenceSnapshot(Snapshot);
   auto* Restored=Cast<AIslandSupercar>(Registry->FindLiveVehicle(SavedID));
   Test->TestTrue(TEXT("Persistence respawns hero class and roadster body"),Restored && Restored->bRoadster);
   Test->TestTrue(TEXT("Persistence retains independent high speed profile"),Restored && Restored->GetVehicleProfile()->GetFName()==TEXT("DA_HeroKestrel") && Restored->GetVehicleProfile()->MaxSpeed==280);
   return true;
  }
 return false;
 }
 void HoldStraight(AIslandSupercar* Car)
 {
  const FVector Here=Car->GetActorLocation();const float Speed=FMath::Abs(Car->GetChaosMovement()->GetForwardSpeed());
  const float LookAhead=FMath::Max(1800.f,Speed*3.f);
  const float Heading=(FVector(Here.X+LookAhead,0,Here.Z)-Here).Rotation().Yaw;
  // A proportional-only robot overcorrects the tiny high-speed lane error and
  // provokes alternating skids. Dampen yaw like a driver unwinding the wheel.
  const float YawRate=Car->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z;
  const float Command=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Heading)/50.f-YawRate/55.f;
  const float Limit=Speed>2800?.15f:.65f;
  Car->GetDrivingComponent()->SetSteerInput(FMath::Clamp(Command,-Limit,Limit));
 }
private:
 FAutomationTestBase* Test;double Started=0;float At=0,Peak=0,StartYaw=0;int32 Stage=0,Index=0,LastTelemetry=-1;
 TArray<AIslandSupercar*> Cars,Runs;TArray<FTransform> Parking;AIslandSupercar* SUV=nullptr;AIslandSupercar* Pickup=nullptr;
 ANeonCityPlayerCharacter* Player=nullptr;ACameraActor* Camera=nullptr;AStaticMeshActor* Track=nullptr;FVector Origin;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandSupercarPIE,"NeonCity.Polish.IslandSupercarPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandSupercarPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FIslandFleetAudit(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
