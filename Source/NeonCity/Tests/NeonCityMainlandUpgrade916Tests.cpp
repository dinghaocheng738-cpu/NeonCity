#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NPCThreatComponent.h"
#include "Combat/ThreatSubsystem.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/GarbageCollection.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Vehicles/NeonCityStarterSedan.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandOffroadVehicle.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Interaction/InteractionComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "InputKeyEventArgs.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace Mainland916
{
// One disclosed observer fixture at the new promenade. No NPCs are spawned by
// this test: population and movement must come from the actual authored map.
class FPopulation : public IAutomationLatentCommand
{
public:
 explicit FPopulation(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>55) return End(TEXT("Population startup timeout"));
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<5) return false;
  if(!Prepared)
  {
   Player=P;Original=P->GetActorTransform();
   P->SetActorLocation(FVector(-27000,-2100,180),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();
   Prepared=true;At=W->GetTimeSeconds();return false;
  }
  int32 Nearby=0,Animated=0,Moving=0;
  for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I)
  {
   if(I->IsHidden() || FVector::DistSquared2D(I->GetActorLocation(),P->GetActorLocation())>FMath::Square(15000.f)) continue;
   ++Nearby;Animated+=Cast<UCharacterPresentationAnimInstance>(I->GetMesh()->GetAnimInstance())!=nullptr;
   auto& Start=Origins.FindOrAdd(*I,I->GetActorLocation());
   Moving+=FVector::DistSquared2D(Start,I->GetActorLocation())>FMath::Square(80.f);
  }
  if(W->GetTimeSeconds()-At<12) return false;
  Test->TestTrue(TEXT("At least four actual nearby pedestrians in the new mainland"),Nearby>=4);
  Test->TestTrue(TEXT("Nearby pedestrians use the shared animation graph"),Animated>=4);
  Test->TestTrue(TEXT("At least two pedestrians actually traverse authored routes"),Moving>=2);
  return End(TEXT(""));
 }
private:
 bool End(const FString& Error)
 {
  if(!Error.IsEmpty()) Test->AddError(Error);
  if(Player.IsValid()) Player->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);
  return true;
 }
 FAutomationTestBase* Test;TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
 TMap<TWeakObjectPtr<ANeonCityNPCCharacter>,FVector> Origins;
 FTransform Original;double Started=0;float At=0;bool Prepared=false;
};

