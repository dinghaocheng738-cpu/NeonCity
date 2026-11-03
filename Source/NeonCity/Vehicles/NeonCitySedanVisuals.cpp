#include "Vehicles/NeonCitySedanVisuals.h"
#include "Presentation/NeonCityVisualAssets.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandOffroadVehicle.h"

UIslandSupercarPresentationData* FNeonCitySedanVisuals::GetFleetPresentation()
{
 return LoadObject<UIslandSupercarPresentationData>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland916/Fleet/DA_GrandTourer916Presentation"),nullptr,LOAD_NoWarn);
}

TArray<TObjectPtr<UStaticMeshComponent>> FNeonCitySedanVisuals::Create(AActor& Owner, USceneComponent* Parent, const TCHAR* Style)
{
 TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
 if(auto* Profile=(Owner.IsA<AIslandSupercar>() || Owner.IsA<AIslandOffroadVehicle>())?nullptr:GetFleetPresentation();Profile && Profile->ResolveRequiredAssets())
 {
  auto Part=[&](FName Name,UStaticMesh* Mesh)
  {
   auto* C=Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(Parent);
   C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);return C;
  };
  auto* Body=Part(TEXT("SedanHeroBody916"),Profile->Body.Get());
  if(FCString::Strcmp(Style,TEXT("Police"))==0)
  {
   auto* Paint=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/NeonCity/Art/Polish/Adventure/Supercars/M_Hero_Titanium"));
   const auto& Slots=Body->GetStaticMesh()->GetStaticMaterials();
   for(int32 I=0;I<Slots.Num();++I)if(Slots[I].MaterialSlotName==TEXT("GT916_Paint"))Body->SetMaterial(I,Paint);
  }
  for(int32 I=0;I<4;++I)
  {
   const auto& Binding=Profile->Wheels[I];
   auto* W=Part(*FString::Printf(TEXT("SedanWheel%d"),I),Binding.Mesh.Get());
   W->SetRelativeLocationAndRotation(Binding.CentreCm,FRotator(0,0,Binding.AxleRollDegrees));
   auto* Caliper=Part(*FString::Printf(TEXT("SedanCaliper%d"),I),Binding.Caliper.Get());
   Caliper->SetRelativeLocationAndRotation(Binding.CentreCm,FRotator(0,0,Binding.AxleRollDegrees));
   Wheels.Add(W);
  }
  return Wheels;
 }
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Blue.M_SB_Blue"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glass(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Glass.M_SB_Glass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Trim(TEXT("/Game/NeonCity/Environment/Materials/M_SB_White.M_SB_White"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Coral.M_SB_Coral"));
	auto Part = [&Owner, Parent](const TCHAR* Name, FVector Location, FVector Size, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Mesh = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetMaterial(0, Material);
		Mesh->SetRelativeLocation(Location - FVector(0.f,0.f,40.f));
		Mesh->SetRelativeScale3D(Size / 100.f);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(true);
		return Mesh;
	};
	Part(TEXT("SedanLowerBody"), FVector(0,0,67), FVector(460,180,60), Paint.Object);
	Part(TEXT("SedanHood"), FVector(152,0,99), FVector(150,176,16), Paint.Object);
	Part(TEXT("SedanTrunk"), FVector(-177,0,100), FVector(100,176,20), Paint.Object);
	Part(TEXT("SedanCabinGlass"), FVector(-20,0,130), FVector(232,164,60), Glass.Object);
	Part(TEXT("SedanRoof"), FVector(-30,0,164), FVector(205,168,10), Paint.Object);
	for (int32 Side : {-1, 1})
	{
		Part(Side < 0 ? TEXT("LeftDoorPillar") : TEXT("RightDoorPillar"), FVector(-25,Side*84,132), FVector(9,5,62), Paint.Object);
		Part(Side < 0 ? TEXT("LeftDoorLine") : TEXT("RightDoorLine"), FVector(-25,Side*90.5,73), FVector(3,2,44), Glass.Object);
		Part(Side < 0 ? TEXT("LeftHandle") : TEXT("RightHandle"), FVector(-10,Side*92,91), FVector(18,3,3), Trim.Object);
		Part(Side < 0 ? TEXT("LeftMirror") : TEXT("RightMirror"), FVector(76,Side*99,116), FVector(22,18,12), Paint.Object);
		Part(Side < 0 ? TEXT("LeftHeadlight") : TEXT("RightHeadlight"), FVector(231,Side*58,84), FVector(3,43,13), Trim.Object);
		Part(Side < 0 ? TEXT("LeftTaillight") : TEXT("RightTaillight"), FVector(-231,Side*60,83), FVector(3,40,14), Red.Object);
	}
	Part(TEXT("FrontBumper"), FVector(234,0,53), FVector(10,174,13), Trim.Object);
	Part(TEXT("RearBumper"), FVector(-234,0,53), FVector(10,174,13), Trim.Object);
	Part(TEXT("FrontGrille"), FVector(231,0,73), FVector(3,54,21), Glass.Object);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FName Name(*FString::Printf(TEXT("SedanWheel%d"), Index));
		UStaticMeshComponent* Wheel = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Wheel->SetupAttachment(Parent);
		Wheel->SetStaticMesh(Cylinder.Object);
		Wheel->SetMaterial(0, Glass.Object);
		Wheel->SetRelativeScale3D(FVector(0.64f,0.64f,0.22f));
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheel->SetVisibility(true);
		UStaticMeshComponent* Hub = Owner.CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SedanHub%d"), Index));
		Hub->SetupAttachment(Wheel);
		Hub->SetStaticMesh(Cylinder.Object);
		Hub->SetMaterial(0, Trim.Object);
		Hub->SetRelativeScale3D(FVector(0.55f,0.55f,1.06f));
		Hub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheels.Add(Wheel);
	}
 TArray<FName> Replaced;
 TInlineComponentArray<UStaticMeshComponent*> Parts(&Owner);
 for(auto* LegacyPart:Parts)
  if(LegacyPart->GetAttachParent()==Parent && !Wheels.Contains(LegacyPart)) Replaced.Add(LegacyPart->GetFName());
 NeonCityVisuals::Create(Owner,Parent,TEXT("Vehicles"),Style,Replaced);
 for(int32 I=0;I<Wheels.Num();++I)
  if(NeonCityVisuals::Apply(Wheels[I],TEXT("Vehicles"),TEXT("WheelTouring")))
   for(auto* LegacyHub:Parts) if(LegacyHub->GetFName()==FName(*FString::Printf(TEXT("SedanHub%d"),I)))
   { LegacyHub->SetVisibility(false); LegacyHub->SetHiddenInGame(true); }
 return Wheels;
}
