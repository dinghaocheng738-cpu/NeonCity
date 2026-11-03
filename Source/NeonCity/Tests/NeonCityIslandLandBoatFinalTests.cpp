#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/AutomationEditorCommon.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Water/CoastalOceanSurface.h"
#include "Vehicles/VehicleOwnershipSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Property/PropertySubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

namespace NeonCityLandBoatFinal
{
const FString LegacyInputSlot=TEXT("NeonCity_Test_LandFinal_Legacy15");
const FString OutputSlotPrefix=TEXT("NeonCity_Test_LandFinal_BoatReturn_");
void Key(APlayerController* PC,FKey Name,EInputEvent Event)
{
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Name,Event,FPlatformTime::Cycles64()));
}

// Canonical value signatures include every persisted fish/meal/property field.
// They never grant inventory, rewrite checkpoints, or expose mutable registries.
template<typename T> FString Values(const TArray<T>& Records)
{
 TArray<FString> Rows;
 for(const T& Record:Records)
 {
  FString Text;T::StaticStruct()->ExportText(Text,&Record,nullptr,nullptr,PPF_None,nullptr);Rows.Add(MoveTemp(Text));
 }
 Rows.Sort();return FString::Join(Rows,TEXT("\n"));
}
FString VehicleIdentities(const TArray<FNeonCityVehicleSaveData>& Records)
{
 TArray<FString> Rows;
 for(const auto& R:Records)Rows.Add(FString::Printf(TEXT("%s|%s|%u|%d|%s|%d"),*R.VehicleID.ToString(),*R.DataAssetID.ToString(),R.VehicleType,R.Owned,*R.StoredPropertyID.ToString(),R.GarageSlot));
 Rows.Sort();return FString::Join(Rows,TEXT("\n"));
}
FString MarineIdentities(const TArray<FNeonCityMarineSaveData>& Records)
{
 TArray<FString> Rows;
 for(const auto& R:Records)Rows.Add(FString::Printf(TEXT("%s|%s|%d"),*R.VehicleID.ToString(),*R.DataAssetID.ToString(),R.Owned));
 Rows.Sort();return FString::Join(Rows,TEXT("\n"));
}

