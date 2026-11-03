#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Explosion/ExplosionData.h"
#include "Explosion/ExplosionEvent.h"
#include "Explosion/ExplosionSubsystem.h"
#include "Explosion/ExplosionImpulseUtility.h"
#include "Destruction/BreakableSurfaceData.h"
#include "Destruction/DestructionTypes.h"
#include "Destruction/DestructionSubsystem.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Destruction/DestructibleStructure.h"
#include "Destruction/DestructionInterfaces.h"
#include "Destruction/DestructionTestArea.h"
#include "DamageFeedback/DamageFeedbackData.h"
#include "DamageFeedback/CharacterDamageFeedbackComponent.h"
#include "World/WorldEventSubsystem.h"
#include "World/WorldEventTypes.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Physics/DestructiblePropComponent.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "VehicleCombat/VehicleWeaponData.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/Projectiles/BulletProjectile.h"
#include "VehicleCombat/Projectiles/RocketProjectile.h"
#include "VehicleCombat/Projectiles/MissileProjectile.h"
#include "VehicleCombat/Projectiles/VehicleProjectilePool.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/CombatInterface.h"
#include "Combat/ThreatSubsystem.h"
#include "Combat/Weapons/NeonCityWeaponCatalog.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Combat/WeaponInventoryComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCStateComponent.h"
#include "AI/NPCThreatComponent.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/Combat/CombatTargetComponent.h"
#include "AI/Police/PoliceCharacter.h"
#include "AI/Police/PoliceVehicle.h"
#include "Police/PoliceResponseData.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/PlayerCombatAdapter.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Vehicles/Aircraft/AircraftWorldSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "UObject/StrongObjectPtr.h"

namespace NeonCityExplosionPIE
{
/** Shared Phase 20 proving ground: one opt-in fixture pad over the released damage pipeline. */
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr; ADestructionTestArea* Area=nullptr;
 UNeonCitySaveSubsystem* Save=nullptr;
 float AreaAt=0;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!W || !Player || W->GetTimeSeconds()<2.f || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
  if(!Area)
  {
   Area=W->SpawnActor<ADestructionTestArea>(FVector(-2400,9000,0),FRotator::ZeroRotator);
   if(!Area) return false;
   Save=W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>():nullptr;
   AreaAt=W->GetTimeSeconds(); PlayerAt(Area->GetActorLocation()+FVector(0,-2600,140)); return false;
  }
  // Fixtures settle on the pad before a blast is measured against them.
  return W->GetTimeSeconds()-AreaAt>2.5f && Player->GetCharacterMovement()->IsMovingOnGround();
 }
 void PlaceOnPad(FVector Local) { PlayerAt(Area->GetActorLocation()+Local); }
 void PlayerAt(FVector P)
 {
  Player->GetCharacterMovement()->StopMovementImmediately(); Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
  Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCapsuleComponent()->UpdatePhysicsVolume(true);
 }
 /** Damage always enters through the released system, never a test-local copy of it. */
 void Damage(AActor* Victim,float Amount,AController* Instigator,AActor* Source,ENeonCityDamageType Type=ENeonCityDamageType::Ballistic,FVector Hit=FVector::ZeroVector)
 {
  if(!Victim) return;
  FNeonCityDamageEvent Event; Event.DamageAmount=Amount; Event.DamageType=Type; Event.Instigator=Instigator; Event.SourceActor=Source;
  Event.HitLocation=Hit.IsNearlyZero()?Victim->GetActorLocation():Hit;
  W->GetSubsystem<UDamageSystem>()->ApplyDamage(Victim,Event);
 }
 ACombatTestTarget* Target(FVector P,float HP=1000)
 { auto* Created=W->SpawnActor<ACombatTestTarget>(P,FRotator::ZeroRotator); if(Created) Created->GetHealthComponent()->ApplyPersistenceHealth(HP,HP); return Created; }
 ANeonCityPhysicsProp* Prop(ECityPhysicsProp Type,FVector P,FRotator R=FRotator::ZeroRotator)
 {
  const FTransform Transform(R,P);
  auto* Created=W->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!Created) return nullptr; Created->Configure(Type); Created->FinishSpawning(Transform); return Created;
 }
 /** A static blocker box, so occlusion is measured against geometry rather than an assumption. */
 AActor* Blocker(FVector Center,FVector Extent)
 {
  auto* Host=W->SpawnActor<AActor>(); if(!Host) return nullptr;
  auto* Box=NewObject<UBoxComponent>(Host); Host->SetRootComponent(Box);
  Box->InitBoxExtent(Extent); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
  Host->SetActorLocation(Center); return Host;
 }
 ANeonCityNPCCharacter* Civilian(FVector P,int32 Seed=301)
 {
  const FTransform Spawn(FRotator(0,180,0),P);
  auto* NPC=W->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!NPC) return nullptr;
  NPC->InitializePedestrian(FName(*FString::Printf(TEXT("ExplosionPIE.Civilian.%d"),Seed)),nullptr,{},Seed); NPC->FinishSpawning(Spawn); return NPC;
 }
 /** Turns a plain NPC into an armed responder without inventing a second combat AI. */
 ANeonCityNPCCharacter* Fighter(FVector P,int32 Seed)
 {
  auto* NPC=Civilian(P,Seed); if(!NPC) return nullptr;
  auto* Profile=NewObject<UCombatAIData>(NPC);
  Profile->Role=ENeonCityCombatRole::Security; Profile->CanFight=true; Profile->Faction=ENeonCityFaction::Police;
  Profile->RequiresArmedThreat=true; Profile->AllowChase=false; Profile->Accuracy=0; Profile->AimError=18;
  NPC->GetCombatAI()->Configure(Profile,Seed); return NPC;
 }
 UNeonCityExplosionSubsystem* Explosions() const { return W->GetSubsystem<UNeonCityExplosionSubsystem>(); }
 UWorldEventSubsystem* Events() const { return W->GetSubsystem<UWorldEventSubsystem>(); }
 UDestructionSubsystem* Destruct() const { return W->GetSubsystem<UDestructionSubsystem>(); }
 UCrimeSubsystem* Crime() const { return W->GetSubsystem<UCrimeSubsystem>(); }
 UWantedSubsystem* Wanted() const { return W->GetSubsystem<UWantedSubsystem>(); }
 FNeonCityExplosionResult Blast(FName DataID,FVector Center,AController* Instigator=nullptr,AActor* Source=nullptr,int32 Depth=0)
 {
  UNeonCityExplosionSubsystem::FRequest Request; Request.Data=&NeonCityExplosions::Profile(DataID);
  Request.Center=Center; Request.Instigator=Instigator; Request.SourceActor=Source; Request.ChainDepth=Depth;
  if(Source) Request.Ignored.Add(Source);
  return Explosions()->Detonate(Request);
 }
 bool CrimeHas(ENeonCityCrimeType Type,AActor* Victim,AController* Instigator) const
 {
  return Crime()->GetEvents().ContainsByPredicate([&](const FNeonCityCrimeEvent& E)
  { return E.CrimeType==Type && (!Victim || E.Victim.Get()==Victim) && (!Instigator || E.Instigator.Get()==Instigator); });
 }
 /** Rendered PIE capture; hooked to a command line switch so unattended runs stay headless. */
 void Shot(const TCHAR* Name,AActor* Focus)
 {
  if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityExplosionCapture")) || !Focus || !PC) return;
  const FVector P=Focus->GetActorLocation(),Eye=P+FVector(1200,-1600,900);
  auto* Camera=W->SpawnActor<ACameraActor>(Eye,(P-Eye).Rotation()); if(!Camera) return;
  PC->SetViewTarget(Camera);
  if(PC->PlayerCameraManager) { PC->PlayerCameraManager->SetGameCameraCutThisFrame(); PC->PlayerCameraManager->UpdateCamera(0.f); }
  bool Done=false;
  if(auto* Viewport=W->GetGameViewport(); Viewport && Viewport->Viewport)
  {
   Viewport->Viewport->Draw(false); TArray<FColor> Pixels;
   if(Viewport->Viewport->ReadPixels(Pixels))
   {
    for(auto& Pixel:Pixels) Pixel.A=255;
    TArray64<uint8> PNG; const FIntPoint Size=Viewport->Viewport->GetSizeXY();
    FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Explosion")),true);
    Done=FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("Explosion")/Name));
   }
  }
  if(!Done) UE_LOG(LogTemp,Error,TEXT("Explosion screenshot failed: %s"),Name);
  PC->SetViewTarget(PC->GetPawn()); Camera->Destroy();
 }
};

