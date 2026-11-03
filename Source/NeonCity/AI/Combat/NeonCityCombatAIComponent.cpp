#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "AI/Combat/CoverPoint.h"
#include "AI/Combat/CoverQuery.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/CombatCollision.h"
#include "Combat/CombatContextSubsystem.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Presentation/CharacterWeaponPresentationComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
UNeonCityCombatAIComponent::UNeonCityCombatAIComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false; PrimaryComponentTick.TickInterval=.1f; }
void UNeonCityCombatAIComponent::BeginPlay()
{
 Super::BeginPlay(); Character=Cast<ACharacter>(GetOwner()); Combat=GetOwner()->FindComponentByClass<UCombatComponent>(); Health=GetOwner()->FindComponentByClass<UHealthComponent>();
 if(!Character.IsValid() || !Combat.IsValid() || !Health.IsValid() || !GetOwner()->HasAuthority()) { SetComponentTickEnabled(false); return; }
 Home=Character->GetActorLocation();
 if(auto* Presentation=GetOwner()->FindComponentByClass<UCharacterWeaponPresentationComponent>()) WeaponMount=Presentation->GetGrip();
 if(!WeaponMount)
 {
  WeaponMount=NewObject<USceneComponent>(GetOwner(),TEXT("AIWeaponMount")); GetOwner()->AddInstanceComponent(WeaponMount);
  WeaponMount->SetupAttachment(GetOwner()->GetRootComponent()); WeaponMount->SetRelativeLocation(FVector(25,18,35)); WeaponMount->RegisterComponent();
 }
 Health->OnHealthChanged.AddDynamic(this,&UNeonCityCombatAIComponent::HealthChanged);
 GetWorld()->GetSubsystem<UThreatSubsystem>()->OnThreat.AddDynamic(this,&UNeonCityCombatAIComponent::OnThreat);
 GetWorld()->GetSubsystem<UCombatContextSubsystem>()->OnReset.AddDynamic(this,&UNeonCityCombatAIComponent::ResetForLoad);
 Configure(Data,Random.GetInitialSeed());
}
bool UNeonCityCombatAIComponent::Configure(UCombatAIData* Profile,int32 Seed)
{
 if(bActive) Disengage(); Data=Profile; Random.Initialize(Seed);
 const bool Valid=!Data || Data->IsValidProfile(); if(!Valid) Data=nullptr;
 if(auto* Identity=GetOwner()->FindComponentByClass<UCombatTargetComponent>()) Identity->Faction=Data?Data->Faction:ENeonCityFaction::Civilian;
 if(Character.IsValid() && CanFight()) Character->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch=true;
 RefreshTick(); return Valid;
}
bool UNeonCityCombatAIComponent::CanFight() const { return Data && Data->CanFight && Data->Role!=ENeonCityCombatRole::Civilian; }
bool UNeonCityCombatAIComponent::ConfigureWeapon(TSubclassOf<AWeaponBase> Type)
{
 if(bActive || !Data || !Type || !GetDefault<AWeaponBase>(Type)->GetDefaultData()->IsValidProfile()) return false;
 auto* Profile=DuplicateObject<UCombatAIData>(Data,this); Profile->WeaponClass=Type; return Configure(Profile,Random.GetInitialSeed());
}
void UNeonCityCombatAIComponent::RefreshTick()
{
 const bool Enabled=Character.IsValid() && CanFight() && GetHealthState()!=ENeonCityCombatHealth::Dead && GetHealthState()!=ENeonCityCombatHealth::Downed;
 SetComponentTickEnabled(Enabled && (bActive || Data->Role==ENeonCityCombatRole::Hostile));
 const bool FarEvidence=bActive && Perception.HasLocation && Character.IsValid() && Data
  && FVector::DistSquared(Character->GetActorLocation(),Perception.LastKnownTargetLocation)>FMath::Square(Data->EngagementRange);
 if(Data) SetComponentTickInterval(bDistantLOD || !bActive || FarEvidence?Data->FarUpdateInterval:.1f);
}
ENeonCityCombatHealth UNeonCityCombatAIComponent::GetHealthState() const
{
 if(!Health.IsValid()) return ENeonCityCombatHealth::Healthy;
 const float Fraction=Health->GetCurrentHealth()/FMath::Max(Health->GetMaxHealth(),1.f);
 return Health->IsDead()?ENeonCityCombatHealth::Dead:Fraction<=.2f?ENeonCityCombatHealth::Downed:Fraction<=.3f?ENeonCityCombatHealth::Critical:Fraction<=.5f?ENeonCityCombatHealth::Injured:ENeonCityCombatHealth::Healthy;
}
void UNeonCityCombatAIComponent::SetState(ENeonCityCombatAIState Value)
{ if(Value==State) return; State=Value; StateAt=GetWorld()->GetTimeSeconds(); Visited|=1u<<uint8(Value); }
void UNeonCityCombatAIComponent::ReleaseCover()
{ if(Cover.IsValid()) Cover->Release(GetOwner()); Cover.Reset(); bAtPeek=false; if(Character.IsValid()) Character->UnCrouch(); }
void UNeonCityCombatAIComponent::Disengage()
{
 const bool WasActive=bActive; bActive=false; bMove=false; Target.Reset(); Perception=FCombatPerceptionData(); ReleaseCover();
 if(Combat.IsValid()) Combat->UnequipWeapon();
 if(WasActive && Character.IsValid()) { Character->GetCharacterMovement()->StopMovementImmediately(); Character->GetCharacterMovement()->MaxWalkSpeed=PreviousMoveSpeed; }
 if(WasActive) if(auto* Ecology=GetOwner()->FindComponentByClass<UNPCStateComponent>()) Ecology->SetState(ENeonCityNPCState::Return);
 SetState(ENeonCityCombatAIState::Inactive); ReacquireAfter=GetWorld()->GetTimeSeconds()+3; RefreshTick();
}
void UNeonCityCombatAIComponent::ResetForLoad() { if(CanFight()) { Disengage(); HealthChanged(0,0); } }
void UNeonCityCombatAIComponent::HealthChanged(float Current,float Maximum)
{
 const auto Condition=GetHealthState();
 if(Condition==ENeonCityCombatHealth::Dead || Condition==ENeonCityCombatHealth::Downed)
 {
  Disengage(); SetState(Condition==ENeonCityCombatHealth::Dead?ENeonCityCombatAIState::Dead:ENeonCityCombatAIState::Downed);
  if(auto* Threat=GetOwner()->FindComponentByClass<UNPCThreatComponent>()) Threat->ClearResponse();
  SetComponentTickEnabled(false);
 }
 else { if(State==ENeonCityCombatAIState::Dead || State==ENeonCityCombatAIState::Downed) SetState(ENeonCityCombatAIState::Inactive); RefreshTick(); }
}
bool UNeonCityCombatAIComponent::CanSee(AActor* Candidate,FVector& Position,FVector& Aim) const
{
 if(!UCombatTargetComponent::IsAliveTarget(Candidate) || UCombatTargetComponent::AreFriendly(GetOwner(),Candidate)) return false;
 // Reading a candidate transform is confined to this bounded sensor query, never the navigation/shooting decisions.
 Position=Candidate->GetActorLocation(); Aim=Position+FVector(0,0,35);
 const FVector Delta=Position-Character->GetActorLocation();
 if(Delta.SizeSquared()>FMath::Square(Data->SightRange) || FVector::DotProduct(Character->GetActorForwardVector(),Delta.GetSafeNormal2D())<FMath::Cos(FMath::DegreesToRadians(Data->SightHalfAngle))) return false;
 return NeonCityCover::ClearSight(GetWorld(),GetOwner(),Character->GetPawnViewLocation(),Aim,Candidate);
}
void UNeonCityCombatAIComponent::Acquire(AActor* Candidate,FVector Evidence,bool Seen)
{
 if(!CanFight() || (Candidate && UCombatTargetComponent::AreFriendly(GetOwner(),Candidate))) return;
 const bool NewEpisode=!bActive;
 Target=Candidate; Perception.HasLocation=true; Perception.HasVisualContact=Seen;
 Perception.LastKnownTargetLocation=Evidence; Perception.LastKnownAimPoint=Evidence+FVector(0,0,35);
 const float Now=GetWorld()->GetTimeSeconds(); Perception.LastEvidenceTime=Now;
 if(Seen) Perception.LastSeenTime=Now;
 if(NewEpisode)
 {
  PreviousMoveSpeed=Character->GetCharacterMovement()->MaxWalkSpeed; bActive=true; StartedAt=Now; NextShot=Now+Data->ReactionTime; BurstShots=0;
  if(auto* Ecology=GetOwner()->FindComponentByClass<UNPCStateComponent>()) { Ecology->SetState(ENeonCityNPCState::Alert); }
  SetState(ENeonCityCombatAIState::AcquireTarget);
 }
 RefreshTick();
}
void UNeonCityCombatAIComponent::OnThreat(const FNeonCityThreatEvent& Event)
{
 if(const auto* NPC=Cast<ANeonCityNPCCharacter>(GetOwner());NPC && NPC->IsPopulationDormant()) return;
 if(!CanFight() || !Character.IsValid() || !UCombatTargetComponent::IsAliveTarget(GetOwner()) || GetWorld()->GetTimeSeconds()<ReacquireAfter) return;
 if(Data->RequiresArmedThreat && Event.Type!=ENeonCityThreatType::Gunshot && Event.Type!=ENeonCityThreatType::Explosion
  && !(Event.Type==ENeonCityThreatType::Attack && (Event.DamageType==ENeonCityDamageType::Ballistic || Event.DamageType==ENeonCityDamageType::Explosion))) return;
 if(Event.Instigator.Get()==Character->GetController() || FVector::DistSquared(Event.Location,Character->GetActorLocation())>FMath::Square(Event.Radius)) return;
 AActor* Culprit=Event.Instigator.IsValid()?Event.Instigator->GetPawn():nullptr;
 if(Culprit && UCombatTargetComponent::AreFriendly(GetOwner(),Culprit)) return;
 const bool Direct=Event.Victim.Get()==GetOwner();
 const bool Ally=Event.Victim.IsValid() && UCombatTargetComponent::AreFriendly(GetOwner(),Event.Victim.Get());
 if(Data->PlayerResponseOnly && !Direct && !Ally && (!Event.Instigator.IsValid() || !Event.Instigator->IsPlayerController())) return;
 FVector Position,Aim; const bool Seen=CanSee(Culprit,Position,Aim);
 if(bActive) { if((!Target.IsValid() || Target.Get()==Culprit) && Seen) Acquire(Culprit,Position,true); return; }
 if(Seen && (Direct || Ally || Event.Type==ENeonCityThreatType::Gunshot || Event.Type==ENeonCityThreatType::NearbyDanger)) Acquire(Culprit,Position,true);
 else if(Event.Type==ENeonCityThreatType::Explosion && Culprit) Acquire(Culprit,Event.Location,false); // A blast is evidence, not wall-piercing live sight.
 else if(Direct) Acquire(Culprit,Event.Location,false); // Injury identity is evidence, not wall-piercing live sight.
}
void UNeonCityCombatAIComponent::Sense()
{
 ++PerceptionQueries; Perception.HasVisualContact=false;
 FVector Position,Aim;
 if(Target.IsValid())
 {
  if(CanSee(Target.Get(),Position,Aim))
  { Perception.LastKnownTargetLocation=Position; Perception.LastKnownAimPoint=Aim; Perception.LastEvidenceTime=Perception.LastSeenTime=GetWorld()->GetTimeSeconds(); Perception.HasVisualContact=true; }
  return;
 }
 if(bActive || Data->Role!=ENeonCityCombatRole::Hostile || GetWorld()->GetTimeSeconds()<ReacquireAfter) return;
 TArray<FOverlapResult> Hits; FCollisionQueryParams Q(SCENE_QUERY_STAT(CombatAcquire),false,GetOwner());
 GetWorld()->OverlapMultiByObjectType(Hits,Character->GetActorLocation(),FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeSphere(Data->SightRange),Q);
 int32 Count=0;
 for(const auto& Hit:Hits)
 {
  if(++Count>64) break;
  auto* Candidate=Hit.GetActor();
  if(Data->ProactiveEnemies.Contains(UCombatTargetComponent::GetFaction(Candidate)) && CanSee(Candidate,Position,Aim)) { Acquire(Candidate,Position,true); break; }
 }
}
bool UNeonCityCombatAIComponent::StartMove(FVector Point)
{
 Character->UnCrouch();
 if(!NeonCityCover::SafeSegment(Character.Get(),Character->GetActorLocation(),Point)) { bMove=false; return false; }
 MoveGoal=Point; bMove=true; LastMovePosition=Character->GetActorLocation(); BlockedSince=GetWorld()->GetTimeSeconds(); return true;
}
void UNeonCityCombatAIComponent::SeekCover(bool Replace)
{
 const float Now=GetWorld()->GetTimeSeconds(); if(Now<NextCover) return;
 NextCover=Now+Data->CoverQueryInterval; ++CoverQueries;
 auto* Selected=NeonCityCover::FindAndReserve(Character.Get(),Perception.LastKnownAimPoint,Data->CoverRadius,Replace?Cover.Get():nullptr);
 if(!Selected) return;
 ReleaseCover(); Cover=Selected; bAtPeek=false; SetState(ENeonCityCombatAIState::MoveToCover);
 if(!StartMove(Cover->GetActorLocation())) { ReleaseCover(); SetState(ENeonCityCombatAIState::Reposition); }
}
void UNeonCityCombatAIComponent::UpdateLocomotion(float DeltaSeconds)
{
 if(!bActive || !Character.IsValid()) return;
 if(const auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return;
 Character->GetCharacterMovement()->MaxWalkSpeed=Data->MoveSpeed;
 if(bMove)
 {
  const FVector Delta=(MoveGoal-Character->GetActorLocation())*FVector(1,1,0);
  if(Delta.Size()<25)
  {
   bMove=false; Character->GetCharacterMovement()->StopMovementImmediately();
   // Arrival facing follows movement. Allow time to turn and reacquire from the peek.
   if(bAtPeek && Cover.IsValid()) SetState(ENeonCityCombatAIState::Engage);
  }
  else Character->AddMovementInput(Delta.GetSafeNormal());
 }
 else if(Perception.HasLocation)
 {
  const FRotator Yaw(0,(Perception.LastKnownTargetLocation-Character->GetActorLocation()).Rotation().Yaw,0);
  Character->SetActorRotation(FMath::RInterpConstantTo(Character->GetActorRotation(),Yaw,DeltaSeconds,180));
 }
}
void UNeonCityCombatAIComponent::OnShot(FTransform Muzzle)
{ if(Target.IsValid()) GetWorld()->GetSubsystem<UCombatContextSubsystem>()->RecordAggression(GetOwner(),Target.Get()); }
void UNeonCityCombatAIComponent::TryShoot()
{
 const float Now=GetWorld()->GetTimeSeconds();
 if(!Perception.HasVisualContact || !Target.IsValid() || bMove || Now<NextShot || Now-Perception.LastSeenTime>Data->PerceptionInterval+.15f) return;
 if(const auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return;
 if(!Combat->GetWeapon() || !Combat->GetWeapon()->IsEquipped() || Combat->GetWeapon()->GetClass()!=Data->WeaponClass)
 {
  if(!Combat->EquipWeapon(Data->WeaponClass,WeaponMount)) return;
  Combat->GetWeapon()->OnMuzzleFlash.AddUniqueDynamic(this,&UNeonCityCombatAIComponent::OnShot);
 }
 auto* Weapon=Combat->GetWeapon();
 if(Weapon->GetAmmo()==0) { if(Combat->ReloadWeapon()) ++Reloads; else if(!Weapon->IsReloading() && Weapon->GetReserveAmmo()==0) Disengage(); return; }
 const auto* Profile=Weapon->GetData();
 if(!Profile || FVector::Dist(Character->GetActorLocation(),Perception.LastKnownTargetLocation)>FMath::Min(Data->EngagementRange,Profile->Range)) return;
 const FVector Eyes=Character->GetPawnViewLocation();
 if(!NeonCityCover::ClearSight(GetWorld(),GetOwner(),Eyes,Perception.LastKnownAimPoint,Target.Get())) { Perception.HasVisualContact=false; return; }
 const FVector Desired=(Perception.LastKnownAimPoint-WeaponMount->GetComponentLocation()).GetSafeNormal();
 // Skeletal hosts follow their aim pose. Generic fallback mounts retain direct aiming.
 if(WeaponMount->GetAttachSocketName().IsNone()) WeaponMount->SetWorldRotation(Desired.Rotation());
 const FVector Start=Weapon->GetMuzzleLocation();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(AIShotSafety),false,GetOwner()); Q.AddIgnoredActor(Weapon); FHitResult Hit;
 if(GetWorld()->LineTraceSingleByChannel(Hit,Start,Perception.LastKnownAimPoint,NeonCityCombat::WeaponTrace,Q) && Hit.GetActor()!=Target.Get())
 { SetState(ENeonCityCombatAIState::Reposition); SeekCover(true); return; }
 const float Cone=FMath::Max(.25f,Data->AimError*(1-Data->Accuracy));
 FCombatAttackRequest Request; Request.ViewOrigin=Start;
 Request.Direction=Random.VRandCone((Perception.LastKnownAimPoint-Start).GetSafeNormal(),FMath::DegreesToRadians(Cone)); Request.Instigator=Character->GetController(); Request.Aiming=true; Request.AvoidFriendlyFire=true; Request.Crouched=Character->bIsCrouched;
 Request.MovementRatio=Character->GetVelocity().Size2D()/FMath::Max(100.f,Data->MoveSpeed);
 // Check the actual spread ray as well: a friendly stepping into it prevents the shot.
 if(GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Request.Direction*Profile->Range,NeonCityCombat::WeaponTrace,Q) && UCombatTargetComponent::AreFriendly(GetOwner(),Hit.GetActor())) return;
 LastAimError=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Request.Direction,(Perception.LastKnownAimPoint-Start).GetSafeNormal()),-1.f,1.f)));
 if(Combat->Attack(Request))
 {
  ++Shots; if(++BurstShots>=Data->BurstLength) { BurstShots=0; NextShot=Now+Data->BurstPause; }
  else NextShot=Now+1.f/FMath::Clamp(Profile->FireRate,.2f,10.f);
 }
}
void UNeonCityCombatAIComponent::Retreat()
{
 ReleaseCover(); SetState(ENeonCityCombatAIState::Retreat); Combat->UnequipWeapon();
 const FVector Away=(Character->GetActorLocation()-Perception.LastKnownTargetLocation).GetSafeNormal2D();
 for(float Angle:{0.f,45.f,-45.f,90.f,-90.f})
  if(StartMove(Character->GetActorLocation()+Away.RotateAngleAxis(Angle,FVector::UpVector)*400)) return;
 bMove=false;
}
void UNeonCityCombatAIComponent::TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DeltaSeconds,TickType,Function);
 if(!CanFight() || !Character.IsValid() || !Health.IsValid()) return;
 const float Now=GetWorld()->GetTimeSeconds(); RefreshTick();
 if(Now>=NextSense) { NextSense=Now+Data->PerceptionInterval; Sense(); }
 if(!bActive) return;
 if((Target.IsValid() && !UCombatTargetComponent::IsAliveTarget(Target.Get())) || Now-StartedAt>Data->MaximumCombatSeconds
  || Now-Perception.LastEvidenceTime>Data->SearchSeconds || FVector::Dist2D(Character->GetActorLocation(),Home)>Data->ChaseLeash) { Disengage(); return; }
 if(bMove)
 {
  if(FVector::Dist2D(Character->GetActorLocation(),LastMovePosition)>10) { LastMovePosition=Character->GetActorLocation(); BlockedSince=Now; }
  else if(Now-BlockedSince>1.5f) { bMove=false; ReleaseCover(); SetState(ENeonCityCombatAIState::Reposition); }
 }
 if(Data->RetreatWhenInjured && Health->GetCurrentHealth()/Health->GetMaxHealth()<=Data->RetreatHealthFraction && State!=ENeonCityCombatAIState::Retreat) Retreat();
 if(State==ENeonCityCombatAIState::Retreat) { if(!bMove && Now-StateAt>1) Disengage(); return; }
 if(auto* Ecology=GetOwner()->FindComponentByClass<UNPCStateComponent>()) Ecology->SetState(ENeonCityNPCState::Fight);
 if(State==ENeonCityCombatAIState::AcquireTarget)
 { if(Now-StateAt<Data->ReactionTime) return; SeekCover(); if(State==ENeonCityCombatAIState::AcquireTarget) SetState(ENeonCityCombatAIState::Engage); }
 if(State==ENeonCityCombatAIState::MoveToCover)
 {
  if(!Cover.IsValid() || !Cover->ProvidesCover(Character.Get(),Perception.LastKnownAimPoint)) { ReleaseCover(); bMove=false; SetState(ENeonCityCombatAIState::Reposition); return; }
  if(bMove) return;
  if(Cover->Stance==ENeonCityCoverStance::Crouched) Character->Crouch();
  SetState(ENeonCityCombatAIState::Engage); return;
 }
 if(Cover.IsValid() && !bAtPeek)
 {
  if(Now-StateAt<.4f) return;
  FVector Peek; if(Cover->FindPeek(Character.Get(),Perception.LastKnownAimPoint,Peek) && StartMove(Peek)) { bAtPeek=true; SetState(ENeonCityCombatAIState::Reposition); }
  else { ReleaseCover(); SetState(ENeonCityCombatAIState::Reposition); }
  return;
 }
 if(bMove) return;
 if(Perception.HasVisualContact)
 {
  if(FVector::Dist2D(Character->GetActorLocation(),Perception.LastKnownTargetLocation)>Data->EngagementRange)
  {
   if(!Data->AllowChase) { Disengage(); return; }
   ReleaseCover(); SetState(ENeonCityCombatAIState::Chase); if(Now>=NextMovePlan) { NextMovePlan=Now+.75f; StartMove(Perception.LastKnownTargetLocation); } return;
  }
  SetState(ENeonCityCombatAIState::Engage); TryShoot(); return;
 }
 // Occluded target: only the captured point is used. No controller/player position shortcut.
 if(Cover.IsValid() && bAtPeek && Now-StateAt<FMath::Max(.6f,Data->ReactionTime)) return;
 SetState(ENeonCityCombatAIState::Reposition);
 if(Data->AllowChase) { ReleaseCover(); SetState(ENeonCityCombatAIState::Chase); if(Now>=NextMovePlan) { NextMovePlan=Now+.75f; StartMove(Perception.LastKnownTargetLocation); } }
 else if(Now-Perception.LastSeenTime>Data->ReactionTime) Disengage();
}
void UNeonCityCombatAIComponent::EndPlay(EEndPlayReason::Type Reason)
{
 ReleaseCover(); Target.Reset();
 if(Health.IsValid()) Health->OnHealthChanged.RemoveAll(this);
 if(auto* Threat=GetWorld()->GetSubsystem<UThreatSubsystem>()) Threat->OnThreat.RemoveAll(this);
 if(auto* Context=GetWorld()->GetSubsystem<UCombatContextSubsystem>()) Context->OnReset.RemoveAll(this);
 if(Combat.IsValid() && Combat->GetWeapon()) Combat->GetWeapon()->OnMuzzleFlash.RemoveAll(this);
 Super::EndPlay(Reason);
}
