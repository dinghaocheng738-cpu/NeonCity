#include "World/Foliage/NeonCityFoliageInteractionComponent.h"
#include "World/Foliage/NeonCityPalmContact.h"
#include "Physics/NeonCityPhysicalSurfaces.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
 constexpr float CellSize=1600;
 FIntPoint Cell(const FVector& P){return FIntPoint(FMath::FloorToInt(P.X/CellSize),FMath::FloorToInt(P.Y/CellSize));}
 void QueryCells(const TMap<FIntPoint,TArray<int32>>& Grid,const FVector& A,const FVector& B,float Radius,TSet<int32>& Out)
 {
  const FIntPoint Lo=Cell(A.ComponentMin(B)-FVector(Radius,Radius,0));
  const FIntPoint Hi=Cell(A.ComponentMax(B)+FVector(Radius,Radius,0));
  for(int32 X=Lo.X;X<=Hi.X;++X)for(int32 Y=Lo.Y;Y<=Hi.Y;++Y)
   if(const auto* Values=Grid.Find(FIntPoint(X,Y)))Out.Append(*Values);
 }
}

UNeonCityFoliageInteractionComponent::UNeonCityFoliageInteractionComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.05f;PrimaryComponentTick.TickGroup=TG_PrePhysics;
 WoodCollisionMesh=FSoftObjectPath(TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_TreeWoodCollision.SM_NCTW_TreeWoodCollision"));
}

void UNeonCityFoliageInteractionComponent::BeginPlay()
{
 Super::BeginPlay();Vegetation=Cast<ANeonCityReferenceVegetation>(GetOwner());
 WoodMesh=WoodCollisionMesh.LoadSynchronous();
 WoodMaterial=NewObject<UPhysicalMaterial>(this);WoodMaterial->Friction=.7f;WoodMaterial->Restitution=.02f;
 WoodMaterial->SurfaceType=NeonCityPhysicalSurfaces::Wood;
 RebuildIndex();
}

void UNeonCityFoliageInteractionComponent::RebuildIndex()
{
 Vegetation=Cast<ANeonCityReferenceVegetation>(GetOwner());
 TSet<int32> Dirty;for(const auto& P:Springs)WriteBend(P.Key,FVector::ZeroVector,Dirty);
 // UE 5.8 tracks each changed instance inside SetCustomDataValue. Do not
 // recreate a complete HISM render state for a handful of flexible plants.
 for(auto C:WoodPool)if(C)C->DestroyComponent();
 for(auto C:PalmPool)if(C)C->DestroyComponent();
 PalmPool.Reset();ActivePalms.Reset();FreePalms.Reset();
 Plants.Reset();WoodCells.Reset();SoftCells.Reset();WoodPool.Reset();ActiveWood.Reset();FreeWood.Reset();Springs.Reset();
 // a scatter hidden in game (a wiped or retired planting left in the map) draws nothing, so it must not collide:
 // its trunks would stand invisible wherever the new world put water or roads over the old plots
 if(!Vegetation || !Vegetation->Data || Vegetation->IsHidden())return;
 for(int32 G=0;G<Vegetation->Data->Groups.Num();++G)
 {
  const auto& Group=Vegetation->Data->Groups[G];const bool Palm=Group.SourceFamily==TEXT("palms");
  const bool Wood=Group.SourceFamily==TEXT("trees") || Palm;
  for(int32 I=0;I<Group.Instances.Num();++I)
  {
   const auto& Instance=Group.Instances[I];const float Scale=Instance.Transform.GetScale3D().GetMax();
   FPlant P;P.Group=G;P.Instance=I;P.Wood=Wood;P.Palm=Palm;
   P.Root=Vegetation->GetActorTransform().TransformPosition(Instance.Transform.GetLocation());
   P.Radius=Wood?650*Scale:(Group.SourceFamily==TEXT("ferns")?85:Group.SourceFamily==TEXT("shrubs")?110:140)*Scale;
   P.Height=Wood?1300*Scale:300*Scale;
   if(Palm)
   {
    P.Height=Instance.ShaderData0.G*100;
    P.Radius=FMath::Abs(Instance.ShaderData1.R)*P.Height+80*Scale;
   }
   const int32 Index=Plants.Add(P);(Wood?WoodCells:SoftCells).FindOrAdd(Cell(P.Root)).Add(Index);
  }
 }
 ObserverElapsed=1;WoodElapsed=1;
 // Actor BeginPlay may follow component BeginPlay; its final rebuild repeats
 // this before the first physics step. No collider is baked into the map.
 GatherContacts(0);UpdateWood();
}

