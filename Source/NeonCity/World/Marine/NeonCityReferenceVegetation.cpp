#include "World/Marine/NeonCityReferenceVegetation.h"
#include "Adventure/IslandLargePlantContact.h"
#include "World/Foliage/NeonCityFoliageInteractionComponent.h"
#include "World/Marine/NeonCityReferenceGrass.h"
#include "World/Audio/NeonCityReferenceSoundscape.h"
#include "World/Foliage/NeonCityFoliageContactAudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ANeonCityReferenceVegetation::ANeonCityReferenceVegetation()
{
 PrimaryActorTick.bCanEverTick=true;
 // Only three shared materials, not thousands of instances. The LOD observer
 // follows every frame so fast helicopter motion cannot skip the source band.
 PrimaryActorTick.TickInterval=0;
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("IslandVegetationRoot"));
 Root->SetMobility(EComponentMobility::Static);SetRootComponent(Root);
 FoliageInteraction=CreateDefaultSubobject<UNeonCityFoliageInteractionComponent>(TEXT("FoliageInteraction"));
 CreateDefaultSubobject<UIslandLargePlantContact>(TEXT("MatureShrubContact"));
 Grass=CreateDefaultSubobject<UNeonCityReferenceGrassComponent>(TEXT("GroundFlora"));
 Soundscape=CreateDefaultSubobject<UNeonCityReferenceSoundscapeComponent>(TEXT("IslandSoundscape"));
 ContactAudio=CreateDefaultSubobject<UNeonCityFoliageContactAudioComponent>(TEXT("FoliageContactAudio"));
}

