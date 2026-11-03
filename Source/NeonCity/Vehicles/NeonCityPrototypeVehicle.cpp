#include "Vehicles/NeonCityPrototypeVehicle.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Vehicles/VehicleData.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
ANeonCityPrototypeVehicle::ANeonCityPrototypeVehicle() { bStartsOwned=false; }
void ANeonCityPrototypeVehicle::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); ApplyBodyStyle(); }
void ANeonCityPrototypeVehicle::BeginPlay() { Super::BeginPlay(); ApplyBodyStyle(); }
void ANeonCityPrototypeVehicle::ApplyBodyStyle()
{
 // The new fitted fleet uses measured wheels and a complete cabin; legacy
 // utility styling must not replace those wheels after the native fit.
 if(FindObject<UStaticMeshComponent>(this,TEXT("SedanHeroBody916")))return;
 const auto Type=GetVehicleProfile()->VehicleType;
 TInlineComponentArray<UStaticMeshComponent*> Parts(this);
 const TCHAR* ArtStyle=Type==ENeonCityVehicleType::SportsCar?TEXT("Sports"):Type==ENeonCityVehicleType::SUV?TEXT("SUV"):Type==ENeonCityVehicleType::Pickup?TEXT("Pickup"):TEXT("Sedan");
 for(auto* ArtPart:Parts)
 {
  if(ArtPart->GetFName()==TEXT("ArtVisual")) NeonCityVisuals::Apply(ArtPart,TEXT("Vehicles"),ArtStyle);
  if(ArtPart->GetName().StartsWith(TEXT("SedanWheel"))) NeonCityVisuals::Apply(ArtPart,TEXT("Vehicles"),Type==ENeonCityVehicleType::SportsCar?TEXT("WheelSport"):TEXT("WheelUtility"));
 }
 auto Part=[&](FName Name,FVector Position,FVector Size)
 {
  for(auto* PartMesh:Parts) if(PartMesh->GetFName()==Name) { PartMesh->SetRelativeLocation(Position-FVector(0,0,40)); PartMesh->SetRelativeScale3D(Size/100); }
 };
 // Compact placeholders share wheelbase/collision; these are not finished production car meshes.
 Part(TEXT("SedanTrunk"),FVector(-177,0,100),FVector(100,176,20));
 Part(TEXT("SedanCabinGlass"),FVector(-20,0,130),FVector(232,164,60));
 Part(TEXT("SedanRoof"),FVector(-30,0,164),FVector(205,168,10));
 if(Type==ENeonCityVehicleType::SportsCar)
 {
  Part(TEXT("SedanCabinGlass"),FVector(-30,0,120),FVector(200,156,40));
  Part(TEXT("SedanRoof"),FVector(-35,0,145),FVector(165,160,10));
 }
 else if(Type==ENeonCityVehicleType::SUV)
 {
  Part(TEXT("SedanCabinGlass"),FVector(-50,0,130),FVector(292,164,60));
  Part(TEXT("SedanRoof"),FVector(-55,0,164),FVector(280,168,10));
 }
 else if(Type==ENeonCityVehicleType::Pickup)
 {
  Part(TEXT("SedanCabinGlass"),FVector(25,0,130),FVector(135,164,60));
  Part(TEXT("SedanRoof"),FVector(20,0,164),FVector(140,168,10));
  Part(TEXT("SedanTrunk"),FVector(-157,0,93),FVector(140,176,9));
 }
 const TCHAR* MaterialPath=Type==ENeonCityVehicleType::SportsCar?TEXT("/Game/NeonCity/Environment/Materials/M_SB_Coral.M_SB_Coral"):
  Type==ENeonCityVehicleType::SUV?TEXT("/Game/NeonCity/Environment/Materials/M_SB_Ivory.M_SB_Ivory"):TEXT("/Game/NeonCity/Environment/Materials/M_SB_Teal.M_SB_Teal");
 auto* Paint=LoadObject<UMaterialInterface>(nullptr,MaterialPath);
 for(auto* PartMesh:Parts)
  if(PartMesh->GetFName()==TEXT("SedanLowerBody") || PartMesh->GetFName()==TEXT("SedanHood") || PartMesh->GetFName()==TEXT("SedanRoof") || PartMesh->GetFName()==TEXT("SedanTrunk")) PartMesh->SetMaterial(0,Paint);
}
