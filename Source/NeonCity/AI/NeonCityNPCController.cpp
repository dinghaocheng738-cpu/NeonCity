#include "AI/NeonCityNPCController.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCData.h"
#include "AI/NPCThreatComponent.h"
#include "Vehicles/VehicleInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/CoastalAmbientSeatComponent.h"

ANeonCityNPCController::ANeonCityNPCController() { PrimaryActorTick.bCanEverTick = true; }
void ANeonCityNPCController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Pedestrian = Cast<ANeonCityNPCCharacter>(InPawn);
	if (Pedestrian.IsValid())
	{
		Random.Initialize(Pedestrian->GetBehaviorSeed());
		IdleDuration = FMath::Max(1.f,Pedestrian->GetNPCData()->IdleSeconds) * Random.FRandRange(0.7f,1.3f);
		if(const auto* Marker=Pedestrian->GetSpawnPoint();Marker && Marker->AmbientActivity!=ECoastalAmbientActivity::Legacy)
		{
			IdleDuration=FMath::Clamp(Marker->AmbientIdleSeconds,2.f,40.f)*Random.FRandRange(.85f,1.15f);
			// The real spawn is already at waypoint zero. After its initial rest,
			// an authored stroller starts walking instead of resting there twice.
			if(Marker->AmbientActivity==ECoastalAmbientActivity::Stroll && Pedestrian->GetRoute().Num()>1)RouteIndex=1;
		}
		LookYaw = Pedestrian->GetActorRotation().Yaw;
		Pedestrian->GetCharacterMovement()->AddTickPrerequisiteActor(this);
	}
}
void ANeonCityNPCController::OnUnPossess()
{
	if (Pedestrian.IsValid()) { Pedestrian->GetCharacterMovement()->RemoveTickPrerequisiteActor(this); }
	Pedestrian.Reset(); SocialPartner.Reset(); Attention.Reset();
	Super::OnUnPossess();
}
void ANeonCityNPCController::LookAt(FVector Point, float DeltaSeconds)
{
	const FRotator Desired(0.f,(Point - Pedestrian->GetActorLocation()).Rotation().Yaw,0.f);
	Pedestrian->SetActorRotation(FMath::RInterpConstantTo(Pedestrian->GetActorRotation(),Desired,DeltaSeconds,160.f));
}
bool ANeonCityNPCController::MoveToPoint(FVector Point)
{
	FVector Delta = Point - Pedestrian->GetActorLocation();
	Delta.Z = 0.f;
	if (Delta.SizeSquared() < FMath::Square(PointArrivalRadius)) { return true; }
	Pedestrian->AddMovementInput(Delta.GetSafeNormal(),1.f);
	return false;
}
bool ANeonCityNPCController::TrySocial()
{
	const auto* Marker=Pedestrian->GetSpawnPoint();
	const bool Authored=Marker && Marker->AmbientActivity!=ECoastalAmbientActivity::Legacy;
	if(Authored && (Marker->AmbientActivity!=ECoastalAmbientActivity::CafeConversation || Marker->AmbientConversationGroup.IsNone()))return false;
	for (TActorIterator<ANeonCityNPCCharacter> It(GetWorld()); It; ++It)
	{
		ANeonCityNPCCharacter* Other = *It;
		const auto* OtherMarker=Other->GetSpawnPoint();
		const bool OtherAuthored=OtherMarker && OtherMarker->AmbientActivity!=ECoastalAmbientActivity::Legacy;
		if(Authored!=OtherAuthored || (Authored && (OtherMarker->AmbientActivity!=ECoastalAmbientActivity::CafeConversation
			|| OtherMarker->AmbientConversationGroup!=Marker->AmbientConversationGroup)))continue;
		if(Other->IsPopulationDormant()) continue;
		if (auto* Body=Other->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { continue; }
		if (Other->GetThreatComponent()->HasActiveThreat() || Other->GetCombatAI()->ControlsLocomotion() || Other == Pedestrian.Get() || Other->GetNPCState()->GetState() != ENeonCityNPCState::Idle
			|| FVector::DistSquared(Other->GetActorLocation(),Pedestrian->GetActorLocation()) > FMath::Square(230.f)) { continue; }
		ANeonCityNPCController* OtherController = Cast<ANeonCityNPCController>(Other->GetController());
		if (!OtherController || !LineOfSightTo(Other)) { continue; }
		SocialPartner = Other;
		OtherController->SocialPartner = Pedestrian;
		Pedestrian->GetNPCState()->SetState(ENeonCityNPCState::Social);
		Other->GetNPCState()->SetState(ENeonCityNPCState::Social);
		return true;
	}
	return false;
}
bool ANeonCityNPCController::FindSafeDodge(AActor* Vehicle, FVector& OutLocation) const
{
	FVector Location = Pedestrian->GetActorLocation();
	const FVector Travel = Vehicle->GetVelocity().GetSafeNormal2D();
	if (Travel.IsNearlyZero()) { return false; }
	const auto* Seat = Pedestrian->FindComponentByClass<UCoastalAmbientSeatComponent>();
	const bool LeavingSeat = Seat && Seat->IsSeatedPresentationActive();
	if (LeavingSeat)
	{
		++DodgeAttempts;
		FTransform StandingExit;
		if (!Seat->GetPlannedStandingExit(StandingExit)) { DodgeDiagnostic=TEXT("no checked standing exit"); return false; }
		// The seat owns the animated departure. Native avoidance starts at its
		// actual checked exit, with the chair included in every movement query.
		Location = StandingExit.GetLocation();
		DodgeDiagnostic=FString::Printf(TEXT("checked standing origin=%s; "),*Location.ToString());
	}
	FVector BoundsOrigin, BoundsExtent;
	Vehicle->GetActorBounds(true,BoundsOrigin,BoundsExtent);
	FBox VisualBounds(ForceInit);
	TInlineComponentArray<UMeshComponent*> VehicleMeshes(Vehicle);
	for (const auto* Mesh : VehicleMeshes)
	{
		if (Mesh->IsVisible() && !Mesh->bHiddenInGame) { VisualBounds += Mesh->Bounds.GetBox(); }
	}
	if (VisualBounds.IsValid) { BoundsOrigin=VisualBounds.GetCenter(); BoundsExtent=VisualBounds.GetExtent(); }
	FVector Side(-Travel.Y, Travel.X, 0.f);
	if (FVector::DotProduct(Side,Location-BoundsOrigin) < 0.f) { Side *= -1.f; }
	const UCapsuleComponent* Capsule = Pedestrian->GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCDodge),false,Pedestrian.Get());
	if (!LeavingSeat) { Query.AddIgnoredActor(Vehicle); }
	const float HalfWidth = FVector::DotProduct(Side.GetAbs(),BoundsExtent);
	const float Radius = Capsule->GetScaledCapsuleRadius();
	// Account for native arrival tolerance, plus half a capsule of room for
	// braking and small lane changes, rather than stopping inside the car lane.
	const float SafeLateralDistance = HalfWidth + Radius + PointArrivalRadius + Radius*.5f;
	const float AuthoredDistance = FMath::Clamp(Pedestrian->GetNPCData()->DodgeDistance,100.f,400.f);
	for (float Sign : {1.f,-1.f})
	{
		const FVector Direction = Side * Sign;
		const float LateralOffset = FVector::DotProduct(Direction,Location-BoundsOrigin);
		const float MinimumDistance = FMath::Max(PointArrivalRadius+Radius*.5f,
			SafeLateralDistance-LateralOffset);
		if (LeavingSeat && MinimumDistance > AuthoredDistance) { DodgeDiagnostic+=TEXT("outside authored dodge reach; "); continue; }
		// Standing already contributes to a seated escape. Continue only as far
		// as necessary to clear the fitted vehicle, keeping a supported short
		// route available beside a quay edge instead of crossing back over a chair.
		FVector Candidate = Location + Direction * (LeavingSeat ? MinimumDistance : AuthoredDistance);
		FHitResult Ground;
		if (!GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector(0,0,50),Candidate-FVector(0,0,180),ECC_Pawn,Query)
			|| Ground.ImpactNormal.Z < Pedestrian->GetCharacterMovement()->GetWalkableFloorZ()) { if(LeavingSeat)DodgeDiagnostic+=TEXT("invalid candidate floor; ");continue; }
		Candidate.Z = Ground.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight()+2.f;
		if (FMath::Abs(Candidate.Z-Location.Z) > 48.f) { if(LeavingSeat)DodgeDiagnostic+=TEXT("candidate height mismatch; ");continue; }
		if (FMath::Abs(FVector::DotProduct(Side,Candidate-BoundsOrigin))+KINDA_SMALL_NUMBER < SafeLateralDistance) { if(LeavingSeat)DodgeDiagnostic+=TEXT("candidate inside vehicle lane; ");continue; }
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit,Location,Candidate,FQuat::Identity,ECC_Pawn,Shape,Query))
		{if(LeavingSeat)DodgeDiagnostic+=FString::Printf(TEXT("path blocked actor=%s initial=%d; "),*GetPathNameSafe(Hit.GetActor()),Hit.bStartPenetrating);continue;}
		if (GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,Shape,Query)) { if(LeavingSeat)DodgeDiagnostic+=TEXT("standing endpoint blocked; ");continue; }
		bool FloorClear = true;
		const int32 Steps = FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Location,Candidate)/50.f));
		for (int32 Step=0; Step<=Steps; ++Step)
		{
			const FVector Sample = FMath::Lerp(Location,Candidate,float(Step)/Steps);
			if (!GetWorld()->LineTraceSingleByChannel(Hit,Sample+FVector(0,0,50),Sample-FVector(0,0,180),ECC_Pawn,Query)
				|| Hit.ImpactNormal.Z < Pedestrian->GetCharacterMovement()->GetWalkableFloorZ()
				|| FMath::Abs(Sample.Z-Hit.ImpactPoint.Z-Capsule->GetScaledCapsuleHalfHeight()) > 48.f)
			{ FloorClear=false; break; }
		}
		if (!FloorClear) { if(LeavingSeat)DodgeDiagnostic+=TEXT("unsupported movement segment; "); continue; }
		if(LeavingSeat)DodgeDiagnostic+=FString::Printf(TEXT("clear real candidate=%s"),*Candidate.ToString());
		OutLocation = Candidate;
		return true;
	}
	return false;
}
void ANeonCityNPCController::ReactTo(AActor* Actor, ENeonCityNPCReaction Kind)
{
	if (!Pedestrian.IsValid() || !IsValid(Actor)) { return; }
	if (Kind == ENeonCityNPCReaction::EvadeVehicle && !FindSafeDodge(Actor,DodgeTarget)) { return; }
	Attention = Actor;
	SocialPartner.Reset();
	Reaction = Kind;
	bObservedPlayer |= Kind == ENeonCityNPCReaction::ObservePlayer;
	bEvadedVehicle |= Kind == ENeonCityNPCReaction::EvadeVehicle;
	Pedestrian->GetNPCState()->SetState(ENeonCityNPCState::React);
	ReactionCooldown = 5.f;
}
void ANeonCityNPCController::Sense()
{
	if (!Pedestrian.IsValid()) { return; }
	const FVector Location = Pedestrian->GetActorLocation();
	const UNPCData* Data = Pedestrian->GetNPCData();
	TArray<FOverlapResult> Hits;
	FCollisionObjectQueryParams Objects(ECC_Vehicle);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCVehicles),false,Pedestrian.Get());
	GetWorld()->OverlapMultiByObjectType(Hits,Location,FQuat::Identity,Objects,
		FCollisionShape::MakeSphere(FMath::Clamp(Data->VehicleDetectDistance,500.f,2000.f)),Query);
	const auto* Seat=Pedestrian->FindComponentByClass<UCoastalAmbientSeatComponent>();
	const bool RecordSeat=Seat && Seat->IsSeatedPresentationActive();
	if(RecordSeat)VehicleSenseDiagnostic=FString::Printf(TEXT("vehicle_channel_overlaps=%d"),Hits.Num());
	for (const FOverlapResult& Hit : Hits)
	{
		AActor* Vehicle = Hit.GetActor();
		if (!IsValid(Vehicle) || !Vehicle->Implements<UVehicleInterface>()) { continue; }
		const FVector Velocity = FVector(Vehicle->GetVelocity().X,Vehicle->GetVelocity().Y,0);
		const FVector Delta = FVector(Location.X-Vehicle->GetActorLocation().X,Location.Y-Vehicle->GetActorLocation().Y,0);
		if (Velocity.SizeSquared() < FMath::Square(FMath::Clamp(Data->FastVehicleSpeed,300.f,1200.f)))
		{if(RecordSeat)VehicleSenseDiagnostic=FString::Printf(TEXT("actor=%s speed=%.3f below threshold=%.3f"),*GetPathNameSafe(Vehicle),Velocity.Size(),FMath::Clamp(Data->FastVehicleSpeed,300.f,1200.f));continue;}
		const float Time = FVector::DotProduct(Delta,Velocity)/Velocity.SizeSquared();
		if(RecordSeat)VehicleSenseDiagnostic=FString::Printf(TEXT("actor=%s speed=%.3f time_to_closest=%.3f closest_distance=%.3f"),*GetPathNameSafe(Vehicle),Velocity.Size(),Time,(Delta-Velocity*Time).Size());
		if (Time < 0.f || Time > 1.5f || (Delta-Velocity*Time).SizeSquared() > FMath::Square(260.f)) { if(RecordSeat)VehicleSenseDiagnostic+=TEXT(" rejected approach");continue; }
		if (!LineOfSightTo(Vehicle)) { if(RecordSeat)VehicleSenseDiagnostic+=TEXT(" rejected line of sight");continue; }
		if(RecordSeat)VehicleSenseDiagnostic+=TEXT(" genuine approaching vehicle, checking dodge");
		if (Reaction != ENeonCityNPCReaction::EvadeVehicle || Pedestrian->GetNPCState()->GetState() != ENeonCityNPCState::React)
		{
			ReactTo(Vehicle,ENeonCityNPCReaction::EvadeVehicle);
		}
		return;
	}
	if (ReactionCooldown > 0.f || Pedestrian->GetNPCState()->GetState() == ENeonCityNPCState::React) { return; }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (Player && !Player->Implements<UVehicleInterface>()
		&& FVector::DistSquared(Player->GetActorLocation(),Location) < FMath::Square(FMath::Clamp(Data->PlayerObserveDistance,100.f,500.f))
		&& LineOfSightTo(Player)) { ReactTo(Player,ENeonCityNPCReaction::ObservePlayer); }
}
void ANeonCityNPCController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Pedestrian.IsValid()) { return; }
	ANeonCityNPCCharacter* NPC = Pedestrian.Get();
	auto* Seat=NPC->FindComponentByClass<UCoastalAmbientSeatComponent>();
	if (auto* Body=NPC->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { if(Seat)Seat->CancelForIncapacity();NPC->GetCharacterMovement()->StopMovementImmediately(); return; }
	UNPCStateComponent* State = NPC->GetNPCState();
	State->AdvanceTime(DeltaSeconds);
 if(NPC->GetCombatAI()->ControlsLocomotion()) { if(Seat && Seat->UpdateSeating(DeltaSeconds,true))return;SocialPartner.Reset(); Attention.Reset(); NPC->GetCombatAI()->UpdateLocomotion(DeltaSeconds); return; }
	if(NPC->GetThreatComponent()->HasActiveThreat() && Seat && Seat->UpdateSeating(DeltaSeconds,true))return;
	if (NPC->GetThreatComponent()->UpdateReaction(DeltaSeconds)) { return; }
	ReactionCooldown = FMath::Max(0.f,ReactionCooldown-DeltaSeconds);
	SensorSeconds -= DeltaSeconds;
	if (SensorSeconds <= 0.f) { SensorSeconds = 0.2f; Sense(); }
	if(Seat && Seat->UpdateSeating(DeltaSeconds,Reaction==ENeonCityNPCReaction::EvadeVehicle))
	{
		// The seated actor keeps sensing and threat/combat precedence. Body yaw
		// cannot rotate a person off their physical chair during a brief glance.
		if(State->GetState()==ENeonCityNPCState::React && Reaction==ENeonCityNPCReaction::ObservePlayer && State->GetStateSeconds()>1.4f)
		{Reaction=ENeonCityNPCReaction::None;Attention.Reset();State->SetState(ENeonCityNPCState::Social);}
		return;
	}
	const TArray<FVector>& Route = NPC->GetRoute();
	if (Route.IsEmpty()) { return; }
	RouteIndex %= Route.Num();
	switch (State->GetState())
	{
	case ENeonCityNPCState::Idle:
		LookCountdown -= DeltaSeconds;
		if (LookCountdown <= 0.f)
		{
			const auto* Marker=NPC->GetSpawnPoint();
			if(Marker && Marker->AmbientActivity!=ECoastalAmbientActivity::Legacy)
			{LookCountdown=Random.FRandRange(2.5f,5.f);LookYaw=Marker->GetActorRotation().Yaw+Random.FRandRange(-FMath::Clamp(Marker->AmbientLookExtentDegrees,0.f,45.f),FMath::Clamp(Marker->AmbientLookExtentDegrees,0.f,45.f));}
			else {LookCountdown=Random.FRandRange(.8f,1.8f);LookYaw=Random.FRandRange(-180.f,180.f);}
		}
		LookAt(NPC->GetActorLocation()+FRotator(0,LookYaw,0).Vector()*200.f,DeltaSeconds);
		if (State->GetStateSeconds() >= 1.f && TrySocial()) { break; }
		if (State->GetStateSeconds() > IdleDuration) { State->SetState(ENeonCityNPCState::Walk); }
		break;
	case ENeonCityNPCState::Social:
		if (!SocialPartner.IsValid() || SocialPartner->GetNPCState()->GetState() != ENeonCityNPCState::Social
			|| State->GetStateSeconds() > FMath::Clamp(NPC->GetNPCData()->SocialSeconds,1.f,8.f))
		{
			SocialPartner.Reset(); State->SetState(ENeonCityNPCState::Walk);
		}
		else { LookAt(SocialPartner->GetActorLocation(),DeltaSeconds); }
		break;
	case ENeonCityNPCState::React:
		if (Reaction == ENeonCityNPCReaction::EvadeVehicle)
		{
			NPC->GetCharacterMovement()->MaxWalkSpeed = 300.f;
			MoveToPoint(DodgeTarget);
		}
		else if (Attention.IsValid()) { LookAt(Attention->GetActorLocation(),DeltaSeconds); }
		if (State->GetStateSeconds() > (Reaction == ENeonCityNPCReaction::EvadeVehicle ? 1.8f : 1.4f))
		{
			NPC->GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(NPC->GetNPCData()->WalkSpeed,60.f,220.f);
			State->SetState(ENeonCityNPCState::Return);
		}
		break;
	case ENeonCityNPCState::Return:
	case ENeonCityNPCState::Walk:
		if (MoveToPoint(Route[RouteIndex]))
		{
			const int32 Arrived=RouteIndex;
			RouteIndex = (RouteIndex+1)%Route.Num();
			Reaction = ENeonCityNPCReaction::None;
			Attention.Reset();
			const auto* Marker=NPC->GetSpawnPoint();
			const bool ContinueStroll=Marker && Marker->AmbientActivity==ECoastalAmbientActivity::Stroll
				&& Route.Num()>=4 && Route.Num()%2==0 && Arrived!=0 && Arrived!=Route.Num()/2;
			State->SetState(ContinueStroll?ENeonCityNPCState::Walk:ENeonCityNPCState::Idle);
			if(ContinueStroll)MoveToPoint(Route[RouteIndex]);
			BlockedSeconds = 0.f;
		}
		else
		{
			BlockedSeconds = NPC->GetVelocity().Size2D() < 5.f ? BlockedSeconds+DeltaSeconds : 0.f;
			// A blocked segment waits and retries; no teleport or path shortcut through geometry.
			if (BlockedSeconds > 2.f) { State->SetState(ENeonCityNPCState::Idle); BlockedSeconds = 0.f; }
		}
		break;
	}
}
