#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Vehicles/Marine/ReferenceBoatHydrodynamics.h"
#include "Vehicles/Marine/ReferenceBoatGeometry.h"
#include "Vehicles/Marine/NeonCityFishingBoat.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineWaterEffectsComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/FishingStation.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/AutomationEditorCommon.h"
#include "InputKeyEventArgs.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Water/CoastalOceanSurface.h"
#include "Physics/NeonCitySurfaceMovementComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/UnrealType.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceBoatHydro,"NeonCity.Marine.ReferenceHydrodynamics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceBoatHydro::RunTest(const FString&)
{
 using namespace NeonCityReferenceBoat;
 FString Text;TSharedPtr<FJsonObject> Data;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/Boat/HydrodynamicsReference.json")))
  || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || !Data.IsValid())return false;
 TestEqual(TEXT("Locked source"),Data->GetStringField(TEXT("commit")),FString(TEXT("4811ba48d795197de5621985f404e765c0b7c0ef")));
 const auto& Cases=Data->GetArrayField(TEXT("cases"));TestEqual(TEXT("Source trajectories"),Cases.Num(),14);
 for(const auto& Value:Cases)
 {
  const auto C=Value->AsObject();const FString Label=C->GetStringField(TEXT("label"));
  auto Vector=[&](const TCHAR* Key) { const auto& A=C->GetArrayField(Key);return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()); };
  FHydroState S;S.Velocity=Vector(TEXT("velocity"));S.Angular=Vector(TEXT("angular"));
  const auto& Q=C->GetArrayField(TEXT("rotation"));S.Rotation=FQuat(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber());
  S.RPM=C->GetNumberField(TEXT("rpm"));S.Throttle=C->GetNumberField(TEXT("throttle"));S.Steering=C->GetNumberField(TEXT("steer"));S.Driven=C->GetBoolField(TEXT("driven"));
  S.WaterHeight.Init(0,UE_ARRAY_COUNT(HullSamples));S.WaterVerticalVelocity.Init(0,UE_ARRAY_COUNT(HullSamples));
  const auto R=EvaluateHydrodynamics(S);
  TestTrue(Label+TEXT(" linear acceleration"),R.Acceleration.Equals(Vector(TEXT("acceleration")),1e-7));
  TestTrue(Label+TEXT(" angular acceleration"),R.AngularAcceleration.Equals(Vector(TEXT("angular_acceleration")),1e-7));
  TestTrue(Label+TEXT(" immersion"),FMath::Abs(R.WetFraction-C->GetNumberField(TEXT("wet")))<1e-9);
  TestTrue(Label+TEXT(" propeller thrust"),FMath::Abs(R.Thrust-C->GetNumberField(TEXT("thrust")))<1e-7);
 }
 const FVector Source(2,3,4);TestTrue(TEXT("Coordinate round trip"),ToSourcePolar(ToUEPolar(Source)).Equals(Source));
 TestTrue(TEXT("Axial handedness"),ToSourceAxial(ToUEAxial(Source)).Equals(Source));
 TestTrue(TEXT("Reference data valid"),GetDefault<UReferenceFishingBoatData>()->IsValidProfile());
 return true;
}
namespace
{
void BoatKey(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }
template<typename T> FString BoatRetentionValues(const TArray<T>& Records)
{
 TArray<FString> Rows;
 for(const auto& R:Records){FString Text;T::StaticStruct()->ExportText(Text,&R,nullptr,nullptr,PPF_None,nullptr);Rows.Add(MoveTemp(Text));}
 Rows.Sort();return FString::Join(Rows,TEXT("\n"));
}
TSharedRef<FJsonObject> BoatCatchProvenance(const FNeonCityCaughtFish& Fish)
{
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("fishId"),Fish.FishID.ToString());J->SetStringField(TEXT("speciesId"),Fish.SpeciesID.ToString());
 J->SetStringField(TEXT("fishingPointId"),Fish.FishingPointID.ToString());J->SetStringField(TEXT("caughtUtc"),Fish.CaughtAt.ToIso8601());
 J->SetStringField(TEXT("caughtUtcTicks"),LexToString(Fish.CaughtAt.GetTicks()));J->SetNumberField(TEXT("weightKg"),Fish.WeightKg);J->SetNumberField(TEXT("sizeCm"),Fish.SizeCm);
 J->SetNumberField(TEXT("storage"),int32(Fish.Storage));J->SetStringField(TEXT("containerId"),Fish.ContainerID.ToString());
 FString Value;FNeonCityCaughtFish::StaticStruct()->ExportText(Value,&Fish,nullptr,nullptr,PPF_None,nullptr);J->SetStringField(TEXT("completeNativeValue"),Value);return J;
}
FString BoatCatchValue(const FNeonCityCaughtFish& Fish)
{FString Text;FNeonCityCaughtFish::StaticStruct()->ExportText(Text,&Fish,nullptr,nullptr,PPF_None,nullptr);return Text;}
// Opt-in inspection of the actual gameplay wake, not a substitute simulated fixture.
// Raw half-float metres/slopes/foam let a rejected appearance be diagnosed without
// increasing wave strength to make a screenshot look more dramatic.
void SaveGameplayWakeFields(UWorld* World)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return;
 FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("WakeFields");
 IFileManager::Get().MakeDirectory(*Folder,true);
 for(TActorIterator<ACoastalOceanSurface> It(World);It;++It)
  for(const TCHAR* Name:{TEXT("WakeDisplay"),TEXT("WakeAeration")})
   if(const auto* Property=FindFProperty<FObjectProperty>(It->GetClass(),Name))
    if(auto* Texture=Cast<UTextureRenderTarget2D>(Property->GetObjectPropertyValue_InContainer(*It)))
    {
     TArray<FFloat16Color> Pixels;
     if(Texture->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels))
     {
      const TArrayView<const uint8> Bytes(reinterpret_cast<const uint8*>(Pixels.GetData()),Pixels.Num()*sizeof(FFloat16Color));
      FFileHelper::SaveArrayToFile(Bytes,*(Folder/(FString(Name)+TEXT("-512x512-rgba16f.bin"))));
     }
    }
}
class FReferenceBoatLoop : public IAutomationLatentCommand
{
public:
 explicit FReferenceBoatLoop(FAutomationTestBase* In,bool WakeOnly=false):Test(In),bWakeReview(WakeOnly) {}
 ~FReferenceBoatLoop()
 {
  if(!TelemetryRows.IsEmpty())FFileHelper::SaveStringToFile(TelemetryRows,*TelemetryFile);
  if(!ApproachTelemetryRows.IsEmpty())FFileHelper::SaveStringToFile(ApproachTelemetryRows,*ApproachTelemetryFile);
  if(ReflectionAuditPaused)
  {
   if(IsValid(PC))PC->SetPause(false);
   if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.Reflection")))C->Set(ReflectionAuditOriginal,ECVF_SetByConsole);
  }
 }
 bool Next(int32 N) { Stage=N;At=W->GetTimeSeconds();ApproachTargetValid=false;return false; }
 bool Based() const { const auto* Base=Cast<UPrimitiveComponent>(Player->GetMovementBaseObject());return Base && Base->GetOwner()==Boat; }
 void Focus(FVector Point) { FVector Eye;FRotator R;Player->GetActorEyesViewPoint(Eye,R);PC->SetControlRotation((Point-Eye).Rotation()); }
 float ApproachInput(FVector Target,float Maximum,const TCHAR* Step)
 {
  const float Distance=FVector::Dist2D(Target,Player->GetActorLocation());
  const float DT=FMath::Max(0.f,W->GetDeltaSeconds()),Speed=Player->GetCharacterMovement()->GetMaxSpeed();
  const float Analog=FMath::Min(Maximum,Distance/FMath::Max(120.f,Speed*DT*3.f));
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityBoatTelemetry")))
  {ApproachTarget=Target;ApproachAnalog=Analog;ApproachTargetValid=true;ApproachInputThisUpdate=true;}
  if(Stage!=LastApproachStage || W->GetTimeSeconds()-LastApproachLog>=1)
  {
   Test->AddInfo(FString::Printf(TEXT("BOAT_NATIVE_APPROACH stage=%d step=%s game_delta=%.6f target=%s player=%s distance_cm=%.3f max_speed_cm_s=%.3f analog=%.6f"),
    Stage,Step,DT,*Target.ToString(),*Player->GetActorLocation().ToString(),Distance,Speed,Analog));LastApproachStage=Stage;LastApproachLog=W->GetTimeSeconds();
  }
  return Analog;
 }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240) { Test->AddError(FString::Printf(TEXT("Reference boat stage %d timeout"),Stage));return true; }
  if(!Stage)
  {
   W=GEditor?GEditor->PlayWorld:nullptr;PC=W?W->GetFirstPlayerController():nullptr;Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
   if(!Player || W->GetTimeSeconds()<2)return false;
   for(TActorIterator<ANeonCityFishingBoat> It(W);It;++It)if(It->VehicleID==UMarineWorldSubsystem::HarborlineID())Boat=*It;
   if(!Test->TestNotNull(TEXT("Working boat exists in the ordinary South Beach world"),Boat))return true;
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityKeepLandBoatCatch")))
   {
    auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
    if(!RetainedInputLoadRequested)
    {
     if(!Test->TestTrue(TEXT("Dedicated byte-exact real-catch input slot selected"),Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_Legacy15"),false) && Save->DoesSaveExist()))return true;
     RetainedInputModel.Reset(Save->ReadSaveForInspection());
     if(!Test->TestTrue(TEXT("Original real-catch native save is readable"),RetainedInputModel.IsValid() && RetainedInputModel->SaveID.IsValid()
      && !RetainedInputModel->Fishing.Fish.IsEmpty()))return true;
     RetainedInputLoadRequested=true;
     if(!Test->TestTrue(TEXT("Retained real ocean fish loaded before the initial dock fixture"),Save->LoadGame())){Test->AddError(Save->GetLastDetail());return true;}
     return false;
    }
    if(Save->IsOperationInProgress())return false;
    if(!Test->TestTrue(TEXT("Native real-catch input load completed"),Save->GetLastResult()==ENeonCitySaveResult::Success)){Test->AddError(Save->GetLastDetail());return true;}
    auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
    if(!Test->TestTrue(TEXT("Original real catch is present before another native catch"),Inventory->GetFish().Num()>=1))return true;
    RetainedFishRecords=Inventory->GetFish();RetainedMealValues=BoatRetentionValues(Inventory->GetMeals());
    if(!Test->TestTrue(TEXT("Original full fish and meal records survive native input load"),BoatRetentionValues(RetainedFishRecords)==BoatRetentionValues(RetainedInputModel->Fishing.Fish)
     && RetainedMealValues==BoatRetentionValues(RetainedInputModel->Fishing.Meals)))return true;
    for(const auto& Fish:RetainedFishRecords)RetainedFishIDs.Add(Fish.FishID);
    NativeCatchRunStartedUtc=FDateTime::UtcNow();
   }
   const auto* Coast=ANeonCityCoastalRegion::Find(W);
   if(!Test->TestNotNull(TEXT("Working boat uses the authored connected island berth"),Coast))return true;
   IslandOrigin=Coast->GetActorLocation();
   for(float Y:{-4300.f,-4500.f,-4700.f,-4900.f,-5100.f})
   {
    FHitResult Ground;FCollisionQueryParams Query(SCENE_QUERY_STAT(ReferenceBerthProbe),false,Player);
    Query.AddIgnoredActor(Boat);
    if(W->LineTraceSingleByChannel(Ground,IslandOrigin+FVector(6450,Y+1000,300),IslandOrigin+FVector(6450,Y+1000,-1800),ECC_Pawn,Query))
     Test->AddInfo(FString::Printf(TEXT("Berth sounding %.0f: floor %.1f (%s)"),Y,Ground.ImpactPoint.Z,*GetNameSafe(Ground.GetActor())));
    else Test->AddInfo(FString::Printf(TEXT("Berth sounding %.0f: depth exceeds 16.7m"),Y));
   }
   // Only start this focused fixture on the connected island pier. The real world boat is never
   // spawned/positioned by this test; access, swim and all following actions are native movement/input.
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(IslandOrigin+FVector(5500,-3510,330),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
   return Next(32);
  }
  const float E=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityBoatTelemetry")))
  {
   if(TelemetryFile.IsEmpty())
   {
    FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Folder,true);
    TelemetryFile=Folder/TEXT("boat-physics.csv");
    TelemetryRows=TEXT("game_seconds,wall_seconds,game_delta,stage,x,y,z,pitch,yaw,roll,vx,vy,vz,wx,wy,wz,physics_steps,last_physics_delta,max_physics_delta,ax,ay,az,alphax,alphay,alphaz,water_min,water_max,water_age,player_mode,player_based,gpu_ms,water_valid_count,water_issued_age,max_angular_acceleration,lease_issued_age,lease_footprint_m,lease_unavailable_count,lease_expired_count,lease_outside_count\n");
   }
   const FVector L=Boat->GetActorLocation(),V=Boat->GetVelocity(),Omega=Boat->GetHull()->GetPhysicsAngularVelocityInRadians();const FRotator R=Boat->GetActorRotation();
   const auto& D=Boat->GetMarineMovement()->GetReferenceStepDiagnostics();
   TelemetryRows+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%llu,%.7f,%.7f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%.4f,%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%d\n"),
    W->GetTimeSeconds(),FPlatformTime::Seconds()-Started,W->GetDeltaSeconds(),Stage,L.X,L.Y,L.Z,R.Pitch,R.Yaw,R.Roll,V.X,V.Y,V.Z,Omega.X,Omega.Y,Omega.Z,D.Steps,D.LastDelta,D.MaximumDelta,D.Acceleration.X,D.Acceleration.Y,D.Acceleration.Z,D.AngularAcceleration.X,D.AngularAcceleration.Y,D.AngularAcceleration.Z,D.MinimumWater,D.MaximumWater,D.MaximumSampleAge,int32(Player->GetCharacterMovement()->MovementMode),Based()?1:0,FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),D.ValidWaterSamples,D.MaximumIssuedAge,D.MaximumAngularAcceleration,D.MaximumLeaseAge,D.MaximumLeaseFootprint,D.UnavailableLeases,D.ExpiredLeases,D.OutsideLeases);
  }
  const int32 EntryStage=Stage;ApproachInputThisUpdate=false;
  ON_SCOPE_EXIT
  {if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityBoatTelemetry")))RecordApproachTelemetry(EntryStage);};
  if(Stage==32)
  {
   if(E<12)return false;
   const FTransform Berth=ANeonCityCoastalRegion::Find(W)->GetFishingBerth();
   Test->TestTrue(TEXT("Undriven boat remains tied alongside the authored pier under waves"),Boat->GetMarineMovement()->IsMoored()
    && FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation())<60
    && FMath::Abs(FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw))<6);
   Test->AddInfo(FString::Printf(TEXT("Berth after wave settling: offset %.1f cm, heading %.2f degrees"),
    FVector::Dist2D(Boat->GetActorLocation(),Berth.GetLocation()),Boat->GetActorRotation().Yaw));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-Moored.png"));return Next(30);
  }
  // Aim at the near gunwale, not through the open working deck toward the far cabin.
  if(Stage==30)
  {
   // Follow the real passage around the solid diving cabinet before approaching the gunwale.
   const FVector Approach[]={FVector(5780,-3750,310),FVector(6110,-3750,310),FVector(6110,-3510,310),FVector(6250,-3510,-180)};
   const FVector Target=IslandOrigin+Approach[PierWaypoint];
   if(FVector::Dist2D(Player->GetActorLocation(),Target)>15)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.6f,TEXT("pier")));
    if(E>14) { Test->AddError(FString::Printf(TEXT("Cannot reach working boat from connected island pier: player %s, target %s"),*Player->GetActorLocation().ToString(),*Target.ToString()));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-PierBlocked.png"));return true; }return false;
   }
   if(PierWaypoint<UE_ARRAY_COUNT(Approach)-1){++PierWaypoint;return false;}
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(31);
  }
  if(Stage==31)
  {
   const FVector Target=Boat->GetActorTransform().TransformPosition(FVector(-145,-195,25));
   if(FVector::Dist2D(Player->GetActorLocation(),Target)>15)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.5f,TEXT("gunwale")));
    if(E>10)
    {
     const auto* M=Player->GetCharacterMovement();
     Test->AddError(FString::Printf(TEXT("Near gunwale access obstructed: player %s target %s boat %s rotation %s mode%d velocity %s floor %s"),
      *Player->GetActorLocation().ToString(),*Target.ToString(),*Boat->GetActorLocation().ToString(),*Boat->GetActorRotation().ToString(),int32(M->MovementMode),*M->Velocity.ToString(),*GetNameSafe(M->CurrentFloor.HitResult.GetComponent())));
     FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-GunwaleBlocked.png"));return true;
    }return false;
   }
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(1);
  }
  if(Stage==1 && Player->GetActorLocation().Z<Boat->GetActorLocation().Z-65)
  {
   BoatKey(PC,EKeys::SpaceBar,IE_Pressed);
   if(E>15){Test->AddError(TEXT("Swimmer cannot surface beside island berth"));return true;}
   return false;
  }
  if(Stage==1)BoatKey(PC,EKeys::SpaceBar,IE_Released);
  if(Stage==1 && E>3.5f)Focus(Boat->GetActorTransform().TransformPosition(FVector(-145,-130,25)));
  if(Stage==1 && E>4)
  {
   Test->AddInfo(FString::Printf(TEXT("Boat equilibrium %s, rotation %s, mass %.2f"),*Boat->GetActorLocation().ToString(),*Boat->GetActorRotation().ToString(),Boat->GetHull()->GetMass()));
   FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(BoatGroundProbe),false,Boat);GroundQuery.AddIgnoredActor(Player);FHitResult Ground;
   if(W->LineTraceSingleByChannel(Ground,Boat->GetActorLocation()+FVector(0,0,500),Boat->GetActorLocation()-FVector(0,0,1500),ECC_Pawn,GroundQuery))
    Test->AddInfo(FString::Printf(TEXT("Ground under boat: %s / %s at %s"),*GetNameSafe(Ground.GetActor()),*GetNameSafe(Ground.GetComponent()),*Ground.ImpactPoint.ToString()));
   Test->AddInfo(FString::Printf(TEXT("COM %s, health %.1f"),*Boat->GetActorTransform().InverseTransformPosition(Boat->GetHull()->GetCenterOfMass()).ToString(),Boat->GetHealthComponent()->GetCurrentHealth()));
   Test->TestTrue(TEXT("Source displacement mass"),FMath::IsNearlyEqual(Boat->GetHull()->GetMass(),3204.f,1.f));
   FWaterSurfaceSample Surface;
   const bool HasWater=W->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Boat->GetActorLocation(),Surface);
   // The original 20 cm flotation error remains. Measure against the current
   // shared wave, not the old fixed -130 cm datum that assumed flat water.
   Test->TestTrue(TEXT("Reference boat stable relative to sampled water"),HasWater && FMath::Abs(Boat->GetActorLocation().Z-Surface.SurfacePoint.Z)<20 && FMath::Abs(Boat->GetActorRotation().Roll)<8);
   Test->AddInfo(FString::Printf(TEXT("Reference water height %.2f, relative hull error %.2f"),Surface.SurfacePoint.Z,Boat->GetActorLocation().Z-Surface.SurfacePoint.Z));
   auto* Interaction=Player->FindComponentByClass<UInteractionComponent>();
   if(!Test->TestTrue(TEXT("Water interaction detects working boat"),Interaction->RefreshFocusedInteractable()==Boat))
   {
    FVector Eye;FRotator Look;Player->GetActorEyesViewPoint(Eye,Look);FHitResult Hit;
    Interaction->DetectInteractable(Eye,Eye+Look.Vector()*Interaction->InteractionDistance,Hit);
    Test->AddInfo(FString::Printf(TEXT("Boarding eye %s look %s player %s mode %d eligible %d hit %s / %s at %s"),
     *Eye.ToString(),*Look.ToString(),*Player->GetActorLocation().ToString(),int32(Player->GetCharacterMovement()->MovementMode),
     Boat->CanInteract_Implementation(Player),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString()));
    Test->AddInfo(FString::Printf(TEXT("Boarding local %s velocity %s player health %.1f"),
     *Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString(),*Boat->GetVelocity().ToString(),Player->FindComponentByClass<UHealthComponent>()->GetCurrentHealth()));
    TArray<FOverlapResult> Overlaps;FCollisionQueryParams BoardingQuery(SCENE_QUERY_STAT(BoardingEvidence),false,Player);
    const auto* Capsule=Player->GetCapsuleComponent();
    const FVector BoardCenter=Boat->GetActorTransform().TransformPosition(Boat->GetProfile()->BoardingPoint)+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+3);
    W->OverlapMultiByChannel(Overlaps,BoardCenter,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),BoardingQuery);
    for(const auto& Overlap:Overlaps)if(Overlap.bBlockingHit)Test->AddInfo(FString::Printf(TEXT("Boarding obstruction %s/%s"),*GetNameSafe(Overlap.GetActor()),*GetNameSafe(Overlap.GetComponent())));
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-BoardingBlocked.png"));
    return true;
   }
   BoatKey(PC,EKeys::F,IE_Pressed);return Next(2);
  }
  if(Stage==2 && E>.7f)
  {
   BoatKey(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("Boarding retains character on the native moving base"),PC->GetPawn()==Player && Based()))
   { Test->AddInfo(FString::Printf(TEXT("Player %s mode %d; boat %s"),*Player->GetActorLocation().ToString(),int32(Player->GetCharacterMovement()->MovementMode),*Boat->GetActorLocation().ToString()));return true; }
   Health=Player->FindComponentByClass<UHealthComponent>()->GetCurrentHealth();return Next(3);
  }
  if(Stage==3)
  {
   const FVector Target=Boat->GetActorTransform().TransformPosition(FVector(-75,0,35));
   if(FVector::Dist2D(Target,Player->GetActorLocation())>12)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.25f,TEXT("helm")));
    if(E>5) { Test->AddError(TEXT("Cabin route blocked"));return true; }return false;
   }
   Player->GetCharacterMovement()->StopMovementImmediately();
   FVector HelmEye;FRotator HelmLook;Player->GetActorEyesViewPoint(HelmEye,HelmLook);FHitResult HelmHit;
   Test->TestTrue(TEXT("Level gaze through wheelhouse glass still selects helm"),Player->FindComponentByClass<UInteractionComponent>()->DetectInteractable(
    HelmEye,HelmEye+Boat->GetActorForwardVector()*300,HelmHit)==Boat);
   Focus(Boat->GetActorTransform().TransformPosition(FVector(110,55,120)));
   if(!Test->TestTrue(TEXT("Helm reachable from deck"),Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==Boat))return true;
   BoatKey(PC,EKeys::F,IE_Pressed);return Next(4);
  }
  if(Stage==4 && E>.4f)
  {
   BoatKey(PC,EKeys::F,IE_Released);if(!Test->TestTrue(TEXT("F takes helm"),PC->GetPawn()==Boat))return true;
   Test->TestFalse(TEXT("Taking helm releases the berth restraint before departure"),Boat->GetMarineMovement()->IsMoored());
   BoatKey(PC,EKeys::V,IE_Pressed);return Next(5);
  }
  if(Stage==5 && E>.4f)
  {
   BoatKey(PC,EKeys::V,IE_Released);Test->TestTrue(TEXT("V selects real helm eye"),Boat->IsHelmCamera());
   Test->TestTrue(TEXT("Camera remains inside authored wheelhouse"),Boat->GetMarineCamera()->GetComponentLocation().Equals(Boat->GetActorTransform().TransformPosition(Boat->GetProfile()->CameraRig.HelmEye),2));
   Test->TestTrue(TEXT("Driver head cannot occlude first person"),Boat->FindComponentByClass<UTransportOccupantComponent>()->GetDriverMesh()->bOwnerNoSee);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-Helm.png"));BoatKey(PC,EKeys::W,IE_Pressed);Origin=Boat->GetActorLocation();return Next(6);
  }
  if(Stage==6 && E>7)
  {
   const auto Telemetry=Boat->GetMarineMovement()->GetTelemetry();
   Test->AddInfo(FString::Printf(TEXT("Cruise %s / %s velocity %s, thrust %.3f target %.3f engine %d health %.1f"),*Boat->GetActorLocation().ToString(),*Boat->GetActorRotation().ToString(),*Boat->GetVelocity().ToString(),Telemetry.SignedThrust,Telemetry.TargetThrottle,Telemetry.EngineRunning,Boat->GetHealthComponent()->GetCurrentHealth()));
   Test->TestTrue(TEXT("Propeller drives simulated boat"),FVector::Dist2D(Origin,Boat->GetActorLocation())>500 && Boat->GetSpeed_Implementation()>300);
   const auto* Spray=Boat->FindComponentByClass<UMarineWaterEffectsComponent>();
   if(Spray)Test->AddInfo(FString::Printf(TEXT("Spray layers: drops=%d mist=%d ligaments=%d whitewater=%d sheets=%d"),Spray->GetSprayLayerBurstCount(0),Spray->GetSprayLayerBurstCount(1),Spray->GetSprayLayerBurstCount(2),Spray->GetSprayLayerBurstCount(3),Spray->GetSprayLayerBurstCount(4)));
   Test->TestTrue(TEXT("Requested stern-only presentation emits no cosmetic bow or side spray"),Spray && Spray->GetContactBurstCount()==0 && Spray->GetActiveSprayCount()==0);
   if(bWakeReview)
   {
    ReviewCamera=W->SpawnActor<ACameraActor>();PC->SetViewTarget(ReviewCamera);
    // Freeze no gameplay: only remove camera streaking from the inspection lens.
    ReviewCamera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;
    ReviewCamera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
    ReviewCamera->GetCameraComponent()->SetFieldOfView(65);return Next(70);
   }
   BoatKey(PC,EKeys::W,IE_Released);BoatKey(PC,EKeys::V,IE_Pressed);return Next(7);
  }
  if(Stage==7 && E>.3f)
  {
   BoatKey(PC,EKeys::V,IE_Released);Test->TestFalse(TEXT("V restores chase camera"),Boat->IsHelmCamera());
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-Chase.png"));BoatKey(PC,EKeys::F,IE_Pressed);return Next(8);
  }
  // Compare identical frozen geometry/light/simulation and camera. Earlier
  // moving-view comparisons confused a submerged camera with reflection noise.
  if(Stage>=7001 && Stage<=7003)
  {
   if(FPlatformTime::Seconds()-ReflectionAuditAt<1.0)return false;
   auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.Reflection"));
   const TCHAR* ReflectionFrames[]={TEXT("DIAGNOSTIC-Frozen-Lumen.png"),TEXT("DIAGNOSTIC-Frozen-SSR-Sky.png"),TEXT("DIAGNOSTIC-Frozen-Captures-Sky.png")};
   FNeonCityVisualEvidence::CaptureHUD(W,ReflectionFrames[Stage-7001]);
   if(Stage==7001){C->Set(3,ECVF_SetByConsole);Stage=7002;ReflectionAuditAt=FPlatformTime::Seconds();return false;}
   if(Stage==7002){C->Set(2,ECVF_SetByConsole);Stage=7003;ReflectionAuditAt=FPlatformTime::Seconds();return false;}
   C->Set(ReflectionAuditOriginal,ECVF_SetByConsole);PC->SetPause(false);ReflectionAuditPaused=false;ReflectionAuditDone=true;return Next(70);
  }
  if(Stage==70 || Stage==71)
  {
   const FVector Forward=Boat->GetActorForwardVector().GetSafeNormal2D(),Right=Boat->GetActorRightVector().GetSafeNormal2D();
   const bool Close=Stage==70 && E<4.5f;
   // Also inspect free-surface relief near the waterline: a high overhead
   // image cannot distinguish a raised divergent wave from pale aeration.
   const bool Grazing=Stage==70 && E<2.5f;
   const FVector FocusPoint=Boat->GetActorLocation()+Forward*(Grazing?-1000:Close?130:-1800);
   FVector Eye=Boat->GetActorLocation()-Forward*(Grazing?3300:Close?400:5300)+Right*(Grazing?1500:Close?900:2500)+FVector(0,0,Grazing?120:Close?230:2700);
   if(Grazing)for(TActorIterator<ACoastalOceanSurface> It(W);It;++It)
   {float Height;FVector Normal,Velocity;It->Sample(Eye,Height,Normal,Velocity);Eye.Z=FMath::Max(Eye.Z,double(Height)+150);}
   ReviewCamera->SetActorLocationAndRotation(Eye,(FocusPoint-Eye).Rotation());
   if(Grazing && E>.7f && !WakeGrazingShot){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Grazing.png"));WakeGrazingShot=true;}
   if(Grazing && E>1.2f && !ReflectionAuditDone && FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.Reflection")))
    {
     Test->AddInfo(FString::Printf(TEXT("Frozen reflection comparison eye %s"),*Eye.ToString()));
     ReflectionAuditOriginal=C->GetInt();C->Set(1,ECVF_SetByConsole);PC->SetPause(true);
     ReflectionAuditPaused=true;ReflectionAuditAt=FPlatformTime::Seconds();return Next(7001);
    }
   if(Close && E>2.9f && !BowShot){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Bow-Close.png"));BowShot=true;}
   if(Close && E>4 && !BowWaveShot){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Bow-NextWave.png"));BowWaveShot=true;}
   if(Stage==70 && E>5.5f)
   { FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Straight.png"));SaveGameplayWakeFields(W);TurnHeading=Boat->GetActorRotation().Yaw;BoatKey(PC,EKeys::D,IE_Pressed);return Next(71); }
   if(Stage==71 && E>5)
   {
    Test->TestTrue(TEXT("Rudder bends the physical hull trajectory under sustained thrust"),FMath::Abs(FMath::FindDeltaAngleDegrees(TurnHeading,Boat->GetActorRotation().Yaw))>15);
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Turning.png"));BoatKey(PC,EKeys::D,IE_Released);BoatKey(PC,EKeys::W,IE_Released);BoatKey(PC,EKeys::SpaceBar,IE_Pressed);return Next(72);
   }
  }
  if(Stage==72 && E>8)
  { BoatKey(PC,EKeys::SpaceBar,IE_Released);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Decelerated.png"));return Next(73); }
  if(Stage==73 && E>14)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wake-Residual.png"));PC->SetViewTarget(Boat);ReviewCamera->Destroy();return true;
  }
  if(Stage==8 && E>.7f)
  {
   BoatKey(PC,EKeys::F,IE_Released);
   if(!Test->TestTrue(TEXT("Leave helm restores character on drifting deck"),PC->GetPawn()==Player && Based()))return true;
   Test->TestTrue(TEXT("Boat did not freeze on leaving helm"),Boat->GetSpeed_Implementation()>100);
   Local=Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation());Origin=Boat->GetActorLocation();return Next(9);
  }
  if(Stage==9 && E>5)
  {
   Test->TestTrue(TEXT("Five seconds carried by moving deck"),Based() && FVector::Dist2D(Origin,Boat->GetActorLocation())>150
    && FVector::Dist2D(Local,Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()))<30);
   Test->TestEqual(TEXT("Standing does not inflict vehicle impact damage"),Player->FindComponentByClass<UHealthComponent>()->GetCurrentHealth(),Health);
   return Next(11);
  }
  if(Stage==11)
  {
   const FVector Target=Boat->GetActorTransform().TransformPosition(FVector(-145,20,35));
   if(FVector::Dist2D(Target,Player->GetActorLocation())>12)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.2f,TEXT("fishing-station")));
    if(E>6) { Test->AddError(TEXT("Route to deck fishing blocked"));return true; }return false;
   }
   return Next(12);
  }
  if(Stage==12)
  {
   Player->AddMovementInput(Boat->GetActorRightVector(),.15f);
   if(E<.35f)return false;
   Player->GetCharacterMovement()->StopMovementImmediately();return Next(13);
  }
  if(Stage==13 && E>.25f)
  {
   auto* Station=Boat->GetDeckFishingStation();
   if(!Test->TestNotNull(TEXT("Stable-ID deck fishing point exists"),Station))return true;
   Focus(Station->InteractionBounds->GetComponentLocation());
   // Character eyes follow controller rotation on the next pawn tick.
   if(E<.5f)return false;
   if(!Test->TestTrue(TEXT("Moving deck rod interaction is reachable"),Player->FindComponentByClass<UInteractionComponent>()->RefreshFocusedInteractable()==Station))
   { Test->AddInfo(FString::Printf(TEXT("Deck angler local %s, facing %s, reach %d"),*Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString(),*Player->GetActorRotation().ToString(),Station->IsInReach(Player)));return true; }
   BoatKey(PC,EKeys::F,IE_Pressed);return Next(14);
  }
  if(Stage==14 && E>.3f)
  {
   BoatKey(PC,EKeys::F,IE_Released);Fishing=PC->FindComponentByClass<UFishingComponent>();
   if(!Test->TestTrue(TEXT("Deck fishing starts with ordinary F input"),Fishing && Fishing->IsActive()))return true;
   KeptBefore=Fishing->GetKeptCatchCount();BoatKey(PC,EKeys::LeftMouseButton,IE_Pressed);return Next(15);
  }
  if((Stage==15 || Stage==16) && E<2.f && E>=NextCastFrame)
  {
   NextCastFrame=E+.07f;
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Fishing-R3-%d-%03d.png"),Stage,CastFrame++));
   Test->AddInfo(FString::Printf(TEXT("CAST_FRAME stage=%d elapsed=%.3f pose=%d clip_time=%.3f ready=%.2f line=%.2f"),Stage,E,Fishing->GetCastPoseIndex(),Fishing->GetCastPoseTime(),Fishing->GetCastMotionCurve(TEXT("CastReady")),Fishing->GetCastMotionCurve(TEXT("LineReleased"))));
  }
  // Release uses the authored pose clock, as the native Release handler does.
  // The fixed rule-step clock can lag that marker by one step at low FPS.
  if(Stage==15 && Fishing->GetCastMotionCurve(TEXT("CastReady"))>=.99f
   && Fishing->GetCastPoseTime()>=Fishing->GetSession().WindupDuration*FNeonCityFishingSession::PerfectLow)
  {
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Fishing-R2-Windup.png"));
   BoatKey(PC,EKeys::LeftMouseButton,IE_Released);NextCastFrame=0;return Next(16);
  }
  if(Stage==15 && E>5)
  {
   const auto* M=Cast<UNeonCitySurfaceMovementComponent>(Player->GetCharacterMovement());
   Test->AddError(FString::Printf(TEXT("Deck windup did not reach release: active%d state%d power%.3f ready%.3f clip%.3f mode%d local%s boat%s release%s"),
    Fishing->IsActive(),int32(Fishing->GetSession().State),Fishing->GetSession().CastPower,Fishing->GetCastMotionCurve(TEXT("CastReady")),Fishing->GetCastPoseTime(),int32(M->MovementMode),
    *Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString(),*Boat->GetActorRotation().ToString(),*M->GetLastSurfaceReleaseDetails()));
   BoatKey(PC,EKeys::LeftMouseButton,IE_Released);return true;
  }
  if(Stage==16)
  {
   if(E>.32f && !FishingCastShot){FishingCastShot=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Fishing-R2-Cast.png"));Test->TestTrue(TEXT("Timed release selected perfect cast"),Fishing->GetSession().PerfectCast);}
   if(!Fishing->IsActive())
   {
    const auto* Move=Cast<UNeonCitySurfaceMovementComponent>(Player->GetCharacterMovement());
    Test->AddInfo(FString::Printf(TEXT("Deck cancellation: local=%s mode=%d releases=%d reason=%s"),*Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString(),int32(Move->MovementMode),Move->GetSurfaceReleaseCount(),*Move->GetLastSurfaceReleaseDetails()));
    Test->AddError(TEXT("Moving-deck fishing cancelled unexpectedly"));return true;
   }
   if(Fishing->GetSession().State==ENeonCityFishingState::Bite)
   { BoatKey(PC,EKeys::LeftMouseButton,IE_Pressed);bReel=true;return Next(17); }
   if(E>75) { Test->AddError(FString::Printf(TEXT("No deck bite from local ecology: %s"),*Fishing->GetDisplayText().ToString()));return true; }
  }
  if(Stage==17)
  {
   const auto& Session=Fishing->GetSession();
   if(E>1 && !FishingReelShot)
   {
    if(!ReviewCamera)
    {
     ReviewCamera=W->SpawnActor<ACameraActor>();ReviewCamera->GetCameraComponent()->SetFieldOfView(58);
     ReviewCamera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;
     ReviewCamera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
    }
    const FVector Look=Player->GetActorTransform().TransformPosition(FVector(38,13,36));
    const FVector AtCamera=Player->GetActorTransform().TransformPosition(FVector(90,-85,58));
    ReviewCamera->SetActorLocationAndRotation(AtCamera,(Look-AtCamera).Rotation());PC->SetViewTarget(ReviewCamera);
   }
   if(!Fishing->IsActive() || Session.State==ENeonCityFishingState::Escaped)
   {
    const auto* Move=Cast<UNeonCitySurfaceMovementComponent>(Player->GetCharacterMovement());
    Test->AddInfo(FString::Printf(TEXT("Deck fight cancellation: local=%s mode=%d releases=%d reason=%s"),*Boat->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString(),int32(Move->MovementMode),Move->GetSurfaceReleaseCount(),*Move->GetLastSurfaceReleaseDetails()));
    Test->AddError(FString::Printf(TEXT("Deck fight interrupted/lost: reason %d, tension %.3f, species %s weight %.3f"),int32(Session.Failure),Session.Tension,*Fishing->GetPendingCatch().SpeciesID.ToString(),Fishing->GetPendingCatch().WeightKg));return true;
   }
   if(Session.State==ENeonCityFishingState::Caught)
   {
    BoatKey(PC,EKeys::LeftMouseButton,IE_Released);Test->TestTrue(TEXT("Deck catch completed with native based movement"),Based());
    if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityKeepLandBoatCatch")))
    {
     NativeDeckCatch=Fishing->GetPendingCatch();NativeCaughtObserved=true;
     if(!Test->TestTrue(TEXT("Native Caught state exposes a second distinct real fish GUID/source/UTC/weight"),NativeDeckCatch.FishID.IsValid()
      && !RetainedFishIDs.Contains(NativeDeckCatch.FishID) && !NativeDeckCatch.SpeciesID.IsNone() && !NativeDeckCatch.FishingPointID.IsNone()
      && NativeDeckCatch.CaughtAt.GetTicks()>0 && NativeDeckCatch.CaughtAt>=NativeCatchRunStartedUtc
      && FMath::IsFinite(NativeDeckCatch.WeightKg) && NativeDeckCatch.WeightKg>0
      && FMath::IsFinite(NativeDeckCatch.SizeCm) && NativeDeckCatch.SizeCm>0))return true;
    }
    // Let the landed mesh and line reach the render thread before reviewing it.
    return Next(175);
   }
   const bool Reel=Session.Tension<.68f && (Session.Surge<.6f || Session.Tension<.35f);
   if(E>2.5f && !FishingReelShot){FishingReelShot=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Fishing-R2-Reel-Close.png"));}
   if(FishingReelShot && E>2.8f)PC->SetViewTarget(Player);
   if(Reel!=bReel) { BoatKey(PC,EKeys::LeftMouseButton,Reel?IE_Pressed:IE_Released);bReel=Reel; }
   if(E>70) { Test->AddError(TEXT("Deck fish fight timed out"));return true; }
  }
  if(Stage==175 && E>.8f)
  { FNeonCityVisualEvidence::CaptureHUD(W,TEXT("ReferenceBoat-DeckCatch.png"));BoatKey(PC,EKeys::One,IE_Pressed);return Next(18); }
  if(Stage==18 && E>.2f)
  {
   BoatKey(PC,EKeys::One,IE_Released);Test->TestEqual(TEXT("One caught fish kept"),Fishing->GetKeptCatchCount(),KeptBefore+1);
   BoatKey(PC,EKeys::F,IE_Pressed);return Next(19);
  }
  if(Stage==19 && E>.3f)
  {
   BoatKey(PC,EKeys::F,IE_Released);
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityKeepLandBoatCatch")))
   {
    if(!RetainNativeCatch())return true;
   }
   return Next(20);
  }
  if(Stage==20)
  {
   const FVector Target=Boat->GetActorTransform().TransformPosition(FVector(-75,0,35));
   if(FVector::Dist2D(Target,Player->GetActorLocation())>12)
   {
    Player->AddMovementInput((Target-Player->GetActorLocation()).GetSafeNormal2D(),ApproachInput(Target,.2f,TEXT("return-helm")));
    if(E>6) { Test->AddError(TEXT("Return to helm blocked"));return true; }return false;
   }
   Player->GetCharacterMovement()->StopMovementImmediately();Focus(Boat->GetActorTransform().TransformPosition(FVector(110,55,120)));
   BoatKey(PC,EKeys::F,IE_Pressed);return Next(10);
  }
  if(Stage==10 && E>.4f)
  {
   BoatKey(PC,EKeys::F,IE_Released);Test->TestTrue(TEXT("Return to moving helm"),PC->GetPawn()==Boat);
   return true;
  }
  return false;
 }
private:
 void RecordApproachTelemetry(int32 EntryStage)
 {
  if(!IsValid(W) || !IsValid(Boat) || !IsValid(Player) || TelemetryFile.IsEmpty())return;
  if(ApproachTelemetryFile.IsEmpty())
  {
   ApproachTelemetryFile=FPaths::GetPath(TelemetryFile)/TEXT("boat-player-approach.csv");
   ApproachTelemetryRows=TEXT("update_index,game_seconds,wall_seconds,game_delta,entry_stage,stage_after_update,player_x,player_y,player_z,player_vx,player_vy,player_vz,player_mode,player_based,floor_component,target_valid,approach_input_this_update,target_x,target_y,target_z,analog_cached,analog_used_this_update,target_distance_cm,player_speed_limit_cm_s,boat_x,boat_y,boat_z,boat_pitch,boat_yaw,boat_roll,boat_vx,boat_vy,boat_vz\n");
  }
  const FVector P=Player->GetActorLocation(),V=Player->GetVelocity(),B=Boat->GetActorLocation(),BV=Boat->GetVelocity();const FRotator R=Boat->GetActorRotation();
  const auto* Move=Player->GetCharacterMovement();const FString Floor=GetNameSafe(Move->CurrentFloor.HitResult.GetComponent());
  const float ActualAnalog=ApproachInputThisUpdate?ApproachAnalog:0.f;
  ApproachTelemetryRows+=FString::Printf(TEXT("%llu,%.6f,%.6f,%.6f,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%s,%d,%d,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n"),
   ApproachUpdateIndex++,W->GetTimeSeconds(),FPlatformTime::Seconds()-Started,W->GetDeltaSeconds(),EntryStage,Stage,P.X,P.Y,P.Z,V.X,V.Y,V.Z,int32(Move->MovementMode),Based()?1:0,*Floor,
   ApproachTargetValid?1:0,ApproachInputThisUpdate?1:0,ApproachTarget.X,ApproachTarget.Y,ApproachTarget.Z,ApproachAnalog,ActualAnalog,
   ApproachTargetValid?FVector::Dist2D(P,ApproachTarget):-1.,Move->GetMaxSpeed(),B.X,B.Y,B.Z,R.Pitch,R.Yaw,R.Roll,BV.X,BV.Y,BV.Z);
 }
 bool RetainNativeCatch()
 {
  auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
  if(!Test->TestTrue(TEXT("Native Keep produced exactly one additional real fish while original records coexist"),NativeCaughtObserved
   && Inventory->GetFish().Num()==RetainedFishRecords.Num()+1 && Fishing->GetKeptCatchCount()==KeptBefore+1))return false;
  const auto* Kept=Inventory->FindFish(NativeDeckCatch.FishID);
  if(!Test->TestTrue(TEXT("Native Keep retained the exact landed fish value and ecology GUID"),Kept && BoatCatchValue(*Kept)==BoatCatchValue(NativeDeckCatch)))return false;
  for(const auto& Old:RetainedFishRecords)
  {
   const auto* Live=Inventory->FindFish(Old.FishID);
   if(!Test->TestTrue(TEXT("Original real fish GUID and every persisted field remain unchanged"),Live && BoatCatchValue(*Live)==BoatCatchValue(Old)))return false;
  }
  const auto ExpectedFish=[&](){auto Records=RetainedFishRecords;Records.Add(NativeDeckCatch);return Records;}();
  if(!Test->TestTrue(TEXT("Native catch inventory has exact original-plus-new values and unchanged meals"),BoatRetentionValues(Inventory->GetFish())==BoatRetentionValues(ExpectedFish)
   && BoatRetentionValues(Inventory->GetMeals())==RetainedMealValues))return false;
  if(!Test->TestTrue(TEXT("Native catch saved only while player stands safely on the real deck after closing fishing"),Based()
   && Player->GetCharacterMovement()->IsMovingOnGround() && !Fishing->IsActive()))return false;
  auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
  if(!Test->TestTrue(TEXT("Dedicated native two-catch output selected"),Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false)))return false;
  if(!Test->TestTrue(TEXT("Two real catches retained in a dedicated native SaveGame"),Save->SaveGame())){Test->AddError(Save->GetLastDetail());return false;}
  TStrongObjectPtr<UNeonCitySaveGame> Disk(Save->ReadSaveForInspection());
  if(!Test->TestTrue(TEXT("Integrity-checked disk save retains every original/new real fish value and meal"),Disk.IsValid() && Disk->SaveID.IsValid()
   && BoatRetentionValues(Disk->Fishing.Fish)==BoatRetentionValues(ExpectedFish) && BoatRetentionValues(Disk->Fishing.Meals)==RetainedMealValues))return false;
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("REAL_NATIVE_DECK_CATCH_RETAINED; full boat/shore cross-process gates are separate"));
  J->SetBoolField(TEXT("nativeCaughtObserved"),NativeCaughtObserved);J->SetBoolField(TEXT("nativeKeepConfirmed"),true);J->SetBoolField(TEXT("nativeSaveReadbackConfirmed"),true);
  J->SetBoolField(TEXT("inventoryGrantedByHarness"),false);J->SetBoolField(TEXT("loadOnMovingDeckPerformed"),false);J->SetBoolField(TEXT("playerWasBasedAndGroundedAtSave"),true);
  J->SetBoolField(TEXT("referenceBoatLoopHasErrorsAtReceipt"),Test->HasAnyErrors());J->SetNumberField(TEXT("processId"),FPlatformProcess::GetCurrentProcessId());
  J->SetStringField(TEXT("inputSlot"),TEXT("NeonCity_Test_LandFinal_Legacy15"));J->SetStringField(TEXT("inputSaveId"),RetainedInputModel->SaveID.ToString());
  J->SetStringField(TEXT("outputSlot"),Save->GetSlotName());J->SetStringField(TEXT("outputSaveId"),Disk->SaveID.ToString());J->SetStringField(TEXT("savedUtc"),Disk->SavedAt.ToIso8601());
  J->SetStringField(TEXT("nativeFishingRunStartedUtc"),NativeCatchRunStartedUtc.ToIso8601());J->SetNumberField(TEXT("originalRealFishCount"),RetainedFishRecords.Num());
  J->SetObjectField(TEXT("newNativeCatch"),BoatCatchProvenance(NativeDeckCatch));
  TArray<TSharedPtr<FJsonValue>> OldValues;for(const auto& Old:RetainedFishRecords)OldValues.Add(MakeShared<FJsonValueObject>(BoatCatchProvenance(Old)));
  J->SetArrayField(TEXT("originalRealFish"),OldValues);J->SetStringField(TEXT("allDiskFishValues"),BoatRetentionValues(Disk->Fishing.Fish));J->SetStringField(TEXT("allDiskMealValues"),BoatRetentionValues(Disk->Fishing.Meals));
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);if(Label.IsEmpty())Label=TEXT("ReferenceBoat");
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label);IFileManager::Get().MakeDirectory(*Folder,true);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
  return Test->TestTrue(TEXT("Native real-catch provenance and disk readback receipt written"),FFileHelper::SaveStringToFile(Text,*(Folder/TEXT("native-boat-catch.json"))));
 }
 FString TelemetryFile,TelemetryRows;
 FString ApproachTelemetryFile,ApproachTelemetryRows;FVector ApproachTarget=FVector::ZeroVector;float ApproachAnalog=0;
 bool ApproachTargetValid=false,ApproachInputThisUpdate=false;uint64 ApproachUpdateIndex=0;
 TArray<FGuid> RetainedFishIDs;
 TArray<FNeonCityCaughtFish> RetainedFishRecords;FNeonCityCaughtFish NativeDeckCatch;FString RetainedMealValues;
 TStrongObjectPtr<UNeonCitySaveGame> RetainedInputModel;FDateTime NativeCatchRunStartedUtc;
 bool RetainedInputLoadRequested=false,NativeCaughtObserved=false;
 int32 LastApproachStage=MIN_int32;float LastApproachLog=-1;
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityFishingBoat* Boat=nullptr;
 int32 Stage=0,PierWaypoint=0;double Started=0;float At=0,Health=0;FVector Origin,Local,IslandOrigin;
 UFishingComponent* Fishing=nullptr;int32 KeptBefore=0;bool bReel=false;
 bool bWakeReview=false,BowShot=false,BowWaveShot=false,WakeGrazingShot=false,FishingCastShot=false,FishingReelShot=false;ACameraActor* ReviewCamera=nullptr;float TurnHeading=0;
 bool ReflectionAuditPaused=false,ReflectionAuditDone=false;int32 ReflectionAuditOriginal=0;double ReflectionAuditAt=0;
 float NextCastFrame=0;int32 CastFrame=0;
 FNeonCityVisualEvidence Evidence{TEXT("ReferenceBoat")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceBoatInput,"NeonCity.Marine.ReferenceBoatLoopPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceBoatInput::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReferenceBoatLoop>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceWakeVisual,"NeonCity.OceanExperience.BoatWakePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceWakeVisual::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReferenceBoatLoop>(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
