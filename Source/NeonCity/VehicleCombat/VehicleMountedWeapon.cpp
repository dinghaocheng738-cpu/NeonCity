#include "VehicleCombat/VehicleMountedWeapon.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "VehicleCombat/TargetingComponent.h"
#include "Combat/CombatCollision.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
AVehicleMountedWeapon::AVehicleMountedWeapon(const FObjectInitializer& Initializer)
 : Super(Initializer.SetDefaultSubobjectClass<UVehicleWeaponData>(TEXT("DefaultWeaponProfile")))
{
 auto* Barrel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MountedWeaponVisual")); Barrel->SetupAttachment(GetRootComponent());
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Barrel->SetStaticMesh(Cube.Object); Barrel->SetRelativeLocation(FVector(-30,0,0)); Barrel->SetRelativeScale3D(FVector(.6,.12,.12)); Barrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if(NeonCityVisuals::Apply(Barrel,TEXT("VehicleWeapons"),TEXT("MountedGun"))) Barrel->SetRelativeLocation(FVector::ZeroVector);
}
namespace
{
void MountedAppearance(AActor& Owner,const TCHAR* Style)
{
 TInlineComponentArray<UStaticMeshComponent*> Parts(&Owner);
 for(auto* Part:Parts) if(Part->GetFName()==TEXT("MountedWeaponVisual"))
  if(NeonCityVisuals::Apply(Part,TEXT("VehicleWeapons"),Style)) Part->SetRelativeLocation(FVector::ZeroVector);
}
}
AVehicleCannon::AVehicleCannon() { CastChecked<UVehicleWeaponData>(DefaultProfile)->ConfigureVehicle(EVehicleWeaponType::Cannon);MountedAppearance(*this,TEXT("Cannon")); }
namespace NeonCityOrdnance
{
// Tools/Art/Phase205cUpgrade/WeaponFX/ordnance.py: the hardpoint sits 14 cm above the pylon's bottom; the rack and the
// pod hang from it. Rack missile stations (UE: y mirrored), nozzle origins; the Hellfire is 163 cm long.
const FVector RackOffset(0,0,-14);
const FVector Stations[AVehicleMissileLauncher::Rails]={FVector(-62,-20,-19),FVector(-62,20,-19),FVector(-62,-20,-53),FVector(-62,20,-53)};
constexpr float MissileLength=163.f;
const FVector PodOffset(0,0,-40);
UStaticMesh* Ordnance(const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/%s.%s"),Name,Name));}
}
using namespace NeonCityOrdnance;
void AVehicleMountedWeapon::AddStubWing()
{
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grey(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/MI_NC_RackGrey.MI_NC_RackGrey"));
 StubWing=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StubWing"));StubWing->SetupAttachment(GetRootComponent());
 if(Cube.Succeeded())StubWing->SetStaticMesh(Cube.Object);
 if(Grey.Succeeded())StubWing->SetMaterial(0,Grey.Object);
 StubWing->SetCollisionEnabled(ECollisionEnabled::NoCollision);StubWing->SetCanEverAffectNavigation(false);StubWing->SetVisibility(false);
}
void AVehicleMountedWeapon::BeginPlay()
{
 Super::BeginPlay();
 // the stub wing runs from the store's top inward to the fuselage side (the carrier's centreline is ~1.1 m in)
 const AActor* Carrier=GetAttachParentActor()?GetAttachParentActor():GetOwner();
 if(StubWing && Carrier && Carrier!=this)
 {
  const float Side=GetActorTransform().InverseTransformPosition(Carrier->GetActorLocation()).Y>0?1.f:-1.f;
  StubWing->SetRelativeLocation(FVector(-10,Side*55,4));StubWing->SetRelativeScale3D(FVector(.62f,1.15f,.07f));
  StubWing->SetRelativeRotation(FRotator(0,0,Side*-6));StubWing->SetVisibility(true);                    // slight anhedral
 }
}
AVehicleRocketPod::AVehicleRocketPod()
{
 CastChecked<UVehicleWeaponData>(DefaultProfile)->ConfigureVehicle(EVehicleWeaponType::Rocket);
 TInlineComponentArray<UStaticMeshComponent*> Parts(this);
 for(auto* Part:Parts)if(Part->GetFName()==TEXT("MountedWeaponVisual"))
 {
  static ConstructorHelpers::FObjectFinder<UStaticMesh> Pod(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/SM_NC_RocketPod.SM_NC_RocketPod"));
  if(Pod.Succeeded()){Part->SetStaticMesh(Pod.Object);Part->SetRelativeLocation(PodOffset);Part->SetRelativeScale3D(FVector(1));}
 }
 Muzzle->SetRelativeLocation(PodOffset+FVector(85,0,0));                          // the tube bundle's mouths
 AddStubWing();
}
AVehicleMissileLauncher::AVehicleMissileLauncher()
{
 CastChecked<UVehicleWeaponData>(DefaultProfile)->ConfigureVehicle(EVehicleWeaponType::Missile);
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Rack(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/SM_NC_HellfireRack.SM_NC_HellfireRack"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Missile(TEXT("/Game/NeonCity/Art/WeaponFX/Ordnance/SM_NC_Hellfire.SM_NC_Hellfire"));
 TInlineComponentArray<UStaticMeshComponent*> Parts(this);
 for(auto* Part:Parts)if(Part->GetFName()==TEXT("MountedWeaponVisual") && Rack.Succeeded())
 {Part->SetStaticMesh(Rack.Object);Part->SetRelativeLocation(RackOffset);Part->SetRelativeScale3D(FVector(1));}
 for(int32 I=0;I<Rails;++I)
 {
  auto* M=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("RailMissile%d"),I));M->SetupAttachment(GetRootComponent());
  if(Missile.Succeeded())M->SetStaticMesh(Missile.Object);
  M->SetRelativeLocation(RackOffset+Stations[I]);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCanEverAffectNavigation(false);
  RailMissiles.Add(M);
 }
 AddStubWing();
}
int32 AVehicleMissileLauncher::GetMissilesOnRails() const { return FMath::Clamp(GetAmmo(),0,Rails); }
void AVehicleMissileLauncher::PresentRails()
{
 // rounds leave from the top rails first; the muzzle is the nose of the next missile to go
 const int32 Loaded=GetMissilesOnRails();
 for(int32 I=0;I<RailMissiles.Num();++I)RailMissiles[I]->SetVisibility(I>=Rails-Loaded);
 const int32 Next=FMath::Clamp(Rails-Loaded,0,Rails-1);
 Muzzle->SetRelativeLocation(RackOffset+Stations[Next]+FVector(MissileLength+10,0,0));
}
void AVehicleMissileLauncher::Tick(float Delta) { Super::Tick(Delta); PresentRails(); }
bool AVehicleMissileLauncher::FireShot(const FCombatAttackRequest& Request) { PresentRails(); const bool bFired=Super::FireShot(Request); return bFired; }
ENeonCityWeaponReport AVehicleMountedWeapon::GetReportKind() const
{
 const auto* D=GetVehicleData();if(!D)return ENeonCityWeaponReport::MachineGun;
 return D->WeaponType==EVehicleWeaponType::Cannon?ENeonCityWeaponReport::Cannon:D->ProjectileType==EVehicleProjectileType::Bullet?ENeonCityWeaponReport::MachineGun:ENeonCityWeaponReport::Rocket;
}
const UVehicleWeaponData* AVehicleMountedWeapon::GetVehicleData() const { return Cast<UVehicleWeaponData>(Data?Data.Get():GetDefaultData()); }
float AVehicleMountedWeapon::GetHeat() const
{ const auto* D=GetVehicleData(); return D && GetWorld()?FMath::Max(0.f,Heat-float(GetWorld()->GetTimeSeconds()-HeatAt)*D->HeatRecovery):0; }
bool AVehicleMountedWeapon::FireShot(const FCombatAttackRequest& Request)
{
 const auto* D=GetVehicleData(); if(!D || GetHeat()>=1) return false;
 AActor* Target=LockedTarget.Get();
 // A locked missile needs line of sight to its target; with no lock it leaves unguided (dumb fire).
 if(D->ProjectileType==EVehicleProjectileType::Missile && Target && !UTargetingComponent::HasLineOfSight(GetWorld(),GetOwner(),Target,GetMuzzleLocation()))
 return false;
 const FVector Direction=FMath::VRandCone(Request.Direction.GetSafeNormal(),FMath::DegreesToRadians(D->Spread));
 // Reject a muzzle embedded in nearby cover; the barrel cannot spawn a shot through a wall.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(MountedMuzzle),false,GetOwner()); Q.AddIgnoredActor(this); FHitResult Block;
 if(GetWorld()->SweepSingleByChannel(Block,GetMuzzleLocation()-Direction*10,GetMuzzleLocation()+Direction*8,FQuat::Identity,NeonCityCombat::WeaponTrace,FCollisionShape::MakeSphere(3),Q))
 return false;
 TSubclassOf<ABulletProjectile> Type=ABulletProjectile::StaticClass();
 if(D->ProjectileType==EVehicleProjectileType::Rocket) Type=ARocketProjectile::StaticClass();
 if(D->ProjectileType==EVehicleProjectileType::Missile) Type=AMissileProjectile::StaticClass();
 auto* Pool=GetWorld()->GetSubsystem<UVehicleProjectilePool>();
 auto* P=Pool->Acquire(Type,FTransform(Direction.Rotation(),GetMuzzleLocation()),GetOwner()); if(!P) return false;
 FNeonCityDamageEvent Event; Event.DamageAmount=D->Damage; Event.SourceActor=this; Event.Instigator=Request.Instigator;
 if(!P->LaunchVehicle(Direction,GetOwner()->GetVelocity(),*D,Event,Target)) { Pool->Release(P); return false; }
 LastProjectile=P; Heat=GetHeat()+D->HeatPerShot; HeatAt=GetWorld()->GetTimeSeconds(); return true;
}
