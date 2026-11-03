#pragma once
#include "CoreMinimal.h"
#include "WorldEventTypes.generated.h"
/** Reserved taxonomy for the future city random-event system. This phase only publishes the signals. */
UENUM(BlueprintType)
enum class ENeonCityWorldEventType : uint8 { Explosion, MajorCollision, VehicleFire, VehicleExplosion, PropertyDestruction };
USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityWorldEvent
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FGuid EventId;
 UPROPERTY(BlueprintReadOnly) ENeonCityWorldEventType Type=ENeonCityWorldEventType::Explosion;
 UPROPERTY(BlueprintReadOnly) FName ContextID;
 UPROPERTY(BlueprintReadOnly) FVector Location=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float Magnitude=0;
 UPROPERTY(BlueprintReadOnly) float Timestamp=0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> SourceActor;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AController> Instigator;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNeonCityWorldEventSignal,const FNeonCityWorldEvent&,Event);