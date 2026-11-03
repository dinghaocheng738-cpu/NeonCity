#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "IslandResidenceLift.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;
class UPointLightComponent;
class ACharacter;

/** Physical, interlocked residence lift. CMC carries riders on its
 * moving base; neither riders nor cabin are teleported between storeys. */
UCLASS()
class NEONCITY_API AIslandResidenceLift : public AActor, public IInteractableInterface, public IDynamicSurfaceInteractionInterface
{
 GENERATED_BODY()
public:
 AIslandResidenceLift();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USceneComponent> Cabin;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Floor;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Interior;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Glazing;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> ControlPanelFace;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> CabinDoors;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> LandingDoors;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<UBoxComponent>> CallPanels;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float FloorSpacing=420;
 /** Empty arrays preserve the existing five equally spaced landings. Explicit
  * heights allow the same lift to serve reference-derived storey intervals. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Residence|Landings") TArray<float> LandingHeightsCm;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Residence|Landings") TArray<FText> LandingNames;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float TravelSpeed=125;
 UFUNCTION(BlueprintPure) int32 GetLandingCount() const;
 UFUNCTION(BlueprintPure) float GetLandingHeightCm(int32 Level) const;
 virtual bool CanInteract_Implementation(AActor* Visitor) const override;
 virtual FText GetInteractionName_Implementation() const override;
 virtual void Interact_Implementation(AActor* Visitor) override;
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 UFUNCTION(BlueprintPure) int32 GetCurrentFloor() const {return CurrentFloor;}
 UFUNCTION(BlueprintPure) bool IsTravelling() const {return bMoving;}
 UFUNCTION(BlueprintPure) bool AreDoorsOpen() const {return GetLandingCount()>1 && DoorFraction>.98f && !bMoving && !bQueued;}
 /** Parked-lift saves use the real clear landing, like a parked vehicle's
  * existing dry exit checkpoint; a lift in transit cannot be saved. */
 bool FindPersistenceExit(const ACharacter* Visitor,FTransform& Destination) const;
 virtual void OnConstruction(const FTransform& Transform) override;
protected:
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UBoxComponent> CabinPanel;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Indicator;
 UPROPERTY() TObjectPtr<UPointLightComponent> CabinLight;
 int32 CurrentFloor=0,TargetFloor=0;
 float DoorFraction=1,VelocityZ=0;
 bool bMoving=false,bQueued=false;
 bool bReportedObstruction=false;
 bool InCabin(const AActor* Visitor) const;
 bool HasValidLandingProfile() const;
 FText LandingName(int32 Level) const;
 int32 ClosestFloor(const AActor* Visitor) const;
 bool DoorOccupied(int32 Level) const;
 void PresentDoors();
 bool TryMoveCabin(float DeltaZ);
};