class FPopulationLifecycle : public IAutomationLatentCommand
{
public:
 explicit FPopulationLifecycle(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>70) return End(TEXT("Population lifecycle timeout"));
  W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<5) return false;
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Player=P;Original=P->GetActorTransform();MoveObserver(FVector(-31000,-1200,160));return Next(1);
  }
  if(Stage==1 && Age>6)
  {
   FCollisionQueryParams Q(SCENE_QUERY_STAT(MainlandPedestrianRoutes),false);
   for(TActorIterator<APawn> I(W);I;++I) Q.AddIgnoredActor(*I);
   int32 Routes=0,ExpansionRoutes=0,Probes=0,Failures=0;
   for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)
   {
    const bool Expanded=I->PedestrianId.ToString().StartsWith(TEXT("Mainland916.Expansion.NPC."));
    if(!I->ActorHasTag(TEXT("NeonCity.Mainland916.Population")) && !Expanded) continue;
    ++Routes;ExpansionRoutes+=Expanded;
    for(int32 Segment=1;Segment<I->LocalRoute.Num();++Segment)
    {
     const FVector A=I->GetActorTransform().TransformPosition(I->LocalRoute[Segment-1]);
     const FVector B=I->GetActorTransform().TransformPosition(I->LocalRoute[Segment]);
     const int32 Steps=FMath::CeilToInt(FVector::Dist2D(A,B)/50.f);
     FVector PreviousCentre;bool PreviousValid=false;
     for(int32 S=0;S<=Steps;++S)
     {
      const FVector Point=FMath::Lerp(A,B,float(S)/FMath::Max(Steps,1));FHitResult H;
      if(!W->LineTraceSingleByChannel(H,Point+FVector(0,0,30),Point-FVector(0,0,180),ECC_Pawn,Q) || H.ImpactNormal.Z<.95)
       return End(FString::Printf(TEXT("Pedestrian route lacks walkable support: %s %s"),*I->GetName(),*Point.ToString()));
      const FVector Centre(Point.X,Point.Y,H.ImpactPoint.Z+92);
      TArray<FOverlapResult> Blocks;
      W->OverlapMultiByChannel(Blocks,Centre,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q);
      FString BlockedBy;
      for(const auto& Block:Blocks) if(Block.bBlockingHit) BlockedBy+=GetNameSafe(Block.GetActor())+TEXT("/")+GetNameSafe(Block.GetComponent())+TEXT(" ");
      FHitResult Sweep;
      if(PreviousValid && W->SweepSingleByChannel(Sweep,PreviousCentre,Centre,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q)) BlockedBy+=TEXT("sweep:")+GetNameSafe(Sweep.GetActor());
      if(!BlockedBy.IsEmpty())
      {
       if(++Failures<=20) Test->AddError(FString::Printf(TEXT("Route %s at %s support %s blocked by %s"),*I->GetName(),*Centre.ToString(),*GetNameSafe(H.GetActor()),*BlockedBy));
      }
      PreviousCentre=Centre;PreviousValid=true;
      ++Probes;
     }
    }
   }
   if(Failures) return End(FString::Printf(TEXT("Authored pedestrian routes have %d blocked samples; fix-specific sweep evidence retained"),Failures));
   Test->TestTrue(TEXT("All old and expanded route markers receive physical support/clearance checks"),Routes>=69 && ExpansionRoutes==37 && Probes>1300);
   for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I)
    if(I->GetActorLocation().X<-28000 && !I->IsPopulationDormant() && I->CanSuspendPopulation()) {Retained=*I;break;}
   if(!Retained.IsValid()) return End(TEXT("No ordinary west-promenade pedestrian for retention test"));
   Id=Retained->GetNPCId_Implementation();Route=Retained->GetRoute();
   ActorTick=Retained->IsActorTickEnabled();MovementTick=Retained->GetCharacterMovement()->IsComponentTickEnabled();
   Collision=Retained->GetActorEnableCollision();
   MoveObserver(FVector(-5000,1200,140));return Next(2);
  }
  if(Stage==2 && Age>8)
  {
   if(!Retained.IsValid() || !Retained->IsPopulationDormant()) return End(TEXT("Distant ambient character failed to suspend without destruction"));
   Test->TestFalse(TEXT("Dormant actor tick off"),Retained->IsActorTickEnabled());
   Test->TestFalse(TEXT("Dormant movement tick off"),Retained->GetCharacterMovement()->IsComponentTickEnabled());
   Test->TestFalse(TEXT("Dormant controller tick off"),Retained->GetController()->IsActorTickEnabled());
   Test->TestFalse(TEXT("Dormant collision off"),Retained->GetActorEnableCollision());
   // A global event still reaches delegates with ticks disabled. Dormant
   // witnesses must ignore it, not manufacture an unseen report on waking.
   FNeonCityThreatEvent Event;Event.Id=FGuid::NewGuid();Event.Type=ENeonCityThreatType::Gunshot;
   Event.Location=Retained->GetActorLocation();Event.Radius=2000;Event.Victim=Retained.Get();
   W->GetSubsystem<UThreatSubsystem>()->OnThreat.Broadcast(Event);
   Test->TestFalse(TEXT("Dormant perception ignores global threat events"),Retained->GetThreatComponent()->HasPerceived(Event.Id));
   MoveObserver(FVector(-31000,-1200,160));return Next(3);
  }
  if(Stage==3 && Age>6)
  {
   if(!Retained.IsValid() || Retained->IsPopulationDormant()) return End(TEXT("Returning observer did not wake the same character"));
   Test->TestEqual(TEXT("Same stable NPC identity"),Retained->GetNPCId_Implementation(),Id);
   Test->TestTrue(TEXT("Route and character state retained"),Retained->GetRoute()==Route);
   Test->TestEqual(TEXT("Actor tick restored exactly"),Retained->IsActorTickEnabled(),ActorTick);
   Test->TestEqual(TEXT("Movement tick restored exactly"),Retained->GetCharacterMovement()->IsComponentTickEnabled(),MovementTick);
   Test->TestEqual(TEXT("Collision restored exactly"),Retained->GetActorEnableCollision(),Collision);
   int32 Active=0;for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I) Active+=!I->IsPopulationDormant();
   Test->TestTrue(TEXT("Ordinary ambient population remains bounded"),Active<=20);
   return End(TEXT(""));
  }
  return false;
 }
