#include "Player/NeonCityFootstepComponent.h"
#include "World/Audio/NeonCityReferenceSoundscape.h"
#include "Water/WaterInteractionComponent.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace NeonCityFootsteps
{
// material-name keywords -> surface (first match wins; checked in order)
const TPair<const TCHAR*,const TCHAR*> Keywords[]={
 {TEXT("Carpet"),TEXT("carpet")},{TEXT("Bedding"),TEXT("carpet")},{TEXT("Cushion"),TEXT("carpet")},{TEXT("Upholstery"),TEXT("carpet")},
 {TEXT("LMBDeck"),TEXT("wood")},{TEXT("Timber"),TEXT("wood")},{TEXT("Wood"),TEXT("wood")},{TEXT("Oak"),TEXT("wood")},{TEXT("Teak"),TEXT("wood")},
 {TEXT("Bamboo"),TEXT("wood")},{TEXT("Plank"),TEXT("wood")},
 {TEXT("Asphalt"),TEXT("asphalt")},{TEXT("Road"),TEXT("asphalt")},{TEXT("Line"),TEXT("asphalt")},
 {TEXT("Sand"),TEXT("sand")},
 {TEXT("Grass"),TEXT("grass")},{TEXT("Clover"),TEXT("grass")},{TEXT("Planting"),TEXT("grass")},{TEXT("Lawn"),TEXT("grass")},
 {TEXT("Ground"),TEXT("grass")},{TEXT("Landscape"),TEXT("grass")},{TEXT("Soil"),TEXT("grass")},
 {TEXT("Granite"),TEXT("stone")},{TEXT("Paving"),TEXT("stone")},{TEXT("Path"),TEXT("stone")},{TEXT("Stone"),TEXT("stone")},
 {TEXT("Limestone"),TEXT("stone")},{TEXT("Marble"),TEXT("stone")},{TEXT("Sidewalk"),TEXT("stone")},{TEXT("Kerb"),TEXT("stone")},
 {TEXT("Concrete"),TEXT("stone")},{TEXT("Courtyard"),TEXT("stone")},{TEXT("Brick"),TEXT("stone")},{TEXT("Tile"),TEXT("stone")}};
}

UNeonCityFootstepComponent::UNeonCityFootstepComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics; }

