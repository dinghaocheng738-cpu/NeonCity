#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityPhase205cVehicleTurnSurvey.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/IslandSupercar.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodySetup.h"
#include "Math/RotationMatrix.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeIslandDelivery
{
/** A creates an actual unpaid carried/transported checkpoint; B resumes it in
 * another process and delivers it; C independently loads the result and cancels
 * a fresh phone run. Only A has one supported player start. Cars, cargo identity,
 * activity state, ownership and money are always produced by normal gameplay. */
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* InTest,int32 InSession):Test(InTest),Session(InSession),Evidence(InSession==0?TEXT("DeliverySource"):InSession==1?TEXT("DeliveryResume"):TEXT("DeliveryCold")){}
 ~FFlow(){if(Driver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Driver);}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>230)return End(TEXT("Native delivery timed out"));
  if(!WalkFailure.IsEmpty())return End(WalkFailure);
  if(!PC || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(58000,-159000,300))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Delivery.Provisions"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job){if(It->Index==0)Pickup=*It;else if(It->Index==1)Clerk=*It;}
   Phone=PC->FindComponentByClass<UPhoneComponent>();Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>();auto* GI=W->GetGameInstance();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=GI->GetSubsystem<UNeonCityEconomySubsystem>();Ownership=GI->GetSubsystem<UVehicleOwnershipSubsystem>();
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityDeliveryRun="),Suffix);Suffix=FPaths::MakeValidFileName(Suffix.IsEmpty()?TEXT("01"):Suffix);
   MidSlot=TEXT("NeonCity_Test_DeliveryMid")+Suffix;DoneSlot=TEXT("NeonCity_Test_DeliveryDone")+Suffix;BridgeDir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("Delivery-")+Suffix);
   if(!P || !Job || !Pickup || !Clerk || !Phone || !Nav || !Save || !Wallet || !Ownership || !Save->ConfigureSlot(Session==2?DoneSlot:MidSlot,false))return End(TEXT("Authored delivery, ordinary player/phone/navigation/save domains missing"));
   if(Session>0){if(!Save->DoesSaveExist())return End(TEXT("Prior genuine delivery F6 required; no checkpoint was invented"));if(!ReadBridge(Session==1?TEXT("Source.json"):TEXT("Completed.json")))return End(TEXT("Independent prior source/completion manifest required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*(BridgeDir/TEXT("Source.json"))))return End(TEXT("Preserve prior genuine delivery source; use a fresh explicit run suffix after a code change"));
   const FVector Around=Pickup->GetActorLocation()+FVector(125,0,0);FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(DeliverySupportedStart),false,P);
   if(!W->LineTraceSingleByChannel(Floor,Around+FVector(0,0,650),Around-FVector(0,0,900),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return End(TEXT("Actual parcel pickup has no supported walking approach"));
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual parcel pickup standing clearance blocked"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);const auto* M=Save->ReadSaveForInspection();const auto D=Job->BuildOpportunitySnapshot(false);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || D.ActivityRunID!=Run || Wallet->GetBalance()!=ExpectedBalance || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>20)return End(TEXT("Independent genuine F7 lost exact saved run, wallet or safe normal player"));
   Before=Wallet->GetBalance();
   if(Session==2){if(D.State!=2 || D.ActivityCompletions!=1 || !Wallet->HasCredit(Job->GetReceiptID()) || Cargo())return End(TEXT("Independent completed delivery lost unique receipt/count or left cargo"));Key(EKeys::P,true);return Next(1);}
   if(D.State!=1 || D.ActivityStage!=1 || D.ActivityCheckpoint!=1 || D.ActivityItemID!=Item || Wallet->HasCredit(Job->GetReceiptID()) || !Cargo() || Cargo()->GetAttachParent()!=P->GetRootComponent())return End(TEXT("Independent unfinished delivery lost actual parcel identity/attachment or unpaid transport checkpoint"));
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->VehicleID==CarID)Car=*It;
   const auto* V=M->Vehicles.FindByPredicate([&](const auto& Value){return Value.VehicleID==CarID;});
   if(!Car || !V || !Ownership->IsOwned(CarID) || FVector::Dist(Car->GetActorLocation(),V->Transform.GetLocation())>35)return End(TEXT("Independent delivery lost original physical owned vehicle identity/position"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Delivery-Cold-Actual-Parcel.png"));return Next(8);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P phone input failed"));Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);Browsed=0;return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Actual delivery offer not discoverable"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run || D.ActivityStage!=0)return End(TEXT("Native ENTER failed to create a fresh unpaid delivery run"));
   if(Session==2){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Delivery replay duplicated its prior completion payment"));Key(EKeys::Delete,true);return Next(30);}
   Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==6 && Age>.2)
  {
   Key(EKeys::P,false);PC->SetControlRotation((Pickup->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Pickup)return End(TEXT("Actual parcel pickup F eye trace inaccessible"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Delivery-Native-Pickup-Prompt.png"));Key(EKeys::F,true);return Next(7);
  }
  if(Stage==7 && Age>.2)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);Item=D.ActivityItemID;
   if(D.ActivityStage!=1 || !Item.IsValid() || !Cargo() || Cargo()->GetAttachParent()!=P->GetRootComponent() || !Pickup->Visual->bHiddenInGame || Wallet->GetBalance()!=Before)return End(TEXT("Actual native pickup did not create one visible carried parcel without reward"));
   double Best=BIG_NUMBER;for(TActorIterator<AIslandSupercar> It(W);It;++It)if(Ownership->IsOwned(It->VehicleID) && It->GetVelocity().Size()<80){const double D2=FVector::DistSquared2D(P->GetActorLocation(),It->GetActorLocation());if(D2<Best){Best=D2;Car=*It;}}
   if(!Car)return End(TEXT("No original stopped owned stock car available; no car fixture was created"));CarID=Car->VehicleID;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Delivery-Native-Carried-Parcel.png"));return Next(8);
  }
  if(Stage==8)
  {
   FVector Door;if(!Phase205cVehicleApproach(W,P,Car,P->GetActorLocation(),Door))return End(TEXT("Existing car has no supported visible and collision-clear player approach"));
   if(!WalkTo(Door,50))return false;Key(EKeys::W,false);PC->SetControlRotation((Car->GetActorLocation()+FVector(0,0,45)-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Car)return End(TEXT("Original car native F entry prompt blocked after actual parcel walk"));Key(EKeys::F,true);return Next(9);
  }
  if(Stage==9 && Age>.25)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Car || !Cargo() || Cargo()->GetAttachParent()!=Car->GetRootComponent())return End(TEXT("Actual F board did not transfer the real carried parcel to its actual vehicle"));
   Nav->RefreshRoadGuide();if(!Nav->GetRoadGuide().FindRoute(Car->GetActorLocation(),Clerk->GetActorLocation(),Path) || Path.Num()<2)return End(TEXT("Actual shared navigation has no connected delivery path"));
   Initial=Car->GetActorLocation();Marker=Initial;LastProgress=W->GetTimeSeconds();PathNode=1;Turning=false;TurnReverse=false;TurnAllowed=false;NextTurnProbe=0;Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FFlow::DriveFrame);return Next(Session==0?10:22);
  }
  if(Stage==10 || Stage==22)
  {
   const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || D.ActivityRunID!=Run || D.ActivityItemID!=Item || !Cargo())return End(TEXT("Actual delivery lost its item/run during continuous physical transport"));
   if(FVector::Dist2D(Car->GetActorLocation(),Marker)>20){Marker=Car->GetActorLocation();LastProgress=W->GetTimeSeconds();}
   const bool AtStop=Session==0?(D.ActivityCheckpoint==1 && FVector::Dist2D(Car->GetActorLocation(),Pickup->GetActorLocation())>1800):FVector::Dist2D(Car->GetActorLocation(),Clerk->GetActorLocation())<800;
   if(AtStop){StopDrive();return Next(Session==0?11:23);}
   if(W->GetTimeSeconds()-LastProgress>12)return End(FString::Printf(TEXT("Actual delivery transport blocked: car%s path_node%d; no correction teleport was applied"),*Car->GetActorLocation().ToString(),PathNode));
  }
  if(Stage==11 && Age>.6 && Car->GetVelocity().Size()<25){if(Job->BuildOpportunitySnapshot(false).ActivityCheckpoint!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Actual transported delivery checkpoint missing or prematurely paid"));Key(EKeys::F6,true);return Next(12);}
  if(Stage==12 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();const auto* D=M?M->Phone.Opportunities.FindByPredicate([&](const auto& Value){return Value.OpportunityID==Job->ActivityID;}):nullptr;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !D || D->State!=1 || D->ActivityStage!=1 || D->ActivityCheckpoint!=1 || D->ActivityRunID!=Run || D->ActivityItemID!=Item || !M->Vehicles.ContainsByPredicate([&](const auto& V){return V.VehicleID==CarID && V.Owned;}))return End(TEXT("Genuine native F6 failed to retain unpaid real parcel/checkpoint/original owned car"));
   if(!WriteBridge(TEXT("Source.json")))return End(TEXT("Could not preserve genuine source manifest"));Key(EKeys::F,true);return Next(13);
  }
  if(Stage==13 && Age>.25){Key(EKeys::F,false);if(PC->GetPawn()!=P || !P->GetCharacterMovement()->IsMovingOnGround() || P->IsHidden() || !Cargo() || Cargo()->GetAttachParent()!=P->GetRootComponent())return End(TEXT("Actual F exit lost real player/cargo"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Delivery-Native-Mid-Transport-Saved.png"));return End(TEXT(""));}
  if(Stage==23 && Age>.6 && Car->GetVelocity().Size()<25){Key(EKeys::F,true);return Next(24);}
  if(Stage==24 && Age>.25){Key(EKeys::F,false);if(PC->GetPawn()!=P || !Cargo() || Cargo()->GetAttachParent()!=P->GetRootComponent())return End(TEXT("Actual delivery arrival F exit lost carried item"));return Next(25);}
  if(Stage==25)
  {
   if(!WalkTo(Clerk->GetActorLocation()+FVector(-150,-100,0),45))return false;Key(EKeys::W,false);PC->SetControlRotation((Clerk->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Clerk)return End(TEXT("Actual receiving-clerk F prompt inaccessible after physical arrival"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Delivery-Native-Clerk-Handoff.png"));Key(EKeys::F,true);return Next(26);
  }
  if(Stage==26 && Age>.2)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityCompletions!=1 || D.ActivityRunID!=Run || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()) || Cargo())return End(TEXT("Actual clerk handoff failed to remove one parcel and pay one shared receipt"));return Next(27);
  }
  if(Stage==27 && Age>.2){Key(EKeys::F,true);return Next(28);}
  if(Stage==28 && Age>.2)
  {
   Key(EKeys::F,false);if(Wallet->GetBalance()!=Before+Job->Reward || Job->BuildOpportunitySnapshot(false).ActivityCompletions!=1 || Cargo())return End(TEXT("Separate native repeated F duplicated parcel or reward"));
   if(!Save->ConfigureSlot(DoneSlot,false) || Save->DoesSaveExist())return End(TEXT("Preserve prior genuine delivery completion checkpoint"));Key(EKeys::F6,true);return Next(29);
  }
  if(Stage==29 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityCompletions==1 && D.ActivityRunID==Run;}))return End(TEXT("Genuine completed delivery F6 lost settled run"));
   if(!WriteBridge(TEXT("Completed.json")))return End(TEXT("Could not preserve genuine completion manifest"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Delivery-Native-Completed-Saved.png"));return End(TEXT(""));
  }
  if(Stage==30 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Cargo() || Wallet->GetBalance()!=Before)return End(TEXT("Native replay cancellation left cargo or duplicated payment"));return End(TEXT(""));}
  return false;
 }
private:
 UStaticMeshComponent* Cargo() const
 {
  if(!Job)return nullptr;TArray<UStaticMeshComponent*> Components;Job->GetComponents(Components);for(auto* C:Components)if(C && C->IsRegistered() && C->GetFName()==TEXT("CarriedIslandParcel"))return C;return nullptr;
 }
 bool WalkTo(FVector Goal,float Radius)
 {
  if(FVector::Dist2D(P->GetActorLocation(),Goal)<Radius){Key(EKeys::W,false);return true;}
  if(WalkPath.IsEmpty() || FVector::Dist2D(WalkGoal,Goal)>120){WalkGoal=Goal;WalkNode=0;if(!Phase205cSupportedWalkPath(W,P,Goal,WalkPath)){WalkFailure=TEXT("No actual supported collision-clear walking path; no wall/collision bypass");return false;}}
  while(WalkNode<WalkPath.Num()-1 && FVector::Dist2D(P->GetActorLocation(),WalkPath[WalkNode])<40)++WalkNode;
  const FVector Aim=WalkNode==WalkPath.Num()-1?Goal:WalkPath[WalkNode];PC->SetControlRotation(FRotator(0,(Aim-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);
  if(W->GetTimeSeconds()-At>35){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(DeliveryWalkBlocker),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+P->GetActorForwardVector()*120,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);WalkFailure=FString::Printf(TEXT("Actual held-W delivery walk blocked by %s/%s at%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*P->GetActorLocation().ToString());return false;}
  return false;
 }
 FVector LookAhead()
 {
  while(PathNode<Path.Num()-1 && FVector::Dist2D(Car->GetActorLocation(),Path[PathNode])<170)++PathNode;
  const FVector Current=Car->GetActorLocation();const FVector To=Path[PathNode]-Current;
  // A253 is retained: its already-passed first transport input is unchanged.
  // This fix targets only the remaining independent-process return leg B.
  if(Session==0){if(To.Size2D()>600 || PathNode==Path.Num()-1)return Path[PathNode];return Path[PathNode]+(Path[PathNode+1]-Path[PathNode]).GetSafeNormal2D()*(600-To.Size2D());}
  const FVector Local=Car->GetActorTransform().InverseTransformVectorNoScale(To);
  if(Turning || Local.X<=0 || FMath::Abs(FMath::Atan2(Local.Y,Local.X))>=FMath::DegreesToRadians(65.f) || To.Size2D()>600 || PathNode==Path.Num()-1)return Path[PathNode];
  // Native measured village knots are only 150-212cm apart. Never extend a
  // short next segment hundreds of centimetres past its physical corner.
  const FVector Next=Path[PathNode+1]-Path[PathNode];
  const float Ahead=FMath::Min(600.f-float(To.Size2D()),float(Next.Size2D()));
  return Path[PathNode]+Next.GetSafeNormal2D()*Ahead;
 }
 TSharedRef<FJsonObject> CollisionRecord(UPrimitiveComponent* Component) const
 {
  auto J=MakeShared<FJsonObject>();if(!Component)return J;
  J->SetStringField(TEXT("component"),Component->GetPathName());J->SetStringField(TEXT("actor"),GetPathNameSafe(Component->GetOwner()));
  J->SetStringField(TEXT("transform"),Component->GetComponentTransform().ToString());J->SetStringField(TEXT("profile"),Component->GetCollisionProfileName().ToString());
  J->SetNumberField(TEXT("collision_enabled"),int32(Component->GetCollisionEnabled()));J->SetNumberField(TEXT("vehicle_response"),int32(Component->GetCollisionResponseToChannel(ECC_Vehicle)));
  J->SetStringField(TEXT("bounds_origin"),Component->Bounds.Origin.ToString());J->SetStringField(TEXT("bounds_extent"),Component->Bounds.BoxExtent.ToString());
  if(const auto* Static=Cast<UStaticMeshComponent>(Component))J->SetStringField(TEXT("static_mesh"),GetPathNameSafe(Static->GetStaticMesh()));
  if(const auto* Skeletal=Cast<USkeletalMeshComponent>(Component))J->SetStringField(TEXT("physics_asset"),GetPathNameSafe(Skeletal->GetPhysicsAsset()));
  if(const auto* Body=Component->GetBodySetup())
  {
   J->SetNumberField(TEXT("collision_trace_flag"),int32(Body->CollisionTraceFlag));J->SetNumberField(TEXT("simple_shape_count"),Body->AggGeom.GetElementCount());
   TArray<TSharedPtr<FJsonValue>> Boxes;for(const auto& Box:Body->AggGeom.BoxElems){auto B=MakeShared<FJsonObject>();B->SetStringField(TEXT("center"),Box.Center.ToString());B->SetStringField(TEXT("rotation"),Box.Rotation.ToString());B->SetStringField(TEXT("full_dimensions"),FVector(Box.X,Box.Y,Box.Z).ToString());Boxes.Add(MakeShared<FJsonValueObject>(B));}J->SetArrayField(TEXT("simple_boxes"),Boxes);
  }
  return J;
 }
 bool TurnClear(bool Reverse,float Steer,FString& Failure)
 {
  // A confined manoeuvre uses a50cm leg plus50cm surveyed braking space.
  // Every live shape and25cm footprint/support/water gate remains unchanged.
  return Phase205cNativeVehicleTurnSurvey(W,Car,Car->GetMesh(),Reverse,Steer,Failure,100.f);
 }
 void DriveFrame(UWorld* World,ELevelTick,float)
 {
  if(World!=W || !Car || !PC || PC->GetPawn()!=Car || (Stage!=10 && Stage!=22))return;
  // The functional driver uses the established gameplay input ports, consumed
  // by real Chaos each frame. No pose/velocity/tire-force or collision writes.
  const FVector Target=LookAhead();const FVector Local=Car->GetActorTransform().InverseTransformVectorNoScale(Target-Car->GetActorLocation());const float Speed=FVector::DotProduct(Car->GetVelocity(),Car->GetActorForwardVector());
  const float Angle=FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)));const float Distance=FVector::Dist2D(Car->GetActorLocation(),Clerk->GetActorLocation());
  float Desired=Angle>20?250.f:400.f;if(Session==1 && Distance<1300)Desired=FMath::Min(Desired,250.f);
  float Steer=FMath::Clamp(float(FMath::RadiansToDegrees(FMath::Atan2(560*Local.Y,FMath::Max(1.,Local.SizeSquared2D())))/32),-1.f,1.f),Drive=0,Brake=0;FString Mode=TEXT("forward"),Clearance;
  if(Session==0){auto* Input=Car->GetDrivingComponent();Input->SetSteerInput(Steer);Input->SetDriveInput(FMath::Clamp((Desired-Speed)/180.f,0.f,1.f));Input->SetBrakeInput(Speed>Desired+30?.4f:0.f);return;}
  if(!Turning && Angle>70){Turning=true;TurnSide=FMath::Abs(Local.Y)>10?FMath::Sign(Local.Y):1.f;TurnReverse=false;TurnLegOrigin=Car->GetActorLocation();NextTurnProbe=0;}
  if(Turning && Angle<55 && Local.X>100){Turning=false;TurnReverse=false;}
  if(Turning)
  {
   Mode=TurnReverse?TEXT("turn_reverse"):TEXT("turn_forward");Desired=75;const float Now=W->GetTimeSeconds();
   // The active native waypoint can advance during a confined turn. Do not
   // retain the previous knot's opposite turn sign and steer off the road.
   if(FMath::Abs(Local.Y)>80 && FMath::Sign(Local.Y)!=TurnSide)
   {if(FMath::Abs(Speed)>25){Brake=1;Mode=TEXT("waypoint_side_braking");}else{TurnSide=FMath::Sign(Local.Y);TurnAllowed=false;NextTurnProbe=0;TurnLegOrigin=Car->GetActorLocation();}}
   if(Now>=NextTurnProbe)
   {
    NextTurnProbe=Now+.2f;FString Why;const bool CurrentClear=TurnClear(TurnReverse,TurnReverse?-TurnSide:TurnSide,Why);
    if(!CurrentClear || FVector::Dist2D(Car->GetActorLocation(),TurnLegOrigin)>50)
    {
     FString OtherWhy;if(FMath::Abs(Speed)>25){Brake=1;Mode=TEXT("turn_leg_braking");NextTurnProbe=Now;}
     else if(TurnClear(!TurnReverse,TurnReverse?TurnSide:-TurnSide,OtherWhy)){TurnReverse=!TurnReverse;TurnLegOrigin=Car->GetActorLocation();TurnAllowed=true;}
     else if(CurrentClear){TurnLegOrigin=Car->GetActorLocation();TurnAllowed=true;Clearance=TEXT("retained clear current leg; alternate blocked: ")+OtherWhy;}
     else {TurnAllowed=false;Clearance=Why+TEXT(" | ")+OtherWhy;}
    }
    else TurnAllowed=true;
   }
   Steer=TurnReverse?-TurnSide:TurnSide;if(!TurnAllowed){Brake=1;Mode=TEXT("turn_clearance_stop");}
   if(Brake==0){Drive=TurnReverse?-FMath::Clamp((Desired+Speed)/100.f,0.f,1.f):FMath::Clamp((Desired-Speed)/100.f,0.f,1.f);Brake=(TurnReverse?-Speed:Speed)>Desired+20?.5f:0.f;}
  }
  else {Drive=FMath::Clamp((Desired-Speed)/180.f,0.f,1.f);Brake=Speed>Desired+30?.4f:0.f;}
  if(Brake>0)Drive=0;auto* Input=Car->GetDrivingComponent();Input->SetSteerInput(Steer);Input->SetDriveInput(Drive);Input->SetBrakeInput(Brake);
  if(W->GetTimeSeconds()-LastDriverRecord>.1f){LastDriverRecord=W->GetTimeSeconds();DriverEvidence+=FString::Printf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%.3f,%.3f,%.3f,"),LastDriverRecord,Stage,PathNode,Car->GetActorLocation().X,Car->GetActorLocation().Y,Car->GetActorLocation().Z,Local.X,Local.Y,Speed,*Mode,Steer,Drive,Brake)+FString(TEXT("\""))+Clearance.Replace(TEXT("\""),TEXT("\"\""))+TEXT("\"\n");}
 }
 void StopDrive(){if(Car){auto* Input=Car->GetDrivingComponent();Input->SetDriveInput(0);Input->SetSteerInput(0);Input->SetBrakeInput(1);}}
 bool Next(int32 Value){Stage=Value;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool ReadBridge(const TCHAR* Name)
 {
  FString Text;TSharedPtr<FJsonObject> J;if(!FFileHelper::LoadFileToString(Text,*(BridgeDir/Name)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid())return false;
  double PID=0;FString R,I,V;if(!J->TryGetNumberField(TEXT("process_id"),PID) || uint32(PID)==FPlatformProcess::GetCurrentProcessId() || !J->TryGetStringField(TEXT("run_id"),R) || !J->TryGetStringField(TEXT("item_id"),I) || !J->TryGetStringField(TEXT("car_id"),V) || !J->TryGetNumberField(TEXT("balance"),ExpectedBalance))return false;
  return FGuid::Parse(R,Run) && FGuid::Parse(I,Item) && FGuid::Parse(V,CarID);
 }
 bool WriteBridge(const TCHAR* Name)
 {
  IFileManager::Get().MakeDirectory(*BridgeDir,true);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("item_id"),Item.ToString());J->SetStringField(TEXT("car_id"),CarID.ToString());J->SetNumberField(TEXT("balance"),double(Wallet->GetBalance()));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*(BridgeDir/Name));
 }
 bool End(FString Error)
 {
  if(Error==TEXT("Native delivery timed out") && !WalkFailure.IsEmpty())Error+=TEXT(": ")+WalkFailure;
  if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}StopDrive();if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("session"),Session);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetNumberField(TEXT("stage"),Stage);J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("item_id"),Item.ToString());J->SetStringField(TEXT("car_id"),CarID.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetNumberField(TEXT("post_setup_pose_writes"),0);
  J->SetStringField(TEXT("scope"),Session==0?TEXT("One physically supported player start at the actual pickup. Native P/O/RIGHT/ENTER/F creates and carries the real crate, held W walks to an existing owned stock car, F boards it. Per-frame existing throttle/steer/brake ports drive actual Chaos on the actual shared navigation path, over15m from pickup; stop, genuine unpaid F6 and normal F exit. No car pose/velocity, cargo/run, reward, completion or ownership fixtures."):Session==1?TEXT("Independent process verified against actual source manifest. Genuine F7 restores unpaid original item/run/car/owned identity and safe saved player; real W/F reboard and existing gameplay input ports continuously drive the actual navigation route. Real F exit/heldW/clerkF handoff removes cargo and pays exactly once; separate repeatedF cannot duplicate; genuine completedF6. Zero pose/state/reward fixtures."):TEXT("Independent process genuine F7 completed delivery, one shared receipt and completion, no orphan cargo. Actual phone replay creates a new run, DELETE cancels it without new payment or cargo. Zero pose/state/reward fixtures. Subjective parcel animation, whole-village presentation and handling remain separate."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(Session==0?TEXT("DeliverySource"):Session==1?TEXT("DeliveryResume"):TEXT("DeliveryCold"));IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));
  FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,node,x,y,z,goal_local_x,goal_local_y,signed_speed,driver_mode,steer,drive,brake,turn_clearance\n")+DriverEvidence,*(Dir/TEXT("physical-driver.csv")));
  auto Audit=MakeShared<FJsonObject>();Audit->SetStringField(TEXT("scope"),TEXT("Read-only exact live PhysicsAsset box/sphere/capsule shapes, measured rigid-body transforms/ride height/suspension attitude. Confined50cm actual turn leg with100cm25cm-step stopping-space forecast. Full245x125cm footprint support, normalZ>=.94, height variation<=65, actual water margin, every blocking overlap/sweep retained. No physical body/road/car state is written. A source branch unchanged."));Audit->SetArrayField(TEXT("rejected_predictions"),TurnAudits);FString AuditText;FJsonSerializer::Serialize(Audit,TJsonWriterFactory<>::Create(&AuditText));FFileHelper::SaveStringToFile(AuditText,*(Dir/TEXT("physical-turn-diagnostics.json")));
  FString Nodes=TEXT("node,x,y,z\n");for(int32 I=0;I<Path.Num();++I)Nodes+=FString::Printf(TEXT("%d,%.3f,%.3f,%.3f\n"),I,Path[I].X,Path[I].Y,Path[I].Z);FFileHelper::SaveStringToFile(Nodes,*(Dir/TEXT("actual-navigation-path.csv")));return true;
 }
 FAutomationTestBase* Test;int32 Session;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Pickup=nullptr;AIslandActivityPoint* Clerk=nullptr;AIslandSupercar* Car=nullptr;UPhoneComponent* Phone=nullptr;UNeonCityNavigationComponent* Nav=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;UVehicleOwnershipSubsystem* Ownership=nullptr;
 TMap<FKey,bool> Keys;TArray<FVector> Path,WalkPath;int32 Stage=0,Browsed=0,PathNode=1,WalkNode=0;float At=0,LastProgress=0;double Started=0,ExpectedBalance=0;int64 Before=0;FGuid Run,Item,CarID;FVector Initial=FVector::ZeroVector,Marker=FVector::ZeroVector,WalkGoal;FString MidSlot,DoneSlot,BridgeDir,WalkFailure;FDelegateHandle Driver;FNeonCityVisualEvidence Evidence;
 bool Turning=false,TurnReverse=false,TurnAllowed=false;float TurnSide=1,NextTurnProbe=0,LastDriverRecord=0;FVector TurnLegOrigin=FVector::ZeroVector;FString DriverEvidence;
 TArray<TSharedPtr<FJsonValue>> TurnAudits;TSet<FString> TurnAuditKeys;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandDeliverySource,"NeonCity.Phase205c.Activity.Delivery.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandDeliverySource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandDelivery::FFlow(this,0));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandDeliveryResume,"NeonCity.Phase205c.Activity.Delivery.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandDeliveryResume::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandDelivery::FFlow(this,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandDeliveryResultCold,"NeonCity.Phase205c.Activity.Delivery.C",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandDeliveryResultCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandDelivery::FFlow(this,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