private:
 void MoveObserver(FVector Position){Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();}
 bool Next(int32 Value){Stage=Value;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error){if(!Error.IsEmpty())Test->AddError(Error);if(Player.IsValid())Player->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
 TWeakObjectPtr<ANeonCityNPCCharacter> Retained;FName Id;TArray<FVector> Route;FTransform Original;
 double Started=0;float At=0;int32 Stage=0;bool ActorTick=false,MovementTick=false,Collision=false;
};

class FMaterialLifetime : public IAutomationLatentCommand
{
public:
 explicit FMaterialLifetime(FAutomationTestBase* InTest,bool InIncremental=false):Test(InTest),Incremental(InIncremental){}
 bool Update() override
 {
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<5) return false;
  if(!Actor.IsValid())
  {
   auto* A=W->SpawnActorDeferred<AActor>(AActor::StaticClass(),FTransform(FVector(0,0,-100000)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   Actor=A;auto* Mesh=NewObject<UStaticMeshComponent>(A);A->SetRootComponent(Mesh);A->AddInstanceComponent(Mesh);
   Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Road/M_RoadAsphalt"));
   if(!Base){Test->AddError(TEXT("Material lifetime fixture missing native base"));A->Destroy();return true;}
   auto* Dynamic=UMaterialInstanceDynamic::Create(Base,A);Original=Dynamic;Mesh->SetMaterial(0,Dynamic);Mesh->RegisterComponent();
   auto* State=NewObject<UDestructibleStateComponent>(A);A->AddInstanceComponent(State);State->BrokenMaterial=Base;State->RegisterComponent();
   Surface=Mesh;Destruction=State;A->FinishSpawning(FTransform(FVector(0,0,-100000)));
   Test->TestTrue(TEXT("Real surface break replaces the dynamic intact material"),State->Break(A->GetActorLocation()) && Mesh->GetMaterial(0)==Base);
   return false;
  }
  if(Incremental)
  {
   if(!CollectionStarted){CollectionStarted=true;At=FPlatformTime::Seconds();CollectGarbage(RF_NoFlags,false);return false;}
   // Read and restore the real owning cache while a delayed collector spans
   // frames. Full-purge collection would not exercise incremental barriers.
   if(IsIncrementalReachabilityAnalysisPending())
   {SawPending=true;if(!ResetDuringCollection){ResetDuringCollection=Destruction->ResetDestruction();Test->TestTrue(TEXT("Real material assignment during pending reachability"),ResetDuringCollection);}}
   if(FPlatformTime::Seconds()-At<2)return false;
   Test->TestTrue(TEXT("Actual reachability analysis spanned frames"),SawPending);
  }
  else CollectGarbage(RF_NoFlags,true);
  Test->TestTrue(TEXT("Intact material cache owns its dynamic material across collection"),Original.IsValid());
  Test->TestTrue(TEXT("Real destruction reset restores the exact material"),Destruction.IsValid() && (ResetDuringCollection || Destruction->ResetDestruction()) && Surface->GetMaterial(0)==Original.Get());
  Actor->Destroy();return true;
 }
private:
 FAutomationTestBase* Test;bool Incremental=false,CollectionStarted=false,SawPending=false,ResetDuringCollection=false;double At=0;TWeakObjectPtr<AActor> Actor;TWeakObjectPtr<UStaticMeshComponent> Surface;
 TWeakObjectPtr<UDestructibleStateComponent> Destruction;TWeakObjectPtr<UMaterialInstanceDynamic> Original;
};

class FPopulationSafety : public IAutomationLatentCommand
{
public:
 explicit FPopulationSafety(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<5)return false;
  // Isolated temporary fixture away from native land. Nothing is persisted.
  auto* N=W->SpawnActor<ANeonCityNPCCharacter>(FVector(0,0,100000),FRotator::ZeroRotator);
  if(!N){Test->AddError(TEXT("Population safety fixture spawn failed"));return true;}
  auto* Block=W->SpawnActor<AActor>();auto* Box=NewObject<UStaticMeshComponent>(Block);
  Block->SetRootComponent(Box);Block->AddInstanceComponent(Box);
  Box->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Box->SetCollisionProfileName(TEXT("BlockAll"));Box->SetWorldScale3D(FVector(2,2,3));Box->RegisterComponent();
  Block->SetActorLocation(N->GetActorLocation());
  N->SetPopulationDormant(true);N->SetPopulationDormant(false);
  Test->TestTrue(TEXT("A vehicle-sized blocking volume prevents dormant NPC penetration on waking"),N->IsPopulationDormant());
  Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);N->SetPopulationDormant(false);
  Test->TestFalse(TEXT("Same character wakes after clearance becomes available"),N->IsPopulationDormant());
  auto* Body=N->FindComponentByClass<UDamageableCharacterComponent>();Body->SetPhysicalLock(true);
  Test->TestFalse(TEXT("Incapacitated body does not reserve an ambient behaviour slot"),N->UsesPopulationBehaviourBudget());
  Test->TestFalse(TEXT("Incapacitated body remains protected from ambient dormancy"),N->CanSuspendPopulation());
  Body->SetPhysicalLock(false);
  auto* Marker=W->SpawnActorDeferred<APedestrianSpawnPoint>(APedestrianSpawnPoint::StaticClass(),FTransform(FVector(0,0,100000)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Marker->bDistanceManaged=true;Marker->LocalRoute={FVector::ZeroVector,FVector(200,0,0)};
  Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Marker->FinishSpawning(FTransform(FVector(0,0,100000)));
  Test->TestFalse(TEXT("Blocked real spawn fails safely"),Marker->EnsurePedestrian());
  Test->TestFalse(TEXT("Failed spawn backs off to allow other markers to progress"),Marker->IsSpawnAttemptReady());
  Marker->Destroy();Block->Destroy();N->Destroy();return true;
 }
private:FAutomationTestBase* Test;
};

class FFleetDrive : public IAutomationLatentCommand
{
public:
 explicit FFleetDrive(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  if(!Wall)Wall=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Wall>85)return End(TEXT("Fitted fleet drive timeout"));
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<5)return false;const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return End(TEXT("Player required"));
   OriginalPlayer=Player->GetActorTransform();int32 Count=0;
   for(TActorIterator<ANeonCityStarterSedan> I(W);I;++I)
   {
    if(I->IsA<AIslandSupercar>() || I->IsA<AIslandOffroadVehicle>())continue;
    ++Count;auto* Body=FindObject<UStaticMeshComponent>(*I,TEXT("SedanHeroBody916"));
    if(!Body || !Body->GetStaticMesh() || !Body->GetStaticMesh()->GetName().StartsWith(TEXT("SM_GrandTourer916_")))return End(TEXT("Old fleet vehicle did not adopt complete native luxury body: ")+I->GetName());
    Test->TestTrue(TEXT("Luxury body uses four built native LODs"),Body->GetStaticMesh()->GetNumLODs()==4);
    Test->TestTrue(TEXT("Single fitted chassis owns collision"),Body->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    const auto* Movement=I->GetChaosMovement();const auto* Profile=I->GetVehicleProfile();
    const float Radius=Movement->Wheels[0]->WheelRadius/100.f;
    const float Reduction=Movement->TransmissionSetup.ForwardGearRatios[0]*Movement->TransmissionSetup.FinalRatio*Movement->TransmissionSetup.TransmissionEfficiency;
    Test->TestTrue(TEXT("Actual fitted wheel radius preserves configured drive force"),FMath::IsNearlyEqual(Movement->EngineSetup.MaxTorque*Reduction/Radius,Movement->Mass*Profile->Acceleration,1.f));
    FHitResult Hood;FCollisionQueryParams Query(SCENE_QUERY_STAT(FleetHood916),true);const FVector HoodXY=I->GetMesh()->GetComponentTransform().TransformPosition(FVector(165,0,0));
    // Inspect the physical chassis directly; the F prompt's visibility-only
    // interaction box intentionally lies above it and is not a floor.
    if(I->GetMesh()->LineTraceComponent(Hood,HoodXY+FVector(0,0,200),HoodXY-FVector(0,0,20),Query))
    {FDynamicStandingSurface Surface;Test->TestTrue(TEXT("Measured hood collision retains standing surface"),I->QueryStandingSurface_Implementation(Hood,Player,Surface));}
    else Test->AddError(TEXT("Fitted hood support missing"));
    for(int32 K=0;K<4;++K)
    {
     const auto* Wheel=FindObject<UStaticMeshComponent>(*I,*FString::Printf(TEXT("SedanWheel%d"),K));
     if(!Wheel || !Wheel->GetStaticMesh()->GetName().StartsWith(TEXT("SM_GrandTourer916_Wheel")))return End(TEXT("Legacy wheel styling overwrote fitted GT wheels"));
    }
    if(I->GetClass()==ANeonCityStarterSedan::StaticClass())Car=*I;
   }
   Test->TestTrue(TEXT("All four old mainland fleet roles adopt luxury presentation"),Count>=4);
   if(!Car)return End(TEXT("Original owned starter identity missing"));
   OriginalCar=Car->GetActorTransform();Initial=Car->GetDamageState()->GetConditionFraction();
   // Disclosed independent fit fixture, before normal F and Chaos driving.
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Car->SetActorLocationAndRotation(FVector(-9000,-180,85),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
   Player->SetActorLocation(FVector(-9000,10,130),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
   return Next(1);
  }
  if(Stage==1 && Age>2)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Fleet entry fixture lacks ground"));
   FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);bool Focused=false;
   for(float Z:{0.f,35.f,65.f})
   {
    PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,Z)-Eye).Rotation());
    if(Player->GetInteractionComponent()->RefreshFocusedInteractable()==Car){Focused=true;break;}
   }
   if(!Focused)return End(TEXT("Normal range F cannot focus upgraded original vehicle"));
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))Test->TestTrue(TEXT("Actual normal player luxury entry evidence"),FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("Fleet916NormalEntry.jpg")));
#endif
   Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Car)return End(TEXT("Original vehicle normal F entry failed"));
   Start=Car->GetActorLocation();return Next(3);
  }
  if(Stage==3)
  {
   const FVector P=Car->GetActorLocation();auto* D=Car->GetDrivingComponent();Peak=FMath::Max(Peak,D->GetSpeedKmh());
   if(P.Z<0 || P.Z>180 || FMath::Abs(P.Y+180)>180)return End(TEXT("Luxury fit lost road support or lane stability"));
   const FVector Goal(P.X-800,-180,P.Z);const float Error=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,(Goal-P).Rotation().Yaw);
   D->SetSteerInput(FMath::Clamp(Error/32.f,-.4f,.4f));D->SetDriveInput(D->GetSpeedKmh()<25?.5f:.04f);D->SetBrakeInput(0);
   if(Age>14 && FVector::Dist2D(P,Start)<1200)return End(TEXT("Luxury fit does not accelerate through native Chaos movement"));
   if(FVector::Dist2D(P,Start)>4500){D->SetDriveInput(0);D->SetSteerInput(0);D->SetBrakeInput(1);return Next(4);}
  }
  if(Stage==4 && Car->GetDrivingComponent()->GetSpeedKmh()<.5)
  {
   Test->TestTrue(TEXT("Upgraded vehicle reaches ordinary road speed"),Peak>15);
   Test->TestTrue(TEXT("Fitted chassis drive causes no collision damage"),Car->GetDamageState()->GetConditionFraction()>=Initial-.001f);
   Key(EKeys::F,true);return Next(5);
  }
  if(Stage==5 && Age>.3){Key(EKeys::F,false);if(PC->GetPawn()!=Player)return End(TEXT("Normal luxury vehicle F exit failed"));return Next(6);}
  if(Stage==6 && Age>1)
  {
   Test->TestTrue(TEXT("Upgraded vehicle safe exit returns to supported walking"),Player->GetCharacterMovement()->IsMovingOnGround());
   return End(TEXT(""));
  }
  return false;
 }
