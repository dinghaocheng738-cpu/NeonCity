#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "RenderingThread.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include <limits>
namespace NativeFlashlightChecks
{
FVector M(double X,double Y,double Z){return NeonCityCoastalLayout::FromLegacyIsland(FVector(48600+100*X,-118800-100*Y,30480+100*Z));}
TSharedPtr<FJsonValue> Vec(FVector V){TArray<TSharedPtr<FJsonValue>> A;for(double N:{V.X,V.Y,V.Z})A.Add(MakeShared<FJsonValueNumber>(N));return MakeShared<FJsonValueArray>(A);}
class FIllumination : public IAutomationLatentCommand
{
public:
 explicit FIllumination(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>100){Test->AddError(TEXT("Night illumination fixture timed out"));return Finish();}
  if(!W)
  {
   W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
   if(!P || W->GetTimeSeconds()<4){W=nullptr;return false;}
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(M(0,0,0))){W=nullptr;return false;}
   Light=P->FindComponentByClass<UNeonCityFlashlightComponent>();if(!Test->TestNotNull(TEXT("Real player's existing flashlight"),Light))return Finish();
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(22);}
   FString Text;TSharedPtr<FJsonObject> Road;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Polish/Adventure/road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Road)){Test->AddError(TEXT("Retained real road source absent"));return Finish();}
   const auto& Points=Road->GetArrayField(TEXT("points"));const auto& A=Points[740]->AsArray();const auto& B=Points[742]->AsArray();
   RoadFoot=FVector(NeonCityCoastalLayout::IslandOrigin.X+100*A[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*A[1]->AsNumber(),-130+100*A[2]->AsNumber());RoadAim=FVector(NeonCityCoastalLayout::IslandOrigin.X+100*B[0]->AsNumber(),NeonCityCoastalLayout::IslandOrigin.Y-100*B[1]->AsNumber(),-130+100*B[2]->AsNumber()+3);
   auto* Actor=W->SpawnActor<AActor>();Capture=NewObject<USceneCaptureComponent2D>(Actor);Actor->SetRootComponent(Capture);Capture->RegisterComponent();
   Target=NewObject<UTextureRenderTarget2D>(Actor);Target->InitCustomFormat(128,128,PF_FloatRGBA,false);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
   Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->CaptureSource=SCS_SceneColorHDR;Capture->FOVAngle=24;
   Capture->ShowFlags.SetEyeAdaptation(false);Capture->ShowFlags.SetPostProcessing(false);Stage=1;Setup();return false;
  }
  Evidence.Record(W,PC,Stage);const float Elapsed=W->GetTimeSeconds()-At;
  if(Stage==1 && Elapsed>1.5f){Key(EKeys::L,IE_Pressed);Stage=2;At=W->GetTimeSeconds();return false;}
  if(Stage==2 && Elapsed>.65f)
  {
   if(!Released){Key(EKeys::L,IE_Released);Released=true;At=W->GetTimeSeconds();return false;}
   if(Elapsed<.2f)return false;
   auto* Beam=Light->GetBeam();
   if(!Test->TestTrue(TEXT("Normal L input equips registered, visible real spot light"),Light->IsEquipped() && Beam && Beam->IsRegistered() && Beam->IsVisible() && Beam->Intensity>0))return Finish();
   const FVector Lens=Beam->GetComponentLocation();const FVector Aim=(AimPoint-Lens).GetSafeNormal();
   Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("surface"),Case==0?TEXT("Existing dark mansion rear wall"):TEXT("Retained mountain road asphalt"));Row->SetField(TEXT("lens_world_cm"),Vec(Lens));Row->SetField(TEXT("beam_forward"),Vec(Beam->GetForwardVector()));Row->SetField(TEXT("normal_aim_target_cm"),Vec(AimPoint));Row->SetNumberField(TEXT("cone_alignment"),FVector::DotProduct(Aim,Beam->GetForwardVector()));Row->SetNumberField(TEXT("lumens"),Beam->Intensity);
   Capture->SetWorldLocationAndRotation(Lens+Aim*20,Aim.Rotation());
   FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Flash-Night-%d-On.png"),Case));
   Key(EKeys::L,IE_Pressed);Released=false;Stage=3;At=W->GetTimeSeconds();return false;
  }
  if(Stage==3 && Elapsed>.65f)
  {
   if(!Released){Key(EKeys::L,IE_Released);Released=true;At=W->GetTimeSeconds();return false;}
   if(Elapsed<.2f)return false;
   Test->TestFalse(TEXT("Normal L disables actual environment light"),Light->IsEquipped());
   Off=ReadRadiance();FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Flash-Night-%d-Off.png"),Case));Key(EKeys::L,IE_Pressed);Released=false;Stage=4;At=W->GetTimeSeconds();return false;
  }
  if(Stage==4 && Elapsed>.65f)
  {
   if(!Released){Key(EKeys::L,IE_Released);Released=true;At=W->GetTimeSeconds();return false;}
   if(Elapsed<.2f)return false;
   const double On=ReadRadiance();Row->SetNumberField(TEXT("off_linear_hdr_radiance"),Off);Row->SetNumberField(TEXT("on_linear_hdr_radiance"),On);Row->SetBoolField(TEXT("same_camera_and_exposure"),true);Rows.Add(MakeShared<FJsonValueObject>(Row));
   Test->TestTrue(TEXT("Real flashlight measurably illuminates the aimed environment surface"),FMath::IsFinite(On) && FMath::IsFinite(Off) && On>Off*1.15 && On-Off>.001);
   UE_LOG(LogTemp,Display,TEXT("Night flashlight %d: off %.6f on %.6f cone %.3f"),Case,Off,On,Row->GetNumberField(TEXT("cone_alignment")));
   Key(EKeys::L,IE_Pressed);Released=false;Stage=5;At=W->GetTimeSeconds();return false;
  }
  if(Stage==5 && Elapsed>.3f){Key(EKeys::L,IE_Released);if(++Case==2)return Finish();Setup();Stage=1;return false;}
  return false;
 }
