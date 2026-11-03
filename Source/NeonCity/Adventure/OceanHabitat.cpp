#include "Adventure/OceanHabitat.h"
#include "Components/SceneComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
AOceanHabitat::AOceanHabitat()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("HabitatRoot")));
 RootComponent->SetMobility(EComponentMobility::Static);
 PrimaryActorTick.bCanEverTick=false;
}
void AOceanHabitat::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);RebuildHabitat();}
void AOceanHabitat::BeginPlay(){Super::BeginPlay();if(Groups.IsEmpty())RebuildHabitat();}
void AOceanHabitat::RebuildHabitat()
{
 // Existing serialized actors can still carry the old movable root. Static
 // habitat instances require a static parent before registration succeeds.
 RootComponent->SetMobility(EComponentMobility::Static);
 for(auto& C:Groups)if(C)C->DestroyComponent();Groups.Reset();
 for(const auto& L:HabitatLayers)
 {
  if(!L.Mesh || L.Instances.IsEmpty())continue;
  auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);AddInstanceComponent(C);
  C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Static);C->SetStaticMesh(L.Mesh);
  C->SetCollisionProfileName(L.bSolid?TEXT("BlockAll"):TEXT("NoCollision"));C->SetCanEverAffectNavigation(false);
  C->SetCastShadow(L.bSolid);C->bAffectDistanceFieldLighting=L.bSolid && !bStreamed;
  // Streamed seabed detail is already optically extinguished beyond 42 m.
  // Preserve authored landmark ranges and the terrain's collision/streaming.
  C->SetCullDistances(bStreamed?2800:L.bSolid?22000:9000,bStreamed?4200:L.bSolid?30000:14000);C->RegisterComponent();
  C->AddInstances(L.Instances,false,false);Groups.Add(C);
 }
}
