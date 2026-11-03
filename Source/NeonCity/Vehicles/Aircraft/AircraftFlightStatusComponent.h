#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AircraftFlightStatusComponent.generated.h"
class UVehicleStatusWidget;
class UVehicleCombatComponent;

/** Local pilot's instruments. Read-only presentation of existing flight/combat state. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UAircraftFlightStatusComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UAircraftFlightStatusComponent();
 void UpdateStatus(float Delta);
 void NotifyExitDenied(bool Airborne);
 void Clear();
 bool IsVisible() const;
 const FText& GetDisplayedStatus() const { return DisplayedStatus; }
protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UVehicleStatusWidget> Widget;
 TWeakObjectPtr<UVehicleCombatComponent> Combat;
 FText DisplayedStatus,ExitFeedback;
 float Elapsed=0,FeedbackRemaining=0;
};
