#include "AI/CoastalAmbientSeatComponent.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

UCoastalAmbientSeatComponent::UCoastalAmbientSeatComponent(){PrimaryComponentTick.bCanEverTick=false;}

FString UCoastalAmbientSeatComponent::GetSeatingDiagnostic()const
{
 const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());const auto* Marker=NPC?NPC->GetSpawnPoint():nullptr;
 return FString::Printf(TEXT("gate=%s active=%d standing=%d alpha=%.3f cooldown=%.3f attempts=%u failures=%u entries=%u state=%s movement=%d distance_to_marker_cm=%.3f support=%s animation_class=%s skin=%s driver=%s admission_failure=[%s] exit_failure=[%s] path_failure=[%s]"),
  *AdmissionGate.ToString(),bActive,bStanding,Blend,Cooldown,StartAttempts,StartFailures,CompletedEntries,
  NPC?*UEnum::GetValueAsString(NPC->GetNPCState()->GetState()):TEXT("NoNPC"),NPC?int32(NPC->GetCharacterMovement()->MovementMode):-1,
  NPC && Marker?FVector::Dist2D(NPC->GetActorLocation(),Marker->GetActorLocation()):-1.,
  *GetPathNameSafe(Marker?Marker->AmbientSeatSupport.Get():nullptr),*GetPathNameSafe(Marker?Marker->AmbientAnimationClass.Get():nullptr),
  *GetPathNameSafe(NPC?NPC->GetMesh()->GetSkeletalMeshAsset():nullptr),*GetPathNameSafe(Occupant?Occupant->GetDriverMesh():nullptr),
  *LastAdmissionFailure,*LastExitFailure,*LastPathFailure);
}

bool UCoastalAmbientSeatComponent::IsStandingPathClear(FVector Start,FVector End)const
{
 const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());const auto* Marker=NPC?NPC->GetSpawnPoint():nullptr;
 if(!NPC || !Marker || !IsValid(Marker->AmbientSeatSupport.Get()) || Start.ContainsNaN() || End.ContainsNaN()){LastPathFailure=TEXT("Missing NPC/marker/support or nonfinite endpoints");return false;}
 const auto* Capsule=NPC->GetCapsuleComponent();FCollisionQueryParams Q(SCENE_QUERY_STAT(CoastalCafeDeparture),false,NPC);Q.AddIgnoredActor(Marker->AmbientSeatSupport.Get());
 FHitResult H;if(GetWorld()->SweepSingleByChannel(H,Start,End,FQuat::Identity,ECC_Pawn,
  FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-1),Q))
 {LastPathFailure=FString::Printf(TEXT("capsule hit actor=%s component=%s initial_overlap=%d penetration=%.3f start=%s end=%s"),*GetPathNameSafe(H.GetActor()),*GetPathNameSafe(H.GetComponent()),H.bStartPenetrating,H.PenetrationDepth,*Start.ToString(),*End.ToString());return false;}
 const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,End)/50));
 for(int32 I=0;I<=Steps;++I)
 {
  const FVector P=FMath::Lerp(Start,End,float(I)/Steps);
  const bool Hit=GetWorld()->LineTraceSingleByChannel(H,P+FVector(0,0,50),P-FVector(0,0,180),ECC_Pawn,Q);
  if(!Hit || H.ImpactNormal.Z<NPC->GetCharacterMovement()->GetWalkableFloorZ()
   || FMath::Abs(P.Z-H.ImpactPoint.Z-Capsule->GetScaledCapsuleHalfHeight())>48)
  {LastPathFailure=FString::Printf(TEXT("floor sample=%d/%d hit=%d actor=%s point=%s normal_z=%.3f root_floor_clearance=%.3f expected_halfheight=%.3f"),I,Steps,Hit,*GetPathNameSafe(H.GetActor()),*P.ToString(),H.ImpactNormal.Z,P.Z-H.ImpactPoint.Z,Capsule->GetScaledCapsuleHalfHeight());return false;}
 }
 LastPathFailure.Reset();return true;
}

bool UCoastalAmbientSeatComponent::GetPlannedStandingExit(FTransform& Exit)const
{
 if(!bActive)return false;
 if(!bExitLocked)return FindStandingExit(Exit);
 const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());if(!NPC)return false;
 const auto* C=NPC->GetCapsuleComponent();FCollisionQueryParams Q(SCENE_QUERY_STAT(CoastalCafePlannedExit),false,NPC);
 if(GetWorld()->OverlapBlockingTestByChannel(LockedExit.GetLocation(),LockedExit.GetRotation(),ECC_Pawn,
  FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()-1),Q)
  || !IsStandingPathClear(NPC->GetActorLocation(),LockedExit.GetLocation()))return false;
 Exit=LockedExit;return true;
}

