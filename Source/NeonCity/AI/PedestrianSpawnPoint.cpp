#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/NeonCityWorldManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Presentation/CharacterAppearanceComponent.h"
#include "AI/CoastalAmbientSeatComponent.h"
#include "Presentation/CoastalAmbientAnimInstance.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

APedestrianSpawnPoint::APedestrianSpawnPoint()
{
	SetRootComponent(CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnMarker")));
	PrimaryActorTick.bCanEverTick = false;
	LocalRoute = {FVector::ZeroVector,FVector(700,0,0)};
}
void APedestrianSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !bAmbientEnabled) return;
	if (bDistanceManaged)
	{
		GetWorld()->GetSubsystem<UNeonCityWorldManager>()->RegisterPedestrian(this);
		return;
	}
	int32 Population = 0;
	for (TActorIterator<ANeonCityNPCCharacter> It(GetWorld()); It; ++It) { ++Population; }
	if (Population >= 20) { UE_LOG(LogTemp,Warning,TEXT("Pedestrian prototype hard cap (20) reached.")); return; }
	EnsurePedestrian();
}
bool APedestrianSpawnPoint::EnsurePedestrian()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(NeonCity_EnsurePedestrian);
	if (Spawned.IsValid()) return true;
	if (!HasAuthority() || !bAmbientEnabled || LocalRoute.Num() < 2 || !IsSpawnAttemptReady()) return false;
	RetrySpawnAt=GetWorld()->GetTimeSeconds()+5.f;
	TArray<FVector> Route;
	for (const FVector& Point : LocalRoute)
	{
		if (Point.ContainsNaN()) { return false; }
		Route.Add(GetActorTransform().TransformPosition(Point));
	}
	const FTransform Transform(GetActorRotation(),Route[0]);
 const auto* Capsule=GetDefault<ANeonCityNPCCharacter>()->GetCapsuleComponent();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(PedestrianSpawnClearance),false,this);
 // Managed markers retry on the scheduler, without shifting off the sidewalk.
 // Legacy markers retain Unreal's original adjusted-spawn handling: they have
 // no scheduler to retry a rejected initial overlap after BeginPlay.
 if(bDistanceManaged && GetWorld()->OverlapBlockingTestByChannel(Route[0],Transform.GetRotation(),ECC_Pawn,
  FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius(),Capsule->GetUnscaledCapsuleHalfHeight()-2),Q))return false;
	ANeonCityNPCCharacter* NPC = GetWorld()->SpawnActorDeferred<ANeonCityNPCCharacter>(
		ANeonCityNPCCharacter::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (NPC)
	{
		NPC->InitializePedestrian(PedestrianId.IsNone() ? GetFName() : PedestrianId,NPCData,Route,BehaviorSeed,this);
		if(AmbientActivity!=ECoastalAmbientActivity::Legacy)
		{
			if(AmbientAnimationClass && AmbientAnimationClass->IsChildOf(UCoastalAmbientAnimInstance::StaticClass()))
				NPC->GetMesh()->SetAnimInstanceClass(AmbientAnimationClass);
			if(auto* Appearance=NPC->FindComponentByClass<UCharacterAppearanceComponent>())
			{
				if(AmbientMaleSkin) Appearance->ReferenceMale=AmbientMaleSkin.Get();
				if(AmbientFemaleSkin) Appearance->ReferenceFemale=AmbientFemaleSkin.Get();
			}
			if(AmbientActivity==ECoastalAmbientActivity::CafeConversation)
			{
				auto* Seat=NewObject<UCoastalAmbientSeatComponent>(NPC,TEXT("CoastalCafeSeat"));
				NPC->AddInstanceComponent(Seat);Seat->RegisterComponent();
			}
		}
		NPC->FinishSpawning(Transform);
		Spawned = NPC;
	}
	return Spawned.IsValid();
}
bool APedestrianSpawnPoint::IsSpawnAttemptReady() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds()>=RetrySpawnAt;
}
void APedestrianSpawnPoint::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (Spawned.IsValid() && EndPlayReason == EEndPlayReason::Destroyed) { Spawned->Destroy(); }
	Spawned.Reset();
	Super::EndPlay(EndPlayReason);
}
