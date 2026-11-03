#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FixedWingTestArea.generated.h"
class ANeonCityFixedWingAircraft;
/** Opt-in native test infrastructure outside the existing city footprint. No map assets overwritten. */
UCLASS()
class NEONCITY_API AFixedWingTestArea : public AActor
{
 GENERATED_BODY()
public:
 AFixedWingTestArea();
 static FGuid PrototypeID() { return FGuid(0x18C00001,0x53424541,0x4348504C,0x414E4531); }
 static FTransform StandTransform() { return FTransform(FRotator(0,180,0),FVector(15000,7000,467)); }
 ANeonCityFixedWingAircraft* GetAircraft() const { return Aircraft; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<ANeonCityFixedWingAircraft> Aircraft;
};
