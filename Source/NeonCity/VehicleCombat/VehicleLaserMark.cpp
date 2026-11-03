#include "VehicleCombat/VehicleLaserMark.h"
#include "Combat/HealthComponent.h"
#include "Components/SceneComponent.h"

AVehicleLaserMark::AVehicleLaserMark()
{
 PrimaryActorTick.bCanEverTick=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Spot")));
 Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
 SetActorEnableCollision(false);SetCanBeDamaged(false);SetActorHiddenInGame(true);
}
