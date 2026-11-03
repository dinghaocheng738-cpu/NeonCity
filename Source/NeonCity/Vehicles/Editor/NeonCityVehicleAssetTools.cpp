#include "Vehicles/Editor/NeonCityVehicleAssetTools.h"
#include "Adventure/IslandSupercar.h"
#include "Adventure/IslandSupercarPresentationData.h"
#include "Engine/World.h"

bool UNeonCityVehicleAssetTools::RebuildAuthoredVehicle(AActor* Actor)
{
#if WITH_EDITOR
 if(!IsValid(Actor) || !Actor->IsA<AIslandSupercar>() || !Actor->GetWorld() || Actor->GetWorld()->WorldType!=EWorldType::Editor)return false;
 Actor->RerunConstructionScripts();
 return true;
#else
 return false;
#endif
}

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

bool UNeonCityVehicleAssetTools::BuildIslandSupercarPhysics(bool Roadster)
{
#if WITH_EDITOR
 const FString Name=Roadster?TEXT("PA_HeroKestrel"):TEXT("PA_HeroAster");
 const FString PackageName=TEXT("/Game/NeonCity/Art/Polish/Adventure/Supercars/")+Name;
 auto* Source=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/NeonCity/Vehicles/PA_StarterSedan"));if(!Source)return false;
 const bool NewAsset=!FPackageName::DoesPackageExist(PackageName);
 auto* Asset=NewAsset?DuplicateObject<UPhysicsAsset>(Source,CreatePackage(*PackageName),*Name):LoadObject<UPhysicsAsset>(nullptr,*PackageName);if(!Asset)return false;
 const int32 Index=Asset->FindBodyIndex(TEXT("Root"));if(!Asset->SkeletalBodySetups.IsValidIndex(Index))return false;
 auto* Body=Asset->SkeletalBodySetups[Index].Get();Asset->Modify();Body->Modify();Body->AggGeom.EmptyElements();
 FKBoxElem Lower(Roadster?438.f:449.f,185.f,35.f);Lower.Center=FVector(Roadster?-8:15,0,3);
 FKBoxElem Cabin(Roadster?190.f:235.f,152.f,Roadster?48.f:55.f);Cabin.Center=FVector(Roadster?-13:-9,0,43);
 Body->AggGeom.BoxElems.Add(Lower);Body->AggGeom.BoxElems.Add(Cabin);
 Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Asset->SetFlags(RF_Public|RF_Standalone);Asset->MarkPackageDirty();
 if(NewAsset)FAssetRegistryModule::AssetCreated(Asset);
 const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;return UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args);
#else
 return false;
#endif
}

bool UNeonCityVehicleAssetTools::BuildNacrePhysics()
{
#if WITH_EDITOR
 const FString PackageName=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Nacre/PA_HeroNacre");
 if(FPackageName::DoesPackageExist(PackageName))return false;
 auto* Source=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/NeonCity/Vehicles/PA_StarterSedan"));if(!Source)return false;
 auto* Asset=DuplicateObject<UPhysicsAsset>(Source,CreatePackage(*PackageName),TEXT("PA_HeroNacre"));
 const int32 Index=Asset->FindBodyIndex(TEXT("Root"));if(!Asset->SkeletalBodySetups.IsValidIndex(Index))return false;
 auto* Body=Asset->SkeletalBodySetups[Index].Get();Body->AggGeom.EmptyElements();
 FKBoxElem Lower(462,188,35);Lower.Center=FVector(0,0,3);
 FKBoxElem Cabin(180,146,72);Cabin.Center=FVector(0,0,61);
 Body->AggGeom.BoxElems.Add(Lower);Body->AggGeom.BoxElems.Add(Cabin);
 Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Asset->SetFlags(RF_Public|RF_Standalone);Asset->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Asset);
 const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;return UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args);
#else
 return false;
#endif
}

bool UNeonCityVehicleAssetTools::BuildReplicaChassis(UIslandSupercarPresentationData* Presentation)
{
#if WITH_EDITOR
 if(!Presentation || Presentation->CollisionBoxes.IsEmpty())return false;
 const bool MainlandCoupe=Presentation->GetOutermost()->GetName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland914/Cars/DA_Carrera914Presentation");
 const bool MainlandGT=Presentation->GetOutermost()->GetName()==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland916/Fleet/DA_GrandTourer916Presentation");
 const FString PackageName=Presentation->Chassis.IsNull()
  ? FPackageName::GetLongPackagePath(Presentation->GetOutermost()->GetName())+(MainlandCoupe?TEXT("/PA_Carrera914"):MainlandGT?TEXT("/PA_GrandTourer916Presentation"):TEXT("/PA_720SSpider"))
  : Presentation->Chassis.ToSoftObjectPath().GetLongPackageName();
 const bool OwnedDestination=PackageName.StartsWith(TEXT("/Game/NeonCity/Art/Phase205cUpgrade/McLaren720S321/Assembly393/"))
  || PackageName==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland914/Cars/PA_Carrera914")
  || (MainlandGT && PackageName==TEXT("/Game/NeonCity/Art/Phase205cUpgrade/Mainland916/Fleet/PA_GrandTourer916Presentation"));
 if(!OwnedDestination || FPackageName::DoesPackageExist(PackageName))return false;
 for(const auto& Box:Presentation->CollisionBoxes)
  if(Box.CentreCm.ContainsNaN() || Box.DimensionsCm.ContainsNaN() || Box.DimensionsCm.GetMin()<=0)return false;
 auto* Source=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/NeonCity/Vehicles/PA_StarterSedan"));if(!Source)return false;
 auto* Asset=DuplicateObject<UPhysicsAsset>(Source,CreatePackage(*PackageName),*FPackageName::GetLongPackageAssetName(PackageName));
 const int32 Index=Asset->FindBodyIndex(TEXT("Root"));if(!Asset->SkeletalBodySetups.IsValidIndex(Index))return false;
 auto* Body=Asset->SkeletalBodySetups[Index].Get();Body->AggGeom.EmptyElements();
 for(const auto& Box:Presentation->CollisionBoxes)
 {FKBoxElem Shape(Box.DimensionsCm.X,Box.DimensionsCm.Y,Box.DimensionsCm.Z);Shape.Center=Box.CentreCm;Body->AggGeom.BoxElems.Add(Shape);}
 Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Asset->SetFlags(RF_Public|RF_Standalone);Asset->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Asset);
 Presentation->Chassis=Asset;Presentation->MarkPackageDirty();
 const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;return UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args);
