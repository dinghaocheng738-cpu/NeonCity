#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandLandDoor.h"
#include "Adventure/IslandGarageDoor.h"
#include "Adventure/IslandResidenceLift.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Vehicles/RoadCruiseComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleData.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "ChaosVehicleManagerAsyncCallback.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "PBDRigidsSolver.h"
#include "EventManager.h"
#include "EventsData.h"
#include "Chaos/ExternalCollisionData.h"

namespace NeonCityLandFinalDrive
{
const FString LegacySlot=TEXT("NeonCity_Test_LandFinal_Legacy15");
const FString BoatCatchSlot=TEXT("NeonCity_Test_LandFinal_BoatCatch");
const FString ArrivalSlot=TEXT("NeonCity_Test_LandFinal_BBQArrival");

bool ReadRoad(TArray<FVector>& Out)
{
 FString Text;TSharedPtr<FJsonObject> Source;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/road-alignment.json")))
  || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Source))return false;
 for(const auto& Row:Source->GetArrayField(TEXT("points")))
 {
  const auto Values=Row->AsArray();if(Values.Num()!=3)return false;
  Out.Add(FVector(NeonCityCoastalLayout::IslandOrigin.X+100*Values[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*Values[1]->AsNumber(),-130+100*Values[2]->AsNumber()));
 }
 return Out.Num()==842;
}

bool ReadVillageWalk(const FTransform& VillageTransform,TArray<FVector>& Out)
{
 Out={VillageTransform.TransformPosition(FVector(5500,-2900,230)),VillageTransform.TransformPosition(FVector(5500,-1500,230)),
  VillageTransform.TransformPosition(FVector(5500,0,230)),VillageTransform.TransformPosition(FVector(5500,2000,230)),
  VillageTransform.TransformPosition(FVector(5500,4200,230)),VillageTransform.TransformPosition(FVector(5500,6250,230))};
 FString Text;TSharedPtr<FJsonObject> Source;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/Village/Source.json")))
  || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Source))return false;
 bool Main=false;
 for(const auto& Value:Source->GetObjectField(TEXT("colliders"))->GetArrayField(TEXT("boxes")))
 {
  const auto Box=Value->AsObject();const bool Boardwalk=Box->GetStringField(TEXT("tag"))==TEXT("boardwalk");
  if(Main && !Boardwalk)break;if(!Boardwalk)continue;Main=true;
  const auto C=Box->GetObjectField(TEXT("center")),H=Box->GetObjectField(TEXT("half"));
  Out.Add(VillageTransform.TransformPosition(FVector(C->GetNumberField(TEXT("x"))*100,-C->GetNumberField(TEXT("z"))*100,
   (C->GetNumberField(TEXT("y"))+H->GetNumberField(TEXT("y")))*100)));
 }
 return Out.Num()==50;
}

/** Test-only route planning queries the same floor and capsule collision used by
 * CharacterMovement. It supplies movement direction, never an actor pose. */
bool PlanVillageGroundWalk(UWorld* W,ANeonCityPlayerCharacter* P,AIslandSupercar* Car,const FVector& Goal,TArray<FVector>& Out)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(LandValidationVillagePathPlanning);
 constexpr float Grid=50.f;const FVector Start=P->GetActorLocation();
 // A world-aligned grid keeps a 2cm arrival variation from shifting every
 // collision sample. Its 50cm pitch also resolves legal steps at road edges.
 const FVector Minimum(FMath::FloorToDouble((FMath::Min(Start.X,Goal.X)-1800)/Grid)*Grid,
  FMath::FloorToDouble((FMath::Min(Start.Y,Goal.Y)-1800)/Grid)*Grid,0);
 const int32 NX=FMath::CeilToInt((FMath::Max(Start.X,Goal.X)+1800-Minimum.X)/Grid)+1;
 const int32 NY=FMath::CeilToInt((FMath::Max(Start.Y,Goal.Y)+1800-Minimum.Y)/Grid)+1;
 if(NX*NY>35000)return false;
 struct FNode {FVector Position=FVector::ZeroVector;float G=BIG_NUMBER;int32 Parent=INDEX_NONE;uint8 State=0;bool Checked=false,Clear=false;};
 TArray<FNode> Nodes;Nodes.SetNum(NX*NY);
 const auto Index=[&](const FVector& V){return FMath::Clamp(FMath::RoundToInt((V.Y-Minimum.Y)/Grid),0,NY-1)*NX+FMath::Clamp(FMath::RoundToInt((V.X-Minimum.X)/Grid),0,NX-1);};
 const int32 First=Index(Start);int32 Last=Index(Goal);
 const float Radius=P->GetCapsuleComponent()->GetScaledCapsuleRadius()+2;
 const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 const auto Shape=FCollisionShape::MakeCapsule(Radius,Half);
 FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(LandFinalWalkFloor),false,P);FloorQuery.AddIgnoredActor(Car);
 FCollisionQueryParams Clearance(SCENE_QUERY_STAT(LandFinalWalkClearance),false,P);
 auto Check=[&](int32 I)
 {
  auto& N=Nodes[I];if(N.Checked)return N.Clear;N.Checked=true;
  FVector At(Minimum.X+(I%NX)*Grid,Minimum.Y+(I/NX)*Grid,Start.Z);
  if(I==First){At.X=Start.X;At.Y=Start.Y;}
  TArray<FHitResult> Floors;
  const FCollisionObjectQueryParams FloorObjects(FCollisionObjectQueryParams::AllObjects);
  W->LineTraceMultiByObjectType(Floors,At+FVector(0,0,1800),At-FVector(0,0,1600),FloorObjects,FloorQuery);
  // A visibility ray from 18m above the player selects a canopy/roof before
  // the physical floor underneath it. Choose an actual Pawn-blocking,
  // capsule-clear walkable surface near the expected route elevation instead.
  const float Progress=FMath::Clamp(FVector::DotProduct(At-Start,(Goal-Start).GetSafeNormal2D())/FMath::Max(1.,FVector::Dist2D(Start,Goal)),0.,1.);
  const double ExpectedFeet=FMath::Lerp(Start.Z,Goal.Z,double(Progress))-Half;
  double BestFloor=DBL_MAX;FString Rejected;
  for(const FHitResult& Hit:Floors)
  {
   const auto* Surface=Hit.GetComponent();
   if(!Surface || Surface->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block
    || Hit.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ())continue;
  // A capsule's spherical bottom rests above the vertical centre ray on an
  // inclined plane. Keep the full capsule and walkable-normal tests; account
  // for that exact geometry instead of misclassifying a legal grade overlap.
  const float SlopeLift=Radius*(1.f/static_cast<float>(Hit.ImpactNormal.Z)-1.f);
   const FVector Position=Hit.ImpactPoint+FVector(0,0,Half+3+SlopeLift);
   if(W->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,Shape,Clearance)){Rejected+=GetNameSafe(Hit.GetActor())+TEXT(";");continue;}
   const double Difference=FMath::Abs(Hit.ImpactPoint.Z-ExpectedFeet);
   if(Difference<BestFloor){BestFloor=Difference;N.Position=Position;N.Clear=true;}
  }
  if(!N.Clear && (I==First || I==Last))UE_LOG(LogTemp,Warning,TEXT("VILLAGE_WALK_ENDPOINT index=%d first=%d last=%d at=%s floor_hits=%d rejected=%s"),I,First,Last,*At.ToString(),Floors.Num(),*Rejected);
  return N.Clear;
 };
 if(!Check(First))return false;
 // A clear point may be isolated by a curb or a post. Search all physically
 // clear, visible approaches within native entry range, not only the nearest
 // clear point. The eventual native eye trace and interaction remain required.
 TSet<int32> Goals;const int32 Requested=Last;
  for(int32 DY=-9;DY<=9;++DY)for(int32 DX=-9;DX<=9;++DX)
  {
   const int32 X=Requested%NX+DX,Y=Requested/NX+DY;if(X<0 || Y<0 || X>=NX || Y>=NY)continue;
   const int32 Candidate=Y*NX+X;if(!Check(Candidate))continue;
   const auto& Position=Nodes[Candidate].Position;
   if(FVector::Dist(Position,Car->GetActorLocation())>330)continue;
   FHitResult Sight;
   if(W->LineTraceSingleByChannel(Sight,Position+FVector(0,0,P->BaseEyeHeight),Car->GetActorLocation()+FVector(0,0,55),ECC_Visibility,Clearance)
    && Sight.GetActor()==Car)Goals.Add(Candidate);
  }
 if(Goals.IsEmpty()){UE_LOG(LogTemp,Warning,TEXT("VILLAGE_WALK_NO_NATIVE_APPROACH candidates=0"));return false;}
 Nodes[First].G=0;Nodes[First].State=1;TArray<int32> Open{First};int32 Expanded=0;
 while(!Open.IsEmpty() && ++Expanded<30000)
 {
  int32 Best=0;float Cost=BIG_NUMBER;
  for(int32 I=0;I<Open.Num();++I)
  {
   const auto& N=Nodes[Open[I]];const float F=N.G+FMath::Max(0.,FVector::Dist2D(N.Position,Car->GetActorLocation())-330);
   if(F<Cost){Cost=F;Best=I;}
  }
  const int32 Current=Open[Best];Open.RemoveAtSwap(Best,EAllowShrinking::No);Nodes[Current].State=2;
  if(Goals.Contains(Current))
  {
   Last=Current;
   UE_LOG(LogTemp,Display,TEXT("VILLAGE_WALK_NATIVE_APPROACH requested=%s selected=%s car_distance_cm=%.1f expanded=%d native_candidates=%d"),*Goal.ToString(),*Nodes[Last].Position.ToString(),FVector::Dist(Nodes[Last].Position,Car->GetActorLocation()),Expanded,Goals.Num());
   TArray<FVector> Reverse;for(int32 I=Last;I!=INDEX_NONE;I=Nodes[I].Parent)Reverse.Add(Nodes[I].Position);
   for(int32 I=Reverse.Num()-1;I>=0;--I)Out.Add(Reverse[I]);return true;
  }
  for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
  {
   if(!DX && !DY)continue;const int32 X=Current%NX+DX,Y=Current/NX+DY;if(X<0 || Y<0 || X>=NX || Y>=NY)continue;
   const int32 Next=Y*NX+X;if(Nodes[Next].State==2 || !Check(Next))continue;
   const FVector From=Nodes[Current].Position,To=Nodes[Next].Position;
   if(FMath::Abs(To.Z-From.Z)>P->GetCharacterMovement()->MaxStepHeight)continue;
   // At an ordinary step the movement component lifts, sweeps and settles.
   // Test that raised capsule path too instead of bypassing a blocking edge.
   const float Raise=FMath::Max(0.f,static_cast<float>(To.Z-From.Z));FHitResult Hit;
   if(W->SweepSingleByChannel(Hit,From+FVector(0,0,Raise+2),To+FVector(0,0,2),FQuat::Identity,ECC_Pawn,Shape,Clearance))continue;
   const float G=Nodes[Current].G+FVector::Dist2D(From,To);
   if(G>=Nodes[Next].G)continue;Nodes[Next].G=G;Nodes[Next].Parent=Current;
   if(Nodes[Next].State==0){Open.Add(Next);Nodes[Next].State=1;}
  }
 }
 return false;
}

