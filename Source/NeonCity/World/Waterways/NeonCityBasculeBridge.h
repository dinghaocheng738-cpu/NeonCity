#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityBasculeBridge.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class UAudioComponent;
class UStaticMesh;
class ANeonCityMarineVehicleBase;

UENUM(BlueprintType)
enum class ENeonCityBridgeState : uint8 { Closed, Warning, Opening, Open, Closing };

/** A double-leaf bascule bridge carrying a road over a canal. The canal runs along the actor's X axis, the road
 * along Y. When a vessel too tall for the closed span comes up the canal (any boat whose air draft exceeds the
 * clearance, e.g. the swan yacht), the bridge warns road traffic (flashing lights, bell, barrier arms down), waits
 * for its deck to clear, raises both leaves, and lowers them again once the vessel has passed.
 * Leaves, barrier arms and piers are meshes from wl_waterway.py; the leaves and arms block while closed / down. */
UCLASS()
class NEONCITY_API ANeonCityBasculeBridge : public AActor
{
 GENERATED_BODY()
public:
 ANeonCityBasculeBridge();
 /** Clear span between the hinges (cm) and carriageway + footway width (cm). */
 UPROPERTY(EditAnywhere,Category="Bridge") float Span=1900;
 UPROPERTY(EditAnywhere,Category="Bridge") float DeckWidth=2600;
 /** Water level to the closed deck's soffit (cm); the actor's origin is the road surface on the canal axis,
  * the leaves hinge at road level. Any craft taller than this calls the bridge. */
 UPROPERTY(EditAnywhere,Category="Bridge") float Clearance=40;
 UPROPERTY(EditAnywhere,Category="Bridge") float OpenAngle=78;
 UPROPERTY(EditAnywhere,Category="Bridge") float TravelSeconds=11;
 /** Leaf / abutment meshes (default: the road bridge's) and the deck width they are modelled at (cm); the parts are
  * stretched along the canal to DeckWidth. The Hubin promenade's footbridge uses SM_WL_FootLeaf / SM_WL_FootPier. */
 UPROPERTY(EditAnywhere,Category="Bridge") TObjectPtr<UStaticMesh> LeafMesh;
 UPROPERTY(EditAnywhere,Category="Bridge") TObjectPtr<UStaticMesh> PierMesh;
 UPROPERTY(EditAnywhere,Category="Bridge") float ModelWidth=2600;
 /** A tall vessel this far up / down the canal (cm) and heading for the bridge has it opened. */
 UPROPERTY(EditAnywhere,Category="Bridge") float CallRange=22000;
 ENeonCityBridgeState GetState() const { return State; }
 float GetOpenFraction() const { return Open; }
 /** Open now (gameplay / tests), as if a vessel had called it. */
 void RequestOpen() { bForced=true; }
protected:
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
private:
 bool VesselCalls() const;
 bool DeckClear() const;
 void Present();
 UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeafA;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LeafB;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PierA;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PierB;
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Barriers;
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UPointLightComponent>> Signals;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> DeckZone;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Bell;
 ENeonCityBridgeState State=ENeonCityBridgeState::Closed;
 float Open=0,Arms=0,StateTime=0,QuietTime=0,Blink=0;
 bool bForced=false;
};
