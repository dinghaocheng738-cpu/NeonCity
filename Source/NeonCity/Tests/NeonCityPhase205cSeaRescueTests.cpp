#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandActivityJob.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeIslandSeaRescue
{
/** These content checks never replay the cached generic boat loop. A sails
 * only to the real stranded visitor and creates an unpaid F6 checkpoint. B is
 * a new process, restores it, returns to the authored dock and delivers the
 * actual guest. C independently loads the settled result and cancels replay.
 * Boat/NPC poses, velocity, reward, inventory and activity state are never set. */
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,int32 S):Test(T),Session(S),Evidence(S==0?TEXT("SeaRescueSource"):S==1?TEXT("SeaRescueResume"):TEXT("SeaRescueCold")){}
 ~FFlow(){if(Driver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Driver);}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>1000)return End(TEXT("Native sea rescue exceeded its bounded timeout"));if(!WalkFailure.IsEmpty())return End(WalkFailure);
  if(!PC || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(56000,-174000,0))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());for(TActorIterator<ANeonCityFishingBoat> It(W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Boat=*It;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Rescue.Buoy"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job){if(It->Index==0)Pickup=*It;else if(It->Index==1)Clerk=*It;}
   auto* GI=W->GetGameInstance();Phone=PC->FindComponentByClass<UPhoneComponent>();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=GI->GetSubsystem<UNeonCityEconomySubsystem>();
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCitySeaRescueRun="),Suffix);if(Suffix.IsEmpty())Suffix=TEXT("01");
   if(Suffix.Len()>20)return End(TEXT("Sea rescue run suffix exceeds20 characters"));for(const TCHAR C:Suffix)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))return End(TEXT("Sea rescue run suffix must use letters/digits/underscore/hyphen"));
   MidSlot=TEXT("NeonCity_Test_SeaRescueMid_")+Suffix;DoneSlot=TEXT("NeonCity_Test_SeaRescueDone_")+Suffix;Bridge=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("SeaRescue-")+Suffix);
   if(!P || !Boat || !Job || !Pickup || !Clerk || !Phone || !Save || !Wallet || !Boat->Owned)return End(TEXT("Actual Harborline/stranded visitor activity/clerk/phone/save domains unavailable"));
   Origin=Coast->GetActorLocation();Berth=Coast->GetFishingBerth();Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FFlow::DriveFrame);
   ReturnSlot=TEXT("NeonCity_Test_SeaRescueReturned_")+Suffix;
   if(Session>0)
   {
    bResumeReturned=Session==1 && IFileManager::Get().FileExists(*(Bridge/TEXT("Returned.json")));
    const FString Checkpoint=Session==1?(bResumeReturned?ReturnSlot:MidSlot):DoneSlot;
    if(!Save->ConfigureSlot(Checkpoint,false) || !Save->DoesSaveExist() || !ReadBridge(Session==1?(bResumeReturned?TEXT("Returned.json"):TEXT("Source.json")):TEXT("Completed.json")))return End(TEXT("Independent genuine prior rescue checkpoint required"));
    Key(EKeys::F7,true);return Next(20);
   }
   if(!Save->ConfigureSlot(MidSlot,false) || Save->DoesSaveExist() || IFileManager::Get().FileExists(*(Bridge/TEXT("Source.json"))))return End(TEXT("Preserve genuine rescue source; choose a fresh explicit suffix after a code change"));
   InputSlot=TEXT("NeonCity_Test_LandFinal_Legacy15");FParse::Value(FCommandLine::Get(),TEXT("NeonCitySeaRescueInput="),InputSlot);
   if(!Save->ConfigureSlot(InputSlot,false) || !Save->DoesSaveExist())return End(TEXT("Retained genuine dock input save required; no save fixture was invented"));Key(EKeys::F7,true);return Next(40);
  }
  Observe();
  if(Boat && PC->GetPawn()==Boat && (FMath::Abs(Boat->GetActorRotation().Roll)>35 || FMath::Abs(Boat->GetActorRotation().Pitch)>30))return End(FString::Printf(TEXT("Actual rescue hull attitude exceeded the retained safe boat envelope: pose%s rotation%s last_near_hull%s"),*Boat->GetActorLocation().ToString(),*Boat->GetActorRotation().ToString(),*LastNearHull));
  if(Stage==40 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Boat->GetMarineMovement()->IsMoored() || FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())>150)return End(TEXT("Genuine input F7 did not retain original moored Harborline berth"));
   if(!Save->ConfigureSlot(MidSlot,false))return End(TEXT("Dedicated rescue output slot unavailable after genuine input load"));
   // The one declared player-only start makes this a bounded content check.
   // It does not restage the original boat, NPC or certify Dock-route continuity.
   const FVector Near=Origin+FVector(5500,-3510,330);FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(SeaRescueSupportedDockStart),false,P);
   if(!W->LineTraceSingleByChannel(Floor,Near+FVector(0,0,350),Near-FVector(0,0,500),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return End(TEXT("Actual dock content start lacks static walking support"));const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual dock content start has blocked standing clearance"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();InitialCondition=Boat->GetHealthComponent()->GetCurrentHealth();PhoneGoal=1;Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();const auto D=Job->BuildOpportunitySnapshot(false);Before=Wallet->GetBalance();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || D.ActivityRunID!=Run || Before!=ExpectedBalance || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>30)return End(TEXT("Independent rescue F7 lost original run/wallet or used a remote player fallback instead of the genuine saved boat/deck position"));
   if(Session==2){if(D.State!=2 || D.ActivityCompletions!=1 || Job->GetActivityNPC() || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Independent settled rescue lost one receipt/count or left an orphan guest"));PhoneGoal=3;Key(EKeys::P,true);return Next(1);}
   const auto* B=M->Marine.FindByPredicate([&](const auto& V){return V.VehicleID==Boat->VehicleID;});
   if(!B || !B->Owned || FVector::Dist2D(Boat->GetActorLocation(),B->Transform.GetLocation())>35 || D.State!=1 || D.ActivityStage!=2 || D.VehicleID!=Boat->VehicleID || !D.ActivityTransportPickupValid || FVector::Dist(D.ActivityTransportPickup,BoardingOrigin)>5 || D.ActivityPassengerExiting || !Seated() || Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Independent rescue F7 reset pickup or lost actual original boat/seated passenger/boarding origin"));
   InitialCondition=Boat->GetHealthComponent()->GetCurrentHealth();FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SeaRescue-Cold-Actual-Passenger.png"));
   if(bResumeReturned)
   {
    if(CachedReturnTravel<18000 || !Based() || !Boat->GetMarineMovement()->IsMoored() || Boat->GetMarineMovement()->IsEngineRunning() || FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())>150)return End(TEXT("Genuine returned rescue checkpoint lost native berth/deck safety or prior continuous-return evidence"));
    Test->AddInfo(FString::Printf(TEXT("SEA_RESCUE_CACHED_GENUINE_RETURN distance_cm=%.3f; this process tests departure/handoff only"),CachedReturnTravel));return Next(28);
   }
   return Next(11);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P phone input failed"));if(Phone->GetActiveApp()==ENeonCityPhoneApp::Opportunities){Browsed=0;return Next(3);}Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);Browsed=0;return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Actual sea rescue offer/action not discoverable in P/O"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);
   if(PhoneGoal==2){if(D.State!=1 || D.ActivityStage<1 || !Job->GetActivityNPC())return End(TEXT("Native phone ENTER refused actual stopped alongside rescue pickup"));Key(EKeys::P,true);return Next(16);}
   if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run || !Job->GetActivityNPC())return End(TEXT("Native phone acceptance did not create a real unpaid visitor run"));
   if(PhoneGoal==3){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Rescue replay duplicated settled reward"));Key(EKeys::Delete,true);return Next(37);}
   Run=D.ActivityRunID;Key(EKeys::P,true);PierNode=0;
   // The new harbour job panels are real BlockAll geometry at the old
   // (5780,-3750) boat-test waypoint. Survey the unchanged static dock instead
   // of driving a standing capsule through those panels or widening arrival.
   if(!Phase205cSupportedWalkPath(W,P,Origin+FVector(6110,-3510,310),PierPath))return End(TEXT("Actual dock floor/capsule survey found no clear route around harbour activity panels"));
   PierPath.Add(Origin+FVector(6250,-3510,-180));
   Test->AddInfo(FString::Printf(TEXT("SEA_RESCUE_NATIVE_DOCK_PATH nodes=%d static_floor_and_capsule_survey=true final_original_water_entry=true"),PierPath.Num()));return Next(6);
  }
  if(Stage==6)
  {
   if(Age>.2)Key(EKeys::P,false);
   if(!MovePlayer(PierPath[PierNode],15,.6f)){if(Age>45)return End(FString::Printf(TEXT("Actual surveyed dock walk/swim approach blocked: node%d player%s target%s floor%s"),PierNode,*P->GetActorLocation().ToString(),*PierPath[PierNode].ToString(),*GetNameSafe(P->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent())));return false;}if(++PierNode<PierPath.Num())return false;return Next(7);
  }
  if(Stage==7)
  {
   if(!MovePlayer(Boat->GetActorTransform().TransformPosition(FVector(-145,-195,25)),15,.5f)){if(Age>25)return End(TEXT("Actual swimming approach to gunwale blocked"));return false;}return Next(8);
  }
  if(Stage==8)
  {
   if(P->GetActorLocation().Z<Boat->GetActorLocation().Z-65){Key(EKeys::SpaceBar,true);if(Age>25)return End(TEXT("Normal swimmer could not surface beside the gunwale"));return false;}Key(EKeys::SpaceBar,false);Focus(Boat->GetActorTransform().TransformPosition(FVector(-145,-130,25)));
   if(Age<3)return false;if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Boat){if(Age>15)return End(TEXT("Normal gunwale F prompt blocked"));return false;}Key(EKeys::F,true);return Next(9);
  }
  if(Stage==9 && Age>.7){Key(EKeys::F,false);if(PC->GetPawn()!=P || !Based())return End(TEXT("Normal F did not board the actual moving deck"));return Next(11);}
  if(Stage==11)
  {
   if(!MovePlayer(Boat->GetActorTransform().TransformPosition(FVector(-75,0,35)),12,.25f)){if(Age>20)return End(TEXT("Normal actual cabin route to helm blocked"));return false;}Focus(Boat->GetActorTransform().TransformPosition(FVector(110,55,120)));
   if(Age<.5)return false;if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Boat){if(Age>20)return End(TEXT("Normal actual helm F prompt blocked"));return false;}Key(EKeys::F,true);return Next(12);
  }
  if(Stage==12 && Age>.4)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Boat || !Boat->GetMarineMovement()->IsEngineRunning() || Boat->GetMarineMovement()->IsMoored())return End(TEXT("Normal F helm entry/engine/mooring release failed"));
   // Keep the approaching bow clear of the stranded capsule and the real
   // pickup panel. This remains inside the native450cm stopped-boat action;
   // the visitor then swims normally to the existing145cm side boarding door.
   if(Session==0){const FVector F=Berth.GetRotation().GetForwardVector(),R=Berth.GetRotation().GetRightVector();Station=Pickup->GetActorLocation()-R*350-F*40;StationYaw=Berth.Rotator().Yaw;Path={Boat->GetActorLocation(),Berth.GetLocation()+F*8000,Berth.GetLocation()+F*17000+R*3000,Station-F*1000,Station};PathNode=1;return Next(14);}
   BuildReturn();return Next(25);
  }
  if(Stage==14)
  {
   if(FVector::Dist2D(Boat->GetActorLocation(),Station)<1400)return Next(15);if(Age>240)return End(TEXT("Actual rescue-only outbound voyage failed to reach buoy approach"));
  }
  if(Stage==15)
  {
   if(FVector::Dist2D(Boat->GetActorLocation(),Station)<250 && FVector::Dist(Boat->GetActorLocation(),Pickup->GetActorLocation())<420 && Boat->GetVelocity().Size()<35 && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,StationYaw))<25){PhoneGoal=2;Key(EKeys::P,true);return Next(1);}
   if(Age>90)return End(FString::Printf(TEXT("Actual alongside rescue stop failed: boat%s goal%s speed%.2f"),*Boat->GetActorLocation().ToString(),*Station.ToString(),Boat->GetVelocity().Size()));
  }
  if(Stage==16)
  {
   if(Age>.2)Key(EKeys::P,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1)return End(TEXT("Real visitor boarding failed or cancelled"));
   if(D.ActivityStage==2){if(!Seated() || D.VehicleID!=Boat->VehicleID || !D.ActivityTransportPickupValid || Wallet->GetBalance()!=Before)return End(TEXT("Real visible rescue passenger/actual boat identity/unpaid stage missing"));BoardingOrigin=D.ActivityTransportPickup;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SeaRescue-Native-Actual-Aboard.png"));return Next(17);}
   if(Age>30)return End(GuestBlocker(TEXT("Real stranded visitor could not swim to the stopped boat")));
  }
  if(Stage==17 && Age>.5 && Boat->GetVelocity().Size()<35){Key(EKeys::F6,true);return Next(18);}
  if(Stage==18 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();const auto* D=M?M->Phone.Opportunities.FindByPredicate([&](const auto& Value){return Value.OpportunityID==Job->ActivityID;}):nullptr;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !D || D->State!=1 || D->ActivityStage!=2 || D->ActivityRunID!=Run || D->VehicleID!=Boat->VehicleID || !D->ActivityTransportPickupValid || D->ActivityPassengerExiting || Wallet->GetBalance()!=Before || BoatTravel<18000 || Boat->GetHealthComponent()->GetCurrentHealth()<InitialCondition-.05f)return End(TEXT("Genuine F6 lost unpaid actual passenger/run/boat/pickup or actual safe outbound voyage evidence"));
   if(!WriteBridge(TEXT("Source.json")))return End(TEXT("Could not preserve genuine rescue source manifest"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SeaRescue-Native-Unpaid-Mid-Saved.png"));return End(TEXT(""));
  }
  if(Stage==25)
  {
   if(!Seated() || Job->BuildOpportunitySnapshot(false).State!=1)return End(TEXT("Actual returned passenger lost during continuous rescue return"));const FVector F=Berth.GetRotation().GetForwardVector().GetSafeNormal2D(),R=Berth.GetRotation().GetRightVector().GetSafeNormal2D(),Offset=Boat->GetActorLocation()-Berth.GetLocation();
   if(PathNode==Path.Num()-1 && FVector::DotProduct(Offset,F)>0 && FVector::DotProduct(Offset,F)<6500 && FMath::Abs(FVector::DotProduct(Offset,R))<120 && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw))<15 && Boat->GetVelocity().Size2D()<170){Station=Berth.GetLocation();StationYaw=Berth.Rotator().Yaw;return Next(26);}
   if(Age>380)return End(TEXT("Actual rescue return did not reach original safe berth approach"));
  }
  if(Stage==26)
  {
   if(FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())<125 && Boat->GetVelocity().Size2D()<65 && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw))<25){FTransform Exit;if(Boat->GetMarineInteraction()->FindExitTransform(Exit)){Input(0,0,0);Key(EKeys::F,true);return Next(27);}}
   if(Age>110)return End(TEXT("Actual rescue boat could not safely dock at unchanged original berth"));
  }
  if(Stage==27 && Age>.8){Key(EKeys::F,false);if(PC->GetPawn()!=P || !Based() || Boat->GetMarineMovement()->IsEngineRunning() || !Boat->GetMarineMovement()->IsMoored() || !Seated())return End(TEXT("Actual rescue F helm exit lost original berth restraint/player/passenger"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SeaRescue-Native-Returned-Docked.png"));return Next(38);}
  if(Stage==38)
  {
   if(!P->GetCharacterMovement()->IsMovingOnGround() || !P->GetCharacterMovement()->CurrentFloor.IsWalkableFloor()){if(Age>12)return End(TEXT("Returned rescue deck did not settle for the genuine native checkpoint"));return false;}
   if(!Save->ConfigureSlot(ReturnSlot,false) || Save->DoesSaveExist() || IFileManager::Get().FileExists(*(Bridge/TEXT("Returned.json"))))return End(TEXT("Preserve prior genuine returned rescue checkpoint"));
   Key(EKeys::F6,true);return Next(39);
  }
  if(Stage==39 && Age>.8)
  {
   Key(EKeys::F6,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !Based() || !Seated() || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==1 && D.ActivityStage==2 && D.ActivityRunID==Run;}) || !WriteBridge(TEXT("Returned.json")))return End(TEXT("Genuine returned rescue F6 did not preserve original unpaid run/guest/boat/deck"));
   return Next(28);
  }
  if(Stage==28)
  {
   if(!MovePlayer(Boat->GetActorTransform().TransformPosition(FVector(-145,75,35)),15,.25f)){if(Age>20)return End(TEXT("Normal rescue deck departure route blocked"));return false;}
   // XY arrival alone may occur during the last step/fall onto the rolling
   // deck. A land jump must be pressed after the native walkable floor exists.
   if(!P->GetCharacterMovement()->IsMovingOnGround() || !P->GetCharacterMovement()->CurrentFloor.IsWalkableFloor())
   {if(Age>20)return End(TEXT("Actual rescue departure stand did not settle on a walkable native deck"));return false;}
   Key(EKeys::SpaceBar,true);return Next(29);
  }
  if(Stage==29)
  {
   SetPlayerGoal(P->GetActorLocation()+Boat->GetActorRightVector().GetSafeNormal2D()*500,.8f);if(Age>.25)Key(EKeys::SpaceBar,false);
   if(!Based() && P->GetCharacterMovement()->IsSwimming()){ShoreNode=0;return Next(30);}
   if(Age>15)return End(FString::Printf(TEXT("Normal Space departure could not clear gunwale into water: native_mode%d base%s walkable%d player%s velocity%s"),int32(P->GetCharacterMovement()->MovementMode),*GetNameSafe(P->GetMovementBaseObject()),P->GetCharacterMovement()->CurrentFloor.IsWalkableFloor()?1:0,*P->GetActorLocation().ToString(),*P->GetVelocity().ToString()));
  }
  if(Stage==30)
  {
   const FVector Targets[]={Origin+FVector(6800,-2900,0),Origin+FVector(6800,2500,0),Origin+FVector(6800,5900,180)};if(FVector::Dist2D(P->GetActorLocation(),Targets[ShoreNode])<90 && ShoreNode<2)++ShoreNode;SetPlayerGoal(Targets[ShoreNode],.85f);Key(EKeys::SpaceBar,P->GetCharacterMovement()->IsSwimming());
   // 256 physically reached dry sand at world footZ~45cm, while this island's
   // ocean origin is -130cm. The old absolute worldZ100 gate could never pass.
   // Retain the100cm dry margin against the actual current water surface.
   float WaterHeight=0;FVector Current;const FVector Feet=P->GetActorLocation()-FVector(0,0,P->GetSimpleCollisionHalfHeight());
   const auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();
   if(ShoreNode==2 && P->GetCharacterMovement()->IsMovingOnGround() && !Based()
    && Marine && Marine->SampleWater(Feet,WaterHeight,Current) && Feet.Z-WaterHeight>100)
   {Key(EKeys::SpaceBar,false);WalkPath.Reset();ShoreNode=0;Test->AddInfo(FString::Printf(TEXT("SEA_RESCUE_NATIVE_DRY_SHORE foot_z=%.3f actual_water_z=%.3f margin_cm=%.3f grounded=true boat_based=false"),Feet.Z,WaterHeight,Feet.Z-WaterHeight));return Next(42);}
   if(Age>220)return End(TEXT("Normal rescue swim could not reach existing sandy static shore with real100cm dry margin"));
  }
  if(Stage==42)
  {
   // The beach and original clerk pier are separated by a real tidal channel.
   // Use the retained native R05/DPV beach/channel entry, already traversed by
   // Salvage.A264. An entirely dry A* cannot connect across actual water.
   const FVector Targets[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(56000,-163000,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-163000,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-173550,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55780,-173550,180))};
   if(FVector::Dist2D(P->GetActorLocation(),Targets[ShoreNode])<45)
   {if(ShoreNode<3)++ShoreNode;else if(P->GetCharacterMovement()->IsMovingOnGround() && !Based()){StopPlayer();Key(EKeys::SpaceBar,false);WalkPath.Reset();WalkNode=0;return Next(31);}}
   SetPlayerGoal(Targets[ShoreNode],.85f);Key(EKeys::SpaceBar,P->GetCharacterMovement()->IsSwimming());
   if(Age>120)return End(FString::Printf(TEXT("Native rescue beach/tidal-channel/pier return blocked at%s mode%d waypoint%d"),*P->GetActorLocation().ToString(),int32(P->GetCharacterMovement()->MovementMode),ShoreNode));
  }
  if(Stage==31)
  {
   // The previous arbitrary +100,+100 point can be outside the real pier.
   // Survey a grounded full-capsule/F-eye destination, as the native salvage
   // handoff already does. Only actual walking follows; no player placement.
   if(WalkPath.IsEmpty() && !PlanClerkApproach())return End(TEXT("Actual static shore has no supported collision-clear F approach to rescue clerk"));
   const FVector Goal=ClerkStand;if(FVector::Dist2D(P->GetActorLocation(),Goal)>45){while(WalkNode<WalkPath.Num()-1 && FVector::Dist2D(P->GetActorLocation(),WalkPath[WalkNode])<40)++WalkNode;MovePlayer(WalkNode==WalkPath.Num()-1?Goal:WalkPath[WalkNode],40,.8f);if(Age>90)return End(TEXT("Actual shore-to-clerk walk blocked"));return false;}
   StopPlayer();Focus(Clerk->Interaction->GetComponentLocation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Clerk)return End(TEXT("Actual rescue clerk normal F eye-trace inaccessible"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SeaRescue-Native-Dock-Handoff-Prompt.png"));Key(EKeys::F,true);return Next(32);
  }
  if(Stage==32 && Age>.2)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);auto* Guest=Job->GetActivityNPC();if(D.State==2)return End(TEXT("Sea rescue paid before actual visitor walked away"));
   if(!Guest || Guest->IsHidden() || !Guest->GetActorEnableCollision() || !Guest->GetCharacterMovement()->IsMovingOnGround() || !D.ActivityPassengerExiting)return End(GuestBlocker(TEXT("Normal clerk F could not place rescued visitor on the actual safe dock")));LastGuest=Guest->GetActorLocation();LastGuestAt=W->GetTimeSeconds();return Next(33);
  }
  if(Stage==33)
  {
   const auto D=Job->BuildOpportunitySnapshot(false);auto* Guest=Job->GetActivityNPC();if(Guest){const float Now=W->GetTimeSeconds(),Distance=FVector::Dist2D(Guest->GetActorLocation(),LastGuest);if(Now>LastGuestAt && Distance/(Now-LastGuestAt)>350)return End(TEXT("Rescued visitor exit exceeded ordinary walking speed"));GuestExitWalk+=Distance;LastGuest=Guest->GetActorLocation();LastGuestAt=Now;}
   if(D.State==2){if(D.ActivityCompletions!=1 || Guest || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()) || GuestExitWalk<20 || BoatTravel+CachedReturnTravel<18000 || Boat->GetHealthComponent()->GetCurrentHealth()<InitialCondition-.05f)return End(TEXT("Actual safe return/dock walk-away failed to clean guest and pay exactly one shared300credit"));return Next(34);}
   if(D.State!=1 || Age>25)return End(GuestBlocker(TEXT("Actual rescued visitor walk-away blocked")));
  }
  if(Stage==34 && Age>.2){Key(EKeys::F,true);return Next(35);}
  if(Stage==35 && Age>.2){Key(EKeys::F,false);if(Wallet->GetBalance()!=Before+Job->Reward || Job->BuildOpportunitySnapshot(false).ActivityCompletions!=1 || Job->GetActivityNPC())return End(TEXT("Repeated separate native F duplicated rescue reward or guest"));if(!Save->ConfigureSlot(DoneSlot,false) || Save->DoesSaveExist())return End(TEXT("Preserve prior genuine rescue completion slot"));Key(EKeys::F6,true);return Next(36);}
  if(Stage==36 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityRunID==Run && D.ActivityCompletions==1;}))return End(TEXT("Genuine completed-rescue F6 lost settled result"));if(!WriteBridge(TEXT("Completed.json")))return End(TEXT("Could not preserve genuine rescue completion manifest"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SeaRescue-Native-Completed-Saved.png"));return End(TEXT(""));
  }
  if(Stage==37 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Job->GetActivityNPC() || Wallet->GetBalance()!=Before)return End(TEXT("Normal rescue replay cancellation left a guest or another payment"));return End(TEXT(""));}
  return false;
 }