bool ANeonCityReferenceVegetation::RebuildVegetation()
{
 if(!Data || Data->Groups.IsEmpty() || Data->ReferenceCommit!=TEXT("4811ba48d795197de5621985f404e765c0b7c0ef"))return false;
 // Source shader coordinates support translating the island as a streamed zone.
 // A rotated/scaled island requires transforming wind and base records together.
 if(!GetActorRotation().IsNearlyZero() || !GetActorScale3D().Equals(FVector::OneVector))return false;
 UStaticMesh* FarMesh=Data->CanopyImpostor.IsNull()?nullptr:Data->CanopyImpostor.LoadSynchronous();
 if(!Data->CanopyImpostor.IsNull() && !FarMesh)return false;
 UStaticMesh* PalmFarMesh=Data->PalmFar.IsNull()?nullptr:Data->PalmFar.LoadSynchronous();
 if(!Data->PalmFar.IsNull() && !PalmFarMesh)return false;
 // Validate before touching existing authored components.
 TArray<UStaticMesh*> Meshes;
 for(const auto& Group:Data->Groups)
 {
  auto* Mesh=Group.Mesh.LoadSynchronous();
  if(!Mesh || Group.Instances.IsEmpty() || Group.FadeStartCm<0 || Group.CullEndCm<=Group.FadeStartCm)return false;
  Meshes.Add(Mesh);
  for(const auto& Plant:Group.Instances)
  {
   if(Plant.Transform.ContainsNaN() || Plant.Transform.GetScale3D().GetMin()<=0
    || FVector::DotProduct(Plant.Transform.GetRotation().GetUpVector(),FVector::UpVector)<.99999)return false;
   const auto A=Plant.ShaderData0,B=Plant.ShaderData1;
   for(float V:{A.R,A.G,A.B,A.A,B.R,B.G,B.B,B.A})if(!FMath::IsFinite(V))return false;
   if(A.G<=0 || A.B<=0 || !Plant.Transform.GetLocation().Equals(FVector(B.G,-B.A,B.B)*100,.05))return false;
   if(FarMesh && (Group.SourceFamily==TEXT("trees") || Group.SourceFamily==TEXT("shrubs")))
   {
    const double Stretch=Plant.Transform.GetScale3D().Z/Plant.Transform.GetScale3D().X;
    const double Yaw=FMath::DegreesToRadians(Plant.Transform.Rotator().Yaw);
    if(!FMath::IsNearlyEqual(FMath::Abs(double(B.R)),Stretch,.0001)
     || (B.R<0)!=(Group.SourceFamily==TEXT("shrubs"))
     || FMath::Abs(FMath::UnwindRadians(Yaw-double(A.A)))>.0001)return false;
   }
  }
 }
 for(auto Component:PlantGroups)if(IsValid(Component))Component->DestroyComponent();
 for(auto Component:ImpostorGroups)if(IsValid(Component))Component->DestroyComponent();
 for(auto Component:FarPalmGroups)if(IsValid(Component))Component->DestroyComponent();
 PlantGroups.Reset();ImpostorGroups.Reset();FarPalmGroups.Reset();Materials.Reset();
 const auto CreateGroup=[&](int32 G,UStaticMesh* Mesh,bool Far)
 {
  const auto& Group=Data->Groups[G];
  const bool Palm=Group.SourceFamily==TEXT("palms") && PalmFarMesh;
  auto* Component=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,
   UHierarchicalInstancedStaticMeshComponent::StaticClass(),*FString::Printf(TEXT("Canonical%s_%d"),Far?(Palm?TEXT("PalmFar"):TEXT("Impostors")):TEXT("Plants"),G)),RF_Transactional);
  if(Palm)Component->ComponentTags.Add(Far?TEXT("NeonCity.Palm.Far"):TEXT("NeonCity.Palm.Near"));
  AddInstanceComponent(Component);Component->SetupAttachment(RootComponent);Component->SetMobility(EComponentMobility::Static);
  Component->SetStaticMesh(Mesh);Component->SetNumCustomDataFloats(Far?8:11);
  Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetGenerateOverlapEvents(false);
  // Canonical Vegetation.js leaves far parts at LodLevel's castShadow=false.
  // Their atlas retains crown AO and they receive shadows. Only nearby real
  // geometry casts, avoiding thousands of billboard bounds in the VSM queue.
  Component->SetCanEverAffectNavigation(false);Component->SetCastShadow(!Far && Group.bCastShadows);
  // View-facing atlas cards represent distant crowns, not physical occluders.
  // Their undeformed ray-tracing geometry would occlude the sky incorrectly
  // and consume Lumen surface-cache pages in addition to the real near trees.
  Component->SetVisibleInRayTracing(!Far);
  Component->SetAffectDistanceFieldLighting(!Far);
  Component->SetAffectDynamicIndirectLighting(!Far);
  const bool Shrub=Group.SourceFamily==TEXT("shrubs");
  Component->SetCullDistances(Far?(Shrub?16000:260000):Group.FadeStartCm,Far?(Shrub?18000:280000):Group.CullEndCm);
  Component->bAutoRebuildTreeOnInstanceChanges=false;
  TArray<FTransform> Transforms;Transforms.Reserve(Group.Instances.Num());
  for(const auto& Plant:Group.Instances)Transforms.Add(Plant.Transform);
  Component->AddInstances(Transforms,false,false,false);
  for(int32 I=0;I<Group.Instances.Num();++I)
  {
   const auto& Plant=Group.Instances[I];const auto A=Plant.ShaderData0,B=Plant.ShaderData1;
   const float Values[]={A.R,A.G,A.B,A.A,B.R,B.G,B.B,B.A,0,0,0};Component->SetCustomData(I,MakeArrayView(Values,Far?8:11),false);
  }
  Component->bAutoRebuildTreeOnInstanceChanges=true;Component->BuildTreeIfOutdated(false,true);
  Component->RegisterComponent();Component->MarkRenderStateDirty();return Component;
 };
 for(int32 G=0;G<Data->Groups.Num();++G)
 {
  const auto& Group=Data->Groups[G];
  PlantGroups.Add(CreateGroup(G,Meshes[G],false));
  if(FarMesh && (Group.SourceFamily==TEXT("trees") || Group.SourceFamily==TEXT("shrubs")))
   ImpostorGroups.Add(CreateGroup(G,FarMesh,true));
  if(PalmFarMesh && Group.SourceFamily==TEXT("palms"))FarPalmGroups.Add(CreateGroup(G,PalmFarMesh,true));
 }
 if(HasActorBegunPlay()){InitializeMaterials();FoliageInteraction->RebuildIndex();}
 return true;
}

void ANeonCityReferenceVegetation::BeginPlay()
{
 Super::BeginPlay();
 if(PlantGroups.IsEmpty() && !RebuildVegetation())return;
 InitializeMaterials();
 FoliageInteraction->RebuildIndex();
}

void ANeonCityReferenceVegetation::InitializeMaterials()
{
 if(GrassData && bUseCanonicalGrassSoundscape && !Soundscape->InitializeSoundscape())
  UE_LOG(LogTemp,Error,TEXT("Canonical sound library rejected for %s"),*GetName());
 if(GrassData && !Grass->InitializeGrass(GrassData))
  UE_LOG(LogTemp,Error,TEXT("Canonical grass library rejected for %s"),*GetName());
 Materials.Reset();
 // Apply the representation policy to older serialized instances as well.
 TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> FarGroups=ImpostorGroups;FarGroups.Append(FarPalmGroups);
 for(auto Component:FarGroups)if(IsValid(Component))
 {
  Component->SetVisibleInRayTracing(false);
  Component->SetAffectDistanceFieldLighting(false);
  Component->SetAffectDynamicIndirectLighting(false);
 }
 TMap<UMaterialInterface*,TMap<int32,UMaterialInstanceDynamic*>> Unique;
 TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> All=PlantGroups;All.Append(FarGroups);
 for(auto Component:All)if(IsValid(Component) && Component->GetStaticMesh())
 {
  const int32 PalmRepresentation=Component->ComponentHasTag(TEXT("NeonCity.Palm.Far"))?2:Component->ComponentHasTag(TEXT("NeonCity.Palm.Near"))?1:0;
  // Authored plants can separate bark, rachis and blades. Each slot must
  // receive the same observer/wind updates; do not leave leaf slots frozen
  // at their material defaults while only the trunk follows the weather.
  for(int32 Slot=0;Slot<Component->GetStaticMesh()->GetStaticMaterials().Num();++Slot)
   if(auto* Base=Component->GetStaticMesh()->GetMaterial(Slot))
   {
    auto*& Material=Unique.FindOrAdd(Base).FindOrAdd(PalmRepresentation);
    if(!Material){Material=UMaterialInstanceDynamic::Create(Base,this);Material->SetScalarParameterValue(TEXT("PalmRepresentation"),PalmRepresentation);Materials.Add(Material);}
    Component->SetMaterial(Slot,Material);
   }
 }
 UpdateMaterials();UpdateObserver();
}

