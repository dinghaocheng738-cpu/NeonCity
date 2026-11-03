#include "Explosion/ExplosionPresentationSubsystem.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Combat/WeaponEffectsSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
const TCHAR* FireballPath=TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion");
const TCHAR* SmokePath=TEXT("/Game/StarterContent/Particles/P_Smoke.P_Smoke");
const TCHAR* FirePath=TEXT("/Game/StarterContent/Particles/P_Fire.P_Fire");
const TCHAR* BoomPath=TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue");
const TCHAR* ScorchPath=TEXT("/Game/NeonCity/Art/Effects/M_NC_ExplosionScorch.M_NC_ExplosionScorch");
const TCHAR* ChunkMaterialPath=TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
constexpr float SpeedOfSound=34300.f;
TAutoConsoleVariable<int32> CVarDebris(TEXT("NeonCity.Explosion.Debris"),1,TEXT("Ground blasts throw simulated debris fragments (0 disables)."));

/** Looping emitters (smoke, fire) stop spawning after their authored duration and remove themselves once their
 * last particles have faded. */
void StopAfter(UWorld* World,UParticleSystemComponent* Emitter,float Seconds)
{
 if(!World || !Emitter) return;
 FTimerHandle Handle; TWeakObjectPtr<UParticleSystemComponent> Weak(Emitter);
 World->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Weak]{ if(Weak.IsValid()) Weak->Deactivate(); }),FMath::Max(.1f,Seconds),false);
}
}

void UNeonCityExplosionShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{ OutInfo.Duration=FCameraShakeDuration::Custom(Duration); OutInfo.BlendIn=0; OutInfo.BlendOut=.15f; }
void UNeonCityExplosionShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{ Elapsed=0; Seed=FVector(FMath::FRand(),FMath::FRand(),FMath::FRand())*100; }
void UNeonCityExplosionShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,FCameraShakePatternUpdateResult& OutResult)
{
 Elapsed+=Params.DeltaTime;
 // Strongest on the first frames (the pressure wave), then an exponential ring-down.
 const float Envelope=FMath::Exp(-5.f*Elapsed/Duration)*Params.GetTotalScale();
 const float T=Elapsed*Frequency;
 auto Noise=[&](float Offset){ return FMath::PerlinNoise1D(T+Offset); };
 OutResult.Rotation=FRotator(Noise(Seed.X),Noise(Seed.Y),Noise(Seed.Z)*.5f)*RotationAmplitude*Envelope*2;
 OutResult.Location=FVector(Noise(Seed.Y+7),Noise(Seed.Z+13),Noise(Seed.X+29))*LocationAmplitude*Envelope*2;
}

UNeonCityExplosionCameraShake::UNeonCityExplosionCameraShake(const FObjectInitializer& Initializer):Super(Initializer)
{ SetRootShakePattern(CreateDefaultSubobject<UNeonCityExplosionShakePattern>(TEXT("Blast"))); bSingleInstance=false; }

ANeonCityExplosionFlash::ANeonCityExplosionFlash()
{
 PrimaryActorTick.bCanEverTick=true;
 Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash")); SetRootComponent(Light);
 Light->SetIntensityUnits(ELightUnits::Lumens); Light->SetLightColor(FLinearColor(1,.62f,.3f)); Light->SetCastShadows(false);
 Light->SetSourceRadius(150); Light->SetMobility(EComponentMobility::Movable);
}
void ANeonCityExplosionFlash::Configure(float Scale)
{
 Peak=900000.f*Scale*Scale; Light->SetAttenuationRadius(2200.f*Scale); Light->SetIntensity(Peak); SetLifeSpan(Lifetime+.1f);
}
void ANeonCityExplosionFlash::Tick(float Delta)
{
 Super::Tick(Delta); Age+=Delta;
 const float A=FMath::Clamp(Age/Lifetime,0.f,1.f);
 Light->SetIntensity(Peak*FMath::Square(1-A)); Light->SetLightColor(FLinearColor(1,FMath::Lerp(.62f,.32f,A),FMath::Lerp(.3f,.08f,A)));
}

