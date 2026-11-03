#include "ReferenceVillageLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "World/Marine/ReferenceVillageGeometry.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

bool UReferenceVillageLibrary::WriteReferenceBakePixels(UTextureRenderTarget2D* Target,const FString& Filename)
{
 if(!IsInGameThread() || !Target || Target->SizeX<1 || Target->SizeY<1 || Target->SizeX>2048 || Target->SizeY>2048) return false;
 FString Path=FPaths::ConvertRelativePathToFull(Filename);
 FPaths::NormalizeFilename(Path);if(!FPaths::CollapseRelativeDirectories(Path))return false;
 FString Base=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VerticalSlice/"));FPaths::NormalizeFilename(Base);
 if(!Path.StartsWith(Base,ESearchCase::IgnoreCase) || !Path.EndsWith(TEXT(".f32le")) || IFileManager::Get().FileExists(*Path))return false;
 FlushRenderingCommands();
 FTextureRenderTargetResource* Resource=Target->GameThread_GetRenderTargetResource();
 if(!Resource)return false;
 TArray<FLinearColor> Pixels;
 FReadSurfaceDataFlags Flags(RCM_MinMax);Flags.SetLinearToGamma(false);
 if(!Resource->ReadLinearColorPixels(Pixels,Flags) || Pixels.Num()!=Target->SizeX*Target->SizeY)return false;
 for(const auto& P:Pixels)if(!FMath::IsFinite(P.R)||!FMath::IsFinite(P.G)||!FMath::IsFinite(P.B)||!FMath::IsFinite(P.A))return false;
 static_assert(sizeof(FLinearColor)==4*sizeof(float),"Expected RGBA float32 layout");
 // Win64 is the current authoring target; format is explicitly little-endian.
 const TArrayView<const uint8> Bytes(reinterpret_cast<const uint8*>(Pixels.GetData()),Pixels.Num()*sizeof(FLinearColor));
 return IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true) && FFileHelper::SaveArrayToFile(Bytes,*Path);
}

TArray<FString> UReferenceVillageLibrary::CheckReferenceVillageSupports(ANeonCityReferenceVillage* Village)
{
 TArray<FString> Errors;
 if(!Village || !Village->GetWorld() || Village->GetWorld()->WorldType!=EWorldType::Editor){Errors.Add(TEXT("Missing editor village"));return Errors;}
 int32 Probes=0;
 for(int32 I=0;I<UE_ARRAY_COUNT(NeonCityReferenceVillage::Boxes);++I)
 {
  const auto& Box=NeonCityReferenceVillage::Boxes[I];if(!Box.Walkable)continue;
  if(!Box.Solid)
  {
   const FName Name(*FString::Printf(TEXT("VillageContact_%d"),I));
   TInlineComponentArray<UBoxComponent*> Contacts(Village);
   const auto* Contact=Contacts.FindByPredicate([&](const auto* C){return C->GetFName()==Name;});
   if(!Contact || (*Contact)->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
    Errors.Add(FString::Printf(TEXT("Virtual ladder support %d must not become a solid wall"),I));
   continue;
  }
  const FVector Expected=Village->GetActorTransform().TransformPosition(Box.Center+FVector(0,0,Box.Half.Z));
  FHitResult Hit;const bool Found=Village->GetWorld()->LineTraceSingleByChannel(Hit,Expected+FVector(0,0,2),Expected-FVector(0,0,12),ECC_Visibility);
  const UPrimitiveComponent* Part=Hit.GetComponent();
  if(!Found || Hit.GetActor()!=Village || !Part || Part->CanCharacterStepUpOn!=ECB_Yes || FMath::Abs(Hit.ImpactPoint.Z-Expected.Z)>1 || Hit.ImpactNormal.Z<.99)
   Errors.Add(FString::Printf(TEXT("Support %d: expected %s, found %s, height error %.3f"),I,*Expected.ToString(),*GetNameSafe(Hit.GetComponent()),Hit.ImpactPoint.Z-Expected.Z));
  ++Probes;
 }
 if(Probes==0)Errors.Add(TEXT("No source support probes"));
 return Errors;
}
