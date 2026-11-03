#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NPCData.generated.h"
class UCombatAIData;

/** Small, authored pedestrian behavior profile. Distances and speeds use cm and cm/s. */
UCLASS(BlueprintType)
class NEONCITY_API UNPCData : public UDataAsset
{
	GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat") TObjectPtr<UCombatAIData> CombatProfile;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Perception") float DangerCooldown=6.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Perception") float SustainedDangerCooldown=12.f;
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pedestrian", meta=(ClampMin="60", ClampMax="220"))
	float WalkSpeed = 140.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pedestrian", meta=(ClampMin="0.5", ClampMax="10"))
	float IdleSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pedestrian", meta=(ClampMin="1", ClampMax="8"))
	float SocialSeconds = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="100", ClampMax="500"))
	float PlayerObserveDistance = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="500", ClampMax="2000"))
	float VehicleDetectDistance = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="300", ClampMax="1200"))
	float FastVehicleSpeed = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="100", ClampMax="400"))
	float DodgeDistance = 220.f;
};
