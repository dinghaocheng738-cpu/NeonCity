#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Misc/AutomationTest.h"
#include "Adventure/WingsuitComponent.h"
#include "Adventure/WingsuitLaunchPoint.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
namespace {
class FHeroFlightReview : public IAutomationLatentCommand {
public: explicit FHeroFlightReview(FAutomationTestBase* T):Test(T){}
bool Update()override {
 if(!Started)Started=FPlatformTime::Seconds();
 if(FPlatformTime::Seconds()-Started>220){Test->AddError(FString::Printf(TEXT("Flight timeout at stage %d"),Stage));return true;}
 auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
 const float Now=W->GetTimeSeconds();auto* F=P->FindComponentByClass<UWingsuitComponent>();
 auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
 if(Stage==0){auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(46550,-120050,30580))))return false;
  for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(12);}
  AWingsuitLaunchPoint* Launch=nullptr;int32 Launches=0;
  for(TActorIterator<AWingsuitLaunchPoint> It(W);It;++It){Launch=*It;++Launches;}
  if(!Test->TestTrue(TEXT("One authored flight kit supplies the intentional sea-side launch opening"),Launches==1))return true;
  // Only the focused fixture starts here. Jump through the designed cliff opening;
  // the old northbound route now leads onto the safe mansion arrival court.
  P->SetActorLocation(Launch->GetActorLocation()+FVector(-100,-110,130),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->SetActorRotation(FRotator(0,-90,0));PC->SetControlRotation(FRotator(0,-90,0));Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;PC->SetViewTarget(Camera);At=Now;Stage=1;return false;}
 const FVector Here=P->GetActorLocation();const bool Reserve=F->IsCanopyOpen();const FVector Focus=Here+FVector(0,0,Reserve?200:0);
 const float ReviewDistance=Stage==4 && FlightView>0?(FlightView==1?1200:3500):440;
 Camera->SetActorLocation(Focus+P->GetActorForwardVector()*(Reserve?1200:ReviewDistance)+P->GetActorRightVector()*220+FVector(0,0,Reserve?100:(F->IsFlying()?ReviewDistance*.75:100)));Camera->SetActorRotation((Focus-Camera->GetActorLocation()).Rotation());
 if(Stage==1 && Now-At>3){if(!Test->TestTrue(TEXT("Equip from real summit ground"),F->Equip()))return true;
  const auto* Deploy=P->GetMesh()->GetSkeletalMeshAsset()->FindMorphTarget(TEXT("Deploy"));
  if(!Test->TestNotNull(TEXT("Production wing inflation morph exists"),Deploy))return true;
  const auto& LODs=Deploy->GetMorphLODModels();Test->TestEqual(TEXT("Inflation preserved at all three garment LODs"),LODs.Num(),3);
  for(int32 I=0;I<LODs.Num();++I)Test->TestTrue(FString::Printf(TEXT("LOD %d retains deployed wing deltas"),I),LODs[I].Vertices.Num()>100);
  Stage=2;At=Now;return false;}
 if(Stage==2 && Now-At>3){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Flight-Summit.png"));Origin=Here;Key(EKeys::W,IE_Pressed);Key(EKeys::SpaceBar,IE_Pressed);Stage=3;At=Now;return false;}
 if(Stage==3){PC->SetControlRotation(FRotator(0,-90,0));if(Now-At>.15)Key(EKeys::SpaceBar,IE_Released);if(Now-At>.55 && Now-At<.62)Key(EKeys::SpaceBar,IE_Pressed);
  if(F->IsFlying()){Key(EKeys::W,IE_Released);Test->TestTrue(TEXT("Flight entered by normal jump/fall, without launch teleport"),FVector::Dist(Origin,Here)>100);FlightOrigin=Here;At=Now;Stage=4;return false;}
  if(Now-At>10){Key(EKeys::W,IE_Released);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Flight-Launch-Failure.png"));Test->AddError(FString::Printf(TEXT("Ordinary summit jump did not deploy; position %s mode %d"),*Here.ToString(),P->GetCharacterMovement()->MovementMode));return true;}}
 if(Stage==4 && Now-At>2){FNeonCityVisualEvidence::CaptureHUD(W,FlightView==0?TEXT("Hero-Flight-Glide.png"):FlightView==1?TEXT("Hero-Flight-Medium.png"):TEXT("Hero-Flight-Distant.png"));Test->TestFalse(TEXT("Inflated wing visible before reserve approach"),F->IsCanopyOpen());
  if(++FlightView<3){At=Now;return false;}
  PC->SetViewTarget(P);StartYaw=P->GetActorRotation().Yaw;Key(EKeys::D,IE_Pressed);Stage=5;At=Now;return false;}
 if(Stage==5 && Now-At>.4){Key(EKeys::D,IE_Released);Test->TestTrue(TEXT("Directional input changes actual flight heading"),FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw,P->GetActorRotation().Yaw))>5);Key(EKeys::A,IE_Pressed);Stage=6;At=Now;return false;}
 if(Stage==6 && Now-At>.4){Key(EKeys::A,IE_Released);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Flight-Bank.png"));PC->SetViewTarget(Camera);Key(EKeys::SpaceBar,IE_Pressed);Stage=7;At=Now;return false;}
 if(Stage==7 && Now-At>.2)Key(EKeys::SpaceBar,IE_Released);
 if(Stage==7 && Now-At>2){Test->TestTrue(TEXT("Player can open reserve canopy"),F->IsCanopyOpen());FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Flight-Canopy.png"));Stage=8;At=Now;return false;}
 if(Stage==8){
  // Pilot a descending holding pattern inland using the same A/D controls;
  // continuing north without steering would leave the island for open sea.
  const FVector Points[]={NeonCityCoastalLayout::FromLegacyIsland(FVector(47000,-110000,0)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-110000,0)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-118000,0)),NeonCityCoastalLayout::FromLegacyIsland(FVector(47000,-118000,0))};
  if(FVector::Dist2D(Here,Points[Approach])<1000)Approach=(Approach+1)%4;
  const float Heading=(Points[Approach]-Here).Rotation().Yaw;
  const float Error=FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Heading);
  Key(EKeys::A,Error<-3?IE_Pressed:IE_Released);Key(EKeys::D,Error>3?IE_Pressed:IE_Released);
  if(P->GetCharacterMovement()->IsMovingOnGround()){Key(EKeys::A,IE_Released);Key(EKeys::D,IE_Released);Test->TestTrue(TEXT("Real horizontal glide distance exceeds 50 m"),FVector::Dist2D(Origin,Here)>5000);Test->TestTrue(TEXT("Flight lands on dry island terrain"),Here.Z>200);Stage=9;At=Now;return false;}
  if(Now-At>150){Test->AddError(FString::Printf(TEXT("Reserve did not land on island: %s"),*Here.ToString()));return true;}}
 if(Stage==9 && Now-At>2){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hero-Flight-Landing.png"));Test->TestFalse(TEXT("Flight state ends after landing presentation settles"),F->IsFlying());Test->TestTrue(TEXT("Landed actor remains alive"),P->FindComponentByClass<UHealthComponent>()->GetCurrentHealth()>0);F->Unequip();Test->TestFalse(TEXT("Suit removal restores ordinary locomotion"),F->IsEquipped());return true;}
 return false;
}
FAutomationTestBase* Test;double Started=0;float At=0,StartYaw=0;int32 Stage=0,Approach=0,FlightView=0;FVector Origin,FlightOrigin;ACameraActor* Camera=nullptr;
}; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeroFlightPIE,"NeonCity.Polish.HeroFlightPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHeroFlightPIE::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FHeroFlightReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
