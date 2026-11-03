#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/DamageEvent.h"
#include "Explosion/ExplosionData.h"
#include "ExplosionDamageComponent.generated.h"
/** Phase 19 compatibility adapter: a one-shot payload detonation routed through the unified explosion subsystem. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UExplosionDamageComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UExplosionDamageComponent() { PrimaryComponentTick.bCanEverTick=false; }
 UFUNCTION(BlueprintCallable) int32 Detonate(FVector Center,float Radius,const FNeonCityDamageEvent& Payload,AActor* IgnoredHost);
 static float Falloff(float Distance,float Radius);
private:
 bool bDetonated=false;
 UPROPERTY() TObjectPtr<UNeonCityExplosionData> AdapterProfile;
};