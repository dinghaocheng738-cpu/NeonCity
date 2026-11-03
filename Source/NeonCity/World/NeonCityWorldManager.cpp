#include "World/NeonCityWorldManager.h"
#include "World/NeonCityMainlandSite.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Marine/MarineTestArea.h"
#include "Vehicles/Aircraft/AircraftTestArea.h"
#include "Vehicles/Aircraft/FixedWing/FixedWingTestArea.h"
#include "Combat/WeaponStoreActor.h"
#include "VehicleCombat/VehicleCombatTestArea.h"
#include "Police/PoliceTestArea.h"
#include "Physics/NeonCityPhysicsTestArea.h"
#include "Destruction/DestructionTestArea.h"
#include "AI/Combat/NeonCityCombatEncounter.h"
#include "Misc/CommandLine.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "AI/PedestrianSpawnPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "GameFramework/PlayerController.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void UNeonCityWorldManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentRegion = NAME_None;
	WorldState = ENeonCityWorldState::Initializing;
	bSystemReady = true;
}

void UNeonCityWorldManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	WorldState = ENeonCityWorldState::Active;
	InWorld.GetTimerManager().SetTimer(PopulationTimer,this,&UNeonCityWorldManager::ReconcilePedestrians,.5f,true);
 const auto* MainlandSite=ANeonCityMainlandSite::Find(&InWorld);
 const bool bMainland=InWorld.GetMapName().EndsWith(TEXT("NeonCity_SouthBeach_Test")) || MainlandSite;
 if(bMainland)
 {
  const bool AuthoredStore=MainlandSite && MainlandSite->WeaponStoreMesh;
  const FTransform Pose=AuthoredStore?MainlandSite->GetWeaponStorePlacement():FTransform(FRotator(0,-90,0),FVector(1800,1100,100));
  if(auto* Store=InWorld.SpawnActor<AWeaponStoreActor>(AWeaponStoreActor::StaticClass(),Pose))
   if(AuthoredStore)Store->ApplyAuthoredPresentation(MainlandSite->WeaponStoreMesh,FText::FromString(TEXT("Marina Outfitters")));
 }
 if(bMainland) InWorld.SpawnActor<AMarineTestArea>();
 if(bMainland) InWorld.SpawnActor<AAircraftTestArea>();
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityFixedWingTestArea"))) InWorld.SpawnActor<AFixedWingTestArea>();
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityVehicleCombatTestArea"))) InWorld.SpawnActor<AVehicleCombatTestArea>();
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityPoliceTestArea"))) InWorld.SpawnActor<APoliceTestArea>();
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityPhysicsTestArea"))) InWorld.SpawnActor<ANeonCityPhysicsTestArea>();
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityCombatEncounter"))) InWorld.SpawnActor<ANeonCityCombatEncounter>();
 // The proving ground sits far from the authored prototype area so no released fixture moves.
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityDestructionTestArea"))) InWorld.SpawnActor<ADestructionTestArea>(FVector(-2400,9000,0),FRotator::ZeroRotator);
 if(auto* GI=InWorld.GetGameInstance()) GI->GetSubsystem<UNeonCitySaveSubsystem>()->NotifyWorldReady(&InWorld);
}

void UNeonCityWorldManager::SetCurrentRegion(FName Region)
{
	if (!bSystemReady || WorldState == ENeonCityWorldState::ShuttingDown || CurrentRegion == Region) { return; }
	const FName Previous = CurrentRegion;
	CurrentRegion = Region;
	OnRegionChanged.Broadcast(Previous, CurrentRegion);
}

void UNeonCityWorldManager::Deinitialize()
{
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PopulationTimer);
	PedestrianMarkers.Reset();
	WorldState = ENeonCityWorldState::ShuttingDown;
	bSystemReady = false;
	CurrentRegion = NAME_None;
	OnRegionChanged.Clear();
	Super::Deinitialize();
}

void UNeonCityWorldManager::RegisterPedestrian(APedestrianSpawnPoint* Marker)
{
	if(IsValid(Marker) && Marker->bDistanceManaged) PedestrianMarkers.AddUnique(Marker);
}

