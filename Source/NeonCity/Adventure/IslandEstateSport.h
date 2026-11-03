#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Interaction/InteractableInterface.h"
#include "Adventure/IslandTennisRules.h"
#include "IslandEstateSport.generated.h"
class ACharacter; class ANeonCityPlayerCharacter; class APlayerController;
class UBoxComponent; class UStaticMeshComponent; class USkeletalMesh; class UAnimInstance;
class UStaticMesh; class UCameraComponent; class UTextRenderComponent; class USoundBase; class USoundAttenuation;
class UEnhancedInputComponent; class UEnhancedInputLocalPlayerSubsystem; class UInputMappingContext; class UInputAction;
enum class EVisibilityBasedAnimTickOption : uint8;

/** Read-only hand contacts consumed by the existing finite limb IK. Locomotion
 * stays in the existing animation graph; no replacement player controller. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UIslandTennisParticipant:public UActorComponent
{
 GENERATED_BODY()
public:
 UIslandTennisParticipant();
 virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)override;
 UPROPERTY(Transient) TObjectPtr<class AIslandEstateSport> Court;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Racket;
 FVector ContactLocal=FVector(85,35,30);
 float SwingAge=10;
 float PreviousSwingAge=10;
 FTransform PreviousRacket=FTransform::Identity;
 EVisibilityBasedAnimTickOption PreviousVisibilityTick;
 bool PreviousUpdateRateOptimization=false;
 bool GetHandTargets(FTransform& Right,FTransform& Left)const;
 void UpdateRacket(float Delta);
};

UCLASS()
class NEONCITY_API AIslandEstateSport:public AActor,public IInteractableInterface
{
 GENERATED_BODY()
public:
 AIslandEstateSport();
 UPROPERTY(EditAnywhere) FName PropertyID=TEXT("Property.Island.Aurelia");
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> BallAsset;
 UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> RacketAsset;
 UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> OpponentAsset;
 UPROPERTY(EditAnywhere) TSubclassOf<UAnimInstance> OpponentAnimation;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> HitSound;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> BounceSound;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> PointSound;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundAttenuation> SoundAttenuation;
 /** Relative to the regulation court centre; prompt hardware at a dry sideline. */
 UPROPERTY(EditAnywhere) FVector ConsoleLocal=FVector(-1450,-680,105);
 UPROPERTY(EditAnywhere,meta=(ClampMin="200",ClampMax="650")) float OpponentSpeed=440;
 UPROPERTY(EditAnywhere,meta=(ClampMin="0.12",ClampMax="0.8")) float OpponentReaction=.24f;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Console;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ball;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CourtCamera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> ScoreDisplay;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 bool IsActive()const{return Player.IsValid();}
 ANeonCityPlayerCharacter* GetPlayer()const{return Player.Get();}
 ACharacter* GetOpponent()const{return Opponent.Get();}
 const FIslandTennisRules& GetRules()const{return Rules;}
 FVector GetBallLocal()const{return BallLocal;}
 FVector GetBallVelocityLocal()const{return BallVelocity;}
 int32 GetPlayerContacts()const{return PlayerContacts;}
 int32 GetAIContacts()const{return AIContacts;}
 float GetOpponentDistance()const{return OpponentDistance;}
 float GetMaxContactPlaneDistance()const{return MaxContactPlaneDistance;}
 float GetMaxContactEllipse()const{return MaxContactEllipse;}
 bool IsRacketFeedbackPlaying()const;
 void EndSession();
 /** Same actions called by the scoped real input adapter; safe for automation. */
 void BeginShot(); void ReleaseShot(); void Serve();
protected:
 virtual void BeginPlay()override;
 virtual void Tick(float Delta)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 bool StartSession(ANeonCityPlayerCharacter* P);
 bool BindInput(APlayerController* PC);
 void RemoveInput();void Consume(){}
 UIslandTennisParticipant* AddParticipant(ACharacter* P);
 void StepBall(float Delta);void StepOpponent(float Delta);
 void ResolveShot(ACharacter* Body,int32 Side);
 bool RacketContact(ACharacter* Body,int32 Side,const FVector& Before,const FVector& After,float Alpha0,float Alpha1);
 void QueueSwing(ACharacter* Body,const FVector& WorldContact);
 void Launch(int32 Side,const FVector& Target,float Apex);
 void FinishPoint();void Present();void Sound(USoundBase* Asset,const FVector& At,float Pitch=1);
 FVector ToLocal(const FVector& World)const{return GetActorTransform().InverseTransformPosition(World);}
 FVector ToWorld(const FVector& Local)const{return GetActorTransform().TransformPosition(Local);}
 TWeakObjectPtr<ANeonCityPlayerCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<ACharacter> Opponent;
 UPROPERTY(Transient) TObjectPtr<UIslandTennisParticipant> PlayerPose;
 UPROPERTY(Transient) TObjectPtr<UIslandTennisParticipant> AIPose;
 TWeakObjectPtr<APlayerController> Viewer;
 TWeakObjectPtr<AActor> PreviousView;
 TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
 TArray<uint32> Bindings;
 FIslandTennisRules Rules;
 FVector BallLocal=FVector(-1260,0,120),BallVelocity=FVector::ZeroVector,LastAIPosition=FVector::ZeroVector;
 float ChargeAge=0,Power=.5f,PointAge=0,AIAge=0,ContactAudioAt=-10;
 float OpponentDistance=0;
 float MaxContactPlaneDistance=0,MaxContactEllipse=0;
 int32 PlayerContacts=0,AIContacts=0;
 bool Charging=false,PlayerShotPending=false,AIShotPending=false;
 bool OrientBefore=true,ControllerFacingBefore=false;
 FString Feedback=TEXT("Practice singles | serve from behind your baseline");
};
