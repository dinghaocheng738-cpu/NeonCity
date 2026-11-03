#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/Marine/NeonCityWaterVolume.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace TheOnePrincipalPool
{
class FSwim:public IAutomationLatentCommand
{
public:
 explicit FSwim(FAutomationTestBase* T,bool InMain=false):Test(T),Main(InMain){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>100)return End(TEXT("New finite principal pool minimum exceeded100seconds"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   bool Found=false;for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_GroundLiftPit481")){Pose=It->GetActorTransform();Found=true;break;}
   for(TActorIterator<ANeonCityWaterVolume> It(W);It;++It)if(It->ActorHasTag(Main?TEXT("NeonCity.Phase205cUpgrade.MainPool554"):TEXT("NeonCity.Phase205cUpgrade.PrincipalPool548")))Pool=*It;
   if(!Found || !Pool || !Pool->bContainedPoolWater)return End(TEXT("Native finite pool or reference estate frame missing"));
   auto* Registry=W->GetSubsystem<UMarineWorldSubsystem>();FWaterSurfaceSample Sample;
   if(!Registry || !Registry->QueryWater(M(1,0,Water()-.4),Sample) || !Sample.InsideVolume || !Sample.ContainedWater || FMath::Abs(Sample.SurfacePoint.Z-M(1,0,Water()).Z)>1)return End(TEXT("Actual finite surface registration or water membership failed"));
   if(Pool->QueryWaterSurface_Implementation(M(1,0,Bottom()-.07),Sample) || Pool->QueryWaterSurface_Implementation(M(1,Main?2.8:1.8,Water()-.4),Sample))return End(TEXT("Pool water leaks below the basin or across its physical rim"));
   FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneOpenPool),true,P);int32 Probes=0;
   for(int32 X=-2;X<=4;++X)for(float Y:{-1.f,0.f,1.f})
   {FHitResult Hit;if(!W->LineTraceSingleByChannel(Hit,M(X,Y*(Main?2:1),Deck()+.58),M(X,Y*(Main?2:1),Bottom()-.27),ECC_Pawn,Q) || FMath::Abs(Hit.ImpactPoint.Z-M(X,Y,Bottom()).Z)>1)return End(FString::Printf(TEXT("Pool floor probe x%d y%.1f hit %s / %s at%s; expected%s; sealed slab, foreign collider or absent bottom"),X,Y,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString(),*M(X,Y,Bottom()).ToString()));++Probes;}
   for(float X:{-4.f,0.f,4.f})
   {FHitResult Hit;if(W->LineTraceSingleByChannel(Hit,M(X,0,13),M(X,0,Deck()+.38),ECC_Pawn,Q))return End(TEXT("New pool daylight courtyard is still sealed overhead"));}
   const FVector Corners[]={FVector(-6.2,-2.5,12.04),FVector(6.2,-2.5,12.04),FVector(6.2,2.5,12.04),FVector(-6.2,2.5,12.04)};
   for(int32 I=0;!Main && I<4;++I)
   {
    const FVector A=Corners[I],B=Corners[(I+1)%4],Mid=(A+B)*.5;
    // Use the edge's actual outward normal, rather than its corner radius.
    const FVector Edge=(B-A).GetSafeNormal();const FVector N(-Edge.Y,Edge.X,0);
    for(ECollisionChannel Channel:{ECC_Pawn,ECC_Vehicle})
    {FHitResult Hit;const FVector From=M(Mid.X-N.X*80/100,Mid.Y-N.Y*80/100,12.7),To=M(Mid.X+N.X*80/100,Mid.Y+N.Y*80/100,12.7);if(!W->SweepSingleByChannel(Hit,From,To,FQuat::Identity,Channel,FCollisionShape::MakeSphere(25),Q))return End(TEXT("Roof courtyard edge lacks real Pawn/Vehicle collision"));}
   }
   NativeFloorProbes=Probes;NativeRoofGuardSweeps=Main?0:8;
   if(Main)
   {
    TSet<FName> Required;
    for(int32 I=174;I<=178;++I)Required.Add(FName(*FString::Printf(TEXT("StaticMeshActor_%d"),I)));
    for(int32 I=278;I<=302;++I)Required.Add(FName(*FString::Printf(TEXT("StaticMeshActor_%d"),I)));
    for(int32 I=371;I<=374;++I)Required.Add(FName(*FString::Printf(TEXT("StaticMeshActor_%d"),I)));
    for(int32 I=10;I<=14;++I)Required.Add(FName(*FString::Printf(TEXT("IslandLandLight_%d"),I)));
    for(int32 I=33;I<=45;++I)Required.Add(FName(*FString::Printf(TEXT("IslandLandLight_%d"),I)));
    for(const TCHAR* N:{TEXT("StaticMeshActor_162"),TEXT("StaticMeshActor_163"),TEXT("StaticMeshActor_166"),TEXT("StaticMeshActor_274"),TEXT("StaticMeshActor_275"),TEXT("StaticMeshActor_356"),TEXT("StaticMeshActor_357"),TEXT("TextRenderActor_3"),TEXT("IslandLandGrill_0")})Required.Add(FName(N));
    const int32 Expected=Required.Num();
    for(TActorIterator<AActor> It(W);It;++It)if(Required.Contains(It->GetFName()))
    {if(It->IsHidden())return End(FString::Printf(TEXT("Original outdoor amenity remains hidden: %s"),*It->GetName()));Required.Remove(It->GetFName());}
    if(Required.Num())return End(TEXT("A retained outdoor actor identity is missing from actual PIE"));OutdoorIdentities=Expected;
    for(int32 I=0;I<=11;++I)
    {const FVector Here=S(-42.6,-3.+I*2.,Deck());FHitResult Hit;if(!W->LineTraceSingleByChannel(Hit,Here+FVector(0,0,50),Here-FVector(0,0,30),ECC_Pawn,Q) || FMath::Abs(Hit.ImpactPoint.Z-Here.Z)>1)return End(TEXT("Pool circulation corridor lacks supported dry floor"));
     const float Height=P->GetSimpleCollisionHalfHeight()+2;if(W->SweepSingleByChannel(Hit,Here+FVector(0,0,Height),Here+FVector(.1,0,Height),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(FString::Printf(TEXT("Dry route probe%d blocked by%s / %s at%s; real capsule halfheight%.1f"),I,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString(),P->GetSimpleCollisionHalfHeight()));++DryRouteProbes;}
   }
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OldTime=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);break;}
   P->SetActorLocation(M(StartX(),0,Deck())+FVector(0,0,P->GetSimpleCollisionHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;Camera->SetActorLocation(M(Main?-11:-7,Main?-6.5:-4.1,Deck()+2.58));Camera->SetActorRotation((M(0,0,Deck()-.32)-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);return Next(1);
  }
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==1)
  {if(Age<3.5)return false;if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Declared pool deck start lacks real capsule support"));FNeonCityVisualEvidence::CaptureHUD(W,Main?TEXT("TheOne-MainPool-Day.png"):TEXT("TheOne-PrincipalPool-Day.png"));PC->SetViewTarget(P);return Next(2);}
  if(Stage==2 || Stage==3)
  {
   const FVector Goal=Stage==2?M(1.5,0,Water()-.42):M(StartX(),0,Deck());const FVector Delta=Goal-P->GetActorLocation();
   if(P->GetCharacterMovement()->IsSwimming())
   {if(!Swam){Swam=true;SwimStart=P->GetActorLocation();}SwimDistance=FMath::Max(SwimDistance,FVector::Dist2D(SwimStart,P->GetActorLocation()));}
   if(Delta.Size2D()<38)
   {
    Key(false);
    if(Stage==2){if(!Swam || SwimDistance<200)return End(TEXT("Actual mapped movement failed to enter swimming and travel two metres"));FNeonCityVisualEvidence::CaptureHUD(W,Main?TEXT("TheOne-MainPool-NativeSwim.png"):TEXT("TheOne-PrincipalPool-NativeSwim.png"));return Next(3);}
    if(!P->GetCharacterMovement()->IsMovingOnGround())return Age>25?End(TEXT("Native pool stairs failed to return swimmer to dry walking")):false;
    Exited=true;FNeonCityVisualEvidence::CaptureHUD(W,Main?TEXT("TheOne-MainPool-NativeExit.png"):TEXT("TheOne-PrincipalPool-NativeExit.png"));if(Main)return Next(6);PC->SetViewTarget(Camera);Clock->SetTimeOfDay(17.8f);return Next(4);
   }
   if(Age>25)return End(FString::Printf(TEXT("Actual mapped pool movement stalled in stage%d at%s"),Stage,*P->GetActorLocation().ToString()));
   PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));Key(true);return false;
  }
  if(Stage==4 || Stage==5)
  {if(Age<3.5)return false;FNeonCityVisualEvidence::CaptureHUD(W,Main?(Stage==4?TEXT("TheOne-MainPool-Sunset.png"):TEXT("TheOne-MainPool-Night.png")):(Stage==4?TEXT("TheOne-PrincipalPool-Sunset.png"):TEXT("TheOne-PrincipalPool-Night.png")));if(Stage==5)return End(TEXT(""));Clock->SetTimeOfDay(22);return Next(5);}
  if(Stage>=6 && Stage<=8)
  {
   const FVector Goal=Stage==6?S(-42.6,-3.,Deck()):Stage==7?S(-42.6,18.,Deck()):S(-40.8,13.2,Deck());const FVector Delta=Goal-P->GetActorLocation();
   if(Delta.Size2D()<35){Key(false);if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Normal circulation did not remain supported walking"));if(Stage<8)return Next(Stage+1);DryRouteWalk=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-MainPool-PlayerCirculation.png"));PC->SetViewTarget(Camera);Clock->SetTimeOfDay(17.8f);return Next(4);}
   if(Age>25)return End(FString::Printf(TEXT("Normal dry circulation stalled in stage%d"),Stage));PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));Key(true);return false;
  }
  return false;
 }
