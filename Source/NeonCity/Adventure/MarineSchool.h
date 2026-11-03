#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Adventure/MarineSpearDeath.h"
#include "MarineSchool.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UFishData;
class UNeonCityFishingSubsystem;
struct FNeonCityCaughtFish;

UENUM(BlueprintType)
enum class EMarineBehaviour : uint8 { School, Hover, Forage, Cruise, Benthic };

/** One simulation/draw component per habitat school. Instance IDs remain stable after a catch. */
UCLASS()
class NEONCITY_API AMarineSchool : public AActor
{
 GENERATED_BODY()
public:
 AMarineSchool();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Bodies;
 UPROPERTY(EditAnywhere, Category="Ecology") TObjectPtr<UFishData> Species;
 UPROPERTY(EditAnywhere, Category="Ecology") TObjectPtr<UStaticMesh> CreatureMesh;
 UPROPERTY(EditAnywhere, Category="Ecology") EMarineBehaviour Behaviour=EMarineBehaviour::School;
 UPROPERTY(EditAnywhere, Category="Ecology") bool bSideways=false;
 UPROPERTY(EditAnywhere, Category="Ecology") FName HabitatID;
 UPROPERTY(EditAnywhere, Category="Ecology") TArray<FTransform> Starts;
 UPROPERTY(EditAnywhere, Category="Ecology") int32 Seed=205102;
 UPROPERTY(EditAnywhere, Category="Ecology") float HomeRadius=1000;
 UPROPERTY(EditAnywhere, Category="Ecology") float CruiseSpeed=40;
 UPROPERTY(EditAnywhere, Category="Ecology") float Spacing=35;
 UPROPERTY(EditAnywhere, Category="Ecology") float FloorClearance=35;
 UPROPERTY(EditAnywhere, Category="Ecology") float SurfaceClearance=100;
 UPROPERTY(EditAnywhere, Category="Ecology") float MeanSeaLevel=-130;
 UPROPERTY(EditAnywhere, Category="Ecology") float SwimFrequency=2;
 UPROPERTY(EditAnywhere, Category="Ecology") float SwimAmplitude=.035f;
 UPROPERTY(EditAnywhere, Category="Ecology") bool bCapturable=true;
 // Terrain sampled during authoring. Local XY spans [-FloorExtent,+FloorExtent].
 UPROPERTY(EditAnywhere, Category="Seabed") int32 FloorResolution=0;
 UPROPERTY(EditAnywhere, Category="Seabed") float FloorExtent=1800;
 UPROPERTY(EditAnywhere, Category="Seabed") TArray<float> FloorHeights;
 // Authored reef/rock envelopes, relative to this school: XYZ centre, W radius.
 UPROPERTY(EditAnywhere, Category="Seabed") TArray<FVector4> Obstacles;
 UFUNCTION(BlueprintCallable, CallInEditor, Category="Ecology") void RebuildSchool();
 bool IsAlive(int32 Index) const;
 bool TryCapture(int32 Index, UNeonCityFishingSubsystem* Inventory);
 bool CanHook(int32 Index) const;
 bool ReserveHook(int32 Index,FGuid Ticket,FNeonCityCaughtFish& Catch);
 bool ReserveSpear(int32 Index,FGuid Ticket,FVector Hit,FVector Direction,FNeonCityCaughtFish& Catch);
 bool GetSpearAttachment(int32 Index,FGuid Ticket,FTransform& Pose,bool& Collectible) const;
 bool PullSpear(int32 Index,FGuid Ticket,FVector Delta);
 EMarineSpearLife GetSpearLife(int32 Index) const;
 bool MoveHook(int32 Index,FGuid Ticket,FVector Target,bool Hooked,bool Landed);
 bool FinishHook(int32 Index,FGuid Ticket,UNeonCityFishingSubsystem* Inventory,const FNeonCityCaughtFish& Catch,bool Keep);
 bool NearestCreature(const FVector& WorldPoint,float& DistanceSquared,FVector& Result) const;
 FVector CreaturePosition(int32 Index) const;
 int32 LivingCount() const;
 bool HasReservedCreatures() const;
 TSet<int32> GetDepletedIndices() const;
 void ApplyDepletedIndices(const TSet<int32>& Indices);
 void StartleAt(FVector WorldPoint,float Radius);
 float FloorAt(FVector LocalPoint) const;
 virtual void Tick(float Delta) override;
 virtual void OnConstruction(const FTransform& Transform) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 struct FCreature
 {
  FVector Position=FVector::ZeroVector,Velocity=FVector::ForwardVector;
  FQuat Rotation=FQuat::Identity;
  float Scale=1,Phase=0,Alarm=0,TurnBend=0;
  FVector AlarmSource=FVector::ZeroVector;
  bool bAlive=true;
  FGuid HookTicket;
  FVector HookTarget=FVector::ZeroVector;
  bool bHooked=false,bLanded=false;
  FMarineSpearDeath Death;
 };
 TArray<FCreature> Creatures;
 FRandomStream Random;
 FVector Goal=FVector::ZeroVector;
 float GoalClock=0,PauseClock=0;
 bool bActive=false;
 bool IsPresent(int32 Index) const { return Creatures.IsValidIndex(Index) && Creatures[Index].bAlive; }
 void PickGoal();
 void WritePresentation(int32 Index,float Time,float Window,bool bMoving);
};
