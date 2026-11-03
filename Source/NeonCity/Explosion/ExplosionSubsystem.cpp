#include "Explosion/ExplosionSubsystem.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageInterface.h"
#include "Combat/ThreatSubsystem.h"
#include "Explosion/ExplosionImpulseUtility.h"
#include "Explosion/ExplosionResponseInterface.h"
#include "Physics/CharacterImpactComponent.h"
#include "World/WorldEventSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
void UNeonCityExplosionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Collection.InitializeDependency<UDamageSystem>();
 Collection.InitializeDependency<UThreatSubsystem>();
}
void UNeonCityExplosionSubsystem::Deinitialize()
{
 OnExplosion.Clear(); Recent.Reset(); Cooldowns.Reset(); DetonationTimes.Reset(); Super::Deinitialize();
}
bool UNeonCityExplosionSubsystem::CanDetonate(const AActor* Source,float CooldownSeconds) const
{
 if(!IsValid(Source) || !FMath::IsFinite(CooldownSeconds) || CooldownSeconds<=0) return true;
 const float* Until=Cooldowns.Find(TWeakObjectPtr<const AActor>(Source));
 return !Until || GetWorld()->GetTimeSeconds()>=*Until;
}
bool UNeonCityExplosionSubsystem::SetWeatherModifier(const FNeonCityWeatherModifier& Modifier)
{ if(!Modifier.IsValid()) return false; Weather=Modifier; return true; }
bool UNeonCityExplosionSubsystem::SubmitWaterImpact(const FNeonCityWaterExplosionRequest& Request)
{
 if(!GetWorld() || GetWorld()->GetNetMode()==NM_Client || Request.SurfacePoint.ContainsNaN() || !FMath::IsFinite(Request.Depth)) return false;
 // Interface only: full underwater propagation belongs to the swimming/water phase.
 ++WaterImpacts; return false;
}
void UNeonCityExplosionSubsystem::ResetForLoad()
{
 Recent.Reset(); Cooldowns.Reset(); DetonationTimes.Reset(); Weather=FNeonCityWeatherModifier();
 DetonationCount=RefusedCount=WaterImpacts=LastDamageTargets=LastImpulseTargets=LastLooseBodyTargets=0;
}
FNeonCityExplosionResult UNeonCityExplosionSubsystem::Detonate(const FRequest& Request)
{
 FNeonCityExplosionResult Result;
 UWorld* World=GetWorld();
 if(!World || World->GetNetMode()==NM_Client) { ++RefusedCount; Result.RefusedReason=TEXT("No authority."); return Result; }
 if(!Request.Data || !Request.Data->IsValidProfile()) { ++RefusedCount; Result.RefusedReason=TEXT("Invalid explosion profile."); return Result; }
 const UNeonCityExplosionData& Data=*Request.Data;
 if(Request.Center.ContainsNaN() || !FMath::IsFinite(Request.DamageOverride) || Request.ChainDepth<-1 || Request.ChainDepth>8)
 { ++RefusedCount; Result.RefusedReason=TEXT("Invalid detonation request."); return Result; }
 const float Now=World->GetTimeSeconds();
 DetonationTimes.RemoveAll([&](float Stamp){ return Now-Stamp>1.f; });
 if(DetonationTimes.Num()>=MaximumDetonationsPerSecond) { ++RefusedCount; Result.RefusedReason=TEXT("Explosion budget exhausted."); return Result; }
 if(Request.SourceActor && !CanDetonate(Request.SourceActor,Data.CooldownSeconds)) { ++RefusedCount; Result.RefusedReason=TEXT("Explosion cooldown active."); return Result; }
 const float BaseDamage=Request.DamageOverride>=0?Request.DamageOverride:Data.Damage;
 if(!FMath::IsFinite(BaseDamage) || BaseDamage<=0) { ++RefusedCount; Result.RefusedReason=TEXT("Explosion carries no damage."); return Result; }
 FNeonCityExplosionEvent Event;
 Event.ExplosionId=FGuid::NewGuid(); Event.DataID=Data.ExplosionID; Event.Center=Request.Center; Event.Damage=BaseDamage;
 Event.InnerRadius=Data.InnerRadius; Event.OuterRadius=Data.OuterRadius; Event.ImpulseStrength=Data.ImpulseStrength;
 Event.FireDuration=Data.FireDuration*Weather.FireScale; Event.SmokeDuration=Data.SmokeDuration*Weather.SmokeScale;
 Event.DamageType=Data.DamageType; Event.SourceType=Data.SourceType; Event.Instigator=Request.Instigator; Event.SourceActor=Request.SourceActor;
 Event.ChainDepth=Request.ChainDepth; Event.ParentExplosionId=Request.ParentExplosionId; Event.Timestamp=Now;
 // Bounded gather: overlaps are deduplicated, sorted by distance and processed up to the authored target cap.
 FCollisionQueryParams Query(SCENE_QUERY_STAT(NeonCityExplosion),false,Request.SourceActor);
 for(const auto& Ignored:Request.Ignored) if(Ignored.IsValid()) Query.AddIgnoredActor(Ignored.Get());
 TArray<FOverlapResult> Overlaps;
 World->OverlapMultiByObjectType(Overlaps,Request.Center,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeSphere(Data.OuterRadius),Query);
 TSet<const AActor*> IgnoredSet;
 if(Request.SourceActor) IgnoredSet.Add(Request.SourceActor);
 for(const auto& Ignored:Request.Ignored) if(Ignored.IsValid()) IgnoredSet.Add(Ignored.Get());
 TArray<AActor*> Candidates; TSet<AActor*> Seen;
 for(const FOverlapResult& Overlap:Overlaps)
 {
  AActor* Actor=Overlap.GetActor();
  if(!IsValid(Actor) || Seen.Contains(Actor) || IgnoredSet.Contains(Actor) || !Actor->Implements<UDamageInterface>()) continue;
  Seen.Add(Actor); Candidates.Add(Actor);
 }
 Candidates.Sort([&](const AActor& A,const AActor& B){ return FVector::DistSquared(A.GetActorLocation(),Request.Center)<FVector::DistSquared(B.GetActorLocation(),Request.Center); });
 if(Candidates.Num()>Data.MaximumTargets) Candidates.SetNum(Data.MaximumTargets);
 for(AActor* Actor:Candidates)
 {
  const FVector Location=Actor->GetActorLocation();
  const float Distance=FVector::Distance(Request.Center,Location);
  bool bOccluded=false;
  if(Data.bRequiresLineOfSight)
  {
   FHitResult Block;
   if(World->LineTraceSingleByChannel(Block,Request.Center,Location,Request.OcclusionChannel,Query) && Block.GetActor()!=Actor) bOccluded=true;
  }
  const float Occlusion=bOccluded?FMath::Clamp(Data.OccludedDamageFraction,0.f,1.f):1.f;
  const EImpactTargetKind Kind=UNeonCityExplosionData::KindOf(Actor);
  const float Falloff=UNeonCityExplosionData::Falloff(Distance,Data.InnerRadius,Data.OuterRadius,Data.DamageFalloff);
  const float Damage=BaseDamage*Falloff*Occlusion*Data.DamageMultiplier(Kind)*Weather.DamageScale;
  if(!FMath::IsFinite(Damage) || Damage<=0)
  {
   if(bOccluded) ++Result.RefusedTargets;
   continue;
  }
  FNeonCityDamageEvent Payload; Payload.DamageAmount=Damage; Payload.DamageType=Data.DamageType;
  Payload.Instigator=Request.Instigator; Payload.SourceActor=Request.SourceActor?Request.SourceActor:Actor; Payload.HitLocation=Location;
  const float Applied=World->GetSubsystem<UDamageSystem>()->ApplyDamage(Actor,Payload);
  if(Applied>0) { ++Result.DamageTargets; Result.TotalDamage+=Applied; }
  // Characters are never launched; the blast drives their bounded stagger / knockdown reaction instead.
  if(Applied>0 && Kind==EImpactTargetKind::Character && Data.Impulse.CharacterReactionSpeed>0)
   if(auto* Reaction=Actor->FindComponentByClass<UCharacterImpactComponent>())
   {
    FPhysicalImpactEvent Hit; Hit.SourceActor=Request.SourceActor; Hit.TargetActor=Actor; Hit.Instigator=Request.Instigator;
    Hit.ImpactDirection=FExplosionImpulseUtility::PushDirection(Request.Center,Location,Data.Impulse.UpwardBias);
    Hit.ImpactSpeed=Data.Impulse.CharacterReactionSpeed*Falloff*Occlusion; Hit.RelativeVelocity=Hit.ImpactDirection*Hit.ImpactSpeed;
    Hit.ImpactLocation=Location; Hit.ImpactNormal=-Hit.ImpactDirection; Hit.Timestamp=Now; Hit.AppliedDamage=Applied;
    Hit.Severity=Falloff>.6f?EImpactSeverity::Extreme:Falloff>.3f?EImpactSeverity::High:Falloff>.12f?EImpactSeverity::Medium:EImpactSeverity::Low;
    Reaction->ReceiveImpact(Hit); ++Result.CharacterReactions;
   }
  if(Request.bApplyImpulse && Data.ImpulseStrength>0)
  {
   if(auto* Body=Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
   {
    FExplosionImpulseUtility::FRequest Impulse;
    Impulse.Center=Request.Center; Impulse.Target=Location; Impulse.Kind=Kind; Impulse.Falloff=Falloff;
    Impulse.ImpulseStrength=Data.ImpulseStrength; Impulse.OcclusionFraction=bOccluded?Data.OccludedImpulseFraction:1;
    Impulse.WeatherScale=Weather.ImpulseScale*Data.RainImpulseScale;
    if(FExplosionImpulseUtility::Apply(Body,Impulse,Data.Impulse)) ++Result.ImpulseTargets;
   }
  }
  if(Request.bNotifyResponses && Actor->GetClass()->ImplementsInterface(UExplosionResponseInterface::StaticClass()))
   if(IExplosionResponseInterface::Execute_ReceiveExplosion(Actor,Event)) ++Result.ResponseTargets;
 }
 if(Request.bApplyImpulse && Data.ImpulseStrength>0 && Data.Impulse.LooseBodyDeltaVelocity>0 && Data.Impulse.MaximumLooseBodies>0)
  Result.LooseBodyTargets=PushLooseBodies(Request,Data,Overlaps,Seen,IgnoredSet,Query);
 if(Request.bEmitDangerSignal && Data.bEmitDangerSignal)
 {
  const float Radius=FMath::Clamp(Data.OuterRadius*Data.DangerRadiusScale,500.f,5000.f);
  World->GetSubsystem<UThreatSubsystem>()->EmitExplosion(Request.SourceActor?Request.SourceActor:Event.SourceActor.Get(),Request.Instigator,Request.Center,Radius);
 }
 if(Request.bPublishWorldEvent)
  if(auto* Events=World->GetSubsystem<UWorldEventSubsystem>())
   Events->Publish(ENeonCityWorldEventType::Explosion,Request.Center,Request.SourceActor?Request.SourceActor:Request.Instigator?Request.Instigator->GetPawn():nullptr,Request.Instigator,BaseDamage,Data.ExplosionID);
 if(Request.SourceActor) Cooldowns.Add(TWeakObjectPtr<const AActor>(Request.SourceActor),Now+Data.CooldownSeconds);
 DetonationTimes.Add(Now); ++DetonationCount;
 LastDamageTargets=Result.DamageTargets; LastImpulseTargets=Result.ImpulseTargets; LastLooseBodyTargets=Result.LooseBodyTargets;
 if(Recent.Num()>=RecentCapacity) Recent.RemoveAt(0);
 Recent.Add(Event);
 Result.bApplied=true; Result.ExplosionId=Event.ExplosionId; Result.ChainDepth=Event.ChainDepth;
 OnExplosion.Broadcast(Event);
 return Result;
}
int32 UNeonCityExplosionSubsystem::PushLooseBodies(const FRequest& Request,const UNeonCityExplosionData& Data,const TArray<FOverlapResult>& Overlaps,
 const TSet<AActor*>& Targets,const TSet<const AActor*>& Ignored,const FCollisionQueryParams& Query)
{
 // Every simulating body in the blast that is not a combat target already handled above (debris, dropped
 // props, earlier blast fragments). Nearest first, one impulse per body, bounded count, walls shelter.
 struct FBody { UPrimitiveComponent* Body; float Distance; };
 TArray<FBody> Bodies; TSet<const UPrimitiveComponent*> Seen;
 for(const FOverlapResult& Overlap:Overlaps)
 {
  UPrimitiveComponent* Body=Overlap.GetComponent(); AActor* Actor=Overlap.GetActor();
  if(!IsValid(Body) || !Body->IsSimulatingPhysics() || Seen.Contains(Body) || Ignored.Contains(Actor)) continue;
  Seen.Add(Body);
  // A combat target's root body was pushed with its own per-kind limit; its other bodies follow through constraints.
  if(IsValid(Actor) && Targets.Contains(Actor)) continue;
  if(IsValid(Actor) && Actor->IsA<APawn>()) continue;
  Bodies.Add({Body,float(FVector::Distance(Request.Center,Body->Bounds.Origin))});
 }
 Bodies.Sort([](const FBody& A,const FBody& B){ return A.Distance<B.Distance; });
 if(Bodies.Num()>Data.Impulse.MaximumLooseBodies) Bodies.SetNum(Data.Impulse.MaximumLooseBodies);
 int32 Pushed=0;
 for(const FBody& Entry:Bodies)
 {
  const FVector Location=Entry.Body->Bounds.Origin;
  bool bOccluded=false;
  if(Data.bRequiresLineOfSight)
  {
   FHitResult Block;
   if(GetWorld()->LineTraceSingleByChannel(Block,Request.Center,Location,Request.OcclusionChannel,Query) && Block.GetComponent()!=Entry.Body && !(Block.GetComponent() && Block.GetComponent()->IsSimulatingPhysics())) bOccluded=true;
  }
  FExplosionImpulseUtility::FRequest Impulse;
  Impulse.Center=Request.Center; Impulse.Target=Location; Impulse.Kind=EImpactTargetKind::Prop; Impulse.bLooseBody=true;
  Impulse.Falloff=UNeonCityExplosionData::Falloff(Entry.Distance,Data.InnerRadius,Data.ImpulseRadius>0?Data.ImpulseRadius:Data.OuterRadius,1);
  Impulse.ImpulseStrength=Data.ImpulseStrength; Impulse.OcclusionFraction=bOccluded?Data.OccludedImpulseFraction:1;
  Impulse.WeatherScale=Weather.ImpulseScale*Data.RainImpulseScale;
  if(FExplosionImpulseUtility::Apply(Entry.Body,Impulse,Data.Impulse)) ++Pushed;
 }
 return Pushed;
}
bool UNeonCityExplosionSubsystem::DetonateProfile(FName DataID,FVector Center,AController* Instigator,AActor* SourceActor)
{
 FRequest Request; Request.Data=&NeonCityExplosions::Profile(DataID); Request.Center=Center;
 Request.Instigator=Instigator; Request.SourceActor=SourceActor; Request.Ignored.Add(SourceActor);
 return Detonate(Request).bApplied;
}