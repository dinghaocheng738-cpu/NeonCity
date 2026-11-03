#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NPCInterface.generated.h"

/** Shared identity contract; independent of NPC movement and spawning policy. */
UINTERFACE(BlueprintType)
class NEONCITY_API UNPCInterface : public UInterface
{
	GENERATED_BODY()
};

class NEONCITY_API INPCInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NeonCity|NPC")
	FName GetNPCId() const;

	virtual FName GetNPCId_Implementation() const;
};
