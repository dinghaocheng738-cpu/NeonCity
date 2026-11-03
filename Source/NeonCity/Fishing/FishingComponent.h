#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Fishing/FishData.h"
#include "Fishing/FishingSessionModel.h"
#include "FishingComponent.generated.h"
class AFishingStation;
class ANeonCityPlayerCharacter;
class UFishingWidget;
class UInputComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UStaticMeshComponent;
class USplineMeshComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UMaterialInstanceDynamic;
class UAnimSequence;

/** Local activity adapter on the controller; no template movement or weapon implementation. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UFishingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UFishingComponent();
 void SetupInput(UInputComponent* Input);
 bool CanStart(const AFishingStation* Point) const;
 bool Start(AFishingStation* Point);
 void Stop();
 bool IsActive() const { return ActiveStation.IsValid(); }
 const FNeonCityFishingSession& GetSession() const { return Session; }
 const FNeonCityCaughtFish& GetPendingCatch() const { return PendingCatch; }
 AFishingStation* GetActiveStation() const { return ActiveStation.Get(); }
 bool IsCookingAt(const AFishingStation* Station) const { return ActiveStation.Get()==Station && CookingFish.IsValid(); }
 float GetCookingSeconds() const { return CookSeconds; }
 FText GetDisplayText() const;
 bool GetHandTargets(FTransform& Right,FTransform& Left) const;
 UStaticMeshComponent* GetRodVisual() const { return Rod; }
 int32 GetKeptCatchCount() const { return KeptCatchCount; }
 bool IsReelAudioPlaying() const;
 float GetCastBodyMotion() const;
 int32 GetCastPoseIndex() const;
 float GetCastPoseTime() const;
 float GetCastMotionCurve(FName Name) const;
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 ANeonCityPlayerCharacter* Player() const;
 void Press();
 void Release();
 void Keep();
 void ReleaseCatch();
 void MenuAction();
 void NextItem();
 void PreviousItem();
 void OpenPhone();
 void ConsumeInput() {}
 void ResetCast();
 bool FindEncounter();
 void UpdateEncounter(float Delta);
 void RemoveInput();
 void CreateVisuals();
 void UpdateVisuals(float Delta);
 void DestroyVisuals();
 void UpdateWidget();
 TArray<FNeonCityCaughtFish> Pouch() const;
 TWeakObjectPtr<AFishingStation> ActiveStation;
 TWeakObjectPtr<ANeonCityPlayerCharacter> ActivePlayer;
 TWeakObjectPtr<AFishingStation> FocusedStation;
 TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
 TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
 UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY() TArray<TObjectPtr<UInputAction>> Actions;
 TArray<uint32> Bindings;
 UPROPERTY() TObjectPtr<UFishingWidget> Widget;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Rod;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ReelParts;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RodMaterial;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Float;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> CatchVisual;
 UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> Line;
 UPROPERTY() TObjectPtr<UAudioComponent> ReelAudio;
 UPROPERTY() TObjectPtr<USoundBase> SplashSound;
 UPROPERTY() TObjectPtr<USoundAttenuation> SplashAttenuation;
 UPROPERTY() TObjectPtr<UAnimSequence> WindupMotion;
 UPROPERTY() TObjectPtr<UAnimSequence> CastMotion;
 ENeonCityFishingState LastPresentedState=ENeonCityFishingState::Ready;
 FNeonCityFishingSession Session;
 FNeonCityCaughtFish PendingCatch;
 FRandomStream Random;
 FVector WaterTarget=FVector::ZeroVector;
 FVector BaitTarget=FVector::ZeroVector,HookPosition=FVector::ZeroVector;
 FString HabitatDescription;
 float EncounterSearchClock=0;
 float RodPitch=24,RodYaw=0,RodBend=0,RodBendVelocity=0,ReelCrank=0,ReelRotor=0,ReelBail=0,ReelRate=0,ReelSpool=0,LastLineOut=0;
 float RodLoad=.15f;
 float ReelMeasuredRate=0;
 float VisualStateAge=0;
 FVector RodBendDirection=FVector(0,0,-1);
 FVector CastOrigin=FVector::ZeroVector,CastVelocity=FVector::ZeroVector;
 bool bCastLaunched=false;
 bool bDeepRig=false;
 float StartHealth=0,RuleAccumulator=0,VisualClock=0,CookSeconds=0;
 FGuid CookingFish;
 bool bIgnoredMove=false;
 int32 SelectedItem=0,KeptCatchCount=0;
 FString Notice;
};
