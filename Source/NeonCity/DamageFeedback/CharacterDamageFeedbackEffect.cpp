#include "DamageFeedback/CharacterDamageFeedbackEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
ACharacterDamageFeedbackEffect::ACharacterDamageFeedbackEffect()
{
 PrimaryActorTick.bCanEverTick=true;
 Marker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); SetRootComponent(Marker);
 Marker->SetMobility(EComponentMobility::Movable); Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Marker->SetCastShadow(false); Marker->SetGenerateOverlapEvents(false);
 Marker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
 Marker->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
}
ACharacterDamageFeedbackEffect* ACharacterDamageFeedbackEffect::Spawn(UWorld* World,FVector Location,float Radius,float Lifetime,FLinearColor Color)
{
 if(!World) return nullptr;
 const FTransform T(Location);
 auto* Effect=World->SpawnActorDeferred<ACharacterDamageFeedbackEffect>(ACharacterDamageFeedbackEffect::StaticClass(),T);
 if(!Effect) return nullptr;
 Effect->Configure(Radius,Lifetime,Color); Effect->FinishSpawning(T); return Effect;
}
void ACharacterDamageFeedbackEffect::Configure(float Radius,float Lifetime,FLinearColor Color)
{
 BaseScale=FVector(FMath::Clamp(Radius,1.f,60.f)/50.f); Life=FMath::Clamp(Lifetime,.05f,5.f);
 Marker->SetWorldScale3D(BaseScale); Marker->SetVectorParameterValueOnMaterials(TEXT("Color"),FVector(Color.R,Color.G,Color.B));
}
void ACharacterDamageFeedbackEffect::BeginPlay() { Super::BeginPlay(); Age=0; }
void ACharacterDamageFeedbackEffect::Tick(float Delta)
{
 Super::Tick(Delta); Age+=Delta;
 const float Alpha=FMath::Clamp(Age/Life,0.f,1.f);
 Marker->SetWorldScale3D(BaseScale*(1.f-Alpha*.7f));
 AddActorWorldOffset(FVector(0,0,28.f*Delta*(1.f-Alpha)));
 if(Age>=Life) Destroy();
}
