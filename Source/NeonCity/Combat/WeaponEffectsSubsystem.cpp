#include "Combat/WeaponEffectsSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Combat/CombatSurface.h"
#include "Combat/DamageInterface.h"
#include "Combat/Weapons/WeaponData.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace NeonCityWeaponEffects
{
const TCHAR* FX=TEXT("/Game/NeonCity/Art/WeaponFX/");
// Per 0.1 s window: a minigun burst into a wall still reads as a stream of strikes, never a wall of emitters.
constexpr int32 MaxEmitters=10,MaxVoices=6,MaxDecals=6;
constexpr float DecalLife=30.f;
template<class T> T* Find(const FString& Path){return LoadObject<T>(nullptr,*Path);}
}
using namespace NeonCityWeaponEffects;

ANeonCityMuzzleFlash::ANeonCityMuzzleFlash()
{
 PrimaryActorTick.bCanEverTick=true;SetCanBeDamaged(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
 Core=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashCore"));SetRootComponent(Core);
 Core->SetStaticMesh(Sphere.Object);
 Petals=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashPlume"));Petals->SetupAttachment(Core);
 Petals->SetStaticMesh(Cone.Object);Petals->SetRelativeRotation(FRotator(-90,0,0));   // cone apex along the bore
 for(auto* Part:{Core.Get(),Petals.Get()})
 {Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);Part->SetGenerateOverlapEvents(false);Part->SetCanEverAffectNavigation(false);}
 Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));Light->SetupAttachment(Core);
 Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetLightColor(FLinearColor(1,.7f,.38f));Light->SetCastShadows(false);Light->SetSourceRadius(15);
}

void ANeonCityMuzzleFlash::Configure(float Scale,UMaterialInterface* Material)
{
 Size=Scale;Peak=9000*Scale*Scale;Light->SetIntensity(Peak);Light->SetAttenuationRadius(600*Scale);
 if(Material){Core->SetMaterial(0,Material);Petals->SetMaterial(0,Material);}
 // Random roll and length per shot: no two flashes identical.
 const float Length=FMath::FRandRange(.8f,1.25f);
 Core->SetRelativeScale3D(FVector(.22f*Length,.16f,.16f)*Scale);
 Petals->SetRelativeLocation(FVector(26*Scale*Length,0,0));Petals->SetRelativeScale3D(FVector(.24f,.24f,.5f*Length)*Scale);
 AddActorLocalRotation(FRotator(0,0,FMath::FRandRange(0.f,360.f)));
}

void ANeonCityMuzzleFlash::Tick(float Delta)
{
 Super::Tick(Delta);Age+=Delta;
 const float A=FMath::Clamp(1-Age/Lifetime,0.f,1.f);
 Light->SetIntensity(Peak*A*A);Core->SetRelativeScale3D(Core->GetRelativeScale3D()*(1-.25f*Delta/Lifetime));
 if(Age>=Lifetime)Destroy();
}

void UNeonCityWeaponEffectsSubsystem::Load()
{
 if(!bFXLoaded)
 {
  bFXLoaded=true;
  const TCHAR* Dir=TEXT("/Game/NeonCity/Art/WeaponFX/FX/");
  auto Get=[Dir](const TCHAR* Name){return LoadObject<UNiagaraSystem>(nullptr,*FString::Printf(TEXT("%s%s.%s"),Dir,Name,Name));};
  PuffFX=Get(TEXT("NS_NC_TrailPuff"));LaunchFX=Get(TEXT("NS_NC_LaunchSmoke"));FireballFX=Get(TEXT("NS_NC_Fireball"));
  BlastSmokeFX=Get(TEXT("NS_NC_BlastSmoke"));SparksFX=Get(TEXT("NS_NC_Sparks"));
 }
 if(bLoaded)return;bLoaded=true;
 Sparks=Find<UParticleSystem>(TEXT("/Game/StarterContent/Particles/P_Sparks.P_Sparks"));
 Smoke=Find<UParticleSystem>(TEXT("/Game/StarterContent/Particles/P_Smoke.P_Smoke"));
 Steam=Find<UParticleSystem>(TEXT("/Game/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit"));
 Blast=Find<UParticleSystem>(TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion"));
 Hole=Find<UMaterialInterface>(FString(FX)+TEXT("M_NC_BulletHole.M_NC_BulletHole"));
 Flash=Find<UMaterialInterface>(FString(FX)+TEXT("MI_NC_MuzzleFlash.MI_NC_MuzzleFlash"));
 auto Wave=[](const TCHAR* Name){return Find<USoundBase>(FString::Printf(TEXT("%sAudio/%s.%s"),FX,Name,Name));};
 MGShot=Wave(TEXT("SW_NC_MGShot"));CannonShot=Wave(TEXT("SW_NC_CannonShot"));RocketLaunch=Wave(TEXT("SW_NC_RocketLaunch"));
 for(const TCHAR* N:{TEXT("SW_NC_ImpactHard"),TEXT("SW_NC_ImpactHard2")})if(auto* S=Wave(N))HardHits.Add(S);
 SoftHit=Wave(TEXT("SW_NC_ImpactSoft"));WaterHit=Wave(TEXT("SW_NC_ImpactWater"));
 auto Falloff=[this](float Inner,float Distance)
 {
  auto* A=NewObject<USoundAttenuation>(this);A->Attenuation.bAttenuate=true;A->Attenuation.bSpatialize=true;
  A->Attenuation.AttenuationShape=EAttenuationShape::Sphere;A->Attenuation.AttenuationShapeExtents=FVector(Inner,0,0);
  A->Attenuation.FalloffDistance=Distance;A->Attenuation.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;return A;
 };
 ShotFalloff=Falloff(800,30000);HitFalloff=Falloff(300,7000);
}

bool UNeonCityWeaponEffectsSubsystem::Budget(int32& Used,int32 Max)
{
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now-WindowStart>.1){WindowStart=Now;EmittersUsed=VoicesUsed=DecalsUsed=0;}
 if(Used>=Max)return false;
 ++Used;return true;
}

