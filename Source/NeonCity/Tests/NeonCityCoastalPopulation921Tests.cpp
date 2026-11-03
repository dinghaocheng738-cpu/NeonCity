#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/CoastalAmbientSeatComponent.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "AI/NeonCityNPCController.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Combat/ThreatSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "Presentation/CoastalAmbientAnimInstance.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "Engine/OverlapResult.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/NeonCityCafe52LifecycleFixtures.h"
#include "Tests/NeonCityScene378PopulationContract.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace CoastalPopulation921
{
class FObserve : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ACameraActor* Camera=nullptr;TWeakObjectPtr<AActor> OldView;
 TArray<APedestrianSpawnPoint*> Markers,Cafe,AllCafe;TMap<ANeonCityNPCCharacter*,FVector> Origins,Hands,FleeStarts;TMap<ANeonCityNPCCharacter*,float> Contacts,HandMotion,FleeDistance;
 float At=0;double Wall=FPlatformTime::Seconds();int32 Stage=0,Stills=0;FGuid Incident;TSet<ANeonCityNPCCharacter*> Fled;FString Failure;
 float NextAdmissionSample=0;TArray<TSharedPtr<FJsonValue>> AdmissionSamples;
 TWeakObjectPtr<AActor> DangerSource;int32 WantedBeforeDanger=-1,EvidenceBeforeDanger=-1;
 FName SelectedGroup;bool NormalExpiry=false,ExtendedPopulation=false,PlayerFixture=false,CleanedUp=false;int32 MaximumLive=0;
 FTransform PlayerBefore;FRotator ControlBefore;TWeakObjectPtr<ACharacter> Observer;
 TArray<TSharedPtr<FJsonValue>> RouteChecks,ExpirySamples,FixtureAttempts;float ExpiryDeadline=0,NextExpirySample=0;
 struct FExpiryProgress
 {
  bool Expired=false,Standing=false,LeftEntry=false,Returned=false,Reseated=false;
  FVector FirstStanding=FVector::ZeroVector,Previous=FVector::ZeroVector;
  float MaximumEntryDistance=0,MaximumStandingDisplacement=0,PathLength=0,FirstStandingSeatSeconds=0;
  int32 NativeMovementSamples=0;uint32 StatesBefore=0,StatesAfter=0;
 };
 TMap<ANeonCityNPCCharacter*,FExpiryProgress> Expiry;
 TMap<ANeonCityNPCCharacter*,float> TeaWindowStart,TeaWindowEnd;
 bool ExpandedCafe52=false;float NextShoeSample=0;TArray<TSharedPtr<FJsonValue>> ActualShoeSamples;
 static constexpr float TeaObservationSeconds=UCoastalAmbientAnimInstance::CupCycleSeconds+.5f;
 TSharedRef<FJsonObject> Describe(APedestrianSpawnPoint* M)const
 {
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("id"),M->PedestrianId.ToString());R->SetStringField(TEXT("seat_support"),GetPathNameSafe(M->AmbientSeatSupport.Get()));
  R->SetStringField(TEXT("marker_location"),M->GetActorLocation().ToString());R->SetStringField(TEXT("measured_hip_world"),M->GetActorTransform().TransformPosition(M->AmbientSeatHipLocal).ToString());
  auto* N=M->GetSpawnedPedestrian();R->SetBoolField(TEXT("spawned"),N!=nullptr);
  if(N)
  {
   auto* S=N->FindComponentByClass<UCoastalAmbientSeatComponent>();auto* C=Cast<ANeonCityNPCController>(N->GetController());
   R->SetStringField(TEXT("npc_location"),N->GetActorLocation().ToString());R->SetStringField(TEXT("velocity"),N->GetVelocity().ToString());
   R->SetStringField(TEXT("actor_owner"),GetPathNameSafe(N->GetOwner()));R->SetStringField(TEXT("source_spawn_point"),GetPathNameSafe(N->GetSpawnPoint()));
   R->SetBoolField(TEXT("source_binding_matches_marker"),N->GetSpawnPoint()==M);
   const bool Female=(FCrc::StrCrc32(*M->PedestrianId.ToString())%2)!=0;
   R->SetStringField(TEXT("authored_expected_skin"),GetPathNameSafe(Female?M->AmbientFemaleSkin.Get():M->AmbientMaleSkin.Get()));
   R->SetStringField(TEXT("actual_skin"),GetPathNameSafe(N->GetMesh()->GetSkeletalMeshAsset()));
   R->SetStringField(TEXT("state"),UEnum::GetValueAsString(N->GetNPCState()->GetState()));R->SetNumberField(TEXT("state_seconds"),N->GetNPCState()->GetStateSeconds());
   R->SetStringField(TEXT("reaction"),C?UEnum::GetValueAsString(C->GetReaction()):TEXT("NoController"));
   R->SetBoolField(TEXT("dormant"),N->IsPopulationDormant());R->SetBoolField(TEXT("threat_active"),N->GetThreatComponent()->HasActiveThreat());
   if(S){R->SetNumberField(TEXT("actual_seated_seconds"),S->GetSeatedSeconds());R->SetNumberField(TEXT("authored_seat_limit_seconds"),FMath::Clamp(M->AmbientSeatSeconds,10.f,120.f));}
   R->SetStringField(TEXT("seat_diagnostic"),S?S->GetSeatingDiagnostic():TEXT("Seat component missing"));
  }
  return R;
 }
 FString CheckAuthoredRoute(APedestrianSpawnPoint* M,ANeonCityNPCCharacter* N,ACharacter* Player)
 {
  // Read the actual loaded marker/capsules and query the game world's Pawn
  // collision. No source-plan geometry, ignored furniture or changed poses.
  if(M->LocalRoute.Num()<4)return TEXT("Foreground cafe needs its authored four-point return loop");
  FCollisionQueryParams Q(SCENE_QUERY_STAT(C923CafeRoute),false,N);
  for(ACharacter* Body:{static_cast<ACharacter*>(N),Player})
  {
   const auto* C=Body->GetCapsuleComponent();const float Radius=C->GetScaledCapsuleRadius(),Half=C->GetScaledCapsuleHalfHeight();
   const FCollisionShape Shape=FCollisionShape::MakeCapsule(Radius,Half);int32 Samples=0;float MaximumFloorOffset=0;
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),M->PedestrianId.ToString());Row->SetStringField(TEXT("capsule_owner"),GetPathNameSafe(Body));
   Row->SetNumberField(TEXT("radius_cm"),Radius);Row->SetNumberField(TEXT("halfheight_cm"),Half);Row->SetNumberField(TEXT("ignored_own_chairs"),0);
   RouteChecks.Add(MakeShared<FJsonValueObject>(Row));
   for(int32 I=0;I<M->LocalRoute.Num();++I)
   {
    const FVector A=M->GetActorTransform().TransformPosition(M->LocalRoute[I]);
    const FVector B=M->GetActorTransform().TransformPosition(M->LocalRoute[(I+1)%M->LocalRoute.Num()]);
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/40.f));FVector Previous=FVector::ZeroVector;
    for(int32 Step=0;Step<=Steps;++Step)
    {
     const FVector Authored=FMath::Lerp(A,B,float(Step)/Steps);FHitResult Floor,Hit;
     if(!W->LineTraceSingleByChannel(Floor,Authored+FVector(0,0,80),Authored-FVector(0,0,220),ECC_Pawn,Q)
        || Floor.ImpactNormal.Z<N->GetCharacterMovement()->GetWalkableFloorZ())
     {Row->SetStringField(TEXT("failure"),TEXT("No actual walkable floor"));Row->SetStringField(TEXT("sample"),Authored.ToString());return TEXT("Foreground cafe return route lost actual walkable floor");}
     const FVector Center=Floor.ImpactPoint+FVector(0,0,Half+2.f);
     MaximumFloorOffset=FMath::Max(MaximumFloorOffset,FMath::Abs(float(Authored.Z-Floor.ImpactPoint.Z-N->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-2.f)));
     if(W->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,Shape,Q)
        || (Step>0 && W->SweepSingleByChannel(Hit,Previous,Center,FQuat::Identity,ECC_Pawn,Shape,Q)))
     {Row->SetStringField(TEXT("failure"),TEXT("Full Pawn capsule route or entry blocked"));Row->SetStringField(TEXT("sample"),Center.ToString());Row->SetStringField(TEXT("blocker"),GetPathNameSafe(Hit.GetActor()));return TEXT("Foreground cafe full Pawn capsule route or entry is blocked");}
     Previous=Center;++Samples;
    }
   }
   Row->SetNumberField(TEXT("actual_floor_samples"),Samples);Row->SetNumberField(TEXT("maximum_authored_npc_floor_offset_cm"),MaximumFloorOffset);
   if(MaximumFloorOffset>8.f)return TEXT("Foreground cafe authored route height does not match actual ground");
   Row->SetBoolField(TEXT("full_capsule_loop_and_entry_clear"),true);
  }
  return FString();
 }
 FString CheckPlannedExit(APedestrianSpawnPoint* M,ANeonCityNPCCharacter* N)
 {
  const auto* S=N->FindComponentByClass<UCoastalAmbientSeatComponent>();FTransform Exit;
  if(!S || !S->GetPlannedStandingExit(Exit))return TEXT("Foreground cafe has no native checked standing exit");
  const auto* C=N->GetCapsuleComponent();const FCollisionShape Shape=FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight());
  FCollisionQueryParams Q(SCENE_QUERY_STAT(C923CafeActualExit),false,N);FHitResult Floor,Hit;
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),M->PedestrianId.ToString());Row->SetStringField(TEXT("planned_exit"),Exit.ToString());Row->SetNumberField(TEXT("radius_cm"),C->GetScaledCapsuleRadius());Row->SetNumberField(TEXT("halfheight_cm"),C->GetScaledCapsuleHalfHeight());
  RouteChecks.Add(MakeShared<FJsonValueObject>(Row));
  if(W->OverlapBlockingTestByChannel(Exit.GetLocation(),Exit.GetRotation(),ECC_Pawn,Shape,Q))return TEXT("Foreground full capsule standing exit overlaps actual world collision");
  // Production seat departure alone ignores its explicitly bound own chair.
  // The return route and standing endpoint above ignore no furniture.
  Q.AddIgnoredActor(M->AmbientSeatSupport.Get());const FVector Start=N->GetActorLocation(),End=Exit.GetLocation();
  if(W->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,Shape,Q)){Row->SetStringField(TEXT("blocker"),GetPathNameSafe(Hit.GetActor()));return TEXT("Foreground full capsule seat-to-exit path blocked");}
  const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,End)/40.f));
  for(int32 I=0;I<=Steps;++I)
  {
   const FVector P=FMath::Lerp(Start,End,float(I)/Steps);
   if(!W->LineTraceSingleByChannel(Floor,P+FVector(0,0,50),P-FVector(0,0,180),ECC_Pawn,Q)
      || Floor.ImpactNormal.Z<N->GetCharacterMovement()->GetWalkableFloorZ()
      || FMath::Abs(P.Z-Floor.ImpactPoint.Z-C->GetScaledCapsuleHalfHeight())>48.f)return TEXT("Foreground native standing transition has no actual walkable floor");
  }
  Row->SetStringField(TEXT("only_ignored_furniture"),GetPathNameSafe(M->AmbientSeatSupport.Get()));Row->SetNumberField(TEXT("actual_floor_samples"),Steps+1);Row->SetBoolField(TEXT("independent_full_capsule_exit_and_departure_clear"),true);return FString();
 }
 FString CheckStanding(APedestrianSpawnPoint* M,ANeonCityNPCCharacter* N,FExpiryProgress& Progress,const TSharedRef<FJsonObject>& Row)
 {
  const auto* C=N->GetCapsuleComponent();const FVector P=N->GetActorLocation();const auto* Move=N->GetCharacterMovement();
  const FCollisionShape Shape=FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight());
  FCollisionQueryParams Q(SCENE_QUERY_STAT(C923CafeNormalStanding),false,N);FHitResult Floor,Hit;
  const bool FloorHit=W->LineTraceSingleByChannel(Floor,P+FVector(0,0,50),P-FVector(0,0,180),ECC_Pawn,Q);
  const float Gap=FloorHit?P.Z-Floor.ImpactPoint.Z-C->GetScaledCapsuleHalfHeight():MAX_flt;
  const bool FloorOK=FloorHit && Floor.ImpactNormal.Z>=Move->GetWalkableFloorZ() && FMath::Abs(Gap)<=8.f;
  const bool Clear=!W->OverlapBlockingTestByChannel(P,N->GetActorQuat(),ECC_Pawn,Shape,Q);
  const bool SegmentClear=!Progress.Standing || !W->SweepSingleByChannel(Hit,Progress.Previous,P,FQuat::Identity,ECC_Pawn,Shape,Q);
  const bool Grounded=Move->IsMovingOnGround() && C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
  Row->SetBoolField(TEXT("actual_floor_valid"),FloorOK);Row->SetNumberField(TEXT("actual_floor_gap_cm"),Gap);Row->SetStringField(TEXT("actual_floor_actor"),GetPathNameSafe(Floor.GetActor()));
  Row->SetBoolField(TEXT("actual_full_capsule_clear"),Clear);Row->SetBoolField(TEXT("actual_standing_segment_clear"),SegmentClear);Row->SetBoolField(TEXT("native_grounded_with_collision"),Grounded);Row->SetStringField(TEXT("segment_blocker"),GetPathNameSafe(Hit.GetActor()));
  if(!FloorOK || !Clear || !SegmentClear || !Grounded)return FString::Printf(TEXT("%s normal cafe departure lost actual support/full capsule clearance/native grounding"),*M->PedestrianId.ToString());
  if(!Progress.Standing){Progress.FirstStanding=P;Progress.FirstStandingSeatSeconds=N->FindComponentByClass<UCoastalAmbientSeatComponent>()->GetSeatedSeconds();}
  else Progress.PathLength+=FVector::Dist2D(Progress.Previous,P);
  Progress.Standing=true;Progress.Previous=P;Progress.MaximumStandingDisplacement=FMath::Max(Progress.MaximumStandingDisplacement,float(FVector::Dist2D(Progress.FirstStanding,P)));
  const float EntryDistance=FVector::Dist2D(P,M->GetActorLocation());Progress.MaximumEntryDistance=FMath::Max(Progress.MaximumEntryDistance,EntryDistance);
  if(EntryDistance>80.f)Progress.LeftEntry=true;
  if(Progress.LeftEntry && EntryDistance<=45.f)Progress.Returned=true;
  if(N->GetLastMovementInputVector().Size2D()>.1f && N->GetVelocity().Size2D()>1.f)++Progress.NativeMovementSamples;
  Progress.StatesAfter=N->GetNPCState()->GetVisitedStateMask();return FString();
 }
 bool UpdateNormalExpiry()
 {
  bool Complete=true;const bool Sample=W->GetTimeSeconds()>=NextExpirySample;if(Sample)NextExpirySample=W->GetTimeSeconds()+.25f;
  for(auto* M:Cafe)
  {
   auto* N=M->GetSpawnedPedestrian();auto* S=N?N->FindComponentByClass<UCoastalAmbientSeatComponent>():nullptr;
   if(!N || !S || N->IsPopulationDormant())return End(TEXT("Foreground cafe identity/seat disappeared during natural expiry"));
    auto* ProgressPtr=Expiry.Find(N);
    if(!ProgressPtr)return End(FString::Printf(TEXT("Foreground cafe marker %s unexpectedly replaced its admitted NPC with %s during normal expiry"),*M->PedestrianId.ToString(),*GetPathNameSafe(N)));
    auto Row=Describe(M);auto& Progress=*ProgressPtr;const float Limit=FMath::Clamp(M->AmbientSeatSeconds,10.f,120.f);
   Row->SetNumberField(TEXT("elapsed_seconds"),W->GetTimeSeconds()-At);Row->SetNumberField(TEXT("authored_seat_limit_seconds"),Limit);Row->SetNumberField(TEXT("actual_seated_seconds"),S->GetSeatedSeconds());
   if(N->GetThreatComponent()->HasActiveThreat())return End(TEXT("An external threat interrupted the foreground normal-expiry test; no expiry pass claimed"));
   if(!S->IsSeatedPresentationActive())
   {
    if(!Progress.Expired && S->GetSeatedSeconds()+.01f<Limit)return End(TEXT("Foreground cafe stood before its actual authored expiry"));
    Progress.Expired=true;
    TInlineComponentArray<UStaticMeshComponent*> Parts(N);for(auto* C:Parts)if(C->GetFName()==TEXT("CoastalCafeCup") && (C->IsVisible() || C->GetAttachParent()!=nullptr))return End(TEXT("Normal-expiry civilian retained its cup or hand attachment"));
    const FString Error=CheckStanding(M,N,Progress,Row);if(!Error.IsEmpty())return End(Error);
   }
   else if(Progress.Expired)
   {
    if(!Progress.Returned || !Progress.LeftEntry || Progress.NativeMovementSamples<2 || Progress.MaximumStandingDisplacement<=1.f)
     return End(TEXT("Foreground cafe reseated before an actual >80cm departure and native return to <=45cm entry"));
    if(S->GetSeatAlpha()>=.99f)Progress.Reseated=true;
   }
   Row->SetBoolField(TEXT("normal_expiry_observed"),Progress.Expired);Row->SetBoolField(TEXT("left_entry_over_80cm"),Progress.LeftEntry);Row->SetBoolField(TEXT("returned_to_entry_within_45cm"),Progress.Returned);Row->SetBoolField(TEXT("naturally_reseated"),Progress.Reseated);
   Row->SetNumberField(TEXT("maximum_entry_distance_cm"),Progress.MaximumEntryDistance);Row->SetNumberField(TEXT("actual_standing_path_cm"),Progress.PathLength);Row->SetNumberField(TEXT("native_movement_input_samples"),Progress.NativeMovementSamples);
   if(Sample)ExpirySamples.Add(MakeShared<FJsonValueObject>(Row));Complete&=Progress.Reseated;
  }
  if(Complete)return End();
  if(W->GetTimeSeconds()>ExpiryDeadline)return End(TEXT("Foreground normal seat expiry/departure/return/reseat exceeded authored duration plus 120-second route allowance"));
  return false;
 }
 void Cleanup()
 {
  if(CleanedUp)return;CleanedUp=true;
  if(DangerSource.IsValid())DangerSource->Destroy();DangerSource.Reset();
  if(IsValid(PC) && OldView.IsValid())PC->SetViewTarget(OldView.Get());OldView.Reset();
  if(IsValid(Camera))Camera->Destroy();Camera=nullptr;
  if(PlayerFixture && Observer.IsValid()){Observer->SetActorTransform(PlayerBefore,false,nullptr,ETeleportType::TeleportPhysics);if(IsValid(PC))PC->SetControlRotation(ControlBefore);}
 }
 bool End(const FString& Error=FString())
 {
  Failure=Error;if(!Error.IsEmpty())Test->AddError(Error);
  Cleanup();
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("enabled_markers"),Markers.Num());J->SetNumberField(TEXT("seated_pair"),Cafe.Num());
  J->SetNumberField(TEXT("initial_player_fixtures"),PlayerFixture?1:0);J->SetNumberField(TEXT("npc_pose_writes"),0);J->SetNumberField(TEXT("actual_stills"),Stills);TArray<TSharedPtr<FJsonValue>> Rows;
  J->SetStringField(TEXT("selected_conversation_group"),SelectedGroup.ToString());J->SetBoolField(TEXT("explicit_foreground18_population_mode"),ExtendedPopulation);J->SetBoolField(TEXT("normal_expiry_mode"),NormalExpiry);
  J->SetNumberField(TEXT("all_cafe_markers"),AllCafe.Num());J->SetNumberField(TEXT("maximum_observed_bound_civilians"),MaximumLive);J->SetNumberField(TEXT("live_civilian_budget"),NeonCityScene378Population::LiveBudget);
  J->SetStringField(TEXT("authored_population_contract"),NeonCityScene378Population::Contract);
  J->SetArrayField(TEXT("actual_skinned_shoe_floor_samples"),ActualShoeSamples);
  J->SetNumberField(TEXT("npc_input_or_state_writes"),0);J->SetNumberField(TEXT("authored_marker_writes"),0);J->SetNumberField(TEXT("injected_danger_events"),Incident.IsValid()?1:0);
  J->SetBoolField(TEXT("danger_has_player_instigator"),false);J->SetNumberField(TEXT("user_crime_resets"),0);
  J->SetNumberField(TEXT("wanted_before_danger"),WantedBeforeDanger);J->SetNumberField(TEXT("evidence_before_danger"),EvidenceBeforeDanger);
  if(W){const auto* Wanted=W->GetSubsystem<UWantedSubsystem>();J->SetNumberField(TEXT("wanted_after"),Wanted->GetWantedLevel());J->SetNumberField(TEXT("evidence_after"),Wanted->GetEvidenceScore());}
  TSet<const USkeletalMesh*> CafeSkins;for(auto* M:Cafe)if(auto* N=M->GetSpawnedPedestrian())CafeSkins.Add(N->GetMesh()->GetSkeletalMeshAsset());
  J->SetNumberField(TEXT("distinct_cafe_skin_count"),CafeSkins.Num());
  J->SetNumberField(TEXT("production_cup_cycle_seconds"),UCoastalAmbientAnimInstance::CupCycleSeconds);
  J->SetNumberField(TEXT("required_complete_tea_window_seconds"),TeaObservationSeconds);
  TArray<TSharedPtr<FJsonValue>> TeaWindows;
  for(const auto& V:TeaWindowStart){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("npc"),GetPathNameSafe(V.Key));R->SetNumberField(TEXT("actual_continuous_seated_seconds"),TeaWindowEnd.FindRef(V.Key)-V.Value);TeaWindows.Add(MakeShared<FJsonValueObject>(R));}
  J->SetArrayField(TEXT("actual_complete_tea_windows"),TeaWindows);
  for(auto* M:Cafe)
  {auto R=Describe(M);if(auto* N=M->GetSpawnedPedestrian()){R->SetNumberField(TEXT("minimum_rim_mouth_cm"),Contacts.FindRef(N));R->SetNumberField(TEXT("left_hand_motion_cm"),HandMotion.FindRef(N));R->SetNumberField(TEXT("flee_displacement_cm"),FleeDistance.FindRef(N));if(const auto* P=Expiry.Find(N)){R->SetBoolField(TEXT("normal_expiry_observed"),P->Expired);R->SetBoolField(TEXT("returned_after_actual_departure"),P->Returned);R->SetBoolField(TEXT("naturally_reseated"),P->Reseated);R->SetNumberField(TEXT("first_standing_seated_seconds"),P->FirstStandingSeatSeconds);R->SetNumberField(TEXT("maximum_entry_distance_cm"),P->MaximumEntryDistance);R->SetNumberField(TEXT("maximum_post_standing_displacement_cm"),P->MaximumStandingDisplacement);R->SetNumberField(TEXT("actual_post_standing_path_cm"),P->PathLength);R->SetNumberField(TEXT("native_movement_input_samples"),P->NativeMovementSamples);R->SetNumberField(TEXT("visited_states_before_expiry"),P->StatesBefore);R->SetNumberField(TEXT("visited_states_after_expiry"),P->StatesAfter);}}Rows.Add(MakeShared<FJsonValueObject>(R));}
  J->SetArrayField(TEXT("pair"),Rows);J->SetArrayField(TEXT("admission_samples"),AdmissionSamples);J->SetArrayField(TEXT("actual_world_capsule_route_checks"),RouteChecks);J->SetArrayField(TEXT("normal_expiry_samples"),ExpirySamples);J->SetArrayField(TEXT("observer_fixture_candidates"),FixtureAttempts);
  J->SetStringField(TEXT("scope"),NormalExpiry?TEXT("One supported player fixture restored afterwards; exact new authored pair only. Ordinary scheduler and licensed seat/tea animation, actual full NPC/player Pawn capsules on loaded route and entry, authored seat expiry without danger or state/timing writes, checked native standing locomotion, >80cm departure and <=45cm return, natural reseating. Original eight markers are read only. No source-plan collision substitute; subjective pose quality separate."):TEXT("One supported player fixture restored afterwards; original C921 pair only; ordinary population scheduler, licensed animation, measured chair/cup/lips, real threat event and native controller standing/flee/recovery. No NPC pose/input/state writes. Subjective pose quality separate."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label.IsEmpty()?TEXT("C921Population"):Label);IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Filename=NeonCityCafe52::IsGroup(SelectedGroup)?(SelectedGroup==TEXT("C923.Cafe52.Table0")?TEXT("cafe52-table0-lifecycle.json"):TEXT("cafe52-table1-lifecycle.json")):NormalExpiry?(SelectedGroup==TEXT("C923.Cafe.Foreground18.West")?TEXT("foreground18-cafe-west.json"):TEXT("foreground18-cafe-east.json")):TEXT("population921.json");const FString ReceiptPath=Dir/Filename;
  if(IFileManager::Get().FileExists(*ReceiptPath)){Test->AddError(FString::Printf(TEXT("Population receipt already exists; refusing to overwrite immutable evidence: %s"),*ReceiptPath));return true;}
  if(!FFileHelper::SaveStringToFile(Text,*ReceiptPath,FFileHelper::EEncodingOptions::AutoDetect,&IFileManager::Get(),FILEWRITE_NoReplaceExisting))Test->AddError(FString::Printf(TEXT("Population receipt could not be saved; validation is not accepted: %s"),*ReceiptPath));
  return true;
 }
 bool PairReady(bool Seated)
 {
  for(auto* M:Cafe)
  {
   auto* N=M->GetSpawnedPedestrian();auto* S=N?N->FindComponentByClass<UCoastalAmbientSeatComponent>():nullptr;
   if(!N || N->IsPopulationDormant() || !S || (Seated && (!S->IsSeatedPresentationActive() || S->GetSeatAlpha()<.99f)))return false;
   if(Seated && NeonCityCafe52::IsGroup(M->AmbientConversationGroup))
   {
    const auto* Occupant=N->FindComponentByClass<UTransportOccupantComponent>();
    const auto* Body=Occupant?Occupant->GetDriverMesh():nullptr;
    const auto* Anim=Body?Cast<UCoastalAmbientAnimInstance>(Body->GetAnimInstance()):nullptr;
    if(!Anim || !Anim->bAmbientFeetGrounded || Anim->AmbientFeetAlpha<.99f)return false;
   }
  }return true;
 }
 void View(int32 Index)
 {
  if(!Camera)return;FVector Center=FVector::ZeroVector;for(auto* M:Cafe)Center+=M->GetActorTransform().TransformPosition(M->AmbientSeatHipLocal);Center=Center/2+FVector(0,0,55);
  const FVector Eyes[]={Center+FVector(0,-420,85),Center+FVector(-420,-120,65),Center+FVector(0,300,70),PC->GetPawn()->GetPawnViewLocation()};
  Camera->SetActorLocationAndRotation(Eyes[Index],(Center-Eyes[Index]).Rotation());
 }
 void Capture(int32 Index)
 {
#if WITH_EDITOR
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
  {const FString Prefix=NeonCityCafe52::IsGroup(SelectedGroup)?(SelectedGroup==TEXT("C923.Cafe52.Table0")?TEXT("C923-Cafe52-Table0"):TEXT("C923-Cafe52-Table1")):NormalExpiry?(SelectedGroup==TEXT("C923.Cafe.Foreground18.West")?TEXT("C923-Foreground18-West-Cafe"):TEXT("C923-Foreground18-East-Cafe")):TEXT("C921-Cafe");const bool OK=FNeonCityVisualEvidence::CaptureRenderedJPEG(W,FString::Printf(TEXT("%s-%02d.jpg"),*Prefix,Index));Test->TestTrue(TEXT("Actual café pose screenshot saved"),OK);if(OK)++Stills;}
#endif
 }
