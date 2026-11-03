#include "Vehicles/Aircraft/AircraftWaterInteraction.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Engine/World.h"
UAircraftWaterInteraction::UAircraftWaterInteraction() { PrimaryComponentTick.bCanEverTick=false; }
void UAircraftWaterInteraction::UpdateImmersion(float HalfHeight)
{
 float Height; FVector Current; const FVector Point=GetOwner()->GetActorLocation();
 auto Next=EAircraftWaterState::Dry;
 if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Point,Height,Current))
  Next=Point.Z<Height?EAircraftWaterState::Submerged:Point.Z-HalfHeight<Height?EAircraftWaterState::Contact:EAircraftWaterState::Dry;
 if(Next!=State) { const auto Before=State; State=Next; OnWaterStateChanged.Broadcast(Before,State); }
}
