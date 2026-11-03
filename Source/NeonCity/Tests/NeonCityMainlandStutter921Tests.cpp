#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Water/CoastalOceanSurface.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace MainlandStutter921
{
/** Short rectangular promenade route with normal CharacterMovement. It can
 * steer around a dynamic pedestrian using real capsule sweeps; it never clears
 * collision or moves any actor by fixture. Interesting frames retain actual
 * obstacle and native ocean-node context missing from the earlier8s probe. */
class FWalk : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Began=0,Previous=0,At=0,LastContext=0,BlockedSeconds=0;
 FVector Last=FVector::ZeroVector,Initial=FVector::ZeroVector;
 double TravelCm=0,PeakIntervalMs=0;int32 Goal=0,Loops=0,Frames=0,Over50=0,BlockedObservations=0;
 FNeonCityLandFrameMetrics Metrics{TEXT("MainlandC921-WalkStutterRoute"),true};
 TArray<TSharedPtr<FJsonValue>> Events;
 bool Finish(ACharacter* Player,const FString& Error)
 {
  TRACE_BOOKMARK(TEXT("C921WalkStutterRoute end moving_interval_ms=%.4f frames=%d over50=%d loops=%d travel_cm=%.3f error=%s"),PeakIntervalMs,Frames,Over50,Loops,TravelCm,*Error);
  if(Player)Player->GetCharacterMovement()->StopMovementImmediately();
  const bool Written=Metrics.Flush();
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  Label=FPaths::MakeValidFileName(Label.IsEmpty()?TEXT("WalkStutterRoute"):Label,TEXT('_'));Label.ReplaceInline(TEXT(".."),TEXT("_"));
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/MainlandC921/Performance")/Label;
  IFileManager::Get().MakeDirectory(*Directory,true);
  auto Receipt=MakeShared<FJsonObject>();
  const bool Passed=Error.IsEmpty() && Written && Loops>=1 && TravelCm>=5000 && FPlatformTime::Seconds()-Began>=25;
  Receipt->SetBoolField(TEXT("route_collection_passed"),Passed);Receipt->SetStringField(TEXT("failure"),Error);
  Receipt->SetStringField(TEXT("test"),TEXT("NeonCity.Phase205c.MainlandC921.Standalone.WalkStutterRoute"));
  Receipt->SetStringField(TEXT("initial_authored_player_start_cm"),Initial.ToString());
  Receipt->SetStringField(TEXT("final_player_cm"),Player?Player->GetActorLocation().ToString():TEXT("absent"));
  Receipt->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Began);
  Receipt->SetNumberField(TEXT("actual_player_travel_cm"),TravelCm);Receipt->SetNumberField(TEXT("completed_loops"),Loops);
  Receipt->SetNumberField(TEXT("sampled_updates"),Frames);Receipt->SetNumberField(TEXT("intervals_over50ms"),Over50);
  Receipt->SetNumberField(TEXT("peak_update_interval_ms"),PeakIntervalMs);Receipt->SetNumberField(TEXT("blocked_observations"),BlockedObservations);
  Receipt->SetBoolField(TEXT("trace_diagnostic"),FParse::Param(FCommandLine::Get(),TEXT("trace")) || FString(FCommandLine::Get()).Contains(TEXT("-trace=")));
  Receipt->SetBoolField(TEXT("image_readback"),false);
  Receipt->SetStringField(TEXT("scope"),TEXT("Normal authored PlayerStart; rectangular promenade route with real turns and capsule-based dynamic-pedestrian avoidance. No player/vehicle pose fixture, collision change, NPC removal or save write. Every raw interval, including turns/blocked intervals, retained. Route collection is not proof that all stutters are fixed; trace and clean timing are separate."));
  Receipt->SetArrayField(TEXT("interesting_frames"),Events);
  FString Json;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
  const FString File=Directory/TEXT("receipt.json");
  const bool Saved=!IFileManager::Get().FileExists(*File) && FFileHelper::SaveStringToFile(Json,*File);
  if(!Error.IsEmpty())Test->AddError(Error);
  Test->TestTrue(TEXT("New targeted walking diagnostic retained its raw metrics and unique receipt"),Written && Saved);
  Test->TestTrue(TEXT("Real turning walking route covered at least50m and25s, without a fixture"),Passed);
  return true;
 }
