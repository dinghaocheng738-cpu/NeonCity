#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeonCityFoliageInteractionComponent.generated.h"

class ANeonCityReferenceVegetation;
class UStaticMesh;
class UStaticMeshComponent;
class UCapsuleComponent;
class UPhysicalMaterial;
class APawn;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FNeonCitySoftFoliageContact,const FVector&,float,float);

/** Spatially bounded wood collision and root-anchored soft-plant springs.
 *  Visual instances keep their source identity; there is no Actor/Tick per tree.
 */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityFoliageInteractionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityFoliageInteractionComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 void RebuildIndex();
 /** World contact point, moving speed (cm/s), spring deflection target (cm). */
 FNeonCitySoftFoliageContact OnSoftContact;
 int32 GetActiveWoodCount() const { return ActiveWood.Num()+ActivePalms.Num(); }
 int32 GetAllocatedWoodCount() const { return WoodPool.Num()+PalmPool.Num(); }
 int32 GetActivePalmCount() const { return ActivePalms.Num(); }
 int32 GetActiveSpringCount() const { return Springs.Num(); }
 int32 GetLastCandidateCount() const { return LastCandidates; }
 int32 GetCapacityMissCount() const { return CapacityMisses; }
 FVector GetPlantBend(int32 Group,int32 Instance) const;
 static constexpr int32 MaximumWoodBodies=512;
 static constexpr int32 MaximumPalmTrees=16;
 static constexpr int32 MaximumSprings=192;
 /** Cooked bark-only geometry. Leaves can never become blocking surfaces. */
 UPROPERTY(EditAnywhere,Category="NeonCity|Foliage") TSoftObjectPtr<UStaticMesh> WoodCollisionMesh;
private:
 struct FPlant
 {
  int32 Group=0,Instance=0;
  FVector Root=FVector::ZeroVector;
  float Radius=100,Height=180;
  bool Wood=false,Palm=false;
 };
 struct FSpring
 {
  FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector,Target=FVector::ZeroVector;
 };
 struct FContact
 {
  FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;
  float Radius=42,HalfHeight=96;
 };
 UPROPERTY(Transient) TObjectPtr<ANeonCityReferenceVegetation> Vegetation;
 UPROPERTY(Transient) TObjectPtr<UStaticMesh> WoodMesh;
 UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> WoodMaterial;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> WoodPool;
 UPROPERTY(Transient) TArray<TObjectPtr<UCapsuleComponent>> PalmPool;
 TArray<FPlant> Plants;
 TMap<FIntPoint,TArray<int32>> WoodCells,SoftCells;
 TMap<int32,int32> ActiveWood;
 TMap<int32,int32> ActivePalms;
 TMap<int32,FSpring> Springs;
 TArray<int32> FreeWood;
 TArray<int32> FreePalms;
 TArray<TWeakObjectPtr<APawn>> NearbyPawns;
 TArray<FContact> Contacts;
 float ObserverElapsed=1,WoodElapsed=1,FeedbackElapsed=0;
 int32 LastCandidates=0,CapacityMisses=0;
 void GatherContacts(float Delta);
 void UpdateWood();
 void UpdatePalms(const TArray<int32>& Order);
 void UpdateSprings(float Delta);
 void WriteBend(int32 Plant,const FVector& Bend,TSet<int32>& DirtyGroups);
};