ANeonCityExplosionDebris::ANeonCityExplosionDebris()
{
 PrimaryActorTick.bCanEverTick=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Origin")));
}
void ANeonCityExplosionDebris::Throw(const FVector& SurfaceNormal,float Scale,int32 Count,UMaterialInterface* Material)
{
 UStaticMesh* Rock=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(!Rock) return;
 const FVector Up=SurfaceNormal.IsNearlyZero()?FVector::UpVector:SurfaceNormal.GetSafeNormal();
 Count=FMath::Clamp(Count,1,MaximumChunks);
 for(int32 I=0;I<Count;++I)
 {
  // Irregular slabs and clods, 6-28 cm, thrown out of a cone around the surface normal.
  const FVector Size=FVector(FMath::FRandRange(.08f,.28f),FMath::FRandRange(.06f,.22f),FMath::FRandRange(.05f,.14f))*FMath::Clamp(Scale,.7f,1.6f);
  // A fragment born inside a wall or the ground would be shoved out by depenetration.
  const FVector At=GetActorLocation()+FMath::VRand().GetSafeNormal2D()*FMath::FRandRange(0.f,40.f)*Scale+Up*(20+Size.Z*60);
  const FQuat Spin=FMath::VRand().ToOrientationQuat();
  FCollisionQueryParams Clear(SCENE_QUERY_STAT(ExplosionDebrisSpawn),false,this);
  if(GetWorld()->OverlapAnyTestByObjectType(At,Spin,FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects),FCollisionShape::MakeBox(Size*50+FVector(2)),Clear)) continue;
  auto* Chunk=NewObject<UStaticMeshComponent>(this); Chunk->SetupAttachment(GetRootComponent());
  Chunk->SetStaticMesh(Rock); if(Material) Chunk->SetMaterial(0,Material);
  Chunk->SetWorldScale3D(Size);
  Chunk->SetWorldLocationAndRotation(At,Spin);
  Chunk->SetCollisionProfileName(TEXT("PhysicsActor"));
  // Fragments bounce and settle on the static world only and are invisible to every query. Chaos wheel suspension
  // traces WorldDynamic: a clod rising under a car reads as the road jumping up and the spring launches the car.
  // Vehicles, loose bodies, characters, weapon and sight traces all pass through them.
  Chunk->SetCollisionResponseToAllChannels(ECR_Ignore); Chunk->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
  Chunk->SetCastShadow(true); Chunk->SetGenerateOverlapEvents(false);
  Chunk->RegisterComponent();
  // Clods of soil and broken surfacing (~1.3 t/m3, at most 4 kg): they rattle off a car without shoving it.
  Chunk->SetSimulatePhysics(true); Chunk->SetMassOverrideInKg(NAME_None,FMath::Clamp(1300.f*Size.X*Size.Y*Size.Z,.5f,4.f),true);
  Chunk->SetLinearDamping(.05f); Chunk->SetAngularDamping(.3f);
  const FVector Direction=FMath::VRandCone(Up,FMath::DegreesToRadians(55.f));
  Chunk->SetPhysicsLinearVelocity(Direction*FMath::FRandRange(650.f,1500.f)*FMath::Clamp(Scale,.7f,1.5f));
  Chunk->SetPhysicsAngularVelocityInDegrees(FMath::VRand()*FMath::FRandRange(180.f,900.f));
  Chunks.Add(Chunk);
 }
 SetLifeSpan(FMath::FRandRange(9.f,12.f));
}

bool UNeonCityExplosionPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
 const UWorld* World=Cast<UWorld>(Outer);
 return World && World->IsGameWorld() && World->GetNetMode()!=NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}
void UNeonCityExplosionPresentationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
 Super::OnWorldBeginPlay(InWorld);
 if(auto* Explosions=InWorld.GetSubsystem<UNeonCityExplosionSubsystem>())
  Explosions->OnExplosion.AddUniqueDynamic(this,&UNeonCityExplosionPresentationSubsystem::Present);
}
void UNeonCityExplosionPresentationSubsystem::Deinitialize()
{
 if(UWorld* World=GetWorld()) if(auto* Explosions=World->GetSubsystem<UNeonCityExplosionSubsystem>()) Explosions->OnExplosion.RemoveAll(this);
 Debris.Reset(); Super::Deinitialize();
}
int32 UNeonCityExplosionPresentationSubsystem::GetActiveDebrisCount() const
{ int32 N=0; for(const auto& D:Debris) if(D.IsValid() && !D->IsActorBeingDestroyed()) ++N; return N; }
void UNeonCityExplosionPresentationSubsystem::LoadAssets()
{
 if(bLoaded) return; bLoaded=true;
 Fireball=LoadObject<UParticleSystem>(nullptr,FireballPath); Smoke=LoadObject<UParticleSystem>(nullptr,SmokePath);
 Fire=LoadObject<UParticleSystem>(nullptr,FirePath); Boom=LoadObject<USoundBase>(nullptr,BoomPath);
 Scorch=LoadObject<UMaterialInterface>(nullptr,ScorchPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
 Attenuation=NewObject<USoundAttenuation>(this);
 Attenuation->Attenuation.bAttenuate=true; Attenuation->Attenuation.bSpatialize=true;
 Attenuation->Attenuation.AttenuationShape=EAttenuationShape::Sphere; Attenuation->Attenuation.AttenuationShapeExtents=FVector(1500,0,0);
 Attenuation->Attenuation.FalloffDistance=45000; Attenuation->Attenuation.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,ChunkMaterialPath))
 {
  auto* Soot=UMaterialInstanceDynamic::Create(Base,this); Soot->SetVectorParameterValue(TEXT("Color"),FLinearColor(.035f,.03f,.026f)); Charred=Soot;
 }
}
void UNeonCityExplosionPresentationSubsystem::Present(const FNeonCityExplosionEvent& Event)
{
 UWorld* World=GetWorld(); if(!World || !Event.IsValidEvent()) return;
 LoadAssets(); ++Presented;
 const float Scale=FMath::Clamp(Event.OuterRadius/400.f,.6f,2.5f);
 const FVector Center=Event.Center;
 // Surface under the blast: a ground burst throws debris, scorches and keeps burning; an air burst does not.
 // Only the static world counts as ground: a missile that strikes a car's flank still scorches the road under it.
 FHitResult Ground; FCollisionQueryParams Query(SCENE_QUERY_STAT(ExplosionPresentation),false);
 Query.AddIgnoredActor(Event.SourceActor.Get());
 const bool bGround=World->LineTraceSingleByObjectType(Ground,Center+FVector(0,0,60),Center-FVector(0,0,320),FCollisionObjectQueryParams(ECC_WorldStatic),Query);
 const FVector Base=bGround?Ground.ImpactPoint:Center;
 const FVector Normal=bGround?FVector(Ground.ImpactNormal):FVector::UpVector;
 // The Niagara blast (fireball, rising smoke column, sparks) when its assets are there; Starter Content otherwise.
 auto* Effects=World->GetSubsystem<UNeonCityWeaponEffectsSubsystem>();
 const bool bNiagara=Effects && Effects->Detonation(Center,Scale*1.3f);
 if(Fireball && !bNiagara)
 {
  // One core fireball and a few rolling secondary bursts a moment later, spread through the lethal core and rising.
  UGameplayStatics::SpawnEmitterAtLocation(World,Fireball,Center,FRotator(0,FMath::FRandRange(0.f,360.f),0),FVector(Scale*2.2f),true);
  const int32 Bursts=FMath::Clamp(FMath::RoundToInt(Scale*1.8f),1,4);
  for(int32 I=0;I<Bursts;++I)
  {
   const FVector Offset=FVector(FMath::VRand().GetSafeNormal2D()*FMath::FRandRange(.25f,.7f)*Event.InnerRadius)+Normal*FMath::FRandRange(40.f,90.f+Event.InnerRadius*.6f);
   TWeakObjectPtr<UWorld> WeakWorld(World); UParticleSystem* Template=Fireball; const FVector At=Center+Offset; const float Size=Scale*FMath::FRandRange(1.2f,1.7f);
   FTimerHandle Handle;
   World->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([WeakWorld,Template,At,Size]
   { if(WeakWorld.IsValid()) UGameplayStatics::SpawnEmitterAtLocation(WeakWorld.Get(),Template,At,FRotator(0,FMath::FRandRange(0.f,360.f),0),FVector(Size),true); }),FMath::FRandRange(.04f,.16f),false);
  }
 }
 if(Smoke && Event.SmokeDuration>0 && !bNiagara)
 {
  // A thick column: a wide base cloud and a second, higher plume feeding it.
  StopAfter(World,UGameplayStatics::SpawnEmitterAtLocation(World,Smoke,Base+Normal*40,FRotator::ZeroRotator,FVector(Scale*2.6f),true),FMath::Min(Event.SmokeDuration,20.f));
  StopAfter(World,UGameplayStatics::SpawnEmitterAtLocation(World,Smoke,Base+Normal*(120+Event.InnerRadius),FRotator::ZeroRotator,FVector(Scale*2.f),true),FMath::Min(Event.SmokeDuration*.6f,12.f));
 }
 if(Fire && bGround && Event.FireDuration>0)
  StopAfter(World,UGameplayStatics::SpawnEmitterAtLocation(World,Fire,Base,FRotator::ZeroRotator,FVector(Scale*.55f),true),FMath::Min(Event.FireDuration,10.f));
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 if(auto* Flash=World->SpawnActor<ANeonCityExplosionFlash>(Center+Normal*120,FRotator::ZeroRotator,Params)) Flash->Configure(Scale);
 if(bGround)
 {
  if(Scorch)
  {
   const float Size=Event.InnerRadius*1.4f+80;
   FRotator Facing=(-Normal).Rotation(); Facing.Roll=FMath::FRandRange(0.f,360.f);
   if(UDecalComponent* Mark=UGameplayStatics::SpawnDecalAtLocation(World,Scorch,FVector(120,Size,Size),Base,Facing,0)) Mark->SetFadeOut(45,15,true);
  }
  Debris.RemoveAll([](const auto& D){ return !D.IsValid() || D->IsActorBeingDestroyed(); });
  while(Debris.Num()>=MaximumDebrisActors) { if(Debris[0].IsValid()) Debris[0]->Destroy(); Debris.RemoveAt(0); }
  if(CVarDebris.GetValueOnGameThread()>0)
  if(auto* Thrown=World->SpawnActor<ANeonCityExplosionDebris>(Base,FRotator::ZeroRotator,Params))
  { Thrown->Throw(Normal,Scale,FMath::RoundToInt(8+4*Scale),Charred); Debris.Add(Thrown); }
 }
 // Each local player: the shake falls off with distance, and the sound arrives after the flash.
 for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
 {
  APlayerController* PC=It->Get(); if(!PC || !PC->IsLocalController()) continue;
  FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
  const float Distance=FVector::Distance(Eye,Center);
  const float Shake=FMath::Square(FMath::Clamp(1.f-Distance/(Event.OuterRadius*9.f),0.f,1.f))*FMath::Clamp(Scale,.6f,1.6f);
  if(Shake>.02f) PC->ClientStartCameraShake(UNeonCityExplosionCameraShake::StaticClass(),Shake);
  if(Boom)
  {
   const float Delay=FMath::Min(Distance/SpeedOfSound,3.f);
   TWeakObjectPtr<UWorld> WeakWorld(World); USoundBase* Sound=Boom; USoundAttenuation* Falloff=Attenuation;
   auto Play=[WeakWorld,Sound,Falloff,Center,Scale]{ if(WeakWorld.IsValid()) UGameplayStatics::PlaySoundAtLocation(WeakWorld.Get(),Sound,Center,FMath::Clamp(Scale,.8f,2.f),FMath::FRandRange(.82f,1.02f),0,Falloff); };
   if(Delay<.05f) Play();
   else { FTimerHandle Handle; World->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda(Play),Delay,false); }
  }
  break;    // one listener: the sound and shake are per machine, not per split-screen view
 }
}