public:
 explicit FObserve(FAutomationTestBase* T,FName Group=TEXT("C921.Cafe.Umbrella01")):Test(T),SelectedGroup(Group),NormalExpiry(Group!=TEXT("C921.Cafe.Umbrella01")){}
 virtual ~FObserve()override{Cleanup();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Wall>(NormalExpiry?360.:180.))return End(TEXT("Coastal population runtime validation exceeded wall timeout"));
  W=NeonCityValidationWorld();if(!W)return false;PC=W->GetFirstPlayerController();auto* Player=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!Player)return false;
  if(Stage==0)
  {
   int32 Retained=0,Disabled=0,Walk=0,Viewpoint=0,Staff=0,ForegroundTags=0;TMap<FName,int32> Groups;TSet<FName> EnabledIDs;
   for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel)
   {
    if(I->ActorHasTag(TEXT("NeonCity.MainlandC923.Foreground18")))++ForegroundTags;
    ++Retained;if(!I->bAmbientEnabled){++Disabled;continue;}Markers.Add(*I);
    const auto* Expected=NeonCityScene378Population::Find(I->PedestrianId);
    if(!Expected || EnabledIDs.Contains(I->PedestrianId) || I->AmbientActivity!=Expected->Activity
     || I->AmbientConversationGroup!=FName(Expected->Group))return End(TEXT("Enabled identity/activity/group differs from the explicit Current346-Scene375 contract"));
    EnabledIDs.Add(I->PedestrianId);
    if(I->AmbientActivity==ECoastalAmbientActivity::Stroll)++Walk;
    else if(I->AmbientActivity==ECoastalAmbientActivity::CafeConversation){AllCafe.Add(*I);++Groups.FindOrAdd(I->AmbientConversationGroup);if(I->AmbientConversationGroup==SelectedGroup)Cafe.Add(*I);}
    else if(I->AmbientActivity==ECoastalAmbientActivity::Viewpoint)++Viewpoint;else if(I->AmbientActivity==ECoastalAmbientActivity::HarborStaff)++Staff;
   }
   ExtendedPopulation=ForegroundTags>0 || FParse::Param(FCommandLine::Get(),TEXT("NeonCityForeground18"));
   ExpandedCafe52=Groups.Contains(TEXT("C923.Cafe52.Table0")) || Groups.Contains(TEXT("C923.Cafe52.Table1"));
   if(NormalExpiry && !ExtendedPopulation)return End(TEXT("Foreground cafe validation requires explicit authored Foreground18 marker tag or command-line mode"));
   if(Retained!=NeonCityScene378Population::Retained || Disabled!=NeonCityScene378Population::Disabled
    || Markers.Num()!=NeonCityScene378Population::Enabled || Walk!=NeonCityScene378Population::Stroll
    || AllCafe.Num()!=NeonCityScene378Population::Cafe || Viewpoint!=NeonCityScene378Population::Viewpoint || Staff!=NeonCityScene378Population::Staff
    || ForegroundTags!=NeonCityScene378Population::ForegroundTags || !ExtendedPopulation || !ExpandedCafe52)
    return End(TEXT("Current346-Scene375 requires exactly 81 retained/61 disabled/20 enabled: 8 stroll/10 cafe/1 view/1 harbor, four Foreground18 tags and five cafe groups"));
   if(Groups.Num()!=NeonCityScene378Population::ConversationGroups || Groups.FindRef(TEXT("C921.Cafe.Umbrella01"))!=2 || Cafe.Num()!=2)return End(TEXT("Expected exact original cafe pair and explicitly selected two-person conversation group"));
   if(ExpandedCafe52)
   {
    if(!ExtendedPopulation || Groups.FindRef(TEXT("C923.Cafe52.Table0"))!=2 || Groups.FindRef(TEXT("C923.Cafe52.Table1"))!=2)return End(TEXT("Cafe52 requires two new pairs alongside all original groups"));
    int32 Guests=0;
    for(auto* M:Markers)if(NeonCityCafe52::IsGroup(M->AmbientConversationGroup)){++Guests;const FString Error=NeonCityCafe52::CheckAuthoredGuest(M);if(!Error.IsEmpty())return End(Error);}
      else if(M->bAmbientGroundFeet)return End(TEXT("Old population must retain default-off foot grounding"));
    if(Guests!=4)return End(TEXT("Exactly four measured Cafe52 identities required"));
   }
   if(ExtendedPopulation)
   {
    if((ForegroundTags!=0 && ForegroundTags!=4) || Groups.FindRef(TEXT("C923.Cafe.Foreground18.West"))!=2 || Groups.FindRef(TEXT("C923.Cafe.Foreground18.East"))!=2)return End(TEXT("Foreground18 requires four tagged new identities and three exact two-person groups"));
    for(int32 I=9;I<=12;++I)
    {
     const FName Id(*FString::Printf(TEXT("SouthBeach.Pedestrian.%02d"),I));const FName Group(I<=10?TEXT("C923.Cafe.Foreground18.West"):TEXT("C923.Cafe.Foreground18.East"));
     int32 Matches=0;for(auto* M:AllCafe)if(M->PedestrianId==Id){++Matches;if(M->AmbientConversationGroup!=Group || (ForegroundTags>0 && !M->ActorHasTag(TEXT("NeonCity.MainlandC923.Foreground18"))))return End(TEXT("Foreground18 identity/group/tag binding mismatch"));}
     if(Matches!=1)return End(TEXT("Foreground18 must reuse exact unique SouthBeach pedestrian IDs 09 through 12"));
    }
   }
   for(auto* M:Markers)if(!M->AmbientAnimationClass || !M->AmbientMaleSkin || !M->AmbientFemaleSkin || !M->bDistanceManaged)return End(TEXT("Authored population missing owned graph/hard skin references or existing distance manager"));
   if(Cafe[0]->AmbientConversationGroup.IsNone() || Cafe[0]->AmbientConversationGroup!=Cafe[1]->AmbientConversationGroup)return End(TEXT("Cafe partners are not an authored pair"));
   const FVector Center=(Cafe[0]->GetActorLocation()+Cafe[1]->GetActorLocation())*.5;
   const TArray<FVector> Offsets=NormalExpiry?TArray<FVector>{FVector(0,-250,0),FVector(0,-450,0),FVector(0,450,0),FVector(-450,0,0),FVector(450,0,0)}:TArray<FVector>{FVector(0,-250,0)};
   FVector Start=FVector::ZeroVector;bool Found=false;FCollisionQueryParams Q(SCENE_QUERY_STAT(C921PlayerFixture),false,Player);
   for(const FVector Offset:Offsets)
   {
    FVector Candidate=Center+Offset;FHitResult H;auto Facts=MakeShared<FJsonObject>();Facts->SetStringField(TEXT("candidate"),Candidate.ToString());FixtureAttempts.Add(MakeShared<FJsonValueObject>(Facts));
    if(NormalExpiry)
    {
     bool AwayFromRoutes=true;const float Required=Player->GetSimpleCollisionRadius()+GetDefault<ANeonCityNPCCharacter>()->GetSimpleCollisionRadius()+25.f;
     for(auto* M:Markers)for(int32 I=0;I<M->LocalRoute.Num();++I)
     {
      const FVector A=M->GetActorTransform().TransformPosition(M->LocalRoute[I]),B=M->GetActorTransform().TransformPosition(M->LocalRoute[(I+1)%M->LocalRoute.Num()]);
      const FVector P(Candidate.X,Candidate.Y,0),FlatA(A.X,A.Y,0),FlatB(B.X,B.Y,0);
      if(FVector::Dist(P,FMath::ClosestPointOnSegment(P,FlatA,FlatB))<Required)AwayFromRoutes=false;
     }
     Facts->SetBoolField(TEXT("clear_of_all_authored_civilian_routes"),AwayFromRoutes);if(!AwayFromRoutes)continue;
    }
    if(!W->LineTraceSingleByChannel(H,Candidate+FVector(0,0,120),Candidate-FVector(0,0,240),ECC_Pawn,Q) || H.ImpactNormal.Z<.94){Facts->SetStringField(TEXT("rejected"),TEXT("No actual quay floor"));continue;}
    Candidate.Z=H.ImpactPoint.Z+Player->GetSimpleCollisionHalfHeight()+2;Facts->SetStringField(TEXT("floor_actor"),GetPathNameSafe(H.GetActor()));
    if(W->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight()-(NormalExpiry?0.f:1.f)),Q)){Facts->SetStringField(TEXT("rejected"),TEXT("Observer capsule blocked"));continue;}
    Start=Candidate;Found=true;Facts->SetBoolField(TEXT("selected"),true);break;
   }
   if(!Found)return End(TEXT("Cafe observer has no supported clear fixture away from actual civilian routes"));
   Observer=Player;PlayerBefore=Player->GetActorTransform();ControlBefore=PC->GetControlRotation();Player->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);PlayerFixture=true;
   OldView=PC->GetViewTarget();Camera=W->SpawnActor<ACameraActor>();if(!Camera)return End(TEXT("Cafe observation camera could not be created"));Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);View(0);At=W->GetTimeSeconds();Stage=1;return false;
  }
  int32 Live=0;for(auto* M:Markers)if(auto* N=M->GetSpawnedPedestrian())
  {
   ++Live;if(N->GetSpawnPoint()!=M)return End(TEXT("Possessed civilian lost its actual weak spawn-point authoring binding"));
   const auto* ExpectedSkin=FCrc::StrCrc32(*M->PedestrianId.ToString())%2?M->AmbientFemaleSkin.Get():M->AmbientMaleSkin.Get();
   if(N->GetMesh()->GetSkeletalMeshAsset()!=ExpectedSkin || !N->GetMesh()->GetAnimInstance() || !N->GetMesh()->GetAnimInstance()->IsA(M->AmbientAnimationClass.Get()))
    return End(TEXT("Possessed civilian lost the identity-selected hard skin or owned ambient animation class"));
  }
  const int32 Budget=NeonCityScene378Population::LiveBudget;if(Live>Budget)return End(TEXT("Authored population exceeded its explicitly selected live civilian budget"));
  int32 BoundCivilians=0;TSet<APedestrianSpawnPoint*> LiveSources;
  for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I)if(auto* Source=I->GetSpawnPoint();Source && Source->GetLevel()==W->PersistentLevel)
  {
   ++BoundCivilians;if(!Source->bAmbientEnabled || !Markers.Contains(Source) || LiveSources.Contains(Source))return End(TEXT("Disabled/unknown/duplicate population identity spawned outside the authored civilian budget"));LiveSources.Add(Source);
  }
  MaximumLive=FMath::Max(MaximumLive,BoundCivilians);if(BoundCivilians>Budget)return End(TEXT("Actual source-bound civilian count exceeded the safe population budget"));
  if(Stage==1)
  {
   if(W->GetTimeSeconds()-At>=NextAdmissionSample)
   {for(auto* M:Cafe){auto R=Describe(M);R->SetNumberField(TEXT("elapsed_seconds"),W->GetTimeSeconds()-At);AdmissionSamples.Add(MakeShared<FJsonValueObject>(R));}NextAdmissionSample+=.5f;}
   bool AdmissionWindow=PairReady(true);
   // A second fix-specific test can share a running world with the first.
   // Observe a complete production tea cycle plus margin, without resetting
   // either NPC or requiring their naturally staggered seat clocks to match.
   if(AdmissionWindow && NormalExpiry)for(auto* M:Cafe)
    if(FMath::Clamp(M->AmbientSeatSeconds,10.f,120.f)-M->GetSpawnedPedestrian()->FindComponentByClass<UCoastalAmbientSeatComponent>()->GetSeatedSeconds()<TeaObservationSeconds+1.f)AdmissionWindow=false;
   if(!AdmissionWindow){if(W->GetTimeSeconds()-At>(NormalExpiry?120.f:15.f))return End(TEXT("Both real cafe civilians failed to reach a natural measured-seat observation window"));return false;}
   if(Cafe[0]->GetSpawnedPedestrian()->GetMesh()->GetSkeletalMeshAsset()==Cafe[1]->GetSpawnedPedestrian()->GetMesh()->GetSkeletalMeshAsset())
    return End(TEXT("Authored cafe pair repeats the same skin instead of its measured appearance variation"));
   for(auto* M:Cafe)
   {
    auto* N=M->GetSpawnedPedestrian();Origins.Add(N,N->GetActorLocation());Contacts.Add(N,10000);HandMotion.Add(N,0);FleeDistance.Add(N,0);
    TeaWindowStart.Add(N,N->FindComponentByClass<UCoastalAmbientSeatComponent>()->GetSeatedSeconds());
    if(NormalExpiry)
    {
     FString Error=CheckAuthoredRoute(M,N,Player);if(!Error.IsEmpty())return End(Error);Error=CheckPlannedExit(M,N);if(!Error.IsEmpty())return End(Error);
     auto& Progress=Expiry.FindOrAdd(N);Progress.StatesBefore=N->GetNPCState()->GetVisitedStateMask();
    }
   }
   At=W->GetTimeSeconds();Stage=2;return false;
  }
  if(Stage==2)
  {
   if(NeonCityCafe52::IsGroup(SelectedGroup) && W->GetTimeSeconds()>=NextShoeSample)
   {
    NextShoeSample=W->GetTimeSeconds()+.5f;
    for(auto* M:Cafe){FString Error;ActualShoeSamples.Add(MakeShared<FJsonValueObject>(NeonCityCafe52::SampleGroundedShoes(M,Error)));if(!Error.IsEmpty())return End(Error);}
   }
   for(auto* M:Cafe)
   {
    auto* N=M->GetSpawnedPedestrian();auto* Seat=N?N->FindComponentByClass<UCoastalAmbientSeatComponent>():nullptr;auto* O=N?N->FindComponentByClass<UTransportOccupantComponent>():nullptr;auto* Body=O?O->GetDriverMesh():nullptr;
    auto* Anim=Body?Cast<UCoastalAmbientAnimInstance>(Body->GetAnimInstance()):nullptr;UStaticMeshComponent* Cup=nullptr;if(N){TInlineComponentArray<UStaticMeshComponent*> Parts(N);for(auto* C:Parts)if(C->GetFName()==TEXT("CoastalCafeCup"))Cup=C;}
    if(!N || !Seat || !Body || !Anim || !Cup || !Cup->IsVisible() || Anim->AmbientActivityIndex!=2 || Anim->AmbientAlpha<.8f || M->AmbientMouthHeadLocal.IsNearlyZero() || M->AmbientCupRimLocal.IsNearlyZero())return End(TEXT("Measured live seated conversation/tea presentation missing"));
    const FVector Target=M->GetActorTransform().TransformPosition(M->AmbientSeatHipLocal),Hip=Body->GetSocketLocation(TEXT("pelvis"));
    const FVector Rim=Cup->GetComponentTransform().TransformPosition(M->AmbientCupRimLocal),Mouth=Body->GetSocketTransform(TEXT("head"),RTS_World).TransformPosition(M->AmbientMouthHeadLocal);
    if(Hip.ContainsNaN() || Rim.ContainsNaN() || Mouth.ContainsNaN() || FVector::Dist(Hip,Target)>2 || FVector::Dist2D(N->GetActorLocation(),Target)>2)return End(TEXT("Cafe person/hip/cup is nonfinite or detached from actual seat"));
    Contacts[N]=FMath::Min(Contacts[N],float(FVector::Dist(Rim,Mouth)));const FVector Hand=Body->GetSocketLocation(TEXT("hand_l"));if(!Hands.Contains(N))Hands.Add(N,Hand);HandMotion[N]=FMath::Max(HandMotion[N],float(FVector::Dist(Hands[N],Hand)));
    TeaWindowEnd.Add(N,Seat->GetSeatedSeconds());
   }
   const float Age=W->GetTimeSeconds()-At;const int32 Index=FMath::Clamp(int32(Age/5),0,3);if(Index<3 && Age>Index*5+2 && Stills==Index){Capture(Index);View(Index+1);}
   if(Age<(NormalExpiry?TeaObservationSeconds:22.f))return false;
   if(Stills==3)Capture(3);
   if(NormalExpiry && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")) && Stills!=4)return End(TEXT("Foreground cafe requested four actual pose images but capture did not complete"));
   for(auto* M:Cafe){auto* N=M->GetSpawnedPedestrian();if(Contacts[N]>3.5f || HandMotion[N]<1.5f)return End(FString::Printf(TEXT("Cafe source animation/tea contact failed: %s contact%.3fcm left-hand-motion%.3fcm"),*M->PedestrianId.ToString(),Contacts[N],HandMotion[N]));}
   for(auto* M:Cafe){auto* N=M->GetSpawnedPedestrian();if(TeaWindowEnd.FindRef(N)-TeaWindowStart.FindRef(N)<UCoastalAmbientAnimInstance::CupCycleSeconds)return End(TEXT("Cafe observation did not cover a complete real seated tea cycle"));}
   if(NormalExpiry)
   {
    float Remaining=0;for(auto* M:Cafe)Remaining=FMath::Max(Remaining,FMath::Clamp(M->AmbientSeatSeconds,10.f,120.f)-M->GetSpawnedPedestrian()->FindComponentByClass<UCoastalAmbientSeatComponent>()->GetSeatedSeconds());
    At=W->GetTimeSeconds();ExpiryDeadline=At+Remaining+120.f;NextExpirySample=At;Stage=4;return false;
   }
   FActorSpawnParameters SourceParams;SourceParams.ObjectFlags|=RF_Transient;SourceParams.Name=MakeUniqueObjectName(W->PersistentLevel,AActor::StaticClass(),TEXT("C921CafeDangerSource"));
   DangerSource=W->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,SourceParams);
   if(!DangerSource.IsValid())return End(TEXT("Transient non-player danger source could not be created"));
   const FVector DangerLocation=Player->GetActorLocation()+FVector(0,75,0);
   auto* Root=NewObject<USceneComponent>(DangerSource.Get());DangerSource->SetRootComponent(Root);DangerSource->AddInstanceComponent(Root);Root->RegisterComponent();DangerSource->SetActorLocation(DangerLocation);
   const auto* Wanted=W->GetSubsystem<UWantedSubsystem>();WantedBeforeDanger=Wanted->GetWantedLevel();EvidenceBeforeDanger=Wanted->GetEvidenceScore();
   // Keep this ordinary sight-dependent stimulus outside the observer's real
   // capsule. The player's pawn is not ignored by the non-player source trace.
   Incident=W->GetSubsystem<UThreatSubsystem>()->EmitNearbyDanger(DangerSource.Get(),nullptr,DangerLocation,1500);if(!Incident.IsValid())return End(TEXT("Normal non-player world danger stimulus was not emitted"));At=W->GetTimeSeconds();Stage=3;return false;
  }
  if(Stage==4)return UpdateNormalExpiry();
  if(Stage==3)
  {
   const auto* Wanted=W->GetSubsystem<UWantedSubsystem>();if(Wanted->GetWantedLevel()>WantedBeforeDanger || Wanted->GetEvidenceScore()>EvidenceBeforeDanger)
    return End(TEXT("Non-player cafe danger incorrectly increased the player's wanted/evidence state"));
   bool Recovered=true;
   for(auto* M:Cafe)
   {
    auto* N=M->GetSpawnedPedestrian();if(!N)return End(TEXT("A cafe identity disappeared during native threat response"));
    const auto* Seat=N->FindComponentByClass<UCoastalAmbientSeatComponent>();
    if(Seat && !Seat->IsSeatedPresentationActive())
    {
     TInlineComponentArray<UStaticMeshComponent*> Parts(N);for(auto* C:Parts)if(C->GetFName()==TEXT("CoastalCafeCup") && (C->IsVisible() || C->GetAttachParent()!=nullptr))
      return End(TEXT("Standing civilian retained a visible cup or a stale seated hand-socket attachment"));
    }
    if(Seat && !Seat->IsSeatedPresentationActive() && N->GetNPCState()->GetState()==ENeonCityNPCState::Flee)
    {
     // Standing restores a checked entrance pose. That seat-to-entrance move
     // is not fleeing: measure only later movement in the genuine Flee state.
     Fled.Add(N);const FVector Start=FleeStarts.FindOrAdd(N,N->GetActorLocation());
     FleeDistance[N]=FMath::Max(FleeDistance[N],float(FVector::Dist2D(N->GetActorLocation(),Start)));
    }
    Recovered&=!N->GetThreatComponent()->HasActiveThreat() && N->GetCharacterMovement()->MovementMode!=MOVE_None;
   }
   if(Recovered && W->GetTimeSeconds()-At>8)
   {for(auto* M:Cafe){auto* N=M->GetSpawnedPedestrian();if(!Fled.Contains(N) || FleeDistance[N]<80)return End(TEXT("Cafe civilian did not actually stand, flee and recover through the existing controller"));}return End();}
   if(W->GetTimeSeconds()-At>40)return End(TEXT("Cafe civilians failed to recover normal locomotion after native danger"));return false;
  }
  return false;
 }
};

