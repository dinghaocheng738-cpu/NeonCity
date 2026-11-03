#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "World/NeonCityMainlandSite.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

namespace MainlandDrive920
{
static bool LaneEndpointMatchesDestination(const FNeonCityRoadGuide& Guide,const FVector& Goal,const FVector& Endpoint,float& AuthoredOffset)
{
 AuthoredOffset=-1;
 for(const auto& Road:Guide.GetRoads())if(Road.CruiseEligible)
  for(int32 I=1;I<Road.Points.Num();++I)
  {
   const FVector A=Road.Points[I-1],B=Road.Points[I];
   if(FVector::Dist(FMath::ClosestPointOnSegment(Goal,A,B),Goal)>1)continue;
   const FVector Tangent=(B-A).GetSafeNormal2D();const float Offset=FMath::Abs(Road.LaneOffsetCm);
   // Compare to the actual road tangent. With exact measured knots, the final
   // route segment can still taper from the preceding junction to this lane.
   if(!Tangent.IsNearlyZero() && FVector::Dist2D(Endpoint,Goal)<=Offset+1
    && FMath::Abs(Endpoint.Z-Goal.Z)<=1
    && FMath::Abs(FVector::DotProduct(Endpoint-Goal,Tangent))<=1)
   {AuthoredOffset=Offset;return true;}
  }
 return false;
}

static bool ValidateNativeGraph(UWorld* World,UNeonCityNavigationComponent* Navigation,FAutomationTestBase* Test)
{
 if(!World || !Navigation || !ANeonCityMainlandSite::Find(World))
 {Test->AddError(TEXT("Native road connectivity requires the adopted C920 world and production navigation"));return false;}
 struct FExpectedRoad { const TCHAR* ID;FVector A,B; };
 const FExpectedRoad Expected[]={
  {TEXT("C920.QuayRear"),FVector(-16000,2500,40),FVector(16000,2500,40)},
  {TEXT("C920.WestLane"),FVector(-10000,2500,40),FVector(-10000,8000,40)},
  {TEXT("C920.GardenStreet"),FVector(-10000,8000,40),FVector(10000,8000,40)},
  {TEXT("C920.EastLane"),FVector(10000,8000,40),FVector(10000,2500,40)}};
 TMap<FName,ANeonCityTrafficRoute*> Actual;
 for(TActorIterator<ANeonCityTrafficRoute> I(World);I;++I)if(I->RoadID.ToString().StartsWith(TEXT("C920.")))
 {
  if(Actual.Contains(I->RoadID)){Test->AddError(TEXT("Duplicate C920 native road identity: ")+I->RoadID.ToString());return false;}
  Actual.Add(I->RoadID,*I);
 }
 if(Actual.Num()!=UE_ARRAY_COUNT(Expected))
 {Test->AddError(FString::Printf(TEXT("Expected four exact C920 roads, found%d"),Actual.Num()));return false;}
 for(const auto& E:Expected)
 {
  ANeonCityTrafficRoute* const* Found=Actual.Find(FName(E.ID));
  if(!Found){Test->AddError(FString(TEXT("Missing native road "))+E.ID);return false;}
  auto* Actor=*Found;const auto* Spline=Actor->GetRoadSpline();
  if(!Actor->ActorHasTag(TEXT("NeonCity.Phase205c.NativeMeasuredRoad")) || !Spline
   || Spline->IsClosedLoop() || Spline->GetNumberOfSplinePoints()!=2
   || Spline->GetSplinePointType(0)!=ESplinePointType::Linear || Spline->GetSplinePointType(1)!=ESplinePointType::Linear
   || !Spline->GetLocationAtSplinePoint(0,ESplineCoordinateSpace::World).Equals(E.A,.01)
   || !Spline->GetLocationAtSplinePoint(1,ESplineCoordinateSpace::World).Equals(E.B,.01)
   || !Actor->bCruiseEligible || !Actor->bBidirectional
   || !FMath::IsNearlyEqual(Actor->CruiseLaneOffsetCm,170.f,.01f)
   || !FMath::IsNearlyEqual(Actor->CruiseSpeedLimitKmh,35.f,.01f)
   || !FMath::IsNearlyZero(Actor->CruiseSurfaceOffsetCm,.01f))
  {Test->AddError(FString::Printf(TEXT("Native road%s violates its surveyed two-knot linear/profile/measured-tag contract at%s"),E.ID,*Actor->GetPathName()));return false;}
 }
 Navigation->RefreshRoadGuide();const auto& Guide=Navigation->GetRoadGuide();int32 Cached=0;
 for(const auto& Road:Guide.GetRoads())if(Actual.Contains(Road.RoadID))
 {
  if(!Road.NativeMeasured || Road.Points.Num()!=2 || !Road.CruiseEligible)
  {Test->AddError(TEXT("Production navigation did not retain exact measured knots for ")+Road.RoadID.ToString());return false;}
  ++Cached;
 }
 if(Cached!=UE_ARRAY_COUNT(Expected)){Test->AddError(TEXT("Production navigation omitted a C920 measured road"));return false;}
 // These are read-only query origins, not actor fixtures. The final origin is
 // the actual Drive2 west-stop pose. Neither the car nor player is moved here.
 const FVector Origins[]={FVector(9830,6200,75),FVector(8276.679,7797.140,75.445),FVector(-9846.560,4277.477,74.535)};
 const FVector Headings[]={FVector(0,1,0),FVector(-1,0,0),FVector(0,-1,0)};
 const FVector Goals[]={FVector(8000,8000,40),FVector(-10000,4000,40),FVector(3500,2500,40)};
 for(int32 Leg=0;Leg<UE_ARRAY_COUNT(Origins);++Leg)
 {
  TArray<FVector> Route;float Limit=0;bool Looped=false;
  if(!Guide.FindDriveRoute(Origins[Leg],Headings[Leg],&Goals[Leg],Route,Limit,Looped) || Route.Num()<2 || Looped)
  {Test->AddError(FString::Printf(TEXT("Production graph cannot connect leg%d from%s toward%s"),Leg,*Origins[Leg].ToString(),*Goals[Leg].ToString()));return false;}
  float AuthoredOffset=0;
  if(!FMath::IsNearlyEqual(Limit,35.f,.01f)
   || !LaneEndpointMatchesDestination(Guide,Goals[Leg],Route.Last(),AuthoredOffset))
  {Test->AddError(FString::Printf(TEXT("Native graph leg%d reaches an incorrect endpoint or speed policy: %s %.1fkm/h"),Leg,*Route.Last().ToString(),Limit));return false;}
  if(Leg==2 && !Route.ContainsByPredicate([](const FVector& P){return P.Equals(FVector(-10000,2500,40),1);}))
  {Test->AddError(TEXT("West-to-quay route omits the actual surveyed endpoint-to-interior junction"));return false;}
  for(const FVector& Point:Route)
  {
   bool OnRoad=false;
   for(const auto& E:Expected)
    if(FVector::Dist2D(Point,FMath::ClosestPointOnSegment(Point,E.A,E.B))<=171 && FMath::Abs(Point.Z-40)<=1)OnRoad=true;
   if(!OnRoad){Test->AddError(FString::Printf(TEXT("Native graph leg%d invents an unsupported road point%s"),Leg,*Point.ToString()));return false;}
  }
  Test->AddInfo(FString::Printf(TEXT("Read-only native graph leg%d: %d ordered lane knots, origin%s, endpoint%s, limit%.1fkm/h"),Leg,Route.Num(),*Origins[Leg].ToString(),*Route.Last().ToString(),Limit));
 }
 return true;
}

class FNativeRoadConnectivity : public IAutomationLatentCommand
{
public:
 explicit FNativeRoadConnectivity(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
  if(Now-Started>45){Test->AddError(TEXT("Native graph validation exceeded its45-second startup bound"));return true;}
  UWorld* World=NeonCityValidationWorld();APlayerController* Controller=World?World->GetFirstPlayerController():nullptr;
  if(!Controller || World->GetTimeSeconds()<8)return false;
  const bool Valid=ValidateNativeGraph(World,Controller->FindComponentByClass<UNeonCityNavigationComponent>(),Test);
  Test->TestTrue(TEXT("Actual authored roads connect all three production navigation queries"),Valid);
  Test->AddInfo(TEXT("Read-only graph configuration validation; no player/car fixture, input, or physical driving evidence is claimed."));
  return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;
};

class FContinuousDrive : public IAutomationLatentCommand
{
public:
 explicit FContinuousDrive(FAutomationTestBase* InTest,bool InExitRepro=false):Test(InTest),
  Metrics(InExitRepro?TEXT("MainlandC920-DriveGarageExitRepro"):TEXT("MainlandC920-ContinuousDrive"),true),ExitRepro(InExitRepro){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();if(!Started)Started=Now;
  if(Now-Started>(ExitRepro?90:360))return End(ExitRepro?TEXT("Isolated garage-exit tail exceeded its90-second wall-clock bound"):TEXT("Authored mainland drive exceeded its 360-second wall-clock bound"));
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(!PC || W->GetTimeSeconds()<8)return false;
  Metrics.Sample(W,FString::Printf(TEXT("drive-phase%02d-cruise-leg%02d-manual-point%02d-walk-point%02d"),Stage,CruiseLeg,Point,WalkPoint));
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
   if(!Player || !ANeonCityMainlandSite::Find(W))return End(TEXT("Normal player and adopted C920 mainland required"));
   for(TActorIterator<AIslandSupercar> I(W);I;++I)
    if(FVector::Dist2D(I->GetActorLocation(),FVector(12460,4150,90))<150)
    {
     if(Car)return End(TEXT("More than one hero car occupies the first authored court bay"));
     Car=*I;
    }
   if(!Car)return End(TEXT("The actual first-bay mainland hero car is absent; no substitute is spawned"));
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I)
    if(I->RoadID.ToString().StartsWith(TEXT("C920.")))Roads.Add(*I);
   if(Roads.Num()!=4)return End(TEXT("The four adopted C920 road guides are required"));
   Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();
   if(!Nav)return End(TEXT("Production player navigation is unavailable"));
   if(!ExitRepro && !ValidateNativeGraph(W,Nav,Test))return End(TEXT("Authored graph preflight failed before any player fixture or drive input"));
   OriginalDestination=Nav->GetDestination();
   if(ExitRepro)
   {
    // Isolated failed-tail fixture: Drive3 recorded this final cruise arrival,
    // but did not record car yaw. East-facing yaw0 is therefore approximate.
    // CSV XYZ are view positions, never a claimed final car/player transform.
    // Only this initial setup moves the car; the five original driveway goals,
    // normal F entry/exit and actual CharacterMovement remain unchanged.
    Car->SetActorTransform(FTransform(FRotator(0,0,0),FVector(3222.225,2671.382,74.532)),false,nullptr,ETeleportType::TeleportPhysics);
    Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    Car->GetMesh()->WakeAllRigidBodies();
    Test->AddInfo(TEXT("Disclosed isolated Drive3 tail fixture: recorded final cruise arrival (3222.225,2671.382,74.532)cm with approximate east-facing yaw0; no claim of an uninterrupted rerun or exact final parked pose. Subsequent car travel/entry/exit/walking use normal native controls and collision."));
   }
   FVector Stand;
   if(!Phase205cVehicleApproach(W,Player,Car,Player->GetActorLocation(),Stand))return End(TEXT("First court bay has no supported, collision-clear normal F approach"));
   // Disclosed one-time on-foot fixture. The continuous route leaves the car
   // untouched; the isolated repro has only the initial car fixture above.
   // Everything after setup uses normal interaction and native movement.
   Player->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   InitialHealth=Car->GetHealthComponent()->GetCurrentHealth();Last=Car->GetActorLocation();
   if(ExitRepro)ManualGoals={FVector(3700,2440,0),FVector(3910,2250,0),FVector(4056,1920,0),FVector(4056,1300,0),FVector(4056,700,0)};
   else
   {
    ManualGoals={FVector(12440,4400,0),FVector(12380,4610,0),FVector(12360,4740,0),
     FVector(12000,4850,0),FVector(11100,4850,0),FVector(10550,4850,0),
     FVector(10230,4950,0),FVector(9940,5220,0),FVector(9830,5570,0),FVector(9830,6200,0)};
    Test->AddInfo(TEXT("One supported initial player approach; all subsequent travel uses the untouched authored hero car and actual player movement. Readback-free route frame telemetry is retained; this gameplay run is separate from clean camera performance sampling."));
   }
   return Next(1);
  }
  if(Stage==1 && Age>1)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Initial player approach did not settle on the real court surface"));
   if(!FocusCar())return End(TEXT("Actual first-bay F interaction prompt is occluded"));
   Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   Key(EKeys::F,false);
   if(PC->GetPawn()!=Car || Car->GetVehicleInteraction()->GetDriver()!=Player)return End(TEXT("Normal F did not possess the authored court car"));
   Last=Car->GetActorLocation();return Next(ExitRepro?6:3);
  }
  if(Stage>=3 && Stage<=7)
  {
   const FVector Here=Car->GetActorLocation();const float Distance=FVector::Dist2D(Here,Last);Last=Here;Travel+=Distance;
   if(PC->GetPawn()!=Car || Here.ContainsNaN() || !Car->GetMesh()->IsSimulatingPhysics()
    || Here.Z<25 || Here.Z>220 || FMath::Abs(Car->GetActorRotation().Roll)>20
    || Car->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.5f)
    return End(FString::Printf(TEXT("Native car state failed: stage%d leg%d point%d pose%s health%.2f/%.2f"),
     Stage,CruiseLeg,Point,*Car->GetActorTransform().ToString(),Car->GetHealthComponent()->GetCurrentHealth(),InitialHealth));
   PeakSpeed=FMath::Max(PeakSpeed,Car->GetDrivingComponent()->GetSpeedKmh());
   for(const auto& Road:Roads)if(Road.IsValid())
   {
    const FVector Projected=Road->GetRoadSpline()->FindLocationClosestToWorldLocation(Here,ESplineCoordinateSpace::World);
    if(FVector::Dist2D(Here,Projected)<380)RoadTravel.FindOrAdd(Road->RoadID)+=Distance;
   }
   if(Now-LastSupportAt>.25)
   {
    FCollisionQueryParams Q(SCENE_QUERY_STAT(Mainland920ActualDriveFloor),false,Car);Q.AddIgnoredActor(Player);FHitResult H;
    if(!W->LineTraceSingleByObjectType(H,Here+FVector(0,0,80),Here-FVector(0,0,250),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
     || H.ImpactNormal.Z<.94 || H.ImpactPoint.Z<38 || H.ImpactPoint.Z>65)
     return End(FString::Printf(TEXT("Actual moving car lost finished static support at%s hit%s height%.2f"),*Here.ToString(),*GetNameSafe(H.GetActor()),H.ImpactPoint.Z));
    LastSupportAt=Now;
   }
  }
  if(Stage==3 || Stage==6)
  {
   const int32 ManualResult=DriveManual(Age);
   if(ManualResult<0)return End(TEXT("Manual parking-court/garage approach failed; see bounded leg diagnostics"));
   if(ManualResult==0)return false;
   if(Stage==3)return BeginCruiseLeg();
   Test->AddInfo(FString::Printf(TEXT("Native final driveway stop: car_actor=%s mesh=%s player_attached=%s speed%.3f health%.3f travel%.1fcm"),
    *Car->GetActorTransform().ToString(),*Car->GetMesh()->GetComponentTransform().ToString(),*Player->GetActorTransform().ToString(),
    Car->GetDrivingComponent()->GetSpeedKmh(),Car->GetHealthComponent()->GetCurrentHealth(),Travel));
   return Next(7);
  }
  if(Stage==4 && Age>.3)
  {
   Key(EKeys::C,false);
   if(!Car->GetRoadCruise()->IsActive())return End(TEXT("Normal C rejected the authored graph leg: ")+Car->GetRoadCruise()->GetReason());
   const auto& Route=Car->GetRoadCruise()->GetRoute();
   if(Route.Num()<2)return End(TEXT("Production cruise produced no ordered lane route"));
   float AuthoredOffset=-1;
   if(!LaneEndpointMatchesDestination(Nav->GetRoadGuide(),Destination,Route.Last(),AuthoredOffset))
    return End(FString::Printf(TEXT("Actual cruise lane endpoint%s does not project to authored destination%s (lane offset%.1fcm)"),
     *Route.Last().ToString(),*Destination.ToString(),AuthoredOffset));
   Test->AddInfo(FString::Printf(TEXT("Leg%d native lane endpoint%s; destination%s; authored lane offset%.1fcm"),
    CruiseLeg,*Route.Last().ToString(),*Destination.ToString(),AuthoredOffset));
   return Next(5);
  }
  if(Stage==5)
  {
   auto* Cruise=Car->GetRoadCruise();const auto& Route=Cruise->GetRoute();
   float Remaining=0,EndpointLaneError=0;
   if(!ObserveLaneEnd(Route,Car->GetActorLocation(),Remaining,EndpointLaneError))return End(TEXT("Actual cruise lane projection is invalid"));
   MaxLaneError=FMath::Max(MaxLaneError,Cruise->GetLaneErrorCm());
   if(Cruise->GetReason().Contains(TEXT("Lane lost")) || Cruise->GetLaneErrorCm()>300)return End(TEXT("Production cruise left the authored supported lane: ")+Cruise->GetReason());
   if(!Cruise->IsActive())return End(TEXT("Cruise cancelled before its real destination: ")+Cruise->GetReason());
   // PreviewSafeSpeed intentionally stops open routes 280cm before their lane
   // endpoint. Validate that longitudinal reserve separately from the native
   // lane offset; distance to the phone's centreline goal combines both axes.
   // One centimetre is numerical projection tolerance, not extra travel slack.
   const FVector EndDirection=(Route.Last()-Route[Route.Num()-2]).GetSafeNormal2D();
   if(Remaining<=281 && EndpointLaneError<=45 && Cruise->GetSafeSpeedKmh()<=.2f
    && FVector::DotProduct(Route.Last()-Car->GetActorLocation(),EndDirection)>=-1
    && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.5f
    && Cruise->GetReason()==TEXT("Bend / approach speed limit"))
   {
    Cruise->Cancel(TEXT("C920 actual destination reached"));Car->GetDrivingComponent()->SetBrakeInput(1);
    Test->AddInfo(FString::Printf(TEXT("Production navigation/C leg%d arrived at%s: native lane remaining%.1fcm, lateral%.1fcm, centreline distance%.1fcm"),
     CruiseLeg,*Car->GetActorLocation().ToString(),Remaining,EndpointLaneError,FVector::Dist2D(Car->GetActorLocation(),Destination)));
    if(++CruiseLeg<3)return BeginCruiseLeg();
    ManualGoals={FVector(3700,2440,0),FVector(3910,2250,0),FVector(4056,1920,0),FVector(4056,1300,0),FVector(4056,700,0)};
    Point=0;ManualStopping=false;return Next(6);
   }
   if(Age>120)return End(FString::Printf(TEXT("Cruise leg%d stalled at%s lane remaining%.1fcm projected lateral%.1fcm centreline distance%.1fcm control lane%.1f reason%s"),
    CruiseLeg,*Car->GetActorLocation().ToString(),Remaining,EndpointLaneError,FVector::Dist2D(Car->GetActorLocation(),Destination),Cruise->GetLaneErrorCm(),*Cruise->GetReason()));
   return false;
  }
  if(Stage==7 && Age>.5){Key(EKeys::F,true);return Next(8);}
  if(Stage==8 && Age>.3)
  {
   Key(EKeys::F,false);
   if(PC->GetPawn()!=Player){if(Age<3)return false;return End(TEXT("Actual parked car F exit failed"));}
   if(!Player->GetCharacterMovement()->IsMovingOnGround()){if(Age<4)return false;return End(TEXT("Normal F exit did not return to supported walking"));}
   Test->AddInfo(FString::Printf(TEXT("Actual normal F exit: player_actor=%s car_actor=%s capsule_radius%.2f half_height%.2f grounded=%d"),
    *Player->GetActorTransform().ToString(),*Car->GetActorTransform().ToString(),Player->GetSimpleCollisionRadius(),
    Player->GetSimpleCollisionHalfHeight(),Player->GetCharacterMovement()->IsMovingOnGround()?1:0));
   return Next(9);
  }
  if(Stage==9)
  {
   const FVector Goal(4056,-200,55+Player->GetSimpleCollisionHalfHeight()+3);
   if(Walk.IsEmpty())
   {
    float FloorProbeAboveCenter=0;
    if(!GarageFloorProbeAboveCenter(FloorProbeAboveCenter))return End(TEXT("Gallery garage source headroom cannot contain the player's native capsule floor probe"));
    FPhase205cWalkSurvey Survey;
    if(!Phase205cSupportedWalkPath(W,Player,Goal,Walk,FloorProbeAboveCenter,50,&Survey,5000))
    {
     DiagnoseGarageSurvey(Goal,Survey);
     return End(TEXT("Real F exit to Gallery garage portal lacks a supported capsule route: ")+Survey.Failure);
    }
    Test->AddInfo(FString::Printf(TEXT("Read-only capsule route to garage: %d physical probes, %d walking points"),Survey.Probed,Walk.Num()));
   }
   const FVector Delta=Walk[WalkPoint]-Player->GetActorLocation();
   if(Delta.Size2D()<25 && FMath::Abs(Delta.Z)<45)
   {
    Player->GetCharacterMovement()->StopMovementImmediately();
    if(++WalkPoint==Walk.Num())
    {
     if(!Player->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Gallery garage arrival is not supported walking"));
     if(ExitRepro)
     {
      Test->TestTrue(TEXT("Isolated native driveway tail physically exceeds15m before normal F exit"),Travel>1500);
      Test->AddInfo(FString::Printf(TEXT("Isolated driveway/F exit/normal supported garage walk PASS: actual car travel%.1fm; player_actor=%s. Passed Drive3 cruise legs were not rerun; this is focused tail evidence, not a full continuous-route result."),Travel/100,*Player->GetActorTransform().ToString()));
      return End(TEXT(""));
     }
     for(const TCHAR* ID:{TEXT("C920.EastLane"),TEXT("C920.GardenStreet"),TEXT("C920.WestLane"),TEXT("C920.QuayRear")})
      if(RoadTravel.FindRef(FName(ID))<1500)return End(FString(TEXT("Actual car did not traverse at least15m of "))+ID);
     Test->TestTrue(TEXT("Actual unbroken driving exceeds350m"),Travel>35000);
     Test->TestTrue(TEXT("Production cruise reaches ordinary urban road speed"),PeakSpeed>20);
     Test->AddInfo(FString::Printf(TEXT("Native court/F/manual exit/4-road navigation+C/Gallery approach/F exit/garage walk: %.1fm, peak%.1fkm/h, max lane%.1fcm"),Travel/100,PeakSpeed,MaxLaneError));
     return End(TEXT(""));
    }
   }
   if(Age>35)return End(FString::Printf(TEXT("Actual garage walking stalled at%s point%d/%d"),*Player->GetActorLocation().ToString(),WalkPoint,Walk.Num()));
   Player->AddMovementInput((Walk[WalkPoint]-Player->GetActorLocation()).GetSafeNormal2D(),FMath::Clamp(float(Delta.Size2D()/100),.1f,1.f));
   PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));return false;
  }
  return false;
 }
