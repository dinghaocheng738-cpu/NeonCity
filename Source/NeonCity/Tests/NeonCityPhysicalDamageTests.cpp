#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/ImpactDamageData.h"
#include "Physics/PhysicalMaterialResponseData.h"
#include "Physics/CharacterImpactComponent.h"
#include "Physics/NeonCityPhysicsProp.h"
#include "Physics/NeonCityPhysicsTestArea.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Vehicles/VehicleData.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCThreatComponent.h"
#include "AI/NPCStateComponent.h"
#include "AI/Police/PoliceResponseDirector.h"
#include "AI/Traffic/NeonCityTrafficRoute.h"
#include "AI/Traffic/NeonCityTrafficVehicle.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Crime/CrimeSubsystem.h"
#include "Wanted/WantedSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactModel,"NeonCity.Physics.ModelInvariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImpactModel::RunTest(const FString& Parameters)
{
 auto* D=NewObject<UImpactDamageData>(); TestTrue(TEXT("Default impact curves valid"),D->IsValidProfile());
 TestEqual(TEXT("Parking/resting contact harmless"),D->Damage(100,1450,80,EImpactTargetKind::Character),0.f);
 const float Low=D->Damage(350,1450,80,EImpactTargetKind::Character),High=D->Damage(1500,1450,80,EImpactTargetKind::Character);
 TestTrue(TEXT("Nonlinear speed-dependent injury"),Low>0 && Low<10 && High>Low*5);
 TestTrue(TEXT("Heavier sources change injury"),D->Damage(700,2200,80,EImpactTargetKind::Character)>D->Damage(700,1200,80,EImpactTargetKind::Character));
 TestTrue(TEXT("Lighter receiving vehicle suffers more energy per mass"),D->Damage(1800,2200,1200,EImpactTargetKind::Vehicle)>D->Damage(1800,1200,2200,EImpactTargetKind::Vehicle));
 TestTrue(TEXT("Glancing normal component reduces damage"),D->Damage(300,1450,0,EImpactTargetKind::World)<D->Damage(1600,1450,0,EImpactTargetKind::World));
 TestEqual(TEXT("Invalid input is harmless"),D->Damage(std::numeric_limits<float>::quiet_NaN(),1450,80,EImpactTargetKind::Character),0.f);
 TestTrue(TEXT("All four severity bands"),D->Severity(300)==EImpactSeverity::Low && D->Severity(700)==EImpactSeverity::Medium && D->Severity(1400)==EImpactSeverity::High && D->Severity(2600)==EImpactSeverity::Extreme);
 auto* Invalid=NewObject<UImpactDamageData>(); Invalid->CharacterDamage.GetRichCurve()->UpdateOrAddKey(0,-.5f); TestFalse(TEXT("Negative curve keys rejected"),Invalid->IsValidProfile());
 D->PairCooldown=0; TestFalse(TEXT("Contact spam configuration rejected"),D->IsValidProfile());
 auto* M=NewObject<UPhysicalMaterialResponseData>(); FPhysicalSurfaceResponse R; R.Restitution=4; M->Surfaces.Add(SurfaceType1,R);
 TestTrue(TEXT("Surface response prevents elastic-ball restitution"),M->Get(SurfaceType1).Restitution<=.15f); return true;
}
namespace NeonCityPhysicsTests
{
void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityPhysicsCapture"))) return;
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Phase15")),true);
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Phase15")/Name,true,false);
}
struct FScene
{
 UWorld* W=nullptr; APlayerController* PC=nullptr; ANeonCityPlayerCharacter* Player=nullptr; TArray<ANeonCityBaseVehicle*> Cars;
 bool Ready()
 {
  W=GEditor?GEditor->PlayWorld:nullptr; PC=W?W->GetFirstPlayerController():nullptr; Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || !W->HasBegunPlay() || !Player->GetCharacterMovement()->IsMovingOnGround() || W->GetTimeSeconds()<1) return false;
  Cars.Reset(); for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It) if(!It->ActorHasTag(TEXT("NeonCity.Adventure.Offroad")))Cars.Add(*It);
  Cars.Sort([](const auto& A,const auto& B){ return uint8(A.GetVehicleProfile()->VehicleType)<uint8(B.GetVehicleProfile()->VehicleType); });
  // Isolate the small test lane; authored traffic still exists and is covered in the dedicated traffic test/regression.
  for(TActorIterator<ANeonCityTrafficVehicle> It(W);It;++It) { It->SetActorTickEnabled(false); It->SetActorEnableCollision(false); }
  return Cars.Num()==4;
 }
 void PlacePlayer(FVector P) { Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->StopMovementImmediately(); }
 static void Stop(ANeonCityBaseVehicle* Car) { Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector); Car->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector); Car->GetDrivingComponent()->SetDrivingEnabled(false); }
 void PlaceCar(ANeonCityBaseVehicle* Car,FVector P)
 { Stop(Car); Car->GetChaosMovement()->ResetVehicle(); Car->SetActorLocationAndRotation(P,FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics); Car->GetHealthComponent()->ApplyPersistenceHealth(Car->GetVehicleProfile()->Durability,Car->GetVehicleProfile()->Durability); Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(.5f); }
 void Launch(ANeonCityBaseVehicle* Car,float Speed) { Car->GetDrivingComponent()->SetDrivingEnabled(true); Car->GetMesh()->SetPhysicsLinearVelocity(FVector(Speed,0,0)); Car->GetMesh()->WakeAllRigidBodies(); }
 ANeonCityNPCCharacter* NPC(FVector P,bool Witness=false)
 {
  const FTransform T(P); auto* N=W->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),T);
  N->InitializePedestrian(Witness?TEXT("Physics.Witness"):TEXT("Physics.Target"),nullptr,Witness?TArray<FVector>{P+FVector(500,0,0),P+FVector(1000,0,0)}:TArray<FVector>{},15); N->FinishSpawning(T);
  if(!Witness && N->GetController()) N->GetController()->SetActorTickEnabled(false); return N;
 }
 ANeonCityPhysicsProp* Prop(ECityPhysicsProp Kind,FVector P)
 { const FTransform T(P); auto* A=W->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),T); A->Configure(Kind); A->FinishSpawning(T); return A; }
 const FPhysicalImpactEvent* Last(AActor* Target) const
 { const auto& E=W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents(); for(int32 I=E.Num()-1;I>=0;--I) if(E[I].TargetActor.Get()==Target) return &E[I]; return nullptr; }
};
class FCharacters : public IAutomationLatentCommand
{
public:
 explicit FCharacters(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>65) { Test->AddError(FString::Printf(TEXT("Character collision timeout case=%d stage=%d"),Case,Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   Witness=F.NPC(FVector(-2600,350,102),true); WitnessOrigin=Witness->GetActorLocation(); Stage=1;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1)
  {
   Car=F.Cars[Case==0?0:1]; F.PlaceCar(Car,FVector(-3050,0,150)); F.PlacePlayer(FVector(-3050,-220,120));
   Victim=F.NPC(FVector(-2600,0,112)); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>1)
  {
   if(!Test->TestTrue(TEXT("Player enters real collision vehicle"),Car->GetVehicleInteraction()->Enter(F.Player))) return true;
   F.Launch(Car,Case==0?350:Case==1?1450:2700); Phase=Now; Stage=3; return false;
  }
  if(Stage==3)
  {
   const auto* Event=F.Last(Victim);
   if(!Event && Now-Phase<4) return false;
   if(!Test->TestNotNull(TEXT("Actual Chaos contact dispatched to character"),Event)) { Test->AddInfo(Car->GetActorLocation().ToString()); return true; }
   const float Damage=100-Victim->GetHealthComponent()->GetCurrentHealth(); auto* Reaction=Victim->FindComponentByClass<UCharacterImpactComponent>();
   Test->AddInfo(FString::Printf(TEXT("Case %d actual speed=%.1f mass=%.1f damage=%.1f state=%d at=%s"),Case,Event->ImpactSpeed,Event->SourceMass,Damage,int32(Reaction->GetState()),*Victim->GetActorLocation().ToString()));
   Test->TestTrue(TEXT("Contact carries driver, finite direction and timestamp"),Event->Instigator.Get()==F.PC && !Event->ImpactDirection.ContainsNaN() && Event->Timestamp>0);
   const auto* Crime=F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().FindByPredicate([&](const auto& E){ return E.Victim.Get()==Victim && E.CrimeType==ENeonCityCrimeType::DamageNPC; });
   Test->TestTrue(TEXT("Driver impact reaches Crime ledger"),Crime && Crime->Instigator.Get()==F.PC);
   if(Case==0) { LowDamage=Damage; Test->TestTrue(TEXT("Low speed small injury and Stagger"),Damage>0 && Damage<15 && Reaction->GetState()==ECharacterImpactState::Stagger); Shot(TEXT("A-Sedan-Low.png")); }
   if(Case==1) { Test->TestTrue(TEXT("Sports high impact exceeds low damage and forces Downed"),Damage>LowDamage*3 && Reaction->GetState()==ECharacterImpactState::Downed && !Victim->GetHealthComponent()->IsDead()); Test->TestTrue(TEXT("High impact starts actual bounded skeletal physics"),Reaction->GetPhysicsActivationCount()>0); Shot(TEXT("B-Sports-High.png")); }
   if(Case==2) { Test->TestTrue(TEXT("Lethal impact marks Dead without destroying NPC"),Reaction->GetState()==ECharacterImpactState::Dead && Victim->GetHealthComponent()->IsDead() && !Victim->IsActorBeingDestroyed()); }
   F.Stop(Car); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>2)
  {
   auto* Reaction=Victim->FindComponentByClass<UCharacterImpactComponent>();
   Test->TestFalse(TEXT("Physics feedback stops; no indefinite ragdoll"),Reaction->IsPhysicsFeedbackActive());
   Test->TestTrue(TEXT("Character remains inside test area and above ground"),FVector::Dist2D(Victim->GetActorLocation(),FVector(-2600,0,112))<250 && Victim->GetActorLocation().Z>85 && Victim->GetActorLocation().Z<150);
   Test->TestTrue(TEXT("Vehicle speed remains bounded"),Car->GetVelocity().Size()<6001);
   if(Case==0) Test->TestTrue(TEXT("Light impact restores character action"),Reaction->GetState()==ECharacterImpactState::Normal && Victim->FindComponentByClass<UDamageableCharacterComponent>()->CanAct());
   if(Case==1) Test->TestTrue(TEXT("Witness flees serious collision and keeps danger memory"),Witness->GetThreatComponent()->HasActiveThreat() && FVector::Dist2D(WitnessOrigin,Witness->GetActorLocation())>100);
   Car->GetVehicleInteraction()->RestoreDriver(true); Victim->Destroy(); F.PlaceCar(Car,FVector(-4000,-1200,150));
   if(++Case<3) { Stage=1; return false; }
   Witness->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; ANeonCityBaseVehicle* Car=nullptr; ANeonCityNPCCharacter *Victim=nullptr,*Witness=nullptr;
 double Started=0; float Phase=0,LowDamage=0; int32 Stage=0,Case=0; FVector WitnessOrigin;
};
class FVehicleContacts : public IAutomationLatentCommand
{
public:
 explicit FVehicleContacts(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>50) { Test->AddError(FString::Printf(TEXT("Vehicle contacts timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; A=F.Cars[1]; B=F.Cars[2]; F.PlacePlayer(FVector(-3600,-220,120));
   F.PlaceCar(A,FVector(-3600,0,150)); F.PlaceCar(B,FVector(-2850,0,150)); Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>1)
  { Test->TestTrue(TEXT("Player drives Sports collision fixture"),A->GetVehicleInteraction()->Enter(F.Player)); F.Launch(A,1800); Phase=Now; Stage=2; return false; }
  if(Stage==2)
  {
   const float DA=A->GetHealthComponent()->GetMaxHealth()-A->GetHealthComponent()->GetCurrentHealth(),DB=B->GetHealthComponent()->GetMaxHealth()-B->GetHealthComponent()->GetCurrentHealth();
   if((DA<=0 || DB<=0) && Now-Phase<4) return false;
   Test->TestTrue(TEXT("Vehicle vs vehicle damages both"),DA>0 && DB>0);
   Test->TestTrue(TEXT("Lighter Sports receives more damage than SUV"),DA>DB);
   Test->TestTrue(TEXT("Chaos collision physically moves stationary SUV"),B->GetVelocity().Size()>5 || FVector::Dist2D(B->GetActorLocation(),FVector(-2850,0,0))>5);
   Test->TestTrue(TEXT("No extreme vehicle launch"),A->GetVelocity().Size()<6001 && B->GetVelocity().Size()<6001);
   Test->AddInfo(FString::Printf(TEXT("Sports/SUV damage %.2f/%.2f masses %.0f/%.0f speeds %.0f/%.0f"),DA,DB,A->GetVehicleProfile()->Weight,B->GetVehicleProfile()->Weight,A->GetVelocity().Size(),B->GetVelocity().Size()));
   const auto& Events=F.W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents();
   Test->TestEqual(TEXT("Reciprocal body callbacks do not double-charge collision"),Events.FilterByPredicate([&](const auto& E){ return (E.SourceActor.Get()==A && E.TargetActor.Get()==B) || (E.SourceActor.Get()==B && E.TargetActor.Get()==A); }).Num(),2);
   Shot(TEXT("C-Sports-SUV.png")); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>.7f)
  {
   F.Stop(A); F.Stop(B); A->GetVehicleInteraction()->RestoreDriver(true); F.PlaceCar(B,FVector(-4500,-1200,150));
   F.PlaceCar(A,FVector(-3600,0,150)); F.PlacePlayer(FVector(-3600,-220,120)); Wall=F.Prop(ECityPhysicsProp::StaticBlock,FVector(-2800,0,120)); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>1)
  { Test->TestTrue(TEXT("Reenter wall scenario"),A->GetVehicleInteraction()->Enter(F.Player)); F.Launch(A,1800); Phase=Now; Stage=5; return false; }
  if(Stage==5)
  {
   if(!F.Last(Wall) && Now-Phase<4) return false;
   Test->TestTrue(TEXT("Wall collision reduces shared vehicle health"),A->GetHealthComponent()->GetCurrentHealth()<A->GetHealthComponent()->GetMaxHealth());
   Test->TestTrue(TEXT("Static structure retains geometry and state"),Wall->GetDestruction()->GetState()==EPhysicsPropState::Normal && !Wall->GetMesh()->IsSimulatingPhysics());
   Test->TestTrue(TEXT("Vehicle slowed by physical wall response"),FMath::Abs(A->GetVelocity().X)<1200);
   F.Stop(A); A->GetHealthComponent()->SetCurrentHealth(A->GetHealthComponent()->GetMaxHealth()*.08f); F.Launch(A,0); A->GetDrivingComponent()->SetDriveInput(1); Origin=A->GetActorLocation(); Phase=Now; Stage=6; return false;
  }
  if(Stage==6 && Now-Phase>.6f)
  {
   Test->TestTrue(TEXT("Disabled is distinct from Destroyed"),A->GetDamageState()->GetCondition()==ENeonCityVehicleCondition::Disabled && !A->GetHealthComponent()->IsDead());
   Test->TestTrue(TEXT("Disabled engine stalls and throttle is ineffective"),A->GetDrivingComponent()->GetEngineState()==ENeonCityEngineState::Stalled && A->GetDamageState()->GetDrivePowerMultiplier()==0 && A->GetChaosMovement()->GetThrottleInput()==0 && FVector::Dist2D(Origin,A->GetActorLocation())<30);
   A->GetVehicleInteraction()->RestoreDriver(true); Wall->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; ANeonCityBaseVehicle *A=nullptr,*B=nullptr; ANeonCityPhysicsProp* Wall=nullptr;
 double Started=0; float Phase=0; int32 Stage=0; FVector Origin;
};
class FProps : public IAutomationLatentCommand
{
public:
 explicit FProps(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>70) { Test->AddError(FString::Printf(TEXT("Props timeout case=%d stage=%d"),Case,Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false;
   // Repeated public-property tests otherwise summon a police officer into this measured lane.
   // Keep Crime/Threat/Wanted active; the separate police and witness tests exercise response actors.
   for(TActorIterator<APoliceResponseDirector> It(F.W);It;++It) It->Destroy();
   Car=F.Cars[1]; Stage=1;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1)
  {
   const ECityPhysicsProp Kinds[]={ECityPhysicsProp::LampPost,ECityPhysicsProp::TrashCan,ECityPhysicsProp::RoadBarrier,ECityPhysicsProp::RoadCone,ECityPhysicsProp::Sign,ECityPhysicsProp::Fence,ECityPhysicsProp::Glass};
   const float Heights[]={160,47.5f,37.5f,32.5f,95,55,75};
   F.PlaceCar(Car,FVector(-3500,0,150)); F.PlacePlayer(FVector(-3500,-220,120)); Prop=F.Prop(Kinds[Case],FVector(-2850,0,22+Heights[Case])); Origin=Prop->GetActorLocation(); Phase=Now; Stage=2; return false;
  }
  if(Stage==2 && Now-Phase>1)
  { Test->TestTrue(TEXT("Enter prop test vehicle"),Car->GetVehicleInteraction()->Enter(F.Player)); Test->AddInfo(FString::Printf(TEXT("Launch case %d pos %s rot %s"),Case,*Car->GetActorLocation().ToString(),*Car->GetActorRotation().ToString())); F.Launch(Car,Case==1 || Case==2 || Case==3?900:1600); Phase=Now; Stage=3; return false; }
  if(Stage==3)
  {
   if(!F.Last(Prop) && Now-Phase<4) return false;
   Test->AddInfo(FString::Printf(TEXT("Prop case=%d event=%d position=%s velocity=%.1f"),Case,F.Last(Prop)!=nullptr,*Prop->GetActorLocation().ToString(),Prop->GetVelocity().Size()));
   Test->AddInfo(FString::Printf(TEXT("Car pos=%s vel=%s health=%.1f"),*Car->GetActorLocation().ToString(),*Car->GetVelocity().ToString(),Car->GetHealthComponent()->GetCurrentHealth()));
   if(!F.Last(Prop)) for(const auto& E:F.W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents()) Test->AddInfo(FString::Printf(TEXT("Contact %s -> %s speed %.1f time %.1f"),*GetNameSafe(E.SourceActor.Get()),*GetNameSafe(E.TargetActor.Get()),E.ImpactSpeed,E.Timestamp));
   Test->TestNotNull(TEXT("Prop receives unified collision event"),F.Last(Prop));
   if(const auto* E=F.Last(Prop)) Test->TestEqual(TEXT("Contact preserves actual surface identity"),int32(E->SurfaceType),Case==6?3:Case>=1 && Case<=3?2:1); Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>.3f)
  {
   if(Case!=6) Test->TestTrue(TEXT("Small prop has actual physical response"),Prop->GetMesh()->IsSimulatingPhysics() && (FVector::Dist(Origin,Prop->GetActorLocation())>2 || Prop->GetMesh()->GetPhysicsLinearVelocity().Size()>10));
   Test->TestTrue(TEXT("Prop velocity bounded"),Prop->GetMesh()->GetPhysicsLinearVelocity().Size()<650);
   if(Case==0) { Test->TestTrue(TEXT("Street lamp Broken and light off"),Prop->GetDestruction()->GetState()==EPhysicsPropState::Broken && !Prop->GetLight()->IsVisible()); Shot(TEXT("D-Lamp-Broken.png")); }
   if(Case==1) Shot(TEXT("D-Trash-Physics.png"));
   if(Case>=4) Test->TestTrue(TEXT("Sign/fence/glass use lightweight broken state"),Prop->GetDestruction()->GetState()==EPhysicsPropState::Broken);
   if(Case==6) Test->TestTrue(TEXT("Vehicle breaks glass and removes collision"),Prop->GetMesh()->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   F.Stop(Car); Phase=Now; Stage=5; return false;
  }
  if(Stage==5 && Now-Phase>.5f)
  {
   Test->TestTrue(TEXT("Exit prop scenario"),Car->GetVehicleInteraction()->RestoreDriver(true)); Prop->Destroy();
   if(++Case<7) { Stage=1; return false; }
   F.PlaceCar(Car,FVector(-4600,-1200,150)); F.PlacePlayer(FVector(-3400,0,120)); Prop=F.Prop(ECityPhysicsProp::Glass,FVector(-2800,0,100));
   F.Player->GetCombatComponent()->EquipWeapon(ANeonCityPistol::StaticClass(),F.Player->GetMesh(),TEXT("hand_r")); Phase=Now; Stage=6; return false;
  }
  if(Stage==6 && Now-Phase>.35f)
  {
   FCombatAttackRequest R; R.ViewOrigin=F.Player->GetPawnViewLocation(); R.Direction=(Prop->GetActorLocation()-R.ViewOrigin).GetSafeNormal(); R.Instigator=F.PC;
   Test->TestTrue(TEXT("Shared pistol fires at glass"),F.Player->GetCombatComponent()->Attack(R)); Phase=Now;
   if(++GlassShots<3) return false;
   Test->TestTrue(TEXT("Glass breaks from real weapon hits and removes collision"),Prop->GetDestruction()->GetState()==EPhysicsPropState::Broken && Prop->GetMesh()->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Test->TestTrue(TEXT("Property damage reaches Crime extension"),F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().ContainsByPredicate([&](const auto& E){ return E.Victim.Get()==Prop && E.CrimeType==ENeonCityCrimeType::PropertyDamage; }));
   Prop->GetDestruction()->ResetForPool();
   Test->TestTrue(TEXT("Pooling restores glass without adding an unintended light"),Prop->GetDestruction()->GetState()==EPhysicsPropState::Normal && Prop->GetMesh()->IsVisible() && Prop->GetMesh()->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics && !Prop->GetLight()->IsVisible());
   Prop->Destroy(); return true;
  }
  return false;
 }
 FAutomationTestBase* Test; FScene F; ANeonCityBaseVehicle* Car=nullptr; ANeonCityPhysicsProp* Prop=nullptr;
 double Started=0; float Phase=0; int32 Stage=0,Case=0,GlassShots=0; FVector Origin;
};
class FTraffic : public IAutomationLatentCommand
{
public:
 explicit FTraffic(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>35) { Test->AddError(FString::Printf(TEXT("Traffic impact timeout %d"),Stage)); return true; }
  if(Stage==0)
  {
   if(!F.Ready()) return false; F.PlacePlayer(FVector(-3400,800,120)); MeshRelative=F.Player->GetMesh()->GetRelativeTransform();
   const FTransform T(FVector::ZeroVector); Route=F.W->SpawnActorDeferred<ANeonCityTrafficRoute>(ANeonCityTrafficRoute::StaticClass(),T); Route->VehicleCount=0;
   auto* S=Route->GetRoadSpline(); S->ClearSplinePoints(); for(FVector P:{FVector(-5000,0,83),FVector(-1200,0,83),FVector(-1200,-1200,83),FVector(-5000,-1200,83)}) S->AddSplinePoint(P,ESplineCoordinateSpace::World,false);
   for(int32 I=0;I<4;++I) S->SetSplinePointType(I,ESplinePointType::Linear,false); S->SetClosedLoop(true); S->UpdateSpline(); Route->FinishSpawning(T);
   Traffic=F.W->SpawnActor<ANeonCityTrafficVehicle>(FVector(-4600,0,83),FRotator::ZeroRotator); Traffic->InitializeRoute(Route,400); Traffic->CruiseSpeed=800;
   Phase=F.W->GetTimeSeconds(); Stage=1; return false;
  }
  const float Now=F.W->GetTimeSeconds();
  if(Stage==1 && Now-Phase>1)
  {
   F.W->GetSubsystem<UWantedSubsystem>()->ResetForLoad();
   // Sudden close entry, inside the normal look-ahead stopping margin: exercise the actual swept movement contact.
   FVector P=Traffic->GetActorLocation()+Traffic->GetActorForwardVector()*295; P.Z=118; F.PlacePlayer(P); Origin=P; Phase=Now; Stage=2; return false;
  }
  if(Stage==2)
  {
   const auto* E=F.Last(F.Player); if(!E && Now-Phase<3) return false;
   if(!Test->TestNotNull(TEXT("Ambient vehicle actually contacts player"),E)) return true;
   auto* Reaction=F.Player->FindComponentByClass<UCharacterImpactComponent>();
   Test->TestTrue(TEXT("Player injured and knocked down by ambient traffic"),F.Player->GetHealthComponent()->GetCurrentHealth()<100 && Reaction->GetState()==ECharacterImpactState::Knockdown);
   Test->TestFalse(TEXT("Knockdown disables combat/action"),F.Player->FindComponentByClass<UDamageableCharacterComponent>()->CanAct());
   Test->TestTrue(TEXT("Original movement temporarily disabled without rewriting it"),F.Player->GetCharacterMovement()->MovementMode==MOVE_None);
   Test->TestFalse(TEXT("Ambient impact never borrows local player as driver"),E->Instigator.IsValid());
   Test->TestEqual(TEXT("Non-player accident does not create player Wanted"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),0);
   Test->AddInfo(FString::Printf(TEXT("Ambient speed %.1f damage %.1f, player reaction %d"),E->ImpactSpeed,E->AppliedDamage,int32(Reaction->GetState())));
   Shot(TEXT("E-Player-Knockdown.png")); Phase=Now; Stage=3; return false;
  }
  if(Stage==3 && Now-Phase>2.3f)
  {
   auto* Reaction=F.Player->FindComponentByClass<UCharacterImpactComponent>();
   Test->TestTrue(TEXT("Knockdown passes Recover and returns control"),Reaction->GetState()==ECharacterImpactState::Normal && (Reaction->GetVisitedStates()&(1u<<uint8(ECharacterImpactState::Recover))) && F.Player->FindComponentByClass<UDamageableCharacterComponent>()->CanAct() && F.Player->GetCharacterMovement()->IsMovingOnGround());
   Test->TestTrue(TEXT("Mesh/capsule alignment restored"),F.Player->GetMesh()->GetRelativeTransform().Equals(MeshRelative,.1f));
   Test->TestTrue(TEXT("No launch or floor penetration"),FVector::Dist2D(Origin,F.Player->GetActorLocation())<250 && F.Player->GetActorLocation().Z>90 && F.Player->GetVelocity().Size()<100);
   auto* Save=F.W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>(); Save->ConfigureSlot(TEXT("NeonCity_Test_Phase15")); Save->DeleteSave();
   Test->TestTrue(TEXT("New save after physical reaction"),Save->CreateNewGameSave()); Test->TestTrue(TEXT("Load clears transient collision state"),Save->LoadGame()); Save->DeleteSave();
   Traffic->Destroy(); Route->Destroy();
   const FTransform T(FVector::ZeroVector); Route=F.W->SpawnActorDeferred<ANeonCityTrafficRoute>(ANeonCityTrafficRoute::StaticClass(),T); Route->VehicleCount=0;
   auto* S=Route->GetRoadSpline(); S->ClearSplinePoints(); for(FVector P:{FVector(-5000,0,83),FVector(-1200,0,83),FVector(-1200,-1200,83),FVector(-5000,-1200,83)}) S->AddSplinePoint(P,ESplineCoordinateSpace::World,false);
   for(int32 I=0;I<4;++I) S->SetSplinePointType(I,ESplinePointType::Linear,false); S->SetClosedLoop(true); S->UpdateSpline(); Route->FinishSpawning(T);
   F.PlacePlayer(FVector(-3400,1200,120)); Traffic=F.W->SpawnActor<ANeonCityTrafficVehicle>(FVector(-4600,0,83),FRotator::ZeroRotator); Traffic->InitializeRoute(Route,400); Traffic->CruiseSpeed=800;
   Phase=Now; Stage=4; return false;
  }
  if(Stage==4 && Now-Phase>1)
  { FVector P=Traffic->GetActorLocation()+Traffic->GetActorForwardVector()*295; P.Z=118; Victim=F.NPC(P); Phase=Now; Stage=5; return false; }
  if(Stage==5)
  {
   const auto* E=F.Last(Victim); if(!E && Now-Phase<3) return false;
   Test->TestTrue(TEXT("Ambient traffic injures NPC without player attribution"),E && E->AppliedDamage>0 && !E->Instigator.IsValid());
   const auto* Crime=F.W->GetSubsystem<UCrimeSubsystem>()->GetEvents().FindByPredicate([&](const auto& C){ return C.Victim.Get()==Victim && C.CrimeType==ENeonCityCrimeType::DamageNPC; });
   Test->TestTrue(TEXT("NPC traffic accident records non-player Crime"),Crime && !Crime->Instigator.IsValid());
   Test->TestEqual(TEXT("Ambient NPC accident leaves player Wanted zero"),F.W->GetSubsystem<UWantedSubsystem>()->GetWantedLevel(),0);
   Traffic->Destroy(); Route->Destroy(); Victim->Destroy(); return true;
  }
  return false;
 }
 ANeonCityNPCCharacter* Victim=nullptr;
 FAutomationTestBase* Test; FScene F; ANeonCityTrafficRoute* Route=nullptr; ANeonCityTrafficVehicle* Traffic=nullptr;
 double Started=0; float Phase=0; int32 Stage=0; FVector Origin; FTransform MeshRelative;
};
class FArea : public IAutomationLatentCommand
{
public:
 explicit FArea(FAutomationTestBase* T):Test(T) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Test area startup timeout")); return true; }
  if(!Area) { if(!F.Ready()) return false; Area=F.W->SpawnActor<ANeonCityPhysicsTestArea>(); Phase=F.W->GetTimeSeconds(); return false; }
  if(!Captured && F.W->GetTimeSeconds()-Phase<2) return false;
  if(Captured)
  {
   if(F.W->GetTimeSeconds()-Phase<.4f) return false;
   Area->Destroy(); int32 Remaining=0; for(TActorIterator<ANeonCityPhysicsProp> It(F.W);It;++It) ++Remaining;
   Test->TestEqual(TEXT("Removing area cleans props"),Remaining,0); return true;
  }
  int32 Props=0,Cars=0; for(TActorIterator<ANeonCityPhysicsProp> It(F.W);It;++It) { ++Props; Test->TestTrue(TEXT("Test prop stays above floor"),It->GetActorLocation().Z>0); }
  for(TActorIterator<ANeonCityBaseVehicle> It(F.W);It;++It) ++Cars;
  Test->TestEqual(TEXT("Small area contains seven prop types"),Props,7); Test->TestEqual(TEXT("Two additional drivable area vehicles"),Cars,6);
  F.PlacePlayer(FVector(-6050,1750,100)); F.PC->SetControlRotation(FRotator(-8,90,0)); Shot(TEXT("Physics-Test-Area.png"));
  Captured=true; Phase=F.W->GetTimeSeconds(); return false;
 }
 FAutomationTestBase* Test; FScene F; ANeonCityPhysicsTestArea* Area=nullptr; bool Captured=false; double Started=0; float Phase=0;
};
}
#define PHYSICS_PIE(Class,Name,Command) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString& Parameters) { \
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test"))); \
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); \
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityPhysicsTests::Command>(this)); \
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
PHYSICS_PIE(FPhysicalCharacters,"NeonCity.Physics.CharacterImpactsPIE",FCharacters)
PHYSICS_PIE(FPhysicalVehicles,"NeonCity.Physics.VehicleContactsPIE",FVehicleContacts)
PHYSICS_PIE(FPhysicalProps,"NeonCity.Physics.PropsPIE",FProps)
PHYSICS_PIE(FPhysicalTraffic,"NeonCity.Physics.TrafficPlayerPIE",FTraffic)
PHYSICS_PIE(FPhysicalArea,"NeonCity.Physics.TestAreaPIE",FArea)
#undef PHYSICS_PIE
#endif
