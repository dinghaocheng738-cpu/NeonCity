#include "Adventure/IslandLargePlantContact.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
namespace { FIntPoint ShrubCell(const FVector& P){return FIntPoint(FMath::FloorToInt(P.X/1200),FMath::FloorToInt(P.Y/1200));} }
UIslandLargePlantContact::UIslandLargePlantContact(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UIslandLargePlantContact::BeginPlay()
{
 Super::BeginPlay();const auto* V=Cast<ANeonCityReferenceVegetation>(GetOwner());
 if(!V || !V->Data){SetComponentTickEnabled(false);return;}
 for(const auto& G:V->Data->Groups)
 if(G.SourceFamily==TEXT("shrubs") || G.SourceFamily==TEXT("bananas") || G.SourceFamily==TEXT("youngPalms") ||
    G.SourceFamily==TEXT("monsteras") || G.SourceFamily==TEXT("elephantEars") || G.SourceFamily==TEXT("heliconias") || G.SourceFamily==TEXT("strelitzias"))
 for(const auto& I:G.Instances)
 {
  // H is the actual height retained by the source vegetation shader. Small,
  // flexible shrubs keep their established bend/audio behavior. Only the dense
  // core of a mature plant blocks movement, rather than its broad leaf canopy.
  const float Height=I.ShaderData0.G*100;if(Height<200)continue;
  const float Scale=I.Transform.GetScale3D().GetMax();
  const float Radius=(G.SourceFamily==TEXT("shrubs")?25.f:14.f)*Scale;
  const int32 N=Shrubs.Add({V->GetActorTransform().TransformPosition(I.Transform.GetLocation()),Radius,Height*.65f});
  Cells.FindOrAdd(ShrubCell(Shrubs[N].Root)).Add(N);
 }
 TickComponent(0,LEVELTICK_All,nullptr);
}
void UIslandLargePlantContact::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 TMap<int32,double> Near;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)if(auto* PC=It->Get())if(APawn* Pawn=PC->GetPawn())
 {
  const FVector Position=Pawn->GetActorLocation(),End=Position+Pawn->GetVelocity().GetClampedToMaxSize(10000)*.4;
  const FIntPoint A=ShrubCell(Position.ComponentMin(End)-FVector(1200,1200,0)),B=ShrubCell(Position.ComponentMax(End)+FVector(1200,1200,0));
  for(int32 X=A.X;X<=B.X;++X)for(int32 Y=A.Y;Y<=B.Y;++Y)if(const auto* Indices=Cells.Find(FIntPoint(X,Y)))for(int32 I:*Indices)
  {
   const auto& S=Shrubs[I];if(FMath::Abs(S.Root.Z-Position.Z)>S.Height+300)continue;
   FVector P=Position,Q=End;P.Z=Q.Z=S.Root.Z;
   if(FMath::PointDistToSegment(S.Root,P,Q)<1100)Near.Add(I,FVector::DistSquared2D(S.Root,Position));
  }
 }
 TArray<int32> Order;Near.GetKeys(Order);Order.Sort([&](int32 A,int32 B){return Near[A]<Near[B];});
 const int32 Count=FMath::Min(64,Order.Num());
 TSet<int32> Wanted;for(int32 I=0;I<Count;++I)Wanted.Add(Order[I]);
 // A physical capsule keeps its source identity while that shrub is nearby.
 // Reordering distance-ranked slots moved live bodies through an occupied car.
 for(auto It=ActiveShrubs.CreateIterator();It;++It)if(!Wanted.Contains(It.Key()))
 {It.Value()->SetCollisionEnabled(ECollisionEnabled::NoCollision);FreeCapsules.Add(It.Value());It.RemoveCurrent();}
 for(int32 Source:Wanted)
 {
  if(ActiveShrubs.Contains(Source))continue;
  UCapsuleComponent* C=nullptr;
  if(!FreeCapsules.IsEmpty())C=FreeCapsules.Pop();
  else
  {
   C=NewObject<UCapsuleComponent>(GetOwner());C->SetCollisionProfileName(TEXT("BlockAll"));C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);C->RegisterComponent();Pool.Add(C);
  }
  const auto& S=Shrubs[Source];C->SetCapsuleSize(S.Radius,FMath::Max(S.Radius,S.Height*.5f),false);
  C->SetWorldLocation(S.Root+FVector(0,0,S.Height*.5f),false,nullptr,ETeleportType::TeleportPhysics);
  C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);ActiveShrubs.Add(Source,C);
 }
}
void UIslandLargePlantContact::EndPlay(EEndPlayReason::Type Reason)
{
 for(auto C:Pool)if(C)C->DestroyComponent();ActiveShrubs.Reset();FreeCapsules.Reset();Pool.Reset();Cells.Reset();Shrubs.Reset();Super::EndPlay(Reason);
}