/** Small real-sensor handoff repro. Only the player and parked Chaos car have
 * declared temporary fixtures; the seated person is never moved or signalled
 * directly and its reaction/state remain owned by the production controller. */
class FSeatedVehicleEvade : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AIslandSupercar> Car;TWeakObjectPtr<APedestrianSpawnPoint> Marker;TWeakObjectPtr<ACharacter> Player;
 TWeakObjectPtr<UStaticMeshComponent> CarBody;
 FTransform CarBefore,PlayerBefore,StandExit=FTransform::Identity;FVector LinearBefore=FVector::ZeroVector,AngularBefore=FVector::ZeroVector,Approach=FVector::ZeroVector;
 FVector SeatedStart=FVector::ZeroVector,ReactionEntry=FVector::ZeroVector,Goal=FVector::ZeroVector,PreviousStanding=FVector::ZeroVector;
 FVector ControlledVelocity=FVector(1000,0,0);
 // Match MoveToPoint's strict native arrival radius; the 80 cm minimum
 // measures the whole response from the seated capsule, not only its tail.
 static constexpr float GoalArrivalCm=35.f,MinimumAvoidanceCm=80.f;
 float At=0,Moved=0,TotalMoved=0,PostStandPath=0,GoalDistance=MAX_flt,LateralClearance=-MAX_flt;
 double Began=FPlatformTime::Seconds();int32 Stage=0,DamageBefore=0,MovingInputSamples=0;
 bool PlayerFixture=false,CarFixture=false,ReactPreserved=false,HaveStart=false,HaveReaction=false,GoalArrived=false,HavePreviousStanding=false;
 TArray<TSharedPtr<FJsonValue>> SensorSamples;
 TSharedRef<FJsonObject> Fixture=MakeShared<FJsonObject>();
 TArray<TSharedPtr<FJsonValue>> FixtureAttempts;
 static TSharedRef<FJsonObject> FloorFacts(const FHitResult& H)
 {
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("hit"),H.bBlockingHit);J->SetStringField(TEXT("actor"),GetPathNameSafe(H.GetActor()));
  J->SetStringField(TEXT("component"),GetPathNameSafe(H.GetComponent()));J->SetStringField(TEXT("impact"),H.ImpactPoint.ToString());J->SetNumberField(TEXT("normal_z"),H.ImpactNormal.Z);return J;
 }
 bool PlanApproach(UWorld* W,const FVector Hip,const FHitResult& OldFloor)
 {
  FVector Low,High;CarBody->GetLocalBounds(Low,High);
  const FTransform BodyLocal=CarBody->GetComponentTransform().GetRelativeTransform(Car->GetActorTransform());
  const FBox LocalBox=FBox(Low,High).TransformBy(BodyLocal);
  const FVector Extent=LocalBox.GetExtent(),Center=LocalBox.GetCenter();
  FCollisionQueryParams Q(SCENE_QUERY_STAT(C921SeatCarEnvelope),false,Car.Get());
  // A centre floor ray alone misses planters inside the real fitted car.
  // Check its entire body and the complete short stimulus corridor before
  // moving it, then sample support under all four footprint corners.
  for(float Direction:{1.f,-1.f})
  {
   auto Attempt=MakeShared<FJsonObject>();FixtureAttempts.Add(MakeShared<FJsonValueObject>(Attempt));
   FVector Start(Hip.X-Direction*1200,Hip.Y+35,200);FHitResult Floor,Hit;
   W->LineTraceSingleByChannel(Floor,Start+FVector(0,0,100),Start-FVector(0,0,300),ECC_Pawn,Q);
   Attempt->SetObjectField(TEXT("center_floor"),FloorFacts(Floor));
   if(!Floor.bBlockingHit || Floor.ImpactNormal.Z<.94f){Attempt->SetStringField(TEXT("rejected"),TEXT("Unsupported centre"));continue;}
   Start.Z=Floor.ImpactPoint.Z+CarBefore.GetLocation().Z-OldFloor.ImpactPoint.Z;
   const FQuat Rotation=FRotator(0,Direction>0?0:180,0).Quaternion();
   const FVector End=Start+FVector(Direction*600,0,0),BodyStart=Start+Rotation.RotateVector(Center),BodyEnd=End+Rotation.RotateVector(Center);
   Attempt->SetStringField(TEXT("start"),Start.ToString());Attempt->SetStringField(TEXT("end"),End.ToString());Attempt->SetStringField(TEXT("body_extent"),Extent.ToString());
   TArray<FOverlapResult> Overlaps;W->OverlapMultiByChannel(Overlaps,BodyStart,Rotation,ECC_Vehicle,FCollisionShape::MakeBox(Extent),Q);
   TArray<TSharedPtr<FJsonValue>> Contacts;bool Blocked=false;
   for(const auto& O:Overlaps)if(O.bBlockingHit){Blocked=true;auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("actor"),GetPathNameSafe(O.GetActor()));C->SetStringField(TEXT("component"),GetPathNameSafe(O.GetComponent()));Contacts.Add(MakeShared<FJsonValueObject>(C));}
   Attempt->SetArrayField(TEXT("initial_blocking_contacts"),Contacts);
   if(Blocked){Attempt->SetStringField(TEXT("rejected"),TEXT("Initial fitted body blocked"));continue;}
   if(W->SweepSingleByChannel(Hit,BodyStart,BodyEnd,Rotation,ECC_Vehicle,FCollisionShape::MakeBox(Extent),Q))
   {Attempt->SetObjectField(TEXT("corridor_blocker"),FloorFacts(Hit));Attempt->SetStringField(TEXT("rejected"),TEXT("Fitted body corridor blocked"));continue;}
   bool Supported=true;TArray<TSharedPtr<FJsonValue>> Floors;
   for(int32 Step=0;Step<=3 && Supported;++Step)for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})
   {
    const FVector P=FMath::Lerp(Start,End,Step/3.f)+Rotation.RotateVector(Center+FVector(X*Extent.X,Y*Extent.Y,0));
    FHitResult H;W->LineTraceSingleByChannel(H,FVector(P.X,P.Y,Start.Z+100),FVector(P.X,P.Y,Start.Z-300),ECC_Pawn,Q);
    Floors.Add(MakeShared<FJsonValueObject>(FloorFacts(H)));
    Supported&=H.bBlockingHit && H.ImpactNormal.Z>=.94f && FMath::Abs(H.ImpactPoint.Z-Floor.ImpactPoint.Z)<12.f;
   }
   Attempt->SetArrayField(TEXT("footprint_floor_samples"),Floors);
   if(!Supported){Attempt->SetStringField(TEXT("rejected"),TEXT("Incomplete footprint/corridor support"));continue;}
   Approach=Start;ControlledVelocity=FVector(Direction*1000,0,0);Attempt->SetBoolField(TEXT("selected"),true);Fixture->SetObjectField(TEXT("new_floor"),FloorFacts(Floor));return true;
  }
  return false;
 }
 TSharedRef<FJsonObject> Sample(UWorld* W,ANeonCityNPCCharacter* N,UCoastalAmbientSeatComponent* Seat,ANeonCityNPCController* Controller)
 {
  auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("stage"),Stage);R->SetNumberField(TEXT("world_seconds"),W->GetTimeSeconds());R->SetNumberField(TEXT("elapsed_seconds"),W->GetTimeSeconds()-At);
  R->SetStringField(TEXT("actual_car_location"),Car->GetActorLocation().ToString());R->SetStringField(TEXT("actual_car_velocity"),Car->GetVelocity().ToString());
  R->SetStringField(TEXT("actual_car_rotation"),Car->GetActorRotation().ToString());R->SetNumberField(TEXT("collision_damage_count"),Car->GetDamageState()->GetCollisionDamageCount());
  R->SetNumberField(TEXT("collision_damage_delta"),Car->GetDamageState()->GetCollisionDamageCount()-DamageBefore);
  TArray<TSharedPtr<FJsonValue>> Wheels;auto* Movement=Car->GetChaosMovement();
  for(int32 I=0;I<Movement->WheelSetups.Num();++I){const auto& Status=Movement->GetWheelState(I);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("wheel"),I);J->SetBoolField(TEXT("in_contact"),Status.bInContact);J->SetStringField(TEXT("contact_point"),Status.ContactPoint.ToString());Wheels.Add(MakeShared<FJsonValueObject>(J));}R->SetArrayField(TEXT("native_wheel_contacts"),Wheels);
  TArray<FOverlapResult> ActualContacts;FComponentQueryParams CQ(TEXT("C921SeatCarActualContacts"),Car.Get());Car->GetMesh()->ComponentOverlapMulti(ActualContacts,W,Car->GetMesh()->GetComponentLocation(),Car->GetMesh()->GetComponentQuat(),ECC_Vehicle,CQ);
  TArray<TSharedPtr<FJsonValue>> ContactFacts;for(const auto& O:ActualContacts)if(O.bBlockingHit){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor"),GetPathNameSafe(O.GetActor()));J->SetStringField(TEXT("component"),GetPathNameSafe(O.GetComponent()));ContactFacts.Add(MakeShared<FJsonValueObject>(J));}R->SetArrayField(TEXT("native_chassis_blocking_contacts"),ContactFacts);
  R->SetStringField(TEXT("actual_npc_location"),N->GetActorLocation().ToString());R->SetStringField(TEXT("actual_npc_velocity"),N->GetVelocity().ToString());
  R->SetStringField(TEXT("actual_npc_rotation"),N->GetActorRotation().ToString());R->SetStringField(TEXT("state"),UEnum::GetValueAsString(N->GetNPCState()->GetState()));
  R->SetStringField(TEXT("reaction"),UEnum::GetValueAsString(Controller->GetReaction()));R->SetStringField(TEXT("attention_actor"),GetPathNameSafe(Controller->GetAttentionActor()));
  R->SetBoolField(TEXT("seat_active"),Seat->IsSeatedPresentationActive());R->SetNumberField(TEXT("seat_alpha"),Seat->GetSeatAlpha());R->SetNumberField(TEXT("movement_mode"),int32(N->GetCharacterMovement()->MovementMode));
  R->SetStringField(TEXT("last_native_movement_input"),N->GetLastMovementInputVector().ToString());R->SetStringField(TEXT("actual_acceleration"),N->GetCharacterMovement()->GetCurrentAcceleration().ToString());
  R->SetStringField(TEXT("controller_reaction_goal"),Controller->GetReactionGoal().ToString());R->SetStringField(TEXT("sensor_diagnostic"),Controller->GetVehicleReactionDiagnostic());
  R->SetNumberField(TEXT("npc_transform_or_state_or_event_writes"),0);SensorSamples.Add(MakeShared<FJsonValueObject>(R));return R;
 }
 FString CheckStanding(UWorld* W,ANeonCityNPCCharacter* N,const TSharedRef<FJsonObject>& R)
 {
  const auto* C=N->GetCapsuleComponent();const FVector Location=N->GetActorLocation();FCollisionQueryParams Q(SCENE_QUERY_STAT(C921SeatEvadeStanding),false,N);FHitResult Floor,Hit;
  const FCollisionShape Shape=FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()-2.f);
  const bool HasFloor=W->LineTraceSingleByChannel(Floor,Location+FVector(0,0,50),Location-FVector(0,0,180),ECC_Pawn,Q);
  const float FloorGap=HasFloor?Location.Z-Floor.ImpactPoint.Z-C->GetScaledCapsuleHalfHeight():MAX_flt;
  const bool FloorValid=HasFloor && Floor.ImpactNormal.Z>=N->GetCharacterMovement()->GetWalkableFloorZ() && FMath::Abs(FloorGap)<=48.f;
  const bool CapsuleClear=!W->OverlapBlockingTestByChannel(Location,N->GetActorQuat(),ECC_Pawn,Shape,Q);
  const bool PathClear=!HavePreviousStanding || !W->SweepSingleByChannel(Hit,PreviousStanding,Location,FQuat::Identity,ECC_Pawn,Shape,Q);
  const bool NativeGrounded=N->GetCharacterMovement()->IsMovingOnGround() && C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
  R->SetBoolField(TEXT("actual_floor_valid"),FloorValid);R->SetStringField(TEXT("actual_floor_actor"),GetPathNameSafe(Floor.GetActor()));R->SetNumberField(TEXT("actual_floor_gap_cm"),FloorGap);R->SetNumberField(TEXT("actual_floor_normal_z"),Floor.ImpactNormal.Z);
  R->SetBoolField(TEXT("actual_capsule_clear"),CapsuleClear);R->SetBoolField(TEXT("actual_post_stand_segment_clear"),PathClear);R->SetStringField(TEXT("segment_blocker"),GetPathNameSafe(Hit.GetActor()));
  R->SetBoolField(TEXT("actual_native_grounded_with_collision"),NativeGrounded);R->SetNumberField(TEXT("actual_capsule_collision_mode"),int32(C->GetCollisionEnabled()));
  R->SetNumberField(TEXT("capsule_radius_cm"),C->GetScaledCapsuleRadius());R->SetNumberField(TEXT("capsule_halfheight_cm"),C->GetScaledCapsuleHalfHeight());
  if(!CarBody.IsValid())return TEXT("Actual fitted vehicle body disappeared during reaction");
  // Project the actual fitted body's world bounds onto the approach lateral
  // axis, then include the civilian's actual capsule radius. This tests the
  // car's swept lane even while the controlled car is stopped longitudinally.
  const FVector Side(-ControlledVelocity.Y,ControlledVelocity.X,0);const FVector Axis=Side.GetSafeNormal2D();const auto Bounds=CarBody->Bounds;
  const float HalfWidth=FMath::Abs(Axis.X)*Bounds.BoxExtent.X+FMath::Abs(Axis.Y)*Bounds.BoxExtent.Y;
  LateralClearance=FMath::Abs(FVector::DotProduct(Location-Bounds.Origin,Axis))-HalfWidth-C->GetScaledCapsuleRadius();
  R->SetStringField(TEXT("actual_car_body_bounds_origin"),Bounds.Origin.ToString());R->SetStringField(TEXT("actual_car_body_bounds_extent"),Bounds.BoxExtent.ToString());
  R->SetNumberField(TEXT("projected_car_body_half_width_cm"),HalfWidth);R->SetNumberField(TEXT("actual_car_body_plus_capsule_lateral_clearance_cm"),LateralClearance);
  if(HavePreviousStanding)PostStandPath+=FVector::Dist2D(PreviousStanding,Location);PreviousStanding=Location;HavePreviousStanding=true;
  if(!FloorValid || !CapsuleClear || !PathClear || !NativeGrounded)return TEXT("Real standing avoidance lost walkable support or crossed world collision");
  return FString();
 }
 bool End(const FString& Error=FString())
 {
  if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetBoolField(TEXT("react_preserved_after_standing"),ReactPreserved);J->SetNumberField(TEXT("actual_post_stand_evade_cm"),Moved);
  J->SetBoolField(TEXT("actual_sensor_entered_vehicle_reaction"),HaveReaction);J->SetStringField(TEXT("original_seated_capsule_before_sensor_entry"),SeatedStart.ToString());J->SetStringField(TEXT("capsule_at_reaction_observation"),ReactionEntry.ToString());J->SetStringField(TEXT("actual_controller_reaction_goal"),Goal.ToString());
  J->SetStringField(TEXT("actual_first_completed_standing_pose"),StandExit.ToString());J->SetBoolField(TEXT("standing_exit_observed"),HaveStart);J->SetNumberField(TEXT("seat_to_stand_exit_cm"),HaveStart?FVector::Dist2D(SeatedStart,StandExit.GetLocation()):0);
  J->SetNumberField(TEXT("actual_whole_reaction_displacement_cm"),TotalMoved);J->SetNumberField(TEXT("actual_post_stand_path_cm"),PostStandPath);J->SetNumberField(TEXT("post_stand_native_moving_input_samples"),MovingInputSamples);
  J->SetNumberField(TEXT("actual_goal_distance_cm"),GoalDistance);J->SetNumberField(TEXT("native_goal_arrival_radius_cm"),GoalArrivalCm);J->SetBoolField(TEXT("actual_goal_arrived_while_reacting"),GoalArrived);J->SetNumberField(TEXT("actual_car_body_plus_capsule_lateral_clearance_cm"),LateralClearance);
  J->SetNumberField(TEXT("npc_transform_or_state_writes"),0);J->SetNumberField(TEXT("player_initial_fixtures"),PlayerFixture?1:0);J->SetNumberField(TEXT("chaos_vehicle_initial_fixtures"),CarFixture?1:0);
  J->SetNumberField(TEXT("npc_event_writes"),0);J->SetStringField(TEXT("controlled_car_velocity_cm_s"),ControlledVelocity.ToString());J->SetStringField(TEXT("actual_car_body_component"),GetPathNameSafe(CarBody.Get()));
  J->SetArrayField(TEXT("sensor_samples"),SensorSamples);
  Fixture->SetArrayField(TEXT("approach_candidates"),FixtureAttempts);J->SetObjectField(TEXT("fixture"),Fixture);
  if(Marker.IsValid())if(auto* NPC=Marker->GetSpawnedPedestrian())
  {if(const auto* Seat=NPC->FindComponentByClass<UCoastalAmbientSeatComponent>())J->SetStringField(TEXT("seat_diagnostic"),Seat->GetSeatingDiagnostic());if(const auto* Controller=Cast<ANeonCityNPCController>(NPC->GetController()))J->SetStringField(TEXT("sensor_diagnostic"),Controller->GetVehicleReactionDiagnostic());}
  J->SetStringField(TEXT("scope"),TEXT("Minimal real-sensor seated vehicle reaction: actual parked Chaos car temporarily approaches on supported quay with declared controlled velocity; ordinary Sense/FindSafeDodge/React, checked standing exit, then native movement input. Require whole seated-origin avoidance >=80cm, separate genuine post-standing movement, real controller goal arrival <35cm while React/EvadeVehicle is preserved, walkable floor, clear capsule and world segments, and projected fitted-car-body plus capsule lateral clearance. Every Stage2/3 frame is sampled. No NPC pose/state/event writes. Fixtures restored. This is not continuous driving."));
  if(CarFixture && Car.IsValid()){Car->SetActorTransform(CarBefore,false,nullptr,ETeleportType::TeleportPhysics);Car->GetMesh()->SetPhysicsLinearVelocity(LinearBefore);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(AngularBefore);}
  if(PlayerFixture && Player.IsValid())Player->SetActorTransform(PlayerBefore,false,nullptr,ETeleportType::TeleportPhysics);
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label.IsEmpty()?TEXT("C921SeatVehicle"):Label);IFileManager::Get().MakeDirectory(*Dir,true);
  FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("seat-vehicle-react921.json")));return true;
 }
