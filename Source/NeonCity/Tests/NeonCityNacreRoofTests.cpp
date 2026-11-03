#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/NacreRoofComponent.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/Collision/CollisionConstraintFlags.h"
#include "HAL/IConsoleManager.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
namespace
{
struct FNativeRoofPairResult { TAtomic<int32> Value{-1}; };
TSharedRef<FNativeRoofPairResult,ESPMode::ThreadSafe> QueryNativeRoofPair(AIslandSupercar* Car,UNacreRoofComponent* Roof)
{
 auto Result=MakeShared<FNativeRoofPairResult,ESPMode::ThreadSafe>();
 auto* ChassisBody=Car->GetMesh()->GetBodyInstance(TEXT("Root"));
 auto* PanelBody=Roof->GetFrontPanel()->GetBodyInstance();
 auto* Chassis=ChassisBody?ChassisBody->GetPhysicsActor():nullptr;
 auto* Panel=PanelBody?PanelBody->GetPhysicsActor():nullptr;
 auto* Solver=Chassis?Chassis->GetSolver<Chaos::FPhysicsSolver>():nullptr;
 if(!Solver || !Panel || Panel->GetSolver<Chaos::FPhysicsSolver>()!=Solver){Result->Value.Store(0);return Result;}
 const auto ChassisID=Chassis->GetGameThreadAPI().UniqueIdx(),PanelID=Panel->GetGameThreadAPI().UniqueIdx();
 // Read the real native filter on its owning physics thread. Publish only the
 // completed result; the game thread never touches the manager or PT handles.
 Solver->EnqueueCommandImmediate([Solver,ChassisID,PanelID,Result]()
 {
  auto* ChassisProxy=Solver->GetParticleProxy_PT(ChassisID);
  auto* PanelProxy=Solver->GetParticleProxy_PT(PanelID);
  auto* A=ChassisProxy?ChassisProxy->GetHandle_LowLevel():nullptr;
  auto* B=PanelProxy?PanelProxy->GetHandle_LowLevel():nullptr;
  const auto& Manager=Solver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager();
  Result->Value.Store(A && B && A->CastToRigidParticle() && B->CastToRigidParticle()
   && A->CastToRigidParticle()->UseIgnoreCollisionManager() && B->CastToRigidParticle()->UseIgnoreCollisionManager()
   && Manager.IgnoresCollision(A,B) && Manager.IgnoresCollision(B,A)?1:0);
 });
 return Result;
}
class FNacreRoofInput : public IAutomationLatentCommand
{
public:
 explicit FNacreRoofInput(FAutomationTestBase* In,bool InterlockOnly=false):Test(In),bInterlockOnly(InterlockOnly){}
 void Key(FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool Next(int32 S){Stage=S;At=World->GetTimeSeconds();return false;}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150){Test->AddError(FString::Printf(TEXT("Nacre input timeout stage%d speed%.3f throttle%.3f brake%.3f handbrake%d interlock%d engine%d health%.2f position%s"),Stage,Car?Car->GetDrivingComponent()->GetSpeedKmh():-1,Car?Car->GetChaosMovement()->GetThrottleInput():-1,Car?Car->GetChaosMovement()->GetBrakeInput():-1,Car?Car->GetChaosMovement()->GetHandbrakeInput():false,Car?Car->GetDrivingComponent()->IsMechanicalMotionInterlocked():false,Car?int(Car->GetDrivingComponent()->GetEngineState()):-1,Car?Car->GetHealthComponent()->GetCurrentHealth():-1,Car?*Car->GetActorLocation().ToString():TEXT("none")));return true;}
  if(!World){World=GEditor?GEditor->PlayWorld.Get():nullptr;PC=World?World->GetFirstPlayerController():nullptr;}
  if(!PC || World->GetTimeSeconds()<4)return false;
  const float Now=World->GetTimeSeconds(),Elapsed=Now-At;
  Evidence.Record(World,PC,Stage);
  if(Car && Roof && Car->GetPresentationProfile())
  {
   const auto& Data=*Car->GetPresentationProfile();const float Progress=Roof->GetProgress();
   if(!Roof->GetFrontPanel()->GetRelativeLocation().Equals(Data.PanelTranslationCm(Progress),.02))
   {Test->AddError(TEXT("Actual rigid roof transform diverges from fitted source motion"));return true;}
   const auto& Arms=Roof->GetSwingArms();const auto& Carriers=Roof->GetPhysicalCarriers();
   for(int32 I=0;I<4;++I)
   {
    const FVector Pin=Data.Anchors[I].BodyPinCm+FVector(0,0,Data.CarrierHeightCm(Progress));
    const FVector End=Arms[I]->GetRelativeTransform().TransformPosition(FVector(0,0,Data.ArmLengthCm));
    if(!Carriers[I]->GetRelativeLocation().Equals(Pin,.02) || !End.Equals(Data.Anchors[I].RoofPinCm+Data.PanelTranslationCm(Progress),.02))
    {Test->AddError(TEXT("Physical arm/carrier endpoints detach during actual roof operation"));return true;}
   }
  }
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(World);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(60380,-152870,750))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   int32 Defaults=0;TSet<FGuid> Identities;
   for(TActorIterator<AIslandSupercar> It(World);It;++It)
   {
    if(It->ActorHasTag(TEXT("NeonCity.IslandDefaultFleet"))){++Defaults;Identities.Add(It->VehicleID);}
    if(It->ActorHasTag(TEXT("NeonCity.IslandDefaultFleet")) && It->bRetractable
     && It->VehicleID==FGuid(0xEA3CB593,0x4EC94A19,0x87F7A19A,0x34C2A1D8))Car=*It;
   }
   if(!Test->TestNotNull(TEXT("New original Nacre exists in authored island"),Car))return true;
   Test->TestEqual(TEXT("Default group contains exactly three unique cars"),Defaults,3);Test->TestEqual(TEXT("Default fleet IDs unique"),Identities.Num(),3);
   Roof=Car->GetRoofMechanism();Occupant=Car->FindComponentByClass<UTransportOccupantComponent>();
   if(Car->GetPresentationProfile())
   {
    if(!PairQuery)PairQuery=QueryNativeRoofPair(Car,Roof);
    if(PairQuery->Value.Load()<0)return false;
    Test->TestEqual(TEXT("Animated roof and its owning chassis actually ignore one another in Chaos"),PairQuery->Value.Load(),1);PairQuery.Reset();
    const auto* ManagerCVar=IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.EnableCollisionManager"));
    if(ManagerCVar && ManagerCVar->GetBool())Test->TestNull(TEXT("Collision-only roof filter does not add a sleeping-island joint"),Car->FindComponentByClass<UPhysicsConstraintComponent>());
    Test->TestEqual(TEXT("Roof retains collision against other vehicles"),Roof->GetFrontPanel()->GetCollisionResponseToChannel(ECC_Vehicle),ECR_Block);
    Test->TestEqual(TEXT("Roof retains native player standing collision"),Roof->GetFrontPanel()->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
   }
   HealthBefore=Car->GetHealthComponent()->GetCurrentHealth();
   if(!bInterlockOnly){FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);Save=World->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();if(!Save->ConfigureSlot(TEXT("NeonCity_Test_RoofCondition_")+FPaths::MakeValidFileName(Run.Left(30)),false) || Save->DoesSaveExist()){Test->AddError(TEXT("Fresh isolated roof-condition checkpoint required"));return true;}}
   if(!Test->TestNotNull(TEXT("Native continuous roof mechanism exists"),Roof))return true;
   for(auto* Part:{Roof->GetFrontPanel(),Roof->GetBayCover()})
   {Test->TestNotNull(TEXT("Independent animated mesh loaded"),Part->GetStaticMesh().Get());Test->TestTrue(TEXT("Animated mesh registered"),Part->IsRegistered());}
   if(bInterlockOnly || FParse::Param(FCommandLine::Get(),TEXT("NeonCityRequire720S")))
   {
    if(!Test->TestNotNull(TEXT("Required new McLaren presentation is actually adopted"),Car->GetPresentationProfile()))return true;
    Test->TestNull(TEXT("Single rigid roof has no leftover legacy second panel"),Roof->GetRearPanel()->GetStaticMesh().Get());
    Test->TestEqual(TEXT("Original third fleet identity preserved"),Car->VehicleID,FGuid(0xEA3CB593,0x4EC94A19,0x87F7A19A,0x34C2A1D8));
    Test->TestEqual(TEXT("Original save profile name retained"),Car->GetVehicleProfile()->GetFName(),FName(TEXT("DA_HeroNacre")));
    Test->TestEqual(TEXT("New native chassis uses rear drive"),Car->GetChaosMovement()->DifferentialSetup.DifferentialType,EVehicleDifferential::RearWheelDrive);
    Test->TestEqual(TEXT("Resolved physical support count"),Roof->GetSwingArms().Num(),4);
    Test->TestEqual(TEXT("New roof duration"),Roof->GetCycleDuration(),11.f);
    for(int32 I=0;I<4;++I)Test->TestEqual(TEXT("Native wheel descriptor has fitted axle radius"),Car->GetChaosMovement()->Wheels[I]->WheelRadius,Car->GetPresentationProfile()->Wheels[I].RadiusCm);
   }
   // Focused test fixture starts beside the selected parked car. From here all
   // controls use ordinary player input and native driving/entry/exit physics.
   Player->SetActorLocation(Car->GetActorLocation()-Car->GetActorRightVector()*240+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((Car->GetActorLocation()-Player->GetActorLocation()).Rotation());return Next(1);
  }
  if(Stage==1 && Elapsed>.6)
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-Player->GetPawnViewLocation()).Rotation());
   if(!Test->TestEqual(TEXT("Normal eye trace focuses parked Nacre"),Player->GetInteractionComponent()->RefreshFocusedInteractable(),static_cast<AActor*>(Car)))return true;
   Key(EKeys::F,IE_Pressed);return Next(18);
  }
  if(Stage==18 && Elapsed>.2)
  {Key(EKeys::F,IE_Released);if(!Test->TestEqual(TEXT("Actual F mapping enters parked Nacre"),PC->GetPawn().Get(),static_cast<APawn*>(Car)))return true;Driver=Occupant->GetDriverMesh();if(!Test->TestNotNull(TEXT("Real driver remains represented"),Driver))return true;return Next(2);}
  if(Stage==2 && Elapsed>.4){Key(EKeys::R,IE_Pressed);Key(EKeys::W,IE_Pressed);Previous=Roof->GetProgress();return Next(3);}
  if(Stage==3 && Elapsed>.12){Key(EKeys::R,IE_Released);return Next(4);}
  if(Stage==4)
  {
   const float P=Roof->GetProgress();Test->TestTrue(TEXT("Opening progress continuous/monotone"),P>=Previous && P-Previous<.15f);Previous=P;
   Test->TestTrue(TEXT("Actual held W cannot drive through roof operation brake lock"),FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.5f);
   Test->TestTrue(TEXT("Mechanical input consumer retains roof interlock"),Car->GetDrivingComponent()->IsMechanicalMotionInterlocked());
   Test->TestTrue(TEXT("Moving roof retains its animation tick"),Roof->IsComponentTickEnabled());
   Test->TestEqual(TEXT("Native throttle remains zero with held W"),Car->GetChaosMovement()->GetThrottleInput(),0.f);
   Test->TestEqual(TEXT("Native service brake remains held"),Car->GetChaosMovement()->GetBrakeInput(),1.f);
   Test->TestTrue(TEXT("Front panel never hidden"),Roof->GetFrontPanel()->IsVisible());Test->TestTrue(TEXT("Driver remains same visible mesh"),Occupant->GetDriverMesh()==Driver && Driver->IsVisible());
   if(P>(bInterlockOnly?.15f:.40f)){Key(EKeys::W,IE_Released);Key(EKeys::R,IE_Pressed);return Next(5);}return false;
  }
  if(Stage==5 && Elapsed>.12){Key(EKeys::R,IE_Released);Previous=Roof->GetProgress();return Next(6);}
  if(Stage==6)
  {
   const float P=Roof->GetProgress();Test->TestTrue(TEXT("Mid-cycle reversal continuously closes"),P<=Previous+.0001f);Previous=P;
   if(!Roof->IsCycling())
   {
    Test->TestEqual(TEXT("Reversal completes closed"),P,0.f);
    Test->TestFalse(TEXT("Completion releases mechanical interlock"),Car->GetDrivingComponent()->IsMechanicalMotionInterlocked());
    Test->TestFalse(TEXT("Closed roof suspends idle tick"),Roof->IsComponentTickEnabled());
    if(bInterlockOnly){Key(EKeys::W,IE_Pressed);return Next(14);}
    Key(EKeys::R,IE_Pressed);return Next(7);
   }return false;
  }
  if(Stage==7 && Elapsed>.12){Key(EKeys::R,IE_Released);return Next(8);}
  if(Stage==8)
  {
   if(Roof->GetProgress()<.60f)return false;
   auto* Ownership=World->GetGameInstance()->GetSubsystem<UVehicleOwnershipSubsystem>();
   const auto Snapshot=Ownership->BuildPersistenceSnapshot();const auto* Saved=Snapshot.FindByPredicate([&](const auto& D){return D.VehicleID==Car->VehicleID;});
   if(!Test->TestNotNull(TEXT("Roof saved under stable car ID"),Saved))return true;
   Test->TestEqual(TEXT("Snapshot retains exact continuous progress"),Saved->RoofProgress,Roof->GetProgress());Test->TestTrue(TEXT("Snapshot retains open target"),Saved->RoofTargetOpen);
   SavedProgress=Saved->RoofProgress;Key(EKeys::F,IE_Pressed);return Next(9);
  }
  if(Stage==9 && Elapsed>.15){Key(EKeys::F,IE_Released);Test->TestTrue(TEXT("Ordinary exit during motion succeeds"),PC->GetPawn()==Player);return Next(10);}
  if(Stage==10)
  {
   if(Roof->IsCycling())return false;
   Test->TestEqual(TEXT("Roof completes while driver is on foot"),Roof->GetProgress(),1.f);
   Test->TestFalse(TEXT("Open roof suspends idle tick"),Roof->IsComponentTickEnabled());
   Test->TestFalse(TEXT("Open roof releases mechanical interlock"),Car->GetDrivingComponent()->IsMechanicalMotionInterlocked());
   if(Car->GetPresentationProfile())Test->TestTrue(TEXT("One rigid roof stows at fitted open endpoint"),Roof->GetFrontPanel()->GetRelativeLocation().Equals(Car->GetPresentationProfile()->PanelTranslationCm(1),.02));
   else
   {
    Test->TestTrue(TEXT("Nested front panel lies inside real storage bay"),Roof->GetFrontPanel()->GetRelativeLocation().Equals(FVector(-135,0,63),.01));
    Test->TestTrue(TEXT("Nested rear panel lies inside real storage bay"),Roof->GetRearPanel()->GetRelativeLocation().Equals(FVector(-135,0,73),.01));
   }
   Test->TestTrue(TEXT("Rear cover physically closed over panels"),Roof->GetBayCover()->GetRelativeRotation().IsNearlyZero(.01));
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Nacre-Roof-Open-OnFoot.png"));
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-Player->GetPawnViewLocation()).Rotation());
   if(!Test->TestEqual(TEXT("Normal eye trace focuses Nacre on return"),Player->GetInteractionComponent()->RefreshFocusedInteractable(),static_cast<AActor*>(Car)))return true;
   Key(EKeys::F,IE_Pressed);return Next(19);
  }
  if(Stage==19 && Elapsed>.2){Key(EKeys::F,IE_Released);if(!Test->TestEqual(TEXT("Actual F reentry retains roof state"),PC->GetPawn().Get(),static_cast<APawn*>(Car)))return true;Key(EKeys::W,IE_Pressed);Key(EKeys::A,IE_Pressed);return Next(20);}
  if(Stage==20)
  {
   Test->TestEqual(TEXT("Open roof stays stowed while driving"),Roof->GetProgress(),1.f);
   Test->TestFalse(TEXT("Open roof driving is not mechanically interlocked"),Car->GetDrivingComponent()->IsMechanicalMotionInterlocked());
   if(Elapsed>.25 && !SteeringChecked){Test->TestTrue(TEXT("Open roof normal steering input reaches native wheels"),FMath::Abs(Car->GetChaosMovement()->GetSteeringInput())>.1f);Key(EKeys::A,IE_Released);SteeringChecked=true;}
   if(Car->GetDrivingComponent()->GetSpeedKmh()<8)return false;
   Key(EKeys::W,IE_Released);Key(EKeys::A,IE_Released);Key(EKeys::S,IE_Pressed);return Next(21);
  }
  if(Stage==21 && Car->GetDrivingComponent()->GetSpeedKmh()<.5f)
  {Key(EKeys::S,IE_Released);Test->TestTrue(TEXT("Open roof retains visible intact driver"),Occupant->GetDriverMesh()==Driver && Driver->IsVisible());Test->TestEqual(TEXT("Open roof drive/braking causes no condition loss"),Car->GetHealthComponent()->GetCurrentHealth(),HealthBefore);return Next(11);}
  if(Stage==11 && Elapsed>.4){Key(EKeys::R,IE_Pressed);return Next(12);}
  if(Stage==12 && Elapsed>.12){Key(EKeys::R,IE_Released);return Next(13);}
  if(Stage==13)
  {
   if(Roof->IsCycling())return false;
   Test->TestEqual(TEXT("Full closing sequence completes"),Roof->GetProgress(),0.f);
   Test->TestFalse(TEXT("Full close releases mechanical interlock"),Car->GetDrivingComponent()->IsMechanicalMotionInterlocked());
   Test->TestFalse(TEXT("Full close suspends idle tick"),Roof->IsComponentTickEnabled());
   FNeonCityVisualEvidence::CaptureHUD(World,TEXT("Nacre-Roof-Closed-Driver.png"));
   Key(EKeys::F6,IE_Pressed);return Next(25);
  }
  if(Stage==25 && Elapsed>.7)
  {Key(EKeys::F6,IE_Released);if(Save->GetLastResult()!=ENeonCitySaveResult::Success){Test->AddError(TEXT("Closed roof actual F6 checkpoint failed"));return true;}Key(EKeys::R,IE_Pressed);return Next(26);}
  if(Stage==26 && Elapsed>.4)
  {Key(EKeys::R,IE_Released);Test->TestTrue(TEXT("Roof wakes again before endpoint restoration"),Roof->IsCycling() && Roof->IsComponentTickEnabled());Key(EKeys::F7,IE_Pressed);return Next(27);}
  if(Stage==27 && Elapsed>.7)
  {
   Key(EKeys::F7,IE_Released);if(Save->GetLastResult()!=ENeonCitySaveResult::Success){Test->AddError(TEXT("Native F7 endpoint restoration failed"));return true;}
   Test->TestEqual(TEXT("Real endpoint checkpoint restores closed roof"),Roof->GetProgress(),0.f);Test->TestFalse(TEXT("Restored closed roof suspends tick"),Roof->IsComponentTickEnabled());
   if(Car->GetPresentationProfile())
   {
    if(!PairQuery)PairQuery=QueryNativeRoofPair(Car,Roof);
    if(PairQuery->Value.Load()<0)return false;
    Test->TestEqual(TEXT("F7 rebinds the actual native collision pair to the restored chassis and roof"),PairQuery->Value.Load(),1);PairQuery.Reset();
   }
   TInlineComponentArray<UAudioComponent*> Audio(Car);for(const auto* C:Audio)if(C->GetFName()==TEXT("NacreRoofMotor"))Test->TestFalse(TEXT("Endpoint restore stops old looping motor"),C->IsPlaying());
   if(PC->GetPawn()==Player){PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-Player->GetPawnViewLocation()).Rotation());return Next(29);}
   if(PC->GetPawn()!=Car){Test->AddError(TEXT("Roof restore lost native player/car possession"));return true;}Key(EKeys::W,IE_Pressed);return Next(14);
  }
  // The safe saved exit can lie beyond the eye trace's 250cm range. Approach
  // through normal walking/collision before genuine F boarding, as a player does.
  if(Stage==29 && Elapsed>.25)
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()==Car){Key(EKeys::F,IE_Pressed);return Next(28);}
   if(Elapsed>5){Test->AddError(FString::Printf(TEXT("Normal roof checkpoint walking return cannot focus car: player%s car%s distance%.2f"),*Player->GetActorLocation().ToString(),*Car->GetActorLocation().ToString(),FVector::Dist(Player->GetActorLocation(),Car->GetActorLocation())));return true;}
   Player->AddMovementInput((Car->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D(),1);return false;
  }
  if(Stage==28 && Elapsed>.2){Key(EKeys::F,IE_Released);if(PC->GetPawn()!=Car){Test->AddError(TEXT("Normal boarding after endpoint restore failed"));return true;}Key(EKeys::W,IE_Pressed);return Next(14);}
  if(Stage==14)
  {
   if(Elapsed>2 && !RestoreDriveLogged){RestoreDriveLogged=true;Test->AddInfo(FString::Printf(TEXT("Restored drive parked%d speed%.3f gear%d rpm%.3f velocity%s"),Car->GetChaosMovement()->IsParked(),Car->GetDrivingComponent()->GetSpeedKmh(),Car->GetChaosMovement()->GetCurrentGear(),Car->GetChaosMovement()->GetEngineRotationSpeed(),*Car->GetVelocity().ToString()));}
   if(Car->GetDrivingComponent()->GetSpeedKmh()<12)return false;
   Key(EKeys::R,IE_Pressed);return Next(15);
  }
  if(Stage==15 && Elapsed>.20)
  {
   Key(EKeys::R,IE_Released);Key(EKeys::W,IE_Released);Key(EKeys::SpaceBar,IE_Pressed);
   Test->TestFalse(TEXT("Actual moving car rejects roof key above safety speed"),Roof->IsCycling());Test->TestEqual(TEXT("Rejected operation keeps roof closed"),Roof->GetProgress(),0.f);return Next(16);
  }
  if(Stage==16 && Car->GetDrivingComponent()->GetSpeedKmh()<.5f)
  {Key(EKeys::SpaceBar,IE_Released);Key(EKeys::F,IE_Pressed);return Next(17);}
  if(Stage==17 && Elapsed>.15){Key(EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("Driver exits intact after repeated cycles and driving"),PC->GetPawn()==Player) || bInterlockOnly)return true;PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-Player->GetPawnViewLocation()).Rotation());return Next(30);}
  if(Stage==30 && Elapsed>.25){if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Car){Test->AddError(TEXT("Post-load parked roof return cannot focus car"));return true;}Key(EKeys::F,IE_Pressed);return Next(31);}
  if(Stage==31 && Elapsed>.2){Key(EKeys::F,IE_Released);if(PC->GetPawn()!=Car){Test->AddError(TEXT("Post-load roof boarding failed"));return true;}Key(EKeys::SpaceBar,IE_Pressed);Key(EKeys::R,IE_Pressed);return Next(32);}
  if(Stage==32 && Elapsed>.12){Key(EKeys::R,IE_Released);return Next(33);}
  if(Stage==33 && !Roof->IsCycling()){Test->TestEqual(TEXT("Post-F7 roof opens normally again"),Roof->GetProgress(),1.f);Key(EKeys::SpaceBar,IE_Released);Key(EKeys::F,IE_Pressed);return Next(34);}
  if(Stage==34 && Elapsed>.2){Key(EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("Post-F7 open roof normal exit succeeds"),PC->GetPawn()==Player))return true;RestoredParkPosition=Car->GetActorLocation();return Next(35);}
  if(Stage==35 && Elapsed>20){Test->TestTrue(TEXT("Post-F7 stowed roof cannot push the parked chassis"),FVector::Dist(RestoredParkPosition,Car->GetActorLocation())<25 && Car->GetVelocity().Size()<10);Test->TestEqual(TEXT("Roof cycles and post-load parking retain intact vehicle condition"),Car->GetHealthComponent()->GetCurrentHealth(),HealthBefore);return true;}
  return false;
 }
 FAutomationTestBase* Test;double Started=0;float At=0,Previous=0,SavedProgress=0;int32 Stage=0;
 bool bInterlockOnly=false,SteeringChecked=false,RestoreDriveLogged=false;float HealthBefore=0;UNeonCitySaveSubsystem* Save=nullptr;
 FVector RestoredParkPosition=FVector::ZeroVector;
 UWorld* World=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandSupercar* Car=nullptr;
 UNacreRoofComponent* Roof=nullptr;UTransportOccupantComponent* Occupant=nullptr;USkeletalMeshComponent* Driver=nullptr;
 TSharedPtr<FNativeRoofPairResult,ESPMode::ThreadSafe> PairQuery;
 FNeonCityVisualEvidence Evidence{TEXT("NacreRoofNativeInput")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNacreRoofNativeInput,"NeonCity.Phase205cUpgrade.NacreRoofNativeInputPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNacreRoofNativeInput::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FNacreRoofInput(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(F720SRoofInterlock,"NeonCity.Phase205cUpgrade.McLarenRoofInterlockPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool F720SRoofInterlock::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FNacreRoofInput(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