void UNeonCityFoliageInteractionComponent::GatherContacts(float Delta)
{
 if(!GetWorld())return;
 ObserverElapsed+=Delta;
 TArray<APawn*> Observers;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
  if(auto* PC=It->Get();PC && PC->IsLocalController() && PC->GetPawn())Observers.Add(PC->GetPawn());
 // Refresh immediately on possession, otherwise only inspect the small Pawn
 // population once a second. Never iterate the island's plant population.
 bool Refresh=ObserverElapsed>=1;
 for(auto* P:Observers)Refresh|=!NearbyPawns.Contains(P);
 if(Refresh)
 {
  ObserverElapsed=0;NearbyPawns.Reset();TArray<APawn*> Candidates;
  for(TActorIterator<APawn> It(GetWorld());It;++It)
   for(auto* Observer:Observers)if(FVector::DistSquared(It->GetActorLocation(),Observer->GetActorLocation())<FMath::Square(6000.))
   {Candidates.AddUnique(*It);break;}
  Candidates.Sort([&](const APawn& A,const APawn& B)
  {
   auto Distance=[&](const APawn& P){double D=MAX_dbl;for(auto* O:Observers)D=FMath::Min(D,FVector::DistSquared(P.GetActorLocation(),O->GetActorLocation()));return D;};
   return Distance(A)<Distance(B);
  });
  for(auto* P:Observers)NearbyPawns.AddUnique(P);
  for(auto* P:Candidates)if(NearbyPawns.Num()<8)NearbyPawns.AddUnique(P);
 }
 Contacts.Reset();
 for(const auto& Weak:NearbyPawns)if(auto* P=Weak.Get();P && P->GetActorEnableCollision() && !P->IsHidden())
 {
  FContact C;C.Position=P->GetActorLocation();C.Velocity=P->GetVelocity().GetClampedToMaxSize(10000);
  P->GetSimpleCollisionCylinder(C.Radius,C.HalfHeight);
  C.Radius=FMath::Clamp(C.Radius,20.f,1500.f);C.HalfHeight=FMath::Clamp(C.HalfHeight,20.f,1500.f);
  if(!C.Position.ContainsNaN() && !C.Velocity.ContainsNaN())Contacts.Add(C);
 }
}

void UNeonCityFoliageInteractionComponent::UpdateWood()
{
 if(!WoodMesh || !Vegetation || !Vegetation->Data)return;
 TMap<int32,double> Wanted;TSet<int32> Candidates;
 for(const auto& C:Contacts)
 {
  const FVector End=C.Position+C.Velocity*.5;Candidates.Reset();
  // Mature palms may lean several metres away from their indexed root.
  QueryCells(WoodCells,C.Position,End,C.Radius+3600,Candidates);
  LastCandidates+=Candidates.Num();
  for(int32 I:Candidates)
  {
   const auto& P=Plants[I];
   if(FMath::Max(C.Position.Z,End.Z)+C.HalfHeight<P.Root.Z-100 || FMath::Min(C.Position.Z,End.Z)-C.HalfHeight>P.Root.Z+P.Height)continue;
   FVector A=C.Position,B=End;A.Z=P.Root.Z;B.Z=P.Root.Z;
   const double Distance=FMath::PointDistToSegment(P.Root,A,B);
   const float Radius=P.Radius+C.Radius+1000+((ActiveWood.Contains(I) || ActivePalms.Contains(I))?250:0);
   if(Distance<Radius)
   {
    // Prioritize actual occupied space before the predictive corridor.
    const double Score=FVector::DistSquared2D(P.Root,C.Position);
    if(auto* Existing=Wanted.Find(I))*Existing=FMath::Min(*Existing,Score);else Wanted.Add(I,Score);
   }
  }
 }
 TArray<int32> All;Wanted.GetKeys(All);All.Sort([&](int32 A,int32 B){return Wanted[A]<Wanted[B];});
 TArray<int32> Order,Palms;
 // Reserve a bounded 160 capsule bodies inside the existing total pool budget.
 constexpr int32 TreeBudget=MaximumWoodBodies-MaximumPalmTrees*NeonCityPalmContact::SegmentCount;
 for(int32 I:All)
 {
  auto& List=Plants[I].Palm?Palms:Order;
  if(List.Num()<(Plants[I].Palm?MaximumPalmTrees:TreeBudget))List.Add(I);else ++CapacityMisses;
 }
 UpdatePalms(Palms);
 TSet<int32> Keep(Order);
 for(auto It=ActiveWood.CreateIterator();It;++It)if(!Keep.Contains(It.Key()))
 {WoodPool[It.Value()]->SetCollisionEnabled(ECollisionEnabled::NoCollision);FreeWood.Add(It.Value());It.RemoveCurrent();}
 for(int32 I:Order)if(!ActiveWood.Contains(I))
 {
  int32 Slot;
  if(!FreeWood.IsEmpty())Slot=FreeWood.Pop(EAllowShrinking::No);
  else
  {
   auto* C=NewObject<UStaticMeshComponent>(GetOwner());GetOwner()->AddInstanceComponent(C);
   C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(WoodMesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);
   C->SetGenerateOverlapEvents(false);C->SetNotifyRigidBodyCollision(true);C->SetCanEverAffectNavigation(false);
   C->SetPhysMaterialOverride(WoodMaterial);C->SetVisibility(false);C->SetHiddenInGame(true);C->SetCastShadow(false);
   C->SetVisibleInRayTracing(false);C->SetAffectDistanceFieldLighting(false);C->SetAffectDynamicIndirectLighting(false);
   C->ComponentTags.Add(TEXT("NeonCity.Foliage.Wood"));C->RegisterComponent();Slot=WoodPool.Add(C);
  }
  auto* C=WoodPool[Slot].Get();const auto& P=Plants[I];
  C->SetWorldTransform(Vegetation->Data->Groups[P.Group].Instances[P.Instance].Transform*Vegetation->GetActorTransform(),false,nullptr,ETeleportType::TeleportPhysics);
  // Kinematic, never SimulatePhysics. Chaos supplies vehicle contact impulse;
  // PhysicalImpactComponent supplies existing collision damage/audio events.
  C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);ActiveWood.Add(I,Slot);
 }
}