private:
 void Setup()
 {
  const FVector Foot=Case==0?M(-3,-7,4.2):RoadFoot;AimPoint=Case==0?M(-3,-11.7,5.6):RoadAim;
  const FVector Position=Foot+FVector(0,0,P->GetSimpleCollisionHalfHeight()+2);const FRotator Aim=(AimPoint-(Position+FVector(0,0,55))).Rotation();
   P->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);P->SetActorRotation(FRotator(0,Aim.Yaw,0));PC->SetControlRotation(Aim);At=W->GetTimeSeconds();Released=false;
 }
 double ReadRadiance()
 {
  Capture->CaptureScene();FlushRenderingCommands();TArray<FFloat16Color> Pixels;
  if(!Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels) || Pixels.Num()!=128*128){Test->AddError(TEXT("Native HDR scene readback failed"));return std::numeric_limits<double>::quiet_NaN();}
  double Sum=0;for(int32 Y=48;Y<80;++Y)for(int32 X=48;X<80;++X){const auto& C=Pixels[Y*128+X];Sum+=.2126*C.R.GetFloat()+.7152*C.G.GetFloat()+.0722*C.B.GetFloat();}return Sum/(32*32);
 }
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool Finish()
 {
  Key(EKeys::L,IE_Released);TSharedPtr<FJsonObject> J=MakeShared<FJsonObject>();J->SetArrayField(TEXT("samples"),Rows);J->SetStringField(TEXT("scope"),TEXT("Two declared normal-player night fixtures, actual L mapping and native linear HDR environment radiance comparison; no exposure lift or emissive-lens substitute. Fixture placements are not travel evidence."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);
  if(FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)))FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("NightFlashlightRadiance.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;UNeonCityFlashlightComponent* Light=nullptr;USceneCaptureComponent2D* Capture=nullptr;UTextureRenderTarget2D* Target=nullptr;
 double Started=0,Off=0;float At=0;int32 Stage=0,Case=0;bool Released=false;FVector AimPoint,RoadFoot,RoadAim;TSharedPtr<FJsonObject> Row;TArray<TSharedPtr<FJsonValue>> Rows;FNeonCityVisualEvidence Evidence{TEXT("NightFlashlight")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase205cNightFlashlight,"NeonCity.Phase205c.NightFlashlightPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPhase205cNightFlashlight::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeFlashlightChecks::FIllumination(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