void UNeonCityWeaponEffectsSubsystem::Burst(UParticleSystem* System,const FVector& Location,const FRotator& Rotation,float Scale,float Seconds)
{
 // StarterContent emitters loop: run one for a moment, then let its particles live out and fade.
 if(!System || !Budget(EmittersUsed,MaxEmitters))return;
 UParticleSystemComponent* PS=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),System,Location,Rotation,FVector(Scale),true);
 if(!PS)return;
 TWeakObjectPtr<UParticleSystemComponent> Weak(PS);FTimerHandle Handle;
 GetWorld()->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Weak]{if(Weak.IsValid())Weak->Deactivate();}),FMath::Max(.02f,Seconds),false);
}

void UNeonCityWeaponEffectsSubsystem::Sound(USoundBase* S,const FVector& Location,float Volume,float Pitch,USoundAttenuation* Falloff)
{
 if(S && Budget(VoicesUsed,MaxVoices))UGameplayStatics::PlaySoundAtLocation(GetWorld(),S,Location,Volume,Pitch*FMath::FRandRange(.94f,1.06f),0,Falloff);
}

void UNeonCityWeaponEffectsSubsystem::Report(const FTransform& Muzzle,ENeonCityWeaponReport Kind)
{
 UWorld* World=GetWorld();if(!World || World->GetNetMode()==NM_DedicatedServer)return;
 Load();++ReportCount;
 const FVector Location=Muzzle.GetLocation();const FRotator Rotation=Muzzle.Rotator();
 const float Scale=Kind==ENeonCityWeaponReport::Cannon?1.7f:Kind==ENeonCityWeaponReport::MachineGun?1.2f:Kind==ENeonCityWeaponReport::Rocket?2.2f:.6f;
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 if(auto* F=World->SpawnActor<ANeonCityMuzzleFlash>(Location,Rotation,P))F->Configure(Scale,Flash);
 switch(Kind)
 {
  case ENeonCityWeaponReport::Rocket:
   Sound(RocketLaunch,Location,1.f,1.f,ShotFalloff);
   Burst(Smoke,Location-Muzzle.GetRotation().GetForwardVector()*80,Rotation,.35f,.25f);   // back-blast
   break;
  case ENeonCityWeaponReport::Cannon:
   Sound(CannonShot,Location,1.f,1.f,ShotFalloff);Burst(Smoke,Location,Rotation,.12f,.06f);break;
  case ENeonCityWeaponReport::MachineGun:
   Sound(MGShot,Location,.9f,1.f,ShotFalloff);break;
  default:
   Sound(MGShot,Location,.55f,1.35f,ShotFalloff);break;
 }
}

