#include "Presentation/NeonCityVisualAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

bool NeonCityVisuals::Apply(UStaticMeshComponent* Component, const TCHAR* Category, const TCHAR* Name)
{
 if(!Component) return false;
 FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/FullArt/%s/SM_NC205B_%s.SM_NC205B_%s"),Category,Name,Name);
 const bool Cockpit=(FCString::Strcmp(Category,TEXT("Vehicles"))==0 && (FString(Name)==TEXT("Sedan") || FString(Name)==TEXT("Sports") || FString(Name)==TEXT("SUV") || FString(Name)==TEXT("Pickup") || FString(Name)==TEXT("Police")))
  || (FCString::Strcmp(Category,TEXT("Marine"))==0 && FCString::Strcmp(Name,TEXT("Speedboat"))==0)
  || (FCString::Strcmp(Category,TEXT("Aircraft"))==0 && (FCString::Strcmp(Name,TEXT("Helicopter"))==0 || FCString::Strcmp(Name,TEXT("FixedWing"))==0));
 const FString Revised=FString::Printf(TEXT("/Game/NeonCity/Art/VerticalSlice/Transport/SM_NC205C_%s"),Name);
 if(Cockpit && FPackageName::DoesPackageExist(Revised))
 { Path=Revised+FString::Printf(TEXT(".SM_NC205C_%s"),Name);Component->ComponentTags.AddUnique(TEXT("NeonCity.Visual205C")); }
 // Player helicopter exterior rebuild (Tools/Art/Phase205cUpgrade/Transport/heli_205d.py): airframe and both rotors.
 const FString Rebuilt=FString::Printf(TEXT("/Game/NeonCity/Art/VerticalSlice/Transport/SM_NC205D_%s"),Name);
 if(FCString::Strcmp(Category,TEXT("Aircraft"))==0 && (FCString::Strcmp(Name,TEXT("Helicopter"))==0 || FCString::Strcmp(Name,TEXT("HelicopterRotor"))==0
  || FCString::Strcmp(Name,TEXT("TailRotor"))==0) && FPackageName::DoesPackageExist(Rebuilt))
 { Path=Rebuilt+FString::Printf(TEXT(".SM_NC205D_%s"),Name);Component->ComponentTags.AddUnique(TEXT("NeonCity.Visual205C")); }
 const FString Pistol=TEXT("/Game/NeonCity/Art/VerticalSlice/Weapons/SM_NC205C_Pistol");
 if(FCString::Strcmp(Category,TEXT("Weapons"))==0 && FCString::Strcmp(Name,TEXT("Pistol"))==0 && FPackageName::DoesPackageExist(Pistol))
 { Path=Pistol+TEXT(".SM_NC205C_Pistol");Component->ComponentTags.AddUnique(TEXT("NeonCity.Visual205C")); }
 auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
 if(!Mesh) return false;
 Component->SetStaticMesh(Mesh);
 Component->EmptyOverrideMaterials();
 Component->SetRelativeScale3D(FVector::OneVector);
 Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Component->SetGenerateOverlapEvents(false);
 Component->CanCharacterStepUpOn=ECB_No;
 Component->ComponentTags.AddUnique(TEXT("NeonCity.Visual205B"));
 return true;
}

UStaticMeshComponent* NeonCityVisuals::Create(AActor& Owner,USceneComponent* Parent,const TCHAR* Category,
 const TCHAR* Name,const TArray<FName>& ReplacedParts,FName ComponentName)
{
 auto* Visual=Owner.CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
 Visual->SetupAttachment(Parent);
 Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if(Apply(Visual,Category,Name))
 {
  TInlineComponentArray<UStaticMeshComponent*> Components(&Owner);
  for(auto* Part:Components) if(ReplacedParts.Contains(Part->GetFName()))
  { Part->SetVisibility(false); Part->SetHiddenInGame(true); }
 }
 return Visual;
}