public:
 explicit FWalk(FAutomationTestBase* InTest):Test(InTest){}
 bool Update()override
 {
  TRACE_CPUPROFILER_EVENT_SCOPE(C921WalkStutterUpdate);
  UWorld* World=NeonCityValidationWorld();if(!World || World->GetTimeSeconds()<8)return false;
  auto* PC=World->GetFirstPlayerController();auto* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Player)return Finish(nullptr,TEXT("Actual normal player character is not possessed"));
  const double Now=FPlatformTime::Seconds();
  if(Began==0)
  {
   Began=Previous=At=Now;Last=Initial=Player->GetActorLocation();
   if(FVector::Dist2D(Initial,FVector(-7000,-2400,0))>200)
    return Finish(Player,TEXT("Normal authored PlayerStart is required; no position reset fallback"));
   TRACE_BOOKMARK(TEXT("C921WalkStutterRoute begin game=%.6f player=%s"),World->GetTimeSeconds(),*Initial.ToString());
  }
  const double Interval=(Now-Previous)*1000.;Previous=Now;++Frames;
  PeakIntervalMs=FMath::Max(PeakIntervalMs,Interval);Over50+=Interval>50.?1:0;
  Metrics.Sample(World,FString::Printf(TEXT("turning-walk-loop%02d-leg%02d"),Loops,Goal));
  const FVector Position=Player->GetActorLocation();TravelCm+=FVector::Dist2D(Position,Last);Last=Position;
  auto* Movement=Player->GetCharacterMovement();
  if(Position.ContainsNaN() || !Player->GetActorEnableCollision() || Player->IsHidden() ||
     Player->GetAttachParentActor() || !Movement->IsMovingOnGround() || Position.Z<100 || Position.Z>300)
   return Finish(Player,TEXT("Actual walking lost its normal supported/collidable/visible state"));
  // Turn toward the arcade side: the new narrowed curved quay removes the old
  // seaward y=-2800 leg. These authored land points work before and after the
  // coast revision; normal floor/collision remains authoritative throughout.
  const FVector Goals[]={FVector(-5000,-2400,0),FVector(-5000,-2000,0),FVector(-7000,-2000,0),FVector(-7000,-2400,0)};
  const FVector Delta=Goals[Goal]-Position;FVector Input=Delta.GetSafeNormal2D();
  FCollisionQueryParams Query(SCENE_QUERY_STAT(C921ActualWalkingObstacle),false,Player);
  FHitResult Obstacle;const auto* Capsule=Player->GetCapsuleComponent();
  const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2);
  const bool Ahead=World->SweepSingleByChannel(Obstacle,Position,Position+Input*130,FQuat::Identity,ECC_Pawn,Shape,Query);
  const double DT=FMath::Clamp(Interval*.001,0.,.1);
  BlockedSeconds=Player->GetVelocity().Size2D()<15 && Delta.Size2D()>80?BlockedSeconds+DT:0;
  if(Ahead && Cast<APawn>(Obstacle.GetActor()))
  {
   // A player's normal side-step, never a phase-through or actor manipulation.
   const FVector Side(-Input.Y,Input.X,0);
   for(float Sign:{1.f,-1.f})
   {
    FHitResult SideHit;const FVector Direction=(Side*Sign+Input*.15).GetSafeNormal2D();
    if(!World->SweepSingleByChannel(SideHit,Position,Position+Direction*130,FQuat::Identity,ECC_Pawn,Shape,Query))
    {Input=Direction;break;}
   }
  }
  if(Interval>33.3 || (BlockedSeconds>=.35 && Now-LastContext>=.5))
  {
   LastContext=Now;BlockedObservations+=BlockedSeconds>=.35?1:0;
   auto Event=MakeShared<FJsonObject>();Event->SetNumberField(TEXT("wall_seconds"),Now-Began);
   Event->SetNumberField(TEXT("game_seconds"),World->GetTimeSeconds());Event->SetNumberField(TEXT("interval_ms"),Interval);
   Event->SetStringField(TEXT("player_cm"),Position.ToString());Event->SetStringField(TEXT("velocity_cm_s"),Player->GetVelocity().ToString());
   Event->SetStringField(TEXT("ahead_actor"),GetPathNameSafe(Obstacle.GetActor()));Event->SetStringField(TEXT("ahead_component"),GetPathNameSafe(Obstacle.GetComponent()));
   Event->SetStringField(TEXT("native_floor_actor"),GetPathNameSafe(Movement->CurrentFloor.HitResult.GetActor()));
   Event->SetNumberField(TEXT("blocked_seconds"),BlockedSeconds);
   for(TActorIterator<ACoastalOceanSurface> It(World);It;++It)
   {
    const auto D=It->GetSurfaceInstanceDiagnostic();Event->SetNumberField(TEXT("ocean_selected_nodes"),It->GetSurfaceNodeCount());
    Event->SetNumberField(TEXT("ocean_rendered_instances"),D.RenderedInstances);Event->SetNumberField(TEXT("ocean_pure_order_updates"),double(D.OrderOnlySelections));
    Event->SetNumberField(TEXT("ocean_added_nodes"),double(D.AddedNodes));Event->SetNumberField(TEXT("ocean_removed_nodes"),double(D.RemovedNodes));break;
   }
   Events.Add(MakeShared<FJsonValueObject>(Event));
   TRACE_BOOKMARK(TEXT("C921WalkStutterRoute interval_ms=%.4f wall=%.6f blocked=%.3f obstacle=%s"),Interval,Now-Began,BlockedSeconds,*GetPathNameSafe(Obstacle.GetActor()));
  }
  for(TActorIterator<ACoastalOceanSurface> It(World);It;++It)if(It->IsReady())
  {
   const auto D=It->GetSurfaceInstanceDiagnostic();
   if(D.TrackedNodes!=It->GetSurfaceNodeCount() || D.RenderedInstances!=D.TrackedNodes)
    return Finish(Player,TEXT("Actual runtime ocean node ownership/count diverged from unchanged CDLOD selection"));
   break;
  }
  if(Delta.Size2D()<60)
  {
   Movement->StopMovementImmediately();At=Now;Goal=(Goal+1)%UE_ARRAY_COUNT(Goals);
   if(Goal==0 && ++Loops>=1 && Now-Began>=25 && TravelCm>=5000)return Finish(Player,TEXT(""));
   return false;
  }
  if(Now-Began>75 || Now-At>25)
   return Finish(Player,FString::Printf(TEXT("Real turning walk stalled at leg%d pose%s obstacle%s; contact context retained"),Goal,*Position.ToString(),*GetPathNameSafe(Obstacle.GetActor())));
  Player->AddMovementInput(Input,FMath::Clamp(float(Delta.Size2D()/120.),.2f,1.f));
  PC->SetControlRotation(FRotator(0,Input.Rotation().Yaw,0));return false;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandWalkStutterRoute921,
 "NeonCity.Phase205c.MainlandC921.Standalone.WalkStutterRoute",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandWalkStutterRoute921::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) ||
    FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
 {AddError(TEXT("Targeted walking diagnostic requires isolated normal play, disabled automatic persistence and no image readback"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandStutter921::FWalk(this));return true;
}
#endif