void UNeonCityWeaponEffectsSubsystem::Impact(const FHitResult& Hit,const FVector& Velocity,float Calibre,const AController* Instigator)
{
 UWorld* World=GetWorld();if(!World)return;
 Load();++ImpactCount;
 const FVector Point=Hit.ImpactPoint;const FVector Normal=Hit.ImpactNormal.GetSafeNormal();
 const FVector Dir=Velocity.GetSafeNormal();
 AActor* Actor=Hit.GetActor();UPrimitiveComponent* Component=Hit.GetComponent();
 // The round's momentum: loose props jump, parked cars rock, a long burst shoves them.
 if(Component && Component->IsSimulatingPhysics(Hit.BoneName) && !Dir.IsNearlyZero())
  Component->AddImpulseAtLocation(Dir*RoundImpulse(Calibre)*(Cast<ACharacter>(Actor)?.15f:1.f),Point,Hit.BoneName);   // ragdoll bones are light
 if(Actor && Instigator && Actor->Implements<UDamageInterface>())ConfirmedHits.Add(Instigator,World->GetTimeSeconds());
 if(World->GetNetMode()==NM_DedicatedServer)return;
 ENeonCityImpactSurface Surface=NeonCityCombat::ResolveImpactSurface(Hit);
 const bool bEarth=Component && Component->GetClass()->GetName().Contains(TEXT("Landscape"));
 const FRotator Out=Normal.Rotation();
 const FRotator Ricochet=(Dir-2*FVector::DotProduct(Dir,Normal)*Normal).Rotation();
 const float C=FMath::Clamp(Calibre,.2f,4.f);
 switch(Surface)
 {
  case ENeonCityImpactSurface::Metal:
   Burst(Sparks,Point+Normal*3,Ricochet,.25f+.15f*C,.07f);
   if(!HardHits.IsEmpty())Sound(HardHits[0],Point,.8f,1.1f,HitFalloff);
   break;
  case ENeonCityImpactSurface::Glass:
   Burst(Sparks,Point+Normal*3,Out,.15f*C,.04f);
   if(HardHits.Num()>1)Sound(HardHits[1],Point,.7f,1.4f,HitFalloff);
   break;
  case ENeonCityImpactSurface::Character:
   Sound(SoftHit,Point,.7f,1.2f,HitFalloff);
   break;
  default:
   // Earth / stone / wood: a kicked-up puff along the surface normal, stone also throws a few sparks.
   Burst(Smoke,Point+Normal*8,Out,.05f+.05f*C,.08f);
   if(!bEarth && Surface!=ENeonCityImpactSurface::Wood)Burst(Sparks,Point+Normal*3,Ricochet,.12f*C,.03f);
   if(bEarth || Surface==ENeonCityImpactSurface::Wood)Sound(SoftHit,Point,.8f,1.f,HitFalloff);
   else if(HardHits.Num()>1)Sound(HardHits[1],Point,.75f,1.f,HitFalloff);
   break;
 }
 if(C>=2.5f)Burst(Blast,Point+Normal*15,Out,.12f*C,.05f);                                  // cannon shells burst on contact
 // Hole: on static and slow surfaces, never on people; attached so it rides a car body or a door.
 if(Hole && Component && Surface!=ENeonCityImpactSurface::Character && !Cast<USkeletalMeshComponent>(Component) && Budget(DecalsUsed,MaxDecals))
 {
  const float S=FMath::Clamp(5.f*C,3.f,20.f);
  FRotator R=(-Normal).Rotation();R.Roll=FMath::FRandRange(0.f,360.f);
  if(auto* D=UGameplayStatics::SpawnDecalAttached(Hole,FVector(S*.6f,S,S),Component,Hit.BoneName,Point,R,EAttachLocation::KeepWorldPosition,DecalLife))
  {D->SetFadeOut(DecalLife-4,4,false);D->SetFadeScreenSize(.0008f);}
 }
}

void UNeonCityWeaponEffectsSubsystem::WaterEntry(const FVector& Point,float Calibre)
{
 UWorld* World=GetWorld();if(!World || World->GetNetMode()==NM_DedicatedServer)return;
 Load();++ImpactCount;
 const float C=FMath::Clamp(Calibre,.2f,8.f);
 // A thrown-up plume of spray and its mist; heavier rounds throw a taller column.
 Burst(Steam,Point+FVector(0,0,10),FRotator::ZeroRotator,.25f*C,.08f+.03f*C);              // white spray plume
 Burst(Sparks,Point+FVector(0,0,5),FRotator(90,0,0),.12f*C,.03f);                            // flung droplets catching the light
 Sound(WaterHit,Point,FMath::Clamp(.5f+.2f*C,.5f,1.f),FMath::Clamp(1.3f-.1f*C,.7f,1.3f),HitFalloff);
}

double UNeonCityWeaponEffectsSubsystem::GetLastConfirmedHit(const AController* Instigator) const
{
 const double* T=ConfirmedHits.Find(Instigator);return T?*T:-100.0;
}

void UNeonCityWeaponEffectsSubsystem::Niagara(UNiagaraSystem* System,const FVector& Location,const FRotator& Rotation,float Scale)
{
 if(!System || Location.ContainsNaN())return;
 UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,Location,Rotation,FVector(Scale),true,true,ENCPoolMethod::AutoRelease);
}

void UNeonCityWeaponEffectsSubsystem::TrailPuff(const FVector& Point,float Density)
{
 Load();
 if(!PuffFX)return;
 // the puffs are cheap lightweight bursts; each lives ~4 s, so a 2 s burn leaves ~30 of them along the line
 Niagara(PuffFX,Point,FRotator::ZeroRotator,FMath::Lerp(.6f,1.f,FMath::Clamp(Density,0.f,1.f)));
}

void UNeonCityWeaponEffectsSubsystem::LaunchSmoke(const FVector& Point,const FVector& Direction)
{
 Load();
 Niagara(LaunchFX,Point-Direction.GetSafeNormal()*120,Direction.Rotation(),1.f);
}

bool UNeonCityWeaponEffectsSubsystem::Detonation(const FVector& Point,float Scale)
{
 Load();
 if(!FireballFX || !BlastSmokeFX)return false;
 const float S=FMath::Clamp(Scale,.3f,4.f);
 Niagara(FireballFX,Point,FRotator::ZeroRotator,S);
 Niagara(BlastSmokeFX,Point+FVector(0,0,60*S),FRotator::ZeroRotator,S);
 Niagara(SparksFX,Point,FRotator::ZeroRotator,FMath::Sqrt(S));
 return true;
}
