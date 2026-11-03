#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Physics/PhysicalImpactEvent.h"
#include "PhysicalImpactSubsystem.generated.h"
class UPhysicalImpactComponent;
class UImpactDamageData;
class UPhysicalMaterialResponseData;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPhysicalReset);
UCLASS()
class NEONCITY_API UPhysicalImpactSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 void Submit(UPhysicalImpactComponent* Source,AActor* Other,const FHitResult& Hit);
 void ResetForLoad();
 UFUNCTION(BlueprintCallable,Category="NeonCity|Physics") bool ConfigureProfiles(UImpactDamageData* DamageProfile,UPhysicalMaterialResponseData* SurfaceProfile);
 UPROPERTY(BlueprintAssignable) FPhysicalImpactSignal OnImpact;
 UPROPERTY(BlueprintAssignable) FPhysicalReset OnReset;
 UPROPERTY() TObjectPtr<UImpactDamageData> Tuning;
 UPROPERTY() TObjectPtr<UPhysicalMaterialResponseData> Materials;
 const TArray<FPhysicalImpactEvent>& GetRecentEvents() const { return RecentEvents; }
 int32 GetImpactCount() const { return ImpactCount; }
private:
 void Deliver(FPhysicalImpactEvent Event,EImpactTargetKind Kind,float Amount);
 struct FPair { TWeakObjectPtr<AActor> A,B; float Until=0; };
 TArray<FPair> Pairs;
 TArray<FPhysicalImpactEvent> RecentEvents;
 int32 ImpactCount=0;
};
