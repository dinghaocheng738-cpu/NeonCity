#include "Physics/NeonCityPhysicsProp.h"
#include "Combat/HealthComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Physics/PhysicalMaterialResponseData.h"
#include "Physics/NeonCityPhysicalSurfaces.h"
#include "Destruction/DestructibleStateComponent.h"
#include "Explosion/ExplosionEvent.h"
#include "Engine/World.h"
#include "Combat/CombatCollision.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
namespace
{
 ENeonCitySurfaceKind SurfaceFor(ECityPhysicsProp Type)
 {
  switch(Type)
  {
  case ECityPhysicsProp::Glass: return ENeonCitySurfaceKind::Glass;
  case ECityPhysicsProp::StorefrontGlass: return ENeonCitySurfaceKind::StorefrontGlass;
  case ECityPhysicsProp::Door: return ENeonCitySurfaceKind::Door;
  case ECityPhysicsProp::Sign: return ENeonCitySurfaceKind::Sign;
  case ECityPhysicsProp::LampPost: return ENeonCitySurfaceKind::StreetLight;
  case ECityPhysicsProp::Billboard: return ENeonCitySurfaceKind::Billboard;
  case ECityPhysicsProp::RoadBarrier: return ENeonCitySurfaceKind::Barrier;
  case ECityPhysicsProp::Fence: return ENeonCitySurfaceKind::Fence;
  case ECityPhysicsProp::TrashCan: return ENeonCitySurfaceKind::TrashCan;
  default: return ENeonCitySurfaceKind::Concrete;
  }
 }
}
ANeonCityPhysicsProp::ANeonCityPhysicsProp()
{
 PrimaryActorTick.bCanEverTick=false;
 Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropBody")); SetRootComponent(Mesh);
 Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic")); Mesh->SetCollisionResponseToChannel(NeonCityCombat::WeaponTrace,ECR_Block);
 Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp")); Light->SetupAttachment(Mesh); Light->SetIntensity(3000); Light->SetAttenuationRadius(550); Light->SetVisibility(false); Light->SetCastShadows(false);
 Health=CreateDefaultSubobject<UHealthComponent>(TEXT("Health")); Destruction=CreateDefaultSubobject<UDestructiblePropComponent>(TEXT("Destruction"));
 Surface=CreateDefaultSubobject<UDestructibleStateComponent>(TEXT("SurfaceState"));
 ArtSurface=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtSurface")); ArtSurface->SetupAttachment(Mesh);
 ArtSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision); ArtSurface->SetAbsolute(false,false,true);
 Surface->OnStateChanged.AddDynamic(this,&ANeonCityPhysicsProp::UpdateArtState);
}
void ANeonCityPhysicsProp::Configure(ECityPhysicsProp Type)
{
 PropType=Type;
 const bool Round=Type==ECityPhysicsProp::TrashCan || Type==ECityPhysicsProp::LampPost;
 const TCHAR* Shape=Type==ECityPhysicsProp::RoadCone?TEXT("/Engine/BasicShapes/Cone.Cone"):Round?TEXT("/Engine/BasicShapes/Cylinder.Cylinder"):TEXT("/Engine/BasicShapes/Cube.Cube");
 Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Shape));
 const FVector Sizes[]={FVector(45,45,65),FVector(65,65,95),FVector(40,180,75),FVector(12,100,190),FVector(20,20,320),FVector(15,210,110),FVector(8,160,150),FVector(60,240,200),
  FVector(12,320,200),FVector(14,110,220),FVector(8,240,180),FVector(30,320,240)};
 const float Masses[]={6,35,65,35,100,55,25,300,80,60,30,320};
 const int32 Index=FMath::Clamp(int32(Type),0,11);
 Mesh->SetRelativeScale3D(Sizes[Index]/100); Destruction->Mass=Masses[Index];
 Destruction->Mode=Index<=2?EPhysicsPropMode::Movable:Type==ECityPhysicsProp::StaticBlock?EPhysicsPropMode::Static:EPhysicsPropMode::Breakable;
 Destruction->bGlass=Type==ECityPhysicsProp::Glass || Type==ECityPhysicsProp::StorefrontGlass;
 const TCHAR* Paint=Type==ECityPhysicsProp::RoadCone||Type==ECityPhysicsProp::Billboard?TEXT("/Game/NeonCity/Environment/Materials/M_SB_Coral.M_SB_Coral"):TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal");
 Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Paint));
 // The unified surface profile carries the break policy; the physics adapter keeps its released 30% rule.
 Surface->SurfaceKind=SurfaceFor(Type);
 Surface->SurfaceProfile=nullptr;
 if(Destruction->bGlass) Surface->CrackedMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Environment/Materials/M_SB_Coral.M_SB_Coral"));
 Material=NewObject<UPhysicalMaterial>(this); Material->SurfaceType=Destruction->bGlass?NeonCityPhysicalSurfaces::Glass:Index<=2?NeonCityPhysicalSurfaces::Metal:NeonCityPhysicalSurfaces::Concrete;
 const auto Response=GetWorld()->GetSubsystem<UPhysicalImpactSubsystem>()->Materials->Get(Material->SurfaceType);
 Material->Friction=Response.Friction; Material->Restitution=Response.Restitution;
 Mesh->SetPhysMaterialOverride(Material); Light->SetVisibility(Type==ECityPhysicsProp::LampPost); Light->SetRelativeLocation(FVector(0,0,50));
 const TCHAR* Art=Type==ECityPhysicsProp::LampPost?TEXT("/Game/NeonCity/Art/SouthBeach/Props/SM_SB205_StreetLamp.SM_SB205_StreetLamp"):
  Type==ECityPhysicsProp::TrashCan?TEXT("/Game/NeonCity/Art/SouthBeach/Props/SM_SB205_LitterBin.SM_SB205_LitterBin"):nullptr;
 if(auto* ShapeArt=Art?LoadObject<UStaticMesh>(nullptr,Art):nullptr)
 {
  ArtSurface->SetStaticMesh(ShapeArt); const FVector Dimensions=ShapeArt->GetBounds().BoxExtent*2;
  // Existing primitive remains the simulated body. Art follows it without adding
  // a second body, mass or contact; scale matches the existing released fixture.
  const float HeightScale=Sizes[Index].Z/Dimensions.Z;
  ArtSurface->SetRelativeScale3D(FVector(HeightScale));
  ArtSurface->SetRelativeLocation(FVector(0,0,-50)); ArtSurface->SetVisibility(true);
  Mesh->SetHiddenInGame(true,false); Mesh->SetCastShadow(false);
 }
 else { ArtSurface->SetStaticMesh(nullptr); Mesh->SetHiddenInGame(false,false); }
}
void ANeonCityPhysicsProp::UpdateArtState(EPhysicsPropState Previous,EPhysicsPropState Current)
{
 if(!ArtSurface || !ArtSurface->GetStaticMesh()) return;
 const auto& Profile=Surface->GetSurface();
 ArtSurface->SetVisibility(Current==EPhysicsPropState::Normal || (Current==EPhysicsPropState::Damaged?!Profile.bHiddenWhenCracked:!Profile.bHiddenWhenBroken));
}
void ANeonCityPhysicsProp::SetSurfaceProfile(UBreakableSurfaceData* Profile)
{ if(Profile && Profile->IsValidProfile()) Surface->SurfaceProfile=Profile; }
bool ANeonCityPhysicsProp::ReceiveExplosion_Implementation(const FNeonCityExplosionEvent& Event)
{
 if(!Surface || !Destruction || Destruction->Mode==EPhysicsPropMode::Static) return false;
 const bool bBroke=Surface->ApplyExplosion(Event);
 if(Surface->GetState()==EPhysicsPropState::Broken) Destruction->ApplyExternalState(EPhysicsPropState::Broken);
 return bBroke || Surface->GetState()!=EPhysicsPropState::Normal;
}
