#pragma once
#include "CoreMinimal.h"
#include "Fishing/FishingStation.h"
#include "Fishing/FishData.h"
#include "IslandLandGrill.generated.h"
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class USoundWaveProcedural;
class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;
enum class EVisibilityBasedAnimTickOption : uint8;

/** Outdoor cooking presentation; fish ownership remains in the fishing system. */
UCLASS()
class NEONCITY_API AIslandLandGrill : public AFishingStation
{
 GENERATED_BODY()
public:
 AIslandLandGrill();
 /** Same fish reservation/meal domain, fitted indoor hob presentation. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bIndoorCooktop=false;
 /** Fitted hob pivot is 19cm inside a 65cm half-depth counter. 92cm
  * leaves 4cm clearance even for the retained 42cm capsule. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="70",ClampMax="140")) float IndoorWorkDistanceCm=92.f;
 FVector GetWorkPositionLocal(float Side=0) const { return FVector(Side,bIndoorCooktop?IndoorWorkDistanceCm:-74.f,0); }
 /** Low front controls need the same articulated reach as outdoor ignition.
  * Other cooking stages retain the established25 degree working pose. */
 float GetCookingBodyLeanDegrees() const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Coals;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Lid;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Tongs;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> TongJaw;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Food;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Plate;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
 UFUNCTION(BlueprintPure) bool IsLit() const { return bLit; }
 UFUNCTION(BlueprintPure) bool IsCooking() const { return bCooking; }
 bool IsCookingAudioPlaying() const;
 bool BeginCook(FGuid FishID,AActor* Visitor);
 void LeaveCooking(AActor* Visitor);
 bool StorePlatedMeal();
 bool EatPlatedMeal(AActor* Visitor);
 bool GetCookingHandTargets(FTransform& Right,FTransform& Left) const;
 ENeonCityGrillStage GetCookingStage() const { return Stage; }
 float GetStageFraction() const;
 FText GetCookingInstructions() const;
 FGuid GetReservedFishID() const { return ReservedFishID; }
 FGuid GetPlatedMealID() const { return PlatedMealID; }
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UPointLightComponent> Heat;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CoalMaterial;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Sizzle;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> SizzleWave;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FoodMaterial;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FoodMaterials;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Steam;
 TArray<int16> SizzlePCM;
 TWeakObjectPtr<AActor> CookVisitor;
 TWeakObjectPtr<USkeletalMeshComponent> CookingPoseMesh;
 TWeakObjectPtr<UCharacterMovementComponent> CookingMovement;
 bool CookingOrientToMovementBefore=false;
 EVisibilityBasedAnimTickOption CookingTickBefore;
 bool CookingUpdateRateBefore=false;
 void SetCookVisitor(AActor* Visitor);
 FGuid ReservedFishID,PlatedMealID;
 FName PresentedSpecies;
 ENeonCityGrillStage Stage=ENeonCityGrillStage::Idle;
 float StageSeconds=0,CookFraction=0,FoodSizeCm=0;
 float LidOpen=0;
 FTransform RightHand,LeftHand;
 void PresentSequence(float Delta);
 void ChangeStage(ENeonCityGrillStage Next);
 void RestoreReservation();
 bool bLit=false,bCooking=false;
 float IdleTime=0;
};
