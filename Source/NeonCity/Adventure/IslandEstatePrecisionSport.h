#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "Interaction/InteractableInterface.h"
#include "IslandEstatePrecisionSport.generated.h"

class UBoxComponent; class USphereComponent; class UStaticMeshComponent; class UPrimitiveComponent; class UStaticMesh;
class UCameraComponent; class UTextBlock; class UProgressBar; class UAudioComponent;
class UPhysicalMaterial; class UInputMappingContext; class UInputAction;
class UEnhancedInputComponent; class UEnhancedInputLocalPlayerSubsystem;
class USoundWaveProcedural; class ANeonCityPlayerCharacter;
struct FInputActionValue;
enum class EVisibilityBasedAnimTickOption : uint8;

UENUM(BlueprintType)
enum class EIslandPrecisionSport : uint8 { Bowling, Putting };
UENUM(BlueprintType)
enum class EIslandPrecisionStage : uint8 { Idle, Aiming, Charging, Swing, Rolling, Result, Finished };

UCLASS()
class NEONCITY_API UIslandPrecisionSportWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Present(const FText& Text,float Power,bool Charging);
protected:
 virtual TSharedRef<SWidget> RebuildWidget()override;
private:
 UPROPERTY() TObjectPtr<UTextBlock> Body;
 UPROPERTY() TObjectPtr<UProgressBar> Meter;
};

/** Existing presentation graph reads these finite, reachable hand contacts.
 * The post-pose update fits the authored grip to the actual right hand. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UIslandPrecisionParticipant : public UActorComponent
{
 GENERATED_BODY()
public:
 UIslandPrecisionParticipant();
 virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)override;
 TWeakObjectPtr<class AIslandEstatePrecisionSport> Sport;
 bool GetHandTargets(FTransform& Right,FTransform& Left)const;
 float GetBodyLeanDegrees()const;
};

/** Finite practice on the retained estate lane/green. A shot applies one
 * impulse to a genuine rigid body; scoring observes actual pins/cup falls.
 * Session input, camera and movement inhibition have one balanced owner. */
UCLASS()
class NEONCITY_API AIslandEstatePrecisionSport : public AActor, public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandEstatePrecisionSport();
 UPROPERTY(EditAnywhere) FName StationID;
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 UPROPERTY(EditAnywhere) EIslandPrecisionSport Kind=EIslandPrecisionSport::Bowling;
 UPROPERTY(EditAnywhere) FText DisplayName;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> BallMesh;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> PinMesh;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> PutterMesh;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> FlagMesh;
 UPROPERTY(EditAnywhere) TArray<FVector> PinPositions;
 UPROPERTY(EditAnywhere) TArray<FVector> HolePositions;
 UPROPERTY(EditAnywhere) TArray<FVector> TeePositions;
 UPROPERTY(EditAnywhere) FVector ApproachLocal=FVector(-80,0,92);
 UPROPERTY(EditAnywhere) FVector PromptLocal=FVector(-25,0,110);
 UPROPERTY(EditAnywhere,meta=(ClampMin="50",ClampMax="1600")) float MinimumSpeed=480;
 UPROPERTY(EditAnywhere,meta=(ClampMin="50",ClampMax="1600")) float MaximumSpeed=980;
 UPROPERTY(EditAnywhere,meta=(ClampMin=".3",ClampMax="3")) float ChargeSeconds=1.3f;
 UPROPERTY(EditAnywhere,meta=(ClampMin=".01",ClampMax="2")) float RollingDamping=.22f;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Ball;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BallVisual;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Putter;
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Pins;
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Flags;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ActivityCamera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> ImpactAudio;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> RollAudio;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 UFUNCTION(BlueprintCallable) void FinishSession();
 bool IsPlaying()const{return Stage!=EIslandPrecisionStage::Idle;}
 bool HasPresentation()const;
 EIslandPrecisionStage GetStage()const{return Stage;}
 int32 GetScore()const{return Score;}
 int32 GetShotCount()const{return Shots;}
 int32 GetCompletedHoles()const{return CompletedHoles;}
 int32 GetFallenPins()const;
 int32 GetPhysicsHitCount()const{return PhysicsHits;}
 float GetPower()const{return Charge;}
 float GetLaunchSpeed()const{return LaunchSpeed;}
 FVector GetShotStandLocation()const;
 bool IsAddressed()const{return bIgnoredMove;}
 FVector GetBallPosition()const;
 FText GetSessionText()const;
 bool IsFeedbackPlaying()const;
protected:
 virtual void BeginPlay()override;
 virtual void Tick(float Delta)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 bool BeginSession(ANeonCityPlayerCharacter* Player);
 bool InstallInput(APlayerController* PC);
 void RemoveInput();
 void Press(); void Release(); void Aim(const FInputActionValue& Value);
 void LeftAim(); void RightAim(); void Next(); void Restart();
 void ResetPins(); void ResetBall(const FVector& Local);
 void Launch(); void ResolveRoll(); void UpdatePresentation(float Delta);
 void SetAddressed(bool Addressed);
 bool HasHandPose()const;
 FTransform DesiredPropTransform()const;
 bool HandTargets(FTransform& Right,FTransform& Left)const;
 void FollowActualHand();
 friend class UIslandPrecisionParticipant;
 void PrepareAudio(); void Notice(const FText& Text)const;
 bool IsPinFallen(int32 Index)const;
 UFUNCTION() void PhysicsHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
 TWeakObjectPtr<ANeonCityPlayerCharacter> ActivePlayer;
 TWeakObjectPtr<APlayerController> ActiveController;
 TWeakObjectPtr<AActor> PreviousViewTarget;
 TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
 UPROPERTY(Transient) TObjectPtr<UIslandPrecisionSportWidget> Widget;
 UPROPERTY(Transient) TObjectPtr<UIslandPrecisionParticipant> Participant;
 UPROPERTY(Transient) TObjectPtr<UPhysicalMaterial> Surface;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> ImpactSound;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> RollingSound;
 TArray<uint32> Bindings;
 TArray<int16> ImpactPCM,RollingPCM;
 TSet<int32> RemovedPins;
 EIslandPrecisionStage Stage=EIslandPrecisionStage::Idle;
 FVector StartPlayerPosition=FVector::ZeroVector,LastShotLocal=FVector::ZeroVector;
 float Charge=0,AimDegrees=0,SwingSeconds=0,RollingSeconds=0,RestSeconds=0,SessionSeconds=0,LastHitAt=-10,LaunchSpeed=0;
 int32 Frame=0,ThrowInFrame=0,FrameFallen=0,Hole=0,Score=0,Shots=0,CompletedHoles=0,PhysicsHits=0;
 bool bIgnoredMove=false,bOwnerNoSeeBefore=false,bShotLaunched=false,bHoleCaptured=false,bOutOfBounds=false;
 bool bOrientBefore=false,bDesiredRotationBefore=false,bControllerYawBefore=false;
 EVisibilityBasedAnimTickOption AnimTickBefore{};
 bool bUpdateRateBefore=false;
};
