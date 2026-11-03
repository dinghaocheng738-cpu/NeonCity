#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/NeonCityRoadGuide.h"
#include "NeonCityNavigationComponent.generated.h"
class UNeonCityNavigationWidget;
class UNeonCityWorldMapWidget;
class UNeonCityMapData;
class UInputAction;
class UInputMappingContext;
class UInputComponent;
class ANeonCityRouteRibbon;

USTRUCT(BlueprintType)
struct NEONCITY_API FNeonCityNavigationDestination
{
 GENERATED_BODY()
 // Stable gameplay publisher and destination identities; never actor names.
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName OwnerID;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName DestinationID;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FText Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Location=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TWeakObjectPtr<AActor> MovingTarget;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool UseRoadRoute=false;
};

struct FNeonCityNavigationDisplay
{
 FVector Origin=FVector::ZeroVector,Goal=FVector::ZeroVector;
 float Heading=0,DistanceMeters=0;
 FText Label,RouteHint,Notice;
 bool Active=false,UsingRoad=false;
 /** The route came from the map's walk / drive grid (a player waypoint). */
 bool UsingMap=false;
 TArray<FVector> Route;
};

/** Local controller presentation. Survives possession; owns no money or mission state. */
UCLASS(ClassGroup=(NeonCity),meta=(BlueprintSpawnableComponent))
class NEONCITY_API UNeonCityNavigationComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UNeonCityNavigationComponent();
 UFUNCTION(BlueprintCallable) bool SetDestination(const FNeonCityNavigationDestination& Value);
 UFUNCTION(BlueprintCallable) void ClearDestination(FName OwnerID);
 UFUNCTION(BlueprintCallable) void ShowNotice(FText Text,float Seconds=6);
 const FNeonCityNavigationDestination& GetDestination() const { return Destination; }
 /** Drive-goal changes require the driver's explicit acceptance; label-only
  * presentation edits do not. Moving targets have a bounded spatial revision. */
 uint64 GetDriveDestinationRevision() const;
 const FNeonCityNavigationDisplay& GetDisplay() const { return Display; }
 const FNeonCityRoadGuide& GetRoadGuide() const { return RoadGuide; }
 void RefreshRoadGuide() { CacheRoads(); }
 bool IsPresented() const;
 /** The painted map of the area, when the world has one (West Lake). */
 const UNeonCityMapData* GetMapData() const { return MapData; }
 /** Player waypoint: guided over the map's paths and roads (walking) or roads (driving). */
 bool SetWaypoint(const FVector& Location,FText Label,FName PlaceID=NAME_None);
 void ClearWaypoint();
 bool HasWaypoint() const { return Destination.OwnerID==WaypointOwner; }
 /** M: the full-screen map (click to set a waypoint). */
 void ToggleWorldMap();
 bool IsWorldMapOpen() const;
 UNeonCityWorldMapWidget* GetWorldMapWidget() const { return WorldMap; }
 /** The route laid on the ground ahead of the player (chevrons flowing to the goal). */
 ANeonCityRouteRibbon* GetRouteRibbon() const { return Ribbon.Get(); }
 void SetupInput(UInputComponent* Input);
 static const FName WaypointOwner;
 UPROPERTY(EditAnywhere,Category="NeonCity|Navigation") TSoftObjectPtr<UNeonCityMapData> MapAsset;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
 void CacheRoads();
 void RequestMapRoute(const FVector& Origin,const FVector& Goal,bool Driving);
 static float DistanceToRoute(const TArray<FVector>& Route,const FVector& P);
 UPROPERTY(Transient) FNeonCityNavigationDestination Destination;
 UPROPERTY(Transient) TObjectPtr<UNeonCityMapData> MapData;
 UPROPERTY(Transient) TObjectPtr<UNeonCityWorldMapWidget> WorldMap;
 UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MapMapping;
 UPROPERTY(Transient) TObjectPtr<UInputAction> MapAction;
 int32 MapRouteRequest=0;bool MapRoutePending=false,MapRouteDriving=false;float MapRouteElapsed=0;
 UPROPERTY(Transient) TObjectPtr<UNeonCityNavigationWidget> Widget;
 TWeakObjectPtr<ANeonCityRouteRibbon> Ribbon;
 void PresentRibbon(APawn* Pawn);
 FNeonCityNavigationDisplay Display;
 FNeonCityRoadGuide RoadGuide;
 FVector RoutedOrigin=FVector::ZeroVector,RoutedGoal=FVector::ZeroVector;
 float RouteElapsed=0,NoticeRemaining=0;
 bool CachedRoads=false,RouteDirty=true,TracksActor=false;
 mutable uint64 DriveDestinationRevision=0;
 mutable FVector DriveRevisionGoal=FVector::ZeroVector;
 mutable bool DriveTargetAvailable=false;
};
