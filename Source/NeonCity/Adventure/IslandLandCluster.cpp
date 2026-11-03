#include "Adventure/IslandLandCluster.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
AIslandLandCluster::AIslandLandCluster()
{
 PrimaryActorTick.bCanEverTick=false;
 Instances=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LandInstances"));SetRootComponent(Instances);
 Instances->SetMobility(EComponentMobility::Static);Instances->SetGenerateOverlapEvents(false);Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