class FContinuousDrive : public IAutomationLatentCommand
{
public:
 explicit FContinuousDrive(FAutomationTestBase* In,bool AcquireOnly=false,bool TerminalOnly=false,bool CurrentEstate=false):Test(In),bAcquireOnly(AcquireOnly),bTerminalOnly(TerminalOnly),bCurrentEstate(CurrentEstate){}
 ~FContinuousDrive(){UnregisterTerminalContacts();}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  UWorld* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(!W || !PC || !W->HasBegunPlay() || W->GetTimeSeconds()<3)return false;
  if(FPlatformTime::Seconds()-Started>(bAcquireOnly?120:1300))return Fail(W,FString::Printf(TEXT("Native route timeout at stage %s road station %d"),StageName(),Station));
  Metrics.Sample(W,MetricStage());
  const float Now=W->GetTimeSeconds();const float DT=FMath::Clamp(Now-PreviousGame,0.f,.1f);PreviousGame=Now;
  if(Stage==0)return Prepare(W,PC);
  if(Stage==1)
  {
   if(Save->IsOperationInProgress())return false;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return Fail(W,TEXT("Isolated legacy load did not complete: ")+Save->GetLastDetail());
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return Fail(W,TEXT("Legacy load must leave a normal player character"));
   auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
   for(const auto& Fish:Legacy->Fishing.Fish)
   {
    if(!Inventory->FindFish(Fish.FishID))return Fail(W,TEXT("Legacy real catch missing before route"));FishIDs.Add(Fish.FishID);
   }
   if(FishIDs.IsEmpty())return Fail(W,TEXT("Real ocean catch required for continuous BBQ arrival checkpoint"));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   for(TActorIterator<AIslandSupercar> It(W);It;++It)
    if(!It->bRoadster && (!Car || FVector::DistSquared2D(It->GetActorLocation(),Road[0])<FVector::DistSquared2D(Car->GetActorLocation(),Road[0])))Car=*It;
   if(!Car || FVector::Dist2D(Car->GetActorLocation(),Road[0])>4000)return Fail(W,TEXT("No retained Aster reachable in the original lower village lay-by after legacy load"));
   if(!Test->TestNotNull(TEXT("Authored Aster profile available"),Car->GetVehicleProfile()) || !Test->TestEqual(TEXT("Authored hero Aster profile"),Car->GetVehicleProfile()->GetFName(),FName(TEXT("DA_HeroAster"))))return Finish(W,false);
   // The only explicit actor relocation in the entire continuous run. Native
   // save load above is preparation and is separately recorded, before T0.
   if(bTerminalOnly)
   {
    Car->SetActorTransform(FTransform((Road[833]-Road[831]).Rotation(),Road[832]+FVector(0,0,40)),false,nullptr,ETeleportType::TeleportPhysics);
    Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);++SetupCarRelocations;
    Player->SetActorLocation(Road[832]-Car->GetActorRightVector()*245+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
   }
   else Player->SetActorLocation(bAcquireOnly?NeonCityCoastalLayout::FromLegacyIsland(FVector(54284.1933,-159355.4774,358.3233)):NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-173600,330)),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();++SetupPlayerRelocations;
   PC->SetControlRotation(FRotator(-5,90,0));RouteT0=Now;At=LastProgress=Now;LastPawnPosition=Player->GetActorLocation();
   Stage=2;Event(TEXT("ROUTE_BEGIN_DOCK: one documented setup; no later explicit pose writes"));return false;
  }
  Sample(W,PC);
  if(Stage==2)
  {
   if(Now-At<1.2f)return false;
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return Fail(W,TEXT("Initial dock setup did not settle onto physical walking support"));
   if(bTerminalOnly)
   {
    PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,55)-Player->GetPawnViewLocation()).Rotation());
    if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Car || !Player->GetInteractionComponent()->TryInteract() || PC->GetPawn()!=Car)return Fail(W,TEXT("Terminal diagnostic fixture cannot enter original Aster normally"));
    InitialCondition=Car->GetDamageState()->GetConditionFraction();LastCarPosition=Car->GetActorLocation();MergeStation=Station=832;At=LastProgress=Now;Stage=5;
    Event(TEXT("FIX_SPECIFIC_TERMINAL_832_ONLY: one car/player setup; no Dock continuity or BBQ arrival checkpoint claim"));EnableTerminalDiagnostics(W);return false;
   }
   if(bAcquireOnly)
   {
    const FVector Side=Car->GetActorLocation()-Car->GetActorRightVector()*245+FVector(0,0,90);
    if(!PlanVillageGroundWalk(W,Player,Car,Side,GroundWalk))return Fail(W,TEXT("Fix-specific village-to-Aster floor/capsule planning failed"));
    Stage=4;Waypoint=0;LastProgress=Now;Event(TEXT("FIX_SPECIFIC_VILLAGE_ACQUISITION_ONLY: one initial player fixture; no boat/road/mansion continuity claim"));return false;
   }
   Stage=3;LastProgress=Now;return false;
  }
  if(Stage==3 || Stage==4 || Stage==10 || Stage==12)
  {
   const TArray<FVector>& Path=Stage==3?Boardwalk:Stage==4?GroundWalk:Stage==10?EntryWalk:InteriorWalk;
   if(!Walk(W,PC,Path))return Done;
   if(Waypoint<Path.Num())return false;
   if(Stage==3)
   {
    Event(TEXT("VILLAGE_BOARDWALK_COMPLETE: physical pier / shore steps / 44 canonical supports"));
    const FVector Side=Car->GetActorLocation()-Car->GetActorRightVector()*245+FVector(0,0,90);
    if(!PlanVillageGroundWalk(W,Player,Car,Side,GroundWalk))return Fail(W,TEXT("No physical walkable path from canonical village boardwalk to the actual Aster door"));
    Stage=4;Waypoint=0;LastProgress=Now;return false;
   }
   if(Stage==4)
   {
    PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,55)-Player->GetPawnViewLocation()).Rotation());
    auto* Interaction=Player->GetInteractionComponent();
    if(Interaction->RefreshFocusedInteractable()!=Car)return Fail(W,TEXT("Normal player eye trace cannot focus the actual village Aster"));
    if(!Interaction->TryInteract() || PC->GetPawn()!=Car || Car->GetVehicleInteraction()->GetDriver()!=Player)
     return Fail(W,TEXT("Native interactable vehicle entry failed"));
    Event(TEXT("ASTER_ENTERED_NATIVE_INTERACTION"));CarDistance=0;LastCarPosition=Car->GetActorLocation();InitialCondition=Car->GetDamageState()->GetConditionFraction();
    if(bAcquireOnly && !bCurrentEstate)return Finish(W,!Test->HasAnyErrors());
    MergeStation=ClosestStation(Car->GetActorLocation(),0,30);Station=MergeStation;At=LastProgress=Now;
    if(bCurrentEstate)
    {
     auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();if(!Nav)return Fail(W,TEXT("Normal navigation component missing"));
     FNeonCityNavigationDestination Goal;Goal.OwnerID=TEXT("Test.CurrentContinuous");Goal.DestinationID=TEXT("Island.Mansion.Garage");Goal.Label=FText::FromString(TEXT("The One garage"));Goal.Location=Garage->GetActorTransform().TransformPosition(FVector(0,750,2));Goal.UseRoadRoute=true;
     if(!Nav->SetDestination(Goal))return Fail(W,TEXT("Current actual garage destination refused"));
     PlannedDriveCm=0;for(int32 I=MergeStation+1;I<Road.Num();++I)PlannedDriveCm+=FVector::Dist2D(Road[I-1],Road[I]);
     Stage=25;return false;
    }
    Stage=5;return false;
   }
   if(Stage==10)
   {
    const FVector Target=Door->GetActorTransform().TransformPosition(FVector(65,0,110));
    PC->SetControlRotation((Target-Player->GetPawnViewLocation()).Rotation());
    if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Door)return Fail(W,TEXT("Normal arrival eye trace cannot focus residence door"));
    PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Pressed,FPlatformTime::Cycles64()));
    FPressed=true;At=Now;Stage=11;Event(TEXT("RESIDENCE_DOOR_F_PRESSED"));return false;
   }
   if(Stage==12)
   {
    const FVector Local=Door->GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
    if(Local.Y<80 || !Player->GetCharacterMovement()->IsMovingOnGround())return Fail(W,TEXT("Normal player did not cross physical mansion doorway"));
    Event(TEXT("MANSION_ENTERED_BY_NORMAL_WALK"));StaticScreenshot(W,TEXT("LandFinal-Continuous-Arrival-Interior.png"));
    if(bTerminalOnly)return Finish(W,!Test->HasAnyErrors());
    for(const FGuid& ID:FishIDs)if(!W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>()->FindFish(ID))return Fail(W,TEXT("Real caught fish lost during route"));
    if(!Save->ConfigureSlot(ArrivalSlot,false) || !Save->SaveGame())return Fail(W,TEXT("Dedicated normal-arrival checkpoint could not be saved: ")+Save->GetLastDetail());
    auto* Snapshot=Save->ReadSaveForInspection();if(!Snapshot)return Fail(W,TEXT("Arrival checkpoint failed native integrity readback"));
    for(const FGuid& ID:FishIDs)
     if(!Snapshot->Fishing.Fish.ContainsByPredicate([&](const FNeonCityCaughtFish& F){return F.FishID==ID;}))return Fail(W,TEXT("Arrival save omitted original actual catch GUID"));
    bool Valid=Test->TestTrue(TEXT("Continuous actual Chaos road travel exceeds 2.4 km"),CarDistance>240000);
    Valid=Test->TestEqual(TEXT("Only one explicit player setup relocation"),SetupPlayerRelocations,1)&&Valid;
    Valid=Test->TestEqual(TEXT("No explicit car relocation anywhere in continuous route"),SetupCarRelocations,0)&&Valid;
    Valid=Test->TestEqual(TEXT("Continuous route reaches original station 841"),HighestStation,841)&&Valid;
    Valid=Test->TestTrue(TEXT("All ascent stations after real lay-by merge observed in order"),VisitedStations.Num()>=842-MergeStation-3)&&Valid;
    Test->AddInfo(FString::Printf(TEXT("Actual route: Dock -> canonical village -> Aster interaction -> merge station %d -> 841 -> driveway -> park -> native exit -> F door -> mansion. Stations 0..%d are the short acquisition connection and were not driven. Real catch saved to %s."),MergeStation,FMath::Max(0,MergeStation-1),*ArrivalSlot));
    return Finish(W,Valid);
   }
  }
  if(bCurrentEstate && Stage>=18)return CurrentArrival(W,PC,Now);
  if(Stage==5 && bCurrentEstate)
  {
   if(CurrentSideContacts>0)return Fail(W,TEXT("Current cruise native chassis side contact: ")+FirstCurrentSideContact);
   auto* Cruise=Car->GetRoadCruise();const FVector Here=Car->GetActorLocation();
   const int32 Best=ClosestStation(Here,FMath::Max(0,Station-3),FMath::Min(Road.Num()-1,Station+16));
   if(Best>Station){for(int32 I=Station;I<=Best;++I)VisitedStations.Add(I);HighestStation=Station=Best;LastProgress=Now;}
   LaneError=FVector::Dist2D(Here,Road[Best]);
   if((LaneError>330 && Station>40) || Here.Z<Road[Best].Z-100 || FMath::Abs(Car->GetActorRotation().Roll)>32 || Car->GetDamageState()->GetConditionFraction()<.97f)return Fail(W,FString::Printf(TEXT("Current cruise physical road envelope failed station%d lane%.1f position%s"),Station,LaneError,*Here.ToString()));
   if(!Cruise->IsActive())return Fail(W,TEXT("Normal C cruise cancelled before current garage: ")+Cruise->GetReason());
   if(Now-LastProgress>25)return Fail(W,FString::Printf(TEXT("Current continuous cruise stalled station%d: %s"),Station,*Cruise->GetReason()));
   if(Station>=424 && !SummitStop){SummitStop=true;Event(TEXT("CURRENT_VIEWPOINT_PASSED_CONTINUOUSLY"));StaticScreenshot(W,TEXT("CurrentContinuous-MountainViewpoint.png"));}
   const FVector Local=Garage->GetActorTransform().InverseTransformPosition(Here);
   if(Local.Y>340 && Local.Y<850 && FMath::Abs(Local.X)<120 && FMath::Abs(Car->GetDrivingComponent()->GetSpeedKmh())<1.2f)
   {HighestStation=Station=Road.Num()-1;VisitedStations.Add(Station);Stage=8;At=Now;Event(TEXT("CURRENT_NATIVE_GARAGE_PARKED_BY_NORMAL_C_CRUISE"));}
   return false;
  }
  if(Stage==5)
  {
   if(Station>=828 && TerminalGuidance.IsEmpty() && !BuildTerminalGuidance(W))return Fail(W,TEXT("No collision-clear terminal lane around retained parked vehicles"));
   if(Station>=832)EnableTerminalDiagnostics(W);
   const FVector Here=Car->GetActorLocation();const int32 Best=ClosestStation(Here,FMath::Max(0,Station-3),FMath::Min(Road.Num()-1,Station+16));
   if(Best>Station){for(int32 I=Station;I<=Best;++I)VisitedStations.Add(I);Station=Best;HighestStation=FMath::Max(HighestStation,Station);LastProgress=Now;}
   const float Lane=FVector::Dist2D(Here,Road[Best]);LaneError=Lane;
   const bool Merging=Now-At<15 && Station<40;
   if((Lane>330 && !Merging) || Here.Z<Road[Best].Z-100 || FMath::Abs(Car->GetActorRotation().Roll)>32)
    return Fail(W,FString::Printf(TEXT("Physical route left safe road envelope at station %d; lane %.1f cm height %.1f cm roll %.1f"),Station,Lane,Here.Z-Road[Best].Z,Car->GetActorRotation().Roll));
   if(Now-LastProgress>22)return Fail(W,FString::Printf(TEXT("No physical progress for 22 s at station %d; speed %.2f km/h roll %.2f; possible road / guard contact"),Station,Car->GetDrivingComponent()->GetSpeedKmh(),Car->GetActorRotation().Roll));
   if(Station>=838)
   {
    if(!SummitStop){SummitStop=true;Event(TEXT("SUMMIT_VIEWPOINT_APPROACH: braking on retained road"));}
    const FVector SummitTarget=TerminalGuidance.IsEmpty()?Road.Last():TerminalGuidance.Last();
    const float SummitDistance=FVector::Dist2D(Here,SummitTarget);
    if(SummitDistance<180)Brake();else DriveToward(SummitTarget,DT,FMath::Clamp(SummitDistance*.55f,100.f,280.f));
    if(SummitDistance<180 && FMath::Abs(Car->GetChaosMovement()->GetForwardSpeed())<35)
    {
     Brake();for(int32 I=Station;I<=841;++I)VisitedStations.Add(I);HighestStation=Station=841;At=Now;Stage=6;
     Event(TEXT("SUMMIT_841_STOP: viewpoint pause, continuous route remains active"));
    }
    return false;
   }
   const float Speed=FMath::Abs(Car->GetChaosMovement()->GetForwardSpeed());
   const FVector Rear=Car->GetActorTransform().TransformPosition(FVector(-139.3,0,0));
   const float Lookahead=FMath::Clamp(550.f+Speed*.45f,600.f,1000.f);
   float Remaining=Lookahead;int32 Ahead=ClosestStation(Rear,FMath::Max(0,Station-4),FMath::Min(Station+4,841));
   const auto& Guidance=TerminalGuidance.IsEmpty()?Road:TerminalGuidance;
   while(Ahead<841 && Remaining>FVector::Dist2D(Guidance[Ahead],Guidance[Ahead+1])){Remaining-=FVector::Dist2D(Guidance[Ahead],Guidance[Ahead+1]);++Ahead;}
   FVector Target=Guidance[Ahead];if(Ahead<841)Target=FMath::Lerp(Guidance[Ahead],Guidance[Ahead+1],FMath::Clamp(Remaining/FVector::Dist2D(Guidance[Ahead],Guidance[Ahead+1]),0.,1.));
   float Curvature=0;
   for(int32 I=FMath::Max(1,Station);I<FMath::Min(Station+9,841);++I)
   {
    const FVector A=(Road[I]-Road[I-1]).GetSafeNormal2D(),B=(Road[I+1]-Road[I]).GetSafeNormal2D();
    Curvature=FMath::Max(Curvature,static_cast<float>(FMath::Acos(FMath::Clamp(FVector::DotProduct(A,B),-1.,1.))/FMath::Max(1.,FVector::Dist2D(Road[I],Road[I+1]))));
   }
   const float Desired=FMath::Clamp(FMath::Sqrt(140.f/FMath::Max(Curvature,.00001f)),300.f,780.f);
   DriveToward(Target,DT,Merging?FMath::Min(Desired,380.f):Desired);
   if(Station/50!=LoggedStation){LoggedStation=Station/50;Event(FString::Printf(TEXT("ASCENT_STATION_%d"),Station));}
   return false;
  }
  if(Stage==6)
  {
   Brake();if(Now-At<3)return false;StaticScreenshot(W,TEXT("LandFinal-Continuous-Summit-841.png"));
   // Court approach follows the existing open entrance and driveway. No
   // shortcut through the cliff railing, no replacement / widened geometry.
   Driveway={NeonCityCoastalLayout::FromLegacyIsland(FVector(46660,-119920,30480)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46790,-119640,30480)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46780,-119220,30480)),
    NeonCityCoastalLayout::FromLegacyIsland(FVector(46780,-118760,30480)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46780,-118330,30480)),NeonCityCoastalLayout::FromLegacyIsland(FVector(47000,-118160,30480)),NeonCityCoastalLayout::FromLegacyIsland(FVector(47340,-118160,30480))};
   Waypoint=0;LastProgress=Now;Stage=7;Event(TEXT("DRIVEWAY_ENTRY_NORMAL_THROTTLE"));return false;
  }
  if(Stage==7)
  {
   const float Distance=FVector::Dist2D(Car->GetActorLocation(),Driveway[Waypoint]);
   if(Distance<125 && Waypoint+1<Driveway.Num()){++Waypoint;LastProgress=Now;}
   if(Now-LastProgress>24)return Fail(W,FString::Printf(TEXT("Driveway circulation blocked at waypoint %d position %s"),Waypoint,*Car->GetActorLocation().ToString()));
   if(Waypoint==Driveway.Num()-1 && Distance<180)Brake();else DriveToward(Driveway[Waypoint],DT,Waypoint==Driveway.Num()-1?FMath::Clamp(Distance*.5f,80.f,200.f):260.f);
   if(Waypoint==Driveway.Num()-1 && Distance<180 && FMath::Abs(Car->GetChaosMovement()->GetForwardSpeed())<30){Brake();At=Now;Stage=8;Event(TEXT("PARKED_PHYSICALLY_ON_DRIVEWAY"));}
   return false;
  }
  if(Stage==8)
  {
   Brake();if(Now-At<2)return false;
   if(bCurrentEstate){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Pressed,FPlatformTime::Cycles64()));FPressed=true;Stage=19;At=Now;return false;}
   if(!Car->GetVehicleInteraction()->Exit() || PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision())return Fail(W,TEXT("Ordinary parked vehicle exit failed"));
   Event(TEXT("NATIVE_VEHICLE_EXIT: production safe local exit, no harness relocation"));
   Stage=9;At=Now;return false;
  }
  if(Stage==9)
  {
   if(Now-At<.5f)return false;
   for(TActorIterator<AIslandLandDoor> It(W);It;++It)if(It->GetActorLocation().Z>30000)Door=*It;
   if(!Door)return Fail(W,TEXT("Actual authored mansion entry door missing"));
   EntryWalk={NeonCityCoastalLayout::FromLegacyIsland(FVector(46780,-118350,30580)),NeonCityCoastalLayout::FromLegacyIsland(FVector(46780,-118760,30580)),Door->GetActorTransform().TransformPosition(FVector(65,-185,100))};
   InteriorWalk={Door->GetActorTransform().TransformPosition(FVector(65,170,100))};
   Stage=10;Waypoint=0;LastProgress=Now;return false;
  }
  if(Stage==11)
  {
   if(FPressed && Now-At>.2f){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Released,FPlatformTime::Cycles64()));FPressed=false;}
   if(!Door->IsOpen())
   {
    if(Now-At>20)return Fail(W,TEXT("F input did not open the physical residence pivot door within20s"));
    return false;
   }
   Stage=12;Waypoint=0;LastProgress=Now;return false;
  }
  return false;
 }
