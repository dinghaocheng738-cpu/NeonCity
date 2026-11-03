#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/WeaponHardpoint.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "VehicleCombat/TargetingComponent.h"
#include "VehicleCombat/LockOnSystem.h"
#include "VehicleCombat/VehicleLaserMark.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "VehicleCombat/VehicleAimPresentation.h"
#include "Vehicles/Aircraft/AircraftExtensionInterfaces.h"
#include "Vehicles/Marine/MarineVehicleInterface.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Combat/CombatCollision.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
UVehicleCombatComponent::UVehicleCombatComponent() { PrimaryComponentTick.bCanEverTick=true; }
void UVehicleCombatComponent::BeginPlay()
{
 Super::BeginPlay();
 Manager=NewObject<UVehicleWeaponManager>(GetOwner()); Manager->RegisterComponent();
 Targeting=NewObject<UTargetingComponent>(GetOwner()); Targeting->RegisterComponent();
 Lock=NewObject<ULockOnSystem>(this); AimDirection=GetOwner()->GetActorForwardVector();
 // The local gunner's sight, laser designator and lock tones (presentation only).
 if(!GetOwner()->FindComponentByClass<UVehicleAimPresentationComponent>()) NewObject<UVehicleAimPresentationComponent>(GetOwner())->RegisterComponent();
}
UVehicleCombatComponent* UVehicleCombatComponent::Install(APawn* Host,EVehicleCombatMountPreset Preset)
{
 if(!IsValid(Host) || !Host->HasAuthority() || !Host->HasActorBegunPlay()) return nullptr;
 if(auto* Existing=Host->FindComponentByClass<UVehicleCombatComponent>()) return Existing;
 auto* C=NewObject<UVehicleCombatComponent>(Host); C->RegisterComponent(); C->bAddMouseLook=Preset==EVehicleCombatMountPreset::Roof;
 auto Add=[&](FName ID,FVector Position,EVehicleWeaponType Type,FName Weapon,bool Fixed=false)
 {
  FTransform Local(Position);
  if(Host->Implements<UWeaponHardpointInterface>())
  { FTransform World; if(IWeaponHardpointInterface::Execute_GetHardpointTransform(Host,ID,World)) Local=World.GetRelativeTransform(Host->GetActorTransform()); }
  if(C->Manager->AddHardpoint(ID,Local,{Type},Fixed)) C->Manager->Equip(ID,Weapon);
 };
 if(Preset==EVehicleCombatMountPreset::FixedWing)
 {
  Add(TEXT("Nose"),FVector(325,0,0),EVehicleWeaponType::Cannon,TEXT("NeonCity.AircraftCannon"),true);
  Add(TEXT("LeftWing"),FVector(20,-270,-18),EVehicleWeaponType::Missile,TEXT("NeonCity.GuidedMissile"),true);
  Add(TEXT("RightWing"),FVector(20,270,-18),EVehicleWeaponType::Missile,TEXT("NeonCity.GuidedMissile"),true);
 }
 else if(Preset==EVehicleCombatMountPreset::Helicopter)
 {
  Add(TEXT("Nose"),FVector(240,0,-25),EVehicleWeaponType::MachineGun,TEXT("NeonCity.MountedMachineGun"));
  // stub-wing height: the stores clear the skids and a pad, so a landed helicopter can still launch
  Add(TEXT("SideMount.Left"),FVector(40,-160,12),EVehicleWeaponType::Rocket,TEXT("NeonCity.RocketPod"),true);
  Add(TEXT("SideMount.Right"),FVector(40,160,12),EVehicleWeaponType::Missile,TEXT("NeonCity.GuidedMissile"),true);
 }
 else Add(Preset==EVehicleCombatMountPreset::Boat?FName(TEXT("SideMount")):FName(TEXT("RoofMount")),Preset==EVehicleCombatMountPreset::Boat?FVector(170,0,110):FVector(70,0,140),EVehicleWeaponType::MachineGun,TEXT("NeonCity.MountedMachineGun"));
 return C;
}
FVector UVehicleCombatComponent::ConvergeAim(const FVector& ViewOrigin,const FVector& ViewDirection) const
{
 // The chase camera sits metres behind and above the mounts: a shot parallel to the view would land short of
 // (or over) what the crosshair is on. Aim the mount at the point under the crosshair instead.
 const auto* W=Manager?Manager->GetWeapon():nullptr;
 if(!W || ViewDirection.IsNearlyZero())return ViewDirection;
 const FVector Muzzle=W->GetMuzzleLocation();const FVector View=ViewDirection.GetSafeNormal();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(VehicleAimConverge),false,GetOwner());
 TArray<AActor*> Attached;GetOwner()->GetAttachedActors(Attached,true,true);Q.AddIgnoredActors(Attached);
 constexpr float Reach=40000;
 FVector Point=ViewOrigin+View*Reach;FHitResult Hit;
 if(GetWorld()->LineTraceSingleByChannel(Hit,ViewOrigin,Point,NeonCityCombat::WeaponTrace,Q))Point=Hit.ImpactPoint;
 // the lake / sea surface has no collision: converge on it where the view ray meets it
 if(auto* Water=GetWorld()->GetSubsystem<UMarineWorldSubsystem>())
  for(float D=500;D<FVector::Dist(ViewOrigin,Point);D+=500)
  {
   float Surface=0;FVector Current;const FVector P=ViewOrigin+View*D;
   if(Water->SampleWater(P,Surface,Current) && P.Z<Surface)
   {const FVector Prev=ViewOrigin+View*(D-500);Point=FMath::Lerp(Prev,P,FMath::Clamp((Prev.Z-Surface)/FMath::Max(1.,Prev.Z-P.Z),0.,1.));break;}
  }
 const FVector ToPoint=Point-Muzzle;
 // never turn the shot back past the muzzle (crosshair on the airframe itself, or point-blank)
 return FVector::DotProduct(ToPoint.GetSafeNormal(),View)>.5f && ToPoint.Size()>300?ToPoint.GetSafeNormal():View;
}
bool UVehicleCombatComponent::ArmKeyIsB() const
{ return GetOwner()->Implements<UMarineVehicleInterface>() || GetOwner()->IsA<ANeonCityBaseVehicle>(); }   // V is their camera key
bool UVehicleCombatComponent::CanOperate() const
{
 const auto* Pawn=Cast<APawn>(GetOwner()); const auto* H=GetOwner()->FindComponentByClass<UHealthComponent>();
 return bArmed && Pawn && Pawn->GetController() && H && !H->IsDead() && Manager && Manager->GetWeapon();
}
void UVehicleCombatComponent::AimWeapon(FVector Direction)
{ if(!Direction.ContainsNaN() && !Direction.IsNearlyZero()) AimDirection=Direction.GetSafeNormal(); }
void UVehicleCombatComponent::LockTarget(bool Enabled) { bLockRequested=Enabled; if(!Enabled && Lock) Lock->Reset(); }
void UVehicleCombatComponent::UpdateLock(float Delta)
{
 if(!Lock || !CanOperate() || !bLockRequested) { if(Lock) Lock->Reset(); return; }
 auto* Point=Manager->GetPoint(); FVector Origin=Point->GetComponentLocation(); FVector Direction=Point->ClampAim(AimDirection);
 // The seeker and the laser look where the gunner looks (the sight's crosshair), not along the rail's own limited
 // angle; the missile leaves its rail and turns onto what the sight holds.
 if(const auto* PC=Cast<APlayerController>(CastChecked<APawn>(GetOwner())->GetController()))
 {
  FRotator View;PC->GetPlayerViewPoint(Origin,View);Direction=View.Vector();
 }
 AActor* Candidate=Lock->GetCandidate();
 FVehicleTargetInfo Info;
 if(Candidate && Candidate!=LaserMark)Info=Targeting->Inspect(Candidate,Origin,Direction,Lock->MaxRange,Lock->AngleLimit);
 else
 {
  // a vehicle or person in the seeker's field wins; otherwise the laser designates whatever it falls on
  Info=Targeting->FindTarget(Origin,Direction,Lock->MaxRange,Lock->AngleLimit);
  if(!Info.Visible)Info=Designate(Origin,Direction);
 }
 Lock->Update(Info,Delta);
}
bool UVehicleCombatComponent::IsDesignatingPoint() const { return Lock && LaserMark && Lock->GetCandidate()==LaserMark; }
FVehicleTargetInfo UVehicleCombatComponent::Designate(const FVector& Origin,const FVector& Direction)
{
 // The laser spot: the first surface down the aim line within the seeker's range. While the lock builds the spot
 // may wander a little with the hand; a big jump restarts it. Once locked it stays put unless re-aimed well away.
 FVehicleTargetInfo Info;FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(LaserDesignator),false,GetOwner());
 for(const auto& Mount:Manager->GetMounts())if(Mount.Combat && Mount.Combat->GetWeapon())Q.AddIgnoredActor(Mount.Combat->GetWeapon());
 const float Range=FMath::Clamp(Lock->MaxRange,100.f,50000.f);
 // the laser passes our own rounds in flight (a departing missile is right in front of the sight)
 for(int32 Pass=0;;++Pass)
 {
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Direction*Range,NeonCityCombat::WeaponTrace,Q))return Info;
  if(Pass>=4 || !Hit.GetActor() || !Hit.GetActor()->IsA<ABulletProjectile>())break;
  Q.AddIgnoredActor(Hit.GetActor());
 }
 // a metre off the surface toward the shooter, so the sight line from the rail (below the sight) stays clear
 const FVector Spot=Hit.ImpactPoint+Hit.ImpactNormal*100-Direction*60;
 if(!LaserMark || !IsValid(LaserMark))
 {
  FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;P.Owner=GetOwner();
  LaserMark=GetWorld()->SpawnActor<AVehicleLaserMark>(Spot,FRotator::ZeroRotator,P);
  if(!LaserMark)return Info;
 }
 const bool bHeld=Lock->GetCandidate()==LaserMark;
 const float Moved=FVector::Dist(LaserMark->GetActorLocation(),Spot);
 // locked: the spot holds while the crosshair stays on it (something passing in front does not move it)
 const bool bStillOnSpot=FVector::DotProduct(Direction,(LaserMark->GetActorLocation()-Origin).GetSafeNormal())>FMath::Cos(FMath::DegreesToRadians(3.f));
 if(bHeld && Lock->State==EVehicleLockState::Locked && (Moved<600 || bStillOnSpot))
 {}
 else
 {
  if(bHeld && Moved>300)Lock->Reset();                                // re-aimed: designate afresh
  LaserMark->SetActorLocation(Spot);
 }
 Info.Actor=LaserMark;Info.Distance=FVector::Dist(Origin,LaserMark->GetActorLocation());
 Info.Direction=(LaserMark->GetActorLocation()-Origin).GetSafeNormal();Info.Visible=true;Info.TargetID=TEXT("LaserSpot");
 return Info;
}
bool UVehicleCombatComponent::IsTargetVisible(AActor* Target,const FVector& Origin,const FVector& Direction)
{
 if(Target && Target==LaserMark)return UTargetingComponent::HasLineOfSight(GetWorld(),GetOwner(),Target,Origin);
 return Targeting->Inspect(Target,Origin,Direction,Lock->MaxRange,Lock->AngleLimit).Visible;
}
bool UVehicleCombatComponent::FireWeapon()
{
 if(!CanOperate()) return false;
 auto* W=Manager->GetWeapon(); auto* P=Manager->GetPoint(); const auto* D=W->GetVehicleData();
 const FVector Direction=P->ClampAim(AimDirection);
 AActor* Target=Lock->GetLockedTarget();
 // Holding lock (RMB) means "guided": the missile waits for a visible lock. Without it the missile is
 // dumb-fired straight down the aim line and detonates on whatever it meets.
 if(D->ProjectileType==EVehicleProjectileType::Missile && bLockRequested)
 {
  if(!Target || !IsTargetVisible(Target,P->GetComponentLocation(),Direction))
  {
   if(Target) Lock->Update(FVehicleTargetInfo(),0); return false;
  }
 }
 else if(D->ProjectileType==EVehicleProjectileType::Missile) Target=nullptr;
 // Lock-on-before-launch: a locked missile comes off the rail already turned onto its target (up to the side of
 // the aircraft), instead of flying out along the rail and having to swing round at full speed.
 FVector LaunchDirection=Direction;
 if(D->ProjectileType==EVehicleProjectileType::Missile && Target)
 {
  const FVector ToTarget=(Target->GetActorLocation()-W->GetMuzzleLocation()).GetSafeNormal();
  if(FVector::DotProduct(ToTarget,GetOwner()->GetActorForwardVector())>-.2f)LaunchDirection=ToTarget;
 }
 W->SetLockedTarget(Target); FCombatAttackRequest R; R.ViewOrigin=P->GetComponentLocation(); R.Direction=LaunchDirection;
 R.Instigator=CastChecked<APawn>(GetOwner())->GetController(); R.Aiming=true;
 const bool bFired=Manager->GetCombat()->Attack(R);
 return bFired;
}
bool UVehicleCombatComponent::ReloadWeapon() { return CanOperate() && Manager->GetCombat()->ReloadWeapon(); }
void UVehicleCombatComponent::ResetTransientState() { bArmed=bTrigger=bLockRequested=false; if(Lock) Lock->Reset(); }
void UVehicleCombatComponent::PressFire() { bTrigger=true; }
void UVehicleCombatComponent::ReleaseFire() { bTrigger=false; }
void UVehicleCombatComponent::PressLock() { LockTarget(true); }
void UVehicleCombatComponent::ReleaseLock() { LockTarget(false); }
void UVehicleCombatComponent::ReloadInput() { ReloadWeapon(); }
void UVehicleCombatComponent::CycleInput() { Manager->NextWeapon(); Lock->Reset(); }
void UVehicleCombatComponent::ToggleInput() { bArmed=!bArmed; }
void UVehicleCombatComponent::LookYaw(const FInputActionValue& V) { if(InputController.IsValid()) InputController->AddYawInput(V.Get<float>()); }
void UVehicleCombatComponent::LookPitch(const FInputActionValue& V) { if(InputController.IsValid()) InputController->AddPitchInput(V.Get<float>()); }
void UVehicleCombatComponent::BindInput(APlayerController* PC)
{
 RemoveInput(); if(!PC || !PC->GetLocalPlayer()) return;
 InputController=PC; if(bAddMouseLook) PC->SetControlRotation(GetOwner()->GetActorRotation()); InputSubsystem=PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 Mapping=NewObject<UInputMappingContext>(this); Input=NewObject<UEnhancedInputComponent>(GetOwner()); Input->Priority=5; Input->RegisterComponent(); Actions.Reset();
 auto Action=[&](FKey Key,EInputActionValueType Type=EInputActionValueType::Boolean)
 { auto* A=NewObject<UInputAction>(this); A->ValueType=Type; A->bConsumeInput=false; Mapping->MapKey(A,Key); Actions.Add(A); return A; };
 auto* Fire=Action(EKeys::LeftMouseButton); Input->BindAction(Fire,ETriggerEvent::Started,this,&UVehicleCombatComponent::PressFire); Input->BindAction(Fire,ETriggerEvent::Completed,this,&UVehicleCombatComponent::ReleaseFire); Input->BindAction(Fire,ETriggerEvent::Canceled,this,&UVehicleCombatComponent::ReleaseFire);
 auto* Aim=Action(EKeys::RightMouseButton); Input->BindAction(Aim,ETriggerEvent::Started,this,&UVehicleCombatComponent::PressLock); Input->BindAction(Aim,ETriggerEvent::Completed,this,&UVehicleCombatComponent::ReleaseLock); Input->BindAction(Aim,ETriggerEvent::Canceled,this,&UVehicleCombatComponent::ReleaseLock);
 Input->BindAction(Action(EKeys::R),ETriggerEvent::Started,this,&UVehicleCombatComponent::ReloadInput);
 Input->BindAction(Action(EKeys::Tab),ETriggerEvent::Started,this,&UVehicleCombatComponent::CycleInput);
 // Marine V is the shared first/third-person camera control. Armed boats keep
 // a separate safety switch; aiming follows the actual marine camera below.
 Input->BindAction(Action(ArmKeyIsB()?EKeys::B:EKeys::V),ETriggerEvent::Started,this,&UVehicleCombatComponent::ToggleInput);
 if(bAddMouseLook)
 {
  Input->BindAction(Action(EKeys::MouseX,EInputActionValueType::Axis1D),ETriggerEvent::Triggered,this,&UVehicleCombatComponent::LookYaw);
  Input->BindAction(Action(EKeys::MouseY,EInputActionValueType::Axis1D),ETriggerEvent::Triggered,this,&UVehicleCombatComponent::LookPitch);
 }
 InputSubsystem->AddMappingContext(Mapping,2); PC->PushInputComponent(Input);
}
void UVehicleCombatComponent::RemoveInput()
{
 if(InputController.IsValid() && Input) InputController->PopInputComponent(Input);
 if(InputSubsystem.IsValid() && Mapping) InputSubsystem->RemoveMappingContext(Mapping);
 if(Input) Input->DestroyComponent(); Input=nullptr; Mapping=nullptr; Actions.Reset(); InputController.Reset(); InputSubsystem.Reset(); ResetTransientState();
}
FString UVehicleCombatComponent::GetDebugStatus() const
{
 auto* W=Manager?Manager->GetWeapon():nullptr;
 const TCHAR* ArmKey=ArmKeyIsB()?TEXT("B"):TEXT("V");
 return W?FString::Printf(TEXT("%s | %d + %d | %s | Lock %s %.0f%% | Heat %.0f%%\n%s arm / LMB fire / hold RMB lock (missile: no lock = straight shot) / R reload / Tab mount"),*W->GetData()->DisplayName.ToString(),W->GetAmmo(),W->GetReserveAmmo(),bArmed?TEXT("ARMED"):TEXT("SAFE"),*StaticEnum<EVehicleLockState>()->GetNameStringByValue(int64(Lock->State)),Lock->GetProgress()*100,W->GetHeat()*100,ArmKey):TEXT("No mounted weapon");
}
void UVehicleCombatComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function); auto* Pawn=Cast<APawn>(GetOwner()); if(!Pawn) return;
 auto* PC=Cast<APlayerController>(Pawn->GetController()); if(PC && !PC->IsLocalController()) PC=nullptr;
 if(InputController.Get()!=PC) { if(PC) BindInput(PC); else RemoveInput(); }
 if(!PC) return; // AI callers use the same explicit interfaces; no AI scanning loop is added.
 FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin,Rotation);
 AimWeapon(ConvergeAim(Origin,bAddMouseLook?PC->GetControlRotation().Vector():Rotation.Vector()));
 ScanElapsed+=Delta; if(ScanElapsed>=.1f) { UpdateLock(ScanElapsed); ScanElapsed=0; }
 if(bTrigger) FireWeapon();
 DebugElapsed+=Delta; if(DebugElapsed>=.2f && GEngine) { if(!bStatusPresented)GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()),.25f,bArmed?FColor::Yellow:FColor::White,GetDebugStatus()); DebugElapsed=0; }
}
void UVehicleCombatComponent::EndPlay(EEndPlayReason::Type Reason)
{ RemoveInput(); if(Manager) Manager->DestroyComponent(); if(Targeting) Targeting->DestroyComponent(); Super::EndPlay(Reason); }
