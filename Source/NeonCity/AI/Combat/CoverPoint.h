#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoverPoint.generated.h"
class ACharacter;
UENUM(BlueprintType)
enum class ENeonCityCoverStance : uint8 { Standing, Crouched };
/** Authored standing capsule-centre anchor; +X faces the threat. No assumed protection. */
UCLASS()
class NEONCITY_API ACoverPoint : public AActor
{
 GENERATED_BODY()
public:
 ACoverPoint();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonCityCoverStance Stance=ENeonCityCoverStance::Standing;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float PeekDistance=160;
 bool IsAvailableFor(const AActor* User) const;
 bool Reserve(ACharacter* User,FVector ThreatAim);
 void Release(const AActor* User);
 AActor* GetOccupant() const { return Occupant.Get(); }
 bool ProvidesCover(ACharacter* User,FVector ThreatAim) const;
 bool FindPeek(ACharacter* User,FVector ThreatAim,FVector& OutPoint) const;
private:
 TWeakObjectPtr<AActor> Occupant;
};