private:
 bool CurrentArrival(UWorld* W,APlayerController* PC,float Now)
 {
  if(Stage==25)
  {
   const int32 Nearest=ClosestStation(Car->GetActorLocation(),0,40);
   const FVector Tangent=(Road[FMath::Min(Nearest+1,Road.Num()-1)]-Road[FMath::Max(0,Nearest-1)]).GetSafeNormal2D();
   const bool Aligned=FVector::Dist2D(Car->GetActorLocation(),Road[Nearest])<180 && FMath::Abs(Car->GetActorLocation().Z-Road[Nearest].Z)<150 && FVector::DotProduct(Car->GetActorForwardVector(),Tangent)>.8;
   if(Aligned){Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetBrakeInput(0);MergeStation=Station=Nearest;Stage=18;At=Now;Event(TEXT("NATIVE_MANUAL_LAYBY_PULL_OUT_ALIGNED"));return false;}
   if(Now-At>30)return Fail(W,FString::Printf(TEXT("Normal lay-by pull-out did not reach connected road at%s station%d lane%.1f"),*Car->GetActorLocation().ToString(),Nearest,FVector::Dist2D(Car->GetActorLocation(),Road[Nearest])));
   DriveToward(Road[FMath::Min(Nearest+3,40)],FMath::Clamp(W->GetDeltaSeconds(),.005f,.1f),300);return false;
  }
  // Possession rebuilds Enhanced Input mappings at the end of the frame.
  // A key submitted in the same interaction frame predates that mapping.
  if(Stage==18){if(Now-At<.4f)return false;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::C,IE_Pressed,FPlatformTime::Cycles64()));Stage=24;At=Now;return false;}
  if(Stage==24){if(Now-At<.2f)return false;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::C,IE_Released,FPlatformTime::Cycles64()));if(!Car->GetRoadCruise()->IsActive())return Fail(W,TEXT("Normal C did not start current full-road cruise: ")+Car->GetRoadCruise()->GetReason());if(bAcquireOnly)return Finish(W,!Test->HasAnyErrors());EnableTerminalDiagnostics(W);if(!TerminalRegistered)return Fail(W,TEXT("Native cruise contact observer unavailable"));CurrentContactsObserved=true;Stage=5;At=LastProgress=Now;return false;}
  if(Stage==19)
  {
   if(Now-At<.3f)return false;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Released,FPlatformTime::Cycles64()));FPressed=false;
   if(PC->GetPawn()!=Player || Player->IsHidden() || !Player->GetActorEnableCollision())return Fail(W,TEXT("Actual current garage F exit failed"));
   const FVector Goal=Lift->GetActorTransform().TransformPosition(FVector(225,0,0))+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3);
   if(!Phase205cSupportedWalkPath(W,Player,Goal,EntryWalk,Player->GetSimpleCollisionHalfHeight()+25,25,nullptr,50000))return Fail(W,TEXT("Current garage-to-lift full-capsule circulation blocked"));
   Waypoint=0;Stage=20;At=LastProgress=Now;Event(TEXT("CURRENT_NORMAL_F_EXIT_AND_GARAGE_WALK"));return false;
  }
  if(Stage==20){if(!Walk(W,PC,EntryWalk))return Done;if(Waypoint<EntryWalk.Num())return false;Stage=21;At=Now;return false;}
  if(Stage==21)
  {
   if(Now-At>35)return Fail(W,TEXT("Current continuous arrival lift call/entry blocked"));
   if(Lift->GetCurrentFloor()!=0 || !Lift->AreDoorsOpen())
   {PC->SetControlRotation((Lift->CallPanels[0]->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());if(!FPressed){if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return Fail(W,TEXT("Current garage call prompt blocked"));PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Pressed,FPlatformTime::Cycles64()));FPressed=true;}else {PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Released,FPlatformTime::Cycles64()));FPressed=false;}return false;}
   const FVector D=Lift->Cabin->GetComponentLocation()-Player->GetActorLocation();if(D.Size2D()>25){PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Player->DoMove(0,FMath::Clamp(float(D.Size2D()/180),.1f,1.f));return false;}
   if(Player->GetMovementBaseObject()!=Lift->Floor)return false;PC->SetControlRotation((Lift->Cabin->GetComponentTransform().TransformPosition(FVector(117,103,140))-Player->GetPawnViewLocation()).Rotation());if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return Fail(W,TEXT("Current arrival actual cabin prompt blocked"));
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Pressed,FPlatformTime::Cycles64()));FPressed=true;Stage=22;At=Now;return false;
  }
  if(Stage==22)
  {
   if(FPressed && Now-At>.2f){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Released,FPlatformTime::Cycles64()));FPressed=false;}
   if(Now-At>25)return Fail(W,TEXT("Current arrival native Main lift ride failed"));if(Lift->GetCurrentFloor()==1 && Lift->AreDoorsOpen()){Stage=23;At=Now;}return false;
  }
  if(Stage==23)
  {
   const FVector Goal=Lift->GetActorTransform().TransformPosition(FVector(285,0,304.8)),D=Goal-Player->GetActorLocation();
   if(D.Size2D()>12){if(Now-At>12)return Fail(W,TEXT("Current Main normal cabin exit blocked"));PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Player->DoMove(0,FMath::Clamp(float(D.Size2D()/180),.08f,1.f));return false;}
   if(!Player->GetCharacterMovement()->IsMovingOnGround())return Fail(W,TEXT("Current Main arrival lacks normal grounded player"));
   bool Valid=Test->TestTrue(TEXT("Continuous current road travel follows measured native path"),CarDistance>PlannedDriveCm*.9);Valid=Test->TestEqual(TEXT("One Dock player setup only"),SetupPlayerRelocations,1)&&Valid;Valid=Test->TestEqual(TEXT("No car relocation"),SetupCarRelocations,0)&&Valid;Valid=Test->TestEqual(TEXT("Current644 road/garage stations reached"),HighestStation,Road.Num()-1)&&Valid;
   if(!Save->ConfigureSlot(CurrentArrivalSlot,false) || Save->DoesSaveExist() || !Save->SaveGame())return Fail(W,TEXT("Fresh current arrival save failed"));const auto* Snapshot=Save->ReadSaveForInspection();if(!Snapshot)return Fail(W,TEXT("Current arrival save inspection failed"));for(const FGuid& ID:FishIDs)if(!Snapshot->Fishing.Fish.ContainsByPredicate([&](const FNeonCityCaughtFish& F){return F.FishID==ID;}))return Fail(W,TEXT("Actual caught fish provenance lost during current route"));
   Event(TEXT("CURRENT_DOCK_VILLAGE_ASTER_C_CRUISE_GARAGE_F_EXIT_NORMAL_LIFT_MAIN_COMPLETE"));StaticScreenshot(W,TEXT("CurrentContinuous-MansionMainArrival.png"));return Finish(W,Valid);
  }
  return false;
 }
 bool BuildTerminalGuidance(UWorld* W)
 {
  TArray<AIslandSupercar*> Parked;TArray<TSharedPtr<FJsonValue>> Obstacles;
  for(TActorIterator<AIslandSupercar> It(W);It;++It)
  {
   if(*It==Car || FVector::DistSquared2D(It->GetActorLocation(),Road.Last())>FMath::Square(1800.f) || FMath::Abs(It->GetActorLocation().Z-Road.Last().Z)>250)continue;
   Parked.Add(*It);auto J=MakeShared<FJsonObject>();const auto B=It->GetMesh()->CalcBounds(It->GetMesh()->GetComponentTransform().GetRelativeTransform(It->GetActorTransform()));
   J->SetStringField(TEXT("actor"),It->GetPathName());J->SetStringField(TEXT("vehicle_guid"),It->VehicleID.ToString());J->SetStringField(TEXT("unchanged_pose"),It->GetActorTransform().ToHumanReadableString());J->SetStringField(TEXT("measured_actor_local_bounds_origin_cm"),B.Origin.ToString());J->SetStringField(TEXT("measured_actor_local_bounds_extent_cm"),B.BoxExtent.ToString());Obstacles.Add(MakeShared<FJsonValueObject>(J));
  }
  float BestOffset=0;int32 BestBlocked=MAX_int32;
  for(float Offset:{0.f,-240.f,240.f})
  {
   int32 Blocked=0;
   for(int32 I=832;I<=841;++I)
   {
    const FVector T=(Road[FMath::Min(I+1,841)]-Road[FMath::Max(I-1,0)]).GetSafeNormal2D(),N(-T.Y,T.X,0);
    const float Fade=FMath::Clamp((I-832)/4.f,0.f,1.f);const FVector P=Road[I]+N*Offset*Fade;
    for(auto* Other:Parked)
    {
     const auto B=Other->GetMesh()->CalcBounds(Other->GetMesh()->GetComponentTransform().GetRelativeTransform(Other->GetActorTransform()));
     const FVector Local=Other->GetActorTransform().InverseTransformPosition(P)-B.Origin;
     const FVector Forward=Other->GetActorForwardVector(),Right=Other->GetActorRightVector();
     const float ExtraX=260*FMath::Abs(FVector::DotProduct(T,Forward))+115*FMath::Abs(FVector::DotProduct(N,Forward))+20;
     const float ExtraY=260*FMath::Abs(FVector::DotProduct(T,Right))+115*FMath::Abs(FVector::DotProduct(N,Right))+20;
     if(FMath::Abs(Local.X)<B.BoxExtent.X+ExtraX && FMath::Abs(Local.Y)<B.BoxExtent.Y+ExtraY)++Blocked;
    }
   }
   if(Blocked<BestBlocked){BestBlocked=Blocked;BestOffset=Offset;}
  }
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("scope"),TEXT("TEST INPUT GUIDANCE ONLY: retain every parked vehicle pose/identity, road, physics, guard and normal controls; use existing carriageway width"));J->SetNumberField(TEXT("selected_lateral_offset_cm"),BestOffset);J->SetNumberField(TEXT("predicted_overlap_samples"),BestBlocked);J->SetArrayField(TEXT("retained_native_vehicles"),Obstacles);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("terminal-input-guidance.json")));
  if(BestBlocked>0)return false;TerminalGuidance=Road;
  for(int32 I=832;I<=841;++I){const FVector T=(Road[FMath::Min(I+1,841)]-Road[FMath::Max(I-1,0)]).GetSafeNormal2D();TerminalGuidance[I]+=FVector(-T.Y,T.X,0)*BestOffset*FMath::Clamp((I-832)/4.f,0.f,1.f);}
  Event(FString::Printf(TEXT("TERMINAL_NATIVE_PARKED_VEHICLE_AVOIDANCE: %.1fcm within unchanged lane; %d retained cars, zero predicted body overlaps"),BestOffset,Parked.Num()));return true;
 }
 bool Prepare(UWorld* W,APlayerController* PC)
 {
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-173600,330))))return false;
  if(bCurrentEstate)
  {
   USplineComponent* Main=nullptr;USplineComponent* Link=nullptr;
   for(TActorIterator<ANeonCityTrafficRoute> I(W);I;++I){if(I->RoadID==TEXT("Island.Mountain.Main"))Main=I->GetRoadSpline();if(I->RoadID==TEXT("Island.Mansion.Garage"))Link=I->GetRoadSpline();}
   for(TActorIterator<AIslandGarageDoor> I(W);I;++I)Garage=*I;
   for(TActorIterator<AIslandResidenceLift> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.TheOne478")))Lift=*I;
   if(!Main || Main->GetNumberOfSplinePoints()!=639 || !Link || Link->GetNumberOfSplinePoints()!=24 || !Garage || !Lift)return Fail(W,TEXT("Current native639 road/24 connector/garage/lift required"));
   for(int32 I=0;I<620;++I)Road.Add(Main->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World));for(int32 I=0;I<24;++I)Road.Add(Link->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World));
  }
  else if(!Test->TestTrue(TEXT("Original 842 surveyed road stations"),ReadRoad(Road)))return Finish(W,false);
  ANeonCityReferenceVillage* Village=nullptr;for(TActorIterator<ANeonCityReferenceVillage> It(W);It;++It)Village=*It;
  if(!Village || !ReadVillageWalk(Village->GetActorTransform(),Boardwalk))return Fail(W,TEXT("Canonical village walking supports not available"));
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  CurrentArrivalSlot=TEXT("NeonCity_Test_CurrentArrival_")+FPaths::MakeValidFileName(Label.Left(24));
  Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label.IsEmpty()?FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")):Label)/TEXT("ContinuousDrive");
  IFileManager::Get().MakeDirectory(*Directory,true);
  FFileHelper::SaveStringToFile(TEXT("wall_seconds,game_seconds,stage,station,pawn,x,y,z,yaw,pitch,roll,speed_kmh,target_cm_s,steer,throttle,brake,lane_error_cm,condition,front_steer_deg\n"),*(Directory/TEXT("trajectory.csv")));
  Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  if(!Save->ConfigureSlot(BoatCatchSlot,false))return Fail(W,TEXT("Dedicated real two-catch input slot could not be selected"));
  if(!Save->DoesSaveExist() && !Save->ConfigureSlot(LegacySlot,false))return Fail(W,TEXT("Dedicated legacy-copy input slot could not be selected"));
  if(!Save->DoesSaveExist())return Fail(W,TEXT("Retained real-catch test input required; neither BoatCatch nor byte-exact Legacy15 copy exists; original player save is never written"));
  InputSlot=Save->GetSlotName();
  Legacy.Reset(Save->ReadSaveForInspection());if(!Legacy.Get())return Fail(W,TEXT("Legacy test slot is not an integrity-checked save"));
  LegacySaveID=Legacy->SaveID;
  if(!Save->LoadGame())return Fail(W,TEXT("Legacy test copy could not be loaded during preparation: ")+Save->GetLastDetail());
  Stage=1;Event(TEXT("PREPARATION_ONLY: native real-catch test-slot load before Dock T0: ")+InputSlot);return false;
 }
 bool Walk(UWorld* W,APlayerController* PC,const TArray<FVector>& Path)
 {
  if(Waypoint>=Path.Num())return true;
  const float Now=W->GetTimeSeconds();const FVector Here=Player->GetActorLocation(),To=Path[Waypoint]-Here;
  if(Now-LastProgress>18)return !Fail(W,FString::Printf(TEXT("Normal player walking blocked stage %s waypoint %d at %s target %s mode %d"),StageName(),Waypoint,*Here.ToString(),*Path[Waypoint].ToString(),int32(Player->GetCharacterMovement()->MovementMode)));
  if(To.Size2D()>24)
  {
   const float Step=FMath::Clamp(W->GetDeltaSeconds(),1.f/120.f,.5f);
   const float ApproachDistance=FMath::Max(120.f,Player->GetCharacterMovement()->GetMaxSpeed()*Step*3.f);
   PC->SetControlRotation(FRotator(-7,To.Rotation().Yaw,0));Player->AddMovementInput(To.GetSafeNormal2D(),FMath::Clamp(To.Size2D()/ApproachDistance,.08,1.));return true;
  }
  if(!Player->GetCharacterMovement()->IsMovingOnGround())return !Fail(W,TEXT("Reached walking waypoint without native ground support"));
  ++Waypoint;LastProgress=Now;return true;
 }
 int32 ClosestStation(const FVector& CandidateWorld,int32 First,int32 Last) const
 {
  int32 Best=First;double Distance=DBL_MAX;
  for(int32 I=First;I<=Last;++I){const double D=FVector::DistSquared2D(CandidateWorld,Road[I]);if(D<Distance){Distance=D;Best=I;}}return Best;
 }
 void DriveToward(const FVector& Target,float DT,float Desired)
 {
  auto* Movement=Car->GetChaosMovement();const float Speed=Movement->GetForwardSpeed();
  // Pure pursuit is measured from the physical rear axle. The production
  // Chaos steering curve is evaluated in MPH, as the engine simulation does.
  const FVector Rear=Car->GetActorTransform().TransformPosition(FVector(-139.3,0,0));
  const FVector Delta=Target-Rear;const float Length=FMath::Max(150.,Delta.Size2D());
  const float Alpha=FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Delta.Rotation().Yaw));
  const float Wheelbase=296.7f;
  const float Angle=FMath::RadiansToDegrees(FMath::Atan2(2.f*Wheelbase*FMath::Sin(Alpha),Length));
  const float Scale=FMath::Max(.05f,Movement->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(FMath::Abs(Speed)*.0223693629f));
  const float MaxAngle=Car->GetVehicleProfile()?Car->GetVehicleProfile()->Handling:32.f;
  const float Demand=FMath::Clamp(Angle/(MaxAngle*Scale),-1.f,1.f);
  Steer=FMath::FInterpConstantTo(Steer,Demand,DT,2.6f);
  DesiredSpeed=Desired;
  const float Error=Desired-Speed;
  Throttle=FMath::Clamp(Error/230.f,0.f,.75f);BrakeInput=Error<-30?FMath::Clamp((-Error-30)/200.f,0.f,.85f):0;
  if(BrakeInput>0)Throttle=0;
  auto* Driving=Car->GetDrivingComponent();Driving->SetSteerInput(Steer);Driving->SetDriveInput(Throttle);Driving->SetBrakeInput(BrakeInput);
 }
 void Brake()
 {
  DesiredSpeed=0;Throttle=0;BrakeInput=1;if(!Car)return;
  Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);Car->GetDrivingComponent()->SetSteerInput(0);Steer=0;
 }
 const TCHAR* StageName() const
 {
  static const TCHAR* Names[]={TEXT("prepare"),TEXT("legacy-load-before-route"),TEXT("dock-settle"),TEXT("dock-village-walk"),TEXT("village-aster-walk"),TEXT("continuous-road-drive"),TEXT("summit-viewpoint-stop"),TEXT("driveway-parking"),TEXT("park-settle"),TEXT("native-exit"),TEXT("mansion-entry-walk"),TEXT("physical-door-input"),TEXT("mansion-interior-walk")};
  return Stage>=0 && Stage<UE_ARRAY_COUNT(Names)?Names[Stage]:TEXT("complete");
 }
 FString MetricStage() const
 {
  if(bCurrentEstate && (Stage==25 || Stage==18 || Stage==24))return TEXT("Village-drive-acquisition");
  if(Stage<=2)return TEXT("warmup-prepare-dock");
  if(Stage==3 || Stage==4)return TEXT("Village-walk");
  if(Stage==5)return TEXT("MountainRoad-drive");
  if(Stage==6)return TEXT("stopped-summit-viewpoint");
  if(Stage==7)return TEXT("MansionExterior-driveway-drive");
  if(Stage==8 || Stage==9)return TEXT("stopped-parking-exit");
  if(Stage==10 || Stage==11)return TEXT("MansionExterior-walk");
  return TEXT("MansionInterior-walk");
 }
 void Sample(UWorld* W,APlayerController* PC)
 {
  APawn* Pawn=PC->GetPawn();if(!Pawn)return;const FVector Position=Pawn->GetActorLocation();
  if(Car && PC->GetPawn()==Car){CarDistance+=FVector::Dist(Position,LastCarPosition);LastCarPosition=Position;}
  if(W->GetTimeSeconds()-LastSample<.1f)return;LastSample=W->GetTimeSeconds();const FRotator Rotation=Pawn->GetActorRotation();
  if(TerminalRegistered && Car)
  {
   auto* M=Car->GetChaosMovement();
   for(int32 I=0;I<M->Wheels.Num();++I){const auto& S=M->GetWheelState(I);TerminalWheels+=FString::Printf(TEXT("%.6f,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%s\n"),W->GetTimeSeconds(),Stage,Station,I,S.bInContact?1:0,S.ContactPoint.X,S.ContactPoint.Y,S.ContactPoint.Z,S.NormalizedSuspensionLength,S.SpringForce,S.SlipAngle,*GetNameSafe(S.PhysMaterial.Get()));}
  }
  if(bCurrentEstate && Car && PC->GetPawn()==Car){auto* Native=Car->GetChaosMovement();DesiredSpeed=Car->GetRoadCruise()->GetSafeSpeedKmh()/.036f;Steer=Native->GetSteeringInput();Throttle=Native->GetThrottleInput();BrakeInput=Native->GetBrakeInput();}
  const float WheelAngle=Car && Car->GetChaosMovement()->Wheels.Num()>0 && Car->GetChaosMovement()->Wheels[0]?Car->GetChaosMovement()->Wheels[0]->GetSteerAngle():0;
  CSV+=FString::Printf(TEXT("%.6f,%.6f,%s,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%.4f\n"),FPlatformTime::Seconds()-Started,W->GetTimeSeconds(),StageName(),Station,*GetNameSafe(Pawn),Position.X,Position.Y,Position.Z,Rotation.Yaw,Rotation.Pitch,Rotation.Roll,Car?Car->GetDrivingComponent()->GetSpeedKmh():0,DesiredSpeed,Steer,Throttle,BrakeInput,LaneError,Car?Car->GetDamageState()->GetConditionFraction():0,WheelAngle);
  if(CSV.Len()>32000)Flush();
 }
 void Flush()
 {
  if(Directory.IsEmpty())return;
  if(!CSV.IsEmpty()){FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);CSV.Reset();}
  if(!TerminalContacts.IsEmpty()){FFileHelper::SaveStringToFile(TerminalContacts,*(Directory/TEXT("terminal-native-contacts.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);TerminalContacts.Reset();}
  if(!TerminalWheels.IsEmpty()){FFileHelper::SaveStringToFile(TerminalWheels,*(Directory/TEXT("terminal-native-wheels.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);TerminalWheels.Reset();}
 }
 void EnableTerminalDiagnostics(UWorld* W)
 {
  if(TerminalRegistered || !W || !Car)return;TerminalScene=W->GetPhysicsScene();
  if(!TerminalScene || !TerminalScene->GetSolver() || !TerminalScene->GetSolver()->GetEventManager())return;
  FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,station,mesh,component,x,y,z,nx,ny,nz,impulse,penetration_cm\n"),*(Directory/TEXT("terminal-native-contacts.csv")));
  FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,station,wheel,in_contact,x,y,z,suspension_normalized,spring_force,slip_angle,physical_material\n"),*(Directory/TEXT("terminal-native-wheels.csv")));
  TerminalScene->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData,FContinuousDrive>(Chaos::EEventType::Collision,this,&FContinuousDrive::HandleTerminalContacts);TerminalRegistered=true;
 }
 void UnregisterTerminalContacts()
 {
  if(TerminalRegistered && TerminalScene && TerminalScene->GetSolver() && TerminalScene->GetSolver()->GetEventManager())TerminalScene->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision,this);
  TerminalRegistered=false;TerminalScene=nullptr;
 }
 void HandleTerminalContacts(const Chaos::FCollisionEventData& Data)
 {
  if(!TerminalRegistered || !TerminalScene || !Car)return;
  for(const auto& C:Data.CollisionData.AllCollisionsArray)
  {
   auto* A=TerminalScene->GetOwningComponent<UPrimitiveComponent>(C.Proxy1);auto* B=TerminalScene->GetOwningComponent<UPrimitiveComponent>(C.Proxy2);
   if(A!=Car->GetMesh() && B!=Car->GetMesh())continue;auto* Other=A==Car->GetMesh()?B:A;if(!Other)continue;
   const auto* Static=Cast<UStaticMeshComponent>(Other);const FVector P(C.Location),N(C.Normal),I(C.AccumulatedImpulse);
   if(bCurrentEstate && Stage==5 && FMath::Abs(N.Z)<.7)
   {++CurrentSideContacts;if(FirstCurrentSideContact.IsEmpty())FirstCurrentSideContact=Other->GetPathName()+TEXT(" at ")+P.ToString();}
   TerminalContacts+=FString::Printf(TEXT("%.6f,%d,%d,%s,%s,%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%.4f,%.6f\n"),Car->GetWorld()->GetTimeSeconds(),Stage,Station,Static?*GetNameSafe(Static->GetStaticMesh()):*GetNameSafe(Other->GetOwner()),*Other->GetPathName(),P.X,P.Y,P.Z,N.X,N.Y,N.Z,I.Size(),static_cast<double>(C.PenetrationDepth));
  }
 }
 void Event(const FString& Text)
 {
  const UWorld* World=NeonCityValidationWorld();Events.Add(FString::Printf(TEXT("%.6f %s"),World?World->GetTimeSeconds():0,*Text));Test->AddInfo(Text);
 }
 void StaticScreenshot(UWorld* W,const FString& Name)
 {
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtMetrics"))
   || FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtSkipHUD"))
   || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return;
  // Only after braking / on-foot arrival. This disclosed paused static
  // image is excluded from motion and performance evidence. No readback
  // occurs while the road controller is driving the moving car.
  const bool WasPaused=UGameplayStatics::IsGamePaused(W);UGameplayStatics::SetGamePaused(W,true);
  const bool Captured=FNeonCityVisualEvidence::CaptureHUD(W,Name);
  UGameplayStatics::SetGamePaused(W,WasPaused);
  if(!Captured)Test->AddWarning(TEXT("Optional paused static screenshot unavailable: ")+Name);
  else Event(TEXT("PAUSED_STATIC_SCREENSHOT: ")+Name);
 }
 bool Fail(UWorld* W,const FString& Reason){Failure=Reason;Test->AddError(Reason);return Finish(W,false);}
 bool Finish(UWorld* W,bool Success)
 {
  Done=true;Brake();UnregisterTerminalContacts();Flush();Metrics.Flush();auto* PC=W?W->GetFirstPlayerController():nullptr;
  if(FPressed && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Released,FPlatformTime::Cycles64()));FPressed=false;}
  if(Car){Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetBrakeInput(0);}
  if(Player)Player->ConsumeMovementInputVector();
  if(Directory.IsEmpty())return true;
  if(bCurrentEstate && Car)
  {
   FString ActualRoute=TEXT("station,x,y,z\n");const auto& Points=Car->GetRoadCruise()->GetRoute();
   for(int32 I=0;I<Points.Num();++I)ActualRoute+=FString::Printf(TEXT("%d,%.6f,%.6f,%.6f\n"),I,Points[I].X,Points[I].Y,Points[I].Z);
   FFileHelper::SaveStringToFile(ActualRoute,*(Directory/TEXT("actual-native-cruise-route.csv")));
  }
  auto Receipt=MakeShared<FJsonObject>();Receipt->SetBoolField(TEXT("passed"),Success);Receipt->SetStringField(TEXT("scope"),TEXT("Automated native player route; no subjective handling/visual approval"));
  Receipt->SetStringField(TEXT("runtime_world"),W && W->WorldType==EWorldType::Game?TEXT("Standalone Game"):TEXT("PIE"));
  Receipt->SetBoolField(TEXT("native_cruise_contacts_observed"),CurrentContactsObserved);Receipt->SetNumberField(TEXT("native_cruise_side_contacts"),CurrentSideContacts);Receipt->SetStringField(TEXT("first_native_cruise_side_contact"),FirstCurrentSideContact);
  Receipt->SetStringField(TEXT("failure"),Failure);Receipt->SetStringField(TEXT("finalStage"),StageName());Receipt->SetNumberField(TEXT("lastRoadStation"),Station);
  Receipt->SetNumberField(TEXT("mergeRoadStation"),MergeStation);Receipt->SetNumberField(TEXT("highestRoadStation"),HighestStation);Receipt->SetNumberField(TEXT("actualCarTravelCm"),CarDistance);
  Receipt->SetNumberField(TEXT("explicitPlayerSetupRelocations"),SetupPlayerRelocations);Receipt->SetNumberField(TEXT("explicitCarSetupRelocations"),SetupCarRelocations);
  Receipt->SetNumberField(TEXT("routeTimeZeroGameSeconds"),RouteT0);Receipt->SetStringField(TEXT("legacyCopiedSaveId"),LegacySaveID.ToString());Receipt->SetStringField(TEXT("arrivalSlot"),Success && !bAcquireOnly && !bTerminalOnly?(bCurrentEstate?CurrentArrivalSlot:ArrivalSlot):TEXT("NOT WRITTEN"));
  Receipt->SetBoolField(TEXT("isolatedVillageAcquisitionFixture"),bAcquireOnly);Receipt->SetBoolField(TEXT("isolatedTerminalFixture"),bTerminalOnly);Receipt->SetBoolField(TEXT("canCertifyDockContinuousGate"),!bAcquireOnly && !bTerminalOnly);
  Receipt->SetStringField(TEXT("inputSlot"),InputSlot);Receipt->SetNumberField(TEXT("inputRealCatchCount"),FishIDs.Num());
  Receipt->SetBoolField(TEXT("current_native_estate_and_normal_C_cruise"),bCurrentEstate);Receipt->SetNumberField(TEXT("planned_current_drive_cm"),PlannedDriveCm);
  Receipt->SetBoolField(TEXT("continuousGpuReadback"),false);Receipt->SetBoolField(TEXT("driverInputsReset"),true);
  Receipt->SetStringField(TEXT("shortEntryConnectionDisclosure"),FString::Printf(TEXT("Road stations 0..%d precede the actual lay-by merge; not claimed as driven"),FMath::Max(0,MergeStation-1)));
  TArray<TSharedPtr<FJsonValue>> EventValues,FishValues,Stations;
  for(const auto& E:Events)EventValues.Add(MakeShared<FJsonValueString>(E));for(const auto& ID:FishIDs)FishValues.Add(MakeShared<FJsonValueString>(ID.ToString()));
  auto Sorted=VisitedStations.Array();Sorted.Sort();for(int32 I:Sorted)Stations.Add(MakeShared<FJsonValueNumber>(I));
  Receipt->SetArrayField(TEXT("events"),EventValues);Receipt->SetArrayField(TEXT("realCatchGuids"),FishValues);Receipt->SetArrayField(TEXT("observedRoadStations"),Stations);
  if(Car)
  {
   const FVector P=Car->GetActorLocation();Receipt->SetStringField(TEXT("vehicleGuid"),Car->VehicleID.ToString());Receipt->SetStringField(TEXT("carFinalLocation"),P.ToString());
   Receipt->SetNumberField(TEXT("initialCondition"),InitialCondition);Receipt->SetNumberField(TEXT("finalCondition"),Car->GetDamageState()->GetConditionFraction());
  }
  FString JSON;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&JSON));Test->TestTrue(TEXT("Continuous route receipt written"),FFileHelper::SaveStringToFile(JSON,*(Directory/TEXT("receipt.json"))));
  return true;
 }
 FAutomationTestBase* Test;bool Done=false,FPressed=false,SummitStop=false,bAcquireOnly=false,bTerminalOnly=false,TerminalRegistered=false,bCurrentEstate=false;
 FPhysScene_Chaos* TerminalScene=nullptr;FString TerminalContacts,TerminalWheels;
 bool CurrentContactsObserved=false;int32 CurrentSideContacts=0;FString FirstCurrentSideContact;
 int32 Stage=0,Waypoint=0,Station=0,MergeStation=0,HighestStation=0,LoggedStation=-1,SetupPlayerRelocations=0,SetupCarRelocations=0;
 double Started=0,CarDistance=0,PlannedDriveCm=0;FString CurrentArrivalSlot;AIslandGarageDoor* Garage=nullptr;AIslandResidenceLift* Lift=nullptr;float At=0,LastProgress=0,LastSample=0,PreviousGame=0,RouteT0=0;
 float DesiredSpeed=0,Steer=0,Throttle=0,BrakeInput=0,LaneError=0,InitialCondition=0;
 FVector LastPawnPosition=FVector::ZeroVector,LastCarPosition=FVector::ZeroVector;
 TArray<FVector> Road,TerminalGuidance,Boardwalk,GroundWalk,Driveway,EntryWalk,InteriorWalk;TSet<int32> VisitedStations;
 TArray<FString> Events;TArray<FGuid> FishIDs;FGuid LegacySaveID;FString Directory,CSV,Failure,InputSlot;
 FNeonCityLandFrameMetrics Metrics{TEXT("ContinuousDrive")};
 ANeonCityPlayerCharacter* Player=nullptr;AIslandSupercar* Car=nullptr;AIslandLandDoor* Door=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 TStrongObjectPtr<UNeonCitySaveGame> Legacy;
};

/** Independent, explicitly relocated fixture. It cannot satisfy the Dock route
 * gate. It covers the second production chassis on the upper switchbacks,
 * station 593, retained road-chunk connections and the repaired terminal. */
class FKestrelUpperConnectors : public IAutomationLatentCommand
{
public:
 explicit FKestrelUpperConnectors(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?W->GetFirstPlayerController():nullptr;if(!W || !PC || W->GetTimeSeconds()<3)return false;
  const float Now=W->GetTimeSeconds(),DT=FMath::Clamp(Now-Previous,0.f,.1f);Previous=Now;
  Metrics.Sample(W,Stage<=1?TEXT("warmup-Kestrel-fixture"):Stage==3?TEXT("stopped-Kestrel-summit"):TEXT("MountainRoad-Kestrel-drive"));
  if(FPlatformTime::Seconds()-Started>420)return Finish(false,FString::Printf(TEXT("Kestrel isolated upper route timeout station %d"),Station));
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30580))))return false;
   if(!ReadRoad(Road))return Finish(false,TEXT("Missing original alignment"));
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->bRoadster){Car=*It;break;}
   if(!Car)return Finish(false,TEXT("Original production Kestrel not available"));
   if(!Test->TestNotNull(TEXT("Authored Kestrel profile available"),Car->GetVehicleProfile()) || !Test->TestEqual(TEXT("Original second hero chassis profile"),Car->GetVehicleProfile()->GetFName(),FName(TEXT("DA_HeroKestrel"))))return Finish(false,TEXT("Wrong Kestrel profile"));
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label.IsEmpty()?FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")):Label)/TEXT("KestrelUpperConnectors");
   IFileManager::Get().MakeDirectory(*Directory,true);
   FFileHelper::SaveStringToFile(TEXT("game_seconds,station,x,y,z,yaw,pitch,roll,speed_kmh,lane_cm,steer,throttle,brake\n"),*(Directory/TEXT("trajectory.csv")));
   const FRotator Rotation=(Road[578]-Road[576]).Rotation();
   Car->SetActorTransform(FTransform(Rotation,Road[577]+FVector(0,0,40)),false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Player->SetActorLocation(Road[577]-Car->GetActorRightVector()*245+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   Test->AddInfo(TEXT("ISOLATED KESTREL FIXTURE ONLY: one initial car relocation and one initial player relocation at station577; cannot substitute for continuous Dock route."));
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   At=Now;Stage=1;return false;
  }
  if(Stage==1)
  {
   if(Now-At<2)return false;
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Car->GetVehicleInteraction()->Enter(Player) || PC->GetPawn()!=Car)
    return Finish(false,TEXT("Kestrel fixture did not support ordinary local vehicle entry"));
   InitialCondition=Car->GetDamageState()->GetConditionFraction();Station=577;LastProgress=Now;LastPosition=Car->GetActorLocation();Stage=2;return false;
  }
  const FVector Here=Car->GetActorLocation();Distance+=FVector::Dist(Here,LastPosition);LastPosition=Here;
  int32 Best=Station;double BestDistance=DBL_MAX;
  for(int32 I=FMath::Max(577,Station-3);I<=FMath::Min(841,Station+16);++I)
  {
   const double D=FVector::DistSquared2D(Here,Road[I]);if(D<BestDistance){Best=I;BestDistance=D;}
  }
  if(Best>Station){Station=Best;LastProgress=Now;}
  if(FMath::Sqrt(BestDistance)>330 || Here.Z<Road[Best].Z-100 || FMath::Abs(Car->GetActorRotation().Roll)>32 || Now-LastProgress>22)
   return Finish(false,FString::Printf(TEXT("Kestrel physical connector failure station %d lane %.1f height %.1f roll %.1f speed %.2f"),Station,FMath::Sqrt(BestDistance),Here.Z-Road[Best].Z,Car->GetActorRotation().Roll,Car->GetDrivingComponent()->GetSpeedKmh()));
  const float Speed=Car->GetChaosMovement()->GetForwardSpeed();
  const float EndDistance=FVector::Dist2D(Here,Road.Last());
  if(Station>=838 && EndDistance<180)
  {
   Stage=3;Throttle=0;BrakeInput=1;Car->GetDrivingComponent()->SetDriveInput(0);Car->GetDrivingComponent()->SetSteerInput(0);Car->GetDrivingComponent()->SetBrakeInput(1);
   if(FMath::Abs(Speed)<35)
   {
    bool Valid=Test->TestTrue(TEXT("Kestrel physically traversed station577 through the upper connectors to summit841"),Distance>76000);
    Valid=Test->TestTrue(TEXT("Upper connector test did not cause severe collision damage"),Car->GetDamageState()->GetConditionFraction()>InitialCondition-.15f)&&Valid;
    Test->AddInfo(TEXT("Station593 and repaired upper/terminal road connections reached by real Kestrel Chaos movement; isolated fixture, no Dock continuity claim."));
    return Finish(Valid,Valid?TEXT(""):TEXT("Kestrel quantitative upper-route gate failed"));
   }
  }
  else
  {
   const FVector Rear=Car->GetActorTransform().TransformPosition(FVector(-154.8,0,0));
   const float Lookahead=FMath::Clamp(550.f+FMath::Abs(Speed)*.45f,600.f,1000.f);int32 Ahead=Station;float Remaining=Lookahead;
   while(Ahead<841 && Remaining>FVector::Dist2D(Road[Ahead],Road[Ahead+1])){Remaining-=FVector::Dist2D(Road[Ahead],Road[Ahead+1]);++Ahead;}
   FVector Target=Road[Ahead];if(Ahead<841)Target=FMath::Lerp(Road[Ahead],Road[Ahead+1],FMath::Clamp(Remaining/FVector::Dist2D(Road[Ahead],Road[Ahead+1]),0.,1.));
   const FVector Delta=Target-Rear;const float Alpha=FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Delta.Rotation().Yaw));
   const float Angle=FMath::RadiansToDegrees(FMath::Atan2(2.f*274.4f*FMath::Sin(Alpha),static_cast<float>(FMath::Max(150.,Delta.Size2D()))));
   const float Scale=FMath::Max(.05f,Car->GetChaosMovement()->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(FMath::Abs(Speed)*.0223693629f));
   const float Demand=FMath::Clamp(Angle/(Car->GetVehicleProfile()->Handling*Scale),-1.f,1.f);
   Steer=FMath::FInterpConstantTo(Steer,Demand,DT,2.6f);float Curvature=0;
   for(int32 I=FMath::Max(1,Station);I<FMath::Min(Station+9,841);++I)
   {
    const FVector A=(Road[I]-Road[I-1]).GetSafeNormal2D(),B=(Road[I+1]-Road[I]).GetSafeNormal2D();
    Curvature=FMath::Max(Curvature,static_cast<float>(FMath::Acos(FMath::Clamp(FVector::DotProduct(A,B),-1.,1.))/FMath::Max(1.,FVector::Dist2D(Road[I],Road[I+1]))));
   }
   const float Desired=Station>=838?FMath::Clamp(EndDistance*.55f,100.f,280.f):FMath::Clamp(FMath::Sqrt(140.f/FMath::Max(Curvature,.00001f)),300.f,780.f);
   const float Error=Desired-Speed;Throttle=FMath::Clamp(Error/230.f,0.f,.75f);BrakeInput=Error<-30?FMath::Clamp((-Error-30)/200.f,0.f,.85f):0;
   if(BrakeInput>0)Throttle=0;auto* Drive=Car->GetDrivingComponent();Drive->SetDriveInput(Throttle);Drive->SetSteerInput(Steer);Drive->SetBrakeInput(BrakeInput);
  }
  if(Now-LastSample>.1f)
  {
   LastSample=Now;const FRotator R=Car->GetActorRotation();CSV+=FString::Printf(TEXT("%.6f,%d,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f\n"),Now,Station,Here.X,Here.Y,Here.Z,R.Yaw,R.Pitch,R.Roll,Speed*.036f,FMath::Sqrt(BestDistance),Steer,Throttle,BrakeInput);
   if(CSV.Len()>32000)Flush();
  }
  return false;
 }