private:
 FVector M(double X,double Y,double Z)const
 {const double A=FMath::DegreesToRadians(Main?90.:58.);return Pose.TransformPosition(FVector(((Main?-46.3:31.7)+X*FMath::Cos(A)-Y*FMath::Sin(A))*100,-((Main?7.:-24.3)+X*FMath::Sin(A)+Y*FMath::Cos(A))*100,Z*100));}
 double Deck()const{return Main?3.048:7.62;}double Water()const{return Deck()-.1;}double Bottom()const{return Main?1.448:6.17;}double StartX()const{return Main?-10.05:-6.05;}
 FVector S(double X,double Y,double Z)const{return Pose.TransformPosition(FVector(X*100,-Y*100,Z*100));}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(bool Down){if(Held==Down || !PC)return;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}
 bool End(const FString& Error)
 {
  Key(false);if(!Error.IsEmpty())Test->AddError(Error);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldTime);Clock->SetCycleEnabled(OldCycle);}
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetBoolField(TEXT("native_swimming"),Swam);J->SetBoolField(TEXT("native_dry_exit"),Exited);J->SetNumberField(TEXT("swim_distance_cm"),SwimDistance);J->SetNumberField(TEXT("physical_floor_probes"),NativeFloorProbes);J->SetNumberField(TEXT("roof_pawn_vehicle_sweeps"),NativeRoofGuardSweeps);J->SetNumberField(TEXT("initial_player_fixtures"),1);J->SetNumberField(TEXT("post_t0_player_pose_or_velocity_writes"),0);J->SetStringField(TEXT("scope"),TEXT("One declared supported deck fixture; real mapped W entry, inherited swimming, physical stair exit, bounded water and actual Day/Sunset/Night evidence. No whole mansion or subjective PASS claim."));
  J->SetStringField(TEXT("pool"),Main?TEXT("Main outdoor swimming court"):TEXT("Upper principal pool terrace"));J->SetNumberField(TEXT("visible_original_outdoor_identities"),OutdoorIdentities);J->SetNumberField(TEXT("dry_circulation_probes"),DryRouteProbes);J->SetBoolField(TEXT("actual_supported_circulation_walk"),DryRouteWalk);FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(Main?TEXT("TheOneMainPool.json"):TEXT("TheOnePrincipalPool.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityWaterVolume* Pool=nullptr;ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;FTransform Pose;FVector SwimStart;
 double Started=0,SwimDistance=0;float At=0,OldTime=0;int32 Stage=0,NativeFloorProbes=0,NativeRoofGuardSweeps=0,OutdoorIdentities=0,DryRouteProbes=0;bool Main=false,OldCycle=false,Held=false,Swam=false,Exited=false,DryRouteWalk=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOnePool550,"NeonCity.Phase205cUpgrade.TheOnePrincipalPoolMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOnePool550::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePrincipalPool::FSwim(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneMainPool559,"NeonCity.Phase205cUpgrade.TheOneMainPoolMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneMainPool559::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePrincipalPool::FSwim(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
