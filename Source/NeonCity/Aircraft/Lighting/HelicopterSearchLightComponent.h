#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HelicopterSearchLightComponent.generated.h"
class USpotLightComponent;
class USearchLightData;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UHelicopterSearchLightComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UHelicopterSearchLightComponent();
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<USearchLightData> SearchLightData;
 UFUNCTION(BlueprintCallable) void SetEnabled(bool Enabled);
 UFUNCTION(BlueprintCallable) void ToggleSearchLight() { SetEnabled(!bEnabled); }
 UFUNCTION(BlueprintPure) bool IsEnabled() const { return bEnabled; }
 UFUNCTION(BlueprintCallable) void SetAimOffset(FRotator Offset);
 UFUNCTION(BlueprintCallable) void ResetForLoad();
 USpotLightComponent* GetLight() const { return Light; }
 UStaticMeshComponent* GetHousing() const { return Housing; }
 FRotator GetAimRotation() const;
 const USearchLightData* GetProfile() const;
 bool IsHighQuality() const { return bHighQuality; }
 float GetUpdateInterval() const { return UpdateInterval; }
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 void UpdateLight(float Delta);
 void RotateOptics(float Delta);
 UPROPERTY() TObjectPtr<USpotLightComponent> Light;
 UPROPERTY() TObjectPtr<UStaticMesh> HousingMesh;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Housing;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Lens;
 FRotator AimOffset=FRotator::ZeroRotator;
 bool bEnabled=false,bHighQuality=false;
 float Elapsed=0,UpdateInterval=.2f;
};
