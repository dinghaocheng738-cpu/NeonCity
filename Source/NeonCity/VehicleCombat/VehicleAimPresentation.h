#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "VehicleAimPresentation.generated.h"
class UVehicleCombatComponent;
class UStaticMeshComponent;
class USoundBase;
class APlayerController;

/** Everything the gunner's sight shows this frame, in widget (DPI-scaled viewport) space. */
struct FVehicleReticleState
{
 bool bVisible=false,bArmed=false,bImpactOnScreen=false,bOnTarget=false,bWater=false;
 FVector2D Center=FVector2D::ZeroVector,Impact=FVector2D::ZeroVector;
 float RangeMeters=0;
 FString Weapon,Ammo;
 // missile seeker
 bool bSeeker=false,bCandidate=false,bLocked=false,bLost=false;
 float SeekerRadius=0,LockProgress=0;
 FVector2D BoxMin=FVector2D::ZeroVector,BoxMax=FVector2D::ZeroVector;
 // own round in flight (rocket / missile), hit confirmation
 bool bRoundOnScreen=false;FVector2D Round=FVector2D::ZeroVector;float RoundMeters=0,RoundSeconds=0;
 float HitMarker=0;
 double Time=0;
 /** 0..1 over the moment after V arms the weapons (the sight boots up). */
 float Boot=1;
 /** Weapon kind of the selected station: 0 gun / cannon, 1 rockets, 2 guided missile. */
 int32 Mode=0;
 /** The seeker holds the laser spot on a surface rather than a vehicle or person. */
 bool bPointLock=false;
 /** Stores panel: one entry per station (label, rounds), the selected one, missiles on the rails. */
 TArray<FString> Stations;int32 SelectedStation=0;int32 RailMissiles=-1;
};

/** Gunner's sight drawn over the view (Slate primitives, no texture assets). */
UCLASS()
class NEONCITY_API UVehicleReticleWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void SetState(const FVehicleReticleState& InState) { State=InState; }
 const FVehicleReticleState& GetState() const { return State; }
protected:
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
private:
 FVehicleReticleState State;
};

/** The local gunner's aiming presentation for any armed vehicle (helicopter, plane, boat, roof gun):
 * nothing while the weapons are safe; V arms them and the gunship sight boots up: impact pipper with range ring
 * following the round's real (dropping) trajectory to the first thing it would hit, missile seeker field / target
 * box / lock diamond with lock and locked tones, own-missile mark with time of flight, hit marker, stores panel;
 * in the world a laser designator beam and spot, shown while the missile's target is being designated (RMB).
 * Read-only on UVehicleCombatComponent; nothing here changes what a shot does. */
UCLASS(ClassGroup=(NeonCity))
class NEONCITY_API UVehicleAimPresentationComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVehicleAimPresentationComponent();
 /** First blocking point along a round's flight (gravity, the lake surface), from Start with Velocity. */
 static bool PredictImpact(UWorld* World,const AActor* Shooter,const FVector& Start,const FVector& Velocity,float GravityZ,float MaxTime,float MaxRange,FVector& OutPoint,FHitResult& OutHit,bool& bOutWater);
 const FVehicleReticleState& GetLastState() const { return Last; }
 bool IsLaserVisible() const;
 FVector GetAimPoint() const { return AimPoint; }
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 void Hide();
 void UpdateWorldMarks(bool bShow,const FVector& From,const FVector& To,const FVector& Eye);
 void UpdateTones(float Delta,const FVehicleReticleState& S);
 bool bWasArmed=false;double ArmedAt=-10;
 UPROPERTY(Transient) TObjectPtr<UVehicleReticleWidget> Widget;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Beam;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Spot;
 UPROPERTY(Transient) TObjectPtr<USoundBase> LockTone;
 UPROPERTY(Transient) TObjectPtr<USoundBase> LockedTone;
 TWeakObjectPtr<UVehicleCombatComponent> Combat;
 TWeakObjectPtr<APlayerController> Viewer;
 FVehicleReticleState Last;
 FVector AimPoint=FVector::ZeroVector;
 float ToneTimer=0;
};
