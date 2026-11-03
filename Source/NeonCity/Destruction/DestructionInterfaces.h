#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DestructionInterfaces.generated.h"
UINTERFACE(BlueprintType)
class NEONCITY_API ULocalizedDestructionInterface : public UInterface { GENERATED_BODY() };
/** Bounded structure-level destruction. The shell stays stable; authored surfaces break selectively. */
class NEONCITY_API ILocalizedDestructionInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Destruction")
 int32 ApplyLocalizedDestruction(FVector Center,float Radius,float Damage,AActor* Instigator);
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Destruction")
 int32 GetIntactSurfaceCount() const;
};
UINTERFACE(BlueprintType)
class NEONCITY_API UPersistentDestructionInterface : public UInterface { GENERATED_BODY() };
/** Opt-in for future mission-critical structures. Transient city damage never uses this. */
class NEONCITY_API IPersistentDestructionInterface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="NeonCity|Destruction")
 bool ShouldPersistDestruction() const;
};