private:
 bool GarageFloorProbeAboveCenter(float& OutProbe)
 {
  // The real rear portal has335cm headroom. A250cm-above-centre ray starts
  // inside its lintel at the50cm grid's threshold cell and falsely calls the
  // downward-facing lintel a floor. Query from the actual capsule top instead;
  // all original full-capsule clearance/sweep/step gates still own admission.
  FString Text;TSharedPtr<FJsonObject> Manifest;
  const TSharedPtr<FJsonObject>* Addresses=nullptr;const TSharedPtr<FJsonObject>* Gallery=nullptr;
  const TSharedPtr<FJsonObject>* Portal=nullptr;double HeightMetres=0;
  if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/MainlandC920/Architecture/Revision1/architecture.json")))
   || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest) || !Manifest
   || !Manifest->TryGetObjectField(TEXT("addresses"),Addresses) || !Addresses || !Addresses->IsValid()
   || !(*Addresses)->TryGetObjectField(TEXT("SM_C920_Gallery27"),Gallery) || !Gallery || !Gallery->IsValid()
   || !(*Gallery)->TryGetObjectField(TEXT("garage_rear"),Portal) || !Portal || !Portal->IsValid()
   || !(*Portal)->TryGetNumberField(TEXT("height_m"),HeightMetres) || !FMath::IsFinite(HeightMetres))return false;
  const float Half=Player->GetSimpleCollisionHalfHeight();const float HeightCm=float(HeightMetres*100.);
  if(!FMath::IsFinite(Half) || Half<=0 || HeightCm<Half*2+3)return false;
  OutProbe=FMath::Min(Half,HeightCm-Half-3);
  Test->AddInfo(FString::Printf(TEXT("Gallery floor probe derived from actual native capsule top: above-centre%.2fcm, capsule full height%.2fcm, immutable portal headroom%.2fcm; complete capsule collision gates unchanged"),OutProbe,Half*2,HeightCm));
  return OutProbe>0;
 }
 void DiagnoseGarageSurvey(const FVector& Goal,const FPhase205cWalkSurvey& Survey)
 {
  Test->AddInfo(FString::Printf(TEXT("Garage walk diagnosis actual player=%s car=%s goal=%s expanded%d probed%d open%d failure%s"),
   *Player->GetActorTransform().ToString(),*Car->GetActorTransform().ToString(),*Goal.ToString(),Survey.Expanded,Survey.Probed,Survey.OpenAtExit,*Survey.Failure));
  for(const auto& Rejected:Survey.Rejections)Test->AddInfo(FString::Printf(TEXT("Garage walk rejection %s count%d"),*Rejected.Key,Rejected.Value));
  FCollisionQueryParams Q(SCENE_QUERY_STAT(Mainland920GarageTailDiagnosis),false,Player);
  const auto Shape=FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight());
  TArray<FOverlapResult> Overlaps;W->OverlapMultiByChannel(Overlaps,Player->GetActorLocation(),FQuat::Identity,ECC_Pawn,Shape,Q);
  for(const auto& Hit:Overlaps)if(Hit.bBlockingHit)Test->AddInfo(FString::Printf(TEXT("Actual F-exit blocking overlap actor%s component%s"),*GetPathNameSafe(Hit.GetActor()),*GetPathNameSafe(Hit.GetComponent())));
  const FVector Probes[]={Player->GetActorLocation(),FVector(4056,1100,145),FVector(4056,800,145),FVector(4056,500,145),FVector(4056,200,145),Goal};
  for(const auto& ProbePosition:Probes)
  {
   FHitResult Floor;W->LineTraceSingleByChannel(Floor,ProbePosition+FVector(0,0,250),ProbePosition-FVector(0,0,450),ECC_Pawn,Q);
   Test->AddInfo(FString::Printf(TEXT("Garage tail floor probe at%s blocking%d actor%s component%s point%s normal%s"),*ProbePosition.ToString(),Floor.bBlockingHit?1:0,
    *GetPathNameSafe(Floor.GetActor()),*GetPathNameSafe(Floor.GetComponent()),*Floor.ImpactPoint.ToString(),*Floor.ImpactNormal.ToString()));
  }
 }
 static bool ObserveLaneEnd(const TArray<FVector>& Route,const FVector& Position,float& Remaining,float& LaneError)
 {
  if(Route.Num()<2 || Position.ContainsNaN())return false;
  double Total=0,Progress=0,Best=TNumericLimits<double>::Max();FVector Projection=FVector::ZeroVector;
  for(int32 I=1;I<Route.Num();++I)
  {
   if(Route[I-1].ContainsNaN() || Route[I].ContainsNaN())return false;
   const FVector P=FMath::ClosestPointOnSegment(Position,Route[I-1],Route[I]);
   const double Distance=FVector::DistSquared(Position,P),Length=FVector::Distance(Route[I-1],Route[I]);
   if(Distance<Best){Best=Distance;Progress=Total+FVector::Distance(Route[I-1],P);Projection=P;}
   Total+=Length;
  }
  Remaining=float(Total-Progress);LaneError=FVector::Dist2D(Position,Projection);
  return Total>350 && FMath::IsFinite(Remaining) && FMath::IsFinite(LaneError);
 }
 void Key(FKey K,bool Down)
 {if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool FocusCar()
 {
  FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);
  for(float Z:{0.f,35.f,65.f})
  {
   PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,Z)-Eye).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()==Car)return true;
  }
  return false;
 }
 bool BeginCruiseLeg()
 {
  const FVector Goals[]={FVector(8000,8000,40),FVector(-10000,4000,40),FVector(3500,2500,40)};
  Destination=Goals[CruiseLeg];FNeonCityNavigationDestination D;
  D.OwnerID=TEXT("Validation.MainlandC920");D.DestinationID=FName(*FString::Printf(TEXT("GraphLeg%d"),CruiseLeg));
  D.Label=FText::FromString(TEXT("C marina road validation"));D.Location=Destination;D.UseRoadRoute=true;
  if(!Nav->SetDestination(D))return End(TEXT("Production navigation rejected an authored road destination"));
  Key(EKeys::C,true);return Next(4);
 }
 int32 DriveManual(float Age)
 {
  auto* Drive=Car->GetDrivingComponent();const FVector Delta=ManualGoals[Point]-Car->GetActorLocation();
  const bool Final=Point==ManualGoals.Num()-1;const float Speed=Drive->GetSpeedKmh();
  if((Final && ManualStopping) || Delta.Size2D()<(Final?95:140))
  {
   if(!Final){++Point;At=W->GetTimeSeconds();return 0;}
   ManualStopping=true;
   Drive->SetDriveInput(0);Drive->SetSteerInput(0);Drive->SetBrakeInput(1);return FMath::Abs(Speed)<.7f?1:0;
  }
  if(Age>30)
  {
   Test->AddError(FString::Printf(TEXT("Native manual stage%d point%d at%s toward%s yaw%.1f speed%.1f"),
    Stage,Point,*Car->GetActorLocation().ToString(),*ManualGoals[Point].ToString(),Car->GetActorRotation().Yaw,Speed));return -1;
  }
  const float Error=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Delta.Rotation().Yaw);
  const float Target=Final?FMath::Clamp(float(Delta.Size2D()*.012),2.f,7.f):8.f;
  Drive->SetSteerInput(FMath::Clamp(Error/35.f,-1.f,1.f));Drive->SetDriveInput(FMath::Clamp((Target-Speed)*.08f,0.f,.4f));
  Drive->SetBrakeInput(Speed>Target+1?.3f:0);return 0;
 }
 bool Next(int32 NewStage){Stage=NewStage;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error)
 {
  Key(EKeys::F,false);Key(EKeys::C,false);if(!Error.IsEmpty())Test->AddError(Error);
  if(Car){Car->GetRoadCruise()->Cancel(TEXT("Isolated C920 gameplay test ended"));Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);}
  if(Nav){Nav->ClearDestination(TEXT("Validation.MainlandC920"));if(!OriginalDestination.DestinationID.IsNone())Nav->SetDestination(OriginalDestination);}
  if(Player && PC && PC->GetPawn()==Player)Player->GetCharacterMovement()->StopMovementImmediately();
  Test->TestTrue(TEXT("Continuous drive frame telemetry retained"),Metrics.Flush());
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;
 AIslandSupercar* Car=nullptr;UNeonCityNavigationComponent* Nav=nullptr;FNeonCityNavigationDestination OriginalDestination;
 TArray<TWeakObjectPtr<ANeonCityTrafficRoute>> Roads;TMap<FName,float> RoadTravel;TArray<FVector> ManualGoals,Walk;
 FNeonCityLandFrameMetrics Metrics;
 FVector Destination,Last;int32 Stage=0,Point=0,CruiseLeg=0,WalkPoint=0;
  float At=0,InitialHealth=0,Travel=0,PeakSpeed=0,MaxLaneError=0;double Started=0,LastSupportAt=0;bool ManualStopping=false,ExitRepro=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandContinuousDrive920,
 "NeonCity.Phase205c.MainlandC920.Standalone.ContinuousDrive",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandContinuousDrive920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("ContinuousDrive requires an isolated process with automatic player persistence disabled"));return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
 {AddError(TEXT("This objective gameplay route runs separately from image readback and clean performance sampling"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandDrive920::FContinuousDrive(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandNativeRoadConnectivity920,
 "NeonCity.Phase205c.MainlandC920.Standalone.NativeRoadConnectivity",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandNativeRoadConnectivity920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("NativeRoadConnectivity requires an isolated process with automatic player persistence disabled"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandDrive920::FNativeRoadConnectivity(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandDriveGarageExitRepro920,
 "NeonCity.Phase205c.MainlandC920.Standalone.DriveGarageExitRepro",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandDriveGarageExitRepro920::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))
 {AddError(TEXT("DriveGarageExitRepro requires an isolated process with automatic player persistence disabled"));return false;}
 if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics")) || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
 {AddError(TEXT("The focused native driveway tail runs without image readback or clean-performance claims"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(MainlandDrive920::FContinuousDrive(this,true));return true;
}
#endif