bool UCoastalAmbientSeatComponent::FindStandingExit(FTransform& Exit)const
{
 const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());if(!NPC)return false;
 const auto* C=NPC->GetCapsuleComponent();FCollisionQueryParams Q(SCENE_QUERY_STAT(CoastalCafeStand),false,NPC);LastExitFailure.Reset();int32 Index=0;
 for(const FVector Local:{FVector::ZeroVector,FVector(0,110,0),FVector(0,-110,0),FVector(-110,0,0)})
 {
  const int32 CandidateIndex=Index++;const FVector P=Entry.TransformPosition(Local);FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByChannel(Floor,P+FVector(0,0,120),P-FVector(0,0,220),ECC_Pawn,Q)
   || Floor.ImpactNormal.Z<NPC->GetCharacterMovement()->GetWalkableFloorZ())
  {LastExitFailure+=FString::Printf(TEXT("candidate%d floor invalid actor=%s normal=%.3f; "),CandidateIndex,*GetPathNameSafe(Floor.GetActor()),Floor.ImpactNormal.Z);continue;}
  const FVector Stand=Floor.ImpactPoint+FVector(0,0,C->GetScaledCapsuleHalfHeight()+2);
  if(GetWorld()->OverlapBlockingTestByChannel(Stand,Entry.GetRotation(),ECC_Pawn,
   FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()-1),Q))
  {LastExitFailure+=FString::Printf(TEXT("candidate%d endpoint blocked at %s; "),CandidateIndex,*Stand.ToString());continue;}
  if(!IsStandingPathClear(NPC->GetActorLocation(),Stand)){LastExitFailure+=FString::Printf(TEXT("candidate%d path [%s]; "),CandidateIndex,*LastPathFailure);continue;}
  LastExitFailure.Reset();
  Exit=FTransform(Entry.GetRotation(),Stand,Entry.GetScale3D());return true;
 }
 return false;
}

bool UCoastalAmbientSeatComponent::StartSeating(ANeonCityNPCCharacter& NPC,APedestrianSpawnPoint& Marker)
{
 ++StartAttempts;
 const auto Fail=[this](const FString& Reason){++StartFailures;LastAdmissionFailure=Reason;return false;};
 if(!IsValid(Marker.AmbientSeatSupport.Get()))return Fail(TEXT("AmbientSeatSupport missing or invalid"));
 Entry=NPC.GetActorTransform();FTransform Exit;
 if(!FindStandingExit(Exit))return Fail(TEXT("No checked standing exit"));
 if(!Occupant)
 {
  Occupant=NewObject<UTransportOccupantComponent>(&NPC,TEXT("CoastalCafeVisiblePerson"));
  NPC.AddInstanceComponent(Occupant);Occupant->RegisterComponent();
  Occupant->SeatedAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_Sitting_Idle_Loop.AN_NC205C_Sitting_Idle_Loop"));
 }
 if(!Occupant->SeatedAnimation || !Marker.AmbientAnimationClass)return Fail(TEXT("Missing production seated animation or owned ambient class"));
 NPC.GetMesh()->TickAnimation(0,false);NPC.GetMesh()->RefreshBoneTransforms();
 StandingHipWorld=NPC.GetMesh()->GetSocketLocation(TEXT("pelvis"));
 SeatHipWorld=Marker.GetActorTransform().TransformPosition(Marker.AmbientSeatHipLocal);
 if(SeatHipWorld.ContainsNaN() || FVector::Dist2D(Entry.GetLocation(),SeatHipWorld)>180)return Fail(FString::Printf(TEXT("Nonfinite/out-of-range measured seat hip entry=%s hip=%s"),*Entry.GetLocation().ToString(),*SeatHipWorld.ToString()));
 bMeshVisible=NPC.GetMesh()->IsVisible();bMeshTick=NPC.GetMesh()->IsComponentTickEnabled();
 CollisionBefore=NPC.GetCapsuleComponent()->GetCollisionEnabled();
 MovementBefore=NPC.GetCharacterMovement()->MovementMode;CustomMovementBefore=NPC.GetCharacterMovement()->CustomMovementMode;
 NPC.GetCharacterMovement()->StopMovementImmediately();NPC.GetCharacterMovement()->DisableMovement();
 NPC.GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 // The seated query capsule remains with the actual person. The visible body
 // blends from the previous standing hip to the measured chair, rather than
 // parking an invisible standing collider a metre away from the person.
 NPC.SetActorLocationAndRotation(SeatHipWorld+FVector(0,0,50),Marker.GetActorRotation(),false,nullptr,ETeleportType::TeleportPhysics);
 Occupant->SeatHipLocation=FVector(0,0,-50);Occupant->SeatYaw=0;Occupant->SeatedAnimClass=Marker.AmbientAnimationClass;
 SeatRoot=NPC.GetActorTransform();Blend=0;bActive=true;bStanding=false;bExitLocked=false;SeatedSeconds=0;
 if(!Occupant->ShowDriver(&NPC)){Restore(Entry,true);return Fail(TEXT("Production ShowDriver rejected skin/animation compatibility"));}
 Occupant->GetDriverMesh()->TickAnimation(0,false);Occupant->GetDriverMesh()->RefreshBoneTransforms();
 NPC.GetMesh()->SetVisibility(false,true);NPC.GetMesh()->SetComponentTickEnabled(false);
 NPC.GetNPCState()->SetState(ENeonCityNPCState::Social);
 if(Marker.AmbientCupMesh)
 {
  if(!Cup){Cup=NewObject<UStaticMeshComponent>(&NPC,TEXT("CoastalCafeCup"));NPC.AddInstanceComponent(Cup);Cup->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cup->SetCanEverAffectNavigation(false);Cup->SetGenerateOverlapEvents(false);Cup->RegisterComponent();}
  Cup->SetStaticMesh(Marker.AmbientCupMesh);Cup->AttachToComponent(Occupant->GetDriverMesh(),FAttachmentTransformRules::KeepRelativeTransform,TEXT("hand_r"));Cup->SetRelativeTransform(Marker.AmbientCupGripLocal);Cup->SetVisibility(true);
 }
 LastAdmissionFailure.Reset();++CompletedEntries;FitBody();return true;
}

