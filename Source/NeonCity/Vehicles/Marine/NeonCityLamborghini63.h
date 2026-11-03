#pragma once
#include "CoreMinimal.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "NeonCityLamborghini63.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class ULocalLightComponent;
class UInputAction;

UENUM(BlueprintType)
enum class ENeonCityDriveMode : uint8 { Docking, Cruising, Corsa };
/** What F does for a person near the 63. */
UENUM(BlueprintType)
enum class ENeonCityYachtAction : uint8 { None, TakeHelm, Board, ClimbAboard, GoAshore };

/** Tecnomar for Lamborghini 63: the player's 19.2 m carbon-fibre supercar yacht (24 t, twin MAN V12 2000 hp,
 * 63 kn). Driven like the real boat: the Lamborghini drive-mode selector (M) chooses Docking (fine low-speed
 * control, 6 kn), Cruising (40 kn) or Corsa (full power, 63 kn); the helm eye is the starboard bucket seat behind
 * the Huracan wheel. LED lights, screens and underwater lights glow through the city night parameter; real ambient
 * lights (cabins always on, deck / cockpit / underwater fading in with the night) light the boat around them.
 * Model: Tools/Art/Phase205cUpgrade/Waterway/lmb63.py (SM_WL_LMB63, SM_WL_LMB63Glass). */
UCLASS()
class NEONCITY_API ANeonCityLamborghini63 : public ANeonCityMarineVehicleBase
{
 GENERATED_BODY()
public:
 ANeonCityLamborghini63();
 virtual FText GetInteractionName_Implementation() const override;
 virtual bool CanInteract_Implementation(AActor* Interactor) const override;
 virtual void Interact_Implementation(AActor* Interactor) override;
 virtual bool QueryStandingSurface_Implementation(const FHitResult& Hit,const ACharacter* Character,FDynamicStandingSurface& Surface) const override;
 /** The action F performs for this person now: the helm when standing at it, boarding from the quay, climbing out
  * of the water at the swim platform, stepping ashore from the stern while alongside. */
 ENeonCityYachtAction GetActionFor(const ACharacter* Character) const;
 bool IsAboard(const ACharacter* Character) const;
 virtual float GetAirDraft() const override { return 530.f; }
 ENeonCityDriveMode GetDriveMode() const { return DriveMode; }
 void SetDriveMode(ENeonCityDriveMode Mode) { DriveMode=Mode; }
 /** km/h the current mode allows. */
 float GetModeSpeedLimit() const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Glass;
 /** Trim, inlays, lights, badges and fittings: drawn, never collided with (the body carries the walking surfaces). */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Detail;
 /** Ambient lights: the cabins below (always on) and the deck, cockpit, canopy and underwater lights (night). */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<ULocalLightComponent>> AmbientLights;
 /** The passerelle: swung out from the swim platform to the quay while she lies alongside with nobody at the helm,
  * so people walk aboard over the water gap (and ashore again). The tread box is what they walk on. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Gangway;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> GangwayTeak;
 bool IsGangwayOut() const { return bGangwayOut; }
 /** 0 by day .. 1 at night: how far the exterior ambient lights are up. */
 float GetExteriorLightLevel() const { return ExteriorLevel; }
 /** Another owner's 63 in its own colours: the hull paint (the Paint slot, Arancio Argos by default) is repainted
  * in LiveryColor, same pearl clear coat. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Livery") bool bCustomLivery=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Livery",meta=(EditCondition="bCustomLivery")) FLinearColor LiveryColor=FLinearColor(.06f,.3f,.24f);
 virtual void OnConstruction(const FTransform& Transform) override;
protected:
 virtual void BeginPlay() override;
 virtual void ApplyDriverInput(float ThrottleValue,float SteeringValue,float BrakeValue) override;
 virtual FText BuildStatusText(const struct FMarineDriveTelemetry& T,const FText& State) const override;
 virtual void BindExtraHelmKeys(class UEnhancedInputComponent* Input,class UInputMappingContext* Context) override;
private:
 void CycleDriveMode();
 void ApplyLivery();
 bool FindAshore(const ACharacter* Character,FTransform& Out) const;
 bool PlaceAboard(ACharacter* Character,const FVector& Local) const;
 void UpdateAmbientLights();
 void UpdateGangway();
 bool FindQuay(FVector& Hinge,FVector& Foot) const;
 TArray<float> AmbientLumens;TArray<bool> AmbientExterior;float ExteriorLevel=-1;FTimerHandle AmbientTimer;
 bool bGangwayOut=false;FTransform GangwayBerth;
 UPROPERTY(Transient) TObjectPtr<UInputAction> ModeAction;
 ENeonCityDriveMode DriveMode=ENeonCityDriveMode::Cruising;
};
