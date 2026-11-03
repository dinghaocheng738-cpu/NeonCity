#include "AI/Combat/NeonCityCombatEncounter.h"
#include "AI/Combat/NeonCityCombatAIComponent.h"
#include "AI/Combat/CombatAIData.h"
#include "AI/Combat/CoverPoint.h"
#include "AI/NeonCityNPCCharacter.h"
#include "AI/NPCData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
ANeonCityCombatEncounter::ANeonCityCombatEncounter()
{ PrimaryActorTick.bCanEverTick=false; SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Encounter"))); }
void ANeonCityCombatEncounter::BeginPlay()
{
 Super::BeginPlay();
 if(!HasAuthority() || UGameplayStatics::GetCurrentLevelName(GetWorld(),true)!=TEXT("NeonCity_SouthBeach_Test")) return;
 auto* Profile=NewObject<UCombatAIData>(this); Profile->CanFight=true; Profile->Role=ENeonCityCombatRole::Hostile;
 Profile->Faction=ENeonCityFaction::Hostile; Profile->Accuracy=.45f; Profile->AimError=6; Profile->ReactionTime=1;
 Profile->SearchSeconds=7; Profile->ChaseLeash=2000; Profile->MaximumCombatSeconds=60;
 auto* NPCProfile=NewObject<UNPCData>(this); NPCProfile->CombatProfile=Profile;
 for(int32 I=0;I<2;++I)
 {
  const float X=I==0?-5950.f:-5660.f;
  auto* Prop=GetWorld()->SpawnActor<AActor>(); Props.Add(Prop);
  auto* Mesh=NewObject<UStaticMeshComponent>(Prop); Prop->SetRootComponent(Mesh); Prop->AddInstanceComponent(Mesh);
  Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal")));
  Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->RegisterComponent();
  const float Height=I==0?200.f:110.f;
  Prop->SetActorLocation(FVector(X,2650,Height*.5f)); Prop->SetActorScale3D(FVector(2,.6,Height/100));
  auto* Point=GetWorld()->SpawnActor<ACoverPoint>(FVector(X,2760,92),FRotator(0,-90,0));
  Point->Stance=I==0?ENeonCityCoverStance::Standing:ENeonCityCoverStance::Crouched; Point->PeekDistance=180; Covers.Add(Point);
  const FTransform Spawn(FRotator(0,-90,0),FVector(I==0?-6250:-5470,3100,94));
  auto* NPC=GetWorld()->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(NPC) { NPC->InitializePedestrian(FName(*FString::Printf(TEXT("CombatTest.Hostile.%d"),I)),NPCProfile,{},174+I); NPC->FinishSpawning(Spawn); Enemies.Add(NPC); }
 }
}
void ANeonCityCombatEncounter::EndPlay(EEndPlayReason::Type Reason)
{
 // Explicitly removing the test encounter cleans its temporary fixtures; health/death never destroys NPCs.
 if(Reason==EEndPlayReason::Destroyed)
 { for(auto NPC:Enemies) if(IsValid(NPC)) NPC->Destroy(); for(auto Point:Covers) if(IsValid(Point)) Point->Destroy(); for(auto Prop:Props) if(IsValid(Prop)) Prop->Destroy(); }
 Super::EndPlay(Reason);
}
