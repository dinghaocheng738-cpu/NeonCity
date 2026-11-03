#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PoliceSearchCoordinator.generated.h"
UENUM(BlueprintType)
enum class EPoliceSearchPattern : uint8 { InvestigatePoint, SearchArea, SweepRoute, Perimeter };
UCLASS()
class NEONCITY_API UPoliceSearchCoordinator : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 FVector GetAssignment(int32 Slot,float Radius,EPoliceSearchPattern Pattern) const;
};