private:
 void Flush(){if(CSV.IsEmpty() || Directory.IsEmpty())return;FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);CSV.Reset();}
 bool Finish(bool Passed,const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);Flush();Metrics.Flush();if(Car){auto* D=Car->GetDrivingComponent();D->SetDriveInput(0);D->SetSteerInput(0);D->SetBrakeInput(0);}
  if(!Directory.IsEmpty())
  {
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetBoolField(TEXT("isolatedFixture"),true);J->SetBoolField(TEXT("canSatisfyDockContinuousGate"),false);J->SetNumberField(TEXT("startRoadStation"),577);J->SetNumberField(TEXT("lastRoadStation"),Station);J->SetNumberField(TEXT("travelCm"),Distance);J->SetStringField(TEXT("failure"),Error);J->SetBoolField(TEXT("continuousGpuReadback"),false);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));Test->TestTrue(TEXT("Kestrel isolated fixture receipt written"),FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json"))));
  }
  return true;
 }
 FAutomationTestBase* Test;AIslandSupercar* Car=nullptr;ANeonCityPlayerCharacter* Player=nullptr;TArray<FVector> Road;
 int32 Stage=0,Station=577;double Started=0,Distance=0;float Previous=0,At=0,LastProgress=0,LastSample=0,InitialCondition=0,Steer=0,Throttle=0,BrakeInput=0;
 FVector LastPosition=FVector::ZeroVector;FString Directory,CSV;
 FNeonCityLandFrameMetrics Metrics{TEXT("KestrelUpperConnectors")};
};

