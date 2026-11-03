#include "OceanLiveReviewLibrary.h"
#include "Editor.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Water/SwimmingAnimInstance.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace {
FTSTicker::FDelegateHandle Recording;
FString Folder,Run;
double Started=0;
int32 Frame=0;
bool Record(float)
{
 auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;
 auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 auto* Client=W?W->GetGameViewport():nullptr;
 if(!P || !Client || !Client->Viewport || FPlatformTime::Seconds()-Started>900)return false;
 TArray<FColor> Pixels;if(!Client->Viewport->ReadPixels(Pixels))return true;
 const FIntPoint Size=Client->Viewport->GetSizeXY();for(auto& Pixel:Pixels)Pixel.A=255;
 TArray64<uint8> Data;
 if(!FImageUtils::CompressImage(Data,TEXT("jpg"),FImageView(Pixels.GetData(),Size.X,Size.Y),94))return true;
 const FString Name=FString::Printf(TEXT("%s-Live-%05d.jpg"),*Run,Frame);
 FFileHelper::SaveArrayToFile(Data,*(Folder/Name));
 const auto* A=Cast<USwimmingAnimInstance>(P->GetMesh()->GetAnimInstance());const auto* E=P->FindComponentByClass<UDiveEquipmentComponent>();
 const FVector At=P->GetActorLocation();
 const FString Row=FString::Printf(TEXT("%d,%.4f,%.4f,%d,%s,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"),
  Frame++,W->GetTimeSeconds(),FPlatformTime::Seconds()-Started,A?A->OceanPose:-1,*GetNameSafe(P),int32(P->GetCharacterMovement()->MovementMode),At.X,At.Y,At.Z,P->GetVelocity().Size(),
  E && E->IsSpeargunDeployed(),E && E->IsDPVDeployed(),PC->IsInputKeyDown(EKeys::W),PC->IsInputKeyDown(EKeys::A),PC->IsInputKeyDown(EKeys::S),PC->IsInputKeyDown(EKeys::D),PC->IsInputKeyDown(EKeys::SpaceBar),PC->IsInputKeyDown(EKeys::C),PC->IsInputKeyDown(EKeys::LeftMouseButton),Size.X,Size.Y);
 FFileHelper::SaveStringToFile(Row,*(Folder/TEXT("frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
 return true;
}
}
bool UOceanLiveReviewLibrary::StartRecording(const FString& Label)
{
 StopRecording();if(!GEditor || !GEditor->PlayWorld)return false;
 Run=FPaths::MakeValidFileName(Label);if(Run.IsEmpty())return false;
 Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("Motion/Live");
 if(IFileManager::Get().DirectoryExists(*Folder))return false;
 IFileManager::Get().MakeDirectory(*Folder,true);
 FFileHelper::SaveStringToFile(TEXT("frame,game_seconds,wall_seconds,stage,pawn,movement_mode,x,y,z,speed,spear,dpv,w,a,s,d,rise,dive,trigger,width,height\n"),*(Folder/TEXT("frames.csv")));
 Frame=0;Started=FPlatformTime::Seconds();Recording=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Record),.10f);return true;
}
void UOceanLiveReviewLibrary::StopRecording()
{
 if(Recording.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Recording);Recording.Reset();}
}
bool UOceanLiveReviewLibrary::HoldGameKey(FName KeyName,float Seconds)
{
 const FKey Key(KeyName);
 const TArray<FKey> Allowed={EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::SpaceBar,EKeys::C,EKeys::LeftMouseButton,EKeys::RightMouseButton,EKeys::R};
 auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
 if(!PC || !Allowed.Contains(Key) || Seconds<=0 || Seconds>12)return false;
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,IE_Pressed,FPlatformTime::Cycles64()));
 TWeakObjectPtr<APlayerController> WeakPC=PC;
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC,Key](float)
 {
  if(auto* Controller=WeakPC.Get())Controller->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,IE_Released,FPlatformTime::Cycles64()));
  return false;
 }),Seconds);
 return true;
}
