#include "Leisure/NeonCityAerialTour.h"
#include "Leisure/NeonCityActivityHUD.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "Vehicles/Aircraft/AircraftVehicleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace NeonCityAerial
{
const float SlingLength=900.f;                 // cm from the hull's belly to the crate's top
const float DropRadius=2500.f,DropMinHeight=600.f,DropMaxHeight=6000.f,DropMaxKmh=20.f;
const float SteadySwing=150.f;                 // cm/s the load may still swing at release
const float RingCm[3]={300.f,800.f,1500.f};
const TCHAR* Grades[4]={TEXT("MISSED"),TEXT("ON TARGET"),TEXT("GOOD DROP"),TEXT("BULLSEYE")};
}
using namespace NeonCityAerial;

ANeonCityAerialTour::ANeonCityAerialTour()
{
 Vehicle=ENeonCityLeisureVehicle::Aircraft;MaximumSeconds=900;RewardPreview=2150;StartRadius=4000;
 Accent=FLinearColor(.25f,.75f,1.f);
 CrateMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/IslandLandPass1/Meshes/SM_Land_wooden_crate_01.SM_Land_wooden_crate_01")));
 TargetMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/NeonCity/Art/WestLake/Waterway/SM_WL_DropTarget.SM_WL_DropTarget")));
}
FString ANeonCityAerialTour::DescribeRules() const
{
 FString S=Description.ToString();
 if(Kind==ENeonCityAerialKind::Sightseeing)
 {
  S+=FString::Printf(TEXT("\n\n%d sights · $%lld a photograph · $%lld for landing back on the helipad"),Stops.Num(),RewardPerStop,FinishBonus);
  for(int32 I=0;I<Stops.Num();++I)S+=FString::Printf(TEXT("\n%d. %s"),I+1,*Stops[I].Label.ToString());
 }
 else S+=FString::Printf(TEXT("\n\n%d drops · bullseye 3 m, good 8 m, on target 15 m · up to $%lld"),Stops.Num(),RewardPerStop*3*Stops.Num()+FinishBonus);
 return S;
}
int32 ANeonCityAerialTour::ScoreLanding(int32 Stop,const FVector& At) const
{
 if(!Stops.IsValidIndex(Stop))return 0;
 // a target laid smaller to fit its island (MaxDistance = its outer ring) scores on its own rings
 const float K=FMath::Clamp(Stops[Stop].MaxDistance/RingCm[2],.3f,1.f);
 const float D=FVector::Dist2D(At,Stops[Stop].Location);
 return D<=RingCm[0]*K?3:D<=RingCm[1]*K?2:D<=RingCm[2]*K?1:0;
}
bool ANeonCityAerialTour::IsFramed(int32 Stop,const FVector& Eye,const FRotator& View,AActor* Ignore) const
{
 if(!Stops.IsValidIndex(Stop))return false;
 const FVector To=Stops[Stop].Location-Eye;const float D=To.Size();
 if(D>Stops[Stop].MaxDistance || D<3000)return false;
 if(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(To/D,View.Vector())))>FrameAngle)return false;
 // in plain sight: nothing solid between the lens and the last tenth of the way to the subject
 FHitResult Hit;FCollisionQueryParams Q(TEXT("AerialPhoto"),true,Ignore);
 return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+To*.9f,ECC_Visibility,Q);
}
void ANeonCityAerialTour::OnAccepted(APlayerController* Player)
{
 Hold=0;Score=0;Photos.Reset();FallSeconds=0;bSlingInit=false;Loaded=0;
 if(Kind==ENeonCityAerialKind::Airdrop)
  if(UStaticMesh* Mesh=TargetMesh.LoadSynchronous())
   for(const auto& S:Stops)
   {
    auto* T=NewObject<UStaticMeshComponent>(this);T->SetStaticMesh(Mesh);T->SetMobility(EComponentMobility::Movable);
    T->SetWorldLocation(S.Location+FVector(0,0,3));T->SetWorldScale3D(FVector(FMath::Clamp(S.MaxDistance/RingCm[2],.3f,1.f),FMath::Clamp(S.MaxDistance/RingCm[2],.3f,1.f),1.f));T->SetCollisionEnabled(ECollisionEnabled::NoCollision);T->SetCastShadow(false);
    T->RegisterComponent();Targets.Add(T);
   }
 Guide(StartLocation,FText::FromString(TEXT("Helipad")));
 Notify(FText::FromString(Kind==ENeonCityAerialKind::Sightseeing
  ?TEXT("Take your helicopter up from the residence helipad. The sights are marked in order.")
  :TEXT("Go to your helicopter on the residence helipad: the crates are hooked on when you are aboard.")),7);
}
void ANeonCityAerialTour::ClearRun()
{
 if(HiddenFor.IsValid())SetShotHidden(HiddenFor.Get(),false);HiddenFor.Reset();
 for(UStaticMeshComponent* C:Slung)if(C)C->DestroyComponent();Slung.Reset();
 for(UStaticMeshComponent* C:Targets)if(C)C->DestroyComponent();Targets.Reset();
 if(Cable)Cable->DestroyComponent();Cable=nullptr;
 if(Falling.IsValid())Falling->SetLifeSpan(20);Falling.Reset();
 Hold=0;Loaded=0;bSlingInit=false;
}
FText ANeonCityAerialTour::CheckFailure(APlayerController* Player,APawn* Pawn) const
{
 const FText Base=Super::CheckFailure(Player,Pawn);if(!Base.IsEmpty())return Base;
 if(Progress.ActivityStage>=2 && Classify(Pawn)!=ENeonCityLeisureVehicle::Aircraft && VehicleMissingSeconds>8)
  return FText::FromString(TEXT("You left the helicopter."));
 return FText::GetEmpty();
}
void ANeonCityAerialTour::GuideNext()
{
 if(Stops.IsValidIndex(Progress.ActivityCheckpoint))Guide(Stops[Progress.ActivityCheckpoint].Location,Stops[Progress.ActivityCheckpoint].Label);
 else Guide(PadLocation,FText::FromString(TEXT("Land on the helipad")));
}
void ANeonCityAerialTour::TickActivity(float Delta,APlayerController* Player,APawn* Pawn)
{
 const bool Heli=Classify(Pawn)==ENeonCityLeisureVehicle::Aircraft;
 VehicleMissingSeconds=Heli?0.f:VehicleMissingSeconds+Delta;
 if(Progress.ActivityStage==0)
 {
  if(!Heli || FVector::Dist2D(Pawn->GetActorLocation(),StartLocation)>StartRadius)return;
  Progress.ActivityCheckpoint=0;Progress.ActivitySeconds=0;SetStage(2);
  if(Kind==ENeonCityAerialKind::Airdrop)
  {
   // the crates hang one under another on the sling
   UStaticMesh* Mesh=CrateMesh.LoadSynchronous();
   Cable=NewObject<UStaticMeshComponent>(this);Cable->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
   Cable->SetMobility(EComponentMobility::Movable);Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cable->SetCastShadow(false);
   if(auto* Dark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/WestLake/Materials/MI_WL_DarkMetal.MI_WL_DarkMetal")))Cable->SetMaterial(0,Dark);
   Cable->RegisterComponent();
   for(int32 I=0;I<Stops.Num() && Mesh;++I)
   {
    auto* C=NewObject<UStaticMeshComponent>(this);C->SetStaticMesh(Mesh);C->SetMobility(EComponentMobility::Movable);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();Slung.Add(C);
   }
   Loaded=Slung.Num();
   Notify(FText::FromString(TEXT("Crates hooked on. Hover low and slow over each target to release one.")),6);
  }
  else Notify(FText::FromString(TEXT("Frame each sight in the middle of your view and hold it there.")),6);
  if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Banner(Title,FText::FromString(Kind==ENeonCityAerialKind::Sightseeing?TEXT("Sightseeing flight"):TEXT("Supply drop")),Accent,2.5f);
  PlayCue(TEXT("Go"));GuideNext();
  return;
 }
 Progress.ActivitySeconds+=Delta;
 if(Kind==ENeonCityAerialKind::Sightseeing)TickSightseeing(Delta,Player,Pawn);else TickAirdrop(Delta,Player,Pawn);
}
void ANeonCityAerialTour::TickSightseeing(float Delta,APlayerController* Player,APawn* Pawn)
{
 if(HiddenFor.IsValid() && --HiddenFrames<=0){SetShotHidden(HiddenFor.Get(),false);HiddenFor.Reset();}
 FNeonCityActivityHUDState S;S.Title=Title;S.Accent=Accent;S.Clock=FText::FromString(FormatTime(Progress.ActivitySeconds));
 const int32 I=Progress.ActivityCheckpoint;
 if(Stops.IsValidIndex(I))
 {
  FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
  const bool Framed=Classify(Pawn)==ENeonCityLeisureVehicle::Aircraft && IsFramed(I,Eye,View,Pawn);
  Hold=Framed?Hold+Delta:FMath::Max(0.f,Hold-Delta*2);
  const float D=FVector::Dist(Eye,Stops[I].Location);
  S.Sub=FText::FromString(FString::Printf(TEXT("Photo %d / %d  ·  %s"),I+1,Stops.Num(),*Stops[I].Label.ToString()));
  S.Lines.Add(FText::FromString(D>Stops[I].MaxDistance?FString::Printf(TEXT("%.0f m away: get within %.0f m"),D/100,Stops[I].MaxDistance/100)
   :TEXT("Put it in the middle of your view and hold steady")));
  S.bFraming=D<=Stops[I].MaxDistance*1.3f;S.FramingProgress=Hold/HoldSeconds;S.FramingLabel=Framed?FText::FromString(TEXT("Hold...")):Stops[I].Label;
  if(Hold>=HoldSeconds)
  {
   TakePhoto(I,Pawn);Hold=0;++Progress.ActivityCheckpoint;
   if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Banner(FText::FromString(TEXT("PHOTO")),Stops[I].Label,FLinearColor::White,1.6f);
   GuideNext();
  }
 }
 else
 {
  // all sights taken: land back on the helipad
  const auto* Move=Cast<ANeonCityAircraftVehicleBase>(Pawn)?Cast<ANeonCityAircraftVehicleBase>(Pawn)->GetAircraftMovement():nullptr;
  const float D=FVector::Dist2D(Pawn->GetActorLocation(),PadLocation);
  S.Sub=FText::FromString(TEXT("All photographs taken  ·  land on the helipad"));
  S.Lines.Add(FText::FromString(FString::Printf(TEXT("%.0f m to the pad"),D/100)));
  if(Move && Move->IsGrounded() && D<1500 && Pawn->GetVelocity().Size()<150)
  {
   const int64 Pay=RewardPerStop*Stops.Num()+FinishBonus;
   Complete(Pay,Progress.ActivitySeconds,FText::FromString(TEXT("TOUR COMPLETE")),
    FText::FromString(FString::Printf(TEXT("%d photographs  ·  %s  ·  $%lld"),Photos.Num(),*FormatTime(Progress.ActivitySeconds),Pay)));
   return;
  }
 }
 S.Progress=Stops.Num()?float(Progress.ActivityCheckpoint)/Stops.Num():-1.f;
 if(Progress.ActivityBestSeconds>0)S.Lines.Add(FText::FromString(FString::Printf(TEXT("Best %s"),*FormatTime(Progress.ActivityBestSeconds))));
 ShowHUD(S);
}
void ANeonCityAerialTour::SetShotHidden(AActor* Actor,bool bHide)
{
 // with what hangs on it (weapon pods, the pilot)
 Actor->SetActorHiddenInGame(bHide);
 TArray<AActor*> Attached;Actor->GetAttachedActors(Attached,true,true);
 for(AActor* A:Attached)if(A)A->SetActorHiddenInGame(bHide);
}
void ANeonCityAerialTour::TakePhoto(int32 Stop,APawn* Heli)
{
 PlayCue(TEXT("Shutter"));
 // the chase camera sits behind the helicopter, which would fill the middle of the frame: the shot is taken as from a
 // belly camera, with the helicopter hidden for the frames the screenshot reads
 if(Heli){SetShotHidden(Heli,true);HiddenFor=Heli;HiddenFrames=3;}
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Leisure/Photos")/Progress.ActivityRunID.ToString(EGuidFormats::Digits);
 IFileManager::Get().MakeDirectory(*Dir,true);
 const FString File=Dir/FString::Printf(TEXT("%02d.png"),Stop+1);
 FScreenshotRequest::RequestScreenshot(File,false,false);
 Photos.Add(File);Progress.ActivityPhotoFile=File;
}
void ANeonCityAerialTour::UpdateSling(float Delta,APawn* Heli)
{
 // the load swings as a damped pendulum under the belly; each crate hangs 1.2 m under the one above
 const FVector Hook=Heli->GetActorLocation()-FVector(0,0,90);
 const FVector Rest=Hook-FVector(0,0,SlingLength);
 // a jump of the helicopter (a reload, a teleport) leaves the load hanging under it, not flung across the lake
 if(bSlingInit && FVector::Dist(SlingPos,Rest)>SlingLength*2.5f)bSlingInit=false;
 if(!bSlingInit){SlingPos=Rest;SlingVel=Heli->GetVelocity();bSlingInit=true;}
 const FVector Accel=(Rest-SlingPos)*6.f-(SlingVel-Heli->GetVelocity())*2.2f;
 SlingVel+=Accel*Delta;SlingPos+=SlingVel*Delta;
 FVector Dir=SlingPos-Hook;Dir=Dir.GetSafeNormal()*SlingLength;SlingPos=Hook+Dir;
 const FRotator Swing=FRotationMatrix::MakeFromZ(-Dir).Rotator();
 int32 K=0;
 for(UStaticMeshComponent* C:Slung)if(C && C->IsVisible())
 {C->SetWorldLocationAndRotation(SlingPos+Dir.GetSafeNormal()*(K*120.f+60.f),FRotator(Swing.Pitch,Heli->GetActorRotation().Yaw,Swing.Roll));++K;}
 if(Cable)
 {
  const float Len=SlingLength+FMath::Max(0,K-1)*120.f;
  Cable->SetWorldLocationAndRotation(Hook+Dir.GetSafeNormal()*Len*.5f,Swing);Cable->SetWorldScale3D(FVector(.03f,.03f,Len/100.f));
  Cable->SetVisibility(K>0);
 }
}
void ANeonCityAerialTour::Release(APawn* Heli)
{
 UStaticMeshComponent* Bottom=nullptr;for(UStaticMeshComponent* C:Slung)if(C && C->IsVisible())Bottom=C;
 if(!Bottom)return;
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Crate=GetWorld()->SpawnActor<AStaticMeshActor>(Bottom->GetComponentLocation(),Bottom->GetComponentRotation(),P);
 if(!Crate)return;
 Crate->SetMobility(EComponentMobility::Movable);
 auto* M=Crate->GetStaticMeshComponent();M->SetStaticMesh(Bottom->GetStaticMesh());M->SetCollisionProfileName(TEXT("PhysicsActor"));
 M->SetSimulatePhysics(true);M->SetPhysicsLinearVelocity(SlingVel);M->SetMassOverrideInKg(NAME_None,60.f,true);
 Crate->Tags.Add(TEXT("WL.LeisureCrate"));
 Bottom->SetVisibility(false);--Loaded;Falling=Crate;FallSeconds=0;
 PlayCue(TEXT("Gate"),.7f);
}
void ANeonCityAerialTour::TickAirdrop(float Delta,APlayerController* Player,APawn* Pawn)
{
 FNeonCityActivityHUDState S;S.Title=Title;S.Accent=Accent;S.Clock=FText::FromString(FormatTime(Progress.ActivitySeconds));
 const bool Heli=Classify(Pawn)==ENeonCityLeisureVehicle::Aircraft;
 if(Heli)UpdateSling(Delta,Pawn);
 const int32 I=Progress.ActivityCheckpoint;
 if(Falling.IsValid())
 {
  // score the crate once it has come to rest (or after 10 s)
  FallSeconds+=Delta;
  auto* M=Falling->GetStaticMeshComponent();
  if((FallSeconds>1.5f && M->GetPhysicsLinearVelocity().Size()<20) || FallSeconds>10)
  {
   const int32 Points=ScoreLanding(I,Falling->GetActorLocation());Score+=Points;
   const float D=FVector::Dist2D(Falling->GetActorLocation(),Stops[I].Location)/100.f;
   if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())
    HUD->Banner(FText::FromString(Grades[Points]),FText::FromString(FString::Printf(TEXT("%s  ·  %.1f m from the centre"),*Stops[I].Label.ToString(),D)),Points?Accent:FLinearColor(.9f,.3f,.2f),2.5f);
   PlayCue(Points?TEXT("Coin"):TEXT("Beep"));
   Falling->SetLifeSpan(60);Falling.Reset();++Progress.ActivityCheckpoint;GuideNext();
   if(Progress.ActivityCheckpoint>=Stops.Num())
   {
    const int64 Pay=RewardPerStop*Score+(Score>0?FinishBonus:0);
    Complete(Pay,Progress.ActivitySeconds,FText::FromString(Score>=Stops.Num()*3?TEXT("PERFECT DROPS"):TEXT("DROPS DELIVERED")),
     FText::FromString(FString::Printf(TEXT("score %d / %d  ·  %s  ·  $%lld"),Score,Stops.Num()*3,*FormatTime(Progress.ActivitySeconds),Pay)));
    return;
   }
  }
  S.Sub=FText::FromString(TEXT("Crate falling..."));
 }
 else if(Stops.IsValidIndex(I) && Heli)
 {
  const FVector At=Pawn->GetActorLocation();const float Off=FVector::Dist2D(At,Stops[I].Location);
  const float Height=At.Z-Stops[I].Location.Z;const float Kmh=Pawn->GetVelocity().Size()*.036f;
  const bool Steady=(SlingVel-Pawn->GetVelocity()).Size()<=SteadySwing;
  const bool Over=Off<=DropRadius && Height>=DropMinHeight && Height<=DropMaxHeight && Kmh<=DropMaxKmh && Steady;
  Hold=Over?Hold+Delta:0.f;
  S.Sub=FText::FromString(FString::Printf(TEXT("Drop %d / %d  ·  %s"),I+1,Stops.Num(),*Stops[I].Label.ToString()));
  S.Lines.Add(FText::FromString(FString::Printf(TEXT("%.0f m off  ·  %.0f m up  ·  %.0f km/h"),Off/100,Height/100,Kmh)));
  S.Lines.Add(FText::FromString(Off>DropRadius?TEXT("Fly over the target"):Height>DropMaxHeight?TEXT("Come lower (under 60 m)"):Height<DropMinHeight?TEXT("Too low: keep the crates off the ground"):Kmh>DropMaxKmh?TEXT("Slow to a hover"):!Steady?TEXT("Let the load stop swinging"):TEXT("Hold the hover... releasing")));
  S.bFraming=Off<=DropRadius*1.5f;S.FramingProgress=Hold/HoldSeconds;S.FramingLabel=FText::FromString(TEXT("Release"));
  if(Hold>=HoldSeconds){Release(Pawn);Hold=0;}
 }
 S.Lines.Add(FText::FromString(FString::Printf(TEXT("Score %d  ·  crates left %d"),Score,Loaded)));
 S.Progress=Stops.Num()?float(Progress.ActivityCheckpoint)/Stops.Num():-1.f;
 ShowHUD(S);
}
