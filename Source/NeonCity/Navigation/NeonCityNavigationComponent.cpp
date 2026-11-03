#include "Navigation/NeonCityNavigationComponent.h"
#include "Navigation/NeonCityNavigationWidget.h"
#include "Navigation/NeonCityWorldMapWidget.h"
#include "Navigation/NeonCityMapData.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Async/Async.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Phone/PhoneComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "Navigation/NeonCityRouteRibbon.h"
const FName UNeonCityNavigationComponent::WaypointOwner(TEXT("Player.Map"));
UNeonCityNavigationComponent::UNeonCityNavigationComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f;
 MapAsset=TSoftObjectPtr<UNeonCityMapData>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/WestLake/Map/DA_WL_Map.DA_WL_Map")));
}
void UNeonCityNavigationComponent::BeginPlay()
{
 Super::BeginPlay();auto* PC=Cast<APlayerController>(GetOwner());
 if(!PC || !PC->IsLocalController()) { SetComponentTickEnabled(false);return; }
 MapData=MapAsset.LoadSynchronous();
 if(MapData && MapData->GetGrid().IsEmpty())MapData=nullptr;          // decoded once here, read by route workers
 Widget=CreateWidget<UNeonCityNavigationWidget>(PC);
 if(Widget) { Widget->SetSource(this);Widget->SetVisibility(ESlateVisibility::HitTestInvisible);Widget->AddToViewport(3); }
}
void UNeonCityNavigationComponent::SetupInput(UInputComponent* Input)
{
 auto* PC=Cast<APlayerController>(GetOwner());auto* Enhanced=Cast<UEnhancedInputComponent>(Input);
 auto* S=PC && PC->GetLocalPlayer()?PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
 if(!Enhanced || !S || MapMapping)return;
 MapMapping=NewObject<UInputMappingContext>(this);MapAction=NewObject<UInputAction>(this);
 MapAction->ValueType=EInputActionValueType::Boolean;MapAction->bConsumeInput=true;
 MapMapping->MapKey(MapAction,EKeys::M);
 Enhanced->BindAction(MapAction,ETriggerEvent::Started,this,&UNeonCityNavigationComponent::ToggleWorldMap);
 S->AddMappingContext(MapMapping,15);
}
bool UNeonCityNavigationComponent::IsWorldMapOpen() const { return WorldMap && WorldMap->IsInViewport(); }
void UNeonCityNavigationComponent::ToggleWorldMap()
{
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->IsLocalController() || !MapData)return;
 if(IsWorldMapOpen())
 {
  WorldMap->RemoveFromParent();PC->SetInputMode(FInputModeGameOnly());PC->SetShowMouseCursor(false);return;
 }
 const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();if(Phone && Phone->IsPhoneOpen())return;
 if(!PC->GetPawn())return;
 if(!WorldMap)WorldMap=CreateWidget<UNeonCityWorldMapWidget>(PC);
 if(!WorldMap)return;
 WorldMap->SetSource(this);WorldMap->AddToViewport(50);
 FInputModeUIOnly Mode;Mode.SetWidgetToFocus(WorldMap->TakeWidget());Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
 PC->SetInputMode(Mode);PC->SetShowMouseCursor(true);
}
bool UNeonCityNavigationComponent::SetWaypoint(const FVector& Location,FText Label,FName PlaceID)
{
 if(Location.ContainsNaN())return false;
 FNeonCityNavigationDestination Value;Value.OwnerID=WaypointOwner;
 // a new id for every new spot, so a moved pin always counts as a new drive goal
 Value.DestinationID=PlaceID.IsNone()?FName(*FString::Printf(TEXT("Waypoint.%d.%d"),FMath::RoundToInt(Location.X/100),FMath::RoundToInt(Location.Y/100))):PlaceID;
 Value.Label=Label.IsEmpty()?NSLOCTEXT("NeonCity","NavWaypoint","Waypoint"):Label;Value.Location=Location;
 if(!SetDestination(Value))return false;
 ++MapRouteRequest;MapRoutePending=false;Display.Route.Reset();Display.UsingMap=false;MapRouteElapsed=99;
 ShowNotice(FText::Format(NSLOCTEXT("NeonCity","NavWaypointSet","Route set: {0}"),Value.Label),4);
 return true;
}
void UNeonCityNavigationComponent::ClearWaypoint()
{
 if(HasWaypoint()){ClearDestination(WaypointOwner);++MapRouteRequest;MapRoutePending=false;}
}
void UNeonCityNavigationComponent::RequestMapRoute(const FVector& Origin,const FVector& Goal,bool Driving)
{
 if(!MapData)return;
 const int32 Request=++MapRouteRequest;MapRoutePending=true;MapRouteDriving=Driving;
 TWeakObjectPtr<UNeonCityNavigationComponent> Self(this);const UNeonCityMapData* Map=MapData;
 Async(EAsyncExecution::ThreadPool,[Self,Map,Origin,Goal,Driving,Request]()
 {
  TArray<FVector> Route;const bool Ok=FNeonCityMapRouter::FindRoute(*Map,Origin,Goal,Driving,Route);
  AsyncTask(ENamedThreads::GameThread,[Self,Route=MoveTemp(Route),Ok,Request]() mutable
  {
   UNeonCityNavigationComponent* Nav=Self.Get();
   if(!Nav || Request!=Nav->MapRouteRequest || !Nav->HasWaypoint())return;
   Nav->MapRoutePending=false;
   if(Ok){Nav->Display.Route=MoveTemp(Route);Nav->Display.UsingMap=true;}
   else{Nav->Display.Route={Nav->Display.Origin,Nav->Display.Goal};Nav->Display.UsingMap=false;}
   Nav->Display.RouteHint=Ok?(Nav->MapRouteDriving?NSLOCTEXT("NeonCity","NavMapDrive","Drive route"):NSLOCTEXT("NeonCity","NavMapWalk","Walking route"))
    :NSLOCTEXT("NeonCity","NavMapNone","No route - head this way");
  });
 });
}
float UNeonCityNavigationComponent::DistanceToRoute(const TArray<FVector>& Route,const FVector& P)
{
 float Best=TNumericLimits<float>::Max();
 for(int32 I=1;I<Route.Num();++I)
  Best=FMath::Min(Best,float(FMath::PointDistToSegment(FVector(P.X,P.Y,0),FVector(Route[I-1].X,Route[I-1].Y,0),FVector(Route[I].X,Route[I].Y,0))));
 return Best;
}
bool UNeonCityNavigationComponent::SetDestination(const FNeonCityNavigationDestination& Value)
{
 if(Value.OwnerID.IsNone() || Value.DestinationID.IsNone() || Value.Label.IsEmpty() || Value.Location.ContainsNaN())return false;
 if(!Value.MovingTarget.IsExplicitlyNull() && (!Value.MovingTarget.IsValid() || Value.MovingTarget->GetWorld()!=GetWorld()))return false;
 const bool Changed=Destination.OwnerID!=Value.OwnerID || Destination.DestinationID!=Value.DestinationID
  || Destination.UseRoadRoute!=Value.UseRoadRoute || Destination.MovingTarget!=Value.MovingTarget
  || (!Value.MovingTarget.IsValid() && !DriveRevisionGoal.Equals(Value.Location,1));
 RouteDirty|=Changed;
 Destination=Value;TracksActor=Value.MovingTarget.IsValid();
 if(Changed)
 {
  ++DriveDestinationRevision;DriveTargetAvailable=TracksActor;
  DriveRevisionGoal=TracksActor?Value.MovingTarget->GetActorLocation():Value.Location;
 }
 return true;
}
uint64 UNeonCityNavigationComponent::GetDriveDestinationRevision() const
{
 // Cruise queries this before writing controls, independently of the HUD's
 // slower tick order. Jitter is measured from the last accepted revision,
 // so many small moves cannot silently carry a goal arbitrarily far away.
 if(TracksActor)
 {
  const bool Available=Destination.MovingTarget.IsValid();
  if(Available!=DriveTargetAvailable || (Available && !Destination.MovingTarget->GetActorLocation().Equals(DriveRevisionGoal,150)))
  {
   ++DriveDestinationRevision;DriveTargetAvailable=Available;
   if(Available)DriveRevisionGoal=Destination.MovingTarget->GetActorLocation();
  }
 }
 return DriveDestinationRevision;
}
void UNeonCityNavigationComponent::ClearDestination(FName OwnerID)
{
 if(OwnerID!=Destination.OwnerID || OwnerID.IsNone())return;
 ++DriveDestinationRevision;DriveTargetAvailable=false;
 Destination=FNeonCityNavigationDestination();Display.Active=false;Display.Route.Reset();RouteDirty=true;TracksActor=false;
}
void UNeonCityNavigationComponent::ShowNotice(FText Text,float Seconds)
{
 if(!FMath::IsFinite(Seconds))return;
 Display.Notice=MoveTemp(Text);NoticeRemaining=FMath::Clamp(Seconds,0.f,15.f);
}
bool UNeonCityNavigationComponent::IsPresented() const
{ return Widget && Widget->IsInViewport() && Widget->GetVisibility()!=ESlateVisibility::Collapsed; }
void UNeonCityNavigationComponent::CacheRoads()
{
 // One scan at startup or explicit destination/cruise change, never per map paint.
 RoadGuide.Reset();
 for(TActorIterator<ANeonCityTrafficRoute> It(GetWorld());It;++It)
 {
  const auto* Spline=It->GetRoadSpline();if(!Spline || Spline->GetSplineLength()<100)continue;
  FNeonCityGuideRoad Road;Road.Closed=Spline->IsClosedLoop();Road.CruiseEligible=It->bCruiseEligible;
  Road.Bidirectional=It->bBidirectional;Road.LaneOffsetCm=It->CruiseLaneOffsetCm;
  Road.SpeedLimitKmh=It->CruiseSpeedLimitKmh;Road.RoadID=It->RoadID.IsNone()?FName(*It->GetPathName()):It->RoadID;
  const bool NativeMeasured=It->ActorHasTag(TEXT("NeonCity.Phase205c.VillageNavigationMeasured228")) || It->ActorHasTag(TEXT("NeonCity.Phase205c.NativeMeasuredRoad"));
  Road.NativeMeasured=NativeMeasured;
  if(NativeMeasured)
  {
   // These linear corridors were collision-surveyed at their exact knots.
   // Uniform distance sampling would cut across short village corners and
   // introduce untested chords. Existing curved roads keep their old cache.
   const int32 Knots=Spline->GetNumberOfSplinePoints();bool Linear=Knots>=2 && Knots<=4096;
   for(int32 I=0;I<Knots && Linear;++I)Linear=Spline->GetSplinePointType(I)==ESplinePointType::Linear;
   if(!Linear){UE_LOG(LogTemp,Warning,TEXT("Measured road %s rejected: expected a bounded linear corridor"),*Road.RoadID.ToString());continue;}
   for(int32 I=0;I<Knots;++I)Road.Points.Add(Spline->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World));
  }
  else
  {
   const int32 Count=FMath::Clamp(FMath::CeilToInt(Spline->GetSplineLength()/250.f),2,4096);
   for(int32 I=0;I<Count;++I)Road.Points.Add(Spline->GetLocationAtDistanceAlongSpline(Spline->GetSplineLength()*I/(Road.Closed?Count:Count-1),ESplineCoordinateSpace::World));
  }
  for(FVector& Point:Road.Points)Point.Z-=It->CruiseSurfaceOffsetCm;
  RoadGuide.AddRoad(MoveTemp(Road));
 }
 CachedRoads=true;RouteDirty=true;
}
void UNeonCityNavigationComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);auto* PC=Cast<APlayerController>(GetOwner());
 APawn* Pawn=PC?PC->GetPawn().Get():nullptr;if(!PC || !PC->IsLocalController())return;
 if(!CachedRoads && GetWorld()->HasBegunPlay())CacheRoads();
 else if(RouteDirty)CacheRoads();
 const auto* Phone=PC->FindComponentByClass<UPhoneComponent>();
 if(Widget)Widget->SetVisibility(Pawn && (!Phone || !Phone->IsPhoneOpen())?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 NoticeRemaining=FMath::Max(0.f,NoticeRemaining-Delta);if(NoticeRemaining==0)Display.Notice=FText::GetEmpty();
 PresentRibbon(Pawn);
 if(!Pawn){if(Destination.DestinationID.IsNone())RouteDirty=false;return;}
 Display.Origin=Pawn->GetActorLocation();Display.Heading=Pawn->GetActorRotation().Yaw;
 if(TracksActor && !Destination.MovingTarget.IsValid())
 { ShowNotice(NSLOCTEXT("NeonCity","NavUnavailable","Destination unavailable"));ClearDestination(Destination.OwnerID); }
 Display.Active=!Destination.DestinationID.IsNone();if(!Display.Active){RouteDirty=false;return;}
 Display.Goal=TracksActor?Destination.MovingTarget->GetActorLocation():Destination.Location;
 Display.Label=Destination.Label;Display.DistanceMeters=FVector::Dist2D(Display.Origin,Display.Goal)/100;
 RouteElapsed+=Delta;
 if(RouteDirty || (RouteElapsed>=.5f && (!RoutedOrigin.Equals(Display.Origin,150) || !RoutedGoal.Equals(Display.Goal,150))))
 {
  RouteElapsed=0;RouteDirty=false;RoutedOrigin=Display.Origin;RoutedGoal=Display.Goal;
  if(HasWaypoint() && MapData)
  {
   Display.UsingRoad=false;                                            // the map route arrives asynchronously
   if(Display.Route.Num()<2){Display.Route={Display.Origin,Display.Goal};Display.RouteHint=NSLOCTEXT("NeonCity","NavMapPlanning","Planning route");}
  }
  else
  {
   Display.UsingMap=false;
   Display.UsingRoad=Destination.UseRoadRoute && RoadGuide.FindRoute(Display.Origin,Display.Goal,Display.Route);
   if(!Display.UsingRoad)Display.Route={Display.Origin,Display.Goal};
   Display.RouteHint=Display.UsingRoad?NSLOCTEXT("NeonCity","NavRoad","Road guide"):NSLOCTEXT("NeonCity","NavDirect","Direct guide");
  }
 }
 if(HasWaypoint() && MapData)
 {
  // arrival; otherwise re-plan when the player leaves the route, changes between walking and driving, or every 6 s
  const bool Driving=Pawn->IsA<ANeonCityBaseVehicle>();
  if(Display.DistanceMeters<(Driving?25.f:12.f))
  {
   ShowNotice(FText::Format(NSLOCTEXT("NeonCity","NavArrived","Arrived: {0}"),Display.Label),5);ClearWaypoint();return;
  }
  MapRouteElapsed+=Delta;
  const bool Off=!Display.UsingMap || DistanceToRoute(Display.Route,Display.Origin)>(Driving?3000.f:2000.f);
  if(!MapRoutePending && (MapRouteElapsed>=6.f || (Off && MapRouteElapsed>=1.f) || Driving!=MapRouteDriving))
  {
   MapRouteElapsed=0;RequestMapRoute(Display.Origin,Display.Goal,Driving);
  }
  else if(Display.UsingMap && Display.Route.Num()>=2)Display.Route[0]=Display.Origin;
 }
}
void UNeonCityNavigationComponent::PresentRibbon(APawn* Pawn)
{
 // a planned route (map grid or road network) is laid on the ground ahead; a bare straight line is not
 const bool bShow=Pawn && Display.Active && (Display.UsingMap || Display.UsingRoad) && Display.Route.Num()>=2;
 if(!bShow){if(Ribbon.IsValid())Ribbon->Clear();return;}
 if(!Ribbon.IsValid())
 {
  FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Ribbon=GetWorld()->SpawnActor<ANeonCityRouteRibbon>(FVector::ZeroVector,FRotator::ZeroRotator,P);
 }
 if(Ribbon.IsValid())Ribbon->Present(Display.Route,Display.Origin,Pawn->IsA<ANeonCityBaseVehicle>(),GetWorld()->GetTimeSeconds());
}
void UNeonCityNavigationComponent::EndPlay(EEndPlayReason::Type Reason)
{
 if(Ribbon.IsValid())Ribbon->Destroy();Ribbon.Reset();
 ++MapRouteRequest;                                                       // drop any route still in flight
 if(WorldMap)WorldMap->RemoveFromParent();WorldMap=nullptr;
 if(auto* PC=Cast<APlayerController>(GetOwner());PC && PC->GetLocalPlayer() && MapMapping)
  if(auto* S=PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())S->RemoveMappingContext(MapMapping);
 if(Widget)Widget->RemoveFromParent();Widget=nullptr;RoadGuide.Reset();Super::EndPlay(Reason);
}
