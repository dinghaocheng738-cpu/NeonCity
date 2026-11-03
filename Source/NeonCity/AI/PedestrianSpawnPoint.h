#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PedestrianSpawnPoint.generated.h"
class ANeonCityNPCCharacter;
class UNPCData;
class UAnimInstance;
class USkeletalMesh;
class UStaticMesh;

UENUM(BlueprintType)
enum class ECoastalAmbientActivity : uint8 { Legacy, Stroll, CafeConversation, Viewpoint, HarborStaff };

UCLASS()
class NEONCITY_API APedestrianSpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	APedestrianSpawnPoint();
	UPROPERTY(EditAnywhere, Category="Pedestrian")
	TObjectPtr<UNPCData> NPCData;
	UPROPERTY(EditAnywhere, Category="Pedestrian")
	FName PedestrianId;
	UPROPERTY(EditAnywhere, Category="Pedestrian")
	int32 BehaviorSeed = 1;
	UPROPERTY(EditAnywhere, Category="Pedestrian", meta=(MakeEditWidget=true))
	TArray<FVector> LocalRoute;
	/** Opt-in ambient population; legacy/mission fixtures keep their lifecycle. */
	UPROPERTY(EditAnywhere, Category="Pedestrian|Population")
	bool bDistanceManaged = false;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Population", meta=(ClampMin="3000",ClampMax="40000",Units="cm"))
	float ActivationRadius = 18000.f;
	/** Retain retired ambient marker identities without creating a crowd. */
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") bool bAmbientEnabled = true;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") ECoastalAmbientActivity AmbientActivity = ECoastalAmbientActivity::Legacy;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") FName AmbientConversationGroup;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient", meta=(ClampMin="0",ClampMax="45")) float AmbientLookExtentDegrees = 18.f;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient", meta=(ClampMin="2",ClampMax="40")) float AmbientIdleSeconds = 8.f;
	/** Hard references load with the authored map, before the first spawn. */
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") TSubclassOf<UAnimInstance> AmbientAnimationClass;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") TObjectPtr<USkeletalMesh> AmbientMaleSkin;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") TObjectPtr<USkeletalMesh> AmbientFemaleSkin;
	/** Hip and safe standing poses are relative to the authored marker. */
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient", meta=(MakeEditWidget=true)) FVector AmbientSeatHipLocal = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient", meta=(ClampMin="10",ClampMax="120")) float AmbientSeatSeconds = 45.f;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") TObjectPtr<UStaticMesh> AmbientCupMesh;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") FTransform AmbientCupGripLocal = FTransform::Identity;
	/** Measured source lips in head-bone space and actual cup rim in mesh space. */
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") FVector AmbientMouthHeadLocal = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient") FVector AmbientCupRimLocal = FVector::ZeroVector;
	/** Optional seated shoe grounding. Existing people keep their exact graph and
	 * pose; only measured summer guests opt in with real shoe surface offsets. */
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient|Feet") bool bAmbientGroundFeet = false;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient|Feet", meta=(Units="cm")) FVector AmbientLeftShoeSoleFootLocal = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category="Pedestrian|Ambient|Feet", meta=(Units="cm")) FVector AmbientRightShoeSoleFootLocal = FVector::ZeroVector;
	/** Exact authored chair, ignored only by its occupant's departure path. */
	UPROPERTY(EditInstanceOnly, Category="Pedestrian|Ambient") TObjectPtr<AActor> AmbientSeatSupport;
	ANeonCityNPCCharacter* GetSpawnedPedestrian() const { return Spawned.Get(); }
	bool EnsurePedestrian();
	bool IsSpawnAttemptReady() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
private:
	TWeakObjectPtr<ANeonCityNPCCharacter> Spawned;
	float RetrySpawnAt=0;
};
