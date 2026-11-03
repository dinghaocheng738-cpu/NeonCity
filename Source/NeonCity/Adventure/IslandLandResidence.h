#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandLandResidence.generated.h"
class UPropertyComponent;
UCLASS()
class NEONCITY_API AIslandLandResidence : public AActor
{
 GENERATED_BODY()
public:
 AIslandLandResidence();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UPropertyComponent> Property;
};