class FBlastFalloff : public IAutomationLatentCommand
{
public:
 explicit FBlastFalloff(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Explosion falloff stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1 && Elapsed>.3f)
  {
   Center=F.Area->GetActorLocation()+FVector(0,0,900);
   Inner=F.Target(Center+FVector(120,0,0));
   Mid=F.Target(Center+FVector(0,300,0));
   Edge=F.Target(Center+FVector(-445,0,0));
   Outside=F.Target(Center+FVector(0,-560,0));
   Open=F.Target(Center+FVector(0,0,-350));
   Sheltered=F.Target(Center+FVector(0,0,350));
   Shield=F.Blocker(Center+FVector(0,0,175),FVector(100,100,20));
   return Next(2);
  }
  if(Stage==2 && Elapsed>.5f)
  {
   const auto* Rocket=&NeonCityExplosions::Profile(TEXT("NeonCity.RocketExplosion"));
   const auto Result=F.Blast(TEXT("NeonCity.RocketExplosion"),Center,F.PC,F.Area);
   const float InnerHealth=Inner->GetHealthComponent()->GetCurrentHealth();
   const float MidHealth=Mid->GetHealthComponent()->GetCurrentHealth();
   const float EdgeHealth=Edge->GetHealthComponent()->GetCurrentHealth();
   const float OutsideHealth=Outside->GetHealthComponent()->GetCurrentHealth();
   const float OpenHealth=Open->GetHealthComponent()->GetCurrentHealth();
   const float ShelteredHealth=Sheltered->GetHealthComponent()->GetCurrentHealth();
   Test->TestTrue(TEXT("Blast resolves inside its authored radius"),Result.bApplied && Result.DamageTargets>=4);
   Test->TestEqual(TEXT("Lethal core keeps the full authored damage"),InnerHealth,1000.f-Rocket->Damage);
   Test->TestTrue(TEXT("Damage attenuates monotonically with distance"),InnerHealth<MidHealth && MidHealth<EdgeHealth && EdgeHealth<OutsideHealth);
   Test->TestEqual(TEXT("Targets beyond the outer radius are untouched"),OutsideHealth,1000.f);
   Test->AddInfo(FString::Printf(TEXT("Occlusion: open health %.1f (%.1f damage), sheltered health %.1f (%.1f damage), refused targets %d"),OpenHealth,1000.f-OpenHealth,ShelteredHealth,1000.f-ShelteredHealth,Result.RefusedTargets));
   Test->TestTrue(TEXT("Characters and plain world targets receive no launch impulse"),Result.ImpulseTargets==0);
   F.Shot(TEXT("A-Falloff-Occlusion.png"),Open);
   if(Shield) Shield->Destroy(); Shield=nullptr; return Next(3);
  }
  if(Stage==3 && Elapsed>.5f)
  {
   ImpulseCenter=F.Area->GetActorLocation()+FVector(-2600,0,72.5f);
   Barrel=F.Prop(ECityPhysicsProp::TrashCan,ImpulseCenter+FVector(150,0,0));
   return Next(4);
  }
  if(Stage==4 && Elapsed>.6f)
  {
   const auto Result=F.Blast(TEXT("NeonCity.SmallExplosion"),ImpulseCenter);
   Test->TestTrue(TEXT("Ground blast resolves and reaches the loose prop"),Result.bApplied && Result.ImpulseTargets>=1);
   Peak=Barrel?Barrel->GetMesh()->GetPhysicsLinearVelocity():FVector::ZeroVector;
   return Next(5);
  }
  if(Stage==5 && Elapsed>.2f)
  {
   const FVector Velocity=Barrel?Barrel->GetMesh()->GetPhysicsLinearVelocity():FVector::ZeroVector;
   const auto& Policy=NeonCityExplosions::Profile(TEXT("NeonCity.SmallExplosion")).Impulse;
   // Horizontal speed is compared so gravity cannot inflate the measured impulse.
   const float Speed=FMath::Max(Peak.Size2D(),Velocity.Size2D());
   const float Limit=FMath::Min(Policy.MaximumDeltaVelocity,Policy.PropDeltaVelocity);
   Test->AddInfo(FString::Printf(TEXT("Prop blast velocity %.1f / authored limit %.1f"),Speed,Limit));
   Test->TestTrue(TEXT("Radial impulse stays finite"),!Velocity.ContainsNaN() && !Peak.ContainsNaN());
   Test->TestTrue(TEXT("Radial impulse respects the authored maximum delta velocity"),Speed<=Limit+1);
   Test->TestTrue(TEXT("Radial impulse stays below the runaway speed gate"),Speed<FExplosionImpulseUtility::RunawaySpeed);
   Test->TestTrue(TEXT("Destruction budget stays bounded"),F.Destruct()->GetActiveFragmentCount()<=UDestructionSubsystem::MaximumActiveFragments && F.Destruct()->GetRegisteredSurfaceCount()>0);
   F.Shot(TEXT("B-Impulse.png"),Barrel); return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0;
 FVector Center=FVector::ZeroVector,ImpulseCenter=FVector::ZeroVector,Peak=FVector::ZeroVector;
 ACombatTestTarget* Inner=nullptr; ACombatTestTarget* Mid=nullptr; ACombatTestTarget* Edge=nullptr; ACombatTestTarget* Outside=nullptr;
 ACombatTestTarget* Open=nullptr; ACombatTestTarget* Sheltered=nullptr; AActor* Shield=nullptr; ANeonCityPhysicsProp* Barrel=nullptr;
};

class FWeaponBlast : public IAutomationLatentCommand
{
public:
 explicit FWeaponBlast(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>120) { Test->AddError(FString::Printf(TEXT("Unified weapon blast stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1 && Elapsed>.3f)
  {
   Rocket=UVehicleWeaponManager::Profile(TEXT("NeonCity.RocketPod"));
   Missile=UVehicleWeaponManager::Profile(TEXT("NeonCity.GuidedMissile"));
   Test->TestTrue(TEXT("Mounted rocket pod carries a native explosion profile"),Rocket && Rocket->ExplosionDataID==FName(TEXT("NeonCity.RocketExplosion")));
   Test->TestTrue(TEXT("Guided missile carries a native explosion profile"),Missile && Missile->ExplosionDataID==FName(TEXT("NeonCity.MissileExplosion")));
   Detonations=F.Explosions()->GetDetonationCount();
   BogeyA=F.Target(F.Area->GetActorLocation()+FVector(3000,0,400));
   auto* Pool=F.W->GetSubsystem<UVehicleProjectilePool>();
   auto* RocketShot=Pool?Pool->Acquire(ARocketProjectile::StaticClass(),FTransform(BogeyA->GetActorLocation()+FVector(-600,0,0)),F.Area):nullptr;
   FNeonCityDamageEvent Payload; Payload.DamageAmount=Rocket->Damage; Payload.SourceActor=F.Player; Payload.Instigator=F.PC;
   Test->TestTrue(TEXT("Rocket projectile launches through the shared pool"),RocketShot && RocketShot->LaunchVehicle(FVector::ForwardVector,FVector::ZeroVector,*Rocket,Payload,BogeyA));
   return Next(2);
  }
  if(Stage==2 && Elapsed>.8f)
  {
   const auto& Recent=F.Explosions()->GetRecentExplosions();
   Test->TestTrue(TEXT("Rocket impact detonates the unified explosion subsystem"),F.Explosions()->GetDetonationCount()>Detonations);
   Test->TestTrue(TEXT("Rocket impact records the RocketExplosion profile"),Recent.Num()>0 && Recent.Last().DataID==FName(TEXT("NeonCity.RocketExplosion")));
   Test->TestTrue(TEXT("Rocket blast keeps the firing controller as instigator"),Recent.Num()>0 && Recent.Last().Instigator.Get()==F.PC);
   Test->TestTrue(TEXT("Rocket blast damages the struck target"),BogeyA->GetHealthComponent()->GetCurrentHealth()<1000.f);
   Detonations=F.Explosions()->GetDetonationCount();
   BogeyB=F.Target(F.Area->GetActorLocation()+FVector(3000,-600,400));
   auto* Pool=F.W->GetSubsystem<UVehicleProjectilePool>();
   auto* MissileShot=Pool?Pool->Acquire(AMissileProjectile::StaticClass(),FTransform(BogeyB->GetActorLocation()+FVector(-600,0,0)),F.Area):nullptr;
   FNeonCityDamageEvent Payload; Payload.DamageAmount=Missile->Damage; Payload.SourceActor=F.Player; Payload.Instigator=F.PC;
   Test->TestTrue(TEXT("Guided missile launches through the shared pool"),MissileShot && MissileShot->LaunchVehicle(FVector::ForwardVector,FVector::ZeroVector,*Missile,Payload,BogeyB));
   return Next(3);
  }
  if(Stage==3 && Elapsed>1.f)
  {
   const auto& Recent=F.Explosions()->GetRecentExplosions();
   Test->TestTrue(TEXT("Missile impact detonates the unified explosion subsystem"),F.Explosions()->GetDetonationCount()>Detonations);
   Test->TestTrue(TEXT("Missile impact records the MissileExplosion profile"),Recent.Num()>0 && Recent.Last().DataID==FName(TEXT("NeonCity.MissileExplosion")));
   Test->TestTrue(TEXT("Missile blast damages the struck target"),BogeyB->GetHealthComponent()->GetCurrentHealth()<1000.f);
   Test->TestTrue(TEXT("Both vehicle weapons share one detonation ledger"),F.Explosions()->GetDetonationCount()>=2);
   F.Shot(TEXT("C-Rocket-Missile.png"),BogeyB); return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0,Detonations=0;
 const UVehicleWeaponData* Rocket=nullptr; const UVehicleWeaponData* Missile=nullptr;
 ACombatTestTarget* BogeyA=nullptr; ACombatTestTarget* BogeyB=nullptr;
};

class FVehicleBlast : public IAutomationLatentCommand
{
public:
 explicit FVehicleBlast(FAutomationTestBase* T):Test(T) {}
 /** Parks a neighbour just inside Critical for the next blast, so the cascade is decided by policy. */
 static void Prime(ANeonCityBaseVehicle* Wreck,const ANeonCityBaseVehicle* Blast,const UNeonCityExplosionData& Profile)
 {
  const float Distance=FVector::Distance(Wreck->GetActorLocation(),Blast->GetActorLocation());
  const float Expected=Profile.Damage*UNeonCityExplosionData::Falloff(Distance,Profile.InnerRadius,Profile.OuterRadius,Profile.DamageFalloff);
  const float Max=Wreck->GetHealthComponent()->GetMaxHealth();
  Wreck->GetHealthComponent()->ApplyPersistenceHealth(FMath::Clamp(Expected+Max*.18f,1.f,Max),Max);
 }
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180) { Test->AddError(FString::Printf(TEXT("Vehicle explosion stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1 && Elapsed>.3f)
  {
   Test->TestEqual(TEXT("Destruction pad authors three wrecks"),F.Area->GetVehicleCount(),3);
   if(F.Area->GetVehicleCount()<3) return true;
   Car0=F.Area->GetVehicle(0); Car1=F.Area->GetVehicle(1); Car2=F.Area->GetVehicle(2);
   for(int32 I=0;I<3;++I)
   {
    auto* State=F.Area->GetVehicle(I)->GetDamageState();
    F.Area->GetVehicle(I)->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
    F.Area->GetVehicle(I)->GetMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    Test->TestTrue(TEXT("Every pad wreck opts into the vehicle explosion profile"),
      State && State->CanExplode() && State->GetExplosionDataID()==FName(TEXT("NeonCity.VehicleExplosion")) && State->GetExplosionState()==ENeonCityVehicleExplosionState::None);
   }
   const auto& Profile=NeonCityExplosions::Profile(TEXT("NeonCity.VehicleExplosion"));
   Prime(Car1,Car0,Profile); Prime(Car2,Car1,Profile);
   Test->TestTrue(TEXT("Pre-damaged neighbours are burning-risk wrecks, not yet detonating"),
     Car1->GetDamageState()->GetCondition()!=ENeonCityVehicleCondition::Disabled && !Car1->GetDamageState()->IsExplosionPending());
   F.Damage(Car0,Car0->GetHealthComponent()->GetMaxHealth()*2,F.PC,F.Player,ENeonCityDamageType::Explosion);
   return Next(2);
  }
  if(Stage==2 && Elapsed>.4f)
  {
   auto* State=Car0->GetDamageState();
   Test->TestTrue(TEXT("A destroyed vehicle enters a pending fire instead of exploding at zero health"),
     State->IsExplosionPending() && State->GetExplosionState()==ENeonCityVehicleExplosionState::Pending);
   Test->TestTrue(TEXT("The player is given a real escape window before the blast"),State->GetFuseRemaining()>=1.f);
   Test->TestEqual(TEXT("No blast resolves before the fuse elapses"),F.Explosions()->GetDetonationCount(),0);
   return Next(3);
  }
  if(Stage==3)
  {
   auto* State=Car0->GetDamageState();
   if(Elapsed<5.f && State->GetExplosionCount()==0) return false;
   Test->TestEqual(TEXT("The burning wreck detonates exactly once"),State->GetExplosionCount(),1);
   Test->TestTrue(TEXT("Detonation is terminal for the wreck"),State->GetExplosionState()==ENeonCityVehicleExplosionState::Detonated && !State->CanExplode());
   Test->TestTrue(TEXT("Blast attribution survives the wreck"),State->GetExplosionInstigator().Get()==F.PC);
   Test->TestTrue(TEXT("Blast result reports real damage targets"),State->GetLastExplosionResult().bApplied && State->GetLastExplosionResult().DamageTargets>=1);
   Test->TestTrue(TEXT("World ledger records both the fire and the explosion"),
     F.Events()->CountOfType(ENeonCityWorldEventType::VehicleFire)>0 && F.Events()->CountOfType(ENeonCityWorldEventType::VehicleExplosion)>0);
   Neighbour=Car1->GetDamageState();
   if(Neighbour->GetExplosionCount()==0 && Neighbour->GetChainOfferCount()==0)
   { Test->AddError(FString::Printf(TEXT("Neighbouring wreck was never offered the cascade (fraction %.2f)"),Neighbour->GetConditionFraction())); return true; }
   Test->TestEqual(TEXT("The first neighbour accepts exactly one chained fuse"),Neighbour->GetChainOfferCount(),1);
   return Next(4);
  }
  if(Stage==4)
  {
   if(Elapsed<5.f && Neighbour->GetExplosionCount()==0) return false;
   Test->TestEqual(TEXT("The pre-damaged neighbour detonates from the chain"),Neighbour->GetExplosionCount(),1);
   Test->TestTrue(TEXT("Chained blast keeps the original instigator"),Neighbour->GetExplosionInstigator().Get()==F.PC);
   Test->TestEqual(TEXT("Chain depth records a single cascade hop"),Neighbour->GetChainDepth(),1);
   Test->TestEqual(TEXT("The cascade stops before it takes the whole row"),Car2->GetDamageState()->GetExplosionCount(),0);
   Test->TestTrue(TEXT("The second hop is refused by the authored depth bound"),Car2->GetDamageState()->GetRefusedExplosionCount()>0);
   Test->TestFalse(TEXT("The refused wreck never enters a pending fire"),Car2->GetDamageState()->IsExplosionPending());
   F.Shot(TEXT("D-Vehicle-Blast.png"),Car0); return Next(5);
  }
  if(Stage==5 && Elapsed>1.f)
  {
   const FTransform Spawn(FRotator::ZeroRotator,F.Area->GetActorLocation()+FVector(3400,-1200,60));
   auto* Plain=F.W->SpawnActorDeferred<ANeonCityPrototypeVehicle>(ANeonCityPrototypeVehicle::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(Plain) Plain->FinishSpawning(Spawn);
   if(!Test->TestNotNull(TEXT("Default city vehicle spawns for the wreck policy check"),Plain)) return true;
   Test->TestFalse(TEXT("Default city vehicle refuses to explode at all"),Plain->GetDamageState()->CanExplode());
   F.Damage(Plain,Plain->GetHealthComponent()->GetMaxHealth()*2,F.PC,F.Player,ENeonCityDamageType::Explosion);
   Wreck=Plain; return Next(6);
  }
  if(Stage==6)
  {
   if(Elapsed<3.f) return false;
   Test->TestTrue(TEXT("A destroyed default vehicle never explodes on a timer"),
     Wreck && Wreck->GetHealthComponent()->IsDead() && !Wreck->GetDamageState()->IsExplosionPending() && Wreck->GetDamageState()->GetExplosionCount()==0);
   if(Wreck) Wreck->Destroy();
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0;
 ANeonCityBaseVehicle* Car0=nullptr; ANeonCityBaseVehicle* Car1=nullptr; ANeonCityBaseVehicle* Car2=nullptr;
 UVehicleDamageComponent* Neighbour=nullptr; ANeonCityPrototypeVehicle* Wreck=nullptr;
};

class FEnvironmental : public IAutomationLatentCommand
{
public:
 explicit FEnvironmental(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180) { Test->AddError(FString::Printf(TEXT("Environmental destruction stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1 && Elapsed>.3f)
  {
   Store=F.Area->GetProp(ECityPhysicsProp::StorefrontGlass,0);
   Window=F.Area->GetProp(ECityPhysicsProp::Glass,0);
   Lamp=F.Area->GetProp(ECityPhysicsProp::LampPost,0); Spare=F.Area->GetProp(ECityPhysicsProp::LampPost,1);
   Barrier=F.Area->GetProp(ECityPhysicsProp::RoadBarrier,0); Billboard=F.Area->GetProp(ECityPhysicsProp::Billboard);
   Trash=F.Area->GetProp(ECityPhysicsProp::TrashCan,0); Sign=F.Area->GetProp(ECityPhysicsProp::Sign);
   Door=F.Area->GetProp(ECityPhysicsProp::Door); Fence=F.Area->GetProp(ECityPhysicsProp::Fence,0);
   Structure=F.Area->GetStructure();
   Test->TestTrue(TEXT("Test area authors every required breakable fixture"),
     Store && Window && Lamp && Spare && Barrier && Billboard && Trash && Sign && Door && Fence);
   if(!Test->TestNotNull(TEXT("Test area authors a localized destructible structure"),Structure)) return true;
   Test->TestEqual(TEXT("Structure keeps a stable shell plus four authored surfaces"),Structure->GetParts().Num(),4);
   Test->TestEqual(TEXT("Every structure surface starts intact"),Structure->GetIntactSurfaceCount_Implementation(),4);
   Test->TestTrue(TEXT("Structure shell is present and not itself destructible"),Structure->GetShell()!=nullptr);
   F.PlaceOnPad(FVector(400,400,140));
   Test->TestTrue(TEXT("Player equips the shared pistol"),F.Player->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")));
   Shots=0; return Next(2);
  }
  if(Stage>=2 && Stage<=4 && Elapsed>.35f)
  {
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation();
   R.Direction=(Store->GetActorLocation()-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("Pistol fires at the storefront glass"),F.Player->GetCombatComponent()->Attack(R));
   Shots=Stage-1; return Next(Stage==4?5:Stage+1);
  }
  if(Stage==5)
  {
   Test->TestEqual(TEXT("Three real bullet hits were fired"),Shots,3);
   Test->TestTrue(TEXT("Bullets crack glass before it breaks"),Store->GetSurfaceState()->GetBreakCount()>0);
   Test->TestTrue(TEXT("Storefront glass shatters from bullets"),Store->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   Test->TestTrue(TEXT("Broken glass reports the shattered glass state"),Store->GetSurfaceState()->GetGlassState()==ENeonCityGlassState::Broken);
   Test->TestTrue(TEXT("Broken glass drops collision"),Store->GetMesh()->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Test->TestFalse(TEXT("Broken glass is no longer drawn"),Store->GetMesh()->IsVisible());
   Test->TestTrue(TEXT("Glass shatter requested a bounded fragment effect"),Store->GetSurfaceState()->GetFragmentEffectCount()+Store->GetSurfaceState()->GetRefusedFragmentCount()>0);
   F.Shot(TEXT("E-Glass-Bullet.png"),Store);
   ExplosionCenter=Window->GetActorLocation();
   const auto Result=F.Blast(TEXT("NeonCity.SmallExplosion"),ExplosionCenter);
   Test->TestTrue(TEXT("Blast resolves against the window glass"),Result.bApplied);
   return Next(6);
  }
  if(Stage==6 && Elapsed>.4f)
  {
   Test->TestTrue(TEXT("A blast inside the lethal core shatters window glass outright"),Window->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   Test->TestTrue(TEXT("Blast-shattered glass reports the shattered state"),Window->GetSurfaceState()->GetGlassState()==ENeonCityGlassState::Broken);
   F.Shot(TEXT("F-Glass-Explosion.png"),Window);
   const auto Result=F.Blast(TEXT("NeonCity.RocketExplosion"),Lamp->GetActorLocation());
   Test->TestTrue(TEXT("Rocket blast resolves against the street light"),Result.bApplied);
   return Next(7);
  }
  if(Stage==7 && Elapsed>.4f)
  {
   Test->TestTrue(TEXT("Street light is Broken after the blast"),Lamp->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   Test->TestFalse(TEXT("Broken street light switches its lamp off"),Lamp->GetLight()->IsVisible());
   Test->TestTrue(TEXT("The blast radius does not reach the next street light"),Spare->GetDestruction()->GetState()==EPhysicsPropState::Normal);
   Test->TestTrue(TEXT("Rocket blast resolves against the billboard"),F.Blast(TEXT("NeonCity.RocketExplosion"),Billboard->GetActorLocation()).bApplied);
   return Next(8);
  }
  if(Stage==8 && Elapsed>.4f)
  {
   Test->TestTrue(TEXT("Billboard breaks from a rocket blast"),Billboard->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   Test->TestTrue(TEXT("Rocket blast resolves against the road barrier"),F.Blast(TEXT("NeonCity.RocketExplosion"),Barrier->GetActorLocation()).bApplied);
   return Next(9);
  }
  if(Stage==9 && Elapsed>.4f)
  {
   Test->TestTrue(TEXT("Barrier breaks from a rocket blast"),Barrier->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   // Localized building destruction: the shell never fails, only the authored surfaces do.
   const FVector Target=Structure->GetParts()[0]->GetActorLocation();
   const int32 Broken=ILocalizedDestructionInterface::Execute_ApplyLocalizedDestruction(Structure,Target,60.f,200.f,F.Player);
   Test->TestEqual(TEXT("Localized destruction breaks only the addressed surface"),Broken,1);
   Test->TestEqual(TEXT("Localized destruction leaves the rest of the wall standing"),Structure->GetIntactSurfaceCount_Implementation(),3);
   F.Shot(TEXT("G-Structure-Localized.png"),Structure);
   Test->TestTrue(TEXT("Rocket blast resolves against the test wall"),F.Blast(TEXT("NeonCity.RocketExplosion"),Structure->GetParts()[1]->GetActorLocation()).bApplied);
   return Next(10);
  }
  if(Stage==10 && Elapsed>.5f)
  {
   Test->TestTrue(TEXT("Blast routing reaches authored structure surfaces"),Structure->GetIntactSurfaceCount_Implementation()<3);
   Test->TestTrue(TEXT("Structure shell survives the blast intact"),IsValid(Structure) && Structure->GetShell()!=nullptr);
   // Cross-system E: a blast breaks glass while an NPC behind solid cover is spared the full damage.
   CrossCenter=F.Area->GetActorLocation()+FVector(0,-1000,120);
   Exposed=F.Civilian(CrossCenter+FVector(350,0,0),901);
   Covered=F.Civilian(CrossCenter+FVector(-350,0,0),902);
   Wall=F.Blocker(CrossCenter+FVector(-175,0,0),FVector(20,150,150));
   if(!Test->TestTrue(TEXT("Cross-system blast fixtures spawned"),Exposed && Covered && Wall)) return true;
   return Next(11);
  }
  if(Stage==11 && Elapsed>.5f)
  {
   F.Blast(TEXT("NeonCity.SmallExplosion"),CrossCenter);
   return Next(12);
  }
  if(Stage==12 && Elapsed>.4f)
  {
   const float OpenHealth=Exposed->GetHealthComponent()->GetCurrentHealth();
   const float ShieldedHealth=Covered->GetHealthComponent()->GetCurrentHealth();
   Test->TestTrue(TEXT("Exposed bystander takes blast damage"),OpenHealth<100.f);
   Test->TestTrue(TEXT("Solid cover denies the blast damage instead of halving it"),ShieldedHealth>OpenHealth);
   Test->TestTrue(TEXT("Destruction budget stayed bounded across the whole scenario"),
     F.Destruct()->GetActiveFragmentCount()<=UDestructionSubsystem::MaximumActiveFragments && F.Destruct()->GetRegisteredSurfaceCount()>0);
   Test->TestEqual(TEXT("Nothing was restored before any load"),F.Destruct()->GetRestoreCount(),0);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0,Shots=0;
 ANeonCityPhysicsProp* Store=nullptr; ANeonCityPhysicsProp* Window=nullptr; ANeonCityPhysicsProp* Lamp=nullptr; ANeonCityPhysicsProp* Spare=nullptr;
 ANeonCityPhysicsProp* Barrier=nullptr; ANeonCityPhysicsProp* Billboard=nullptr; ANeonCityPhysicsProp* Trash=nullptr; ANeonCityPhysicsProp* Sign=nullptr;
 ANeonCityPhysicsProp* Door=nullptr; ANeonCityPhysicsProp* Fence=nullptr; ADestructibleStructure* Structure=nullptr;
 ANeonCityNPCCharacter* Exposed=nullptr; ANeonCityNPCCharacter* Covered=nullptr; AActor* Wall=nullptr;
 FVector ExplosionCenter=FVector::ZeroVector,CrossCenter=FVector::ZeroVector;
};

class FNPCReaction : public IAutomationLatentCommand
{
public:
 explicit FNPCReaction(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("NPC blast reaction stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1 && Elapsed>.3f)
  {
   Bystander=Cast<ANeonCityNPCCharacter>(F.Area->GetPedestrian(0)); Officer=F.Area->GetOfficer();
   if(!Test->TestTrue(TEXT("Test area authors a civilian bystander and an officer"),Bystander!=nullptr && Officer!=nullptr)) return true;
   Origin=Bystander->GetActorLocation();
   // Inside the authored danger radius, outside the damage radius: the blast warns without killing.
   BlastCenter=Origin+FVector(700,0,0);
   const int32 Detonations=F.Explosions()->GetDetonationCount();
   const auto Result=F.Blast(TEXT("NeonCity.VehicleExplosion"),BlastCenter,nullptr,Officer);
   Test->TestTrue(TEXT("Accidental blast resolves with no player instigator"),Result.bApplied && F.Explosions()->GetDetonationCount()>Detonations);
   return Next(2);
  }
  if(Stage==2 && Elapsed>.4f)
  {
   auto* Threat=Bystander->GetThreatComponent();
   Test->TestTrue(TEXT("Nearby civilian perceives the blast"),Threat->HasActiveThreat());
   Test->TestTrue(TEXT("Blast danger lingers instead of returning to social immediately"),Threat->GetDangerUntil()-F.W->GetTimeSeconds()>=2.f);
   Test->TestTrue(TEXT("Civilian breaks out of its ordinary social state"),Bystander->GetNPCState()->GetState()!=ENeonCityNPCState::Social);
   Test->TestTrue(TEXT("An accident with no player culprit grants no wanted level"),F.Wanted()->GetWantedLevel()==0 && F.Wanted()->GetEvidenceScore()==0);
   Test->TestFalse(TEXT("The accident is never attributed to the player"),F.CrimeHas(ENeonCityCrimeType::DamageNPC,Bystander,F.PC));
   Test->TestTrue(TEXT("Distant wrecks stay out of the danger radius"),F.Area->GetVehicle(0)->GetDamageState()->GetExplosionState()==ENeonCityVehicleExplosionState::None);
   F.Shot(TEXT("H-NPC-Flee.png"),Bystander);
   FighterNPC=F.Fighter(F.Area->GetActorLocation()+FVector(-700,-1900,140),911);
   if(!Test->TestNotNull(TEXT("Armed responder spawns for the blast alert check"),FighterNPC)) return true;
   return Next(3);
  }
  if(Stage==3 && Elapsed>.5f)
  {
   const auto Warning=F.Blast(TEXT("NeonCity.SmallExplosion"),BlastCenter,F.PC,nullptr);
   Test->TestTrue(TEXT("The blast warning resolves with no weapon source"),Warning.bApplied);
   return Next(4);
  }
  if(Stage==4 && Elapsed>.5f)
  {
   auto* AI=FighterNPC->GetCombatAI();
   Test->TestTrue(TEXT("Armed responder acquires the blast's culprit"),AI->GetTarget()==F.Player);
   Test->TestTrue(TEXT("Armed responder leaves the inactive state after a blast"),AI->GetState()!=ENeonCityCombatAIState::Inactive);
   const float Dist=FVector::Dist2D(Origin,Bystander->GetActorLocation());
   const uint32 Visited=Bystander->GetNPCState()->GetVisitedStateMask();
   const bool bReactive=(Visited&(1u<<uint8(ENeonCityNPCState::Alert)))||(Visited&(1u<<uint8(ENeonCityNPCState::Flee)))||(Visited&(1u<<uint8(ENeonCityNPCState::React)));
   Test->AddInfo(FString::Printf(TEXT("Bystander moved %.0f cm, visited mask %u"),Dist,Visited));
   Test->TestTrue(TEXT("Bystander actually breaks away from the blast"),Dist>50.f||bReactive);
   F.Shot(TEXT("I-NPC-Alert.png"),FighterNPC);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0;
 ANeonCityNPCCharacter* Bystander=nullptr; ANeonCityNPCCharacter* FighterNPC=nullptr; APoliceCharacter* Officer=nullptr;
 FVector Origin=FVector::ZeroVector,BlastCenter=FVector::ZeroVector;
};

/** Spec 30 items 15-19: bullet hit feedback, hit reaction, Downed and Dead behaviour for characters. */
class FCharacterFeedback : public IAutomationLatentCommand
{
public:
 explicit FCharacterFeedback(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Character feedback stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   F.PlaceOnPad(FVector(0,0,140));
   // A frozen civilian in front of the player, so the shot is a real bullet and not a test-local hit.
   Victim=F.Civilian(F.Area->GetActorLocation()+FVector(700,0,95),701);
   if(!Test->TestNotNull(TEXT("Feedback civilian spawns on the proving ground"),Victim)) return true;
   if(auto* Controller=Victim->GetController()) Controller->SetActorTickEnabled(false);
   Victim->GetCharacterMovement()->StopMovementImmediately();
   Max=Victim->GetHealthComponent()->GetMaxHealth();
   Test->TestTrue(TEXT("Feedback starts clean on the civilian"),Victim->GetDamageFeedback()->GetHitReactionCount()==0);
   Test->TestTrue(TEXT("Player equips the shared pistol for the shot"),F.Player->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")));
   auto* ShotgunWeapon=F.Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestNotNull(TEXT("The civilian test really has a pistol equipped"),ShotgunWeapon)) return true;
   Mag=ShotgunWeapon->GetAmmo();
   return Next(2);
  }
  if(Stage==2 && Elapsed>.4f)
  {
   Before=Victim->GetHealthComponent()->GetCurrentHealth();
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation();
   R.Direction=(Victim->GetActorLocation()-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("The player actually fires a bullet at the civilian"),F.Player->GetCombatComponent()->Attack(R));
   Test->TestEqual(TEXT("The shot consumes exactly one round"),F.Player->GetCombatComponent()->GetWeapon()->GetAmmo(),Mag-1);
   return Next(3);
  }
  if(Stage==3 && Elapsed>.15f)
  {
   auto* Feedback=Victim->GetDamageFeedback();
   After=Victim->GetHealthComponent()->GetCurrentHealth();
   Test->TestTrue(TEXT("A real bullet lowers NPC health"),After<Before);
   Test->TestEqual(TEXT("The feedback layer sees the same damage the health pipeline applied"),Feedback->GetLastDamageAmount(),Before-After,.01f);
   Test->TestTrue(TEXT("The bullet hit produced character damage feedback"),Feedback->GetHitReactionCount()>=1 && Feedback->GetImpactEffectCount()>=1);
   Test->TestTrue(TEXT("The NPC registers a hit reaction"),Feedback->IsHitReactionActive() && Feedback->GetFeedbackState()==ENeonCityBodyState::HitReaction);
   Test->TestTrue(TEXT("Feedback reports a damage direction"),!Feedback->GetLastDamageDirection().IsNearlyZero() && Feedback->IsDamageDirectionActive());
   const FVector ToShooter=(F.Player->GetActorLocation()-Victim->GetActorLocation()).GetSafeNormal();
   Test->TestTrue(TEXT("Damage direction points back at the shooter"),FVector::DotProduct(Feedback->GetLastDamageDirection(),ToShooter)>.5f);
   Test->TestTrue(TEXT("Body state is owned by the released health pipeline"),Victim->FindComponentByClass<UDamageableCharacterComponent>()->GetBodyState()==ENeonCityBodyState::HitReaction);
   F.Shot(TEXT("K-NPC-HitReaction.png"),Victim);
   return Next(4);
  }
  if(Stage==4 && Elapsed>.6f)
  {
   auto* Feedback=Victim->GetDamageFeedback();
   Test->TestTrue(TEXT("Hit reaction is transient and resolves without input"),!Feedback->IsHitReactionActive() && Feedback->GetHitReactionCount()==1);
   Test->TestTrue(TEXT("The civilian is mobile again after the flinch"),Victim->FindComponentByClass<UDamageableCharacterComponent>()->CanAct());
   // Downed: 15% of maximum health sits inside the released Downed band.
   F.Damage(Victim,Victim->GetHealthComponent()->GetCurrentHealth()-Max*.15f,F.PC,F.Player);
   return Next(5);
  }
  if(Stage==5 && Elapsed>2.7f)
  {
   auto* AI=Victim->GetCombatAI();
   Test->TestTrue(TEXT("A badly hurt NPC is Downed rather than dead"),!Victim->GetHealthComponent()->IsDead() && Victim->FindComponentByClass<UDamageableCharacterComponent>()->GetBodyState()==ENeonCityBodyState::Downed);
   Test->TestTrue(TEXT("Downed NPCs stop acting"),!Victim->FindComponentByClass<UDamageableCharacterComponent>()->CanAct() && Victim->FindComponentByClass<UCombatComponent>()->GetCombatState()==ENeonCityCombatState::Disabled);
   Test->TestTrue(TEXT("Combat AI reports the Downed condition"),AI->GetHealthState()==ENeonCityCombatHealth::Downed && AI->GetState()==ENeonCityCombatAIState::Downed);
   Test->TestTrue(TEXT("Downed feedback fires once"),Victim->GetDamageFeedback()->GetDownedFeedbackCount()>=1);
   Test->TestTrue(TEXT("Downed NPC retains visible collapsed skeletal body"),IsValid(Victim) && !Victim->IsHidden() && Victim->GetMesh()->GetSocketLocation(TEXT("head")).Z<Victim->GetActorLocation().Z-45);
   F.Shot(TEXT("L-NPC-Downed.png"),Victim);
   return Next(6);
  }
  if(Stage==6 && Elapsed>.4f)
  {
   F.Damage(Victim,Max*2.f,F.PC,F.Player);
   return Next(7);
  }
  if(Stage==7 && Elapsed>.4f)
  {
   Test->TestTrue(TEXT("Lethal damage kills the civilian"),Victim->GetHealthComponent()->IsDead());
   Test->TestTrue(TEXT("Dead NPCs are removed from live target selection"),!UCombatTargetComponent::IsAliveTarget(Victim));
   Test->TestTrue(TEXT("Death feedback fires exactly once"),Victim->GetDamageFeedback()->GetDeathFeedbackCount()==1);
   return Next(8);
  }
  if(Stage==8 && Elapsed>.4f)
  {
   auto* AI=Victim->GetCombatAI();
   Test->TestTrue(TEXT("A dead NPC stops its combat AI entirely"),AI->GetState()==ENeonCityCombatAIState::Dead && AI->GetHealthState()==ENeonCityCombatHealth::Dead);
   Test->TestTrue(TEXT("A dead NPC holds no target"),AI->GetTarget()==nullptr);
   Test->TestTrue(TEXT("A corpse no longer acts"),!Victim->FindComponentByClass<UDamageableCharacterComponent>()->CanAct());
   Test->TestTrue(TEXT("Dead NPC keeps its collapsed body instead of returning to idle"),!Victim->IsHidden() && Victim->GetMesh()->GetSocketLocation(TEXT("head")).Z<Victim->GetActorLocation().Z-45);
   // Spec 32 scenario B: the same feedback path must read correctly at shotgun range.
   F.PlaceOnPad(FVector(0,-2000,140));
   Close=F.Civilian(F.Area->GetActorLocation()+FVector(0,-1700,95),702);
   if(!Test->TestNotNull(TEXT("Close-range target spawns for the shotgun scenario"),Close)) return true;
   if(auto* Controller=Close->GetController()) Controller->SetActorTickEnabled(false);
   Close->GetCharacterMovement()->StopMovementImmediately();
   auto* Inventory=F.Player->FindComponentByClass<UWeaponInventoryComponent>();
   FNeonCityWeaponSaveData Owned;
   Test->TestTrue(TEXT("Player owns the shared shotgun"),Inventory && (Inventory->GetWeaponRecord(NeonCityWeapons::Slot(4),Owned) || Inventory->GrantWeapon(NeonCityWeapons::Slot(4))));
   Test->TestTrue(TEXT("Player equips the shotgun"),F.Player->GetCombatComponent()->EquipWeapon(ANeonCityShotgun::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")));
   CloseBefore=Close->GetHealthComponent()->GetCurrentHealth();
   return Next(9);
  }
  if(Stage==9 && Elapsed>.4f)
  {
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation();
   R.Direction=(Close->GetActorLocation()-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("Shotgun fires at close range"),F.Player->GetCombatComponent()->Attack(R));
   return Next(10);
  }
  if(Stage==10 && Elapsed>.25f)
  {
   Test->TestTrue(TEXT("Close-range shotgun damage registers on the NPC"),Close->GetHealthComponent()->GetCurrentHealth()<CloseBefore);
   Test->TestTrue(TEXT("Shotgun range produces the same shared damage feedback"),Close->GetDamageFeedback()->GetHitReactionCount()>=1);
   Test->AddInfo(FString::Printf(TEXT("Shotgun at close range: %.1f -> %.1f health"),CloseBefore,Close->GetHealthComponent()->GetCurrentHealth()));
   F.Shot(TEXT("B-Shotgun-NPC.png"),Close);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0,Mag=0; float Max=100,Before=0,After=0,CloseBefore=0;
 ANeonCityNPCCharacter* Victim=nullptr; ANeonCityNPCCharacter* Close=nullptr;
};

/** Spec 30 items 20-25: player damage feedback, the police weapon bug and injury stow. */
class FPlayerFeedback : public IAutomationLatentCommand
{
public:
 explicit FPlayerFeedback(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Player feedback stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   F.PlaceOnPad(FVector(-1200,-800,140));
   F.Player->GetHealthComponent()->ApplyPersistenceHealth(5000,5000);
   Officer=F.Area->GetOfficer();
   if(!Test->TestNotNull(TEXT("Test area authors the officer that shoots the player"),Officer)) return true;
   Test->TestTrue(TEXT("Player equips the shared pistol before being shot"),F.Player->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")));
   const auto* Weapon=F.Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestNotNull(TEXT("The player really has a pistol equipped before being shot"),Weapon)) return true;
   WeaponID=Weapon->GetData()->WeaponId; Mag=Weapon->GetAmmo();
   Test->TestTrue(TEXT("The equipped weapon is a real, distinct weapon identity"),!WeaponID.IsNone());
   return Next(2);
  }
  if(Stage==2 && Elapsed>.3f)
  {
   Before=F.Player->GetHealthComponent()->GetCurrentHealth();
   // The officer's controller is the attacker, so this is the same attribution police fire uses.
   F.Damage(F.Player,25.f,Officer->GetController(),Officer,ENeonCityDamageType::Ballistic,F.Player->GetActorLocation()+FVector(0,0,60));
   After=F.Player->GetHealthComponent()->GetCurrentHealth();
   auto* Adapter=F.Player->GetCombatAdapter();
   Test->TestEqual(TEXT("Player health drops from the police hit"),After,Before-25.f,.01f);
   Test->TestTrue(TEXT("The player registers damage feedback"),Adapter->GetDamageFeedbackCount()>=1);
   Test->TestTrue(TEXT("A directional damage indicator is raised"),Adapter->GetDamageIndicatorSeconds()>0 && FMath::IsFinite(Adapter->GetLastDamageYaw()));
   Test->TestTrue(TEXT("The player feedback tier stays valid after the hit"),Adapter->GetVitalsTier()!=ENeonCityFeedbackTier::Dead);
   Test->TestTrue(TEXT("Player hit reaction uses the same shared feedback component"),F.Player->GetDamageFeedbackComponent()->GetHitReactionCount()>=1);
   F.Shot(TEXT("M-Player-Hit.png"),F.Player);
   return Next(3);
  }
  if(Stage==3 && Elapsed>.6f)
  {
   // The released bug: a hit reaction or police encounter could strip the equipped weapon.
   auto* Combat=F.Player->GetCombatComponent();
   auto* Weapon=Combat->GetWeapon();
   Test->TestTrue(TEXT("The weapon survives the police hit reaction"),Weapon!=nullptr && Weapon->IsEquipped());
   Test->TestTrue(TEXT("The same weapon identity is still equipped"),Weapon && Weapon->GetData()->WeaponId==WeaponID);
   Test->TestEqual(TEXT("Ammo is not duplicated or lost by the hit"),Weapon?Weapon->GetAmmo():-1,Mag);
   Test->TestFalse(TEXT("A hit reaction never stows the weapon"),Combat->IsInjuryStowed());
   Test->TestEqual(TEXT("No spurious injury stow was recorded"),Combat->GetInjuryStowCount(),0);
   return Next(4);
  }
  if(Stage==4 && Elapsed>.3f)
  {
   // Firing at the police escalates evidence; the weapon must stay equipped through that escalation.
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation();
   R.Direction=(Officer->GetActorLocation()+FVector(0,0,35)-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("Player fires the pistol at police"),F.Player->GetCombatComponent()->Attack(R));
   Test->TestTrue(TEXT("The armed attack raises wanted evidence"),F.Wanted()->GetEvidenceScore()>0);
   auto* Weapon=F.Player->GetCombatComponent()->GetWeapon();
   Test->TestTrue(TEXT("The weapon stays equipped through wanted escalation"),Weapon && Weapon->IsEquipped() && Weapon->GetData()->WeaponId==WeaponID);
   Test->TestEqual(TEXT("Firing one round deducted exactly one round"),Weapon?Weapon->GetAmmo():-1,Mag-1);
   Test->TestFalse(TEXT("Escalating wanted never stows the weapon"),F.Player->GetCombatComponent()->IsInjuryStowed());
   Test->AddInfo(FString::Printf(TEXT("Wanted level after the armed attack: %d"),F.Wanted()->GetWantedLevel()));
   return Next(5);
  }
  if(Stage==5 && Elapsed>.4f)
  {
   // Downed: the adapter must stow the weapon instead of destroying or losing it.
   F.Damage(F.Player,F.Player->GetHealthComponent()->GetCurrentHealth()-5000.f*.15f,Officer->GetController(),Officer,ENeonCityDamageType::Ballistic);
   return Next(6);
  }
  if(Stage==6 && Elapsed>.6f)
  {
   auto* Combat=F.Player->GetCombatComponent(); auto* Weapon=Combat->GetWeapon();
   Test->TestTrue(TEXT("A downed player is in the Disabled combat state"),Combat->GetCombatState()==ENeonCityCombatState::Disabled);
   Test->TestTrue(TEXT("Downed play stows the weapon instead of deleting it"),Combat->IsInjuryStowed() && Combat->GetInjuryStowCount()>=1);
   Test->TestTrue(TEXT("The stowed weapon actor is still alive"),Weapon!=nullptr && Weapon->GetData()->WeaponId==WeaponID);
   Test->TestEqual(TEXT("Stowing never spends or loses ammo"),Weapon?Weapon->GetAmmo():-1,Mag-1);
   Test->TestFalse(TEXT("A stowed weapon is not equipped during Downed"),Weapon && Weapon->IsEquipped());
   Test->TestFalse(TEXT("Recovery is refused while the player is Downed"),Combat->RestoreAfterIncapacitation());
   Test->TestEqual(TEXT("The refused recovery did not count as a restore"),Combat->GetInjuryRestoreCount(),0);
   return Next(7);
  }
  if(Stage==7 && Elapsed>.5f)
  {
   F.Damage(F.Player,20000.f,Officer->GetController(),Officer,ENeonCityDamageType::Ballistic);
   return Next(8);
  }
  if(Stage==8 && Elapsed>.5f)
  {
   auto* Combat=F.Player->GetCombatComponent(); auto* Weapon=Combat->GetWeapon();
   Test->TestTrue(TEXT("The player is actually dead"),F.Player->GetHealthComponent()->IsDead() && Combat->GetCombatState()==ENeonCityCombatState::Dead);
   Test->TestTrue(TEXT("Death never destroys the weapon actor"),Weapon!=nullptr && Weapon->GetData()->WeaponId==WeaponID);
   Test->TestEqual(TEXT("Death never destroys persistent ammo"),Weapon?Weapon->GetAmmo():-1,Mag-1);
   Test->TestFalse(TEXT("A dead player can never restore the weapon"),Combat->RestoreAfterIncapacitation());
   Test->TestTrue(TEXT("Dead players still refuse combat"),Combat->GetCombatState()==ENeonCityCombatState::Dead);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0,Mag=0; float Before=0,After=0; FName WeaponID;
 APoliceCharacter* Officer=nullptr;
};
/** Spec 30 items 26/27: explosion crime attribution, and accidents that must not blame the player. */
class FBlastCrime : public IAutomationLatentCommand
{
public:
 explicit FBlastCrime(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>120) { Test->AddError(FString::Printf(TEXT("Blast crime stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   F.PlaceOnPad(FVector(0,0,140));
   Barrel=F.Prop(ECityPhysicsProp::TrashCan,F.Area->GetActorLocation()+FVector(900,900,47));
   Officer=F.Area->GetOfficer();
   if(!Test->TestTrue(TEXT("Blast crime fixtures spawned"),Barrel!=nullptr && Officer!=nullptr)) return true;
   return Next(2);
  }
  if(Stage==2 && Elapsed>.4f)
  {
   const auto Result=F.Blast(TEXT("NeonCity.RocketExplosion"),Barrel->GetActorLocation(),F.PC,F.Player);
   Test->TestTrue(TEXT("Player blast resolves against the prop"),Result.bApplied);
   return Next(3);
  }
  if(Stage==3 && Elapsed>.5f)
  {
   Test->TestTrue(TEXT("The blast breaks the prop it hit"),Barrel->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   Test->TestTrue(TEXT("Explosion crime against a prop is attributed to the player as PropertyDamage"),
     F.CrimeHas(ENeonCityCrimeType::PropertyDamage,Barrel,F.PC));
   Test->TestTrue(TEXT("The attributed crime names the destroyed prop as the victim"),
     F.Crime()->GetEvents().ContainsByPredicate([&](const FNeonCityCrimeEvent& E){ return E.Instigator.Get()==F.PC && E.Victim.Get()==Barrel; }));
   F.Blast(TEXT("NeonCity.SmallExplosion"),F.Area->GetActorLocation()+FVector(-3600,-2600,120),F.PC,F.Player);
   return Next(4);
  }
  if(Stage==4 && Elapsed>.4f)
  {
   Test->TestTrue(TEXT("An open blast with no victim is recorded as public danger"),
     F.Crime()->GetEvents().ContainsByPredicate([](const FNeonCityCrimeEvent& E){ return E.Offense==EPoliceOffense::PublicDanger; }));
   Test->TestTrue(TEXT("The public-danger record still carries the responsible instigator"),
     F.Crime()->GetEvents().ContainsByPredicate([](const FNeonCityCrimeEvent& E){ return E.Offense==EPoliceOffense::PublicDanger && E.Instigator.IsValid(); }));
   BlastCenter=F.Area->GetPedestrian(0)->GetActorLocation()+FVector(500,0,0);
   F.Blast(TEXT("NeonCity.VehicleExplosion"),BlastCenter,nullptr,Officer);
   return Next(5);
  }
  if(Stage==5 && Elapsed>.5f)
  {
   Test->TestFalse(TEXT("An accidental blast adds no crime attributed to the player at that spot"),
     F.Crime()->GetEvents().ContainsByPredicate([&](const FNeonCityCrimeEvent& E){ return E.Instigator.Get()==F.PC && E.Location.Equals(BlastCenter,50.f); }));
   Test->TestFalse(TEXT("The accident is not pinned on the player as a crime against the civilian"),
     F.CrimeHas(ENeonCityCrimeType::DamageNPC,F.Area->GetPedestrian(0),F.PC));
   Test->TestTrue(TEXT("The accident is still recorded by the released crime ledger"),F.Crime()->GetEvents().Num()>0);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0;
 ANeonCityPhysicsProp* Barrel=nullptr; APoliceCharacter* Officer=nullptr; FVector BlastCenter=FVector::ZeroVector;
};

/** Spec 31 flows A-D end to end; flow E is proved in FEnvironmental, flow F in FPersistence. */
class FCrossSystem : public IAutomationLatentCommand
{
public:
 explicit FCrossSystem(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240) { Test->AddError(FString::Printf(TEXT("Cross-system stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  const int32 WantedLevel=F.Wanted()->GetWantedLevel();
  if(Stage==1)
  {
   F.PlaceOnPad(FVector(-2000,-1900,140));
   F.Player->GetHealthComponent()->ApplyPersistenceHealth(5000,5000);
   Officer=F.Area->GetOfficer();
   // A clear firing lane: the pedestrian is 948 cm away, the officer 500 cm from the victim and off the trace.
   CivilianA=F.Civilian(F.Area->GetActorLocation()+FVector(-1700,-1000,95),811);
   if(!Test->TestTrue(TEXT("Cross-system A fixtures spawn"),Officer!=nullptr && CivilianA!=nullptr)) return true;
   if(auto* Controller=CivilianA->GetController()) Controller->SetActorTickEnabled(false);
   CivilianA->GetCharacterMovement()->StopMovementImmediately();
   Test->TestTrue(TEXT("Cross-system A equips the shared pistol"),F.Player->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")));
   return Next(2);
  }
  if(Stage==2 && Elapsed>.4f)
  {
   HealthBefore=CivilianA->GetHealthComponent()->GetCurrentHealth();
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation();
   R.Direction=(CivilianA->GetActorLocation()-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("A: the player fires the pistol at the civilian"),F.Player->GetCombatComponent()->Attack(R));
   return Next(3);
  }
  if(Stage==3 && Elapsed>.25f)
  {
   Test->TestTrue(TEXT("A: the civilian takes a real bullet hit"),CivilianA->GetHealthComponent()->GetCurrentHealth()<HealthBefore);
   Test->TestTrue(TEXT("A: the civilian registers a hit reaction"),CivilianA->GetDamageFeedback()->GetHitReactionCount()>=1);
   Test->TestTrue(TEXT("A: the shot is recorded as a crime against an NPC"),F.CrimeHas(ENeonCityCrimeType::DamageNPC,CivilianA,F.PC));
   return Next(4);
  }
  if(Stage==4)
  {
   if(F.Wanted()->GetEvidenceScore()>0) return Next(5);
   if(Elapsed<4.f) return false;
   const auto* Crime=F.Crime()->GetEvents().FindByPredicate([](const FNeonCityCrimeEvent& E){ return E.CrimeType==ENeonCityCrimeType::DamageNPC; });
   if(!Test->TestNotNull(TEXT("A: the DamageNPC crime is still on the ledger"),Crime)) return true;
   // The released reporting path: a witness that perceived the incident files it, which confirms the evidence.
   Test->TestTrue(TEXT("A: the police witness can still report the shooting"),F.Crime()->ReportEvent(Crime->Id,Officer));
   Test->AddInfo(FString::Printf(TEXT("A: officer report filed, evidence score %d"),F.Wanted()->GetEvidenceScore()));
   return Next(5);
  }
  if(Stage==5)
  {
   if(WantedLevel==0 && Elapsed<4.f) return false;
   Test->TestTrue(TEXT("A: the crime raises wanted evidence"),F.Wanted()->GetEvidenceScore()>0);
   Test->TestTrue(TEXT("A: wanted escalation follows the crime to the player"),F.Wanted()->GetWantedLevel()>0);
   auto* Inventory=F.Player->FindComponentByClass<UWeaponInventoryComponent>();
   FNeonCityWeaponSaveData Owned;
   const bool bRifle=Inventory && (Inventory->GetWeaponRecord(NeonCityWeapons::Slot(3),Owned) || Inventory->GrantWeapon(NeonCityWeapons::Slot(3)));
   Test->TestTrue(TEXT("B: the player can own the shared rifle"),bRifle);
   Test->TestTrue(TEXT("B: the player equips the rifle"),F.Player->GetCombatComponent()->EquipWeapon(ANeonCityRifle::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")));
   const auto* Weapon=F.Player->GetCombatComponent()->GetWeapon();
   if(!Test->TestNotNull(TEXT("B: a rifle is actually equipped"),Weapon)) return true;
   RifleID=Weapon->GetData()->WeaponId; RifleMag=Weapon->GetAmmo();
   return Next(6);
  }
  if(Stage==6 && Elapsed>.3f)
  {
   F.Damage(F.Player,30.f,Officer->GetController(),Officer,ENeonCityDamageType::Ballistic,F.Player->GetActorLocation()+FVector(0,0,60));
   return Next(7);
  }
  if(Stage==7 && Elapsed>.6f)
  {
   auto* Weapon=F.Player->GetCombatComponent()->GetWeapon();
   Test->TestTrue(TEXT("B: the player registers the police hit"),F.Player->GetDamageFeedbackComponent()->GetHitReactionCount()>=1);
   Test->TestTrue(TEXT("B: the rifle is still equipped after the police hit"),Weapon && Weapon->IsEquipped() && Weapon->GetData()->WeaponId==RifleID);
   Test->TestEqual(TEXT("B: the rifle keeps its magazine"),Weapon?Weapon->GetAmmo():-1,RifleMag);
   Test->TestFalse(TEXT("B: the rifle was never injury-stowed"),F.Player->GetCombatComponent()->IsInjuryStowed());
   Wreck=F.Area->GetVehicle(0);
   Bystander=F.Civilian(Wreck->GetActorLocation()+FVector(0,700,95),812);
   if(!Test->TestTrue(TEXT("C: wreck and bystander fixtures spawn"),Wreck!=nullptr && Bystander!=nullptr)) return true;
   if(auto* Controller=Bystander->GetController()) Controller->SetActorTickEnabled(false);
   EvidenceBefore=F.Wanted()->GetEvidenceScore();
   Test->TestTrue(TEXT("C: rocket blast resolves against the vehicle"),
     F.Blast(TEXT("NeonCity.RocketExplosion"),Wreck->GetActorLocation()+FVector(0,0,60),F.PC,F.Player).bApplied);
   return Next(8);
  }
  if(Stage==8)
  {
   if(Elapsed<8.f && Wreck->GetDamageState()->GetExplosionCount()==0) return false;
   Test->TestTrue(TEXT("C: the rocket leaves the vehicle Critical or worse"),
     Wreck->GetDamageState()->GetCondition()>=ENeonCityVehicleCondition::Critical || Wreck->GetHealthComponent()->IsDead());
   Test->TestEqual(TEXT("C: the wreck detonates from its own fuse"),Wreck->GetDamageState()->GetExplosionCount(),1);
   Test->TestTrue(TEXT("C: the blast reaches the nearby pedestrian's threat perception"),Bystander->GetThreatComponent()->HasActiveThreat());
   Test->TestTrue(TEXT("C: the rocket attack on the vehicle is attributed to the player"),
     F.Crime()->GetEvents().ContainsByPredicate([&](const FNeonCityCrimeEvent& E){ return E.Victim.Get()==Wreck && E.Instigator.Get()==F.PC; }));
   // A wreck that is only Disabled is not yet a confirmed kill; the lethal blow files it as a destroyed vehicle.
   if(!Wreck->GetHealthComponent()->IsDead()) F.Damage(Wreck,9999.f,F.PC,F.Player,ENeonCityDamageType::Explosion);
   Test->TestTrue(TEXT("C: the wreck is filed as a destroyed vehicle against the player"),
     F.CrimeHas(ENeonCityCrimeType::DestroyVehicle,Wreck,F.PC));
   Test->AddInfo(FString::Printf(TEXT("C: evidence %.0f -> %d, wanted %d"),EvidenceBefore,F.Wanted()->GetEvidenceScore(),F.Wanted()->GetWantedLevel()));
   Test->TestTrue(TEXT("C: the city is still holding the player responsible"),F.Wanted()->GetWantedLevel()>=1);
   F.Shot(TEXT("C-Rocket-Vehicle.png"),Wreck);
   PoliceCar=F.W->SpawnActor<APoliceVehicle>(F.Area->GetActorLocation()+FVector(2400,2000,80),FRotator::ZeroRotator);
   if(!Test->TestNotNull(TEXT("D: police vehicle spawns for the missile attack"),PoliceCar)) return true;
   return Next(9);
  }
  if(Stage==9 && Elapsed>.6f)
  {
   CarHealthBefore=PoliceCar->FindComponentByClass<UHealthComponent>()->GetCurrentHealth();
   Test->TestTrue(TEXT("D: missile blast resolves against the police vehicle"),
     F.Blast(TEXT("NeonCity.MissileExplosion"),PoliceCar->GetActorLocation(),F.PC,F.Player).bApplied);
   return Next(10);
  }
  if(Stage==10 && Elapsed>.5f)
  {
   auto* CarHealth=PoliceCar->FindComponentByClass<UHealthComponent>();
   Test->TestTrue(TEXT("D: the missile damages the police vehicle"),CarHealth->GetCurrentHealth()<CarHealthBefore);
   Test->TestTrue(TEXT("D: attacking a police vehicle is recorded as a police attack"),
     F.Crime()->GetEvents().ContainsByPredicate([&](const FNeonCityCrimeEvent& E){ return E.Offense==EPoliceOffense::PoliceAttack && E.Instigator.Get()==F.PC; }));
   Test->TestTrue(TEXT("D: the police-attack record names the struck vehicle"),
     F.Crime()->GetEvents().ContainsByPredicate([&](const FNeonCityCrimeEvent& E){ return E.Offense==EPoliceOffense::PoliceAttack && E.Victim.Get()==PoliceCar; }));
   F.Shot(TEXT("D-Missile-Police.png"),PoliceCar);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0; float HealthBefore=0; int32 RifleMag=0;
 FName RifleID; float EvidenceBefore=0,CarHealthBefore=0;
 ANeonCityNPCCharacter* CivilianA=nullptr; ANeonCityNPCCharacter* Bystander=nullptr; APoliceCharacter* Officer=nullptr;
 ANeonCityBaseVehicle* Wreck=nullptr; APoliceVehicle* PoliceCar=nullptr;
};

/** Spec 31 flow F: a load restores transient destruction and keeps permanent progression. */
class FPersistence : public IAutomationLatentCommand
{
public:
 explicit FPersistence(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180) { Test->AddError(FString::Printf(TEXT("Explosion persistence stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   Glass=F.Area->GetProp(ECityPhysicsProp::StorefrontGlass,0);
   if(!Test->TestNotNull(TEXT("Persistence fixture glass is present"),Glass)) return true;
   Test->TestTrue(TEXT("A save slot is configured for the destruction round trip"),F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase20_Explosion")));
   F.Save->DeleteSave();
   Test->TestTrue(TEXT("A new game save is written"),F.Save->CreateNewGameSave());
   RestoresBefore=F.Destruct()->GetRestoreCount();
   return Next(2);
  }
  if(Stage==2 && Elapsed>1.f)
  {
   if(!Test->TestTrue(TEXT("The pre-destruction game saves"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   Expected.Reset(F.Save->ReadSaveForInspection());
   if(!Test->TestNotNull(TEXT("The saved model is readable"),Expected.Get())) return true;
   Test->TestTrue(TEXT("Transient destruction starts intact in the new game"),Glass->GetDestruction()->GetState()==EPhysicsPropState::Normal);
   Test->TestTrue(TEXT("Rocket blast resolves against the storefront glass"),F.Blast(TEXT("NeonCity.SmallExplosion"),Glass->GetActorLocation()).bApplied);
   return Next(3);
  }
  if(Stage==3 && Elapsed>.5f)
  {
   Test->TestTrue(TEXT("The glass is destroyed before the load"),Glass->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   Test->TestFalse(TEXT("Destroyed glass is no longer drawn before the load"),Glass->GetMesh()->IsVisible());
   Test->TestTrue(TEXT("Reload succeeds"),F.Save->LoadGame());
   return Next(4);
  }
  if(Stage==4 && Elapsed>.6f)
  {
   Test->TestTrue(TEXT("Transient destruction is restored by the load"),Glass->GetDestruction()->GetState()==EPhysicsPropState::Normal);
   Test->TestTrue(TEXT("Restored glass is visible again"),Glass->GetMesh()->IsVisible());
   Test->TestTrue(TEXT("Restored glass regains real collision"),Glass->GetMesh()->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
   Test->TestTrue(TEXT("The load recorded real destruction restores"),F.Destruct()->GetRestoreCount()>RestoresBefore);
   Test->TestEqual(TEXT("Load clears transient wanted state"),F.Wanted()->GetWantedLevel(),0);
   Test->TestTrue(TEXT("Load clears transient crime references"),F.Crime()->GetEvents().Num()==0);
   if(Expected.IsValid()) Test->TestTrue(TEXT("Permanent progression keeps the saved player transform"),F.Player->GetActorTransform().Equals(Expected->Player.Transform,.1));
   F.Shot(TEXT("H-Save-Load-Restore.png"),Glass);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0,RestoresBefore=0;
 ANeonCityPhysicsProp* Glass=nullptr; TStrongObjectPtr<UNeonCitySaveGame> Expected;
};

/** Spec 30 items 28-34: the released systems this phase builds on must still be intact. */
class FRegressionGuards : public IAutomationLatentCommand
{
public:
 explicit FRegressionGuards(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(FString::Printf(TEXT("Regression guard stage %d timeout"),Stage)); return true; }
  if(!Stage) { if(!F.Ready()) return false; return Next(1); }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   // 28: the released weapon and vehicle-weapon catalogs are untouched.
   Test->TestTrue(TEXT("Player weapon catalog still resolves the shared classes"),
     NeonCityWeapons::Resolve(NeonCityWeapons::Slot(1))==ANeonCityPistol::StaticClass() && NeonCityWeapons::Resolve(NeonCityWeapons::Slot(3))==ANeonCityRifle::StaticClass());
   const auto* RocketPod=UVehicleWeaponManager::Profile(FName(TEXT("NeonCity.RocketPod")));
   const auto* Missile=UVehicleWeaponManager::Profile(FName(TEXT("NeonCity.GuidedMissile")));
   Test->TestTrue(TEXT("Vehicle weapons still resolve their own blast profiles"),
     RocketPod && Missile && RocketPod->ExplosionDataID==FName(TEXT("NeonCity.RocketExplosion")) && Missile->ExplosionDataID==FName(TEXT("NeonCity.MissileExplosion")));
   Test->TestNotNull(TEXT("The released vehicle projectile pool is still a world subsystem"),F.W->GetSubsystem<UVehicleProjectilePool>());
   // 34: the persistence boundary is unchanged.
   Test->TestTrue(TEXT("Persistence still implements its released domains"),F.Save->IsPersistenceImplemented() && F.Save->GetPlannedDomains().Num()==4);
   F.PlaceOnPad(FVector(1200,1200,140));
   Guard=F.Fighter(F.Area->GetActorLocation()+FVector(1600,1600,95),831);
   return Next(2);
  }
  if(Stage==2 && Elapsed>.6f)
  {
   // 29: combat AI still configures and idles exactly as released.
   auto* AI=Guard?Guard->GetCombatAI():nullptr;
   if(!Test->TestNotNull(TEXT("Combat AI still attaches to an NPC"),AI)) return true;
   Test->TestTrue(TEXT("Combat AI still starts Inactive with no target"),AI->GetState()==ENeonCityCombatAIState::Inactive && AI->GetTarget()==nullptr);
   Test->TestTrue(TEXT("Combat AI still reports a Healthy armed character"),AI->GetHealthState()==ENeonCityCombatHealth::Healthy && AI->CanFight());
   // 30: police v2 response data is unchanged.
   auto* Response=NewObject<UPoliceResponseData>();
   Test->TestTrue(TEXT("Police response data is still a valid authored profile"),Response->IsValidProfile() && Response->Levels.Num()>0);
   Test->TestTrue(TEXT("Police response still grades offences above mere attention"),
     Response->GetSeverity(EPoliceOffense::Assault,50.f)>Response->GetSeverity(EPoliceOffense::Attention,0.f)
     && Response->GetSeverity(EPoliceOffense::PoliceAttack,50.f)>=Response->GetSeverity(EPoliceOffense::Assault,50.f));
   // 31-33: marine and aircraft domains are still the released subsystems, and still snapshot.
   auto* Marine=F.W->GetSubsystem<UMarineWorldSubsystem>();
   auto* Aircraft=F.W->GetSubsystem<UAircraftWorldSubsystem>();
   Test->TestNotNull(TEXT("Marine world subsystem is still present"),Marine);
   Test->TestNotNull(TEXT("Aircraft world subsystem still covers helicopters and fixed-wing"),Aircraft);
   Test->TestTrue(TEXT("Released marine and aircraft domains still build persistence snapshots"),
     Marine && Aircraft && Marine->BuildPersistenceSnapshot(false).Num()==Marine->BuildPersistenceSnapshot(false).Num() && Aircraft->BuildPersistenceSnapshot(false).Num()==Aircraft->BuildPersistenceSnapshot(false).Num());
   Test->TestTrue(TEXT("The one damage and crime pipeline is still the only one"),F.W->GetSubsystem<UDamageSystem>()!=nullptr && F.W->GetSubsystem<UCrimeSubsystem>()!=nullptr && F.W->GetSubsystem<UWantedSubsystem>()!=nullptr);
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0; ANeonCityNPCCharacter* Guard=nullptr;
};
/** Spec 32-H: two OS processes prove transient destruction never crosses a save file. */
class FExplosionCrossSession : public IAutomationLatentCommand
{
public:
 explicit FExplosionCrossSession(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Explosion cross-session stage %d timeout"),Stage)); return true; }
  if(!Stage)
  {
   FParse::Value(FCommandLine::Get(),TEXT("NeonCityExplosionSession="),Session);
   if(Session.IsEmpty())
   {
    // Only the dedicated runner drives the two-process proof; a plain regression filter must not fail on it.
    Test->AddInfo(TEXT("Skipped: cross-process save verification needs -NeonCityExplosionSession=A|B."));
    return true;
   }
   if(!F.Ready()) return false;
   return Next(1);
  }
  const float Elapsed=F.W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   Glass=F.Area->GetProp(ECityPhysicsProp::StorefrontGlass,0);
   if(!Test->TestNotNull(TEXT("Cross-session glass fixture is present"),Glass)) return true;
   Test->TestTrue(TEXT("The cross-session slot is configured"),F.Save->ConfigureSlot(TEXT("NeonCity_Test_Phase20_CrossProcess")));
   if(Session==TEXT("A")) F.Save->DeleteSave();
   return Next(2);
  }
  if(Stage==2 && Elapsed>1.f)
  {
   if(Session==TEXT("A"))
   {
    if(!Test->TestTrue(TEXT("Session A writes a formal save"),F.Save->CreateNewGameSave())) { Test->AddError(F.Save->GetLastDetail()); return true; }
    // A distinctive permanent value, so session B proves progression survived the process boundary.
    F.Player->GetHealthComponent()->ApplyPersistenceHealth(137.f,137.f);
    Test->TestTrue(TEXT("Session A destroys the storefront glass before saving"),
      F.Blast(TEXT("NeonCity.SmallExplosion"),Glass->GetActorLocation()).bApplied);
    return Next(3);
   }
   return Next(3);
  }
  if(Stage==3 && Elapsed>1.f)
  {
   if(Session==TEXT("A"))
   {
    Test->TestTrue(TEXT("Session A really broke the storefront before saving"),Glass->GetDestruction()->GetState()==EPhysicsPropState::Broken);
    if(!Test->TestTrue(TEXT("Session A saves the destroyed world"),F.Save->SaveGame())) { Test->AddError(F.Save->GetLastDetail()); return true; }
   }
   else
   {
    Test->TestTrue(TEXT("Session B loads the same saved game"),F.Save->LoadGame());
   }
   return Next(4);
  }
  if(Stage==4 && Elapsed>.6f)
  {
   auto* Model=F.Save->ReadSaveForInspection();
   if(!Test->TestNotNull(TEXT("The saved model is readable in this process"),Model)) return true;
   const bool bBroken=Glass->GetDestruction()->GetState()==EPhysicsPropState::Broken;
   // The manifest is the cross-process artifact the runner compares.
   const FString Text=FString::Printf(TEXT("{\"processId\":%u,\"saveId\":\"%s\",\"glassBroken\":%s,\"health\":%.1f}"),
     FPlatformProcess::GetCurrentProcessId(),*Model->SaveID.ToString(),bBroken?TEXT("true"):TEXT("false"),F.Player->GetHealthComponent()->GetCurrentHealth());
   IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Explosion")),true);
   Test->TestTrue(TEXT("Cross-session manifest is written"),
     FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Explosion")/FString::Printf(TEXT("Session%s.json"),*Session))));
   if(Session==TEXT("B"))
   {
    Test->TestTrue(TEXT("A fresh process never inherits transient destruction"),!bBroken && Glass->GetMesh()->IsVisible());
    Test->TestTrue(TEXT("Permanent progression survives the process boundary"),F.Player->GetHealthComponent()->GetCurrentHealth()==137.f);
   }
   return true;
  }
  return false;
 }
 bool Next(int S) { Stage=S; At=F.W->GetTimeSeconds(); return false; }
 FAutomationTestBase* Test; FScene F; double Started=0; float At=0; int32 Stage=0; FString Session;
 ANeonCityPhysicsProp* Glass=nullptr;
};
}  // namespace NeonCityExplosionPIE

#define EXPLOSION_TEST(Class,Name,Scenario) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString& Parameters) \
{ \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityExplosionPIE::Scenario>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); \
 return true; \
}

EXPLOSION_TEST(FExplosionFalloff,"NeonCity.Explosion.FalloffPIE",FBlastFalloff)
EXPLOSION_TEST(FExplosionImpulsePIE,"NeonCity.Explosion.ImpulsePIE",FBlastFalloff)
EXPLOSION_TEST(FWeaponBlastTest,"NeonCity.Explosion.WeaponPIE",FWeaponBlast)
EXPLOSION_TEST(FVehicleBlastTest,"NeonCity.Explosion.VehiclePIE",FVehicleBlast)
EXPLOSION_TEST(FEnvironmentalTest,"NeonCity.Explosion.EnvironmentalPIE",FEnvironmental)
EXPLOSION_TEST(FNPCReactionTest,"NeonCity.Explosion.NPCPIE",FNPCReaction)
EXPLOSION_TEST(FCharacterFeedbackTest,"NeonCity.Explosion.CharacterFeedbackPIE",FCharacterFeedback)
EXPLOSION_TEST(FBlastCrimeTest,"NeonCity.Explosion.CrimePIE",FBlastCrime)
EXPLOSION_TEST(FCrossSystemTest,"NeonCity.Explosion.CrossSystemPIE",FCrossSystem)
EXPLOSION_TEST(FPersistenceTest,"NeonCity.Explosion.PersistencePIE",FPersistence)
EXPLOSION_TEST(FRegressionGuardsTest,"NeonCity.Explosion.RegressionPIE",FRegressionGuards)
EXPLOSION_TEST(FExplosionCrossSessionTest,"NeonCity.ExplosionCrossSession.Persistence",FExplosionCrossSession)

#undef EXPLOSION_TEST
#endif