void UNeonCityFoliageInteractionComponent::UpdatePalms(const TArray<int32>& Order)
{
 using namespace NeonCityPalmContact;
 TSet<int32> Keep(Order);
 for(auto It=ActivePalms.CreateIterator();It;++It)if(!Keep.Contains(It.Key()))
 {
  for(int32 S=0;S<SegmentCount;++S)PalmPool[It.Value()+S]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  FreePalms.Add(It.Value());It.RemoveCurrent();
 }
 for(int32 I:Order)if(!ActivePalms.Contains(I))
 {
  int32 Slot;
  if(!FreePalms.IsEmpty())Slot=FreePalms.Pop(EAllowShrinking::No);
  else
  {
   Slot=PalmPool.Num();
   for(int32 S=0;S<SegmentCount;++S)
   {
    auto* C=NewObject<UCapsuleComponent>(GetOwner());GetOwner()->AddInstanceComponent(C);
    C->SetMobility(EComponentMobility::Movable);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);
    C->SetGenerateOverlapEvents(false);C->SetNotifyRigidBodyCollision(true);C->SetCanEverAffectNavigation(false);
    // Segment caps are a collision approximation, never a staircase up a trunk.
    C->CanCharacterStepUpOn=ECB_No;
    C->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable,0));
    C->SetPhysMaterialOverride(WoodMaterial);C->SetVisibility(false);C->SetHiddenInGame(true);C->SetCastShadow(false);
    C->ComponentTags.Add(TEXT("NeonCity.Foliage.PalmWood"));C->RegisterComponent();PalmPool.Add(C);
   }
  }
  const auto& P=Plants[I];const auto& Instance=Vegetation->Data->Groups[P.Group].Instances[P.Instance];
  const auto A=Instance.ShaderData0,B=Instance.ShaderData1;
  for(int32 S=0;S<SegmentCount;++S)
  {
   const FVector Start=Center(P.Root,A.G*100,B.R,A.A,A.R,Rows[S]);
   const FVector End=Center(P.Root,A.G*100,B.R,A.A,A.R,Rows[S+1]);
   // Mid-segment radius avoids a bulbous root radius along the upper stem.
   const float Radius=RadiusCm((Rows[S]+Rows[S+1])*.5f,A.B);
   auto* C=PalmPool[Slot+S].Get();C->SetCapsuleSize(Radius,float((End-Start).Size())*.5f+Radius,false);
   C->SetWorldLocationAndRotation((Start+End)*.5,FQuat::FindBetweenNormals(FVector::UpVector,(End-Start).GetSafeNormal()),false,nullptr,ETeleportType::TeleportPhysics);
   C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  }
  ActivePalms.Add(I,Slot);
 }
}

void UNeonCityFoliageInteractionComponent::WriteBend(int32 I,const FVector& Bend,TSet<int32>& Dirty)
{
 if(!Vegetation || !Plants.IsValidIndex(I))return;const auto& P=Plants[I];
 if(auto* C=Vegetation->GetPlantGroup(P.Group);C && C->NumCustomDataFloats>=11 && P.Instance<C->GetInstanceCount())
 {
  for(int32 Axis=0;Axis<3;++Axis)C->SetCustomDataValue(P.Instance,8+Axis,Bend[Axis],false);
  Dirty.Add(P.Group);
 }
}

