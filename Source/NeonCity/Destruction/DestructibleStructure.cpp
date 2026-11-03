#include "Destruction/DestructibleStructure.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Combat/DamageSystem.h"
#include "Combat/HealthComponent.h"
#include "Physics/DestructiblePropComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
ADestructibleStructure::ADestructibleStructure()
{
 PrimaryActorTick.bCanEverTick=false;
 Shell=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell")); SetRootComponent(Shell);
 Shell->SetMobility(EComponentMobility::Movable); Shell->SetCollisionProfileName(TEXT("BlockAll"));
 Shell->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Shell->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal")));
 Shell->SetCastShadow(true);
}
void ADestructibleStructure::Configure(FName InStructureID,FVector ShellExtent,bool bGlassStorefront)
{
 StructureID=InStructureID.IsNone()?TEXT("NeonCity.Structure"):InStructureID;
 Shell->SetRelativeScale3D(ShellExtent/100.f);
 if(bGlassStorefront) Shell->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Environment/Materials/M_SB_Coral.M_SB_Coral")));
}
ANeonCityPhysicsProp* ADestructibleStructure::AddPart(ECityPhysicsProp Type,FVector LocalLocation,FRotator LocalRotation,float Health)
{
 if(!GetWorld()) return nullptr;
 // Parts are world actors: released debris must not inherit a destroyed parent transform.
 const FTransform Transform(LocalRotation,GetActorTransform().TransformPosition(LocalLocation));
 auto* Part=GetWorld()->SpawnActorDeferred<ANeonCityPhysicsProp>(ANeonCityPhysicsProp::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Part) return nullptr;
 Part->Configure(Type);
 Part->FinishSpawning(Transform);
 if(Health>0 && Part->GetHealthComponent()) { Part->GetHealthComponent()->SetMaxHealth(Health); Part->GetHealthComponent()->SetCurrentHealth(Health); }
 Parts.Add(Part);
 return Part;
}
int32 ADestructibleStructure::ApplyLocalizedDestruction_Implementation(FVector Center,float Radius,float Damage,AActor* DamageInstigator)
{
 if(Center.ContainsNaN() || !FMath::IsFinite(Radius) || !FMath::IsFinite(Damage) || Radius<=0 || Damage<=0 || !GetWorld()) return 0;
 const float Now=GetWorld()->GetTimeSeconds();
 int32 Broken=0,BrokenTotal=0;
 for(const auto& Entry:Parts)
 {
  ANeonCityPhysicsProp* Part=Entry.Get();
  if(!IsValid(Part) || !Part->GetDestruction() || Part->GetDestruction()->Mode==EPhysicsPropMode::Static) continue;
  UHealthComponent* Health=Part->GetHealthComponent(); if(!Health || Health->IsDead()) { ++BrokenTotal; continue; }
  const FVector Location=Part->GetActorLocation();
  const float Distance=FVector::Distance(Center,Location);
  if(Distance>Radius) continue;
  // Never double-charge a surface the global blast pass already resolved.
  if(Part->GetLastDamageTime()>=Now-.05f) continue;
  const float Falloff=UNeonCityExplosionData::Falloff(Distance,0,Radius,1);
  if(Falloff<=0) continue;
  FNeonCityDamageEvent Event; Event.DamageAmount=Damage*Falloff; Event.DamageType=ENeonCityDamageType::Explosion;
  Event.SourceActor=this; Event.Instigator=Cast<AController>(DamageInstigator); Event.HitLocation=Location;
  if(GetWorld()->GetSubsystem<UDamageSystem>()->ApplyDamage(Part,Event)>0)
  {
   ++LocalizedDamages;
   if(auto* State=Part->GetSurfaceState(); State && State->IsBroken()) { ++Broken; ++LocalizedBreaks; }
  }
 }
 if(Broken>0) return Broken;
 for(const auto& Entry:Parts) if(IsValid(Entry.Get()) && Entry->GetSurfaceState() && Entry->GetSurfaceState()->IsBroken()) ++BrokenTotal;
 return BrokenTotal;
}
int32 ADestructibleStructure::GetIntactSurfaceCount_Implementation() const
{
 int32 Count=0;
 for(const auto& Entry:Parts) if(IsValid(Entry.Get()) && Entry->GetSurfaceState() && !Entry->GetSurfaceState()->IsBroken()) ++Count;
 return Count;
}
bool ADestructibleStructure::ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event)
{
 if(!Event.IsValidEvent()) return false;
 // Blast routing only reaches authored surfaces; the shell and the rest of the city stay stable.
 return ApplyLocalizedDestruction_Implementation(Event.Center,Event.OuterRadius,Event.Damage,Event.Instigator.Get()?Event.Instigator->GetPawn():nullptr)>0;
}
void ADestructibleStructure::EndPlay(EEndPlayReason::Type Reason)
{
 if(Reason==EEndPlayReason::Destroyed) for(const auto& Entry:Parts) if(IsValid(Entry.Get())) Entry->Destroy();
 Parts.Reset();
 Super::EndPlay(Reason);
}