class FBoatFinal : public IAutomationLatentCommand
{
public:
 FBoatFinal(FAutomationTestBase* In,FString Session,FString Run):Test(In),Mode(MoveTemp(Session)),RunID(MoveTemp(Run)){}
 ~FBoatFinal(){Flush();ReleaseInput();}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>900)return Finish(false,TEXT("Native island boat return/save scenario exceeded 900 seconds."));
  if(Stage==0)return Initialize();
  if(!IsValid(W) || !IsValid(PC) || !IsValid(Boat) || !IsValid(Player))return Finish(false,TEXT("Native boat/player/world became unavailable."));
  Observe();
  if(Mode==TEXT("B"))return RestoreSession();
  if(Stage>=8 && Stage<=12 && (FMath::Abs(Boat->GetActorRotation().Roll)>35 || FMath::Abs(Boat->GetActorRotation().Pitch)>30))
   return Finish(false,FString::Printf(TEXT("Unsafe actual hull attitude during return: %s; native surface thresholds retained."),*Boat->GetActorRotation().ToString()));
  const float E=W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   if(E<10)return false;
   if(!Test->TestTrue(TEXT("Original Harborline remains moored alongside original island pier"),Boat->GetMarineMovement()->IsMoored()
    && FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())<100))return Finish(false,TEXT("Initial authored mooring did not settle."));
   return Next(2);
  }
  if(Stage==2)
  {
   const FVector Approach[]={Origin+FVector(5780,-3750,310),Origin+FVector(6110,-3750,310),Origin+FVector(6110,-3510,310),Origin+FVector(6250,-3510,-180)};
   const FVector Target=Approach[PierWaypoint];
   if(FVector::Dist2D(Target,Player->GetActorLocation())>15)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.6f,TEXT("pier")));
    if(E>35)return Finish(false,TEXT("Normal walk/swim approach to authored boat berth was blocked."));return false;
   }
   if(++PierWaypoint<UE_ARRAY_COUNT(Approach))return false;
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(3);
  }
  if(Stage==3)
  {
   const FVector Target=Boat->GetActorTransform().TransformPosition(FVector(-145,-195,25));
   if(FVector::Dist2D(Target,Player->GetActorLocation())>15)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.5f,TEXT("gunwale")));
    if(E>20)return Finish(false,TEXT("Normal swimmer approach to gunwale was blocked."));return false;
   }
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(4);
  }
  if(Stage==4)
  {
   if(Player->GetActorLocation().Z<Boat->GetActorLocation().Z-65)
   {Key(PC,EKeys::SpaceBar,IE_Pressed);if(E>25)return Finish(false,TEXT("Swimmer could not surface beside gunwale."));return false;}
   Key(PC,EKeys::SpaceBar,IE_Released);Focus(Boat->GetActorTransform().TransformPosition(FVector(-145,-130,25)));
   if(E<4)return false;
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Boat)
   {if(E>15)return Finish(false,TEXT("Native F boarding prompt did not resolve at authored gunwale."));return false;}
   Key(PC,EKeys::F,IE_Pressed);return Next(5);
  }
  if(Stage==5 && E>.7f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("Ordinary F boards character onto real moving deck"),PC->GetPawn()==Player && Based()))return Finish(false,TEXT("Native gunwale boarding failed."));
   return Next(6);
  }
  if(Stage==6)
  {
   const FVector Target=Boat->GetActorTransform().TransformPosition(FVector(-75,0,35));
   if(FVector::Dist2D(Target,Player->GetActorLocation())>12)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.25f,TEXT("helm")));
    if(E>15)return Finish(false,TEXT("Native cabin route to helm was blocked."));return false;
   }
   Player->GetCharacterMovement()->StopMovementImmediately();Focus(Boat->GetActorTransform().TransformPosition(FVector(110,55,120)));
   if(E<.5f)return false;
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Boat)
   {if(E>15)return Finish(false,TEXT("Native helm prompt did not resolve."));return false;}
   Key(PC,EKeys::F,IE_Pressed);return Next(7);
  }
  if(Stage==7 && E>.4f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("F takes Harborline helm and releases original berth restraint"),PC->GetPawn()==Boat
    && Boat->GetMarineMovement()->IsEngineRunning() && !Boat->GetMarineMovement()->IsMoored()))return Finish(false,TEXT("Native helm entry failed."));
   LastBoat=Boat->GetActorLocation();PreviousHeading=Boat->GetActorRotation().Yaw;return Next(8);
  }
  if(Stage==8)return SailRoute(E);
  if(Stage==9)return Dock(E);
  if(Stage==10 && E>.8f)
  {
   Key(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("Normal F exits helm to native deck, stops engine, and ties to original berth"),PC->GetPawn()==Player
    && Based() && !Boat->GetMarineMovement()->IsEngineRunning() && Boat->GetMarineMovement()->IsMoored()))return Finish(false,TEXT("Native return docking/helm exit failed."));
   if(Mode==TEXT("D"))return Finish(!Test->HasAnyErrors(),TEXT(""));
   return Next(11);
  }
  if(Stage==11)
  {
   // The source pier is 2.3m above water; the boat deck is .35m. No invented
   // shore-exit teleport or climbing action. Use the ordinary jump over the
   // .75m gunwale, then surface swim east of the pier back to its sandy foot.
   const FVector DeckTarget=Boat->GetActorTransform().TransformPosition(FVector(-145,75,35));
   if(FVector::Dist2D(DeckTarget,Player->GetActorLocation())>15)
   {
    Player->AddMovementInput((DeckTarget-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(DeckTarget,.25f,TEXT("departure-jump")));
    if(E>15)return Finish(false,TEXT("Deck route to native departure jump was blocked."));return false;
   }
   JumpStarted=W->GetTimeSeconds();Key(PC,EKeys::SpaceBar,IE_Pressed);return Next(12);
  }
  if(Stage==12)
  {
   Player->AddMovementInput(Boat->GetActorRightVector().GetSafeNormal2D(),.8f);
   if(W->GetTimeSeconds()-JumpStarted>.25f)Key(PC,EKeys::SpaceBar,IE_Released);
   if(!Based() && Player->GetCharacterMovement()->IsSwimming()){SawSwimming=true;return Next(13);}
   if(E>12)return Finish(false,TEXT("Normal Space jump could not clear gunwale into water."));return false;
  }
  if(Stage==13)return SwimToShore(E);
  if(Stage==14)
  {
   Key(PC,EKeys::SpaceBar,IE_Released);
   // The real swimming path has already reached dry shore. Its actual static
   // walkable floor is the checkpoint; an arbitrary additional XY target can
   // lie beyond a physical ledge and is not a gameplay save requirement.
   Player->GetCharacterMovement()->StopMovementImmediately();
   if(E<2 || !Player->GetCharacterMovement()->IsMovingOnGround() || !Player->GetCharacterMovement()->CurrentFloor.IsWalkableFloor()
    || Player->GetMovementBaseObject()==Boat->GetHull() || Player->GetActorLocation().Z-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()<100)
   {if(E>45)return Finish(false,TEXT("Player did not reach static onshore floor after native swimming."));return false;}
   if(Mode!=TEXT("S") && !Test->TestTrue(TEXT("Full native 500m boat departure, turn, return, docking and swimming completed"),Travel>50000 && HeadingTravel>170
    && PeakSpeed>300 && SawSwimming && RollMaximum-RollMinimum>.2 && HeightMaximum-HeightMinimum>4))return Finish(false,TEXT("Continuous route/acceleration/turn/wave evidence was insufficient."));
   if(Mode!=TEXT("S") && !Test->TestTrue(TEXT("Voyage used the live unmodified coastal ocean with advancing native water queries"),IsValid(Ocean) && Ocean->IsReady()
    && Ocean->GetStrength()>0 && Ocean->GetQuerySequence()>InitialOceanQuery+25))return Finish(false,TEXT("Native ocean wave/query evidence did not advance during the voyage."));
   if(Boat->GetHealthComponent()->GetCurrentHealth()<InitialCondition-.05f)return Finish(false,TEXT("Continuous route incurred boat impact damage."));
   if(!Save->SaveGame())return Finish(false,TEXT("Native onshore SaveGame failed: ")+Save->GetLastDetail());
   Expected.Reset(Save->ReadSaveForInspection());
   if(!Expected.IsValid() || !Expected->SaveID.IsValid())return Finish(false,TEXT("Native onshore save was not readable."));
   const auto* SavedBoat=FindBoat(Expected.Get());
   if(!Test->TestTrue(TEXT("Saved Harborline remained at original berth with stable ID"),SavedBoat
    && FVector::Dist2D(SavedBoat->Transform.GetLocation(),Berth.GetLocation())<150 && Expected->Vehicles.Num()==InitialVehicleCount
    && VehicleIdentities(Expected->Vehicles)==InitialVehicleIdentities
    && MarineIdentities(Expected->Marine)==InitialMarineIdentities))return Finish(false,TEXT("Onshore snapshot changed native boat/land identities."));
   if(Values(Expected->Fishing.Fish)!=InitialFish || Values(Expected->Fishing.Meals)!=InitialMeals || Values(Expected->Properties)!=InitialProperties)
    return Finish(false,TEXT("Continuous boat route unexpectedly changed existing fish, meals or property state."));
   if(!WriteManifest(Expected.Get()))return Finish(false,TEXT("Session A evidence could not be written."));
   return Finish(!Test->HasAnyErrors(),TEXT(""));
  }
  return false;
 }
