#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
AWeaponBase::AWeaponBase(const FObjectInitializer& Initializer) : Super(Initializer)
{
	PrimaryActorTick.bCanEverTick=false;
	DefaultProfile=CreateDefaultSubobject<UWeaponData>(TEXT("DefaultWeaponProfile"));
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WeaponMount")));
	Muzzle=CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(GetRootComponent());
	SetActorEnableCollision(false);
}
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	Data=DataAsset.LoadSynchronous();
	if (!Data) { Data=DefaultProfile; }
	Ammo=FMath::Clamp(Data->MagazineSize,1,100); Reserve=FMath::Clamp(Data->StartingReserve,0,1000);
	Unequip();
}
bool AWeaponBase::Equip(USceneComponent* Mount,FName Socket)
{
	if (!HasAuthority() || !IsValid(Mount) || Mount->GetOwner()!=GetOwner()) { return false; }
	if (!AttachToComponent(Mount,FAttachmentTransformRules::SnapToTargetNotIncludingScale,Socket)) { return false; }
	bEquipped=true; SetActorHiddenInGame(false); return true;
}
void AWeaponBase::Unequip() { bEquipped=false; SetActorHiddenInGame(true); CancelReload(); }
FVector AWeaponBase::GetMuzzleLocation() const { return Muzzle->GetComponentLocation(); }
bool AWeaponBase::Fire(const FCombatAttackRequest& Request)
{
	if (!HasAuthority() || !bEquipped || bReloading || Ammo<=0 || !IsValid(GetOwner()) || !Data
		|| Request.ViewOrigin.ContainsNaN() || Request.Direction.ContainsNaN() || Request.Direction.IsNearlyZero()
		|| !FMath::IsFinite(Data->Range) || !FMath::IsFinite(Data->Damage) || Data->Damage<=0
		|| !FMath::IsFinite(Data->FireRate) || GetWorld()->GetTimeSeconds()+KINDA_SMALL_NUMBER<NextFireTime) { return false; }
	if (!Data->IsValidProfile()) return false;
	if (auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
	if (auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead()) { return false; }
	// Reserve the round/cooldown before invoking extensible feedback handlers.
	--Ammo; NextFireTime=GetWorld()->GetTimeSeconds()+1.f/FMath::Clamp(Data->FireRate,0.2f,10.f);
	LastHit=FHitResult(); LastAppliedDamage=0;
	if (!FireShot(Request)) { ++Ammo; return false; }
	GetWorld()->GetSubsystem<UThreatSubsystem>()->EmitGunshot(this,Request.Instigator,GetMuzzleLocation());
	OnMuzzleFlash.Broadcast(Muzzle->GetComponentTransform());
	if (auto* Effects=GetWorld()->GetSubsystem<UNeonCityWeaponEffectsSubsystem>()) { Effects->Report(Muzzle->GetComponentTransform(),GetReportKind()); }
	SustainedFire=FMath::Min(8.f,GetSustainedFire()+1); LastShotTime=GetWorld()->GetTimeSeconds(); ++ShotCount;
	const float Gain=1+SustainedFire*Data->SustainedFireMultiplier;
	LastYawRecoil=Data->HorizontalRecoil*(ShotCount%4<2?1.f:-1.f)*Gain;
	OnRecoil.Broadcast(FMath::Clamp(Data->Recoil*Gain,0.f,5.f));
	return true;
}
bool AWeaponBase::Reload()
{
	if (!HasAuthority() || !IsValid(GetOwner()) || !bEquipped || bReloading || !Data || Reserve<=0
		|| Ammo>=FMath::Clamp(Data->MagazineSize,1,100) || !FMath::IsFinite(Data->ReloadSeconds)) { return false; }
	if (auto* Health=GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead()) { return false; }
 if(auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) return false;
	bReloading=true;
	GetWorldTimerManager().SetTimer(ReloadTimer,this,&AWeaponBase::FinishReload,FMath::Clamp(Data->ReloadSeconds,0.1f,10.f),false);
	return true;
}
void AWeaponBase::FinishReload()
{
	if (!bReloading || !bEquipped || !Data) { return; }
	if(auto* Body=GetOwner()->FindComponentByClass<UDamageableCharacterComponent>(); Body && !Body->CanAct()) { CancelReload(); return; }
	const int32 Transfer=FMath::Min(Reserve,FMath::Max(0,FMath::Clamp(Data->MagazineSize,1,100)-Ammo));
	Ammo+=Transfer; Reserve-=Transfer; bReloading=false;
}
void AWeaponBase::CancelReload() { GetWorldTimerManager().ClearTimer(ReloadTimer); bReloading=false; }
void AWeaponBase::EndPlay(EEndPlayReason::Type Reason) { CancelReload(); Super::EndPlay(Reason); }
const UWeaponData* AWeaponBase::GetDefaultData() const
{ const auto* Asset=DataAsset.LoadSynchronous(); return Asset?Asset:DefaultProfile.Get(); }
float AWeaponBase::GetSustainedFire() const
{ return Data && GetWorld()?FMath::Max(0.f,SustainedFire-float(GetWorld()->GetTimeSeconds()-LastShotTime)*Data->RecoverySpeed):0; }
float AWeaponBase::GetSpreadDegrees(const FCombatAttackRequest& Request) const
{
 if(!Data) return 0;
 const float Move=FMath::IsFinite(Request.MovementRatio)?FMath::Clamp(Request.MovementRatio,0.f,3.f):3.f;
 float Cone=(Request.Aiming?Data->AimSpread:Data->HipFireSpread)+GetSustainedFire()*Data->Spread;
 Cone*=Move>1.1f?Data->SprintSpreadMultiplier:1+Move*(Data->MovementSpreadMultiplier-1);
 if(Move>1.1f) Cone=FMath::Max(Cone,Data->HipFireSpread*Data->SprintSpreadMultiplier);
 if(Request.Crouched) Cone*=Data->CrouchSpreadMultiplier;
 return FMath::Clamp(Cone,0.f,20.f);
}
