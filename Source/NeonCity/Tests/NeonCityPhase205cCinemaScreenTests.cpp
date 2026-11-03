#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandScenePanel.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
namespace CinemaScreenNative
{
class FFlow:public IAutomationLatentCommand
{
public:FFlow(FAutomationTestBase* T,bool Seated=false):Test(T),Evidence(Seated?TEXT("CinemaSeatedView"):TEXT("CinemaScreenFix")),SeatedView(Seated){}
 bool Update()override
 {
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>60)return End(TEXT("Screen fix-specific timeout"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(47000,-119000,31500))))return false;for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)if(I->Kind==EIslandLeisureKind::Cinema)F=*I;if(!F)return End(TEXT("Cinema missing"));Scene=F->ScenePanel->GetScene();const FVector Around=F->GetActorLocation()+F->GetActorForwardVector()*125;FCollisionQueryParams Q(SCENE_QUERY_STAT(CinemaScreenStanding),false,P);FHitResult H;
   if(!W->LineTraceSingleByChannel(H,Around+FVector(0,0,150),Around-FVector(0,0,100),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Actual standing approach lacks support"));const FVector At=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);if(W->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual standing approach obstructed"));P->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((F->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=F)return End(TEXT("Normal F prompt obstructed"));Key(true);Stage=1;AtTime=W->GetTimeSeconds();return false;
  }
  if(Stage==1 && W->GetTimeSeconds()-AtTime>.3)
  {
   Key(false);if(!F->IsOccupied() || F->GetStage()!=EIslandLeisureStage::Active || !F->IsFeedbackPlaying())return End(TEXT("Normal F playback failed"));auto* M=Cast<UMaterialInstanceDynamic>(F->Screen->GetMaterial(0));float V=0;if(!M || !M->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ScreenEmission")),V) || !FMath::IsNearlyEqual(V,12.f))return End(TEXT("Calibrated native screen parameter missing"));
   if(SeatedView){if(PC->GetViewTarget()!=F)return End(TEXT("Normal cinema seat did not own its actual player camera"));}
   else{Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=92;Camera->SetActorLocation(F->GetActorTransform().TransformPosition(FVector(35,0,175)));const FVector Target=F->Screen->GetComponentLocation()+FVector(0,0,125);Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);}Stage=2;return false;
  }
  if(Stage==2 && F->GetElapsed()>6){if(!Patch(First,Mean1,Range1))return End(TEXT("Native screen readback failed"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Cinema-Screen-Actual-6s.png"));Stage=3;return false;}
  if(Stage==3 && F->GetElapsed()>12)
  {
   TArray<FColor> Second;if(!Patch(Second,Mean2,Range2) || Second.Num()!=First.Num())return End(TEXT("Second native screen readback failed"));for(int I=0;I<First.Num();++I)Difference+=FMath::Abs(float(First[I].R)-Second[I].R)+FMath::Abs(float(First[I].G)-Second[I].G)+FMath::Abs(float(First[I].B)-Second[I].B);Difference/=First.Num()*3.;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Cinema-Screen-Actual-12s.png"));
   if(Mean1<35 || Mean2<35 || Range1<12 || Range2<12 || Difference<2)return End(FString::Printf(TEXT("Actual screen remains dark, flat or static: means%.2f/%.2f ranges%.2f/%.2f difference%.2f"),Mean1,Mean2,Range1,Range2,Difference));PC->SetViewTarget(P);Key(true);Stage=4;AtTime=W->GetTimeSeconds();return false;
  }
  if(Stage==4 && W->GetTimeSeconds()-AtTime>.3){Key(false);if(F->IsOccupied() || F->ScenePanel->GetScene()!=Scene || P->IsHidden() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Normal F leave failed safe lighting/walking restoration"));return End(TEXT(""));}return false;
 }
 bool Patch(TArray<FColor>& Out,float& Mean,float& Range)
 {
  auto* V=W->GetGameViewport()->Viewport;V->Draw(false);TArray<FColor> Pixels;if(!V->ReadPixels(Pixels))return false;const auto Size=V->GetSizeXY();if(Size.X!=1600 || Size.Y!=900 || Pixels.Num()!=Size.X*Size.Y)return false;float Low=255,High=0;Mean=0;for(int Y=Size.Y*.35;Y<Size.Y*.65;Y+=3)for(int X=Size.X*.30;X<Size.X*.70;X+=3){const FColor C=Pixels[Y*Size.X+X];const float L=C.R*.2126f+C.G*.7152f+C.B*.0722f;Mean+=L;Low=FMath::Min(Low,L);High=FMath::Max(High,L);Out.Add(C);}Mean/=Out.Num();Range=High-Low;return true;
 }
 void Key(bool Down){if(Held!=Down && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 bool End(const FString& Error){Key(false);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(!Error.IsEmpty())Test->AddError(Error);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("CinemaScreenFix");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nfailure=%s\nmean6s=%.3f\nmean12s=%.3f\nrange6s=%.3f\nrange12s=%.3f\nanimation_difference=%.3f\nActual normal F playback; camera policy: CinemaSeatedViewMinimum uses the normal player camera; CinemaScreenFix uses a fixed comparison camera. Central screen pixel values are LDR sRGB code values, not measured physical nits. Local material only; no global exposure or Bloom adjustment. Existing finite completion/save scopes cached. No subjective film-quality approval.\n"),Error.IsEmpty(),*Error,Mean1,Mean2,Range1,Range2,Difference),*(Dir/TEXT("scope.txt")));return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLeisureFacility* F=nullptr;ACameraActor* Camera=nullptr;int Stage=0,Scene=0;double Start=0;float AtTime=0,Mean1=0,Mean2=0,Range1=0,Range2=0,Difference=0;bool Held=false;TArray<FColor> First;FNeonCityVisualEvidence Evidence;bool SeatedView=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemaScreenFix,"NeonCity.Phase205c.Leisure.CinemaScreenFixPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCinemaScreenFix::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CinemaScreenNative::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinemaSeatedView,"NeonCity.Phase205c.Leisure.CinemaSeatedViewMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCinemaSeatedView::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CinemaScreenNative::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