private:
 bool Initialize()
 {
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<2)return false;
  Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->IsInitialRegionReady())return false;
  for(TActorIterator<ANeonCityFishingBoat> It(W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Boat=*It;
  if(!Boat)return Finish(false,TEXT("Original authored Harborline is missing."));
  for(TActorIterator<ACoastalOceanSurface> It(W);It;++It)if(It->IsReady()){Ocean=*It;break;}
  if(!Ocean)return false;
  if(Mode==TEXT("D"))return InitializeDockFixture();
  Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  Slot=OutputSlotPrefix+RunID;
  Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1/Automation15/BoatFinal")/RunID;
  IFileManager::Get().MakeDirectory(*Directory,true);
  CSV=TEXT("game_seconds,wall_seconds,stage,waypoint,boat_x,boat_y,boat_z,pitch,yaw,roll,speed_cm_s,throttle,steer,brake,player_x,player_y,player_z,player_mode,player_based,moored\n");
  if(!Save)return Finish(false,TEXT("Native save subsystem is unavailable."));
  if((Mode==TEXT("B") || !InputLoadRequested) && !Save->ConfigureSlot(Slot,false))return Finish(false,TEXT("Dedicated boat return test slot rejected."));
  Origin=Coast->GetActorLocation();Berth=Coast->GetFishingBerth();
  if(Mode==TEXT("B"))
  {
   if(!Save->DoesSaveExist())return Finish(false,TEXT("Session B requires successful Session A disk save."));
   Expected.Reset(Save->ReadSaveForInspection());if(!Expected.IsValid() || !FindBoat(Expected.Get()))return Finish(false,TEXT("Session B cannot inspect retained native boat checkpoint."));
   FString Text;TSharedPtr<FJsonObject> A;
   if(!FFileHelper::LoadFileToString(Text,*(Directory/TEXT("SessionA.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),A)
    || !A.IsValid())return Finish(false,TEXT("Successful Session A receipt is missing."));
   if(!Test->TestTrue(TEXT("Session B is a different process using identical disk save ID"),uint32(A->GetNumberField(TEXT("processId")))!=FPlatformProcess::GetCurrentProcessId()
    && A->GetBoolField(TEXT("passed")) && A->GetStringField(TEXT("saveId"))==Expected->SaveID.ToString()))return Finish(false,TEXT("Cross-process identity/Session A gate failed."));
   if(!Test->TestTrue(TEXT("Session A disk generation retains all real fishing/property and stable transport values"),!Expected->Fishing.Fish.IsEmpty()
    && A->GetStringField(TEXT("fishValues"))==Values(Expected->Fishing.Fish)
    && A->GetStringField(TEXT("mealValues"))==Values(Expected->Fishing.Meals)
    && A->GetStringField(TEXT("propertyValues"))==Values(Expected->Properties)
    && A->GetStringField(TEXT("landVehicleIdentities"))==VehicleIdentities(Expected->Vehicles)
    && A->GetStringField(TEXT("marineIdentities"))==MarineIdentities(Expected->Marine)))return Finish(false,TEXT("Native disk save differs from Session A value receipt."));
   InputSaveID=A->GetStringField(TEXT("nativeInputSaveId"));InputLoadDetail=A->GetStringField(TEXT("nativeInputLoadDetail"));
   if(!Save->LoadGame())return Finish(false,TEXT("Native Session B LoadGame failed: ")+Save->GetLastDetail());
   Stage=20;At=W->GetTimeSeconds();return RestoreSession();
  }
  if(!InputLoadRequested)
  {
   if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*(Directory/TEXT("SessionA.json"))))return Finish(false,TEXT("Session A run ID already has retained evidence; choose a new NeonCityLandBoatRun ID."));
   // BoatCatch is saved at sea. Loading it here would restore its offshore boat
   // and invalidate the dock-departure scenario. Its two catches are validated
   // by ContinuousDrive/BBQ; this voyage uses the byte-exact Legacy15 source.
   if(!Save->ConfigureSlot(LegacyInputSlot,false) || !Save->DoesSaveExist())return Finish(false,TEXT("The retained byte-exact Legacy15 native input save is required."));
   InputModel.Reset(Save->ReadSaveForInspection());
   if(!InputModel.IsValid() || !InputModel->SaveID.IsValid() || InputModel->Fishing.Fish.IsEmpty())return Finish(false,TEXT("Legacy15 input must contain a readable real fish record."));
   InputSaveID=InputModel->SaveID.ToString();InputLoadRequested=true;InputLoadStarted=FPlatformTime::Seconds();
   if(!Save->LoadGame())return Finish(false,TEXT("Full native Legacy15 LoadGame failed: ")+Save->GetLastDetail());
   return false;
  }
  // Do not switch slots while a deferred native load is outstanding.
  if(Save->IsOperationInProgress())
  {if(FPlatformTime::Seconds()-InputLoadStarted>20)return Finish(false,TEXT("Native Legacy15 input load did not complete."));return false;}
  if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return Finish(false,TEXT("Native Legacy15 input restore failed: ")+Save->GetLastDetail());
  InputLoadDetail=Save->GetLastDetail();
  auto* GI=W->GetGameInstance();const auto Fish=GI->GetSubsystem<UNeonCityFishingSubsystem>()->BuildPersistenceSnapshot();
  if(!Test->TestTrue(TEXT("Original real fish and meal values survive full Legacy15 native load"),!Fish.Fish.IsEmpty()
   && Values(Fish.Fish)==Values(InputModel->Fishing.Fish) && Values(Fish.Meals)==Values(InputModel->Fishing.Meals)))return Finish(false,TEXT("Native input load lost or changed original real fishing values."));
  for(const auto& F:Fish.Fish)
   if(!Test->TestTrue(TEXT("Loaded real catch has native GUID/source/UTC/weight provenance"),F.FishID.IsValid() && !F.SpeciesID.IsNone()
    && !F.FishingPointID.IsNone() && F.CaughtAt.GetTicks()>0 && FMath::IsFinite(F.WeightKg) && F.WeightKg>0))return Finish(false,TEXT("Input real-catch provenance is invalid."));
  for(const auto& P:InputModel->Properties)
   if(!Test->TestEqual(TEXT("Original property ownership survives native input load"),GI->GetSubsystem<UPropertySubsystem>()->IsOwned(P.PropertyID),P.Owned))return Finish(false,TEXT("Native input changed original property ownership."));
  const auto LoadedVehicles=GI->GetSubsystem<UVehicleOwnershipSubsystem>()->BuildPersistenceSnapshot();
  const auto LoadedMarine=W->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot();
  for(const auto& R:InputModel->Vehicles)
  {
   const auto* Live=LoadedVehicles.FindByPredicate([&](const auto& L){return L.VehicleID==R.VehicleID;});
   if(!Test->TestTrue(TEXT("Original land vehicle ID/profile/ownership survives native input load"),Live && Live->DataAssetID==R.DataAssetID
    && Live->VehicleType==R.VehicleType && Live->Owned==R.Owned))return Finish(false,TEXT("Native input changed original land vehicle identity."));
  }
  for(const auto& R:InputModel->Marine)
  {
   const auto* Live=LoadedMarine.FindByPredicate([&](const auto& L){return L.VehicleID==R.VehicleID;});
   if(!Test->TestTrue(TEXT("Original marine ID/profile/ownership survives native input load"),Live && Live->DataAssetID==R.DataAssetID && Live->Owned==R.Owned))return Finish(false,TEXT("Native input changed original marine identity."));
  }
  if(!Test->TestTrue(TEXT("Legacy15 restores the real working boat alongside original berth without fixture repair"),Boat->GetMarineMovement()->IsMoored()
   && FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())<150
   && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw))<35))return Finish(false,TEXT("Legacy15 boat is not at original berth; no boat pose correction is permitted."));
  if(!Save->ConfigureSlot(Slot,false))return Finish(false,TEXT("Dedicated boat output slot rejected after native input load."));
  InitialFish=Values(Fish.Fish);InitialMeals=Values(Fish.Meals);InitialProperties=Values(GI->GetSubsystem<UPropertySubsystem>()->BuildPersistenceSnapshot());
  InitialVehicleCount=LoadedVehicles.Num();InitialVehicleIdentities=VehicleIdentities(LoadedVehicles);InitialCondition=Boat->GetHealthComponent()->GetCurrentHealth();
  InitialMarineIdentities=MarineIdentities(LoadedMarine);InitialOceanQuery=Ocean->GetQuerySequence();
  if(Mode==TEXT("S"))
  {
   Test->AddInfo(TEXT("FIX-SPECIFIC DRY SHORE SAVE ONLY: full Legacy15 native load, one initial player fixture at the retained failed shore endpoint, no boat pose write or voyage claim. Return/dock/exit/swim evidence remains in Final16-04."));
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(56705.047,-163789.102,257.150)),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
   LastBoat=Boat->GetActorLocation();HeightMinimum=HeightMaximum=LastBoat.Z;RollMinimum=RollMaximum=Boat->GetActorRotation().Roll;
   return Next(14);
  }
  const FVector F=Berth.GetRotation().GetForwardVector().GetSafeNormal2D(),R=Berth.GetRotation().GetRightVector().GetSafeNormal2D(),B=Berth.GetLocation();
  for(const FVector2D P:{FVector2D(14000,0),FVector2D(22000,-4000),FVector2D(15000,-11000),FVector2D(5000,-10000)})Route.Add(B+F*P.X+R*P.Y);
  // The eastern route crossed a real 10cm-deep shoal and grounded the hull.
  // Keep the return turn west of the berth and offshore, then back into the
  // original berth while facing out to sea, as the authored boat does. This
  // changes only native pilot inputs; no terrain, draft or docking gate changes.
  for(int32 I=1;I<=8;++I)
  {
   const float A=PI*float(I)/8.f;
   Route.Add(B+F*(5000.f-4000.f*FMath::Sin(A))-R*(5000.f+5000.f*FMath::Cos(A)));
  }
  Route.Add(B+F*6500.f);
  double PlannedLength=0;FVector Previous=B;
  for(int32 I=0;I<Route.Num();++I)
  {
   PlannedLength+=FVector::Dist2D(Previous,Route[I]);Previous=Route[I];
   Test->AddInfo(FString::Printf(TEXT("BOAT_FINAL_ROUTE waypoint=%d local_forward_cm=%.1f local_right_cm=%.1f world=%s"),
    I,FVector::DotProduct(Route[I]-B,F),FVector::DotProduct(Route[I]-B,R),*Route[I].ToString()));
  }
  Test->AddInfo(FString::Printf(TEXT("BOAT_FINAL_ROUTE count=%d planned_length_cm=%.1f sail_deadline_game_seconds=460 dock_gate_cm=125 speed_gate_cm_s=65 yaw_gate_degrees=25"),Route.Num(),PlannedLength));
  // The sole scenario fixture moves only the player to the real connected pier.
  // Every boat motion thereafter comes from the normal controller/Chaos forces.
  Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  Player->SetActorLocation(Origin+FVector(5500,-3510,330),false,nullptr,ETeleportType::TeleportPhysics);
  Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
  LastBoat=Boat->GetActorLocation();HeightMinimum=HeightMaximum=LastBoat.Z;RollMinimum=RollMaximum=Boat->GetActorRotation().Roll;
  return Next(1);
 }
 bool SailRoute(float E)
 {
  if(PC->GetPawn()!=Boat)return Finish(false,TEXT("Native pilot possession was lost during continuous voyage."));
  if(E>460)return Finish(false,FString::Printf(TEXT("Continuous native route stalled at waypoint %d."),Waypoint));
  const FVector Here=Boat->GetActorLocation(),F=Berth.GetRotation().GetForwardVector().GetSafeNormal2D(),R=Berth.GetRotation().GetRightVector().GetSafeNormal2D();
  const float Speed=FVector::DotProduct(Boat->GetVelocity(),Boat->GetActorForwardVector());
  const float BerthAlong=FVector::DotProduct(Here-Berth.GetLocation(),F),BerthCross=FVector::DotProduct(Here-Berth.GetLocation(),R);
  const float BerthYawError=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw);
  float Distance=FVector::Dist2D(Here,Route[Waypoint]);
  FVector From=Waypoint?Route[Waypoint-1]:Berth.GetLocation(),Segment=Route[Waypoint]-From;
  float Length=Segment.Size2D();FVector Direction=Segment.GetSafeNormal2D();
  float Along=FVector::DotProduct(Here-From,Direction);
  const float CrossTrack=FVector::Dist2D(Here,From+Direction*Along);
  if(Waypoint<Route.Num()-1 && (Distance<(Waypoint<3?950.f:550.f) || (Along>Length && CrossTrack<700.f)))
  {
   ++Waypoint;Distance=FVector::Dist2D(Here,Route[Waypoint]);From=Route[Waypoint-1];Segment=Route[Waypoint]-From;
   Length=Segment.Size2D();Direction=Segment.GetSafeNormal2D();Along=FVector::DotProduct(Here-From,Direction);
  }
  const bool StraightApproach=Waypoint==Route.Num()-1;
  if(StraightApproach && BerthAlong>0 && BerthAlong<6500 && FMath::Abs(BerthCross)<120
   && FMath::Abs(BerthYawError)<15 && Boat->GetVelocity().Size2D()<170)
   return Next(9);
  if(StraightApproach && BerthAlong>9000)
   return Finish(false,TEXT("Native pilot missed the original berth approach corridor; no boat pose correction performed."));
  // Look ahead along the real polyline before reaching a corner. A missed
  // waypoint plane with small cross-track error is progress, not a low-speed
  // orbit around a point that is already behind the bow.
  float Ahead=FMath::Clamp(FMath::Abs(Speed)*5.f,900.f,1700.f);
  FVector Target;
  if(StraightApproach)Target=Berth.GetLocation()+F*(BerthAlong+Ahead);
  else
  {
   Target=From+Direction*FMath::Clamp(Along,0.f,Length);
   float Remaining=FVector::Dist2D(Target,Route[Waypoint]);int32 NextPoint=Waypoint;
   while(Ahead>Remaining && NextPoint<Route.Num()-1)
   {Ahead-=Remaining;Target=Route[NextPoint++];Remaining=FVector::Dist2D(Target,Route[NextPoint]);}
   Target+=(Route[NextPoint]-Target).GetSafeNormal2D()*FMath::Min(Ahead,Remaining);
  }
  const FVector Delta=Target-Here;
  const float HeadingError=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Delta.Rotation().Yaw);
  const bool Aligned=FMath::Abs(BerthYawError)<15 && FMath::Abs(BerthCross)<120;
  // Rudder lift depends on water speed and propeller race. Keep usable flow
  // during a turn; do not multiply a 110cm/s approach by a .2 heading factor.
  float TargetSpeed=Waypoint<4?450.f:280.f;
  if(FMath::Abs(HeadingError)>45)TargetSpeed=280.f;
  if(StraightApproach && Aligned)TargetSpeed=100.f;
  const float LookDistance=FMath::Max(500.f,float(Delta.Size2D()));
  const float FullRudderRadius=2800.f+FMath::Abs(Speed)*3.f;
  Steer=FMath::Abs(HeadingError)>70?FMath::Sign(HeadingError)
   :FMath::Clamp(2.f*FullRudderRadius*FMath::Sin(FMath::DegreesToRadians(HeadingError))/LookDistance,-1.f,1.f);
  Throttle=FMath::Clamp((TargetSpeed-Speed)/350.f,-.45f,.8f);Brake=0;
  // Reverse propeller thrust is the real brake. Space only closes throttle on
  // this reference hull and must never be interpreted as an instant stop.
  SetInput(Throttle,Steer,Brake);
  if(W->GetTimeSeconds()-LastPilotLog>=2 || Waypoint!=LastPilotWaypoint)
  {
   Test->AddInfo(FString::Printf(TEXT("BOAT_FINAL_PILOT elapsed=%.2f waypoint=%d target=%s target_distance_cm=%.1f heading_error=%.2f target_speed_cm_s=%.1f actual_forward_cm_s=%.1f berth_along_cm=%.1f berth_cross_cm=%.1f berth_yaw_error=%.2f throttle=%.4f steer=%.4f straight=%d aligned=%d"),
    E,Waypoint,*Target.ToString(),Delta.Size2D(),HeadingError,TargetSpeed,Speed,BerthAlong,BerthCross,BerthYawError,Throttle,Steer,StraightApproach?1:0,Aligned?1:0));
   LastPilotLog=W->GetTimeSeconds();LastPilotWaypoint=Waypoint;
  }
  return false;
 }
 bool Dock(float E)
 {
  if(E>110)return Finish(false,FString::Printf(TEXT("Native final berth maneuver failed: offset %.1fcm pose %s speed %.1fcm/s."),
   FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation()),*Boat->GetActorRotation().ToString(),Boat->GetVelocity().Size2D()));
  const FVector ActualDelta=Berth.GetLocation()-Boat->GetActorLocation();
  // Starboard pilot clearance counters reverse prop-walk beside the real
  // pier. Acceptance remains measured from the unchanged original berth.
  const FVector Delta=ActualDelta+Berth.GetRotation().GetRightVector().GetSafeNormal2D()*60.f;
  const FVector F=Berth.GetRotation().GetForwardVector().GetSafeNormal2D();
  const float Distance=ActualDelta.Size2D(),ForwardError=FVector::DotProduct(Delta,F),Speed=FVector::DotProduct(Boat->GetVelocity(),Boat->GetActorForwardVector());
  const float YawError=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw);
  if(Distance<125 && Boat->GetVelocity().Size2D()<65 && FMath::Abs(YawError)<25)
  {
   FTransform Exit;
   if(Boat->GetMarineInteraction()->FindExitTransform(Exit))
   {SetInput(0,0,0);Key(PC,EKeys::F,IE_Pressed);return Next(10);}
  }
  // Keep the original berth heading during slow forward/reverse docking.
  // Aim with a bounded lateral correction instead of demanding a large turn
  // toward the berth centre when the remaining longitudinal distance is tiny.
  const float CrossError=FVector::DotProduct(Delta,Berth.GetRotation().GetRightVector().GetSafeNormal2D());
  const float Correction=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(CrossError,FMath::Max(500.f,FMath::Abs(ForwardError))))*(ForwardError<0?-1.f:1.f),-20.f,20.f);
  const float WantedYaw=Berth.Rotator().Yaw+(Distance>90?Correction:0.f);
  const float Error=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,WantedYaw);
  const float Desired=FMath::Clamp(ForwardError*.35f,-110.f,100.f);
  const float YawRate=Boat->GetHull()->GetPhysicsAngularVelocityInDegrees().Z;
  Throttle=FMath::Clamp((Desired-Speed)/190.f,-.55f,.45f);
  Steer=FMath::Clamp((Error-2.5f*YawRate)/20.f,-1.f,1.f)*(Speed<-15?-1.f:1.f);Brake=0;
  SetInput(Throttle,Steer,Brake);return false;
 }
 bool InitializeDockFixture()
 {
  Origin=Coast->GetActorLocation();Berth=Coast->GetFishingBerth();
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  Directory=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/Label/TEXT("BoatBerthRecovery");
  IFileManager::Get().MakeDirectory(*Directory,true);
  CSV=TEXT("game_seconds,wall_seconds,stage,waypoint,boat_x,boat_y,boat_z,pitch,yaw,roll,speed_cm_s,throttle,steer,brake,player_x,player_y,player_z,player_mode,player_based,moored\n");
  // The recorded start of the failed docking stage, before it diverged.
  // Initial fixtures are disclosed and cannot certify a continuous voyage.
  if(!Boat->GetMarineMovement()->StartEngine())return Finish(false,TEXT("Dock fixture could not release native mooring."));
  Boat->SetActorTransform(FTransform(FRotator(-2.4294,-88.5478,2.8030),NeonCityCoastalLayout::FromLegacyIsland(FVector(56503.788,-178554.442,-148.399))),false,nullptr,ETeleportType::TeleportPhysics);
  Boat->GetHull()->SetPhysicsLinearVelocity(Boat->GetActorForwardVector()*168.829f);
  Boat->GetHull()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
  const FVector Point=Boat->GetActorTransform().TransformPosition(FVector(-75,0,35));
  FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(BoatDockFixture),false,Player);
  if(!W->LineTraceSingleByChannel(Floor,Point+FVector(0,0,20),Point-FVector(0,0,30),ECC_Pawn,Q)
   || Floor.GetActor()!=Boat)return Finish(false,TEXT("Dock fixture requires the real source deck floor."));
  Player->SetActorLocation(Floor.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
  Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Player->SetBase(Floor.GetComponent());
  if(!Boat->GetMarineInteraction()->Enter(Player) || PC->GetPawn()!=Boat)return Finish(false,TEXT("Dock fixture could not enter native helm from its real deck."));
  Test->AddInfo(TEXT("FIX-SPECIFIC DOCK ONLY: one initial recorded hull pose/velocity and one deck player fixture; native helm, driver inputs, collision, F exit and mooring; no saves or continuous-voyage claim."));
  InitialCondition=Boat->GetHealthComponent()->GetCurrentHealth();LastBoat=Boat->GetActorLocation();
  At=W->GetTimeSeconds();Stage=9;return false;
 }
 bool SwimToShore(float E)
 {
  if(E>220)return Finish(false,FString::Printf(TEXT("Native surface swim to sandy pier foot failed at %s mode%d."),*Player->GetActorLocation().ToString(),int32(Player->GetCharacterMovement()->MovementMode)));
  const FVector Targets[]={Origin+FVector(6800,-2900,0),Origin+FVector(6800,2500,0),Origin+FVector(6800,5900,180)};
  const FVector Target=Targets[ShoreWaypoint];
  if(FVector::Dist2D(Player->GetActorLocation(),Target)<90)
  {if(++ShoreWaypoint==UE_ARRAY_COUNT(Targets)){Key(PC,EKeys::SpaceBar,IE_Released);return Next(14);}}
  const FVector Destination=Targets[ShoreWaypoint];
  Player->AddMovementInput((Destination-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Destination,.85f,TEXT("surface-swim")));
  if(Player->GetCharacterMovement()->IsSwimming())
  {SawSwimming=true;Key(PC,EKeys::SpaceBar,IE_Pressed);}
  else Key(PC,EKeys::SpaceBar,IE_Released);
  return false;
 }
 bool RestoreSession()
 {
  if(Save->IsOperationInProgress())
  {if(W->GetTimeSeconds()-At>20)return Finish(false,TEXT("Native cross-process load did not finish."));return false;}
  if(Stage==20)
  {
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return Finish(false,TEXT("Native cross-process load returned: ")+Save->GetLastDetail());
   const auto* R=FindBoat(Expected.Get());
   if(!R)return Finish(false,TEXT("Saved stable Harborline record is missing."));
   Test->TestTrue(TEXT("Native load restores stable boat pose before normal wave settling"),FVector::Dist(Boat->GetActorLocation(),R->Transform.GetLocation())<15
    && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,R->Transform.Rotator().Yaw))<1);
   Test->TestTrue(TEXT("Native boat ownership and condition survive process restart"),Boat->Owned==R->Owned
    && FMath::IsNearlyEqual(Boat->GetHealthComponent()->GetCurrentHealth(),R->Condition*Boat->GetProfile()->Durability,.02f));
   Test->TestFalse(TEXT("Restored native boat engine is stopped"),Boat->GetMarineMovement()->IsEngineRunning());
   Test->TestTrue(TEXT("Native load restores player to actual onshore checkpoint without city fallback"),PC->GetPawn()==Player
    && FVector::Dist(Player->GetActorLocation(),Expected->Player.Transform.GetLocation())<15);
   auto* GI=W->GetGameInstance();const auto Fish=GI->GetSubsystem<UNeonCityFishingSubsystem>()->BuildPersistenceSnapshot();
   Test->TestEqual(TEXT("Every existing fish value survives cross-process load"),Values(Fish.Fish),Values(Expected->Fishing.Fish));
   Test->TestEqual(TEXT("Every existing meal value survives cross-process load"),Values(Fish.Meals),Values(Expected->Fishing.Meals));
   Test->TestEqual(TEXT("Property ownership and authored garage values survive cross-process load"),Values(GI->GetSubsystem<UPropertySubsystem>()->BuildPersistenceSnapshot()),Values(Expected->Properties));
   Test->TestEqual(TEXT("Land vehicle IDs and ownership survive boat save/load without duplication"),VehicleIdentities(GI->GetSubsystem<UVehicleOwnershipSubsystem>()->BuildPersistenceSnapshot()),VehicleIdentities(Expected->Vehicles));
   Test->TestEqual(TEXT("Every native marine vehicle ID/profile/ownership survives process restart"),MarineIdentities(W->GetSubsystem<UMarineWorldSubsystem>()->BuildPersistenceSnapshot()),MarineIdentities(Expected->Marine));
   return Next(21);
  }
  if(Stage==21 && W->GetTimeSeconds()-At>4)
  {
   Test->TestTrue(TEXT("After native load/wave settling, player remains safely onshore"),Player->GetCharacterMovement()->IsMovingOnGround()
    && !Based() && FVector::Dist2D(Player->GetActorLocation(),Expected->Player.Transform.GetLocation())<30);
   Test->TestTrue(TEXT("Loaded boat retains native original-berth mooring"),Boat->GetMarineMovement()->IsMoored()
    && FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())<150);
   if(!WriteManifest(Expected.Get()))return Finish(false,TEXT("Session B evidence could not be written."));
   return Finish(!Test->HasAnyErrors(),TEXT(""));
  }
  return false;
 }
 const FNeonCityMarineSaveData* FindBoat(const UNeonCitySaveGame* Model)const
 {return Model?Model->Marine.FindByPredicate([](const auto& R){return R.VehicleID==UMarineWorldSubsystem::HarborlineID();}):nullptr;}
 bool Next(int32 Value){Stage=Value;At=W->GetTimeSeconds();Test->AddInfo(FString::Printf(TEXT("BOAT_FINAL session=%s stage=%d waypoint=%d"),*Mode,Stage,Waypoint));return false;}
 bool Based()const{const auto* Base=Cast<UPrimitiveComponent>(Player->GetMovementBaseObject());return Base && Base->GetOwner()==Boat && Player->GetCharacterMovement()->IsMovingOnGround();}
 void Focus(FVector Point){FVector Eye;FRotator R;Player->GetActorEyesViewPoint(Eye,R);PC->SetControlRotation((Point-Eye).Rotation());}
 float ApproachInput(FVector Target,float Maximum,const TCHAR* Step)
 {
  const float Distance=FVector::Dist2D(Target,Player->GetActorLocation());
  const float DT=FMath::Max(0.f,W->GetDeltaSeconds()),Speed=Player->GetCharacterMovement()->GetMaxSpeed();
  const float Analog=FMath::Min(Maximum,Distance/FMath::Max(120.f,Speed*DT*3.f));
  if(Stage!=LastApproachStage || W->GetTimeSeconds()-LastApproachLog>=1)
  {
   Test->AddInfo(FString::Printf(TEXT("BOAT_FINAL_NATIVE_APPROACH session=%s stage=%d step=%s game_delta=%.6f target=%s player=%s distance_cm=%.3f max_speed_cm_s=%.3f analog=%.6f"),
    *Mode,Stage,Step,DT,*Target.ToString(),*Player->GetActorLocation().ToString(),Distance,Speed,Analog));LastApproachStage=Stage;LastApproachLog=W->GetTimeSeconds();
  }
  return Analog;
 }
 void SetInput(float T,float S,float B){Throttle=T;Steer=S;Brake=B;Boat->SubmitDriverInput_Implementation(PC,T,S,B);}
 void ReleaseInput()
 {
  if(IsValid(PC)){Key(PC,EKeys::F,IE_Released);Key(PC,EKeys::SpaceBar,IE_Released);}
  if(IsValid(Boat) && IsValid(PC) && PC->GetPawn()==Boat)Boat->SubmitDriverInput_Implementation(PC,0,0,0);
 }
 void Observe()
 {
  const FVector Here=Boat->GetActorLocation();const FRotator R=Boat->GetActorRotation();
  if(Mode==TEXT("A"))
  {
   if(Stage>=8 && Stage<=10){Travel+=FVector::Dist2D(Here,LastBoat);HeadingTravel+=FMath::Abs(FMath::FindDeltaAngleDegrees(PreviousHeading,R.Yaw));}
   LastBoat=Here;PreviousHeading=R.Yaw;PeakSpeed=FMath::Max(PeakSpeed,float(Boat->GetVelocity().Size2D()));
   HeightMinimum=FMath::Min(HeightMinimum,Here.Z);HeightMaximum=FMath::Max(HeightMaximum,Here.Z);
   RollMinimum=FMath::Min(RollMinimum,double(R.Roll));RollMaximum=FMath::Max(RollMaximum,double(R.Roll));
  }
  const double Now=W->GetTimeSeconds();if(Now-LastSample<.1)return;LastSample=Now;const FVector P=Player->GetActorLocation();
  CSV+=FString::Printf(TEXT("%.6f,%.6f,%d,%d,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%d,%d,%d\n"),
   Now,FPlatformTime::Seconds()-Started,Stage,Waypoint,Here.X,Here.Y,Here.Z,R.Pitch,R.Yaw,R.Roll,Boat->GetVelocity().Size2D(),Throttle,Steer,Brake,P.X,P.Y,P.Z,int32(Player->GetCharacterMovement()->MovementMode),Based()?1:0,Boat->GetMarineMovement()->IsMoored()?1:0);
  if(CSV.Len()>32000)Flush();
 }
 void Flush()
 {if(CSV.IsEmpty() || Directory.IsEmpty())return;FFileHelper::SaveStringToFile(CSV,*(Directory/(TEXT("Trajectory-")+Mode+TEXT(".csv"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);CSV.Reset();}
 bool WriteManifest(const UNeonCitySaveGame* Model)
 {
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("processId"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("saveId"),Model->SaveID.ToString());
  J->SetStringField(TEXT("slot"),Slot);J->SetStringField(TEXT("vehicleId"),Boat->VehicleID.ToString());J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());
  J->SetBoolField(TEXT("boatTransformSetByHarness"),false);J->SetBoolField(TEXT("nativeCrossProcessLoad"),Mode==TEXT("B"));
  J->SetBoolField(TEXT("initialPlayerDockFixtureOnly"),Mode==TEXT("A"));J->SetBoolField(TEXT("continuousGpuReadback"),false);J->SetBoolField(TEXT("objectiveVisualInspectionCertified"),false);
  J->SetBoolField(TEXT("shoreDiagnosticInitialPlayerFixture"),Mode==TEXT("S"));J->SetBoolField(TEXT("canCertifyContinuousVoyage"),Mode==TEXT("A"));
  J->SetStringField(TEXT("validationScope"),Mode==TEXT("S")?TEXT("FIX-SPECIFIC static dry-shore native save; no fresh voyage claim"):TEXT("Native boat save / cross-process restore"));
  J->SetNumberField(TEXT("travelCm"),Travel);J->SetNumberField(TEXT("requiredTravelCm"),50000);J->SetNumberField(TEXT("headingTravelDegrees"),HeadingTravel);J->SetNumberField(TEXT("peakSpeedCmS"),PeakSpeed);
  J->SetNumberField(TEXT("routeWaypointCount"),Route.Num());J->SetStringField(TEXT("pilotModel"),TEXT("native-input polyline lookahead; flowing broad return turn; aligned straight approach; original dock gate"));
  J->SetNumberField(TEXT("rollMinimum"),RollMinimum);J->SetNumberField(TEXT("rollMaximum"),RollMaximum);J->SetNumberField(TEXT("heaveRangeCm"),HeightMaximum-HeightMinimum);
  J->SetNumberField(TEXT("nativeOceanQueriesAdvanced"),Mode==TEXT("A") && IsValid(Ocean)?double(Ocean->GetQuerySequence()-InitialOceanQuery):0);
  J->SetNumberField(TEXT("fishCount"),Model->Fishing.Fish.Num());J->SetNumberField(TEXT("mealCount"),Model->Fishing.Meals.Num());J->SetNumberField(TEXT("landVehicleCount"),Model->Vehicles.Num());
  J->SetStringField(TEXT("nativeInputSlot"),LegacyInputSlot);J->SetStringField(TEXT("nativeInputSaveId"),InputSaveID);J->SetStringField(TEXT("nativeInputLoadDetail"),InputLoadDetail);
  J->SetBoolField(TEXT("fullNativeInputLoad"),true);J->SetBoolField(TEXT("inventoryGrantedByHarness"),false);
  J->SetStringField(TEXT("landVehicleIdentities"),VehicleIdentities(Model->Vehicles));J->SetStringField(TEXT("marineIdentities"),MarineIdentities(Model->Marine));
  J->SetStringField(TEXT("fishValues"),Values(Model->Fishing.Fish));J->SetStringField(TEXT("mealValues"),Values(Model->Fishing.Meals));J->SetStringField(TEXT("propertyValues"),Values(Model->Properties));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*(Directory/(TEXT("Session")+(Mode==TEXT("S")?TEXT("A"):Mode)+TEXT(".json"))));
 }
 bool Finish(bool Passed,const FString& Error)
 {
  if(!Error.IsEmpty())Test->AddError(Error);Flush();ReleaseInput();
  if(!Directory.IsEmpty())
  {
   auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed && !Test->HasAnyErrors());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);
   J->SetNumberField(TEXT("waypoint"),Waypoint);J->SetBoolField(TEXT("boatTransformSetByHarness"),Mode==TEXT("D"));J->SetNumberField(TEXT("travelCm"),Travel);
   J->SetBoolField(TEXT("dockDiagnosticInitialFixture"),Mode==TEXT("D"));J->SetBoolField(TEXT("shoreDiagnosticInitialPlayerFixture"),Mode==TEXT("S"));J->SetBoolField(TEXT("canCertifyContinuousVoyage"),Mode==TEXT("A"));
   J->SetStringField(TEXT("status"),Passed && !Test->HasAnyErrors()?TEXT("AUTOMATED GAMEPLAY PASS; USER HANDLING REVIEW REQUIRED"):TEXT("FAIL"));
   FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/(TEXT("Receipt-")+Mode+TEXT(".json"))));
  }
  return true;
 }
 FAutomationTestBase* Test;FString Mode,RunID,Slot,Directory,CSV,InitialFish,InitialMeals,InitialProperties,InitialMarineIdentities,InitialVehicleIdentities,InputSaveID,InputLoadDetail;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityFishingBoat* Boat=nullptr;
 ANeonCityCoastalRegion* Coast=nullptr;ACoastalOceanSurface* Ocean=nullptr;UNeonCitySaveSubsystem* Save=nullptr;TStrongObjectPtr<UNeonCitySaveGame> Expected,InputModel;
 FVector Origin=FVector::ZeroVector,LastBoat=FVector::ZeroVector;FTransform Berth;TArray<FVector> Route;
 int32 Stage=0,PierWaypoint=0,Waypoint=0,ShoreWaypoint=0,InitialVehicleCount=0;double Started=0,LastSample=0,Travel=0,HeadingTravel=0;
 double HeightMinimum=0,HeightMaximum=0,RollMinimum=0,RollMaximum=0;float At=0,PreviousHeading=0,PeakSpeed=0,Throttle=0,Steer=0,Brake=0,InitialCondition=0,JumpStarted=0;
 bool SawSwimming=false,InputLoadRequested=false;double InputLoadStarted=0;int32 LastApproachStage=MIN_int32,LastPilotWaypoint=-1;float LastApproachLog=-1,LastPilotLog=-1;
 uint64 InitialOceanQuery=0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandBoatBerthRecovery,"NeonCity.IslandLand.BoatBerthRecoveryPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandBoatBerthRecovery::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityLandBoatFinal::FBoatFinal>(this,TEXT("D"),TEXT("DockFixture")));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandBoatDryShoreSave,"NeonCity.IslandLand.BoatDryShoreSavePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandBoatDryShoreSave::RunTest(const FString&)
{
 FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
 if(Run.IsEmpty() || Run.Len()>64-NeonCityLandBoatFinal::OutputSlotPrefix.Len()){AddError(TEXT("Unique short art run label required for native shore-save slot"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityLandBoatFinal::FBoatFinal>(this,TEXT("S"),Run));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FIslandLandBoatFinalCrossSession,"NeonCity.IslandLand.BoatFinalCrossSession",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FIslandLandBoatFinalCrossSession::GetTests(TArray<FString>& Names,TArray<FString>& Commands)const
{
 FString Mode;FParse::Value(FCommandLine::Get(),TEXT("NeonCityLandBoatSession="),Mode);
 if(Mode==TEXT("A") || Mode==TEXT("B")){Names.Add(Mode);Commands.Add(Mode);}
}
bool FIslandLandBoatFinalCrossSession::RunTest(const FString& Parameters)
{
 FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityLandBoatRun="),Run);if(Run.IsEmpty())Run=TEXT("Final15");
 if(Run.Len()>64-NeonCityLandBoatFinal::OutputSlotPrefix.Len() || Run.IsEmpty())
 {AddError(TEXT("NeonCityLandBoatRun plus native output slot prefix must fit the 64-character save-slot limit."));return false;}
 for(const TCHAR C:Run)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-')){AddError(TEXT("NeonCityLandBoatRun must contain only letters, digits, underscore or hyphen."));return false;}
 if(Parameters!=TEXT("A") && Parameters!=TEXT("B")){AddError(TEXT("Use NeonCityLandBoatSession=A or B in two separate processes."));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityLandBoatFinal::FBoatFinal>(this,Parameters,Run));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