void UNeonCityWorldManager::ReconcilePedestrians()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(NeonCityReconcilePedestrians);
	UWorld* W=GetWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
	if(!PC || !PC->GetPawn() || W->GetNetMode()==NM_Client) return;
	const FVector Observer=PC->GetPawn()->GetActorLocation();
	constexpr int32 ActiveBudget=20, SpawnPerUpdate=2;
	struct FCandidate { APedestrianSpawnPoint* Marker; float Score; };
	TArray<FCandidate> Candidates;TSet<ANeonCityNPCCharacter*> Protected;
	PedestrianMarkers.RemoveAll([](const auto& M){return !M.IsValid();});
	for(const auto& Weak:PedestrianMarkers)
	{
		auto* M=Weak.Get();auto* N=M->GetSpawnedPedestrian();
		if(N && !N->IsPopulationDormant() && !N->CanSuspendPopulation()) {Protected.Add(N);continue;}
		if(!N && !M->IsSpawnAttemptReady()) continue;
		const bool Active=N && !N->IsPopulationDormant();
		const float Distance=FMath::Min(FVector::DistSquared2D(Observer,M->GetActorLocation()),N?FVector::DistSquared2D(Observer,N->GetActorLocation()):MAX_flt);
		const float Radius=FMath::Clamp(M->ActivationRadius,3000.f,40000.f)+(Active?3000.f:0.f);
		if(Distance<=FMath::Square(Radius)) Candidates.Add({M,FMath::Sqrt(Distance)-(Active?1500.f:0.f)});
	}
	// Non-managed quest/test characters count against ambient headroom but never
	// have their lifecycle, collision or memory changed by this scheduler.
	int32 Reserved=0;
	for(const auto* N:Protected) Reserved+=N->UsesPopulationBehaviourBudget();
	for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I)
		if(I->UsesPopulationBehaviourBudget() && !I->GetSpawnPoint()) ++Reserved;
	for(TActorIterator<APedestrianSpawnPoint> I(W);I;++I)
		if(!I->bDistanceManaged && I->GetSpawnedPedestrian() && I->GetSpawnedPedestrian()->UsesPopulationBehaviourBudget()) ++Reserved;
	Candidates.Sort([](const FCandidate& A,const FCandidate& B){return A.Score<B.Score;});
	TSet<APedestrianSpawnPoint*> Selected;
	for(int32 I=0;I<FMath::Min(Candidates.Num(),FMath::Max(0,ActiveBudget-Reserved));++I) Selected.Add(Candidates[I].Marker);
	for(const auto& Weak:PedestrianMarkers)
	{
		auto* M=Weak.Get();auto* N=M->GetSpawnedPedestrian();
		if(N && !Protected.Contains(N) && !Selected.Contains(M)) N->SetPopulationDormant(true);
	}
	int32 Spawns=0;
	Selected.Reset();
	for(const auto& C:Candidates)
	{
		if(Selected.Num()>=FMath::Max(0,ActiveBudget-Reserved)) break;
		if(!C.Marker->GetSpawnedPedestrian())
		{
			if(Spawns>=SpawnPerUpdate) continue;
			++Spawns;if(!C.Marker->EnsurePedestrian()) continue;
		}
		C.Marker->GetSpawnedPedestrian()->SetPopulationDormant(false);
		// A blocked wake does not consume headroom: try the next clear route.
		if(!C.Marker->GetSpawnedPedestrian()->IsPopulationDormant()) Selected.Add(C.Marker);
	}
	for(const auto& Weak:PedestrianMarkers)
		if(auto* N=Weak->GetSpawnedPedestrian();N && !Protected.Contains(N) && !Selected.Contains(Weak.Get())) N->SetPopulationDormant(true);
}

bool UNeonCityWorldManager::IsSystemReady_Implementation() const
{
 if (!bSystemReady) return false;
 const auto* Coast = ANeonCityCoastalRegion::Find(GetWorld());
 return !Coast || Coast->IsInitialRegionReady();
}

bool UNeonCityWorldManager::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
