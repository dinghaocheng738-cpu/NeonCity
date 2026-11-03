#include "Adventure/IslandScenePanel.h"
#include "Adventure/IslandLandLight.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Property/PropertySubsystem.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
AIslandScenePanel::AIslandScenePanel()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RoomPanel")));
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("SceneInteraction"));Interaction->SetupAttachment(RootComponent);Interaction->SetBoxExtent(FVector(6,16,22));Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Face=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrushedPanel"));Face->SetupAttachment(RootComponent);Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);Face->SetCanEverAffectNavigation(false);
 Display=CreateDefaultSubobject<UTextRenderComponent>(TEXT("SceneLabels"));Display->SetupAttachment(RootComponent);Display->SetRelativeLocation(FVector(7,0,7));Display->SetHorizontalAlignment(EHTA_Center);Display->SetWorldSize(4);Display->SetTextRenderColor(FColor(185,194,190));
}
bool AIslandScenePanel::CanInteract_Implementation(AActor* Visitor)const
{
 const auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);auto* GI=GetGameInstance();
 return P && GI && P->IsLocallyControlled() && P->HasAuthority() && !P->GetAttachParentActor() && P->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist(P->GetActorLocation(),GetActorLocation())<240 && GI->GetSubsystem<UPropertySubsystem>()->IsOwned(PropertyID);
}
FText AIslandScenePanel::GetInteractionName_Implementation()const
{
 const TCHAR* Names[]={TEXT("Day / task"),TEXT("Evening / relax"),TEXT("Cinema / low")};
 return FText::FromString(FString::Printf(TEXT("F Lighting — %s | %s"),*ZoneName.ToString(),Names[Scene]));
}
void AIslandScenePanel::Interact_Implementation(AActor* Visitor){if(CanInteract_Implementation(Visitor))SetScene((Scene+1)%3);}
bool AIslandScenePanel::SetScene(int32 Index)
{
 if(Index<0 || Index>2)return false;Scene=Index;Apply();return true;
}
void AIslandScenePanel::Apply()
{
 Display->SetText(FText::FromString(Scene==0?TEXT("DAY / TASK"):Scene==1?TEXT("EVENING / RELAX"):TEXT("CINEMA / LOW")));
 if(auto* Scheduler=GetWorld()?GetWorld()->GetSubsystem<UWorldLightingSubsystem>():nullptr)
 {
  const float Power[]={1.f,.58f,.12f};
  for(const auto& L:Lights)if(IsValid(L) && L->GetWorld()==GetWorld())
  {
   FNeonCityLocalLightPolicy Policy;Policy.Intensity=L->Lumens*Power[Scene];Policy.DayMultiplier=FMath::Max(L->DayMultiplier,Scene==0?.6f:.15f);Policy.bCastShadows=L->bBudgetShadows;Policy.CullDistance=6000;Policy.ShadowDistance=1400;Policy.VolumetricScattering=.1f;
   Scheduler->RegisterLight(Cast<UPointLightComponent>(L->GetLightComponent()),Policy);
  }
  Scheduler->RefreshLighting();
 }
}
void AIslandScenePanel::BeginPlay(){Super::BeginPlay();Apply();}
void AIslandScenePanel::EndPlay(EEndPlayReason::Type Reason)
{
 // Lamp actors own their lifecycle. This panel never unregisters another actor's lamp.
 Super::EndPlay(Reason);
}
