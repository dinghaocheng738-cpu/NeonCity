#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceResponseDirector.generated.h"
class ANeonCityTrafficRoute;
class APoliceVehicle;
class APoliceCharacter;
class UWantedStatusWidget;
/** Authored station positions only. One officer and at most two cars per world session. */
UCLASS()
class NEONCITY_API APoliceResponseDirector : public AActor
{
 GENERATED_BODY()
public:
 APoliceResponseDirector();
 UPROPERTY(EditInstanceOnly,Category="Police") TObjectPtr<ANeonCityTrafficRoute> Road;
 UPROPERTY(EditAnywhere,Category="Police") FVector FootPatrolOrigin=FVector(-5400,1180,135);
 UPROPERTY(EditAnywhere,Category="Police") float FirstRoadEntry=1800;
 UPROPERTY(EditAnywhere,Category="Police") float SecondRoadEntry=9500;
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UWantedStatusWidget> StatusWidget;
};
