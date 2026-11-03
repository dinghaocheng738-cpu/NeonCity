#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterDamageFeedbackEffect.generated.h"
class UStaticMeshComponent;
/** Bounded prototype impact marker used until authored Niagara assets exist. No gore, no dismemberment. */
UCLASS()
class NEONCITY_API ACharacterDamageFeedbackEffect : public AActor
{
 GENERATED_BODY()
public:
 ACharacterDamageFeedbackEffect();
 /** Spawn-then-configure-then-finish keeps BeginPlay after the tuning is applied. */
 static ACharacterDamageFeedbackEffect* Spawn(UWorld* World,FVector Location,float Radius,float Lifetime,FLinearColor Color);
 void Configure(float Radius,float Lifetime,FLinearColor Color);
 float GetLifetime() const { return Life; }
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
private:
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Marker;
 FVector BaseScale=FVector::OneVector;
 float Life=.5f,Age=0;
};