public:
 explicit FSeatedVehicleEvade(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Began>90)return End(TEXT("Seated vehicle reaction minimum repro timed out"));
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!W || !P)return false;World=W;
  if(Stage==0)
  {
   for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->bAmbientEnabled && I->AmbientActivity==ECoastalAmbientActivity::CafeConversation
    && I->AmbientConversationGroup==FName(TEXT("C921.Cafe.Umbrella01")))
    if(!Marker.IsValid() || I->GetActorLocation().X<Marker->GetActorLocation().X)Marker=*I;
   FGuid ExpectedCar;FGuid::ParseExact(TEXT("0889167F430338D9839BAEB8F5500BB3"),EGuidFormats::Digits,ExpectedCar);
   for(TActorIterator<AIslandSupercar> I(W);I;++I)if(I->GetLevel()==W->PersistentLevel && I->VehicleID==ExpectedCar && I->bRoadster && !I->GetController()){Car=*I;break;}
   if(!Marker.IsValid() || !Car.IsValid() || !IsValid(Marker->AmbientSeatSupport.Get()))return End(TEXT("Actual authored cafe chair/person and unoccupied mainland roadster required"));
   Fixture->SetStringField(TEXT("car_actor"),Car->GetPathName());Fixture->SetStringField(TEXT("car_guid"),Car->VehicleID.ToString(EGuidFormats::Digits));Fixture->SetStringField(TEXT("car_profile"),GetPathNameSafe(Car->GetVehicleProfile()));
   if(!Car->GetVehicleProfile() || Car->GetVehicleProfile()->GetFName()!=TEXT("DA_HeroKestrel"))return End(TEXT("Stable mainland Kestrel fixture identity has unexpected profile"));
   Player=P;PlayerBefore=P->GetActorTransform();FVector Point=Marker->GetActorLocation()+FVector(0,-220,0);FCollisionQueryParams Q(SCENE_QUERY_STAT(C921SeatVehiclePlayer),false,P);FHitResult H;
   if(!W->LineTraceSingleByChannel(H,Point+FVector(0,0,100),Point-FVector(0,0,300),ECC_Pawn,Q) || H.ImpactNormal.Z<.94)return End(TEXT("Vehicle-reaction observer has no actual floor"));
   Point.Z=H.ImpactPoint.Z+P->GetSimpleCollisionHalfHeight()+2;
   if(W->OverlapBlockingTestByChannel(Point,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()-1),Q))return End(TEXT("Vehicle-reaction observer capsule blocked"));
   P->SetActorLocation(Point,false,nullptr,ETeleportType::TeleportPhysics);PlayerFixture=true;At=W->GetTimeSeconds();Stage=1;return false;
  }
  if(!Marker.IsValid() || !Car.IsValid())return End(TEXT("Authored cafe identity or actual car disappeared during fixture"));
  auto* N=Marker->GetSpawnedPedestrian();auto* Seat=N?N->FindComponentByClass<UCoastalAmbientSeatComponent>():nullptr;auto* Controller=N?Cast<ANeonCityNPCController>(N->GetController()):nullptr;
  if(Stage==1)
  {
   if(!Seat || !Controller || !Seat->IsSeatedPresentationActive() || Seat->GetSeatAlpha()<.99f){if(W->GetTimeSeconds()-At>18)return End(TEXT("Actual cafe person failed to reach a genuine seated pose"));return false;}
   TInlineComponentArray<UStaticMeshComponent*> Parts(Car.Get());for(auto* Part:Parts)if(Part->GetFName()==TEXT("ArtVisual") && Part->GetStaticMesh() && Part->IsVisible()){CarBody=Part;break;}
   if(!CarBody.IsValid())return End(TEXT("Actual fitted roadster body bounds required for lane clearance"));
   SeatedStart=N->GetActorLocation();
   if(N->GetThreatComponent()->HasActiveThreat() || N->GetNPCState()->GetState()!=ENeonCityNPCState::Social)return End(TEXT("Cafe person already has another genuine threat before the vehicle fixture"));
   CarBefore=Car->GetActorTransform();LinearBefore=Car->GetMesh()->GetPhysicsLinearVelocity();AngularBefore=Car->GetMesh()->GetPhysicsAngularVelocityInDegrees();DamageBefore=Car->GetDamageState()->GetCollisionDamageCount();
   Fixture->SetStringField(TEXT("car_before_pose"),CarBefore.ToString());Fixture->SetStringField(TEXT("car_before_linear_velocity"),LinearBefore.ToString());Fixture->SetStringField(TEXT("car_before_angular_velocity"),AngularBefore.ToString());Fixture->SetNumberField(TEXT("collision_damage_before"),DamageBefore);
   FCollisionQueryParams Q(SCENE_QUERY_STAT(C921SeatCarFloor),false,Car.Get());FHitResult OldFloor;
   W->LineTraceSingleByChannel(OldFloor,CarBefore.GetLocation(),CarBefore.GetLocation()-FVector(0,0,300),ECC_Pawn,Q);Fixture->SetObjectField(TEXT("old_floor"),FloorFacts(OldFloor));
   if(!OldFloor.bBlockingHit || OldFloor.ImpactNormal.Z<.94f)return End(TEXT("Controlled approaching car fixture lacks actual original parking support"));
   const FVector Hip=Marker->GetActorTransform().TransformPosition(Marker->AmbientSeatHipLocal);
   if(!PlanApproach(W,Hip,OldFloor))return End(TEXT("Neither approach has a clear fitted-body envelope and fully supported stimulus corridor"));
   Car->SetActorLocationAndRotation(Approach,FRotator(0,ControlledVelocity.X>0?0:180,0),false,nullptr,ETeleportType::TeleportPhysics);CarFixture=true;Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);At=W->GetTimeSeconds();Stage=2;return false;
  }
  if(!N || !Seat || !Controller || !Car.IsValid())return End(TEXT("Cafe identity/controller or actual car disappeared during vehicle reaction"));
  if(Stage==2)
  {
   const auto R=Sample(W,N,Seat,Controller);
   if(Car->GetDamageState()->GetCollisionDamageCount()!=DamageBefore)return End(TEXT("Controlled vehicle stimulus collided before genuine sensor entry; inspect fixture contacts"));
   if(N->GetThreatComponent()->HasActiveThreat())return End(TEXT("Another genuine threat interrupted seating before the vehicle sensor; inspect fixture contacts"));
   if(Controller->GetReaction()==ENeonCityNPCReaction::EvadeVehicle && Controller->GetAttentionActor()==Car.Get())
   {
    HaveReaction=true;ReactionEntry=N->GetActorLocation();Goal=Controller->GetReactionGoal();R->SetStringField(TEXT("original_seated_capsule_before_sensor_entry"),SeatedStart.ToString());
    R->SetBoolField(TEXT("real_sensor_entry_observed"),true);R->SetNumberField(TEXT("actual_goal_from_seated_origin_cm"),FVector::Dist2D(SeatedStart,Goal));
    if(N->GetNPCState()->GetState()!=ENeonCityNPCState::React || !Seat->IsSeatedPresentationActive() || Goal.ContainsNaN())return End(TEXT("Vehicle evasion did not enter through a real seated React state with a finite goal"));
    Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);At=W->GetTimeSeconds();Stage=3;return false;
   }
   if(Seat->IsSeatedPresentationActive() && Seat->GetSeatAlpha()>=.99f)SeatedStart=N->GetActorLocation();
   if(W->GetTimeSeconds()-At>.6f){Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);return End(TEXT("Real sensor did not enter vehicle evasion while seated; inspect actual chair initial-overlap/FindSafeDodge, do not inject React"));}
   Car->GetMesh()->SetPhysicsLinearVelocity(ControlledVelocity);return false;
  }
  if(Stage==3)
  {
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
   const auto R=Sample(W,N,Seat,Controller);const bool IsReacting=N->GetNPCState()->GetState()==ENeonCityNPCState::React && Controller->GetReaction()==ENeonCityNPCReaction::EvadeVehicle && Controller->GetAttentionActor()==Car.Get();
   R->SetBoolField(TEXT("real_vehicle_react_preserved"),IsReacting);R->SetNumberField(TEXT("actual_goal_distance_cm"),FVector::Dist2D(N->GetActorLocation(),Goal));
   if(!IsReacting)return End(TEXT("Vehicle React/EvadeVehicle handoff expired or was overwritten before genuine goal arrival"));
   if(!Controller->GetReactionGoal().Equals(Goal,.1f))return End(TEXT("Production reaction goal changed during seated avoidance"));
   if(Seat->IsSeatedPresentationActive()){if(W->GetTimeSeconds()-At>4)return End(TEXT("Vehicle-react person could not complete checked standing exit"));return false;}
   if(!HaveStart)
   {ReactPreserved=IsReacting;StandExit=N->GetActorTransform();HaveStart=true;R->SetBoolField(TEXT("first_completed_standing_pose"),true);}
   Moved=FMath::Max(Moved,float(FVector::Dist2D(StandExit.GetLocation(),N->GetActorLocation())));TotalMoved=FVector::Dist2D(SeatedStart,N->GetActorLocation());GoalDistance=FVector::Dist2D(N->GetActorLocation(),Goal);
   const FString StandingError=CheckStanding(W,N,R);if(!StandingError.IsEmpty())return End(StandingError);
   if(Moved>1.f && N->GetLastMovementInputVector().Size2D()>.1f && N->GetVelocity().Size2D()>1.f)++MovingInputSamples;
   GoalArrived=GoalDistance<GoalArrivalCm;R->SetNumberField(TEXT("actual_post_stand_displacement_cm"),Moved);R->SetNumberField(TEXT("actual_whole_reaction_displacement_cm"),TotalMoved);R->SetNumberField(TEXT("actual_post_stand_path_cm"),PostStandPath);R->SetNumberField(TEXT("post_stand_native_moving_input_samples"),MovingInputSamples);R->SetBoolField(TEXT("actual_goal_arrived"),GoalArrived);
   if(GoalArrived && TotalMoved>=MinimumAvoidanceCm && Moved>1.f && MovingInputSamples>=2 && LateralClearance>=0.f)
   {if(Car->GetDamageState()->GetCollisionDamageCount()!=DamageBefore)return End(TEXT("Controlled vehicle fixture collided and changed the stimulus"));return End();}
   if(W->GetTimeSeconds()-At>4)return End(TEXT("Real avoidance failed whole displacement, native goal arrival, continued movement input, or car-body lateral clearance"));return false;
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalPopulation921,"NeonCity.Phase205c.MainlandC921.Standalone.CafePopulation",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalPopulation921::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalPopulation921::FObserve(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalForegroundCafeWest923,"NeonCity.Phase205c.MainlandC923.Standalone.Foreground18CafeWest",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalForegroundCafeWest923::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalPopulation921::FObserve(this,FName(TEXT("C923.Cafe.Foreground18.West"))));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalForegroundCafeEast923,"NeonCity.Phase205c.MainlandC923.Standalone.Foreground18CafeEast",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalForegroundCafeEast923::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalPopulation921::FObserve(this,FName(TEXT("C923.Cafe.Foreground18.East"))));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalCafe52Table0,"NeonCity.Phase205c.MainlandC923.Standalone.Cafe52Table0Lifecycle",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalCafe52Table0::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalPopulation921::FObserve(this,FName(TEXT("C923.Cafe52.Table0"))));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalCafe52Table1,"NeonCity.Phase205c.MainlandC923.Standalone.Cafe52Table1Lifecycle",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalCafe52Table1::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalPopulation921::FObserve(this,FName(TEXT("C923.Cafe52.Table1"))));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalSeatedVehicleEvade921,"NeonCity.Phase205c.MainlandC921.Standalone.SeatedVehicleEvade",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCoastalSeatedVehicleEvade921::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(CoastalPopulation921::FSeatedVehicleEvade(this));return true;}
#endif