void UNeonCityFoliageInteractionComponent::UpdateSprings(float Delta)
{
 for(auto& Pair:Springs)Pair.Value.Target=FVector::ZeroVector;
 FeedbackElapsed+=Delta;const bool Emit=FeedbackElapsed>=.25f && OnSoftContact.IsBound();
 if(Emit)FeedbackElapsed=0;
 struct FFeedback {FVector Position;float Speed=0,Strength=0;};TArray<FFeedback> Feedback;
 TMap<int32,FVector> Targets;TSet<int32> Candidates;
 for(const auto& C:Contacts)
 {
  FFeedback Best;Best.Speed=C.Velocity.Size();
  Candidates.Reset();QueryCells(SoftCells,C.Position,C.Position,C.Radius+700,Candidates);LastCandidates+=Candidates.Num();
  for(int32 I:Candidates)
  {
   const auto& P=Plants[I];
   if(C.Position.Z+C.HalfHeight<P.Root.Z+10 || C.Position.Z-C.HalfHeight>P.Root.Z+P.Height)continue;
   FVector Out=P.Root-C.Position;Out.Z=0;const float Distance=Out.Size(),Reach=P.Radius+C.Radius;
   if(Distance>=Reach)continue;
   const FVector Motion=FVector(C.Velocity.X,C.Velocity.Y,0).GetSafeNormal();
   const FVector Direction=(Out.GetSafeNormal()+Motion*.35).GetSafeNormal();
   const float Strength=FMath::SmoothStep(0.f,1.f,1-Distance/Reach)*FMath::Min(65.f,22.f+C.Radius*.2f+C.Velocity.Size2D()*.025f);
   FVector& Target=Targets.FindOrAdd(I);const FVector Bend=(Direction.IsNearlyZero()?FVector::ForwardVector:Direction)*Strength;
   if(Bend.SizeSquared()>Target.SizeSquared())Target=Bend;
   if(Emit && Best.Speed>=35 && Strength>Best.Strength && (Springs.Contains(I) || Springs.Num()<MaximumSprings))
   {Best.Strength=Strength;Best.Position=FVector(P.Root.X,P.Root.Y,FMath::Clamp(C.Position.Z,P.Root.Z+10,P.Root.Z+P.Height));}
  }
  if(Best.Strength>=4)Feedback.Add(Best);
 }
 TArray<int32> Order;Targets.GetKeys(Order);Order.Sort([&](int32 A,int32 B){return Targets[A].SizeSquared()>Targets[B].SizeSquared();});
 for(int32 I:Order)
 {
  if(!Springs.Contains(I) && Springs.Num()>=MaximumSprings)continue;
  Springs.FindOrAdd(I).Target=Targets[I];
 }
 TSet<int32> Dirty;
 // Fixed small substeps keep the damped physical spring stable at low FPS.
 const float Time=FMath::Clamp(Delta,0.f,.15f);const int32 Steps=FMath::Max(1,FMath::CeilToInt(Time*120));const float Dt=Time/Steps;
 for(auto It=Springs.CreateIterator();It;++It)
 {
  auto& S=It.Value();
  for(int32 Step=0;Step<Steps;++Step)
  {S.Velocity+=((S.Target-S.Position)*100-S.Velocity*12)*Dt;S.Position+=S.Velocity*Dt;}
  S.Position=S.Position.GetClampedToMaxSize(75);
  if(S.Target.IsNearlyZero() && S.Position.SizeSquared()<.01 && S.Velocity.SizeSquared()<.01)
  {WriteBend(It.Key(),FVector::ZeroVector,Dirty);It.RemoveCurrent();}
  else WriteBend(It.Key(),S.Position,Dirty);
 }
 for(const auto& Event:Feedback)OnSoftContact.Broadcast(Event.Position,Event.Speed,Event.Strength);
}

void UNeonCityFoliageInteractionComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);if(!FMath::IsFinite(Delta) || Delta<=0 || !Vegetation)return;
 LastCandidates=0;GatherContacts(Delta);WoodElapsed+=Delta;
 if(WoodElapsed>=.1f){WoodElapsed=0;UpdateWood();}
 UpdateSprings(Delta);
}

FVector UNeonCityFoliageInteractionComponent::GetPlantBend(int32 Group,int32 Instance) const
{
 for(const auto& Pair:Springs)if(Plants[Pair.Key].Group==Group && Plants[Pair.Key].Instance==Instance)return Pair.Value.Position;
 return FVector::ZeroVector;
}

void UNeonCityFoliageInteractionComponent::EndPlay(EEndPlayReason::Type Reason)
{
 for(auto C:WoodPool)if(C)C->DestroyComponent();WoodPool.Reset();ActiveWood.Reset();Springs.Reset();Plants.Reset();
 for(auto C:PalmPool)if(C)C->DestroyComponent();PalmPool.Reset();ActivePalms.Reset();
 Super::EndPlay(Reason);
}
