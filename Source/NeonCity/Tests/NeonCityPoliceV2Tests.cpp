#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Police/PoliceResponseData.h"
#include "Police/PoliceDispatchSubsystem.h"
#include "Police/PoliceUnitManager.h"
#include "Police/PoliceObservation.h"
#include "Police/PoliceSearchCoordinator.h"
#include "Police/RoadblockManager.h"
#include "Police/SecurityCamera.h"
#include "Police/PoliceTestArea.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceController.h"
#include "AI/Police/PoliceVehicle.h"
#include "AI/Police/PoliceSensorComponent.h"
#include "AI/Police/PoliceResponseDirector.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCThreatComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "Wanted/SearchSystem.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoliceV2Model,"NeonCity.PoliceV2.ProfileInvariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPoliceV2Model::RunTest(const FString& P)
{
 auto* D=NewObject<UPoliceResponseData>(); TestTrue(TEXT("Six finite response tiers"),D->IsValidProfile());
 TestTrue(TEXT("Escalating resources remain conservative"),D->Levels[1].Vehicles==0 && D->Levels[2].Vehicles<=3 && D->Levels[3].Roadblocks==1 && D->Levels[5].Roadblocks==2 && D->Levels[5].Officers<=6);
 TestTrue(TEXT("Police attack more serious than minor property damage"),D->GetSeverity(EPoliceOffense::PoliceAttack,25)>D->GetSeverity(EPoliceOffense::PropertyDamage,5));
 TestTrue(TEXT("Known cars identify faster, never instant initial identification"),D->KnownVehicleIdentificationSeconds>=.1f && D->KnownVehicleIdentificationSeconds<D->PersonIdentificationSeconds);
 D->Levels[5].Vehicles=50; TestFalse(TEXT("Unbounded vehicle budget rejected"),D->IsValidProfile());
 D=NewObject<UPoliceResponseData>(); D->ReplacementInterval=0; TestFalse(TEXT("Infinite replacement rate rejected"),D->IsValidProfile()); return true;
}
namespace NeonCityPoliceV2Tests
{
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr;
 bool Ready() { W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr; return Player && W->HasBegunPlay() && W->GetTimeSeconds()>1 && Player->GetCharacterMovement()->IsMovingOnGround(); }
 void Place(FVector P) { Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately(); }
 void Isolate() { for(TActorIterator<APoliceResponseDirector> It(W);It;++It) It->Destroy(); for(TActorIterator<ANeonCityTrafficVehicle> It(W);It;++It) { It->SetActorTickEnabled(false); It->SetActorEnableCollision(false); } }
 ANeonCityNPCCharacter* NPC(FVector P) { const FTransform T(P); auto* N=W->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),T); N->InitializePedestrian(TEXT("PoliceV2.Witness"),nullptr,{},163); N->FinishSpawning(T); return N; }
 ANeonCityPhysicsProp* Prop(FVector P) { const FTransform T(P); auto* A=W->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),T); A->Configure(ECityPhysicsProp::LampPost); A->FinishSpawning(T); return A; }
 FGuid Damage(AActor* Target,float Amount=5,ENeonCityDamageType Type=ENeonCityDamageType::Impact,AActor* Source=nullptr)
 { FNeonCityDamageEvent D; D.SourceActor=Source?Source:Player; D.Instigator=PC; D.DamageType=Type; D.DamageAmount=Amount; W->GetSubsystem<UDamageSystem>()->ApplyDamage(Target,D); return W->GetSubsystem<UCrimeSubsystem>()->GetEvents().Last().Id; }
 void Shot(const TCHAR* Name,FVector Eye,FVector Target)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityPoliceV2Capture"))) return;
  auto* Camera=W->SpawnActor<ACameraActor>(); Camera->SetActorLocation(Eye); Camera->SetActorRotation((Target-Eye).Rotation()); Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;
  Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
  PC->SetViewTarget(Camera); if(PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
  IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase16")),true);
  // Capture the PIE render target explicitly: the global screenshot request can be
  // consumed by an editor viewport when many maps have been opened in one test process.
  bool Captured=false;
  if(auto* Client=W->GetGameViewport(); Client && Client->Viewport)
  {
   if(PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
   Client->Viewport->Draw(false);
   TArray<FColor> Pixels;
   if(Client->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pixel:Pixels) Pixel.A=255;
    const FIntPoint Size=Client->Viewport->GetSizeXY(); TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    Captured=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Phase16")/Name));
   }
  }
  if(!Captured) UE_LOG(LogTemp,Error,TEXT("PIE viewport capture failed: %s"),Name);
  // Capture must not alter later spawn-visibility decisions or possession camera state.
  FTimerHandle Restore; TWeakObjectPtr<APlayerController> Owner=PC; TWeakObjectPtr<ACameraActor> ShotCamera=Camera;
  W->GetTimerManager().SetTimer(Restore,[Owner,ShotCamera]() { if(Owner.IsValid() && ShotCamera.IsValid() && Owner->GetViewTarget()==ShotCamera.Get()) Owner->SetViewTarget(Owner->GetPawn()); if(ShotCamera.IsValid()) ShotCamera->Destroy(); },.25f,false);
 }
 AActor* Wall(FVector P,FVector Extent)
 { auto* A=W->SpawnActor<AActor>(); auto* C=NewObject<UBoxComponent>(A); A->SetRootComponent(C); C->SetBoxExtent(Extent); C->SetCollisionProfileName(TEXT("BlockAll")); C->RegisterComponent(); A->SetActorLocation(P); return A; }
};
class FWitness : public IAutomationLatentCommand
{
public:
 explicit FWitness(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Witness timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Place(FVector(-6050,2650,94));
   // The authored officer normally sees the west alley. Put the patrol on the far side of the solid cafe.
   for(TActorIterator<APoliceCharacter> It(F.W);It;++It) It->SetActorLocation(FVector(-4400,1000,135));
   Prop=F.Prop(FVector(-5800,2600,162));
   F.Damage(Prop); Test->TestEqual(TEXT("Unseen property damage cannot create pursuit"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),0);
   Test->TestFalse(TEXT("No perfect location without observer"),F.W->GetSubsystem<USearchSystem>()->HasLocation());
   F.Shot(TEXT("A-Unseen-Property.png"),FVector(-6100,2100,330),Prop->GetActorLocation());
   Witness=F.NPC(FVector(-6000,2900,94)); Stage=1; Phase=F.W->GetTimeSeconds(); return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto* Crime=F.W->GetSubsystem<UCrimeSubsystem>(); auto* Search=F.W->GetSubsystem<USearchSystem>();
  if(Stage==1 && Now-Phase>.3f)
  {
   Incident=F.Damage(Prop); const auto O=Witness->GetThreatComponent()->GetObservation(Incident); Known=O.LastKnownLocation;
   Test->TestTrue(TEXT("Witness captured a real visible suspect snapshot"),O.IsValid() && Known.Equals(F.Player->GetActorLocation()));
   Test->TestFalse(TEXT("Civilian call is not instant"),Crime->GetReports().ContainsByPredicate([&](const auto& R){return R.CrimeId==Incident;}));
   F.Place(FVector(-4500,3400,100)); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>2)
  {
   const auto* Report=Crime->GetReports().FindByPredicate([&](const auto& R){return R.CrimeId==Incident;});
   Test->TestTrue(TEXT("Automatic delayed report uses observed position, not current player position"),Report && Report->Observation.LastKnownLocation.Equals(Known));
   Test->TestTrue(TEXT("Dispatch receives light response with last known evidence"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel()==1 && Search->GetLastKnownLocation().Equals(Known));
   F.Shot(TEXT("B-Witness-Report.png"),FVector(-6250,2100,380),Witness->GetActorLocation());
   F.Place(FVector(-6050,2650,94)); Canceled=F.Damage(Prop); Witness->GetHealthComponent()->SetCurrentHealth(15);
   Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>1.8f)
  {
   Witness->GetHealthComponent()->ApplyPersistenceHealth(100,100);
   Test->TestFalse(TEXT("Incapacitation cancels pending report, including after recovery"),Crime->ReportEvent(Canceled,Witness));
   Test->TestFalse(TEXT("Canceled call not submitted"),Crime->GetReports().ContainsByPredicate([&](const auto& R){return R.CrimeId==Canceled;}));
   Witness->Destroy(); Prop->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0; float Phase=0; ANeonCityNPCCharacter* Witness=nullptr; ANeonCityPhysicsProp* Prop=nullptr; FGuid Incident,Canceled; FVector Known;
};
class FCamera : public IAutomationLatentCommand
{
public:
 explicit FCamera(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Camera timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Isolate(); F.Place(FVector(0,0,5000)); F.Player->GetCharacterMovement()->DisableMovement();
   Camera=F.W->SpawnActor<ASecurityCamera>(FVector(-600,0,5000),FRotator::ZeroRotator); Prop=F.Prop(FVector(200,100,5000));
   F.Damage(Prop); auto* Search=F.W->GetSubsystem<USearchSystem>();
   Test->TestTrue(TEXT("Camera sees actual crime within cone and LOS"),Search->GetDescriptor().Source==EPoliceEvidenceSource::Camera && Search->HasVisualContact());
   Snapshot=Search->GetLastKnownLocation(); Wall=F.Wall(FVector(-300,0,5000),FVector(35,600,600)); F.Place(FVector(100,100,5000));
   Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto* Search=F.W->GetSubsystem<USearchSystem>();
  if(Stage==1 && Now-Phase>1)
  {
   Test->TestFalse(TEXT("Camera cannot identify through solid wall"),Search->HasVisualContact());
   Test->TestTrue(TEXT("Hidden movement never updates snapshot"),Snapshot.Equals(Search->GetLastKnownLocation()));
   Test->TestTrue(TEXT("Confidence ages and search radius widens"),Search->GetConfidence()<1 && Search->GetDynamicSearchRadius(600)>600);
   Wall->Destroy(); Camera->SetActorRotation(FRotator(0,180,0)); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>1)
  {
   Test->TestFalse(TEXT("Surveillance is not 360-degree vision"),Search->HasVisualContact()); Camera->SetActorRotation(FRotator::ZeroRotator); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>1.2f)
  {
   Test->TestTrue(TEXT("Person reacquired only after visible observation interval"),Search->HasVisualContact() && Search->GetLastKnownLocation().Equals(F.Player->GetActorLocation()));
   Camera->Destroy(); Prop->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0; float Phase=0; FVector Snapshot; ASecurityCamera* Camera=nullptr; ANeonCityPhysicsProp* Prop=nullptr; AActor* Wall=nullptr;
};
class FSwitch : public IAutomationLatentCommand
{
public:
 explicit FSwitch(FAutomationTestBase* T):Test(T) {}
 void Park(ANeonCityBaseVehicle* C,FVector P)
 { C->GetDrivingComponent()->SetDrivingEnabled(false); C->GetChaosMovement()->ResetVehicle(); C->SetActorLocationAndRotation(P,FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics); }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>55) { Test->AddError(FString::Printf(TEXT("Vehicle switch timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Isolate();
   for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) { if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::SportsCar) Sports=*It; if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::Sedan) Sedan=*It; }
   if(!Test->TestTrue(TEXT("Existing two vehicle profiles available"),Sports && Sedan)) return true;
   Park(Sports,FVector(-3600,0,150)); F.Place(FVector(-3600,-220,120));
   for(TActorIterator<ANeonCityTrafficRoute> It(F.W);It;++It) Road=*It;
   const float D=300; auto* S=Road->GetRoadSpline(); Police=F.W->SpawnActor<APoliceVehicle>(S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World),S->GetRotationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World)); Police->InitializeRoute(Road,D); Police->SetResponding(true);
   Camera=F.W->SpawnActor<ASecurityCamera>(FVector(-4300,-450,300),FRotator(-12,0,0));
   Prop=F.Prop(FVector(-3400,-450,162)); F.Damage(Prop);
   Test->TestTrue(TEXT("Crime visibly establishes person description before entry"),!F.W->GetSubsystem<USearchSystem>()->GetDescriptor().CharacterAppearance.IsNone()); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  auto* Search=F.W->GetSubsystem<USearchSystem>(); const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Visible driver enters Sports using normal interaction"),Sports->GetVehicleInteraction()->Enter(F.Player));
   Test->TestTrue(TEXT("Observed entry records stable Sports identity"),Search->GetDescriptor().Vehicle.VehicleID==Sports->VehicleID);
   Search->Clear(); // A fresh crime seen through the vehicle body identifies the car, not its concealed driver.
   F.Damage(Prop,10,ENeonCityDamageType::Impact,Sports); F.Damage(Prop,10,ENeonCityDamageType::Impact,Sports);
   Test->TestTrue(TEXT("Vehicle-only observation does not invent driver appearance"),Search->GetDescriptor().InVehicle && Search->GetDescriptor().CharacterAppearance.IsNone());
   Sports->GetDrivingComponent()->SetDriveInput(1); Phase=Now; Start=Sports->GetActorLocation(); Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>1.5f)
  {
   Test->TestTrue(TEXT("Actual Chaos drive advances Sports"),FVector::Dist2D(Start,Sports->GetActorLocation())>40);
   Test->TestTrue(TEXT("Police pursues known vehicle"),Police->GetPoliceState()==ENeonCityPoliceState::Chase);
   F.Shot(TEXT("C-Known-Sports.png"),Sports->GetActorLocation()+FVector(-800,-600,400),Sports->GetActorLocation());
   Sports->GetDrivingComponent()->SetDriveInput(0);
   Camera->SetActorLocation(Sports->GetActorLocation()+FVector(-600,500,300)); Camera->SetActorRotation((Sports->GetActorLocation()-Camera->GetActorLocation()).Rotation());
   Test->TestTrue(TEXT("Visible exit uses normal driver restoration"),Sports->GetVehicleInteraction()->RestoreDriver(true));
   Test->TestTrue(TEXT("Observed exit links person to already known car"),!Search->GetDescriptor().CharacterAppearance.IsNone() && !Search->GetDescriptor().InVehicle);
   Park(Sports,Sports->GetActorLocation()); F.Place(Sports->GetActorLocation()+FVector(0,220,100));
   Test->TestTrue(TEXT("Observed return to Sports succeeds"),Sports->GetVehicleInteraction()->Enter(F.Player));
   Camera->Destroy(); Camera=nullptr;
   // Arrange the hidden parking leg behind the existing block. Recognition remains live throughout.
   Park(Sports,FVector(5100,920,160)); Park(Sedan,FVector(4200,920,160)); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>1)
  {
   Test->TestFalse(TEXT("Parking leg genuinely outside police observation"),Search->HasVisualContact()); Old=Search->GetDescriptor();
   Test->TestTrue(TEXT("Driver exits old car"),Sports->GetVehicleInteraction()->RestoreDriver(true)); F.Place(Sedan->GetActorLocation()+FVector(0,-220,100)); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>.4f)
  {
   Test->TestTrue(TEXT("Unobserved normal interaction enters Sedan"),Sedan->GetVehicleInteraction()->Enter(F.Player)); Sedan->GetDrivingComponent()->SetDriveInput(-1); Start=Sedan->GetActorLocation(); Phase=Now; Stage=5; return false;
  }
  if(Stage==5 && Now-Phase>1.5f)
  {
   Test->TestTrue(TEXT("Sedan actually drives away in reverse direction"),FVector::Dist2D(Start,Sedan->GetActorLocation())>40);
   Test->TestTrue(TEXT("Unknown sedan never inherits Sports identity or live position"),Search->GetDescriptor().Vehicle.VehicleID==Sports->VehicleID && Search->GetLastKnownLocation().Equals(Old.LastKnownLocation));
   F.Shot(TEXT("C-Hidden-Sedan.png"),Sedan->GetActorLocation()+FVector(-700,-600,400),Sedan->GetActorLocation());
   Park(Sedan,FVector(4200,920,160)); Phase=Now; Stage=6; return false;
  }
  if(Stage==6 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Exit Sedan before visual reidentification"),Sedan->GetVehicleInteraction()->RestoreDriver(true));
   Camera=F.W->SpawnActor<ASecurityCamera>(F.Player->GetActorLocation()+FVector(-650,0,80),FRotator(-5,0,0)); Phase=Now; Stage=7; return false;
  }
  if(Stage==7 && Now-Phase>1.3f)
  {
   Test->AddInfo(FString::Printf(TEXT("Reidentify: visible=%d confidence=%.3f vehicle=%d camera=%s person=%s"),Search->HasVisualContact(),Search->GetConfidence(),Search->GetDescriptor().InVehicle,*Camera->GetActorLocation().ToString(),*F.Player->GetActorLocation().ToString()));
   Test->TestTrue(TEXT("Seeing known person again rebuilds confidence"),Search->HasVisualContact() && !Search->GetDescriptor().InVehicle && Search->GetConfidence()>.9f);
   // A subsequently witnessed entry is positive evidence for the formerly unknown Sedan.
   F.Place(Sedan->GetActorLocation()+FVector(0,220,100));
   Test->TestTrue(TEXT("Observed reentry succeeds"),Sedan->GetVehicleInteraction()->Enter(F.Player));
   Test->TestTrue(TEXT("Visible person-to-car transition associates new vehicle"),Search->GetDescriptor().Vehicle.VehicleID==Sedan->VehicleID);
   Camera->Destroy(); Police->Destroy(); Prop->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0; float Phase=0; FVector Start; FSuspectDescriptor Old;
 ANeonCityBaseVehicle *Sports=nullptr,*Sedan=nullptr; APoliceVehicle* Police=nullptr; ANeonCityTrafficRoute* Road=nullptr; ANeonCityPhysicsProp* Prop=nullptr; ASecurityCamera* Camera=nullptr;
};
class FDispatch : public IAutomationLatentCommand
{
public:
 explicit FDispatch(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>65) { Test->AddError(FString::Printf(TEXT("Dispatch timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   if(!bPositioned)
   {
    F.Place(FVector(-5000,0,100)); F.PC->SetControlRotation(FRotator(0,180,0)); bPositioned=true;
    return false;
   }
   // The teleport/control rotation above do not update the camera cache synchronously.
   // Wait for the real camera tick before any crime can enable visibility-dependent dispatch.
   FVector InitialEye; FRotator InitialView; F.PC->GetPlayerViewPoint(InitialEye,InitialView);
   if(FVector::Dist(InitialEye,F.Player->GetActorLocation())>700 || FVector::DotProduct(InitialView.Vector(),FVector(-1,0,0))<.98f) return false;
   Test->AddInfo(FString::Printf(TEXT("Crime fixture camera settled: eye %s view %s player %s"),*InitialEye.ToString(),*InitialView.ToString(),*F.Player->GetActorLocation().ToString()));
   Camera=F.W->SpawnActor<ASecurityCamera>(FVector(-5500,0,250),FRotator(-12,0,0)); Victim=F.NPC(FVector(-4800,0,100)); Victim->GetController()->SetActorTickEnabled(false);
   F.Damage(Victim,12,ENeonCityDamageType::Impact); F.Damage(Victim,12,ENeonCityDamageType::Impact);
   Test->TestEqual(TEXT("Two observed assaults reach Level 3 without forcing state"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),3);
   Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  auto* Dispatch=F.W->GetSubsystem<UPoliceDispatchSubsystem>(); auto* Blocks=F.W->GetSubsystem<URoadblockManager>(); const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>5)
  {
   Test->TestEqual(TEXT("Level 3 dispatch deploys one physical roadblock"),Blocks->GetCount(),1);
   if(!Test->TestTrue(TEXT("Roadblock exists"),Blocks->GetPositions().Num()>0)) return true;
   const FVector P=Blocks->GetPositions()[0];
   FVector DeploymentEye; FRotator DeploymentView; F.PC->GetPlayerViewPoint(DeploymentEye,DeploymentView);
   Test->AddInfo(FString::Printf(TEXT("Roadblock geometry: block %s eye %s view %s player %s"),*P.ToString(),*DeploymentEye.ToString(),*DeploymentView.ToString(),*F.Player->GetActorLocation().ToString()));
   Test->TestTrue(TEXT("Roadblock is distant and outside direct camera view"),NeonCityObservation::SafeDeployment(F.W,P,2400));
   FVector Eye; FRotator View; F.PC->GetPlayerViewPoint(Eye,View);
   Test->TestTrue(TEXT("Independent camera geometry puts deployed block behind view and beyond 25 metres"),FVector::Dist(P,Eye)>2500 && FVector::DotProduct((P-Eye).GetSafeNormal(),View.Vector())<0);
   FCollisionQueryParams Q(SCENE_QUERY_STAT(TestRoadblock));
   const FVector Alternate(P.X,-P.Y,110); FHitResult Hit;
   Test->TestFalse(TEXT("Opposite lane permits a swept bypass"),F.W->SweepSingleByChannel(Hit,Alternate-FVector(400,0,0),Alternate+FVector(400,0,0),FQuat::Identity,ECC_Vehicle,FCollisionShape::MakeBox(FVector(180,75,45)),Q));
   const auto* Coord=F.W->GetSubsystem<UPoliceSearchCoordinator>(); const FVector A=Coord->GetAssignment(1,800,EPoliceSearchPattern::SearchArea),B=Coord->GetAssignment(2,800,EPoliceSearchPattern::Perimeter);
   Test->TestTrue(TEXT("Multiple units receive distinct search assignments"),FVector::Dist2D(A,B)>100);
   Test->TestTrue(TEXT("Dispatch officer and car resource budget respected"),Dispatch->GetActiveOfficers()+Blocks->GetCount()<=4 && Dispatch->GetActiveVehicles()+Blocks->GetCount()<=2);
   for(auto V:Dispatch->GetUnits()->Vehicles) if(IsValid(V) && V->IsResponding()) { Disabled=V; break; }
   if(!Test->TestNotNull(TEXT("Roadblock also has a separate response vehicle"),Disabled)) return true;
   Disabled->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(8); Origin=Disabled->GetActorLocation(); Before=Dispatch->GetUnits()->Vehicles.Num(); Phase=Now; Stage=2;
   F.Shot(TEXT("D-Roadblock.png"),P+FVector(-700,-700,450),P); return false;
  }
  if(Stage==2 && Now-Phase>5)
  {
   Test->TestTrue(TEXT("Disabled police car stops without teleporting"),FVector::Dist(Origin,Disabled->GetActorLocation())<1 && Disabled->GetVelocity().IsNearlyZero());
   Test->AddInfo(FString::Printf(TEXT("Replacement: before=%d after=%d active=%d blocks=%d level=%d"),Before,Dispatch->GetUnits()->Vehicles.Num(),Dispatch->GetActiveVehicles(),Blocks->GetCount(),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel()));
   Test->TestTrue(TEXT("Budgeted replacement dispatched, wreck retained"),Dispatch->GetUnits()->Vehicles.Num()>Before && !Disabled->IsActorBeingDestroyed() && Dispatch->GetActiveVehicles()+Blocks->GetCount()<=2);
   BypassPoint=Blocks->GetPositions()[0];
   for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::Sedan) Sedan=*It;
   if(!Test->TestNotNull(TEXT("Existing sedan available for physical roadblock bypass"),Sedan)) return true;
   Sedan->GetDrivingComponent()->SetDrivingEnabled(false); Sedan->GetChaosMovement()->ResetVehicle(); Sedan->FindComponentByClass<UPhysicalImpactComponent>()->Suppress();
   Sedan->SetActorLocationAndRotation(FVector(BypassPoint.X-800,-BypassPoint.Y,145),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>.5f)
  {
   F.Place(Sedan->GetActorLocation()+FVector(0,220,100));
   Test->TestTrue(TEXT("Player enters bypass sedan through existing interaction"),Sedan->GetVehicleInteraction()->Enter(F.Player));
   Sedan->GetDrivingComponent()->SetDriveInput(1); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>5)
  {
   Test->TestTrue(TEXT("Player actually drives past roadblock in open lane"),Sedan->GetActorLocation().X>BypassPoint.X+250 && Sedan->FindComponentByClass<UHealthComponent>()->GetCurrentHealth()>0);
   Sedan->GetDrivingComponent()->SetDriveInput(0);
   F.Shot(TEXT("D-Roadblock-Bypass.png"),Sedan->GetActorLocation()+FVector(-650,-650,400),Sedan->GetActorLocation());
   F.W->GetSubsystem<UWantedSubsystem>()->ResetForLoad();
   Test->TestTrue(TEXT("Load reset removes temporary police and roadblocks"),Dispatch->GetUnits()->Vehicles.IsEmpty() && Dispatch->GetUnits()->Officers.Num()<=1 && Blocks->GetCount()==0 && F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel()==0);
   Camera->Destroy(); Victim->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; bool bPositioned=false; int32 Stage=0,Before=0; float Phase=0; ASecurityCamera* Camera=nullptr; ANeonCityNPCCharacter* Victim=nullptr; APoliceVehicle* Disabled=nullptr; ANeonCityBaseVehicle* Sedan=nullptr; FVector Origin,BypassPoint;
};
class FWitnessVehicle : public IAutomationLatentCommand
{
public:
 explicit FWitnessVehicle(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>25) { Test->AddError(TEXT("Witness vehicle timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Isolate();
   for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::Sedan) Car=*It;
   Car->GetDrivingComponent()->SetDrivingEnabled(false); Car->GetChaosMovement()->ResetVehicle(); Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress();
   Car->SetActorLocationAndRotation(FVector(-3600,0,145),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Witness=F.NPC(FVector(-4100,-400,100)); Prop=F.Prop(FVector(-3400,-450,162)); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>.5f)
  {
   F.Place(Car->GetActorLocation()+FVector(0,-220,100)); Id=F.Damage(Prop); Original=F.Player->GetActorLocation();
   Test->TestEqual(TEXT("No instant police knowledge before civilian call"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),0);
   Test->TestTrue(TEXT("Witness sees entry before submitting original call"),Car->GetVehicleInteraction()->Enter(F.Player)); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>2.5f)
  {
   const auto* R=F.W->GetSubsystem<UCrimeSubsystem>()->GetReports().FindByPredicate([&](const auto& V){return V.CrimeId==Id && V.Witness.Get()==Witness;});
   Test->TestTrue(TEXT("Original civilian crime snapshot remains historical"),R && R->Observation.LastKnownLocation.Equals(Original));
   const auto& D=F.W->GetSubsystem<USearchSystem>()->GetDescriptor();
   Test->TestTrue(TEXT("Independent witness supplies newer observed vehicle transition"),D.Source==EPoliceEvidenceSource::Witness && D.InVehicle && D.Vehicle.VehicleID==Car->VehicleID);
   Car->GetVehicleInteraction()->RestoreDriver(true); Witness->Destroy(); Prop->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0; float Phase=0; FGuid Id; FVector Original;
 ANeonCityNPCCharacter* Witness=nullptr; ANeonCityBaseVehicle* Car=nullptr; ANeonCityPhysicsProp* Prop=nullptr;
};
class FHighTier : public IAutomationLatentCommand
{
public:
 explicit FHighTier(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>75) { Test->AddError(TEXT("High-tier response/decay timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Place(FVector(-5000,0,100)); F.PC->SetControlRotation(FRotator(0,180,0));
   Camera=F.W->SpawnActor<ASecurityCamera>(FVector(-5500,0,250),FRotator(-12,0,0)); Victim=F.NPC(FVector(-4800,0,100)); Victim->GetController()->SetActorTickEnabled(false);
   Victim->GetHealthComponent()->ApplyPersistenceHealth(1000,1000);
   auto* VehicleVictim=F.W->SpawnActor<APoliceVehicle>(FVector(-4800,200,100),FRotator::ZeroRotator);
   F.W->GetSubsystem<UThreatSubsystem>()->EmitGunshot(F.Player,F.PC,F.Player->GetActorLocation());
   const int32 NoiseScore=F.W->GetSubsystem<UWantedSubsystem>()->GetEvidenceScore();
   F.Damage(VehicleVictim,12,ENeonCityDamageType::Impact);
   Test->TestEqual(TEXT("A recent gunshot cannot suppress a serious police vehicle attack"),F.W->GetSubsystem<UWantedSubsystem>()->GetEvidenceScore(),NoiseScore+6);
   VehicleVictim->Destroy();
   for(int32 I=0;I<5;++I) F.Damage(Victim,12,ENeonCityDamageType::Impact);
   Test->TestEqual(TEXT("Repeated observed serious assaults reach Level 5"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),5);
   Stage=1; Phase=F.W->GetTimeSeconds(); return false;
  }
  auto* Wanted=F.W->GetSubsystem<UWantedSubsystem>(); auto* D=F.W->GetSubsystem<UPoliceDispatchSubsystem>(); auto* B=F.W->GetSubsystem<URoadblockManager>();
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>12)
  {
   Test->TestTrue(TEXT("High-tier dispatcher deploys multiple ordinary vehicles"),D->GetActiveVehicles()+B->GetCount()>=3);
   Test->TestEqual(TEXT("High-tier can deploy two spaced roadblocks"),B->GetCount(),2);
   Test->TestTrue(TEXT("Level 5 remains within six officers and four vehicles"),D->GetActiveOfficers()+B->GetCount()<=6 && D->GetActiveVehicles()+B->GetCount()<=4);
   Camera->Destroy(); F.Place(FVector(-4400,3400,100)); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Wanted->GetWantedLevel()==4) { Phase=Now; Stage=3; return false; }
  if(Stage==3 && Now-Phase>.6f)
  {
   Test->TestEqual(TEXT("Decay removes excess Level 5 roadblock"),B->GetCount(),1);
   Test->TestTrue(TEXT("Lower-tier officer/car budgets enforced after decay"),D->GetActiveOfficers()+B->GetCount()<=5 && D->GetActiveVehicles()+B->GetCount()<=3);
   Test->TestFalse(TEXT("Building concealment prevents exact high-tier tracking"),F.W->GetSubsystem<USearchSystem>()->HasVisualContact());
   Wanted->ResetForLoad(); Victim->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0; float Phase=0;
 ASecurityCamera* Camera=nullptr; ANeonCityNPCCharacter* Victim=nullptr;
};
class FRecovery : public IAutomationLatentCommand
{
public:
 explicit FRecovery(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Road recovery timeout")); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Isolate(); F.Place(FVector(3000,3400,100));
   Area=F.W->SpawnActor<APoliceTestArea>(); int32 Cameras=0;
   for(TActorIterator<ASecurityCamera> It(F.W);It;++It) ++Cameras;
   Test->TestEqual(TEXT("Opt-in fixture adds only two local cameras"),Cameras,2);
   ANeonCityTrafficRoute* Road=nullptr; for(TActorIterator<ANeonCityTrafficRoute> It(F.W);It;++It) Road=*It;
   auto* S=Road->GetRoadSpline(); const float D=600;
   const FVector P=S->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
   Car=F.W->SpawnActor<APoliceVehicle>(P,S->GetRotationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World)); Car->InitializeRoute(Road,D);
   Origin=P; Wall=F.Wall(P+FVector(400,0,0),FVector(60,150,150)); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1)
  {
   BackedUp|=Car->GetActorLocation().X<Origin.X-30;
   Test->TestTrue(TEXT("Recovery never teleports across road obstruction"),Car->GetActorLocation().X<Origin.X+170);
   if(Now-Phase<5) return false;
   Test->TestTrue(TEXT("Blocked vehicle performs bounded swept reverse"),BackedUp);
   Wall->Destroy(); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>4)
  {
   Test->TestTrue(TEXT("Clear road permits continuous forward recovery"),Car->GetActorLocation().X>Origin.X+150 && Car->GetVelocity().Size2D()>20);
   Area->Destroy(); int32 Cameras=0; for(TActorIterator<ASecurityCamera> It(F.W);It;++It) if(!It->IsActorBeingDestroyed()) ++Cameras;
   Test->TestEqual(TEXT("Test area cleans up its cameras"),Cameras,0); Car->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0; float Phase=0; bool BackedUp=false; FVector Origin;
 APoliceTestArea* Area=nullptr; APoliceVehicle* Car=nullptr; AActor* Wall=nullptr;
};
class FCombatDecay : public IAutomationLatentCommand
{
public:
 explicit FCombatDecay(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds(); if(FPlatformTime::Seconds()-Started>115) { Test->AddError(FString::Printf(TEXT("Police combat/decay timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.Isolate(); F.Place(FVector(-6100,1750,94));
   Officer=F.W->SpawnActor<APoliceCharacter>(FVector(-6000,3000,94),FRotator(0,-90,0)); Officer->GetController()->SetActorTickEnabled(false);
   Profile=NewObject<UCombatAIData>(Officer); Profile->Role=ENeonCityCombatRole::Security; Profile->CanFight=true; Profile->Faction=ENeonCityFaction::Police; Profile->RequiresArmedThreat=true; Profile->AllowChase=false; Profile->Accuracy=0; Profile->AimError=18;
   Officer->GetCombatAI()->Configure(Profile,166); F.Player->GetHealthComponent()->ApplyPersistenceHealth(5000,5000);
   F.W->GetSubsystem<UThreatSubsystem>()->EmitNearbyDanger(F.Player,F.PC,F.Player->GetActorLocation());
   Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds(); auto* AI=Officer->GetCombatAI(); auto* Wanted=F.W->GetSubsystem<UWantedSubsystem>();
  // Controller is deliberately fixed for a repeatable firing lane; shared combat decisions/weapon timers remain live.
  AI->UpdateLocomotion(.02f);
  if(Stage==1 && Now-Phase>1.5f)
  {
   Test->TestTrue(TEXT("Ordinary unarmed danger never grants police combat authority"),AI->GetShotsFired()==0 && Officer->GetUseOfForce()!=EPoliceForceState::Combat);
   F.Player->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),F.Player->GetMesh(),TEXT("hand_r"));
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation(); R.Direction=(Officer->GetActorLocation()+FVector(0,0,35)-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("Player actually fires existing pistol at police"),F.Player->GetCombatComponent()->Attack(R));
   Test->TestTrue(TEXT("Actual armed police attack is weighted more heavily"),Wanted->GetEvidenceScore()>=6);
   Phase=Now; Stage=2; return false;
  }
  if(Stage==2)
  {
   if(AI->GetReloadCount()==0 && Now-Phase<25) return false;
   Test->TestTrue(TEXT("Police fires with shared ammo and must reload"),AI->GetShotsFired()>=12 && AI->GetReloadCount()>0);
   Test->TestTrue(TEXT("Police force state distinguishes combat"),Officer->GetUseOfForce()==EPoliceForceState::Combat);
   auto* Ally=F.W->SpawnActor<APoliceCharacter>(FVector(-7000,3300,94),FRotator::ZeroRotator);
   Test->TestTrue(TEXT("Shared faction prevents police targeting other officers"),UCombatTargetComponent::AreFriendly(Officer,Ally)); Ally->Destroy();
   F.Shot(TEXT("E-Police-Combat.png"),FVector(-6300,2250,320),Officer->GetActorLocation());
   Shots=AI->GetShotsFired(); Health=F.Player->GetHealthComponent()->GetCurrentHealth(); F.Place(FVector(-4400,3300,94)); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>2)
  {
   Test->TestEqual(TEXT("Existing cafe blocks police shots"),AI->GetShotsFired(),Shots);
   Test->TestEqual(TEXT("No through-building player damage"),F.Player->GetHealthComponent()->GetCurrentHealth(),Health);
   Test->TestFalse(TEXT("Combat LOS is false behind solid cafe"),AI->GetPerception().HasVisualContact);
   Previous=Wanted->GetWantedLevel(); Visited=1u<<Previous; LastKnown=F.W->GetSubsystem<USearchSystem>()->GetLastKnownLocation(); Stage=4; return false;
  }
  if(Stage==4)
  {
   const int32 Current=Wanted->GetWantedLevel(); if(Current!=Previous) { Test->TestEqual(TEXT("Wanted decays one level per step"),Current,Previous-1); Previous=Current; Visited|=1u<<Current; }
   if(Current>0) { Test->TestTrue(TEXT("Hidden search never reads changing player position"),F.W->GetSubsystem<USearchSystem>()->GetLastKnownLocation().Equals(LastKnown)); return false; }
   Test->TestTrue(TEXT("Searching eventually passes level 1 and reaches zero"),(Visited&3)==3);
   Test->TestFalse(TEXT("Escape clears pursuit evidence"),F.W->GetSubsystem<USearchSystem>()->HasLocation()); F.Shot(TEXT("F-Escaped.png"),FVector(-4800,3400,300),F.Player->GetActorLocation()); Stage=5; Phase=Now; return false;
  }
  if(Stage==5 && Now-Phase>.5f) { Officer->Destroy(); return true; }
  return false;
 }
 FAutomationTestBase* Test; FScene F; double Started=0; int32 Stage=0,Shots=0,Previous=0; uint32 Visited=0; float Phase=0,Health=0; FVector LastKnown; APoliceCharacter* Officer=nullptr; UCombatAIData* Profile=nullptr;
};
}
#define POLICE_V2_TEST(Class,Name,Command) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString& P) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityPoliceV2Tests::Command>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
POLICE_V2_TEST(FPoliceV2Witness,"NeonCity.PoliceV2.WitnessPIE",FWitness)
POLICE_V2_TEST(FPoliceV2Camera,"NeonCity.PoliceV2.CameraPIE",FCamera)
POLICE_V2_TEST(FPoliceV2Switch,"NeonCity.PoliceV2.VehicleSwitchPIE",FSwitch)
POLICE_V2_TEST(FPoliceV2Dispatch,"NeonCity.PoliceV2.DispatchRoadblockPIE",FDispatch)
POLICE_V2_TEST(FPoliceV2Combat,"NeonCity.PoliceV2.CombatDecayPIE",FCombatDecay)
POLICE_V2_TEST(FPoliceV2Recovery,"NeonCity.PoliceV2.ChaseRecoveryPIE",FRecovery)
POLICE_V2_TEST(FPoliceV2HighTier,"NeonCity.PoliceV2.HighTierBudgetPIE",FHighTier)
POLICE_V2_TEST(FPoliceV2WitnessVehicle,"NeonCity.PoliceV2.WitnessVehiclePIE",FWitnessVehicle)
#undef POLICE_V2_TEST
#endif
