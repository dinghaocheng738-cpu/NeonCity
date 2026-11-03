#pragma once
#include "CoreMinimal.h"
#include "Adventure/IslandHomeSeat.h"
#include "Adventure/IslandLandGrill.h"
#include "IslandEstateAmenity.generated.h"
class UStaticMesh; class UStaticMeshComponent; class UTextRenderComponent; class UAudioComponent;
class UPoseableMeshComponent; class USoundWaveProcedural; class ANeonCityPlayerCharacter; class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EIslandEstateAmenityKind : uint8 { Read, Drink, Wash, Shower, Bath, Inspect, Storage, Meal };
UENUM(BlueprintType)
enum class EIslandEstateAmenityStage : uint8 { Idle, Entering, Active, Leaving };

/** Finite use of a real estate furnishing. Seats and bath rests retain the
 * existing HomeSeat safe-standing save domain. Standing uses never disable
 * movement, change possession or replace the player's normal input mapping. */
UCLASS()
class NEONCITY_API AIslandEstateAmenity : public AIslandHomeSeat
{
 GENERATED_BODY()
public:
 AIslandEstateAmenity();
 UPROPERTY(EditAnywhere) FName AmenityID;
 UPROPERTY(EditInstanceOnly) TObjectPtr<AActor> TargetFixture;
 UPROPERTY(EditAnywhere) EIslandEstateAmenityKind Kind=EIslandEstateAmenityKind::Read;
 UPROPERTY(EditAnywhere) FText AmenityName;
 UPROPERTY(EditAnywhere) bool bUseSeatedPose=true;
 UPROPERTY(EditAnywhere,meta=(ClampMin="4",ClampMax="120")) float SessionSeconds=24;
 UPROPERTY(EditAnywhere) TArray<FText> Pages;
 UPROPERTY(EditAnywhere) FVector ObjectRestLocal=FVector(26,0,94);
 UPROPERTY(EditAnywhere) FVector RightHandLocal=FVector(30,19,93);
 UPROPERTY(EditAnywhere) FVector LeftHandLocal=FVector(30,-19,93);
 UPROPERTY(EditAnywhere) FVector WaterEmptyLocal=FVector::ZeroVector;
 UPROPERTY(EditAnywhere) FVector WaterFullLocal=FVector(0,0,28);
 UPROPERTY(EditAnywhere) FVector OpenPartOffset=FVector(0,-30,0);
 /** Actual normal walking stance at a fitted standing faucet; never teleports. */
 UPROPERTY(EditAnywhere) FVector StandLocal=FVector(52,72,0);
 UPROPERTY(EditAnywhere) FVector FacingLocal=FVector(0,-1,0);
 UPROPERTY(EditAnywhere) float StandToleranceCm=24;
 /** Opt-in for other standing fixtures; accepted seated/legacy fittings keep
  * their current behavior. Wash/shower always require their fitted stance. */
 UPROPERTY(EditAnywhere) bool bRequireFittedStance=false;
 UPROPERTY(EditAnywhere) float WashLeanDegrees=42;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeldObject;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WaterSurface;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WaterStream;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MovingPart;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Details;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Feedback;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> TaskBody;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 bool FinishUse(bool Completed=false);
 bool IsUsing()const{return Stage!=EIslandEstateAmenityStage::Idle;}
 bool HasPresentation()const;
 EIslandEstateAmenityStage GetStage()const{return Stage;}
 float GetUseSeconds()const{return UseSeconds;}
 float GetWaterFraction()const{return WaterFraction;}
 float GetDrinkFraction()const{return DrinkFraction;}
 int32 GetPageIndex()const{return PageIndex;}
 int32 GetCompletedUses()const{return CompletedUses;}
 bool IsFeedbackPlaying()const;
 bool CanReachFixture(const ANeonCityPlayerCharacter* Player)const;
protected:
 virtual void BeginPlay()override;
 virtual void Tick(float Delta)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 void BeginUse(ANeonCityPlayerCharacter* Player);
 void Cleanup();
 void Present(float Delta);
 void PrepareAudio();
 void Notice(const FText& Text)const;
 void PoseHands();
 void FitArm(FName Upper,FName Lower,FName Hand,const FVector& Target,const FVector& Pole);
 TWeakObjectPtr<ANeonCityPlayerCharacter> ActivePlayer;
 TWeakObjectPtr<USkeletalMeshComponent> PoseSource;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> Sound;
 TArray<int16> SoundPCM;
 EIslandEstateAmenityStage Stage=EIslandEstateAmenityStage::Idle;
 FVector StartingLocation=FVector::ZeroVector;
 FTransform MovingPartClosed;
 float UseSeconds=0,WaterFraction=0,DrinkFraction=1;
 int32 PageIndex=0,CompletedUses=0;
 bool bSourceVisible=true,bLeftBefore=false,bRightBefore=false;
 uint8 SourceTickBefore=0;
 FVector StreamScaleBefore=FVector::OneVector;
 bool bRestoreMesh=false;
 FGuid PendingMeal;
 bool bMealConsumed=false;
};

/** Indoor counter presentation of the existing fish reservation/cooking/meal
 * module. It remains an IslandLandGrill for current hand IK, menu and save. */
UCLASS()
class NEONCITY_API AIslandEstateCooktop : public AIslandLandGrill
{
 GENERATED_BODY()
public:
 AIslandEstateCooktop();
 UPROPERTY(EditInstanceOnly) TObjectPtr<AActor> TargetCooktop;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> CooktopMesh;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pan;
protected:
 virtual void BeginPlay()override;
};
