#include "Combat/ThreatSubsystem.h"
#include "Combat/DamageSystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
void UThreatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Collection.InitializeDependency<UDamageSystem>();
 GetWorld()->GetSubsystem<UDamageSystem>()->OnDamage.AddDynamic(this,&UThreatSubsystem::DamageApplied);
}
void UThreatSubsystem::Deinitialize()
{
 if (auto* Damage=GetWorld()->GetSubsystem<UDamageSystem>()) { Damage->OnDamage.RemoveAll(this); }
 OnThreat.Clear(); Super::Deinitialize();
}
FGuid UThreatSubsystem::Publish(FNeonCityThreatEvent Event)
{
 if (GetWorld()->GetNetMode()==NM_Client || Event.Location.ContainsNaN() || !FMath::IsFinite(Event.Radius) || Event.Radius<=0) { return FGuid(); }
 if ((Event.SourceActor.IsValid() && Event.SourceActor->GetWorld()!=GetWorld())
  || (Event.Instigator.IsValid() && Event.Instigator->GetWorld()!=GetWorld())) { return FGuid(); }
 Event.Radius=FMath::Min(Event.Radius,5000.f); Event.Id=FGuid::NewGuid(); Event.Time=GetWorld()->GetTimeSeconds();
 OnThreat.Broadcast(Event); return Event.Id;
}
FGuid UThreatSubsystem::EmitGunshot(AActor* Source,AController* Instigator,FVector Location)
{
 if (!IsValid(Source) || !Source->HasAuthority()) { return FGuid(); }
 FNeonCityThreatEvent Event; Event.Type=ENeonCityThreatType::Gunshot; Event.SourceActor=Source;
 Event.Instigator=Instigator; Event.Location=Location; Event.Radius=2200;
 return Publish(Event);
}
FGuid UThreatSubsystem::EmitNearbyDanger(AActor* Source,AController* Instigator,FVector Location,float Radius)
{
 if (!IsValid(Source) || !Source->HasAuthority()) { return FGuid(); }
 FNeonCityThreatEvent Event; Event.SourceActor=Source; Event.Instigator=Instigator; Event.Location=Location; Event.Radius=Radius;
 return Publish(Event);
}
FGuid UThreatSubsystem::EmitExplosion(AActor* Source,AController* Instigator,FVector Location,float Radius)
{
 if (!IsValid(Source) || !Source->HasAuthority()) { return FGuid(); }
 FNeonCityThreatEvent Event; Event.Type=ENeonCityThreatType::Explosion; Event.SourceActor=Source; Event.Instigator=Instigator;
 Event.Location=Location; Event.Radius=Radius; Event.DamageType=ENeonCityDamageType::Explosion;
 return Publish(Event);
}
void UThreatSubsystem::DamageApplied(AActor* Victim,const FNeonCityDamageEvent& Damage)
{
 // Admin setters/death fixtures are not violence. Environmental harm can still warn bystanders.
 if (!IsValid(Victim) || Damage.DamageType==ENeonCityDamageType::Administrative) { return; }
 FNeonCityThreatEvent Event; Event.Type=ENeonCityThreatType::Attack; Event.Victim=Victim;
 Event.Instigator=Damage.Instigator; Event.SourceActor=Damage.SourceActor; Event.DamageType=Damage.DamageType;
 Event.DamageAmount=Damage.DamageAmount; Event.CombatContextID=Damage.CombatContextID; Event.Disposition=Damage.Disposition; Event.OriginalAggressor=Damage.OriginalAggressor;
 Event.Location=Victim->GetActorLocation(); Publish(Event);
}