/** Reproduce the actual outer-side station593 guard contact with the real
 * production Aster or Kestrel. Capture solver contact events, then use only ordinary
 * reverse / steering / forward inputs to leave the contact. */
class FGuardrailContact : public IAutomationLatentCommand
{
public:
 explicit FGuardrailContact(FAutomationTestBase* In,bool InKestrel=false):Test(In),bKestrel(InKestrel),Metrics(InKestrel?TEXT("GuardrailContact593-Kestrel"):TEXT("GuardrailContact593")){}
 ~FGuardrailContact(){Unregister();}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
  auto* PC=W?W->GetFirstPlayerController():nullptr;if(!W || !PC || W->GetTimeSeconds()<3)return false;
  if(FPlatformTime::Seconds()-Started>110)return Finish(false,FString::Printf(TEXT("Guard contact fixture timeout stage %d"),Stage));
  Metrics.Sample(W,Stage<=1?TEXT("warmup-guard-fixture"):Stage==3 || Stage==5?TEXT("stopped-guard-contact"):Stage==4?TEXT("Guardrail-reverse-recovery"):Stage==6?TEXT("Guardrail-forward-recovery"):TEXT("Guardrail-real-contact-approach"));
  const float Now=W->GetTimeSeconds(),DT=FMath::Clamp(Now-Previous,0.f,.1f);Previous=Now;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(53323,-124420,20500))))return false;
   if(!ReadRoad(Road))return Finish(false,TEXT("Original road alignment not available"));
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return false;
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->bRoadster==bKestrel){Car=*It;break;}
   const FName RequiredProfile(bKestrel?TEXT("DA_HeroKestrel"):TEXT("DA_HeroAster"));
   if(!Car || !Car->GetVehicleProfile() || Car->GetVehicleProfile()->GetFName()!=RequiredProfile)return Finish(false,TEXT("Original production ")+FString(Model())+TEXT(" required for guard reproduction"));
   World=W;Scene=W->GetPhysicsScene();if(!Scene || !Scene->GetSolver() || !Scene->GetSolver()->GetEventManager())return Finish(false,TEXT("Native Chaos event stream unavailable"));
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label.IsEmpty()?FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")):Label)/(bKestrel?TEXT("GuardrailContact593-Kestrel"):TEXT("GuardrailContact593"));
   IFileManager::Get().MakeDirectory(*Directory,true);
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,station,x,y,z,yaw,pitch,roll,speed_kmh,signed_lateral_cm,condition,steer,throttle,brake,guard_contacts\n"),*(Directory/TEXT("trajectory.csv")));
   FFileHelper::SaveStringToFile(TEXT("game_seconds,solver_seconds,other_actor,other_component,location_x,location_y,location_z,normal_x,normal_y,normal_z,impulse_x,impulse_y,impulse_z,penetration_cm,signed_lateral_cm,height_above_road_cm,guard_hardware_contact\n"),*(Directory/TEXT("contacts.csv")));
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,station,wheel,in_contact,contact_x,contact_y,contact_z,hit_x,hit_y,hit_z,suspension_normalized,spring_force,slip_angle,skidding,skid_normal_x,skid_normal_y,skid_normal_z,physical_material\n"),*(Directory/TEXT("wheel-state.csv")));
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,station,target_x,target_y,target_z,target_offset_cm,target_in_car_forward_cm,road_heading_error_degrees,steer,throttle,brake\n"),*(Directory/TEXT("controller.csv")));
   const FVector Tangent=(Road[592]-Road[590]).GetSafeNormal2D();const FVector Right(-Tangent.Y,Tangent.X,0);
   const FRotator Rotation=(Road[592]-Road[590]).Rotation();
   Car->SetActorTransform(FTransform(Rotation,Road[591]+Right*235+FVector(0,0,40)),false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Player->SetActorLocation(Road[591]-Right*10+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
   Test->AddInfo(TEXT("GUARD CONTACT ISOLATED FIXTURE: one initial ")+FString(Model())+TEXT(" placement and one initial player placement at591, same outer/right side as Manual14; no later pose/velocity writes, no save operations."));
   At=Now;Stage=1;return false;
  }
  if(Stage==1)
  {
   if(Now-At<2)return false;
   if(!GeometryRecorded && !RecordProductionGeometry())return Finish(false,TEXT("Production wheel geometry unavailable; do not substitute another model's rear axle / wheelbase"));
   // Read the original scene's collision before driving. Query hits are
   // diagnostics only: none can count as a native guard contact or a PASS.
   if(!GuardProbed){ProbeGuardHardware(W);GuardProbed=true;}
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Car->GetVehicleInteraction()->Enter(Player) || PC->GetPawn()!=Car)return Finish(false,TEXT("Physical guard fixture could not enter the actual ")+FString(Model())+TEXT(" normally"));
   InitialCondition=Car->GetDamageState()->GetConditionFraction();
   Scene->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData,FGuardrailContact>(Chaos::EEventType::Collision,this,&FGuardrailContact::HandleContacts);
   Registered=true;At=Now;Stage=2;return false;
  }
  int32 Best=591;double BestDistance=DBL_MAX;
  for(int32 I=585;I<=610;++I){const double D=FVector::DistSquared2D(Car->GetActorLocation(),Road[I]);if(D<BestDistance){Best=I;BestDistance=D;}}
  Station=Best;const FVector T=(Road[FMath::Min(Best+1,841)]-Road[FMath::Max(Best-1,0)]).GetSafeNormal2D(),N(-T.Y,T.X,0);
  SignedLateral=FVector::DotProduct(Car->GetActorLocation()-Road[Best],N);MaxLateral=FMath::Max(MaxLateral,SignedLateral);
  MaxRoll=FMath::Max(MaxRoll,FMath::Abs(Car->GetActorRotation().Roll));
  // Sample every exported gameplay frame, including a final failure/pass
  // frame. A brief full-compression / guard-top force spike must survive.
  SampleWheels(Now,Best);
  if(SignedLateral>430 || Car->GetActorLocation().Z<Road[Best].Z-120 || MaxRoll>32)return Finish(false,FString::Printf(TEXT("Guard containment failed station %d lateral %.1f cm roll %.1f height %.1f"),Best,SignedLateral,MaxRoll,Car->GetActorLocation().Z-Road[Best].Z));
  if(Stage==2)
  {
   if(GuardContacts>0)
   {
    ContactPosition=Car->GetActorLocation();ContactStation=Best;ContactAt=Now;Brake();At=Now;Stage=3;
    Test->AddInfo(FString::Printf(TEXT("REAL CHAOS GUARD CONTACT: station %d component %s, car %s, roll %.2f, normal %s, impulse %s"),Best,*FirstGuardComponent,*ContactPosition.ToString(),Car->GetActorRotation().Roll,*FirstGuardNormal.ToString(),*FirstGuardImpulse.ToString()));
   }
   else
   {
    // The former fixed station594 target fell behind the rear axle and
    // steered the nose into Landscape. Keep a rolling 4.5 m lookahead along
    // the surveyed road instead, with the same speed and lateral ramp.
    // The moving target follows the guard's curve; it never chases a point
    // that the vehicle has already passed.
    TargetOffset=FMath::Min(385.f,275.f+(Now-At)*24.f);
    LastTarget=ParallelTarget(Best,TargetOffset,450.f);
    const FVector Rear=Car->GetActorTransform().TransformPosition(FVector(RearAxleX,0,0));
    TargetForward=FVector::DotProduct(LastTarget-Rear,Car->GetActorForwardVector());
    if(TargetForward<100)return Finish(false,TEXT("Guard approach target left the forward field; fixture cannot claim a parallel-contact result"));
    ForwardTarget(LastTarget,DT,170.f);
    if(Now-At>14 || Best>600)return Finish(false,TEXT("No native guard-hardware contact generated; fixture cannot claim a guard contact PASS"));
   }
  }
  else if(Stage==3)
  {
   Brake();if(FMath::Abs(Car->GetChaosMovement()->GetForwardSpeed())<25 && Now-At>.4f){At=Now;Stage=4;}
   else if(Now-At>4)return Finish(false,FString(Model())+TEXT(" could not brake after real guard contact"));
  }
  else if(Stage==4)
  {
   // Ordinary S/D: reverse and right steering rotates the nose away from the
   // right-hand guard. A short reverse avoids swinging the rear across it.
   SetInputs(-.36f,.60f,0);ReverseDistance=FVector::Dist2D(Car->GetActorLocation(),ContactPosition);
   if(Now-At>=1.4f || ReverseDistance>190){Brake();At=Now;Stage=5;}
  }
  else if(Stage==5)
  {
   Brake();if(FMath::Abs(Car->GetChaosMovement()->GetForwardSpeed())<25 && Now-At>.35f){At=Now;Stage=6;}
   else if(Now-At>4)return Finish(false,FString(Model())+TEXT(" could not settle after reverse recovery input"));
  }
  else if(Stage==6)
  {
   // Ordinary forward / left correction, using the retained road centre as a
   // destination. No force, torque, pose, collision or tuning manipulation.
   // A guard recovery starts only centimetres from the outside rail. The
   // former fixed four-station target (~8m away) demanded too little steering
   // and drove the production lower-box front corner back into the next bend.
   // Project onto the same surveyed centre line and use a 2.5m lookahead for
   // this low-speed correction. All containment/contact/time gates stay exact.
   LastTarget=ParallelTarget(Best,0.f,250.f);
   ForwardTarget(LastTarget,DT,230.f);
   if(FVector::Dist2D(Car->GetActorLocation(),ContactPosition)>600 && FMath::Abs(SignedLateral)<220 && Car->GetChaosMovement()->GetForwardSpeed()>80 && FMath::Abs(Car->GetActorRotation().Roll)<15)
   {
    bool Valid=Test->TestTrue(TEXT("Actual guard contact and native reverse/forward recovery occurred"),GuardContacts>0 && ReverseDistance>40);
    Valid=Test->TestTrue(TEXT("Guard retained safe centre containment"),MaxLateral<=430)&&Valid;
    Valid=Test->TestTrue(TEXT("Low-speed guard recovery did not destroy the chassis"),Car->GetDamageState()->GetConditionFraction()>InitialCondition-.15f)&&Valid;
    Valid=Test->TestTrue(TEXT("Independent guard retains native car/Pawn blocking and production wheel exclusion"),SeparateGuardCount>0 && SeparateGuardPolicyFailures==0 && PawnGuardQueryHits>0 && WheelGuardQueryHits==0)&&Valid;
    Valid=Test->TestTrue(TEXT("Native suspension did not use raised roadside hardware as wheel support"),RaisedEdgeWheelSupportSamples==0)&&Valid;
    return Finish(Valid,Valid?TEXT(""):TEXT("Guard recovery quantitative gate failed"));
   }
   if(Now-At>12)return Finish(false,FString::Printf(TEXT("GUARD SNAG REPRODUCED: native S/D %.1f cm reverse then forward/left unable to clear within12s; station %d car %s roll %.2f speed %.2f condition %.4f"),ReverseDistance,Best,*Car->GetActorLocation().ToString(),Car->GetActorRotation().Roll,Car->GetDrivingComponent()->GetSpeedKmh(),Car->GetDamageState()->GetConditionFraction()));
  }
  if(Now-LastSample>.05f)
  {
   LastSample=Now;const FVector P=Car->GetActorLocation();const FRotator R=Car->GetActorRotation();
   Trajectory+=FString::Printf(TEXT("%.6f,%d,%d,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.4f,%.4f,%.4f,%d\n"),Now,Stage,Best,P.X,P.Y,P.Z,R.Yaw,R.Pitch,R.Roll,Car->GetDrivingComponent()->GetSpeedKmh(),SignedLateral,Car->GetDamageState()->GetConditionFraction(),Steer,Throttle,BrakeInput,GuardContacts);
   ControllerRows+=FString::Printf(TEXT("%.6f,%d,%d,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f\n"),Now,Stage,Best,LastTarget.X,LastTarget.Y,LastTarget.Z,TargetOffset,TargetForward,FMath::FindDeltaAngleDegrees(T.Rotation().Yaw,R.Yaw),Steer,Throttle,BrakeInput);
   if(Trajectory.Len()>24000 || ContactRows.Len()>24000 || WheelRows.Len()>24000)Flush();
  }
  return false;
 }
