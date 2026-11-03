#include "World/Marine/NeonCityReferenceVillage.h"
#include "World/Marine/ReferenceVillageGeometry.h"
#include "World/Lighting/WorldLightingSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

using namespace NeonCityReferenceVillage;
ANeonCityReferenceVillage::ANeonCityReferenceVillage()
{
 PrimaryActorTick.bCanEverTick=true;
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("VillageOrigin"));SetRootComponent(Root);Root->SetMobility(EComponentMobility::Static);
 for(int32 I=0;I<UE_ARRAY_COUNT(Parts);++I)
 {
  const FPart& P=Parts[I];auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("VillageSurface_%d"),I));
  Mesh->SetupAttachment(Root);Mesh->SetRelativeLocation(P.Pivot);Mesh->SetMobility(P.Swing>=-1?EComponentMobility::Movable:EComponentMobility::Static);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);
  Mesh->SetCastShadow(FString(P.Material)!=TEXT("net") && FString(P.Material)!=TEXT("fabric"));Mesh->SetBoundsScale(1.05f);
  Surfaces.Add(Mesh);
 }
 for(int32 I=0;I<UE_ARRAY_COUNT(Boxes);++I)
 {
  const auto& B=Boxes[I];auto* Box=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("VillageContact_%d"),I));
  Box->SetupAttachment(Root);Box->SetMobility(EComponentMobility::Static);Box->SetRelativeLocation(B.Center);Box->SetRelativeRotation(FRotator(0,B.Yaw,0));
  Box->SetBoxExtent(B.Half);Box->SetCollisionProfileName(TEXT("BlockAll"));Box->SetGenerateOverlapEvents(false);
  // Source ladder rungs are non-solid virtual supports used by its climbing
  // controller. Turning the vertically stacked volumes into solid boxes creates
  // a wall. Keep them non-blocking for the native ladder adapter.
  Box->SetCollisionEnabled(B.Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
  Box->CanCharacterStepUpOn=B.Walkable?ECB_Yes:ECB_No;
  Box->SetWalkableSlopeOverride(FWalkableSlopeOverride(B.Walkable?WalkableSlope_Default:WalkableSlope_Unwalkable,0));
 }
 // Cylinder contact meshes are authoring assets with convex hulls. Retain exact
 // radius/height, flat ends and floor exclusion instead of substituting capsules.
 for(int32 I=0;I<UE_ARRAY_COUNT(Cylinders);++I)
 {
  const auto& C=Cylinders[I];auto* Contact=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("VillagePile_%d"),I));
  Contact->SetupAttachment(Root);Contact->SetMobility(EComponentMobility::Static);Contact->SetRelativeLocation(C.Center);
  Contact->SetRelativeScale3D(FVector(C.Radius/50,C.Radius/50,C.HalfHeight/50));Contact->SetHiddenInGame(true);
  Contact->SetVisibility(false);Contact->SetCollisionProfileName(TEXT("BlockAll"));Contact->SetGenerateOverlapEvents(false);
  Contact->CanCharacterStepUpOn=ECB_No;Contact->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable,0));
 }
 for(int32 I=0;I<UE_ARRAY_COUNT(NeonCityReferenceVillage::Lights);++I)
 {
  const auto& L=NeonCityReferenceVillage::Lights[I];auto* Light=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("VillageLight_%d"),I));
  Light->SetupAttachment(Root);Light->SetMobility(EComponentMobility::Movable);Light->SetRelativeLocation(L.Position);
  Light->SetIntensityUnits(ELightUnits::Lumens);
  Light->SetLightColor(L.Color);Light->SetIntensity(0);Light->SetCastShadows(false);Light->SetAttenuationRadius(700);Light->SetSourceRadius(8);Light->SetVisibility(false);
  Lights.Add(Light);
 }
 Swings.SetNum(LanternCount);for(int32 I=0;I<Swings.Num();++I){Swings[I].T=I*3.73;Swings[I].Phase=I*1.71;}
}
void ANeonCityReferenceVillage::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 for(int32 I=0;I<Surfaces.Num();++I)
 {
  const FString Path=FString(TEXT("/Game/NeonCity/Art/Tidewater/Village/"))+Parts[I].Asset;
  auto* Override=LandSurfaceOverrides.Find(I);
  if(Override && *Override)Surfaces[I]->SetStaticMesh(*Override);
  else if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path))Surfaces[I]->SetStaticMesh(Mesh);
  // The land doorway derivatives supply exact plaster/stone wall probes to
  // the orbit camera. Walking continues to use the measured room contacts.
  const bool CameraWalls=Override && *Override && (I==1 || I==4);
  Surfaces[I]->SetCollisionEnabled(CameraWalls?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
  Surfaces[I]->SetCollisionResponseToAllChannels(ECR_Ignore);
  if(CameraWalls)Surfaces[I]->SetCollisionResponseToChannel(ECC_Camera,ECR_Block);
 }
 TInlineComponentArray<UBoxComponent*> BoxesToReview(this);
 for(auto* Box:BoxesToReview)
 {
  const FString Name=Box->GetName();
  if(!Name.StartsWith(TEXT("VillageContact_")))continue;
  const int32 Index=FCString::Atoi(*Name.RightChop(15));
  if(Index>=0 && Index<UE_ARRAY_COUNT(Boxes))
  {
   Box->SetCollisionEnabled(Boxes[Index].Solid && !OpenBuildingContacts.Contains(Index)?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
   Box->SetCollisionResponseToChannel(ECC_Visibility,SightTransparentContacts.Contains(Index)?ECR_Ignore:ECR_Block);
  }
 }
 if(auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Village/SM_NCTW_ContactCylinder")))
 {
  TInlineComponentArray<UStaticMeshComponent*> Meshes(this);
  for(auto* Mesh:Meshes)if(Mesh->GetName().StartsWith(TEXT("VillagePile_")))Mesh->SetStaticMesh(Cylinder);
 }
}
void ANeonCityReferenceVillage::BeginPlay()
{
 Super::BeginPlay();
 // Streaming may re-enter BeginPlay on retained actors. Reuse each existing
 // instance; a MID cannot be the parent of another MID. Rebuild the bounded
 // working list so repeated visibility/load cycles do not retain duplicates.
 Materials.Reset();
 TMap<UMaterialInterface*,UMaterialInstanceDynamic*> Unique;
 for(auto Mesh:Surfaces)if(auto* Base=Mesh->GetMaterial(0))
 {
  auto*& MID=Unique.FindOrAdd(Base);
  if(!MID)
  {
   MID=Cast<UMaterialInstanceDynamic>(Base);
   if(!MID)MID=UMaterialInstanceDynamic::Create(Base,this);
   Materials.AddUnique(MID);
  }
  Mesh->SetMaterial(0,MID);
 }
 if(auto* Lighting=GetWorld()->GetSubsystem<UWorldLightingSubsystem>())
 {
  for(auto Material:Materials)Lighting->RegisterNightMaterial(Material);
  for(int32 I=0;I<Lights.Num();++I)
  {
   // House lanterns sit close to the plaster and window frames. Their old
   // dock-lamp conversion washed out those surfaces in the normal night view.
   // Keep the pier and path fixtures unchanged; retain the house light budget.
   const float LumensPerSourceUnit=I>=15 && I<=37?19.25f:55.f;
   FNeonCityLocalLightPolicy Policy;Policy.Intensity=NeonCityReferenceVillage::Lights[I].SourceIntensity*LumensPerSourceUnit;
   Policy.CullDistance=6000;Policy.ShadowDistance=1800;Policy.bCastShadows=true;
   Lighting->RegisterLight(Lights[I],Policy);
  }
 }
}
void ANeonCityReferenceVillage::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const FVector2D Wind=WindDirection.GetSafeNormal();const double Speed=FMath::Clamp(WindSpeed,0.f,20.f);
 const int32 Steps=FMath::Max(1,FMath::CeilToInt(FMath::Min(DeltaSeconds,.1f)*120));const double H=FMath::Min(DeltaSeconds,.1f)/Steps;
 for(int32 Step=0;Step<Steps;++Step)
 {
  const double T=(Sign.T+=H),Gust=1+.35*FMath::Sin(T*.73+1.3)*FMath::Sin(T*.31)+.22*FMath::Sin(T*2.1+FMath::Sin(T*.9))+.1*FMath::Sin(T*5.3+2);
  const double Push=-.0033*Speed*Speed*Gust*Wind.Y*FMath::Abs(Wind.Y)-.0009*Speed*FMath::Sin(T*1.7+FMath::Sin(T*.43)*2);
  Sign.AV+=(4.9*4.9*(Push*FMath::Cos(Sign.A)-FMath::Sin(Sign.A))-2*.07*4.9*Sign.AV)*H;Sign.A+=Sign.AV*H;
  const double Twist=.004*Speed*Speed*Wind.X*Wind.Y*Gust+.0015*Speed*FMath::Sin(T*2.9+1.1);
  Sign.BV+=(10.5*10.5*(Twist-Sign.B)-2*.12*10.5*Sign.BV)*H;Sign.B+=Sign.BV*H;
  for(auto& S:Swings)
  {
   const double Time=(S.T+=H),P=S.Phase,G=1+.4*FMath::Sin(Time*.73+P)*FMath::Sin(Time*.31+P*.5)+.25*FMath::Sin(Time*2.3+FMath::Sin(Time*.9+P)*1.5);
   const double X=.0011*Speed*Speed*G*Wind.X+.0012*Speed*FMath::Sin(Time*1.9+P+FMath::Sin(Time*.53)*2);
   const double Z=.0011*Speed*Speed*G*Wind.Y+.0012*Speed*FMath::Sin(Time*1.6+P*1.7+FMath::Sin(Time*.41)*2);
   S.AV+=(5.4*5.4*(X-S.A)-2*.05*5.4*S.AV)*H;S.A+=S.AV*H;
   S.BV+=(5.4*5.4*(Z-S.B)-2*.05*5.4*S.BV)*H;S.B+=S.BV*H;
  }
 }
 for(int32 I=0;I<Surfaces.Num();++I)
 {
  const int32 S=Parts[I].Swing;if(S==-2)continue;
  const FQuat Rotation=S==-1?FQuat(FVector::UpVector,Sign.B)*FQuat(FVector::ForwardVector,Sign.A):
   FQuat(FVector::ForwardVector,Swings[S].B)*FQuat(FVector::RightVector,Swings[S].A);
  Surfaces[I]->SetRelativeRotation(Rotation);
  if(S>=0){const int32 L=LanternLights[S];Lights[L]->SetRelativeLocation(Parts[I].Pivot+Rotation.RotateVector(NeonCityReferenceVillage::Lights[L].Position-Parts[I].Pivot));}
 }
 Refresh-=DeltaSeconds;if(Refresh>0)return;Refresh=.25f;
 for(auto M:Materials){M->SetScalarParameterValue(TEXT("WindSpeed"),float(Speed));
  M->SetVectorParameterValue(TEXT("WindDirection"),FLinearColor(float(Wind.X),float(Wind.Y),0,0));
  const FVector Origin=GetActorLocation();M->SetVectorParameterValue(TEXT("IslandOriginCm"),FLinearColor(float(Origin.X),float(Origin.Y),float(Origin.Z),0));}
 const auto* PC=GetWorld()->GetFirstPlayerController();const FVector Eye=PC?PC->GetFocalLocation():GetActorLocation();
 SetActorTickInterval(FVector::DistSquared(Eye,GetActorLocation()+FVector(5000,10000,0))>FMath::Square(30000.f)?1.f:0.f);
}
