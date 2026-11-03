#include "Combat/Weapons/NeonCityPistol.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
ANeonCityPistol::ANeonCityPistol()
{
	DataAsset=FSoftObjectPath(TEXT("/Game/NeonCity/Combat/DA_NeonCityPistol.DA_NeonCityPistol"));
	Muzzle->SetRelativeLocation(FVector(35,0,8));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dark(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Glass.M_SB_Glass"));
	auto* Slide=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PistolSlide"));
	Slide->SetupAttachment(GetRootComponent()); Slide->SetStaticMesh(Cube.Object); Slide->SetMaterial(0,Dark.Object);
	Slide->SetRelativeLocation(FVector(17,0,8)); Slide->SetRelativeScale3D(FVector(.34,.06,.08)); Slide->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	auto* Grip=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PistolGrip"));
	Grip->SetupAttachment(GetRootComponent()); Grip->SetStaticMesh(Cube.Object); Grip->SetMaterial(0,Dark.Object);
	Grip->SetRelativeLocation(FVector(3,0,0)); Grip->SetRelativeScale3D(FVector(.09,.065,.16)); Grip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 auto* Visual=NeonCityVisuals::Create(*this,GetRootComponent(),TEXT("Weapons"),TEXT("Pistol"),{TEXT("PistolSlide"),TEXT("PistolGrip")});
 // Presentation and traces share the authored muzzle. Derived long guns still
 // apply their own model and muzzle after this base constructor.
 if(Visual && Visual->DoesSocketExist(TEXT("Muzzle")))
  Muzzle->SetRelativeLocation(Visual->GetSocketTransform(TEXT("Muzzle"),RTS_Component).GetLocation());
}