private:
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error)
 {
  Key(EKeys::F,false);if(!Error.IsEmpty())Test->AddError(Error);
  if(Car){Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);}
  if(Car && PC && PC->GetPawn()==Car)Car->GetVehicleInteraction()->RestoreDriver(true);
  if(Car){Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Car->SetActorTransform(OriginalCar,false,nullptr,ETeleportType::TeleportPhysics);}
  if(Player)Player->SetActorTransform(OriginalPlayer,false,nullptr,ETeleportType::TeleportPhysics);
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityStarterSedan* Car=nullptr;
 FTransform OriginalCar,OriginalPlayer;FVector Start;double Wall=0;float At=0,Peak=0,Initial=0;int32 Stage=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPopulation916,"NeonCity.Phase205c.Mainland916.Standalone.Population",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPopulation916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("Population fixture requires isolated persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(Mainland916::FPopulation(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPopulationLifecycle916,"NeonCity.Phase205c.Mainland916.Standalone.PopulationLifecycle",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPopulationLifecycle916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(Mainland916::FPopulationLifecycle(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandMaterialLifetime916,"NeonCity.Phase205c.Mainland916.Standalone.MaterialLifetime",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandMaterialLifetime916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(Mainland916::FMaterialLifetime(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandPopulationSafety916,"NeonCity.Phase205c.Mainland916.Standalone.PopulationSafety",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandPopulationSafety916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(Mainland916::FPopulationSafety(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandIncrementalLifetime916,"NeonCity.Phase205c.Mainland916.Standalone.IncrementalLifetime",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandIncrementalLifetime916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(Mainland916::FMaterialLifetime(this,true));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandFleetDrive916,"NeonCity.Phase205c.Mainland916.Standalone.FleetDrive",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandFleetDrive916::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;
 ADD_LATENT_AUTOMATION_COMMAND(Mainland916::FFleetDrive(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandLegacySpawn916,"NeonCity.Phase205c.Mainland916.Standalone.LegacySpawn",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandLegacySpawn916::RunTest(const FString&)
{
 UWorld* W=NeonCityValidationWorld();if(!W)return false;
 // A narrow wall overlaps the intended capsule, but permits Unreal's normal
 // adjusted spawn on its clear side. Managed markers must wait instead.
 const FVector At(0,0,-100000);auto* Block=W->SpawnActor<AActor>();
 auto* Box=NewObject<UBoxComponent>(Block);Block->SetRootComponent(Box);Block->AddInstanceComponent(Box);Box->SetBoxExtent(FVector(8,80,90));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Block->SetActorLocation(At+FVector(32,0,0));
 auto* Managed=W->SpawnActorDeferred<APedestrianSpawnPoint>(APedestrianSpawnPoint::StaticClass(),FTransform(At),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 Managed->bDistanceManaged=true;Managed->FinishSpawning(FTransform(At));
 TestFalse(TEXT("Managed sidewalk capsule waits while obstructed"),Managed->EnsurePedestrian());Managed->Destroy();
 auto* Legacy=W->SpawnActorDeferred<APedestrianSpawnPoint>(APedestrianSpawnPoint::StaticClass(),FTransform(At),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 Legacy->bDistanceManaged=false;Legacy->FinishSpawning(FTransform(At));
 TestTrue(TEXT("Unmanaged marker keeps its original adjusted spawn at BeginPlay"),IsValid(Legacy->GetSpawnedPedestrian()));
 if(auto* NPC=Legacy->GetSpawnedPedestrian())TestTrue(TEXT("Real capsule moved clear of the temporary obstruction"),FVector::DistSquared(NPC->GetActorLocation(),At)>1);
 Legacy->Destroy();Block->Destroy();return true;
}
#endif
