#include "Physics/PhysicalImpactSubsystem.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/ImpactDamageData.h"
#include "Physics/PhysicalMaterialResponseData.h"
#include "Physics/ImpactResponseInterface.h"
#include "Physics/CharacterImpactComponent.h"
#include "Physics/DynamicSurfaceInteractionInterface.h"
#include "Physics/DestructiblePropComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/Weapons/ProjectileBase.h"
#include "Combat/DamageSystem.h"
#include "Combat/ThreatSubsystem.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleInterface.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
namespace NeonCityImpact
{
 EImpactTargetKind Kind(AActor* A)
 {
  if(Cast<ACharacter>(A)) return EImpactTargetKind::Character;
  if(A->Implements<UVehicleInterface>()) return EImpactTargetKind::Vehicle;
  const auto* Prop=A->FindComponentByClass<UDestructiblePropComponent>();
  return Prop && Prop->Mode!=EPhysicsPropMode::Static?EImpactTargetKind::Prop:EImpactTargetKind::World;
 }
 float Mass(AActor* A)
 {
  if(auto* C=A->FindComponentByClass<UPhysicalImpactComponent>()) return C->GetContactMass();
  if(auto* C=Cast<ACharacter>(A)) return FMath::Clamp(C->GetCharacterMovement()->Mass,30.f,200.f);
  if(auto* Prop=A->FindComponentByClass<UDestructiblePropComponent>()) return Prop->Mode==EPhysicsPropMode::Static?0:Prop->Mass;
  if(auto* Root=Cast<UPrimitiveComponent>(A->GetRootComponent()); Root && Root->IsSimulatingPhysics()) return FMath::Clamp(Root->GetMass(),1.f,10000.f);
  return A->Implements<UVehicleInterface>()?1450:0;
 }
 AController* Driver(AActor* A) { if(auto* Pawn=Cast<APawn>(A)) return Pawn->GetController(); return A->GetInstigatorController(); }
}
void UPhysicalImpactSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection); Collection.InitializeDependency<UDamageSystem>();
 Tuning=NewObject<UImpactDamageData>(this); Materials=NewObject<UPhysicalMaterialResponseData>(this);
}
void UPhysicalImpactSubsystem::Submit(UPhysicalImpactComponent* Source,AActor* Other,const FHitResult& Hit)
{
 if(!IsValid(Source) || !IsValid(Other) || !Source->GetOwner()->HasAuthority() || Other==Source->GetOwner() || Other->GetWorld()!=GetWorld()
  || !Hit.bBlockingHit || Hit.bStartPenetrating || Hit.ImpactNormal.ContainsNaN() || Hit.ImpactPoint.ContainsNaN() || Source->IsSuppressed()) return;
 auto* OtherSource=Other->FindComponentByClass<UPhysicalImpactComponent>(); if(OtherSource && OtherSource->IsSuppressed()) return;
 // Projectile payloads already own damage. Their query contacts are not road-crash masses.
 if(Other->IsA<AProjectileBase>()) return;
 if(!Tuning || !Tuning->IsValidProfile()) return;
 AActor* Owner=Source->GetOwner(); const float Now=GetWorld()->GetTimeSeconds();
 // Support contacts are neither a road impact nor a crime. Side/high-relative-speed contacts continue below.
 if(NeonCityDynamicSurfaces::IsSupportContact(Owner,Cast<ACharacter>(Other),Hit)) return;
 const auto* PropVelocity=Other->FindComponentByClass<UDestructiblePropComponent>();
 const FVector Relative=Source->GetContactVelocity()-(OtherSource?OtherSource->GetContactVelocity():PropVelocity?PropVelocity->GetImpactVelocity():Other->GetVelocity());
 if(Relative.ContainsNaN()) return;
 const FVector Normal=Hit.ImpactNormal.GetSafeNormal();
 const float Speed=FMath::Clamp(FVector::DotProduct(Relative,-Normal),0.f,Tuning->MaximumSpeed);
 // Suspension/resting floor contacts and small spawn drops are not road crashes.
 if(Speed<Tuning->MinimumSpeed || (Normal.Z>.65f && FMath::Abs(Relative.Z)<FMath::Clamp(Source->MinimumFloorImpactSpeed,150.f,1000.f))) return;
 Pairs.RemoveAll([&](const auto& P){ return !P.A.IsValid() || !P.B.IsValid() || P.Until<Now; });
 if(Pairs.ContainsByPredicate([&](const auto& P){ return (P.A.Get()==Owner && P.B.Get()==Other) || (P.B.Get()==Owner && P.A.Get()==Other); })) return;
 if(Pairs.Num()>=256) Pairs.RemoveAt(0); Pairs.Add({Owner,Other,Now+Tuning->PairCooldown}); ++ImpactCount;
 FPhysicalImpactEvent Event; Event.SourceActor=Owner; Event.TargetActor=Other; Event.Instigator=NeonCityImpact::Driver(Owner);
 Event.RelativeVelocity=Relative; Event.ImpactSpeed=Speed; Event.ImpactDirection=-Normal; Event.ImpactNormal=Normal;
 Event.SourceMass=Source->GetContactMass(); Event.TargetMass=NeonCityImpact::Mass(Other); Event.ImpactLocation=Hit.ImpactPoint; Event.Timestamp=Now;
 const UPhysicalMaterial* Surface=Hit.PhysMaterial.Get();
 if(!Surface) if(auto* HitBody=Hit.GetComponent()) if(auto* Instance=HitBody->GetBodyInstance(Hit.BoneName)) Surface=Instance->GetSimplePhysicalMaterial();
 Event.SurfaceType=Surface?Surface->SurfaceType.GetValue():SurfaceType_Default;
 const float Scale=Materials->Get(Event.SurfaceType).DamageScale;
 const auto Kind=NeonCityImpact::Kind(Other);
 Event.Severity=Tuning->Severity(Speed*FMath::Sqrt(FMath::Clamp(Event.SourceMass/1450.f,.4f,2.f)));
 Deliver(Event,Kind,Tuning->Damage(Speed,Event.SourceMass,Event.TargetMass,Kind,Scale));
 // Both recipients use the same contact, each with its own receiving mass. Chaos supplies the physical impulse once.
 FPhysicalImpactEvent Reverse=Event; Reverse.SourceActor=Other; Reverse.TargetActor=Owner; Reverse.Instigator=NeonCityImpact::Driver(Other);
 Reverse.SourceMass=Event.TargetMass; Reverse.TargetMass=Event.SourceMass; Reverse.RelativeVelocity=-Relative;
 Reverse.ImpactDirection=-Event.ImpactDirection; Reverse.ImpactNormal=-Normal;
 const float OwnDamage=Event.TargetMass>0?Tuning->Damage(Speed,Event.TargetMass,Event.SourceMass,EImpactTargetKind::Vehicle,Scale):Tuning->Damage(Speed,Event.SourceMass,0,EImpactTargetKind::World,Scale);
 if(OwnDamage>.01f && FParse::Param(FCommandLine::Get(),TEXT("NeonCityCruiseDiagnostic")))
  UE_LOG(LogTemp,Warning,TEXT("Cruise actual damaging contact: vehicle=%s other=%s component=%s damage=%.4f speed=%.1f impact=%s normal=%s"),*GetNameSafe(Owner),*GetNameSafe(Other),*GetNameSafe(Hit.GetComponent()),OwnDamage,Speed,*Hit.ImpactPoint.ToString(),*Normal.ToString());
 Deliver(Reverse,EImpactTargetKind::Vehicle,OwnDamage);
 if(Event.Severity>=EImpactSeverity::Medium)
  GetWorld()->GetSubsystem<UThreatSubsystem>()->EmitNearbyDanger(Owner,Event.Instigator.Get(),Hit.ImpactPoint,1800);
}
void UPhysicalImpactSubsystem::Deliver(FPhysicalImpactEvent Event,EImpactTargetKind Kind,float Amount)
{
 auto* Target=Event.TargetActor.Get(); if(!IsValid(Target)) return;
 auto* Health=Target->FindComponentByClass<UHealthComponent>();
 if(Kind!=EImpactTargetKind::World && Health && !Health->IsDead())
 {
  if(Kind==EImpactTargetKind::Character && Event.Severity>=EImpactSeverity::High)
   Amount=FMath::Max(Amount,Health->GetCurrentHealth()-(Event.Severity==EImpactSeverity::Extreme?0:.2f*Health->GetMaxHealth()));
  FNeonCityDamageEvent Damage; Damage.DamageAmount=Amount; Damage.DamageType=Event.SourceMass>0?ENeonCityDamageType::Impact:ENeonCityDamageType::Environmental;
  Damage.SourceActor=Event.SourceActor.Get(); Damage.Instigator=Event.Instigator.Get(); Damage.HitLocation=Event.ImpactLocation;
  if(Amount>0) Event.AppliedDamage=GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(Target,Damage);
 }
 if(auto* Character=Target->FindComponentByClass<UCharacterImpactComponent>()) Character->ReceiveImpact(Event);
 if(auto* Prop=Target->FindComponentByClass<UDestructiblePropComponent>()) Prop->ReceiveImpact(Event);
 if(auto* Vehicle=Target->FindComponentByClass<UVehicleDamageComponent>()) Vehicle->NotifyPhysicalImpact(Event.ImpactSpeed,Event.AppliedDamage,Event.ImpactLocation);
 if(auto* Component=Target->FindComponentByClass<UPhysicalImpactComponent>()) Component->Notify(Event);
 if(Target->Implements<UImpactResponseInterface>()) IImpactResponseInterface::Execute_ReceivePhysicalImpact(Target,Event);
 if(RecentEvents.Num()>=64) RecentEvents.RemoveAt(0); RecentEvents.Add(Event);
 OnImpact.Broadcast(Event);
}
void UPhysicalImpactSubsystem::ResetForLoad() { Pairs.Reset(); RecentEvents.Reset(); OnReset.Broadcast(); }

bool UPhysicalImpactSubsystem::ConfigureProfiles(UImpactDamageData* DamageProfile,UPhysicalMaterialResponseData* SurfaceProfile)
{
 if(!IsValid(DamageProfile) || !DamageProfile->IsValidProfile() || !IsValid(SurfaceProfile)) return false;
 Tuning=DamageProfile; Materials=SurfaceProfile; return true;
}
