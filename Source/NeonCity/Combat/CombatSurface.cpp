#include "Combat/CombatSurface.h"
#include "Physics/NeonCityPhysicalSurfaces.h"
#include "Physics/DestructiblePropComponent.h"
#include "Vehicles/VehicleDamageComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

ENeonCityImpactSurface NeonCityCombat::ResolveImpactSurface(const FHitResult& Hit)
{
 const auto* Actor=Hit.GetActor();
 if(Cast<ACharacter>(Actor))return ENeonCityImpactSurface::Character;
 const UPhysicalMaterial* Material=Hit.PhysMaterial.Get();
 // Some simple/complex collision paths omit the material in FHitResult.
 if(!Material || Material->SurfaceType==SurfaceType_Default)
  if(auto* Component=Hit.GetComponent())
   if(auto* Body=Component->GetBodyInstance(Hit.BoneName))Material=Body->GetSimplePhysicalMaterial();
 if(Material)
 {
  switch(Material->SurfaceType)
  {
   case NeonCityPhysicalSurfaces::Wood:return ENeonCityImpactSurface::Wood;
   case NeonCityPhysicalSurfaces::Metal:return ENeonCityImpactSurface::Metal;
   case NeonCityPhysicalSurfaces::Glass:return ENeonCityImpactSurface::Glass;
   case NeonCityPhysicalSurfaces::Concrete:return ENeonCityImpactSurface::Concrete;
   default:break;
  }
 }
 if(Actor)
 {
  if(auto* Prop=Actor->FindComponentByClass<UDestructiblePropComponent>();Prop && Prop->bGlass)return ENeonCityImpactSurface::Glass;
  if(Actor->FindComponentByClass<UVehicleDamageComponent>() || Actor->ActorHasTag(TEXT("Surface.Metal")))return ENeonCityImpactSurface::Metal;
 }
 return ENeonCityImpactSurface::Concrete;
}
