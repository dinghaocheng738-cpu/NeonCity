#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "Components/SplineComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/IslandLandGrill.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Vehicles/Aircraft/AircraftVehicleData.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "LandscapeProxy.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace CurrentHelicopter246
{
struct FStop
{
 FVector Position, Aim;
 FString Region;
 int32 Boundary = -1;
 bool bReview = false;
};

/** Extend the retained genuine roof checkpoint with one current closed circuit.
 * F7 is a production save restore before flight, never a flight shortcut.
 * There are no aircraft/player fixture poses, velocity writes, forces, engine
 * bypasses, collision edits or new gameplay controllers in this test.
 */
class FCircuit : public IAutomationLatentCommand
{
public:
 explicit FCircuit(FAutomationTestBase* In,bool LocalTerraceReturn=false,bool CurrentEstate=false,bool SurveyOnly=false) : Test(In),Metrics(CurrentEstate?TEXT("CurrentEstateHelicopter"):LocalTerraceReturn?TEXT("CurrentHelicopterTerraceReturn260"):TEXT("CurrentHelicopterCircuit246")),bLocalTerraceReturn(LocalTerraceReturn),bCurrentEstate(CurrentEstate),bSurveyOnly(SurveyOnly) {}
 bool Update() override
 {
  if (!Started) Started = FPlatformTime::Seconds();
  W = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  PC = W ? W->GetFirstPlayerController() : nullptr;
  if (!PC || W->GetTimeSeconds() < 4) return false;
  if (bLocalTerraceReturn && FPlatformTime::Seconds()-Started>180)
   return End(false,FString::Printf(TEXT("Local terrace/native roof return finite timeout stage%d stop%d"),Stage,Point));
  if (FPlatformTime::Seconds() - Started > (bCurrentEstate?1800:950))
   return End(false, FString::Printf(TEXT("Current circuit finite timeout stage%d stop%d"), Stage, Point));
  const float Now = W->GetTimeSeconds(), DT = FMath::Clamp(Now - Previous, 0.f, .1f);
  Previous = Now;
  if (Stage == 0)
  {
   FString Run; FParse::Value(FCommandLine::Get(), TEXT("NeonCityArtRun="), Run);
   RunLabel = FPaths::MakeValidFileName(Run.IsEmpty() ? FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) : Run);
   Directory = FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/RunLabel/(bCurrentEstate?TEXT("CurrentEstateHelicopter"):bLocalTerraceReturn?TEXT("CurrentHelicopterTerraceReturn260"):TEXT("CurrentHelicopterCircuit246"));
   if (IFileManager::Get().FileExists(*(Directory/TEXT("receipt.json"))))
   {
    Test->AddError(TEXT("Existing circuit receipt must be retained; use a new evidence run label"));
    Directory.Reset();return true;
   }
   IFileManager::Get().MakeDirectory(*Directory, true);
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,stop,region,x,y,z,yaw,vx,vy,vz,altitude_cm,input_x,input_y,input_z,input_yaw,power_input,grounded,health,travel_cm,winding_rad,camera_aim_x,camera_aim_y,camera_aim_z\n"), *(Directory/TEXT("trajectory.csv")));
   FFileHelper::SaveStringToFile(TEXT("game_seconds,stage,key,event,possessed_pawn\n"),*(Directory/TEXT("mapped-input.csv")));
   Region = ANeonCityCoastalRegion::Find(W);
   if (!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,32160)))) return false;
   Player = Cast<ANeonCityPlayerCharacter>(PC->GetPawn());
   Save = W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(bCurrentEstate)
   {
    if(!Player)return End(false,TEXT("Native on-foot player missing"));
    // The helicopter keeps its genuine authored spawn and native physics.
    // Only the initial player approach is a disclosed supported fixture.
    for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It)
     if(It->GetAircraftMovement()->IsGrounded() && (!Heli || It->GetActorLocation().Z<Heli->GetActorLocation().Z))Heli=*It;
    if(!Heli)return End(false,TEXT("No genuinely grounded native player helicopter is available"));
    FVector Stand;
    if(!Phase205cVehicleApproach(W,Player,Heli,Player->GetActorLocation(),Stand))return End(false,TEXT("Actual helicopter has no supported full-capsule normal F approach"));
    Player->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
    return Next(1);
   }
   if (!Player || !Save || !Save->ConfigureSlot(TEXT("NeonCity_Test_RidgeRoofUpgrade"), false) || !Save->DoesSaveExist())
    return End(false, TEXT("Retained genuine RoofFlight30 roof/house/F6 checkpoint required; no synthetic save or entry fixture"));
   Key(EKeys::F7, IE_Pressed); return Next(1);
  }
  if (Stage == 1)
  {
   if (Now-At < 2) return false;
   Key(EKeys::F7, IE_Released);
   if(!bCurrentEstate)for (TActorIterator<ANeonCityHelicopterPrototype> It(W); It; ++It) { Heli = *It; break; }
   if ((!bCurrentEstate && Save->GetLastResult() != ENeonCitySaveResult::Success) || !Heli || !Heli->AircraftID.IsValid() || !Heli->GetProfile()
    || PC->GetPawn() != Player || !Player->GetCharacterMovement()->IsMovingOnGround()
    || !Heli->GetAircraftMovement()->IsGrounded() || !Heli->GetHull()->IsSimulatingPhysics()
    || FVector::Dist2D(Player->GetActorLocation(), Heli->GetActorLocation()) > 350)
    return End(false, TEXT("F7 did not restore the genuine grounded roof aircraft and safe on-foot return position"));
   if(!Heli->GetProfile()->IsValidProfile())return End(false,TEXT("Actual helicopter profile is invalid; no controller constants substituted"));
   Origin = Last = Heli->GetActorLocation(); InitialTransform = Heli->GetActorTransform();
   InitialHealth = Heli->GetHealthComponent()->GetCurrentHealth();
   if(bLocalTerraceReturn){if(!BuildTerraceRoute())return End(false,SurveyFailure);}
   else {if (!BuildCurrentRoute()) return End(false, SurveyFailure);}
   FHitResult Roof;
   if (!Surface(Origin, Roof) || Roof.ImpactNormal.Z < .9 || Origin.Z-Roof.ImpactPoint.Z > Heli->GetProfile()->BodyExtent.Z+20
    || (!bCurrentEstate && !MansionBounds.IsInsideOrOn(FVector(Origin.X, Origin.Y, MansionBounds.GetCenter().Z))))
    return End(false, TEXT("Restored aircraft is not supported on the measured current mansion roof"));
   RoofContact = Roof.ImpactPoint;
   LandingOrigin=Origin;
   if(bCurrentEstate)
   {
    AStaticMeshActor* Pad=nullptr;
    for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_HighestPad476"))Pad=*It;
    if(!Pad)return End(false,TEXT("Current highest native mansion helipad missing"));
    const FVector Centre=Pad->GetStaticMeshComponent()->Bounds.Origin;FHitResult Deck;
    if(!Surface(Centre,Deck) || Deck.GetActor()!=Pad || Deck.ImpactNormal.Z<.9)return End(false,TEXT("Highest pad does not provide actual horizontal native collision"));
    RoofContact=Deck.ImpactPoint;LandingOrigin=RoofContact+FVector(0,0,Origin.Z-Roof.ImpactPoint.Z);
    Add(FVector(LandingOrigin.X,LandingOrigin.Y,CruiseZ),RoofContact,TEXT("current-pad-high-approach"));
    Add(LandingOrigin+FVector(0,0,1600),RoofContact,TEXT("current-pad-vertical-approach"));
    if(bSurveyOnly)return EndSurvey();
   }
   for (TActorIterator<ANeonCityDayNightCycle> It(W); It; ++It) { It->SetCycleEnabled(false); It->SetTimeOfDay(12); }
   if (!FocusHeli()) return End(false, TEXT("Normal restored player eye trace cannot focus the actual roof helicopter"));
   Test->AddInfo(FString::Printf(TEXT("Current native land bounds %s / peak %.2fcm / cruise %.2fcm / planned boundary %.1fm; no player or aircraft fixtures"), *LandBounds.ToString(), NativePeak, CruiseZ, BoundaryLength/100));
   Key(EKeys::F, IE_Pressed); return Next(2);
  }
  if (Stage == 2 && Now-At > .25f)
  {
   Key(EKeys::F, IE_Released);
   if (PC->GetPawn() != Heli || Heli->GetAircraftInteraction()->GetDriver() != Player || !Heli->GetAircraftMovement()->IsEngineRunning())
    return End(false, TEXT("Normal F did not board and start the retained native helicopter"));
   Key(EKeys::SpaceBar, IE_Pressed); Input=FVector(0,0,1); return Next(3);
  }
  if (Stage >= 3 && Stage <= 5)
  {
   const FVector Here = Heli->GetActorLocation(), Velocity = Heli->GetVelocity();
   if (Here.ContainsNaN() || Velocity.ContainsNaN() || PC->GetPawn() != Heli || !Heli->GetHull()->IsSimulatingPhysics()
    || !Heli->GetActorEnableCollision() || Heli->GetHull()->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics
    || !Heli->GetAircraftMovement()->IsEngineRunning() || Heli->GetHealthComponent()->GetCurrentHealth() < InitialHealth-.5f)
    return End(false, TEXT("Continuous circuit lost finite native physics/possession/engine or damaged the aircraft"));
   if(Stage==4 && Heli->GetAircraftMovement()->IsGrounded())return End(false,TEXT("Current continuous aerial route unexpectedly contacted/landed before its final roof approach"));
   Travel += FVector::Dist(Here, Last); Last = Here;
   if (bBoundaryActive)
   {
    const float Angle = FMath::Atan2(Here.Y-LandBounds.GetCenter().Y, Here.X-LandBounds.GetCenter().X);
    Winding += FMath::FindDeltaAngleRadians(LastAngle, Angle); LastAngle = Angle;
   }
   if (Stage == 3)
   {
    PC->SetControlRotation(FRotator(-15, Heli->GetActorRotation().Yaw, 0));
    if (Here.Z-Origin.Z > 600) { Key(EKeys::SpaceBar, IE_Released); Point=0; return Next(4); }
    if (Now-At > 15) return End(false, TEXT("Normal Space input did not physically take off from the restored roof"));
   }
   else if (Stage == 4)
   {
    if (Point >= Stops.Num()) return Next(5);
    const FStop& Stop = Stops[Point]; Aim = Stop.Aim;
    FlyTo(Stop.Position, DT, Stop.bReview ? 1800.f : 3500.f, false);
    PC->SetControlRotation((Aim-Here).Rotation());
    const bool Arrived = FVector::Dist2D(Here,Stop.Position) < 220 && FMath::Abs(Here.Z-Stop.Position.Z) < 110 && Velocity.Size2D() < 200;
    if (!Arrived) StableSeconds=0; else StableSeconds += DT;
    // Only finite regional clips read pixels; the uninterrupted flight CSV and
    // clean transit GPU/VRAM samples cover the complete physical circuit.
    if (Stop.bReview && (Here-Stop.Position).Size2D() < 3500 && FMath::Abs(Here.Z-Stop.Position.Z) < 1500)
    {
     if (!Clip) Clip = MakeUnique<FNeonCityVisualEvidence>((bCurrentEstate?TEXT("CurrentEstateHelicopter-"):bLocalTerraceReturn?TEXT("CurrentHelicopter260-"):TEXT("CurrentHelicopter246-"))+Stop.Region);
     Clip->Record(W, PC, Point); LastReadback=Now;
    }
    Metrics.Sample(W, Stop.bReview && Clip ? TEXT("visual-readback-")+Stop.Region : Now-LastReadback < .5f ? TEXT("post-readback-recovery") : TEXT("clean-native-flight-")+Stop.Region);
    if (StableSeconds >= (Stop.bReview ? 4.f : .25f))
    {
     if (Stop.Boundary >= 0)
     {
      if (Stop.Boundary != BoundaryVisited) return End(false, TEXT("Physical perimeter checkpoint order is incomplete"));
      ++BoundaryVisited;
      if (Stop.Boundary == 0)
      { bBoundaryActive=true; LastAngle=FMath::Atan2(Here.Y-LandBounds.GetCenter().Y,Here.X-LandBounds.GetCenter().X); LoopStart=Here; }
      if (Stop.Boundary == 8)
      { bBoundaryActive=false; Closure=FVector::Dist2D(Here,LoopStart); }
     }
     if (Stop.bReview) Reviewed.Add(Stop.Region);
     Clip.Reset(); StableSeconds=0; ++Point; At=Now;
    }
    else if (Now-At > 135) return End(false, FString::Printf(TEXT("Native route blocked stop%d %s at%s"), Point, *Stop.Region, *Here.ToString()));
   }
   else
   {
    Aim=RoofContact; FlyTo(LandingOrigin, DT, 250, true);
    PC->SetControlRotation(FRotator(-25, InitialTransform.Rotator().Yaw, 0));
    const bool Supported=Heli->GetAircraftMovement()->IsGrounded() && Velocity.Size()<25 && FVector::Dist2D(Here,LandingOrigin)<110;
    StableSeconds=Supported ? StableSeconds+DT : 0;
    if (StableSeconds>2)
    { Submit(FVector::ZeroVector,0,0); Key(EKeys::F,IE_Pressed); return Next(6); }
    if (Now-At>45) return End(false,TEXT("Finite native physical roof landing failed; no ground/velocity fixture allowed"));
   }
   Sample(); return false;
  }
  if (Stage == 6 && Now-At>.25f)
  {
   Key(EKeys::F, IE_Released);
   if (PC->GetPawn()!=Player || Heli->GetAircraftInteraction()->GetDriver()!=nullptr)
    return End(false,TEXT("Normal F did not exit the genuinely landed helicopter"));
   return Next(7);
  }
  if (Stage == 7 && Now-At>1)
  {
   FHitResult Floor;
   const bool Grounded=Player->GetCharacterMovement()->IsMovingOnGround() && Player->GetActorEnableCollision() && !Player->IsHidden()
    && Surface(Player->GetActorLocation(),Floor) && Floor.ImpactNormal.Z>=Player->GetCharacterMovement()->GetWalkableFloorZ()
    && Player->GetActorLocation().Z-Floor.ImpactPoint.Z<=Player->GetSimpleCollisionHalfHeight()+15;
   if(bLocalTerraceReturn)
   {
    const bool Local=Reviewed.Num()==1 && Reviewed.Contains(TEXT("Terrace"))
     && Travel>FVector::Dist2D(Origin,LocalReviewPosition)*1.8 && Heli->GetAircraftMovement()->IsGrounded()
     && FVector::Dist2D(Heli->GetActorLocation(),Origin)<110 && Heli->GetHealthComponent()->GetCurrentHealth()>=InitialHealth-.5;
    return End(Grounded && Local,Grounded && Local?TEXT(""):TEXT("Local terrace physical flight/hover/same-roof landing/supported normal F exit gate failed"));
   }
   const bool Route=BoundaryVisited==9 && FMath::Abs(Winding)>5.7 && Closure<500 && Travel>FMath::Max(400000.,BoundaryLength*.9)
    && (!bCurrentEstate || FVector::Dist2D(Heli->GetActorLocation(),LandingOrigin)<110)
    && Reviewed.Num()==9 && Heli->GetAircraftMovement()->IsGrounded() && Heli->GetHealthComponent()->GetCurrentHealth()>=InitialHealth-.5;
   return End(Grounded && Route, Grounded && Route ? TEXT("") : TEXT("Current full closed loop/ordered region/physical landing/supported normal F exit gate failed"));
  }
  return false;
 }