private:
 bool Based() const {const auto* Base=P?Cast<UPrimitiveComponent>(P->GetMovementBaseObject()):nullptr;return Base && Base->GetOwner()==Boat;}
 bool Seated() const
 {
  auto* Guest=Job?Job->GetActivityNPC():nullptr;if(!Guest || !Boat || !Guest->IsHidden() || Guest->GetActorEnableCollision() || Guest->GetAttachParentActor()!=Boat)return false;int32 Count=0;TArray<UTransportOccupantComponent*> Seats;Boat->GetComponents(Seats);for(auto* Seat:Seats)if(Seat && Seat->GetRepresentedDriver()==Guest && Seat->GetDriverMesh() && Seat->GetDriverMesh()->IsVisible())++Count;return Count==1;
 }
 void Focus(FVector Target){PC->SetControlRotation((Target-P->GetPawnViewLocation()).Rotation());}
 bool PlanClerkApproach()
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(SeaRescueNativeClerkApproach),false,P);const auto Shape=FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());
  for(float Radius:{100.f,140.f,180.f})for(const FVector Direction:{FVector(-1,0,0),FVector(1,0,0),FVector(0,-1,0),FVector(0,1,0)})
  {
   const FVector AtPoint=Clerk->GetActorLocation()+Direction*Radius;FHitResult H,Eye;
   if(!W->LineTraceSingleByChannel(H,AtPoint+FVector(0,0,350),AtPoint-FVector(0,0,250),ECC_Pawn,Q) || !H.IsValidBlockingHit() || H.ImpactNormal.Z<.7)continue;
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);float Water=0;FVector Current;
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,Shape,Q) || (W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(H.ImpactPoint,Water,Current) && H.ImpactPoint.Z<Water+15))continue;
   W->LineTraceSingleByChannel(Eye,Stand+FVector(0,0,P->BaseEyeHeight),Clerk->Interaction->GetComponentLocation(),ECC_Visibility,Q);
   if(Eye.GetActor()!=Clerk || FVector::Dist(Stand,Clerk->GetActorLocation()+FVector(0,0,90))>=235 || !Phase205cSupportedWalkPath(W,P,Stand,WalkPath))continue;
   ClerkStand=Stand;WalkNode=0;return true;
  }
  return false;
 }
 void SetPlayerGoal(FVector Goal,float Scale){PlayerGoal=Goal;PlayerScale=Scale;bMovePlayer=true;PC->SetControlRotation(FRotator(0,(Goal-P->GetActorLocation()).Rotation().Yaw,0));}
 void StopPlayer(){bMovePlayer=false;PlayerScale=0;}
 bool MovePlayer(FVector Goal,float Radius,float Scale){if(FVector::Dist2D(P->GetActorLocation(),Goal)<Radius){StopPlayer();return true;}const float D=FVector::Dist2D(P->GetActorLocation(),Goal);const float Denominator=FMath::Max(120.f,P->GetCharacterMovement()->GetMaxSpeed()*FMath::Max(0.f,W->GetDeltaSeconds())*3.f);SetPlayerGoal(Goal,FMath::Min(Scale,D/Denominator));return false;}
 void BuildReturn()
 {
  const FVector Start=Boat->GetActorLocation(),F=Berth.GetRotation().GetForwardVector().GetSafeNormal2D(),R=Berth.GetRotation().GetRightVector().GetSafeNormal2D(),B=Berth.GetLocation();Path.Reset();Path.Add(Start);
  // Physical broad U-turn, then the same proved local harbour approach curve.
  // This does not replay the unrelated full500m boat acceptance loop.
  for(int32 I=1;I<=10;++I){const float A=PI*I/10;Path.Add(Start-R*4500+R*(4500*FMath::Cos(A))+F*(4500*FMath::Sin(A)));}
  Path.Add(B+F*15000-R*10000);Path.Add(B+F*5000-R*10000);
  for(int32 I=1;I<=10;++I){const float A=PI*I/10;Path.Add(B+F*(5000-4000*FMath::Sin(A))-R*(5000+5000*FMath::Cos(A)));}
  Path.Add(B+F*6500);PathNode=1;
 }
 void Input(float T,float S,float B){LastThrottle=T;LastSteer=S;LastBrake=B;if(PC && Boat && PC->GetPawn()==Boat)Boat->SubmitDriverInput_Implementation(PC,T,S,B);}
 void Sail(bool Returning)
 {
  const FVector Here=Boat->GetActorLocation();while(PathNode<Path.Num()-1){const FVector From=Path[PathNode-1],Segment=Path[PathNode]-From;const float Length=Segment.Size2D(),Along=FVector::DotProduct(Here-From,Segment.GetSafeNormal2D());if(FVector::Dist2D(Here,Path[PathNode])>700 && !(Along>Length && FVector::Dist2D(Here,From+Segment.GetSafeNormal2D()*Along)<700))break;++PathNode;}
  const float Speed=FVector::DotProduct(Boat->GetVelocity(),Boat->GetActorForwardVector());const FVector From=Path[PathNode-1],Segment=Path[PathNode]-From;FVector Target=From+Segment.GetSafeNormal2D()*FMath::Clamp(float(FVector::DotProduct(Here-From,Segment.GetSafeNormal2D())),0.f,float(Segment.Size2D()));float Ahead=FMath::Clamp(FMath::Abs(Speed)*5,900.f,1700.f);int32 N=PathNode;
  while(N<Path.Num()-1 && Ahead>FVector::Dist2D(Target,Path[N])){Ahead-=FVector::Dist2D(Target,Path[N]);Target=Path[N++];}Target+=(Path[N]-Target).GetSafeNormal2D()*FMath::Min(Ahead,float(FVector::Dist2D(Target,Path[N])));
  if(Returning && PathNode==Path.Num()-1){const FVector F=Berth.GetRotation().GetForwardVector().GetSafeNormal2D();const float Along=FVector::DotProduct(Here-Berth.GetLocation(),F);Target=Berth.GetLocation()+F*(Along+Ahead);}
  const FVector Delta=Target-Here;const float Error=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Delta.Rotation().Yaw);float Desired=FMath::Abs(Error)>45?280.f:450.f;if(Returning && PathNode>=Path.Num()-11)Desired=280;
  if(Returning && PathNode==Path.Num()-1)Desired=100;const float Radius=2800+FMath::Abs(Speed)*3;const float Steer=FMath::Abs(Error)>70?FMath::Sign(Error):FMath::Clamp(2*Radius*FMath::Sin(FMath::DegreesToRadians(Error))/FMath::Max(500.f,float(Delta.Size2D())),-1.f,1.f);Input(FMath::Clamp((Desired-Speed)/350.f,-.45f,.8f),Steer,0);
 }
 void StationKeep(bool Docking)
 {
  const FVector F=FRotator(0,StationYaw,0).Vector(),R=FRotationMatrix(FRotator(0,StationYaw,0)).GetUnitAxis(EAxis::Y),Actual=Station-Boat->GetActorLocation(),Delta=Actual+(Docking?R*60:FVector::ZeroVector);const float Along=FVector::DotProduct(Delta,F),Cross=FVector::DotProduct(Delta,R),Speed=FVector::DotProduct(Boat->GetVelocity(),Boat->GetActorForwardVector());
  const float Correction=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Cross,FMath::Max(500.f,FMath::Abs(Along))))*(Along<0?-1.f:1.f),-20.f,20.f);const float Wanted=StationYaw+(Actual.Size2D()>90?Correction:0);const float Error=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Wanted),Rate=Boat->GetHull()->GetPhysicsAngularVelocityInDegrees().Z;
  const float Desired=FMath::Clamp(Along*.35f,-110.f,100.f);Input(FMath::Clamp((Desired-Speed)/190.f,-.55f,.45f),FMath::Clamp((Error-2.5f*Rate)/20.f,-1.f,1.f)*(Speed<-15?-1.f:1.f),0);
 }
 void DriveFrame(UWorld* World,ELevelTick,float)
 {
  if(World!=W || !PC || !P || !Boat)return;if(bMovePlayer && PC->GetPawn()==P)P->AddMovementInput((PlayerGoal-P->GetActorLocation()).GetSafeNormal2D(),PlayerScale);
  if(PC->GetPawn()!=Boat)return;if(Stage==14)Sail(false);else if(Stage==25)Sail(true);else if(Stage==26)StationKeep(true);else if(Stage==15 || Stage==16 || Stage==17 || Stage==18 || (PhoneGoal==2 && Stage>=1 && Stage<=5))StationKeep(false);
 }
 void Observe()
 {
  if(!Boat || !Job)return;const float Now=W->GetTimeSeconds();if(Now<LastObserve+.12f)return;LastObserve=Now;const FVector Here=Boat->GetActorLocation();if(LastBoatValid && PC->GetPawn()==Boat)BoatTravel+=FVector::Dist2D(Here,LastBoat);LastBoat=Here;LastBoatValid=true;auto* Guest=Job->GetActivityNPC();const FVector G=Guest?Guest->GetActorLocation():FVector::ZeroVector;const auto D=Job->BuildOpportunitySnapshot(false);
  const FVector Player=P?P->GetActorLocation():FVector::ZeroVector;
  CSV+=FString::Printf(TEXT("%.4f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f\n"),Now,Stage,D.ActivityStage,Here.X,Here.Y,Here.Z,Boat->GetActorRotation().Yaw,Boat->GetVelocity().Size2D(),PathNode,G.X,G.Y,G.Z,Guest?int32(Guest->GetCharacterMovement()->MovementMode):-1,Player.X,Player.Y,Player.Z,PierNode,bMovePlayer?1:0,PlayerGoal.X,PlayerGoal.Y,PlayerGoal.Z);
  if(PC->GetPawn()==Boat && (Stage==14 || Stage==15 || Stage==16 || Stage==17 || Stage==18 || (PhoneGoal==2 && Stage>=1 && Stage<=5)))
  {
   float RootWater=0,PortWater=0,StarboardWater=0;FVector Flow;auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();
   if(Marine){Marine->SampleWater(Here,RootWater,Flow);Marine->SampleWater(Boat->GetActorTransform().TransformPosition(FVector(0,-125,0)),PortWater,Flow);Marine->SampleWater(Boat->GetActorTransform().TransformPosition(FVector(0,125,0)),StarboardWater,Flow);}
   TArray<FOverlapResult> Nearby;FCollisionQueryParams Q(SCENE_QUERY_STAT(SeaRescueActualHullNeighbours),false,Boat);Q.AddIgnoredActor(P);FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
   W->OverlapMultiByObjectType(Nearby,Here,Boat->GetActorQuat(),Objects,FCollisionShape::MakeBox(Boat->GetHull()->GetScaledBoxExtent()),Q);LastNearHull.Reset();
   for(const auto& Hit:Nearby)if(auto* Part=Hit.GetComponent())if(Part->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block){if(!LastNearHull.IsEmpty())LastNearHull+=TEXT("|");LastNearHull+=GetNameSafe(Hit.GetActor())+TEXT("/")+GetNameSafe(Part);}
   const FVector LocalGuest=Guest?Boat->GetActorTransform().InverseTransformPosition(G):FVector::ZeroVector;
   StationCSV+=FString::Printf(TEXT("%.4f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%s\n"),Now,Stage,D.ActivityStage,Boat->GetActorRotation().Pitch,Boat->GetActorRotation().Roll,Boat->GetVelocity().Z,RootWater,PortWater,StarboardWater,FVector::Dist2D(Here,Station),FVector::Dist(Here,Pickup->GetActorLocation()),LocalGuest.X,LocalGuest.Y,LocalGuest.Z,LastThrottle,LastSteer,LastBrake,Boat->GetHull()->GetPhysicsAngularVelocityInDegrees().Z,Guest && Guest->GetCharacterMovement()->bEnablePhysicsInteraction?1:0,*LastNearHull);
  }
 }
 FString GuestBlocker(const TCHAR* Reason) const
 {
  auto* Guest=Job?Job->GetActivityNPC():nullptr;float Surface=0;FVector Current;if(Guest)W->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(Guest->GetActorLocation(),Surface,Current);return FString::Printf(TEXT("%s: guest%s mode%d water%.3f boat%s velocity%.2f"),Reason,Guest?*Guest->GetActorLocation().ToString():TEXT("missing"),Guest?int32(Guest->GetCharacterMovement()->MovementMode):-1,Surface,Boat?*Boat->GetActorLocation().ToString():TEXT("missing"),Boat?Boat->GetVelocity().Size():0);
 }
 bool ReadBridge(const TCHAR* Name)
 {
  FString Text;TSharedPtr<FJsonObject> J;double PID=0,X=0,Y=0,Z=0;FString R,B;FGuid OriginalBoat;if(!FFileHelper::LoadFileToString(Text,*(Bridge/Name)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid() || !J->TryGetNumberField(TEXT("process_id"),PID) || uint32(PID)==FPlatformProcess::GetCurrentProcessId() || !J->TryGetStringField(TEXT("run_id"),R) || !J->TryGetStringField(TEXT("boat_id"),B) || !FGuid::Parse(B,OriginalBoat) || OriginalBoat!=Boat->VehicleID || !J->TryGetNumberField(TEXT("balance"),ExpectedBalance) || !J->TryGetNumberField(TEXT("pickup_x"),X) || !J->TryGetNumberField(TEXT("pickup_y"),Y) || !J->TryGetNumberField(TEXT("pickup_z"),Z))return false;
  if(bResumeReturned && (!J->TryGetNumberField(TEXT("native_return_travel_cm"),CachedReturnTravel) || CachedReturnTravel<18000))return false;
  BoardingOrigin=FVector(X,Y,Z);return FGuid::Parse(R,Run);
 }
 bool WriteBridge(const TCHAR* Name)
 {
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("boat_id"),Boat->VehicleID.ToString());J->SetNumberField(TEXT("balance"),double(Wallet->GetBalance()));J->SetNumberField(TEXT("pickup_x"),BoardingOrigin.X);J->SetNumberField(TEXT("pickup_y"),BoardingOrigin.Y);J->SetNumberField(TEXT("pickup_z"),BoardingOrigin.Z);J->SetNumberField(TEXT("native_return_travel_cm"),BoatTravel+CachedReturnTravel);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Bridge,true);return FFileHelper::SaveStringToFile(Text,*(Bridge/Name));
 }
 bool Next(int32 S){StopPlayer();Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Error)
 {
  StopPlayer();if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}Input(0,0,0);if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("session"),Session);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("boat_id"),Boat?Boat->VehicleID.ToString():TEXT("missing"));J->SetNumberField(TEXT("actual_boat_travel_cm"),BoatTravel);J->SetNumberField(TEXT("cached_genuine_return_travel_cm"),CachedReturnTravel);J->SetBoolField(TEXT("genuine_returned_checkpoint_used"),bResumeReturned);J->SetNumberField(TEXT("actual_guest_exit_walk_cm"),GuestExitWalk);J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetNumberField(TEXT("boat_condition_before"),InitialCondition);J->SetNumberField(TEXT("boat_condition_after"),Boat?Boat->GetHealthComponent()->GetCurrentHealth():0);J->SetNumberField(TEXT("boat_pose_writes"),0);J->SetNumberField(TEXT("npc_pose_writes"),0);
  J->SetStringField(TEXT("scope"),Session==0?TEXT("Genuine prior dock F7, one declared supported player-only dock start. Normal P/O/RIGHT/ENTER accepts real rescue; normal character movement and F gunwale/helm board original Harborline. Existing marine gameplay input port drives real physics only to actual stranded visitor, safe stop, normal phone action boards actual visible guest; genuine unpaid Stage2/run/original boat/boarding origin F6. No boat/NPC pose or velocity writes, reward/completion/ownership/state injection. This does not rerun/certify the cached generic500m boat loop."):Session==1?TEXT("Independent process/genuine F7 of actual unpaid rescue save must preserve exact saved safe player/deck, original boat/run/boarding origin and one visible seated actual guest; remote fallback is a failure. Normal F helm, existing marine input port physically returns to unchanged original berth, safe F exit, normal jump/swim/static-foot path/clerkF releases actual visitor onto safe dock; real guest walking/cleanup/one shared300credit/repeatedF guard and genuine completedF6. Zero pose/state/reward fixtures."):TEXT("Independent genuine completed-rescue F7 preserves one original run/receipt/completion/payment and no orphan guest; normal P/O/RIGHT/ENTER replay and DELETE cancellation with no new payment. Zero pose/state/reward fixtures. Subjective audio/handling/whole-island presentation are separate."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(Session==0?TEXT("SeaRescueSource"):Session==1?TEXT("SeaRescueResume"):TEXT("SeaRescueCold"));IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("trajectory.csv")));FFileHelper::SaveStringToFile(StationCSV,*(Dir/TEXT("station-collision-water.csv")));return true;
 }
 FAutomationTestBase* Test;int32 Session;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityCoastalRegion* Coast=nullptr;ANeonCityFishingBoat* Boat=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Pickup=nullptr;AIslandActivityPoint* Clerk=nullptr;UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;TMap<FKey,bool> Keys;TArray<FVector> Path,WalkPath,PierPath;FDelegateHandle Driver;FNeonCityVisualEvidence Evidence;FTransform Berth;int32 Stage=0,PhoneGoal=1,Browsed=0,PierNode=0,ShoreNode=0,WalkNode=0,PathNode=1;float At=0,PlayerScale=0,StationYaw=0,LastObserve=-1,LastGuestAt=0,GuestExitWalk=0,BoatTravel=0,InitialCondition=0,LastThrottle=0,LastSteer=0,LastBrake=0;double Started=0,ExpectedBalance=0,CachedReturnTravel=0;int64 Before=0;bool bMovePlayer=false,LastBoatValid=false,bResumeReturned=false;FGuid Run;FVector Origin=FVector::ZeroVector,ClerkStand=FVector::ZeroVector,PlayerGoal=FVector::ZeroVector,Station=FVector::ZeroVector,BoardingOrigin=FVector::ZeroVector,LastGuest=FVector::ZeroVector,LastBoat=FVector::ZeroVector;FString MidSlot,DoneSlot,ReturnSlot,Bridge,InputSlot,WalkFailure,LastNearHull,StationCSV=TEXT("game_seconds,test_stage,activity_stage,pitch_deg,roll_deg,vertical_speed_cm_s,water_root,water_port,water_starboard,station_distance_cm,pickup_distance_cm,guest_local_x,guest_local_y,guest_local_z,throttle,steer,brake,yaw_rate_deg_s,guest_physics_interaction,actual_near_hull_blocking_components\n"),CSV=TEXT("game_seconds,test_stage,activity_stage,boat_x,boat_y,boat_z,boat_yaw,speed_cm_s,path_node,guest_x,guest_y,guest_z,guest_movement_mode,player_x,player_y,player_z,pier_node,player_goal_active,player_goal_x,player_goal_y,player_goal_z\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandSeaRescueSource,"NeonCity.Phase205c.Activity.SeaRescue.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandSeaRescueSource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandSeaRescue::FFlow(this,0));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandSeaRescueResume,"NeonCity.Phase205c.Activity.SeaRescue.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandSeaRescueResume::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandSeaRescue::FFlow(this,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandSeaRescueResultCold,"NeonCity.Phase205c.Activity.SeaRescue.C",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandSeaRescueResultCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandSeaRescue::FFlow(this,2));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
