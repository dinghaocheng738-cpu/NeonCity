#include "Adventure/CoastalWildlife.h"
#include "Adventure/MarineEcologySubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Fishing/FishData.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

ACoastalWildlife::ACoastalWildlife()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.033f;
 Target=CreateDefaultSubobject<USphereComponent>(TEXT("CaptureTarget"));SetRootComponent(Target);
 Target->InitSphereRadius(22);Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Target->SetCollisionResponseToAllChannels(ECR_Ignore);Target->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wildlife"));Visual->SetupAttachment(Target);
 Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCanEverAffectNavigation(false);
 AnimatedBody=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedMarineBody"));AnimatedBody->SetupAttachment(Target);
 AnimatedBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);AnimatedBody->SetCanEverAffectNavigation(false);
 LeftWing=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftWing"));LeftWing->SetupAttachment(Visual);LeftWing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 RightWing=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightWing"));RightWing->SetupAttachment(Visual);RightWing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ACoastalWildlife::BeginPlay()
{
 Super::BeginPlay();Centre=GetActorLocation();Angle=Phase;
 Radius=FMath::Clamp(Radius,50.f,5000.f);Speed=FMath::Clamp(Speed,10.f,1200.f);
 Heading=FRotator(0,Phase*57.29578f,0).Vector();Steering=Heading;
 DecisionClock=FMath::Fmod(FMath::Abs(Phase),.16f);
 if(!bBird)GetWorld()->GetSubsystem<UMarineEcologySubsystem>()->RegisterFish(this);
 AnimatedBody->bEnableUpdateRateOptimizations=true;
 AnimatedBody->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
 if(Species && !Visual->GetStaticMesh())Visual->SetStaticMesh(Species->Mesh.LoadSynchronous());
 if(AnimatedBody->GetSkeletalMeshAsset() && SwimAnimation)
 {
  Visual->SetVisibility(false);AnimatedBody->PlayAnimation(SwimAnimation,true);AnimatedBody->SetPosition(Phase,false);
  AnimatedBody->SetPlayRate(.85f+FMath::Fmod(Phase,.3f));
 }
 if(bBird)
 {
  Target->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  // The rejected low-poly bird is not loaded. Birds require the separate reviewed skeletal presentation.
 }
}
void ACoastalWildlife::Tick(float Delta)
{
 Super::Tick(Delta);if(bCaught || !FMath::IsFinite(Delta) || Delta<=0)return;
 if(Death.Active())
 {
  auto* PC=GetWorld()->GetFirstPlayerController();auto* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
  FVector At=GetActorLocation();FQuat Rotation=GetActorQuat();
  const float Distance=Camera?FVector::Distance(At,Camera->GetCameraLocation()):10000;
  SetActorTickInterval(Distance<3000?1.f/30:.25f);
  float Surface=At.Z+1000;FVector Current=FVector::ZeroVector;
  GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->SampleWater(At,Surface,Current);
  FHitResult Ground;FCollisionQueryParams Q(SCENE_QUERY_STAT(DeadFishSeabed),false,this);
  const bool Found=GetWorld()->LineTraceSingleByObjectType(Ground,At+FVector(0,0,20),At-FVector(0,0,200),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
  Death.Step(GetWorld()->GetTimeSeconds(),Delta,Current*.25f,Found?float(Ground.ImpactPoint.Z+8):-100000.f,Surface-8,At,Rotation);
  SetActorLocationAndRotation(At,Rotation);AnimatedBody->SetPlayRate(Death.TailStrength);
  if(Death.Expired(Distance,HookTicket.IsValid())){bCaught=true;Destroy();}return;
 }
 if(bLanded)return;
 if(bHooked)
 {
  const FVector Tow=HookTarget-GetActorLocation();SetActorLocation(HookTarget);
  if(!Tow.IsNearlyZero())SetActorRotation(Tow.Rotation());return;
 }
 if(!bBird)
 {
  const float Step=FMath::Min(Delta,.4f);const FVector P=GetActorLocation();
  auto* PC=GetWorld()->GetFirstPlayerController();auto* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
  const float Distance=Camera?FVector::DistSquared(P,Camera->GetCameraLocation()):MAX_flt;
  SetActorTickInterval(Distance<FMath::Square(3000.f)?1.f/30:Distance<FMath::Square(9000.f)?.1f:.33f);
  DecisionClock-=Step;Alarm=FMath::Max(0.f,Alarm-Step*.4f);
  if(DecisionClock<=0)
  {
   DecisionClock=Distance<FMath::Square(4000.f)?.16f:.4f;
   const float Time=GetWorld()->GetTimeSeconds();
   const FVector Home=Centre-P;
   Steering=Heading+GetWorld()->GetSubsystem<UMarineEcologySubsystem>()->SchoolDirection(this,P,Heading);
   // Gentle exploration around the habitat; turns follow velocity, never a positional orbit.
   Steering+=FVector(FMath::Sin(Time*.27f+Phase)*.18f,FMath::Cos(Time*.21f+Phase)*.18f,FMath::Sin(Time*.17f+Phase)*.08f);
   const float Edge=FMath::Clamp((Home.Size2D()-Radius*.55f)/(Radius*.45f),0.f,2.f);
   Steering+=Home.GetSafeNormal2D()*Edge*2;
   Steering.Z+=FMath::Clamp(Home.Z/FMath::Max(VerticalRange,30.f),-1.f,1.f)*.65f;
   if(HookTicket.IsValid())Steering=(HookTarget-P).GetSafeNormal()*2;
   if(const APawn* Player=PC?PC->GetPawn():nullptr)
   {
    const FVector Away=P-Player->GetActorLocation();
    // A slow diver can approach within collector reach. Fast swimming startles
    // the school much earlier, while personal-space avoidance is always active.
    const float ThreatRadius=105+FMath::Clamp((Player->GetVelocity().Size()-70.f)*.85f,0.f,420.f);
    if(Away.SizeSquared()<FMath::Square(ThreatRadius))
    { Alarm=1;Steering+=Away.GetSafeNormal()*3.5f; }
   }
   FCollisionQueryParams Q(SCENE_QUERY_STAT(MarineAvoidance),false,this);FHitResult Obstacle;
   const FVector Ahead=P+Heading*FMath::Max(130.f,Speed*1.1f);
   if(GetWorld()->SweepSingleByObjectType(Obstacle,P,Ahead,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(18),Q))
    Steering+=Obstacle.ImpactNormal*3.5f+FVector::UpVector*.7f;
   FHitResult Floor;
   if(GetWorld()->LineTraceSingleByObjectType(Floor,P,P-FVector(0,0,110),FCollisionObjectQueryParams(ECC_WorldStatic),Q))
    Steering.Z+=1.5f*(1-Floor.Time);
   FWaterSurfaceSample Water;
   if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(P,Water) && Water.DepthAt(P)<MinimumDepth+70)
    Steering.Z-=2*FMath::Clamp((MinimumDepth+70-Water.DepthAt(P))/100.f,0.f,2.f);
   // Reef avoidance must not turn large fish into vertical buoys. Keep a
   // forward swimming envelope while allowing a short, steeper escape climb.
   const FVector Horizontal=Steering.GetSafeNormal2D(UE_SMALL_NUMBER,Heading.GetSafeNormal2D());
   const float MaxPitch=Alarm>.1f?45.f:28.f;
   const float Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Steering.Z,Steering.Size2D())),-MaxPitch,MaxPitch);
   Steering=FRotator(Pitch,Horizontal.Rotation().Yaw,0).Vector();
  }
  Heading=FMath::VInterpNormalRotationTo(Heading,Steering,Step,Alarm>0.1f?130.f:48.f);
  const float SwimSpeed=Speed*(1+Alarm*.9f)*(HookTicket.IsValid()?1.6f:1.f);
  const FVector Desired=P+Heading*SwimSpeed*Step;
  FWaterSurfaceSample Water;
  FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(MarineTravel),false,this);
  if(GetWorld()->GetSubsystem<UMarineWorldSubsystem>()->QueryWater(Desired,Water) && Water.DepthAt(Desired)>35
   && !GetWorld()->SweepSingleByObjectType(Hit,P,Desired,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(14),Q))
   SetActorLocation(Desired);
  else { Steering=(Heading*-1+FVector(0,0,-.2)).GetSafeNormal();DecisionClock=.25f; }
  SetActorRotation(Heading.Rotation());
  AnimatedBody->SetPlayRate((.85f+FMath::Fmod(FMath::Abs(Phase),.3f))*(1+Alarm*.75f));
  return;
 }
 Angle+=FMath::Min(Delta,.1f)*Speed/Radius;
 const FVector Desired=Centre+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius*.72f,FMath::Sin(Angle*2+Phase)*35);
 const FVector Direction=Desired-GetActorLocation();
 SetActorLocation(Desired);SetActorRotation(FMath::RInterpTo(GetActorRotation(),Direction.Rotation(),Delta,3));
 Visual->SetRelativeRotation(FRotator(0,FMath::Sin(Angle*18)*5,bBird?FMath::Sin(Angle*5)*12:0));
 if(bBird)
 {
  const float Flap=FMath::Sin(GetWorld()->GetTimeSeconds()*7+Phase)*30;
  LeftWing->SetRelativeRotation(FRotator(0,0,Flap));RightWing->SetRelativeRotation(FRotator(0,0,-Flap));
 }
}
void ACoastalWildlife::EndPlay(EEndPlayReason::Type Reason)
{
 if(!bBird)if(auto* Ecology=GetWorld()->GetSubsystem<UMarineEcologySubsystem>())Ecology->UnregisterFish(this);
 Super::EndPlay(Reason);
}
bool ACoastalWildlife::TryCapture(UNeonCityFishingSubsystem* Inventory)
{
 if(bCaught || Death.Active() || HookTicket.IsValid() || bBird || !bCapturable || !Species || !Inventory || !Inventory->HasPouchSpace())return false;
 FNeonCityCaughtFish Fish;Fish.FishID=FGuid::NewGuid();Fish.SpeciesID=Species->SpeciesID;
 Fish.WeightKg=(Species->WeightKg.X+Species->WeightKg.Y)*.5;Fish.SizeCm=(Species->SizeCm.X+Species->SizeCm.Y)*.5;
 Fish.CaughtAt=FDateTime::UtcNow();Fish.FishingPointID=TEXT("Island.DiveReef");
 if(!Inventory->StoreCatch(Fish))return false;
 bCaught=true;Destroy();return true;
}
bool ACoastalWildlife::CanHook() const
{return HasAuthority() && !bCaught && !Death.Active() && !HookTicket.IsValid() && !bBird && bCapturable && Species && Species->IsValidProfile();}
void ACoastalWildlife::StartleAt(FVector Point,float Range)
{if(!bBird && !bCaught && !Death.Active() && FVector::DistSquared(GetActorLocation(),Point)<FMath::Square(Range))Alarm=1;}
bool ACoastalWildlife::ReserveHook(FGuid Ticket,FNeonCityCaughtFish& Catch)
{
 if(!Ticket.IsValid() || !CanHook())return false;
 HookTicket=Ticket;HookTarget=GetActorLocation();Catch=FNeonCityCaughtFish();Catch.FishID=Ticket;Catch.SpeciesID=Species->SpeciesID;
 Catch.WeightKg=(Species->WeightKg.X+Species->WeightKg.Y)*.5f;Catch.SizeCm=(Species->SizeCm.X+Species->SizeCm.Y)*.5f;
 Catch.FishingPointID=SchoolID.IsNone()?FName(TEXT("Island.DiveReef")):SchoolID;return true;
}
bool ACoastalWildlife::MoveHook(FGuid Ticket,FVector At,bool Hooked,bool Landed)
{
 if(bCaught || HookTicket!=Ticket || !Ticket.IsValid() || At.ContainsNaN())return false;
 if(Death.Active() && (!Landed || !Death.Collectible()))return false;
 HookTarget=At;bHooked=Hooked;bLanded=Landed;SetActorHiddenInGame(Landed);return true;
}
bool ACoastalWildlife::FinishHook(FGuid Ticket,UNeonCityFishingSubsystem* Inventory,const FNeonCityCaughtFish& Catch,bool Keep)
{
 if(bCaught || HookTicket!=Ticket || !Ticket.IsValid())return false;
 if(Keep)
 {
  if(!bLanded || (Death.Active() && !Death.Collectible()) || !Inventory || Catch.FishID!=Ticket || Catch.SpeciesID!=Species->SpeciesID)return false;
  bCaught=true;if(!Inventory->StoreCatch(Catch)){bCaught=false;return false;}
  Destroy();return true;
 }
 HookTicket.Invalidate();bHooked=bLanded=false;Alarm=1;SetActorHiddenInGame(false);return true;
}
bool ACoastalWildlife::ReserveSpear(FGuid Ticket,FVector Hit,FVector Direction,FNeonCityCaughtFish& Catch)
{
 if(!HasAuthority() || bCaught || bBird || !bCapturable || !Species || !Ticket.IsValid() || HookTicket.IsValid())return false;
 if(Death.Active())
 {
  if(!Death.Collectible())return false;HookTicket=Ticket;Catch=FNeonCityCaughtFish();Catch.FishID=Ticket;Catch.SpeciesID=Species->SpeciesID;
  Catch.WeightKg=(Species->WeightKg.X+Species->WeightKg.Y)*.5f;Catch.SizeCm=(Species->SizeCm.X+Species->SizeCm.Y)*.5f;
  Catch.FishingPointID=SchoolID.IsNone()?FName(TEXT("Island.DiveReef")):SchoolID;
 }
 else if(!ReserveHook(Ticket,Catch))return false;
 if(!Death.Active())Death.Begin(GetWorld()->GetTimeSeconds(),GetActorLocation(),GetActorQuat(),Heading*Speed,Hit,Direction,Catch.SizeCm,GetTypeHash(Species->SpeciesID));
 else Death.Attach(GetActorLocation(),GetActorQuat(),Hit,Direction,Catch.SizeCm);
 bHooked=bLanded=false;return true;
}
bool ACoastalWildlife::GetSpearAttachment(FGuid Ticket,FTransform& Pose,bool& Collectible) const
{
 if(bCaught || !Ticket.IsValid() || HookTicket!=Ticket || !Death.Active())return false;
 Pose=Death.Attachment(GetActorLocation(),GetActorQuat());Collectible=Death.Collectible();return true;
}
bool ACoastalWildlife::PullSpear(FGuid Ticket,FVector Delta)
{
 FTransform Pose;bool Ready=false;if(!GetSpearAttachment(Ticket,Pose,Ready) || !Ready || Delta.ContainsNaN())return false;
 AddActorWorldOffset(Delta);Death.Velocity*=.8f;return true;
}
