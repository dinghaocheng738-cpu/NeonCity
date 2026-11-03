#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionEvent.h"
#include "ExplosionComponent.generated.h"
/** Actor-side explosion adapter. Data selection and counters live here; execution lives in the subsystem. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityExplosionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityExplosionComponent();
 /** Optional authored asset. Without one the native profile for ExplosionDataID is used. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion") TObjectPtr<UNeonCityExplosionData> ExplosionProfile;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Explosion") FName ExplosionDataID=TEXT("NeonCity.SmallExplosion");
 UFUNCTION(BlueprintPure,Category="Explosion") const UNeonCityExplosionData* GetExplosionData() const { return ExplosionProfile?ExplosionProfile.Get():&NeonCityExplosions::Profile(ExplosionDataID); }
 UFUNCTION(BlueprintCallable,Category="Explosion") bool SetExplosionDataID(FName ID);
 UFUNCTION(BlueprintPure,Category="Explosion") int32 GetDetonationCount() const { return Detonations; }
 UFUNCTION(BlueprintCallable,Category="Explosion")
 FNeonCityExplosionResult DetonateAt(FVector Center,AController* Instigator,AActor* SourceActor,AActor* IgnoredActor=nullptr,int32 ChainDepth=0,FGuid ParentExplosionId=FGuid());
 FNeonCityExplosionResult DetonateAtOwnLocation(AController* Instigator,int32 ChainDepth=0,FGuid ParentExplosionId=FGuid());
 void ResetCounters() { Detonations=0; }
private:
 int32 Detonations=0;
};