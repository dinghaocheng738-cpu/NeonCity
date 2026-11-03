#pragma once
#include "CoreMinimal.h"
#include "Adventure/IslandHomeSeat.h"
#include "IslandLeisureFacility.generated.h"
class UStaticMeshComponent; class UTextRenderComponent; class UAudioComponent;
class USoundWaveProcedural; class UMaterialInstanceDynamic; class UAnimSequence;
class AIslandScenePanel;
class UCameraComponent;
UENUM()
enum class EIslandLeisureKind:uint8 { Cinema, ExerciseBike, Sauna };
UENUM()
enum class EIslandLeisureStage:uint8 { Idle, Entering, Active, Leaving };
/** A finite use of actual furnishings, using the existing owned-home seat,
 * player, gradual rest and safe standing save domain. No separate rewards. */
UCLASS()
class NEONCITY_API AIslandLeisureFacility:public AIslandHomeSeat
{
 GENERATED_BODY()
public:
 AIslandLeisureFacility();
 UPROPERTY(EditAnywhere) EIslandLeisureKind Kind=EIslandLeisureKind::Cinema;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> ExerciseEnter;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> ExerciseLoop;
 UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> ExerciseExit;
 UPROPERTY(EditAnywhere) TObjectPtr<AIslandScenePanel> ScenePanel;
 UPROPERTY(EditAnywhere,meta=(ClampMin="20",ClampMax="180")) float SessionLength=30;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Furniture;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Crank;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeftPedal;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RightPedal;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Screen;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Heater;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Display;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Feedback;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CinemaCamera;
 virtual bool CanInteract_Implementation(AActor* Visitor)const override;
 virtual FText GetInteractionName_Implementation()const override;
 virtual void Interact_Implementation(AActor* Visitor)override;
 EIslandLeisureStage GetStage()const{return Stage;}
 float GetElapsed()const{return Elapsed;}
 float GetTemperature()const{return Temperature;}
 float GetCrankRadians()const{return CrankRadians;}
 int32 GetCompletedSessions()const{return CompletedSessions;}
 bool IsFeedbackPlaying()const;
protected:
 virtual void BeginPlay()override;
 virtual void Tick(float Delta)override;
 virtual void EndPlay(EEndPlayReason::Type Reason)override;
private:
 void StartSession(); void StopSession(bool Completed); void RequestExit();
 void UpdatePresentation(); void PrepareSound(); void Notice(const FString& Text);
 EIslandLeisureStage Stage=EIslandLeisureStage::Idle;
 float Elapsed=0,StageElapsed=0,Temperature=22,CrankRadians=0;
 int32 CompletedSessions=0,SceneBefore=0;
 bool bExitRequested=false;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> ProceduralSound;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FilmMaterial;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HeaterMaterial;
 TArray<int16> SoundPCM;
 TWeakObjectPtr<APlayerController> CinemaViewer;
 TWeakObjectPtr<AActor> PreviousViewTarget;
};
