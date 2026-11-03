#include "Police/PoliceTestArea.h"
#include "Police/SecurityCamera.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
void APoliceTestArea::BeginPlay()
{
 Super::BeginPlay(); if(!HasAuthority() || UGameplayStatics::GetCurrentLevelName(GetWorld(),true)!=TEXT("NeonCity_SouthBeach_Test")) return;
 for(FVector P:{FVector(-5200,1100,300),FVector(2500,1100,300)}) if(auto* Camera=GetWorld()->SpawnActor<ASecurityCamera>(P,FRotator(-12,-90,0))) Fixtures.Add(Camera);
 const FTransform T(FVector(-4700,1000,134)); auto* NPC=GetWorld()->SpawnActorDeferred<ANeonCityNPCCharacter>(ANeonCityNPCCharacter::StaticClass(),T);
 if(NPC) { NPC->InitializePedestrian(TEXT("PoliceTest.Witness"),nullptr,{FVector(-4700,1000,134),FVector(-4000,1000,134)},161); NPC->FinishSpawning(T); Fixtures.Add(NPC); }
}
void APoliceTestArea::EndPlay(EEndPlayReason::Type R)
{ if(R==EEndPlayReason::Destroyed) for(auto A:Fixtures) if(IsValid(A)) A->Destroy(); Super::EndPlay(R); }
