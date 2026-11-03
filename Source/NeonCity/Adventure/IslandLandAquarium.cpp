#include "Adventure/IslandLandAquarium.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "StaticMeshResources.h"
AIslandLandAquarium::AIslandLandAquarium()
{
 Kind=ENeonCityFishingStation::Aquarium;StationID=TEXT("Island.Mansion.Aquarium.Main");DisplayName=FText::FromString(TEXT("Aurelia Marine Aquarium"));
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.05f;
 InteractionBounds->SetBoxExtent(FVector(145,45,90));InteractionBounds->SetRelativeLocation(FVector(0,0,125));
}
void AIslandLandAquarium::BeginPlay()
{
 Super::BeginPlay();
 if(!Visual->GetStaticMesh())Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/BBQ/SM_MansionAquarium.SM_MansionAquarium")));
 WaterSurface=NewObject<UStaticMeshComponent>(this,TEXT("ContainedAquariumSurface"));AddInstanceComponent(WaterSurface);WaterSurface->SetupAttachment(RootComponent);
 WaterSurface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane")));
 auto* WaterMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/M_AquariumStillWater"));
 if(!WaterMaterial)WaterMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/M_RidgeContainedWater"));
 WaterSurface->SetMaterial(0,WaterMaterial);
 WaterSurface->SetRelativeLocation(FVector(0,0,194));WaterSurface->SetRelativeScale3D(FVector(2.94,.74,1));
 WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);WaterSurface->SetCanEverAffectNavigation(false);WaterSurface->RegisterComponent();
 WaterBody=NewObject<UStaticMeshComponent>(this,TEXT("ContainedAquariumWater"));AddInstanceComponent(WaterBody);WaterBody->SetupAttachment(RootComponent);
 WaterBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));WaterBody->SetMaterial(0,WaterMaterial);
 WaterBody->SetRelativeLocation(FVector(0,0,139));WaterBody->SetRelativeScale3D(FVector(2.94,.74,1.10));
 WaterBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);WaterBody->SetCanEverAffectNavigation(false);WaterBody->RegisterComponent();
 TankLight=NewObject<UPointLightComponent>(this,TEXT("RecessedAquariumLight"));AddInstanceComponent(TankLight);TankLight->SetupAttachment(RootComponent);
 TankLight->SetMobility(EComponentMobility::Movable);TankLight->SetRelativeLocation(FVector(0,0,190));TankLight->SetIntensityUnits(ELightUnits::Lumens);
 TankLight->SetIntensity(FMath::Clamp(DisplayLumens,0.f,1000.f));TankLight->SetAttenuationRadius(170);TankLight->SetLightColor(FLinearColor(.8f,.91f,1));TankLight->SetCastShadows(false);TankLight->RegisterComponent();
}
FText AIslandLandAquarium::GetInteractionName_Implementation() const
{return FText::FromString(TEXT("Manage living fish · ENTER put in / take out"));}
bool AIslandLandAquarium::TransferFish(FGuid ID)
{
 auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();const auto* Record=Inventory->FindFish(ID);
 if(!Record)return false;
 return Record->Storage==ENeonCityFishStorage::Pouch?Inventory->PutInAquarium(ID,ContainerID,bSaltwater):
  Record->Storage==ENeonCityFishStorage::Aquarium?Inventory->RemoveFromAquarium(ID,ContainerID):false;
}
bool AIslandLandAquarium::GetDisplayedFishTransform(FGuid ID,FTransform& Result) const
{
 const auto* Found=FishVisuals.Find(ID);if(!Found || !*Found || !(*Found)->IsRegistered())return false;
 Result=(*Found)->GetRelativeTransform();return true;
}
void AIslandLandAquarium::Tick(float Delta)
{
 Super::Tick(Delta);const auto* Inventory=GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
 RefreshClock+=Delta;
 if(RefreshClock>=.5f)
 {
  RefreshClock=0;TSet<FGuid> Present;
  for(const auto& Fish:Inventory->GetFish())if(Fish.Storage==ENeonCityFishStorage::Aquarium && Fish.ContainerID==ContainerID)
  {
   Present.Add(Fish.FishID);if(FishVisuals.Contains(Fish.FishID))continue;
   const auto* Species=Inventory->FindSpecies(Fish.SpeciesID);auto* Mesh=Species?Species->Mesh.LoadSynchronous():nullptr;if(!Mesh)continue;
   auto* C=NewObject<UStaticMeshComponent>(this);C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Movable);
   C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->RegisterComponent();
   const auto* Data=Mesh->GetRenderData();const float Length=FMath::Max(1.f,float((Data?Data->Bounds:Mesh->GetBounds()).BoxExtent.X*2));
   C->SetRelativeScale3D(FVector(Fish.SizeCm/Length));FishVisuals.Add(Fish.FishID,C);
  }
  for(auto It=FishVisuals.CreateIterator();It;++It)if(!Present.Contains(It.Key())){if(It.Value())It.Value()->DestroyComponent();It.RemoveCurrent();}
 }
 const float Time=GetWorld()->GetTimeSeconds();
 for(const auto& Pair:FishVisuals)
 {
  const auto* Fish=Inventory->FindFish(Pair.Key);if(!Fish || !Pair.Value)continue;
  const float Phase=uint32(Pair.Key.A%997)*(.006f*PI),Clock=Time*(.19f+float(Pair.Key.B%11)*.009f)+Phase;
  // Both anatomy and turn radius stay inside the real tank; large caught fish
  // are rejected by the container rather than cosmetically shrunk to fit.
  const float X=FMath::Max(8.f,float(TankExtent.X)-Fish->SizeCm*.6f-8),Y=FMath::Max(5.f,float(TankExtent.Y)-Fish->SizeCm*.14f-5);
  const FVector Pos=WaterCentre+FVector(FMath::Sin(Clock)*X,FMath::Cos(Clock)*Y,FMath::Sin(Clock*.57f+Phase)*20);
  const FVector Tangent(FMath::Cos(Clock)*X,-FMath::Sin(Clock)*Y,FMath::Cos(Clock*.57f+Phase)*11.4f);
  Pair.Value->SetRelativeLocationAndRotation(Pos,Tangent.Rotation());
 }
}