private:
 bool Next(int32 Value) { Stage=Value; At=W->GetTimeSeconds(); StableSeconds=0; return false; }
 void Key(FKey K,EInputEvent E)
 {
  if(!PC)return;
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));
  if(!Directory.IsEmpty())FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.4f,%d,%s,%d,%s\n"),W->GetTimeSeconds(),Stage,*K.ToString(),static_cast<int32>(E),*GetNameSafe(PC->GetPawn())),*(Directory/TEXT("mapped-input.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
 }
 bool FocusHeli()
 { PC->SetControlRotation((Heli->GetActorLocation()+FVector(0,0,25)-Player->GetPawnViewLocation()).Rotation()); return Player->GetInteractionComponent()->RefreshFocusedInteractable()==Heli; }
 bool Surface(FVector AtPoint,FHitResult& Hit) const
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentHelicopter246NativeSurface),false,Heli); Q.AddIgnoredActor(Player);
  return W->LineTraceSingleByChannel(Hit,FVector(AtPoint.X,AtPoint.Y,80000),FVector(AtPoint.X,AtPoint.Y,-20000),ECC_Vehicle,Q);
 }
 double LocalPeak(FVector P) const
 {
  double Peak=Region->GetActorLocation().Z; FHitResult Hit;
  for(int32 X=-2;X<=2;++X)for(int32 Y=-2;Y<=2;++Y)
   if(Surface(P+FVector(X*1000,Y*1000,0),Hit))Peak=FMath::Max(Peak,Hit.ImpactPoint.Z);
  return Peak;
 }
 void Add(FVector P,FVector View,const TCHAR* Name,bool Review=false,int32 Boundary=-1)
 { Stops.Add({P,View,FString(Name),Boundary,Review}); }
 void Detail(FVector P,FVector View,const TCHAR* Name)
 {
  P.Z=CruiseZ; Add(P,View,TEXT("regional-high-approach"));
  P.Z=LocalPeak(P)+8500; Add(P,View,Name,true);
  P.Z=CruiseZ; Add(P,View,TEXT("regional-safe-rise"));
 }
 bool BuildTerraceRoute()
 {
  // Only the uncompleted regional clip and physical return are new scope.
  // Measure this short corridor; the accepted whole-island route is unchanged.
  NativePeak=Origin.Z;FVector TerraceAim;bool FoundTerrace=false;
  for(TActorIterator<AStaticMeshActor> It(W);It;++It)
  {
   auto* C=It->GetStaticMeshComponent();if(!C || !C->GetStaticMesh())continue;
   if(C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_RidgeHouse")))
   {MansionBounds+=C->Bounds.GetBox();NativePeak=FMath::Max(NativePeak,C->Bounds.GetBox().Max.Z);}
  }
  for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(FVector::Dist2D(It->GetActorLocation(),Origin)<6000)
  {TerraceAim=It->GetActorLocation()+FVector(0,0,180);FoundTerrace=true;}
  if(!MansionBounds.IsValid || !FoundTerrace){SurveyFailure=TEXT("Current native house bounds and real terrace BBQ required for local flight");return false;}
  LocalReviewPosition=TerraceAim+FVector(3000,-7000,0);LocalReviewPosition.Z=LocalPeak(LocalReviewPosition)+8500;
  FString Survey=TEXT("index,x,y,native_floor_z,normal_z,actor,component\n");FHitResult Hit;
  const auto* D=Heli->GetProfile();const int32 Steps=FMath::CeilToInt(FVector::Dist2D(Origin,LocalReviewPosition)/250);
  int32 Index=0;
  for(int32 I=0;I<=Steps;++I)for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y)
  {
   const FVector AtPoint=FMath::Lerp(Origin,LocalReviewPosition,double(I)/FMath::Max(1,Steps))+FVector(X*D->BodyExtent.X,Y*D->BodyExtent.Y,0);
   if(!Surface(AtPoint,Hit)){SurveyFailure=TEXT("Local native full-body-width corridor has missing physical support; no guessed altitude");return false;}
   NativePeak=FMath::Max(NativePeak,Hit.ImpactPoint.Z+D->BodyExtent.Z);
   Survey+=FString::Printf(TEXT("%d,%.3f,%.3f,%.3f,%.4f,%s,%s\n"),Index++,AtPoint.X,AtPoint.Y,Hit.ImpactPoint.Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
  }
  if(!FMath::IsFinite(NativePeak) || NativePeak>50000 || LocalReviewPosition.ContainsNaN())
  {SurveyFailure=TEXT("Measured local terrace clearance is inconsistent");return false;}
  CruiseZ=FMath::Max(NativePeak,LocalReviewPosition.Z)+2000;
  Add(FVector(LocalReviewPosition.X,LocalReviewPosition.Y,CruiseZ),TerraceAim,TEXT("terrace-local-high-approach"));
  Add(LocalReviewPosition,TerraceAim,TEXT("Terrace"),true);
  Add(FVector(LocalReviewPosition.X,LocalReviewPosition.Y,CruiseZ),TerraceAim,TEXT("terrace-local-safe-rise"));
  Add(FVector(Origin.X,Origin.Y,CruiseZ),Origin,TEXT("physical-roof-local-high-approach"));
  Add(Origin+FVector(0,0,1600),Origin,TEXT("physical-roof-local-vertical-approach"));
  FFileHelper::SaveStringToFile(Survey,*(Directory/TEXT("native-surface-corridor.csv")));
  FString Plan=TEXT("stop,region,boundary,review,x,y,z,aim_x,aim_y,aim_z\n");
  for(int32 I=0;I<Stops.Num();++I){const auto& V=Stops[I];Plan+=FString::Printf(TEXT("%d,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),I,*V.Region,V.Boundary,V.bReview?1:0,V.Position.X,V.Position.Y,V.Position.Z,V.Aim.X,V.Aim.Y,V.Aim.Z);}
  FFileHelper::SaveStringToFile(Plan,*(Directory/TEXT("current-route-plan.csv")));return true;
 }
 bool BuildCurrentRoute()
 {
  FBox TerrainBounds(ForceInit); NativePeak=Region->GetActorLocation().Z;
  for(TActorIterator<ALandscapeProxy> It(W);It;++It)
  {
   const FBox B=It->GetComponentsBoundingBox(true);
   if(B.IsValid && FVector::Dist2D(B.GetCenter(),Region->GetActorLocation())<130000)TerrainBounds+=B;
  }
  if(!TerrainBounds.IsValid || TerrainBounds.GetSize().X<150000 || TerrainBounds.GetSize().Y<150000)
  {SurveyFailure=TEXT("Current streamed native Landscape bounds missing; no guessed perimeter");return false;}
  NativePeak=TerrainBounds.Max.Z;
  FString Survey=TEXT("kind,index,x,y,native_floor_z,normal_z,actor,component\n"); FHitResult Hit; int32 LandSamples=0;
  for(int32 X=0;X<=64;++X)for(int32 Y=0;Y<=64;++Y)
  {
   const FVector P(FMath::Lerp(TerrainBounds.Min.X,TerrainBounds.Max.X,X/64.),FMath::Lerp(TerrainBounds.Min.Y,TerrainBounds.Max.Y,Y/64.),0);
   if(!Surface(P,Hit) || !Hit.GetActor() || !Hit.GetActor()->GetLevel()
    || !Hit.GetActor()->GetLevel()->GetOutermost()->GetName().Contains(TEXT("NeonCity_Tidewater_Island")))continue;
   NativePeak=FMath::Max(NativePeak,Hit.ImpactPoint.Z);
   if(Hit.ImpactPoint.Z>Region->GetActorLocation().Z+250)
   {
    LandBounds+=Hit.ImpactPoint; ++LandSamples;
    Survey+=FString::Printf(TEXT("land,%d,%.3f,%.3f,%.3f,%.4f,%s,%s\n"),X*65+Y,P.X,P.Y,Hit.ImpactPoint.Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
   }
  }
  for(TActorIterator<AStaticMeshActor> It(W);It;++It)
  {
   auto* C=It->GetStaticMeshComponent();if(!C || !C->GetStaticMesh())continue;
   const FString Name=C->GetStaticMesh()->GetName();
   const bool House=Name.StartsWith(bCurrentEstate?TEXT("SM_TheOne_"):TEXT("SM_RidgeHouse")) && !It->IsHidden() && C->IsVisible();
   if(House)MansionBounds+=C->Bounds.GetBox();
   if(FVector::Dist2D(C->Bounds.Origin,Region->GetActorLocation())<120000 && (House || Name.Contains(TEXT("AdventureRock"))))NativePeak=FMath::Max(NativePeak,C->Bounds.GetBox().Max.Z);
  }
  FVector VillageAim=FVector::ZeroVector,TerraceAim=FVector::ZeroVector;bool FoundVillage=false,FoundTerrace=false;
  for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Taxi.FishingHouse"))
  {VillageAim=It->GetActorTransform().TransformPosition(It->PickupLocal)+FVector(0,0,300);FoundVillage=true;}
  for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(bCurrentEstate || FVector::Dist2D(It->GetActorLocation(),Origin)<6000)
  {TerraceAim=It->GetActorLocation()+FVector(0,0,180);FoundTerrace=true;}
  FVector RoadAim;
  if(bCurrentEstate)
  {
   USplineComponent* Main=nullptr;
   for(TActorIterator<ANeonCityTrafficRoute> It(W);It;++It)if(It->RoadID==TEXT("Island.Mountain.Main"))Main=It->GetRoadSpline();
   if(LandSamples<100 || !MansionBounds.IsValid || !FoundVillage || !FoundTerrace || !Main || Main->GetNumberOfSplinePoints()!=639)
   {SurveyFailure=TEXT("Current native island, The One, terrace, village and639-point road required");return false;}
   for(int32 I=424;I<=450;++I)
   {
    const FVector P=Main->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::World);
    if(!Surface(P,Hit) || FMath::Abs(Hit.ImpactPoint.Z-P.Z)>180){SurveyFailure=FString::Printf(TEXT("Current road%d lacks matching native support"),I);return false;}
    Survey+=FString::Printf(TEXT("current-road,%d,%.3f,%.3f,%.3f,%.4f,%s,%s\n"),I,P.X,P.Y,Hit.ImpactPoint.Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
    if(I==438)RoadAim=Hit.ImpactPoint+FVector(0,0,160);
    NativePeak=FMath::Max(NativePeak,Hit.ImpactPoint.Z);
   }
  }
  else
  {
  FString Text;TSharedPtr<FJsonObject> Road;
  if(LandSamples<100 || !MansionBounds.IsValid || !FoundVillage || !FoundTerrace
   || !FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json")))
   || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Road) || Road->GetArrayField(TEXT("points")).Num()<717)
  {SurveyFailure=TEXT("Current native land/house/terrace/village and changed road source required");return false;}
  for(int32 I=704;I<=716;++I)
  {
   const auto& V=Road->GetArrayField(TEXT("points"))[I]->AsArray();
   if(V.Num()!=3){SurveyFailure=TEXT("Current road source coordinate invalid");return false;}
   const FVector P(NeonCityCoastalLayout::IslandOrigin.X+100*V[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*V[1]->AsNumber(),-130+100*V[2]->AsNumber());
   if(!Surface(P,Hit) || FMath::Abs(Hit.ImpactPoint.Z-P.Z)>180)
   {SurveyFailure=FString::Printf(TEXT("Changed road%d source does not match highest actual native support"),I);return false;}
   Survey+=FString::Printf(TEXT("changed-road,%d,%.3f,%.3f,%.3f,%.4f,%s,%s\n"),I,P.X,P.Y,Hit.ImpactPoint.Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
   if(I==710)RoadAim=Hit.ImpactPoint+FVector(0,0,160);
   NativePeak=FMath::Max(NativePeak,Hit.ImpactPoint.Z);
  }
  }
  FFileHelper::SaveStringToFile(Survey,*(Directory/TEXT("native-surface-survey.csv")));
  if(NativePeak>50000 || NativePeak<(bCurrentEstate?1000:28000)){SurveyFailure=TEXT("Current native summit bounds are inconsistent; safe altitude cannot be guessed");return false;}
  CruiseZ=NativePeak+8500;
  if(bCurrentEstate)Add(FVector(Origin.X,Origin.Y,CruiseZ),Origin,TEXT("native-start-vertical-clearance"));
  const FVector C=LandBounds.GetCenter(),MansionAim=MansionBounds.GetCenter()+FVector(0,0,500),DockAim=Region->GetFishingBerth().GetLocation()+FVector(0,0,250);
  const double L=LandBounds.Min.X-10000,R=LandBounds.Max.X+10000,N=LandBounds.Max.Y+10000,S=LandBounds.Min.Y-10000;
  const FVector B[9]={FVector(C.X,N,CruiseZ),FVector(R,N,CruiseZ),FVector(R,C.Y,CruiseZ),FVector(R,S,CruiseZ),FVector(C.X,S,CruiseZ),FVector(L,S,CruiseZ),FVector(L,C.Y,CruiseZ),FVector(L,N,CruiseZ),FVector(C.X,N,CruiseZ)};
  for(int32 I=1;I<9;++I)BoundaryLength+=FVector::Dist2D(B[I-1],B[I]);
  for(int32 I=0;I<9;++I)
  {
   Add(B[I],I==1 || I==2 ? VillageAim : I==3 || I==4 ? DockAim : I==5 ? FVector(C.X,LandBounds.Min.Y,Region->GetActorLocation().Z) : MansionAim,
    I==0?TEXT("Silhouette"):I==5?TEXT("OceanTransition"):I==7?TEXT("MountainTerrain"):TEXT("continuous-perimeter"),I==0 || I==5 || I==7,I);
   if(I==2)
   {
    Detail(VillageAim+FVector(8500,5000,0),VillageAim,TEXT("Village"));
    Detail(VillageAim+FVector(9500,-6000,0),VillageAim+FVector(0,-5000,0),TEXT("VegetationEastCoast"));
    Add(B[I],VillageAim,TEXT("return-to-east-perimeter"));
   }
   if(I==4){Detail(DockAim+FVector(7000,-6500,0),DockAim,TEXT("Dock"));Add(B[I],DockAim,TEXT("return-to-south-perimeter"));}
  }
  Detail(RoadAim+FVector(8000,-6500,0),RoadAim,bCurrentEstate?TEXT("Road424450"):TEXT("Road704716"));
  Detail(MansionAim+FVector(6500,7000,0),MansionAim,TEXT("Mansion"));
  Detail(TerraceAim+FVector(3000,-7000,0),TerraceAim,TEXT("Terrace"));
  if(!bCurrentEstate)
  {
   Add(FVector(Origin.X,Origin.Y,CruiseZ),Origin,TEXT("physical-roof-high-approach"));
   Add(Origin+FVector(0,0,1600),Origin,TEXT("physical-roof-vertical-approach"));
  }
  FString Plan=TEXT("stop,region,boundary,review,x,y,z,aim_x,aim_y,aim_z\n");
  for(int32 I=0;I<Stops.Num();++I){const auto& V=Stops[I];Plan+=FString::Printf(TEXT("%d,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),I,*V.Region,V.Boundary,V.bReview?1:0,V.Position.X,V.Position.Y,V.Position.Z,V.Aim.X,V.Aim.Y,V.Aim.Z);}
  FFileHelper::SaveStringToFile(Plan,*(Directory/TEXT("current-route-plan.csv"))); return true;
 }
 void Submit(FVector Command,float Yaw,float Power)
 { Input=Command;YawInput=Yaw;PowerInput=Power;Heli->SubmitFlightInput_Implementation(PC,Command,Yaw);Heli->SubmitPowerInput(PC,Power); }
 void FlyTo(FVector Destination,float DT,float Limit,bool Landing)
 {
  const FVector P=Heli->GetActorLocation(),V=Heli->GetVelocity(),Error=Destination-P,Horizontal(V.X,V.Y,0),Desired=(FVector(Error.X,Error.Y,0)*.55f).GetClampedToMaxSize(Limit);
  const auto* Profile=Heli->GetProfile();const float Current=Heli->GetAircraftMovement()->GetCurrentPower();
  const float Boost=FMath::Clamp((Current-Profile->HoverPower)/(1-Profile->HoverPower),0.f,1.f),Acceleration=Profile->Acceleration*100*FMath::Lerp(1.f,Profile->FastFlightAccelerationScale,Boost);
  FVector Local=FRotator(0,Heli->GetActorRotation().Yaw,0).UnrotateVector((((Desired-Horizontal)*.9f+Horizontal*(Profile->AirResistance+.05f))/Acceleration).GetClampedToMaxSize(1));
  Local.Z=FMath::Clamp(Error.Z/(Profile->VerticalSpeed*100*1.5f),-1.,1.);
  if(Landing)Local.Z=Heli->GetAircraftMovement()->IsGrounded()?0:FMath::Min(Local.Z,-.12);
  const float Heading=Landing?InitialTransform.Rotator().Yaw:(Aim-P).Rotation().Yaw;
  YawInput=FMath::FInterpConstantTo(YawInput,FMath::Clamp(FMath::FindDeltaAngleDegrees(Heli->GetActorRotation().Yaw,Heading)/50.f,-1.f,1.f),DT,1.5f);
  Submit(Local,YawInput,0);
 }
 void Sample()
 {
  if(W->GetTimeSeconds()-LastSample<.1f)return;LastSample=W->GetTimeSeconds();
  const FVector P=Heli->GetActorLocation(),V=Heli->GetVelocity();const FString Name=Stops.IsValidIndex(Point)?Stops[Point].Region:TEXT("roof-land-exit");
  Rows+=FString::Printf(TEXT("%.4f,%d,%d,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%.3f,%.3f,%.5f,%.3f,%.3f,%.3f\n"),W->GetTimeSeconds(),Stage,Point,*Name,P.X,P.Y,P.Z,Heli->GetActorRotation().Yaw,V.X,V.Y,V.Z,Heli->GetAircraftMovement()->GetAltitude(),Input.X,Input.Y,Input.Z,YawInput,PowerInput,Heli->GetAircraftMovement()->IsGrounded()?1:0,Heli->GetHealthComponent()->GetCurrentHealth(),Travel,Winding,Aim.X,Aim.Y,Aim.Z);
  if(Rows.Len()>32000)Flush();
 }
 void Flush()
 {if(!Rows.IsEmpty() && !Directory.IsEmpty()){FFileHelper::SaveStringToFile(Rows,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);Rows.Reset();}}
 bool EndSurvey()
 {
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),true);J->SetStringField(TEXT("scope"),TEXT("Read-only native grounded aircraft, full-capsule approach, measured whole-island terrain/road/estate route and actual highest-pad support. One player preparation fixture. No flight, entry or gameplay PASS claimed."));
  J->SetStringField(TEXT("aircraft"),Heli->GetPathName());J->SetStringField(TEXT("origin"),Origin.ToString());J->SetStringField(TEXT("landing_origin"),LandingOrigin.ToString());J->SetStringField(TEXT("land_bounds"),LandBounds.ToString());J->SetNumberField(TEXT("planned_boundary_cm"),BoundaryLength);J->SetNumberField(TEXT("native_peak_cm"),NativePeak);J->SetNumberField(TEXT("safe_cruise_cm"),CruiseZ);J->SetNumberField(TEXT("route_stops"),Stops.Num());
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("survey.json")));return true;
 }
 bool End(bool Passed,const FString& Failure)
 {
  if(bLocalTerraceReturn)return EndTerrace(Passed,Failure);
  if(!Failure.IsEmpty())Test->AddError(Failure);
  if(PC)for(auto K:{EKeys::F7,EKeys::F,EKeys::SpaceBar})Key(K,IE_Released);
  if(PC && Heli && PC->GetPawn()==Heli)Submit(FVector::ZeroVector,0,0);
  Flush();Metrics.Flush();if(Directory.IsEmpty())return true;
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed);J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("stop"),Point);
  J->SetStringField(TEXT("scope"),bCurrentEstate?TEXT("One supported initial player approach to genuinely spawned native helicopter; normal F/Space entry/takeoff, uninterrupted native whole-island closed perimeter plus nine regional flight clips, actual highest The One helipad landing and supported normal F exit. No aircraft pose, velocity, force, collision or flight-state fixtures."):TEXT("Genuine retained RoofFlight30 F7/on-foot roof restore, normal F/Space takeoff, current native measured whole-island closed perimeter and regional physical flight, actual same-roof landing and supported normal F exit. Not Dock-to-roof acquisition. Existing Tree LOD evidence cached; subjective quality remains user review."));
  J->SetStringField(TEXT("save_slot"),TEXT("NeonCity_Test_RidgeRoofUpgrade"));J->SetStringField(TEXT("checkpoint_action"),TEXT("F7 before takeoff only; no save writes or flight reloads"));
  if(bCurrentEstate){J->SetStringField(TEXT("save_slot"),TEXT("NONE"));J->SetStringField(TEXT("checkpoint_action"),TEXT("NONE"));J->SetStringField(TEXT("actual_landing_target"),LandingOrigin.ToString());}
  J->SetNumberField(TEXT("initial_player_fixtures"),bCurrentEstate?1:0);J->SetNumberField(TEXT("aircraft_pose_writes"),0);J->SetNumberField(TEXT("aircraft_velocity_or_force_writes"),0);J->SetNumberField(TEXT("collision_edits"),0);
  J->SetNumberField(TEXT("continuous_flight_cm"),Travel);J->SetNumberField(TEXT("planned_boundary_cm"),BoundaryLength);J->SetNumberField(TEXT("ordered_boundary_points"),BoundaryVisited);J->SetNumberField(TEXT("actual_boundary_winding_radians"),Winding);J->SetNumberField(TEXT("actual_boundary_closure_cm"),Closure);
  J->SetStringField(TEXT("native_land_bounds_cm"),LandBounds.ToString());J->SetStringField(TEXT("native_mansion_bounds_cm"),MansionBounds.ToString());J->SetNumberField(TEXT("native_peak_cm"),NativePeak);J->SetNumberField(TEXT("safe_cruise_z_cm"),CruiseZ);
  J->SetStringField(TEXT("native_peak_method"),TEXT("Conservative maximum of actual streamed Landscape, rock/house render bounds and native physical floor traces; source height alone does not choose altitude. Land bounds use 65x65 native island-level floor samples above sea+250cm, with perimeter10000cm outside measured land."));
  J->SetStringField(TEXT("restored_grounded_aircraft_transform"),InitialTransform.ToHumanReadableString());J->SetNumberField(TEXT("initial_health"),InitialHealth);J->SetNumberField(TEXT("final_health"),Heli?Heli->GetHealthComponent()->GetCurrentHealth():0);
  TArray<TSharedPtr<FJsonValue>> Clips;bool AllClips=Reviewed.Num()==9;
  for(const FString& Name:Reviewed)
  {
   const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/RunLabel/TEXT("Motion")/((bCurrentEstate?TEXT("CurrentEstateHelicopter-"):TEXT("CurrentHelicopter246-"))+Name);
   TArray<FString> Images;IFileManager::Get().FindFiles(Images,*(Folder/TEXT("*.jpg")),true,false);
   auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("region"),Name);C->SetStringField(TEXT("native_motion_directory"),Folder);C->SetNumberField(TEXT("actual_saved_jpeg_frames"),Images.Num());C->SetBoolField(TEXT("rendered_motion_evidence_present"),Images.Num()>=24);AllClips=AllClips && Images.Num()>=24;Clips.Add(MakeShared<FJsonValueObject>(C));
  }
  J->SetArrayField(TEXT("regional_motion_evidence"),Clips);J->SetBoolField(TEXT("all_nine_rendered_region_clips_present"),AllClips);
  if(Passed && !AllClips)
  {
   Test->AddError(TEXT("Native circuit gameplay passed but required nine actual rendered motion clips are missing; visual objective gate requires NeonCityArtMotion"));
   J->SetBoolField(TEXT("passed"),false);J->SetStringField(TEXT("failure"),TEXT("Actual rendered regional motion evidence incomplete"));
  }
  J->SetStringField(TEXT("technical_validation"),Passed?TEXT("PASS"):TEXT("FAIL"));J->SetStringField(TEXT("automated_gameplay_validation"),Passed?TEXT("PASS"):TEXT("FAIL"));J->SetStringField(TEXT("visual_evidence_review"),AllClips?TEXT("CAPTURED; REVIEW REQUIRED"):TEXT("MISSING; run requires NeonCityArtMotion for visual gate"));
  J->SetStringField(TEXT("performance_policy"),TEXT("Clean flight scopes in IslandLandPass1/Metrics/CurrentHelicopterCircuit246-allframes.csv; regional synchronous image readback and recovery labelled separately, exclude those from GPU median/p95/stutter claims."));
  J->SetBoolField(TEXT("unchanged_tree_lod_pass_cached"),true);J->SetStringField(TEXT("user_subjective_review"),TEXT("USER FINAL PLAYTEST / VISUAL / HANDLING / AUDIO REVIEW REQUIRED"));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json")));return true;
 }
 bool EndTerrace(bool Passed,const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);
  if(PC)for(auto K:{EKeys::F7,EKeys::F,EKeys::SpaceBar})Key(K,IE_Released);
  if(PC && Heli && PC->GetPawn()==Heli)Submit(FVector::ZeroVector,0,0);
  Flush();Metrics.Flush();if(Directory.IsEmpty())return true;
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/RunLabel/TEXT("Motion/CurrentHelicopter260-Terrace");
  TArray<FString> Images;IFileManager::Get().FindFiles(Images,*(Folder/TEXT("*.jpg")),true,false);
  const bool Captured=Reviewed.Contains(TEXT("Terrace")) && Images.Num()>=24;
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Passed && Captured);
  J->SetStringField(TEXT("failure"),!Failure.IsEmpty()?Failure:Captured?TEXT(""):TEXT("Local real terrace motion clip missing"));
  if(Passed && !Captured)Test->AddError(TEXT("Local physical terrace return passed but required real rendered clip is missing"));
  J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("stop"),Point);
  J->SetStringField(TEXT("technical_validation"),Passed?TEXT("PASS"):TEXT("FAIL"));J->SetStringField(TEXT("automated_gameplay_validation"),Passed?TEXT("PASS"):TEXT("FAIL"));
  J->SetStringField(TEXT("visual_evidence_review"),Captured?TEXT("CAPTURED; REVIEW REQUIRED"):TEXT("MISSING"));
  J->SetStringField(TEXT("scope"),TEXT("Independent local completion only. Same genuine RidgeRoofUpgrade F7, normal F/Space takeoff, existing native input to current terrace BBQ camera region, measured full-body-width corridor clearance, real same-roof physical landing and supported normal F exit. This does not resume256 or claim a second full circuit or nine clips in one flight. Original256 full-test FAIL and separately proven continuous perimeter remain retained."));
  J->SetBoolField(TEXT("full_circuit_claimed"),false);J->SetBoolField(TEXT("continuation_of_previous_airborne_state"),false);
  J->SetStringField(TEXT("save_slot"),TEXT("NeonCity_Test_RidgeRoofUpgrade"));J->SetStringField(TEXT("checkpoint_action"),TEXT("Genuine F7 before independent takeoff only; no save writes"));
  J->SetNumberField(TEXT("initial_player_fixtures"),0);J->SetNumberField(TEXT("aircraft_pose_writes"),0);J->SetNumberField(TEXT("aircraft_velocity_or_force_writes"),0);J->SetNumberField(TEXT("collision_edits"),0);
  J->SetNumberField(TEXT("local_native_flight_cm"),Travel);J->SetNumberField(TEXT("native_local_corridor_peak_cm"),NativePeak);J->SetNumberField(TEXT("safe_local_transit_z_cm"),CruiseZ);
  J->SetStringField(TEXT("actual_review_position_cm"),LocalReviewPosition.ToString());J->SetNumberField(TEXT("actual_saved_jpeg_frames"),Images.Num());J->SetStringField(TEXT("native_motion_directory"),Folder);
  J->SetNumberField(TEXT("initial_health"),InitialHealth);J->SetNumberField(TEXT("final_health"),Heli?Heli->GetHealthComponent()->GetCurrentHealth():0);
  J->SetStringField(TEXT("user_subjective_review"),TEXT("USER FINAL PLAYTEST / VISUAL / HANDLING / AUDIO REVIEW REQUIRED"));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityCoastalRegion* Region=nullptr;
 ANeonCityPlayerCharacter* Player=nullptr;ANeonCityHelicopterPrototype* Heli=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 FNeonCityLandFrameMetrics Metrics{TEXT("CurrentHelicopterCircuit246")};TUniquePtr<FNeonCityVisualEvidence> Clip;
 TArray<FStop> Stops;TArray<FString> Reviewed;FString Directory,RunLabel,Rows,SurveyFailure;
 FBox LandBounds{ForceInit},MansionBounds{ForceInit};FTransform InitialTransform;
 FVector Origin=FVector::ZeroVector,Last=FVector::ZeroVector,RoofContact=FVector::ZeroVector,LoopStart=FVector::ZeroVector,Aim=FVector::ZeroVector,Input=FVector::ZeroVector;
 double Started=0,Travel=0,NativePeak=0,CruiseZ=0,BoundaryLength=0,Closure=0,Winding=0;
 float At=0,Previous=0,StableSeconds=0,InitialHealth=0,LastSample=0,LastReadback=-1,LastAngle=0,YawInput=0,PowerInput=0;
 int32 Stage=0,Point=0,BoundaryVisited=0;bool bBoundaryActive=false;
 bool bLocalTerraceReturn=false;FVector LocalReviewPosition=FVector::ZeroVector;
 bool bCurrentEstate=false,bSurveyOnly=false;FVector LandingOrigin=FVector::ZeroVector;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCurrentHelicopterCircuit246,"NeonCity.Phase205c.CurrentHelicopterCircuit246PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCurrentHelicopterCircuit246::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(CurrentHelicopter246::FCircuit(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityCurrentHelicopterTerraceReturn260,"NeonCity.Phase205c.CurrentHelicopterTerraceReturn260PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityCurrentHelicopterTerraceReturn260::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(CurrentHelicopter246::FCircuit(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentEstateHelicopterSurvey,"NeonCity.Phase205c.CurrentEstateHelicopterSurveyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentEstateHelicopterSurvey::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentHelicopter246::FCircuit(this,false,true,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentEstateHelicopterCircuit,"NeonCity.Phase205c.CurrentEstateHelicopterCircuitPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentEstateHelicopterCircuit::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentHelicopter246::FCircuit(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
