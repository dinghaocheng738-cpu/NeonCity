#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Persistence/NeonCitySaveTypes.h"
#include "MessageSystem.generated.h"

USTRUCT(BlueprintType)
struct NEONCITY_API FPhoneMessage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid Id;
	UPROPERTY(BlueprintReadOnly) FName Sender;
	UPROPERTY(BlueprintReadOnly) FText Content;
	UPROPERTY(BlueprintReadOnly) FDateTime Timestamp;
	UPROPERTY(BlueprintReadOnly) FName ActionId;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNeonCityInboxChanged);

/** Session inbox accessible to NPC/gameplay senders; no dialogue simulation or persistence. */
UCLASS()
class NEONCITY_API UMessageSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
 TArray<FNeonCityMessageSaveData> BuildPersistenceSnapshot() const;
 void ApplyPersistenceSnapshot(const TArray<FNeonCityMessageSaveData>& Snapshot);

	UFUNCTION(BlueprintCallable, Category="NeonCity|Messages")
	FGuid SendMessage(FName Sender, FText Content, FName ActionId = NAME_None);
	const TArray<FPhoneMessage>& GetMessages() const { return Messages; }
	UPROPERTY(BlueprintAssignable, Category="NeonCity|Messages") FNeonCityInboxChanged OnInboxChanged;
private:
	UPROPERTY() TArray<FPhoneMessage> Messages;
};