private:
 const TCHAR* Model()const{return bKestrel?TEXT("Kestrel"):TEXT("Aster");}
 static bool IsHardwareMesh(const UStaticMesh* Mesh){return Mesh && (Mesh->GetName().Contains(TEXT("AdventureRoad")) || Mesh->GetName().Contains(TEXT("Guard")));}
 bool RecordProductionGeometry()
 {
  auto* Movement=Car->GetChaosMovement();auto* Mesh=Car->GetMesh();const auto* Rig=Mesh->GetSkeletalMeshAsset();
  if(!Rig || Movement->WheelSetups.Num()!=4 || Movement->GetNumWheels()!=4)return false;
  FMatrix RootBody=FMatrix::Identity;
  if(const auto* Body=Mesh->GetBodyInstance();Body && Body->BodySetup.IsValid())RootBody=Rig->GetComposedRefPoseMatrix(Body->BodySetup->BoneName);
  double Front=0,Rear=0;int32 FrontCount=0,RearCount=0;TArray<TSharedPtr<FJsonValue>> WheelConfig;
  for(int32 I=0;I<Movement->WheelSetups.Num();++I)
  {
   const auto& Setup=Movement->WheelSetups[I];const UChaosVehicleWheel* Default=Setup.WheelClass.GetDefaultObject();auto* Live=Movement->Wheels[I].Get();if(!Default || !Live)return false;
   FVector Rest=Default->Offset+Setup.AdditionalOffset;
   if(Setup.BoneName!=NAME_None)Rest+=RootBody.InverseTransformPosition(Rig->GetComposedRefPoseMatrix(Setup.BoneName).GetOrigin()*Mesh->GetRelativeScale3D());
   if(Default->bAffectedBySteering){Front+=Rest.X;++FrontCount;}else{Rear+=Rest.X;++RearCount;}
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("wheel"),I);Row->SetStringField(TEXT("bone"),Setup.BoneName.ToString());Row->SetStringField(TEXT("productionClass"),Default->GetClass()->GetPathName());Row->SetStringField(TEXT("restingBodyPosition"),Rest.ToString());
   Row->SetNumberField(TEXT("radiusCm"),Live->WheelRadius);Row->SetNumberField(TEXT("widthCm"),Live->WheelWidth);Row->SetNumberField(TEXT("maxSteerAngleDegrees"),Live->MaxSteerAngle);Row->SetBoolField(TEXT("affectedBySteering"),Live->bAffectedBySteering);
   Row->SetNumberField(TEXT("sweepShape"),static_cast<int32>(Live->SweepShape));Row->SetNumberField(TEXT("sweepType"),static_cast<int32>(Live->SweepType));WheelConfig.Add(MakeShared<FJsonValueObject>(Row));
  }
  if(FrontCount!=2 || RearCount!=2)return false;RearAxleX=static_cast<float>(Rear/RearCount);Wheelbase=static_cast<float>(Front/FrontCount-RearAxleX);if(Wheelbase<200 || Wheelbase>400)return false;
  WheelPeaks.SetNum(4);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("model"),Model());J->SetStringField(TEXT("profile"),Car->GetVehicleProfile()->GetPathName());J->SetNumberField(TEXT("rearAxleBodyXCm"),RearAxleX);J->SetNumberField(TEXT("wheelbaseCm"),Wheelbase);J->SetNumberField(TEXT("unchangedHandling"),Car->GetVehicleProfile()->Handling);
  J->SetStringField(TEXT("geometryMethod"),TEXT("Production LocateBoneOffset: composed reference pose, root body inverse pose, relative scale, WheelClass.Offset + WheelSetup.AdditionalOffset"));J->SetArrayField(TEXT("wheels"),WheelConfig);
  J->SetNumberField(TEXT("nativeSuspensionQueryChannel"),static_cast<int32>(ECC_WorldDynamic));J->SetStringField(TEXT("nativeSuspensionQueryChannelName"),TEXT("ECC_WorldDynamic; ChaosWheeledVehicleMovementComponent.cpp PerformSuspensionTraces"));J->SetStringField(TEXT("suspensionShape"),TEXT("Actual per-wheel production Spherecast / SweepType above; no wheel settings modified"));
  J->SetNumberField(TEXT("wheelTraceVehicleResponse"),static_cast<int32>(Movement->WheelTraceCollisionResponses.GetResponse(ECC_Vehicle)));J->SetNumberField(TEXT("wheelTracePawnResponse"),static_cast<int32>(Movement->WheelTraceCollisionResponses.GetResponse(ECC_Pawn)));J->SetNumberField(TEXT("wheelTraceWorldStaticResponse"),static_cast<int32>(Movement->WheelTraceCollisionResponses.GetResponse(ECC_WorldStatic)));
  J->SetStringField(TEXT("bodyRayDifference"),TEXT("guard-probes uses Vehicle/Visibility + default allBlock; guard-query-policy also compares Pawn blocking and WorldDynamic with the actual native WheelTraceCollisionResponses container"));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));GeometryRecorded=FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("production-wheel-config.json")));return GeometryRecorded;
 }
 int32 PointRoadFrame(const FVector& Position,float& Lateral,float& Height)const
 {
  int32 Best=591;double Distance=DBL_MAX;for(int32 I=585;I<=610;++I){const double D=FVector::DistSquared2D(Position,Road[I]);if(D<Distance){Best=I;Distance=D;}}
  const FVector Segment=Road[Best+1]-Road[Best-1],Tangent=Segment.GetSafeNormal2D(),Right(-Tangent.Y,Tangent.X,0);
  const float Fraction=static_cast<float>(FMath::Clamp(FVector::DotProduct((Position-Road[Best-1])*FVector(1,1,0),Segment*FVector(1,1,0))/FMath::Max(1.,Segment.SizeSquared2D()),0.,1.));
  const FVector OnRoad=FMath::Lerp(Road[Best-1],Road[Best+1],Fraction);Lateral=static_cast<float>(FVector::DotProduct(Position-OnRoad,Right));Height=static_cast<float>(Position.Z-OnRoad.Z);return Best;
 }
 void SampleWheels(float Now,int32 Best)
 {
  auto* Movement=Car->GetChaosMovement();
  for(int32 I=0;I<Movement->GetNumWheels();++I)
  {
   const auto& S=Movement->GetWheelState(I);float Lateral=0,Height=0;PointRoadFrame(S.ContactPoint,Lateral,Height);
   WheelRows+=FString::Printf(TEXT("%.6f,%d,%d,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%s\n"),Now,Stage,Best,I,S.bInContact?1:0,S.ContactPoint.X,S.ContactPoint.Y,S.ContactPoint.Z,S.HitLocation.X,S.HitLocation.Y,S.HitLocation.Z,S.NormalizedSuspensionLength,S.SpringForce,S.SlipAngle,S.bIsSkidding?1:0,S.SkidNormal.X,S.SkidNormal.Y,S.SkidNormal.Z,*GetNameSafe(S.PhysMaterial.Get()));
   if(WheelPeaks.IsValidIndex(I))
   {
    auto& Peak=WheelPeaks[I];Peak.MinimumForce=FMath::Min(Peak.MinimumForce,S.SpringForce);
    if(S.SpringForce>Peak.Force){Peak.Force=S.SpringForce;Peak.Now=Now;Peak.Stage=Stage;Peak.Station=Best;Peak.Contact=S.ContactPoint;Peak.Centre=S.HitLocation;Peak.Suspension=S.NormalizedSuspensionLength;Peak.Height=Height;Peak.Lateral=Lateral;}
    if(S.bInContact && S.SpringForce>0 && Lateral>365 && Lateral<470 && Height>8 && Height<130){++Peak.RaisedSamples;++RaisedEdgeWheelSupportSamples;}
   }
  }
 }
 static const TCHAR* CollisionMode(ECollisionEnabled::Type Mode)
 {
  switch(Mode)
  {
   case ECollisionEnabled::NoCollision:return TEXT("NoCollision");
   case ECollisionEnabled::QueryOnly:return TEXT("QueryOnly");
   case ECollisionEnabled::PhysicsOnly:return TEXT("PhysicsOnly");
   case ECollisionEnabled::QueryAndPhysics:return TEXT("QueryAndPhysics");
   case ECollisionEnabled::ProbeOnly:return TEXT("ProbeOnly");
   case ECollisionEnabled::QueryAndProbe:return TEXT("QueryAndProbe");
   default:return TEXT("Unknown");
  }
 }
 static const TCHAR* TracePolicy(const UBodySetup* Setup)
 {
  if(!Setup)return TEXT("NoBodySetup");
  switch(Setup->GetCollisionTraceFlag().GetValue())
  {
   case CTF_UseDefault:return TEXT("UseDefault");
   case CTF_UseSimpleAndComplex:return TEXT("SimpleAndComplex");
   case CTF_UseSimpleAsComplex:return TEXT("SimpleAsComplex");
   case CTF_UseComplexAsSimple:return TEXT("ComplexAsSimple");
   default:return TEXT("Unknown");
  }
 }
 void ProbeGuardHardware(UWorld* W)
 {
  FString Rows=TEXT("game_seconds,station,height_above_road_cm,trace_complex,query_channel,blocking_hit,actor,component,mesh,face_index,section,visual_material,physical_material,hit_x,hit_y,hit_z,normal_x,normal_y,normal_z,hit_lateral_cm,component_collision,body_collision,trace_policy,physics_state_created,body_valid,component_blocks_vehicle,body_blocks_vehicle,component_blocks_car_channel,body_blocks_car_channel,car_blocks_other_object,component_object_channel,car_object_channel,collision_profile,original_guard_mesh_hit\n");
  const ECollisionChannel CarChannel=Car->GetMesh()->GetCollisionObjectType();
  // Inventory nearby hardware independently of query results. A PhysicsOnly
  // body or a missing physics state must remain visible even if every ray
  // misses; a trace result alone cannot establish simulation collision.
  FString Inventory=TEXT("game_seconds,actor,component,mesh,component_collision,body_collision,trace_policy,physics_state_created,body_valid,component_blocks_vehicle,body_blocks_vehicle,component_blocks_car_channel,body_blocks_car_channel,car_blocks_other_object,component_object_channel,car_object_channel,collision_profile,bounds_min_x,bounds_min_y,bounds_min_z,bounds_max_x,bounds_max_y,bounds_max_z,scale_x,scale_y,scale_z\n");
  for(TActorIterator<AActor> It(W);It;++It)
  {
   TInlineComponentArray<UStaticMeshComponent*> Components(*It);
   for(auto* Component:Components)
   {
    const UStaticMesh* Static=Component->GetStaticMesh().Get();
    if(!IsHardwareMesh(Static) || Component->Bounds.GetBox().ComputeSquaredDistanceToPoint(Road[593])>FMath::Square(3000.))continue;
    const auto* Body=Component->GetBodyInstance();const auto Bounds=Component->Bounds.GetBox();const auto Scale=Component->GetComponentScale();++GuardBodyCount;
    Inventory+=FString::Printf(TEXT("%.6f,%s,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f\n"),W->GetTimeSeconds(),*GetNameSafe(*It),*Component->GetPathName(),*Static->GetPathName(),CollisionMode(Component->GetCollisionEnabled()),Body?CollisionMode(Body->GetCollisionEnabled()):TEXT("None"),TracePolicy(Static->GetBodySetup()),Component->IsPhysicsStateCreated()?1:0,Body && Body->IsValidBodyInstance()?1:0,
     Component->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block?1:0,Body && Body->GetResponseToChannel(ECC_Vehicle)==ECR_Block?1:0,Component->GetCollisionResponseToChannel(CarChannel)==ECR_Block?1:0,Body && Body->GetResponseToChannel(CarChannel)==ECR_Block?1:0,Car->GetMesh()->GetCollisionResponseToChannel(Component->GetCollisionObjectType())==ECR_Block?1:0,
     static_cast<int32>(Component->GetCollisionObjectType()),static_cast<int32>(CarChannel),*Component->GetCollisionProfileName().ToString(),Bounds.Min.X,Bounds.Min.Y,Bounds.Min.Z,Bounds.Max.X,Bounds.Max.Y,Bounds.Max.Z,Scale.X,Scale.Y,Scale.Z);
    if(Static->GetName().Contains(TEXT("AdventureGuard")))
    {
     ++SeparateGuardCount;const bool Policy=Component->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics && Body && Body->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics && Body->IsValidBodyInstance()
      && Component->GetCollisionObjectType()==ECC_Vehicle && Body->GetObjectType()==ECC_Vehicle && Component->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && Body->GetResponseToChannel(ECC_Pawn)==ECR_Block
      && Player->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block && Component->GetCollisionResponseToChannel(CarChannel)==ECR_Block && Body->GetResponseToChannel(CarChannel)==ECR_Block
      && Car->GetMesh()->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block && Car->GetChaosMovement()->WheelTraceCollisionResponses.GetResponse(ECC_Vehicle)==ECR_Ignore;
     if(!Policy)++SeparateGuardPolicyFailures;
    }
   }
  }
  Test->TestTrue(TEXT("Original nearby guard body inventory written"),FFileHelper::SaveStringToFile(Inventory,*(Directory/TEXT("guard-body-inventory.csv"))));
  for(int32 I=591;I<=595;++I)
  {
   const FVector Tangent=(Road[I+1]-Road[I-1]).GetSafeNormal2D(),Right(-Tangent.Y,Tangent.X,0);
   for(const float Height:{26.f,50.f,62.f,68.f,78.f,86.f,100.f})for(const bool Complex:{false,true})for(const ECollisionChannel Channel:{CarChannel,ECC_Visibility})
   {
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LandGuardPhysicalProbe),Complex,Car);Query.AddIgnoredActor(Player);Query.bReturnFaceIndex=true;Query.bReturnPhysicalMaterial=true;
    const FVector Start=Road[I]+Right*350+FVector(0,0,Height),End=Road[I]+Right*480+FVector(0,0,Height);FHitResult Hit;
    const bool Found=W->LineTraceSingleByChannel(Hit,Start,End,Channel,Query);++GuardProbeCount;
    auto* Component=Hit.GetComponent();auto* Mesh=Cast<UStaticMeshComponent>(Component);
    UStaticMesh* Static=Mesh?Mesh->GetStaticMesh().Get():nullptr;const UBodySetup* Setup=Static?Static->GetBodySetup():nullptr;
    const FBodyInstance* Body=Component?Component->GetBodyInstance():nullptr;
    int32 Section=INDEX_NONE;const auto* Material=Component?Component->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex,Section):nullptr;
    const float Lateral=Found?FVector::DotProduct(Hit.ImpactPoint-Road[I],Right):0.f;
    const bool Guard=Found && IsHardwareMesh(Static) && Lateral>365 && Lateral<470 && Height>8 && Height<130;
    if(Guard)++GuardQueryHits;
    Rows+=FString::Printf(TEXT("%.6f,%d,%.2f,%d,%d,%d,%s,%s,%s,%d,%d,%s,%s,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.4f,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d\n"),W->GetTimeSeconds(),I,Height,Complex?1:0,static_cast<int32>(Channel),Found?1:0,
     *GetNameSafe(Hit.GetActor()),Component?*Component->GetPathName():TEXT("None"),*GetNameSafe(Static),Hit.FaceIndex,Section,*GetNameSafe(Material),*GetNameSafe(Hit.PhysMaterial.Get()),
     Hit.ImpactPoint.X,Hit.ImpactPoint.Y,Hit.ImpactPoint.Z,Hit.ImpactNormal.X,Hit.ImpactNormal.Y,Hit.ImpactNormal.Z,Lateral,
     Component?CollisionMode(Component->GetCollisionEnabled()):TEXT("None"),Body?CollisionMode(Body->GetCollisionEnabled()):TEXT("None"),TracePolicy(Setup),
     Component && Component->IsPhysicsStateCreated()?1:0,Body && Body->IsValidBodyInstance()?1:0,
     Component && Component->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block?1:0,Body && Body->GetResponseToChannel(ECC_Vehicle)==ECR_Block?1:0,
     Component && Component->GetCollisionResponseToChannel(CarChannel)==ECR_Block?1:0,Body && Body->GetResponseToChannel(CarChannel)==ECR_Block?1:0,
     Component && Car->GetMesh()->GetCollisionResponseToChannel(Component->GetCollisionObjectType())==ECR_Block?1:0,
     Component?static_cast<int32>(Component->GetCollisionObjectType()):INDEX_NONE,static_cast<int32>(CarChannel),Component?*Component->GetCollisionProfileName().ToString():TEXT("None"),Guard?1:0);
   }
  }
  Test->TestTrue(TEXT("Original guard collision query evidence written"),FFileHelper::SaveStringToFile(Rows,*(Directory/TEXT("guard-probes.csv"))));
  ProbeQueryPolicy(W);
  Test->AddInfo(FString::Printf(TEXT("Original guard physical queries: %d traces, %d road-hardware hits; query hits do not count as native contacts. See face material and QueryAndPhysics / ComplexAsSimple / mutual channel responses in guard-probes.csv."),GuardProbeCount,GuardQueryHits));
 }
 void ProbeQueryPolicy(UWorld* W)
 {
  FString Rows=TEXT("game_seconds,station,height_cm,policy,channel,trace_complex,blocking_hit,component,mesh,object_channel,component_collision,body_collision,body_valid,guard_blocks_pawn,body_blocks_pawn,pawn_blocks_guard,guard_blocks_car,car_blocks_guard,actual_wheel_response_to_guard,independent_guard_hit\n");
  const auto& Responses=Car->GetChaosMovement()->WheelTraceCollisionResponses;
  for(int32 I=591;I<=595;++I)for(const float Height:{26.f,62.f,78.f})for(const bool Complex:{false,true})for(int32 Policy=0;Policy<2;++Policy)
  {
   const FVector Tangent=(Road[I+1]-Road[I-1]).GetSafeNormal2D(),Right(-Tangent.Y,Tangent.X,0);FHitResult Hit;
   FCollisionQueryParams Query(SCENE_QUERY_STAT(LandGuardQueryPolicy),Complex,Car);Query.AddIgnoredActor(Player);Query.bReturnPhysicalMaterial=true;Query.bReturnFaceIndex=true;
   const ECollisionChannel Channel=Policy?ECC_WorldDynamic:ECC_Pawn;const FCollisionResponseParams Response=Policy?FCollisionResponseParams(Responses):FCollisionResponseParams();
   const bool Found=W->LineTraceSingleByChannel(Hit,Road[I]+Right*350+FVector(0,0,Height),Road[I]+Right*480+FVector(0,0,Height),Channel,Query,Response);
   auto* Component=Hit.GetComponent();auto* StaticComponent=Cast<UStaticMeshComponent>(Component);const UStaticMesh* Static=StaticComponent?StaticComponent->GetStaticMesh().Get():nullptr;const auto* Body=Component?Component->GetBodyInstance():nullptr;
   const bool Separate=Found && Static && Static->GetName().Contains(TEXT("AdventureGuard"));if(Separate){if(Policy)++WheelGuardQueryHits;else ++PawnGuardQueryHits;}
   Rows+=FString::Printf(TEXT("%.6f,%d,%.2f,%s,%d,%d,%d,%s,%s,%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d\n"),W->GetTimeSeconds(),I,Height,Policy?TEXT("NativeWheelResponseContainer"):TEXT("PawnBodyBlock"),static_cast<int32>(Channel),Complex?1:0,Found?1:0,Component?*Component->GetPathName():TEXT("None"),*GetNameSafe(Static),Component?static_cast<int32>(Component->GetCollisionObjectType()):INDEX_NONE,
    Component?CollisionMode(Component->GetCollisionEnabled()):TEXT("None"),Body?CollisionMode(Body->GetCollisionEnabled()):TEXT("None"),Body && Body->IsValidBodyInstance()?1:0,
    Component && Component->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block?1:0,Body && Body->GetResponseToChannel(ECC_Pawn)==ECR_Block?1:0,
    Component && Player->GetCapsuleComponent()->GetCollisionResponseToChannel(Component->GetCollisionObjectType())==ECR_Block?1:0,
    Component && Component->GetCollisionResponseToChannel(Car->GetMesh()->GetCollisionObjectType())==ECR_Block?1:0,Component && Car->GetMesh()->GetCollisionResponseToChannel(Component->GetCollisionObjectType())==ECR_Block?1:0,
    Component?static_cast<int32>(Responses.GetResponse(Component->GetCollisionObjectType())):INDEX_NONE,Separate?1:0);
  }
  Test->TestTrue(TEXT("Actual wheel/Pawn guard query policy evidence written"),FFileHelper::SaveStringToFile(Rows,*(Directory/TEXT("guard-query-policy.csv"))));
 }
 FVector ParallelTarget(int32 Best,float Offset,float LookAhead) const
 {
  // Project the current position onto the forward surveyed segment, then
  // advance by distance. Grade remains the authored grade; all lateral
  // offsets use the local road normal, rather than station594's fixed normal.
  int32 SegmentIndex=FMath::Clamp(Best-1,585,608);const FVector Position=Car->GetActorLocation();
  double Closest=DBL_MAX;float Fraction=0;
  for(int32 I=FMath::Max(585,Best-1);I<=FMath::Min(608,Best+1);++I)
  {
   const FVector Segment=Road[I+1]-Road[I];
   const float F=FMath::Clamp(static_cast<float>(FVector::DotProduct((Position-Road[I])*FVector(1,1,0),Segment*FVector(1,1,0))/FMath::Max(1.,Segment.SizeSquared2D())),0.f,1.f);
   const double D=FVector::DistSquared2D(Position,FMath::Lerp(Road[I],Road[I+1],F));
   if(D<Closest){Closest=D;SegmentIndex=I;Fraction=F;}
  }
  while(SegmentIndex<609)
  {
   const float Length=static_cast<float>(FVector::Dist2D(Road[SegmentIndex],Road[SegmentIndex+1]));
   const float Available=(1-Fraction)*Length;
   if(LookAhead<=Available)
   {
    const float F=Fraction+LookAhead/FMath::Max(1.f,Length);const FVector Point=FMath::Lerp(Road[SegmentIndex],Road[SegmentIndex+1],F);
    const FVector A=(Road[SegmentIndex+1]-Road[SegmentIndex-1]).GetSafeNormal2D(),B=(Road[SegmentIndex+2]-Road[SegmentIndex]).GetSafeNormal2D(),Tangent=FMath::Lerp(A,B,F).GetSafeNormal2D();
    return Point+FVector(-Tangent.Y,Tangent.X,0)*Offset;
   }
   LookAhead-=Available;++SegmentIndex;Fraction=0;
  }
  const FVector Tangent=(Road[610]-Road[608]).GetSafeNormal2D();return Road[609]+FVector(-Tangent.Y,Tangent.X,0)*Offset;
 }
 void HandleContacts(const Chaos::FCollisionEventData& Data)
 {
  if(!Registered || !Scene || !Car || !World.IsValid())return;
  for(const auto& C:Data.CollisionData.AllCollisionsArray)
  {
   auto* A=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy1);auto* B=Scene->GetOwningComponent<UPrimitiveComponent>(C.Proxy2);
   if(A!=Car->GetMesh() && B!=Car->GetMesh())continue;auto* Other=A==Car->GetMesh()?B:A;if(!Other)continue;
   const FVector Position(C.Location),Normal(C.Normal),Impulse(C.AccumulatedImpulse);int32 Best=591;double Distance=DBL_MAX;
   for(int32 I=585;I<=610;++I){const double D=FVector::DistSquared2D(Position,Road[I]);if(D<Distance){Distance=D;Best=I;}}
   const FVector T=(Road[Best+1]-Road[Best-1]).GetSafeNormal2D(),N(-T.Y,T.X,0);
   // Project onto the road segment to avoid classifying longitudinal grade
   // difference as contact height. Floor and post / W-beam share one mesh.
   const int32 Prev=FMath::Max(0,Best-1),Next=FMath::Min(841,Best+1);
   const FVector Segment=Road[Next]-Road[Prev];const float Along=FMath::Clamp(FVector::DotProduct((Position-Road[Prev])*FVector(1,1,0),Segment*FVector(1,1,0))/FMath::Max(1.,Segment.SizeSquared2D()),0.,1.);
   const FVector OnRoad=FMath::Lerp(Road[Prev],Road[Next],Along);
   const float Lateral=FVector::DotProduct(Position-OnRoad,N),Height=Position.Z-OnRoad.Z;
   const auto* Static=Cast<UStaticMeshComponent>(Other);
   const bool RoadHardware=Static && Static->GetStaticMesh() && (Static->GetStaticMesh()->GetName().Contains(TEXT("AdventureRoad")) || Static->GetStaticMesh()->GetName().Contains(TEXT("Guard")))
    && Lateral>365 && Lateral<470 && Height>8 && Height<130 && Impulse.Size()>.1;
   ++AllCarContacts;if(RoadHardware)
   {
    ++GuardContacts;if(FirstGuardComponent.IsEmpty()){FirstGuardComponent=Other->GetPathName();FirstGuardActor=GetNameSafe(Other->GetOwner());FirstGuardLocation=Position;FirstGuardNormal=Normal;FirstGuardImpulse=Impulse;}
   }
   ContactRows+=FString::Printf(TEXT("%.6f,%.6f,%s,%s,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d\n"),World->GetTimeSeconds(),C.SolverTime,*GetNameSafe(Other->GetOwner()),*Other->GetName(),Position.X,Position.Y,Position.Z,Normal.X,Normal.Y,Normal.Z,Impulse.X,Impulse.Y,Impulse.Z,static_cast<double>(C.PenetrationDepth),Lateral,Height,RoadHardware?1:0);
  }
 }
 void SetInputs(float Drive,float Steering,float BrakeValue)
 {
  Throttle=Drive;Steer=Steering;BrakeInput=BrakeValue;auto* D=Car->GetDrivingComponent();D->SetDriveInput(Drive);D->SetSteerInput(Steering);D->SetBrakeInput(BrakeValue);
  auto* Movement=Car->GetChaosMovement();const auto& Output=Movement->PhysicsVehicleOutput();
  FString InputFile=Directory/TEXT("native-input.csv");
  if(!NativeInputInitialized)
  {
   NativeInputInitialized=true;
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,command_drive,command_steer,command_brake,raw_throttle,raw_steer,raw_brake,current_gear,target_gear,forward_speed_cm_s,output_available,physics_engine_rpm,physics_engine_torque,physics_transmission_torque,front_left_steering_degrees,front_right_steering_degrees\n"),*InputFile);
  }
  NativeInputRows+=FString::Printf(TEXT("%.6f,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%.5f,%d,%.5f,%.5f,%.5f,%.5f,%.5f\n"),
   World.IsValid()?World->GetTimeSeconds():0,Stage,Drive,Steering,BrakeValue,Movement->GetThrottleInput(),Movement->GetSteeringInput(),Movement->GetBrakeInput(),Movement->GetCurrentGear(),Movement->GetTargetGear(),Movement->GetForwardSpeed(),Output?1:0,
   Output?Output->EngineRPM:0,Output?Output->EngineTorque:0,Output?Output->TransmissionTorque:0,
   Output && Output->Wheels.IsValidIndex(0)?Output->Wheels[0].SteeringAngle:0,Output && Output->Wheels.IsValidIndex(1)?Output->Wheels[1].SteeringAngle:0);
 }
 void ForwardTarget(const FVector& Target,float DT,float Desired)
 {
  const float Speed=Car->GetChaosMovement()->GetForwardSpeed();const FVector Rear=Car->GetActorTransform().TransformPosition(FVector(RearAxleX,0,0)),Delta=Target-Rear;
  const float Alpha=FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Delta.Rotation().Yaw));
  const float Angle=FMath::RadiansToDegrees(FMath::Atan2(2.f*Wheelbase*FMath::Sin(Alpha),static_cast<float>(FMath::Max(150.,Delta.Size2D()))));
  const float Scale=FMath::Max(.05f,Car->GetChaosMovement()->SteeringSetup.SteeringCurve.GetRichCurveConst()->Eval(FMath::Abs(Speed)*.0223693629f));
  const float S=FMath::FInterpConstantTo(Steer,FMath::Clamp(Angle/(Car->GetVehicleProfile()->Handling*Scale),-1.f,1.f),DT,2.6f);
  const float Error=Desired-Speed;const float B=Error<-25?FMath::Clamp((-Error-25)/180.f,0.f,.85f):0;
  SetInputs(B>0?0:FMath::Clamp(Error/220.f,0.f,.55f),S,B);
 }
 void Brake(){if(Car)SetInputs(0,0,1);}
 void Flush()
 {
  if(Directory.IsEmpty())return;
  if(!Trajectory.IsEmpty()){FFileHelper::SaveStringToFile(Trajectory,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);Trajectory.Reset();}
  if(!ContactRows.IsEmpty()){FFileHelper::SaveStringToFile(ContactRows,*(Directory/TEXT("contacts.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);ContactRows.Reset();}
  if(!WheelRows.IsEmpty()){FFileHelper::SaveStringToFile(WheelRows,*(Directory/TEXT("wheel-state.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);WheelRows.Reset();}
  if(!ControllerRows.IsEmpty()){FFileHelper::SaveStringToFile(ControllerRows,*(Directory/TEXT("controller.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);ControllerRows.Reset();}
  if(!NativeInputRows.IsEmpty()){FFileHelper::SaveStringToFile(NativeInputRows,*(Directory/TEXT("native-input.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);NativeInputRows.Reset();}
 }
 void Unregister()
 {
  if(!Registered)return;Registered=false;
  if(World.IsValid() && World->GetPhysicsScene()==Scene && Scene->GetSolver())Scene->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision,this);
 }
 bool Finish(bool Passed,const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);Unregister();Flush();Metrics.Flush();if(Car)SetInputs(0,0,0);
  if(!Directory.IsEmpty())
  {
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetBoolField(TEXT("isolatedFixture"),true);J->SetBoolField(TEXT("canSatisfyDockContinuousGate"),false);
   J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("startStation"),591);J->SetNumberField(TEXT("contactStation"),ContactStation);J->SetNumberField(TEXT("lastStation"),Station);
   J->SetNumberField(TEXT("realChaosCarContacts"),AllCarContacts);J->SetNumberField(TEXT("realGuardHardwareContacts"),GuardContacts);J->SetStringField(TEXT("guardComponent"),FirstGuardComponent);J->SetStringField(TEXT("guardActor"),FirstGuardActor);
   J->SetStringField(TEXT("guardContactLocation"),FirstGuardLocation.ToString());J->SetStringField(TEXT("guardContactNormal"),FirstGuardNormal.ToString());J->SetStringField(TEXT("guardContactImpulse"),FirstGuardImpulse.ToString());
   J->SetNumberField(TEXT("maximumOuterLateralCm"),MaxLateral);J->SetNumberField(TEXT("maximumRollDegrees"),MaxRoll);J->SetNumberField(TEXT("reverseRecoveryTravelCm"),ReverseDistance);
   J->SetNumberField(TEXT("initialCondition"),InitialCondition);J->SetNumberField(TEXT("finalCondition"),Car?Car->GetDamageState()->GetConditionFraction():0);J->SetNumberField(TEXT("explicitPlayerSetupRelocations"),1);J->SetNumberField(TEXT("explicitCarSetupRelocations"),1);J->SetBoolField(TEXT("driverAndContactObserverReset"),true);J->SetBoolField(TEXT("continuousGpuReadback"),false);
   J->SetNumberField(TEXT("originalGuardQueryCount"),GuardProbeCount);J->SetNumberField(TEXT("originalGuardQueryHits"),GuardQueryHits);J->SetNumberField(TEXT("nearbyOriginalHardwareBodyCount"),GuardBodyCount);J->SetBoolField(TEXT("queryHitsCanSatisfyContactGate"),false);J->SetStringField(TEXT("approachController"),TEXT("Rolling parallel 450cm lookahead; original 170cm/s speed and 275-to-385cm lateral ramp"));
   J->SetStringField(TEXT("model"),Model());J->SetStringField(TEXT("automationTest"),bKestrel?TEXT("NeonCity.IslandLand.GuardrailContactKestrelPIE"):TEXT("NeonCity.IslandLand.GuardrailContactPIE"));J->SetNumberField(TEXT("rearAxleBodyXCm"),RearAxleX);J->SetNumberField(TEXT("productionWheelbaseCm"),Wheelbase);
   J->SetNumberField(TEXT("independentGuardCount"),SeparateGuardCount);J->SetNumberField(TEXT("independentGuardPolicyFailures"),SeparateGuardPolicyFailures);J->SetNumberField(TEXT("pawnGuardBlockingQueryHits"),PawnGuardQueryHits);J->SetNumberField(TEXT("nativeWheelContainerGuardQueryHits"),WheelGuardQueryHits);J->SetBoolField(TEXT("independentGuardPolicyPass"),SeparateGuardCount>0 && SeparateGuardPolicyFailures==0 && PawnGuardQueryHits>0 && WheelGuardQueryHits==0);
   J->SetNumberField(TEXT("raisedRoadEdgeWheelSupportSamples"),RaisedEdgeWheelSupportSamples);J->SetStringField(TEXT("raisedSupportDefinition"),TEXT("Actual bInContact and positive native SpringForce at the unchanged hardware lateral365..470cm / road-height8..130cm band; positions retained; not a native triangle-ID assertion"));
   TArray<TSharedPtr<FJsonValue>> Peaks;for(int32 I=0;I<WheelPeaks.Num();++I)
   {
    const auto& P=WheelPeaks[I];auto Peak=MakeShared<FJsonObject>();Peak->SetNumberField(TEXT("wheel"),I);Peak->SetNumberField(TEXT("maximumNativeSpringForce"),P.Force);Peak->SetNumberField(TEXT("maximumSpringForceHudN"),P.Force/100.f);Peak->SetNumberField(TEXT("minimumNativeSpringForce"),P.MinimumForce);Peak->SetNumberField(TEXT("gameSecondsAtMaximum"),P.Now);Peak->SetNumberField(TEXT("stageAtMaximum"),P.Stage);Peak->SetNumberField(TEXT("stationAtMaximum"),P.Station);Peak->SetStringField(TEXT("contactAtMaximum"),P.Contact.ToString());Peak->SetStringField(TEXT("sphereCentreAtMaximum"),P.Centre.ToString());Peak->SetNumberField(TEXT("suspensionAtMaximum"),P.Suspension);Peak->SetNumberField(TEXT("contactRoadHeightAtMaximumCm"),P.Height);Peak->SetNumberField(TEXT("contactRoadLateralAtMaximumCm"),P.Lateral);Peak->SetNumberField(TEXT("raisedSupportSamples"),P.RaisedSamples);Peaks.Add(MakeShared<FJsonValueObject>(Peak));
   }
   J->SetArrayField(TEXT("allFrameNativeWheelPeaks"),Peaks);
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));Test->TestTrue(TEXT("Native guard contact and recovery receipt written"),FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json"))));
  }
  return true;
 }
 struct FWheelPeakEvidence{float Force=-FLT_MAX,MinimumForce=FLT_MAX,Now=0,Suspension=0,Height=0,Lateral=0;int32 Stage=0,Station=0,RaisedSamples=0;FVector Contact=FVector::ZeroVector,Centre=FVector::ZeroVector;};
 FAutomationTestBase* Test;bool bKestrel=false;TWeakObjectPtr<UWorld> World;FPhysScene_Chaos* Scene=nullptr;AIslandSupercar* Car=nullptr;ANeonCityPlayerCharacter* Player=nullptr;TArray<FVector> Road;TArray<FWheelPeakEvidence> WheelPeaks;
 FNeonCityLandFrameMetrics Metrics;bool Registered=false,GuardProbed=false,GeometryRecorded=false,NativeInputInitialized=false;FString NativeInputRows;int32 Stage=0,Station=591,ContactStation=INDEX_NONE,GuardContacts=0,AllCarContacts=0,GuardProbeCount=0,GuardQueryHits=0,GuardBodyCount=0,SeparateGuardCount=0,SeparateGuardPolicyFailures=0,PawnGuardQueryHits=0,WheelGuardQueryHits=0,RaisedEdgeWheelSupportSamples=0;
 double Started=0;float Previous=0,At=0,ContactAt=0,LastSample=0,InitialCondition=0,Steer=0,Throttle=0,BrakeInput=0,SignedLateral=0,MaxLateral=0,MaxRoll=0,ReverseDistance=0;
 float TargetOffset=0,TargetForward=0,RearAxleX=0,Wheelbase=0;FString Directory,Trajectory,ContactRows,WheelRows,ControllerRows,FirstGuardComponent,FirstGuardActor;FVector LastTarget=FVector::ZeroVector,ContactPosition=FVector::ZeroVector,FirstGuardLocation=FVector::ZeroVector,FirstGuardNormal=FVector::ZeroVector,FirstGuardImpulse=FVector::ZeroVector;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandContinuousDriveFinal,"NeonCity.IslandLand.ContinuousDriveFinalPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandContinuousDriveFinal::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FContinuousDrive(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandVillageAsterApproach,"NeonCity.IslandLand.VillageAsterApproachPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandVillageAsterApproach::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FContinuousDrive(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandTerminalAsterApproach,"NeonCity.IslandLand.TerminalAsterApproachPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandTerminalAsterApproach::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FContinuousDrive(this,false,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandCurrentContinuous,"NeonCity.IslandLand.CurrentContinuousDrivePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandCurrentContinuous::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FContinuousDrive(this,false,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandCurrentCruiseEntry,"NeonCity.IslandLand.CurrentCruiseEntryMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandCurrentCruiseEntry::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FContinuousDrive(this,true,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandCurrentContinuousGame,"NeonCity.IslandLand.CurrentContinuousDriveStandalone",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandCurrentContinuousGame::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Standalone route requires isolated validation and disabled automatic persistence"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FContinuousDrive(this,false,false,true));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandKestrelUpperConnectors,"NeonCity.IslandLand.KestrelUpperConnectorsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandKestrelUpperConnectors::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FKestrelUpperConnectors(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandGuardrailContact,"NeonCity.IslandLand.GuardrailContactPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandGuardrailContact::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FGuardrailContact(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandGuardrailContactKestrel,"NeonCity.IslandLand.GuardrailContactKestrelPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandGuardrailContactKestrel::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityLandFinalDrive::FGuardrailContact(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