void ANeonCityReferenceVegetation::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 // Source-integrated travelling field stays continuous when wind changes.
 const float Speed=FMath::IsFinite(WindSpeed)?FMath::Clamp(WindSpeed,0.f,25.f):0;
 if(FMath::IsFinite(DeltaSeconds) && DeltaSeconds>0)
  GustOffset+=(WindDirection.ContainsNaN()?FVector2D::ZeroVector:WindDirection.GetSafeNormal())*DeltaSeconds*(Speed*.7+1.5);
 if(FMath::IsFinite(DeltaSeconds) && DeltaSeconds>0)WindRefreshElapsed+=DeltaSeconds;
 if(WindRefreshElapsed>=.2f){WindRefreshElapsed=0;UpdateMaterials();}
 UpdateObserver();
}

void ANeonCityReferenceVegetation::UpdateMaterials()
{
 const FVector Origin=GetActorLocation();const FVector2D Direction=WindDirection.ContainsNaN()?FVector2D::ZeroVector:WindDirection.GetSafeNormal();
 Grass->SetWind(Direction,WindSpeed,GustOffset);
 for(auto Material:Materials)if(IsValid(Material))
 {
  Material->SetVectorParameterValue(TEXT("IslandOriginCm"),FLinearColor(Origin.X,Origin.Y,Origin.Z,0));
  Material->SetVectorParameterValue(TEXT("GustOffset"),FLinearColor(GustOffset.X,GustOffset.Y,0,0));
  Material->SetVectorParameterValue(TEXT("WindDirection"),FLinearColor(Direction.X,Direction.Y,0,0));
  Material->SetScalarParameterValue(TEXT("WindSpeed"),FMath::IsFinite(WindSpeed)?FMath::Clamp(WindSpeed,0.f,25.f):0);
  Material->SetScalarParameterValue(TEXT("CanopyImpostorsEnabled"),ImpostorGroups.IsEmpty()?0:1);
  Material->SetScalarParameterValue(TEXT("CanopyThinningFraction"),FMath::IsFinite(DistantCanopyThinning)?FMath::Clamp(DistantCanopyThinning,0.f,.5f):0);
 }
}

void ANeonCityReferenceVegetation::UpdateObserver()
{
 // The canonical source has one main observer. Shadow cameras may orient the
 // quad, but must not independently select its near/far representation.
 FVector Eye=FVector::ZeroVector,Focus=FVector::ZeroVector;bool Found=false;APlayerController* Observer=nullptr;
 for(auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
  if(auto* PC=It->Get();PC && PC->IsLocalController())
  {FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);Focus=PC->GetFocalLocation();Found=!Eye.ContainsNaN() && !Focus.ContainsNaN();if(Found){Observer=PC;break;}}
 if(Observer)Grass->UpdateObserver(Eye,Observer);
 for(auto Material:Materials)if(IsValid(Material))
 {
  Material->SetScalarParameterValue(TEXT("UseGameplayLODView"),Found?1:0);
  if(Found)
  {
   Material->SetVectorParameterValue(TEXT("VegetationViewerCm"),FLinearColor(Eye.X,Eye.Y,Eye.Z,0));
   Material->SetVectorParameterValue(TEXT("VegetationFocusCm"),FLinearColor(Focus.X,Focus.Y,Focus.Z,0));
  }
 }
}

int32 ANeonCityReferenceVegetation::GetPlantCount() const
{
 int32 Count=0;for(auto Component:PlantGroups)if(IsValid(Component))Count+=Component->GetInstanceCount();return Count;
}

int32 ANeonCityReferenceVegetation::GetImpostorCount() const
{
 int32 Count=0;for(auto Component:ImpostorGroups)if(IsValid(Component))Count+=Component->GetInstanceCount();return Count;
}

int32 ANeonCityReferenceVegetation::GetFarPalmCount() const
{
 int32 Count=0;for(auto Component:FarPalmGroups)if(IsValid(Component))Count+=Component->GetInstanceCount();return Count;
}
