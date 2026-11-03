#include "Adventure/IslandLandResidence.h"
#include "Property/PropertyComponent.h"
AIslandLandResidence::AIslandLandResidence()
{
 PrimaryActorTick.bCanEverTick=false;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ResidenceOrigin")));
 Property=CreateDefaultSubobject<UPropertyComponent>(TEXT("IslandProperty"));
}
