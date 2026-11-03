#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SecurityCamera.generated.h"
class UPoliceSensorComponent;
UCLASS()
class NEONCITY_API ASecurityCamera : public AActor
{
 GENERATED_BODY()
public:
 ASecurityCamera();
 UPoliceSensorComponent* GetSensor() const { return Sensor; }
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPoliceSensorComponent> Sensor;
};