#else
 return false;
#endif
}

bool UNeonCityVehicleAssetTools::BuildIslandOffroadPhysics(bool Pickup)
{
#if WITH_EDITOR
 const FString Name=Pickup?TEXT("PA_ExpeditionPickup"):TEXT("PA_ExpeditionSUV");
 const FString PackageName=TEXT("/Game/NeonCity/Art/Polish/Adventure/Offroad/")+Name;
 auto* Source=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/NeonCity/Vehicles/PA_StarterSedan"));
 if(!Source)return false;
 const bool NewAsset=!FPackageName::DoesPackageExist(PackageName);
 auto* Asset=NewAsset?DuplicateObject<UPhysicsAsset>(Source,CreatePackage(*PackageName),*Name):LoadObject<UPhysicsAsset>(nullptr,*PackageName);
 if(!Asset)return false;
 const int32 Index=Asset->FindBodyIndex(TEXT("Root"));
 if(!Asset->SkeletalBodySetups.IsValidIndex(Index))return false;
 auto* Body=Asset->SkeletalBodySetups[Index].Get();Asset->Modify();Body->Modify();Body->AggGeom.EmptyElements();
 FKBoxElem Lower(Pickup?517.f:385.f,Pickup?182.f:185.f,53.f);
 Lower.Center=FVector(Pickup?-18.f:-1.f,0,48);
 FKBoxElem Cabin(Pickup?242.f:242.f,Pickup?165.f:161.f,Pickup?78.f:93.f);
 Cabin.Center=FVector(Pickup?13.f:-48.f,0,Pickup?111.f:122.f);
 Body->AggGeom.BoxElems.Add(Lower);Body->AggGeom.BoxElems.Add(Cabin);
 Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Asset->SetFlags(RF_Public|RF_Standalone);Asset->MarkPackageDirty();
 if(NewAsset)FAssetRegistryModule::AssetCreated(Asset);
 const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 return UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Args);
#else
 return false;
#endif
}

bool UNeonCityVehicleAssetTools::BuildStarterSedanPhysics()
{
#if WITH_EDITOR
	const TCHAR* SourcePath = TEXT("/Game/Vehicles/SportsCar/PA_SportsCar.PA_SportsCar");
	const FString PackageName = TEXT("/Game/NeonCity/Vehicles/PA_StarterSedan");
	const FString ObjectPath = PackageName + TEXT(".PA_StarterSedan");
	UPhysicsAsset* Source = LoadObject<UPhysicsAsset>(nullptr, SourcePath);
	if (!Source || Source->FindBodyIndex(TEXT("Root")) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("Starter Sedan physics: official source or Root body is missing."));
		return false;
	}

	const bool bNewAsset = !FPackageName::DoesPackageExist(PackageName);
	UPackage* Package = nullptr;
	UPhysicsAsset* Asset = nullptr;
	if (bNewAsset)
	{
		Package = CreatePackage(*PackageName);
		Asset = DuplicateObject<UPhysicsAsset>(Source, Package, TEXT("PA_StarterSedan"));
	}
	else
	{
		Asset = LoadObject<UPhysicsAsset>(nullptr, *ObjectPath);
		Package = Asset ? Asset->GetOutermost() : nullptr;
	}
	if (!Asset || !Package || Package->GetName() != PackageName)
	{
		UE_LOG(LogTemp, Error, TEXT("Starter Sedan physics: cannot create/load the project-owned asset."));
		return false;
	}
	const int32 RootIndex = Asset->FindBodyIndex(TEXT("Root"));
	if (!Asset->SkeletalBodySetups.IsValidIndex(RootIndex) || !Asset->SkeletalBodySetups[RootIndex])
	{
		UE_LOG(LogTemp, Error, TEXT("Starter Sedan physics: duplicate has no Root body."));
		return false;
	}

	USkeletalBodySetup* Root = Asset->SkeletalBodySetups[RootIndex];
	if (Root->GetOutermost() != Package) { return false; }
	Asset->SetFlags(RF_Public | RF_Standalone);
	Asset->Modify();
	Root->Modify();
	Root->AggGeom.EmptyElements();
	FKBoxElem Lower(460.f, 180.f, 60.f);
	Lower.Center = FVector(0.f, 0.f, 27.f);
	FKBoxElem Cabin(232.f, 164.f, 68.f);
	Cabin.Center = FVector(-20.f, 0.f, 90.f);
	Root->AggGeom.BoxElems.Add(Lower);
	Root->AggGeom.BoxElems.Add(Cabin);
	Root->InvalidatePhysicsData();
	Root->CreatePhysicsMeshes();
	Asset->MarkPackageDirty();
	if (bNewAsset) { FAssetRegistryModule::AssetCreated(Asset); }

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	UE_LOG(LogTemp, Display, TEXT("Starter Sedan physics saved=%d: %s; only Root geometry changed, wheel bodies and constraints retained."),
		bSaved ? 1 : 0, *ObjectPath);
	return bSaved;
#else
	return false;
#endif
}