bool UNeonCityFootstepComponent::Load()
{
 if(bLoaded)return Banks.Num()>0;
 bLoaded=true;
 auto Bank=[&](FName Surface,const TCHAR* Clip,float Volume,float Pitch)
 {
  FBank B;B.Volume=Volume;B.Pitch=Pitch;
  for(int32 K=1;K<=5;++K)
   if(auto* S=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_%s_%02d.SW_NCTW_step_%s_%02d"),Clip,K,Clip,K),nullptr,LOAD_NoWarn|LOAD_Quiet))
   {B.Clips.Add(S);Held.Add(S);}
  if(B.Clips.Num())Banks.Add(Surface,B);
 };
 Bank(TEXT("grass"),TEXT("grass"),.8f,1.f);Bank(TEXT("stone"),TEXT("rock"),.8f,1.06f);Bank(TEXT("asphalt"),TEXT("rock"),.7f,.86f);
 Bank(TEXT("wood"),TEXT("wood"),.85f,1.f);Bank(TEXT("carpet"),TEXT("grass"),.35f,1.18f);Bank(TEXT("sand"),TEXT("sand"),.8f,1.f);
 Bank(TEXT("water"),TEXT("water"),.9f,1.f);
 Falloff=NewObject<USoundAttenuation>(this);
 Falloff->Attenuation.bAttenuate=true;Falloff->Attenuation.bSpatialize=true;Falloff->Attenuation.AttenuationShape=EAttenuationShape::Sphere;
 Falloff->Attenuation.AttenuationShapeExtents=FVector(200,0,0);Falloff->Attenuation.FalloffDistance=2500;
 Falloff->Attenuation.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;
 return Banks.Num()>0;
}

void UNeonCityFootstepComponent::BeginPlay()
{
 Super::BeginPlay();
 Walker=Cast<ACharacter>(GetOwner());
 // the island (Tidewater) plays its own surface steps while the listener is in its soundscape
 for(TActorIterator<AActor> It(GetWorld());It;++It)if(auto* Island=It->FindComponentByClass<UNeonCityReferenceSoundscapeComponent>())Islands.Add(Island);
 if(Walker.IsValid())Walker->LandedDelegate.AddDynamic(this,&UNeonCityFootstepComponent::OnLanded);
}

FName UNeonCityFootstepComponent::ProbeSurface(const FVector& Foot) const
{
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(Footstep),true,GetOwner());Q.bReturnFaceIndex=true;
 if(!GetWorld()->LineTraceSingleByChannel(Hit,Foot+FVector(0,0,20),Foot-FVector(0,0,60),ECC_Visibility,Q))
  return LastSurface.IsNone()?FName(TEXT("stone")):LastSurface;            // over an edge: keep the last ground
 UMaterialInterface* Material=nullptr;FString FaceLookup;
 if(auto* Part=Hit.GetComponent())
 {
  if(Part->GetNumMaterials()==1)Material=Part->GetMaterial(0);
  else{int32 Section=0;Material=Part->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex,Section);FaceLookup=FString::Printf(TEXT(" face %d -> %s"),Hit.FaceIndex,*GetNameSafe(Material));}
  if(!Material && Part->GetNumMaterials()>0)Material=Part->GetMaterial(0);
  // a boat's single body mesh carries many finishes: its decks are teak, its cabin soles carpet
  if(Part->GetNumMaterials()>1 && Part->GetOwner() && Part->GetOwner()->IsA<ANeonCityMarineVehicleBase>())
  {
   const float LocalZ=Part->GetOwner()->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).Z;
   LastGround=FString::Printf(TEXT("%s deck at %.0f cm"),*GetNameSafe(Part->GetOwner()),LocalZ);
   return LocalZ<50.f?FName(TEXT("carpet")):FName(TEXT("wood"));
  }
 }
 const FString Name=Material?Material->GetName():Hit.GetComponent()?Hit.GetComponent()->GetName():FString();
 LastGround=FString::Printf(TEXT("%s/%s: %s%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Name,*FaceLookup);
 for(const auto& K:NeonCityFootsteps::Keywords)if(Name.Contains(K.Key))return FName(K.Value);
 return TEXT("stone");                                                        // hard floors are the common case in town
}

void UNeonCityFootstepComponent::Step(float Volume)
{
 if(!Walker.IsValid() || !Load())return;
 const FVector Foot=Walker->GetActorLocation()-FVector(0,0,Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 FName Surface=ProbeSurface(Foot);
 if(const auto* Water=Walker->FindComponentByClass<UWaterInteractionComponent>();Water && Water->GetWaterState()==EWaterInteractionState::Shallow)Surface=TEXT("water");
 const FBank* B=Banks.Find(Surface);if(!B)B=Banks.Find(TEXT("stone"));if(!B || !B->Clips.Num())return;
 UGameplayStatics::PlaySoundAtLocation(this,B->Clips[FMath::RandHelper(B->Clips.Num())],Foot,FRotator::ZeroRotator,Volume*B->Volume,
  B->Pitch*FMath::FRandRange(.94f,1.06f),0.f,Falloff);
 LastSurface=Surface;++Steps;
}

bool UNeonCityFootstepComponent::OnIsland() const
{
 for(const auto& Island:Islands)if(Island.IsValid() && Island->IsActive())return true;
 return false;
}

void UNeonCityFootstepComponent::OnLanded(const FHitResult& Hit)
{
 if(OnIsland() || !Walker.IsValid() || Walker->IsHidden())return;
 const float Impact=FMath::Clamp(-Walker->GetVelocity().Z/900.f,.4f,1.2f);
 Step(Impact);Travelled=0;
}

void UNeonCityFootstepComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 ACharacter* C=Walker.Get();
 if(!C || C->IsHidden() || !C->GetActorEnableCollision() || OnIsland()){Travelled=0;return;}
 auto* Move=C->GetCharacterMovement();
 if(!Move || !Move->IsMovingOnGround()){Travelled=0;return;}
 // speed relative to what it stands on (a moving deck), horizontal only
 const float Speed=FVector(Move->Velocity.X,Move->Velocity.Y,0).Size();
 if(Speed<20){Travelled=FMath::Min(Travelled,40.f);return;}
 const float Stride=Speed>320?120.f:FMath::Lerp(55.f,80.f,FMath::Clamp(Speed/320.f,0.f,1.f));
 Travelled+=Speed*Delta;
 if(Travelled>=Stride){Travelled-=Stride;Step(FMath::Lerp(.45f,.95f,FMath::Clamp(Speed/600.f,0.f,1.f)));}
}
