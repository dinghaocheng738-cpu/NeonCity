#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NeonCityNPCController.h"
#include "AI/NPCStateComponent.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace NeonCityLivingTests
{
class FVerify : public IAutomationLatentCommand
{
public:
 explicit FVerify(FAutomationTestBase* InTest) : Test(InTest) {}
 bool Update() override
 {
  if (!Started) { Started = FPlatformTime::Seconds(); }
  if (FPlatformTime::Seconds()-Started > 300) { Test->AddError(TEXT("Living city PIE timed out")); return true; }
  UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
  APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
  ANeonCityPlayerCharacter* Player = PC ? Cast<ANeonCityPlayerCharacter>(PC->GetPawn()) : nullptr;
  if (!Player || !World->HasBegunPlay()) { return false; }
  const float Now = World->GetTimeSeconds();
  if(Stage==1)
  {
   for(int32 I=0;I<NPCs.Num();++I)
    FootMotion[I]=FMath::Max(FootMotion[I],FVector::Dist(FootOrigins[I],NPCs[I]->GetMesh()->GetSocketTransform(TEXT("foot_r"),RTS_Component).GetLocation()));
  }
  if (Stage == 0)
  {
   if (!Player->GetCharacterMovement()->IsMovingOnGround()) { return false; }
   Player->SetActorLocation(FVector(-6000,1200,136));
   int32 Markers = 0, Sedans = 0;
   for (TActorIterator<APedestrianSpawnPoint> It(World); It; ++It) { ++Markers; }
   for (TActorIterator<ANeonCityNPCCharacter> It(World); It; ++It)
   {
    NPCs.Add(*It); Origins.Add(It->GetActorLocation());
    Test->TestNotNull(TEXT("NPC owns modular controller"), Cast<ANeonCityNPCController>(It->GetController()));
    Test->TestTrue(TEXT("NPC has authored route and shared animation graph"), It->GetRoute().Num() >= 2
     && It->GetMesh()->GetAnimationMode()==EAnimationMode::AnimationBlueprint
     && Cast<UCharacterPresentationAnimInstance>(It->GetMesh()->GetAnimInstance()));
    // This fixture samples offscreen bone motion as well as world movement.
    // Rendering visibility must not make the observation nondeterministic.
    It->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    FootOrigins.Add(It->GetMesh()->GetSocketTransform(TEXT("foot_r"),RTS_Component).GetLocation());FootMotion.Add(0);
   }
   for (TActorIterator<ANeonCityTrafficVehicle> It(World); It; ++It) { Cars.Add(*It); CarOrigins.Add(It->GetActorLocation()); }
   for (TActorIterator<ANeonCityStarterSedan> It(World); It; ++It) { if(It->GetClass()==ANeonCityStarterSedan::StaticClass()) { Sedan = *It; ++Sedans; } }
   Test->TestEqual(TEXT("Twelve authored spawn points"), Markers,12);
   Test->TestEqual(TEXT("Twelve pedestrians spawned"),NPCs.Num(),12);
   Test->TestEqual(TEXT("Two ambient vehicles spawned"),Cars.Num(),2);
   Test->TestEqual(TEXT("One original Chaos player sedan"),Sedans,1);
   if (NPCs.Num() != 12 || Cars.Num() != 2 || !Sedan.IsValid()) { return true; }
   SedanParking=Sedan->GetActorTransform();
   Stage = 1; Phase = Now;
  }
  else if (Stage == 1 && Now-Phase > 9)
  {
   uint8 States = 0; int32 Moved = 0;
   for (int32 I=0; I<NPCs.Num(); ++I)
   {
    States |= NPCs[I]->GetNPCState()->GetVisitedStateMask();
    Moved += FVector::Dist2D(Origins[I],NPCs[I]->GetActorLocation()) > 100;
   }
   Test->TestTrue(TEXT("Pedestrians actually walk along routes"),Moved >= 8);
   Test->TestTrue(TEXT("Walking pedestrians animate actual foot bones"),FootMotion.FilterByPredicate([](float Motion){return Motion>2.f;}).Num()>=8);
   Test->TestTrue(TEXT("Natural Idle Walk and Social transitions"),(States & 7) == 7);
   for (int32 I=0; I<Cars.Num(); ++I) { Test->TestTrue(TEXT("Traffic moves on road"),FVector::Dist2D(CarOrigins[I],Cars[I]->GetActorLocation()) > 500 && FMath::Abs(Cars[I]->GetActorLocation().Y)<600); }
   if (FParse::Param(FCommandLine::Get(),TEXT("NeonCityLivingCapture")))
   {
    ACameraActor* Camera = World->SpawnActor<ACameraActor>();
    Camera->SetActorLocation(FVector(-3800,-2350,750));
    Camera->SetActorRotation((FVector(-3000,700,100)-Camera->GetActorLocation()).Rotation());
    PC->SetViewTarget(Camera); Stage=2; Phase=Now;
   }
   else { Stage=3; }
  }
  else if (Stage == 2 && Now-Phase > 3)
  {
   IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase6")),true);
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase6/LivingCity.png"),false,false);
   Stage=3;
  }
  else if (Stage == 3)
  {
   if (FScreenshotRequest::IsScreenshotRequested()) { return false; }
   for (auto NPC : NPCs)
   {
    if (NPC->GetActorLocation().Y>700 && NPC->GetActorLocation().X>0) { Target=NPC; break; }
   }
   if (!Test->TestNotNull(TEXT("Observation target"),Target.Get())) { return true; }
   Player->SetActorLocation(Target->GetActorLocation()+FVector(0,200,4));
   Stage=4; Phase=Now;
  }
  else if (Stage == 4 && Now-Phase > 0.8f)
  {
   Test->TestTrue(TEXT("Nearby player triggers observation"),CastChecked<ANeonCityNPCController>(Target->GetController())->HasObservedPlayer());
   Player->SetActorLocation(FVector(-6000,1200,136));
   Stage=5; Phase=Now;
  }
  else if (Stage == 5 && Now-Phase > 2)
  {
   Test->TestTrue(TEXT("Observation returns to route"),(Target->GetNPCState()->GetVisitedStateMask() & (1<<4)) != 0);
   // Controlled physical threat fixture: actual Chaos sedan, no forced NPC state.
   // Keep its footprint clear of lamps and the other south-side pedestrian group.
   Target->SetActorLocation(FVector(500,-800,132));
   Target->GetCharacterMovement()->StopMovementImmediately();
   DodgeOrigin=Target->GetActorLocation();
   Sedan->SetActorLocationAndRotation(FVector(-400,-800,65),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Sedan->GetDrivingComponent()->SetDrivingEnabled(true);
   Sedan->GetMesh()->SetPhysicsLinearVelocity(FVector(1000,0,0));
   Stage=6; Phase=Now;
  }
  else if (Stage == 6)
  {
   // Sustain a reproducible approaching speed until the perception system responds.
   auto Controller=CastChecked<ANeonCityNPCController>(Target->GetController());
   if (!Controller->HasEvadedVehicle() && Now-Phase<0.6f) { Sedan->GetMesh()->SetPhysicsLinearVelocity(FVector(1000,0,0)); }
   if (Controller->HasEvadedVehicle()) { Sedan->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); Sedan->GetDrivingComponent()->SetDrivingEnabled(false); }
   if (Now-Phase < 1.3f) { return false; }
   Test->TestEqual(TEXT("Approach fixture does not crash and trigger a different threat"),Sedan->GetDamageState()->GetCollisionDamageCount(),0);
   Test->TestTrue(TEXT("Approaching fast vehicle triggers evasion"),Controller->HasEvadedVehicle());
   Test->TestTrue(TEXT("Evasion moves NPC sideways"),FMath::Abs(Target->GetActorLocation().Y-DodgeOrigin.Y)>60);
   Sedan->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
   Sedan->GetDrivingComponent()->SetDrivingEnabled(false);
   Stage=7; Phase=Now;
  }
  else if (Stage == 7 && Now-Phase>2)
  {
   Test->TestTrue(TEXT("All five NPC states exercised"),(Target->GetNPCState()->GetVisitedStateMask() & 31)==31);
   Test->TestTrue(TEXT("Evasion finishes"),Target->GetNPCState()->GetState()!=ENeonCityNPCState::React);
   // Restore the temporary threat fixture before timing an unobstructed road loop.
   // Its original north-side route must not cause an artificial cross-road return.
   // Restore the Chaos fixture too: after the evasion it may slide off the kerb into traffic.
   Sedan->SetActorTransform(SedanParking,false,nullptr,ETeleportType::TeleportPhysics);
   Sedan->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
   Sedan->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Target->SetActorLocation(Target->GetRoute()[0]);
   Target->GetCharacterMovement()->StopMovementImmediately();
   Player->SetActorLocation(Cars[0]->GetActorLocation()+Cars[0]->GetActorForwardVector()*450+FVector(0,0,15));
   Stage=8; Phase=Now;
  }
  else if (Stage == 8 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Traffic yields to player obstacle"),Cars[0]->IsYielding() && Cars[0]->GetVelocity().Size()<1);
   Player->SetActorLocation(FVector(-6000,1200,136));
   CarOrigins.Reset(); Distances.Init(0,Cars.Num());
   for (auto Car : Cars) { Car->CruiseSpeed=800; CarOrigins.Add(Car->GetActorLocation()); }
   Stage=9; Phase=Now;
  }
  else if (Stage == 9)
  {
   for (int32 I=0; I<Cars.Num(); ++I)
   {
    Distances[I]+=FVector::Dist2D(CarOrigins[I],Cars[I]->GetActorLocation());
    CarOrigins[I]=Cars[I]->GetActorLocation();
    if (FMath::Abs(CarOrigins[I].Y)>600 || FMath::Abs(CarOrigins[I].X)>6300) { Test->AddError(TEXT("Traffic leaves road bounds")); return true; }
   }
   // Ambient actors can legitimately yield at different times; verify actual traversal,
   // with a bounded deadline, rather than requiring uninterrupted full-speed travel.
   const bool Completed=Distances.ContainsByPredicate([](float Distance) { return Distance<=25000.f; })==false;
   if (!Completed && Now-Phase<50.f) { return false; }
   for (int32 I=0; I<Cars.Num(); ++I)
   {
    Test->AddInfo(FString::Printf(TEXT("Traffic %d traveled %.1f cm; position %s; blocker %s"),I,Distances[I],*Cars[I]->GetActorLocation().ToString(),*GetNameSafe(Cars[I]->GetBlockingActor())));
    Test->TestTrue(TEXT("Traffic completes road loop including both turns and resumes after obstacle"),Distances[I]>25000);
   }
   return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;
 int32 Stage=0; double Started=0; float Phase=0;
 TArray<TWeakObjectPtr<ANeonCityNPCCharacter>> NPCs;
 TArray<TWeakObjectPtr<ANeonCityTrafficVehicle>> Cars;
 TArray<FVector> Origins,CarOrigins;
 TArray<FVector> FootOrigins;
 TArray<float> FootMotion;
 TArray<float> Distances;
 TWeakObjectPtr<ANeonCityNPCCharacter> Target;
 TWeakObjectPtr<ANeonCityStarterSedan> Sedan;
 FVector DodgeOrigin;
 FTransform SedanParking;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityLivingCity,"NeonCity.AI.LivingCityPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityLivingCity::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompiling());
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityLivingTests::FVerify>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