void UCoastalAmbientSeatComponent::FitBody()
{
 if(!Occupant || !Occupant->GetDriverMesh())return;
 auto* Body=Occupant->GetDriverMesh();const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());if(!NPC)return;
 const FVector Hip=Body->GetSocketTransform(TEXT("pelvis"),RTS_Component).GetLocation();
 const FVector Target=NPC->GetActorTransform().InverseTransformPosition(FMath::Lerp(bStanding && bExitLocked?ExitHipWorld:StandingHipWorld,SeatHipWorld,FMath::SmoothStep(0.f,1.f,Blend)));
 Body->SetRelativeLocation(Target-Body->GetRelativeRotation().RotateVector(Hip));
 if(Cup)Cup->SetVisibility(Blend>.95f && !bStanding);
}

void UCoastalAmbientSeatComponent::Restore(const FTransform& Exit,bool RestoreMovement)
{
 auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());if(!NPC)return;
 StowCup();
 if(Occupant)Occupant->HideDriver();
 NPC->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 NPC->SetActorTransform(Exit,false,nullptr,ETeleportType::TeleportPhysics);
 NPC->GetCapsuleComponent()->SetCollisionEnabled(CollisionBefore);
 NPC->GetMesh()->SetVisibility(bMeshVisible,true);NPC->GetMesh()->SetComponentTickEnabled(bMeshTick);
 if(RestoreMovement)
 {
  // The damage component may have captured MOVE_None while this seat owned
  // locomotion. Its existing API updates the deferred recovery mode while
  // keeping downed/dead/physical-lock movement disabled.
  if(auto* Body=NPC->FindComponentByClass<UDamageableCharacterComponent>())Body->RestoreLocomotionMode(MovementBefore,CustomMovementBefore);
  else NPC->GetCharacterMovement()->SetMovementMode(MovementBefore,CustomMovementBefore);
 }
 bActive=false;bStanding=false;bExitLocked=false;Blend=0;Cooldown=12.f;
}

void UCoastalAmbientSeatComponent::StowCup()
{
 if(!Cup)return;
 // HideDriver clears its skeletal mesh. Release the hand socket first so a
 // hidden cup cannot keep querying a now empty presentation skeleton.
 Cup->SetVisibility(false);Cup->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);Cup->SetComponentTickEnabled(false);
}

bool UCoastalAmbientSeatComponent::UpdateSeating(float DeltaSeconds,bool Interrupt)
{
 auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());auto* Marker=NPC?NPC->GetSpawnPoint():nullptr;
 if(!NPC || !Marker || Marker->AmbientActivity!=ECoastalAmbientActivity::CafeConversation)return false;
 const float Delta=FMath::Clamp(DeltaSeconds,0.f,.1f);
 if(!bActive)
 {
  Cooldown=FMath::Max(0.f,Cooldown-Delta);
  if(Interrupt){AdmissionGate=TEXT("ExternalInterrupt");return false;}
  if(Cooldown>0){AdmissionGate=TEXT("Cooldown");return false;}
  if(NPC->GetNPCState()->GetState()==ENeonCityNPCState::React){AdmissionGate=TEXT("ExternalReact");return false;}
  if(FVector::Dist2D(NPC->GetActorLocation(),Marker->GetActorLocation())>45){AdmissionGate=TEXT("ReturningToEntry");return false;}
  if(!NPC->GetCharacterMovement()->IsMovingOnGround()){AdmissionGate=TEXT("NotGrounded");return false;}
  if(!StartSeating(*NPC,*Marker)){AdmissionGate=TEXT("StartSeatingFailed");Cooldown=5;return false;}
 }
 AdmissionGate=TEXT("Active");
 SeatedSeconds+=Delta;
 bStanding|=Interrupt || SeatedSeconds>=FMath::Clamp(Marker->AmbientSeatSeconds,10.f,120.f);
 if(bStanding && !bExitLocked)
 {
  if(!FindStandingExit(LockedExit)){FitBody();return true;}
  ExitHipWorld=LockedExit.TransformPosition(Entry.InverseTransformPosition(StandingHipWorld));bExitLocked=true;
 }
 float TargetBlend=bStanding?0.f:1.f;
 if(bStanding)
 {
  // Keep one checked destination. A newly occupied path returns toward the
  // original seat only when that retreat is clear; it never swaps exits or
  // drives the visible body through a player while its capsule stays behind.
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CoastalCafeLockedExit),false,NPC);
  const auto* C=NPC->GetCapsuleComponent();const bool EndClear=!GetWorld()->OverlapBlockingTestByChannel(LockedExit.GetLocation(),LockedExit.GetRotation(),ECC_Pawn,
   FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()-1),Q);
  if(!EndClear || !IsStandingPathClear(NPC->GetActorLocation(),LockedExit.GetLocation()))
   TargetBlend=IsStandingPathClear(NPC->GetActorLocation(),SeatRoot.GetLocation())?1.f:Blend;
 }
 const float NextBlend=FMath::FInterpConstantTo(Blend,TargetBlend,Delta,1.f/1.2f);
 if(bStanding)
 {
  const float Progress=1-FMath::SmoothStep(0.f,1.f,NextBlend);const FVector Next=FMath::Lerp(SeatRoot.GetLocation(),LockedExit.GetLocation(),Progress);
  if(!IsStandingPathClear(NPC->GetActorLocation(),Next)){FitBody();return true;}
  // This small animation-driven kinematic step was just swept against the
  // real world, including players; only the explicit own chair is ignored.
  NPC->SetActorLocationAndRotation(Next,FQuat::Slerp(SeatRoot.GetRotation(),LockedExit.GetRotation(),Progress),false,nullptr,ETeleportType::None);
 }
 Blend=NextBlend;
 FitBody();
 if(bStanding && Blend<=KINDA_SMALL_NUMBER)
 {
  const bool NormalSocialExpiry=!Interrupt && NPC->GetNPCState()->GetState()==ENeonCityNPCState::Social
   && SeatedSeconds>=FMath::Clamp(Marker->AmbientSeatSeconds,10.f,120.f);
  Restore(LockedExit,true);
  if(NormalSocialExpiry)NPC->GetNPCState()->SetState(ENeonCityNPCState::Return);
  return false;
 }
 return true;
}

void UCoastalAmbientSeatComponent::CancelForIncapacity()
{
 if(!bActive)return;
 // Damage/physics keeps the actual seated civilian position, not a forced
 // safe-exit relocation. The existing damage system takes over their body.
 Restore(GetOwner()->GetActorTransform(),true);
}
void UCoastalAmbientSeatComponent::EndPlay(EEndPlayReason::Type Reason)
{StowCup();if(bActive && Occupant)Occupant->HideDriver();Super::EndPlay(Reason);}
