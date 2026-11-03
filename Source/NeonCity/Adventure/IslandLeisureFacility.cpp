#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandScenePanel.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
namespace Leisure {constexpr float RevolutionSeconds=2.4f;constexpr int32 SampleRate=24000;}
AIslandLeisureFacility::AIslandLeisureFacility()
{
 Furniture=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacilityFurniture"));Furniture->SetupAttachment(RootComponent);Furniture->SetCollisionProfileName(TEXT("BlockAll"));Furniture->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);Furniture->SetCanEverAffectNavigation(false);
 Crank=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BicycleCrank"));Crank->SetupAttachment(RootComponent);Crank->SetRelativeLocation(FVector(22,0,36));Crank->SetCollisionEnabled(ECollisionEnabled::NoCollision);Crank->SetCanEverAffectNavigation(false);
 LeftPedal=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLevelPedal"));LeftPedal->SetupAttachment(Crank);LeftPedal->SetRelativeLocation(FVector(-15,18,0));LeftPedal->SetCollisionEnabled(ECollisionEnabled::NoCollision);LeftPedal->SetCanEverAffectNavigation(false);
 RightPedal=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLevelPedal"));RightPedal->SetupAttachment(Crank);RightPedal->SetRelativeLocation(FVector(15,-18,0));RightPedal->SetCollisionEnabled(ECollisionEnabled::NoCollision);RightPedal->SetCanEverAffectNavigation(false);
 Screen=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OriginalFilmScreen"));Screen->SetupAttachment(RootComponent);Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);Screen->SetCanEverAffectNavigation(false);
 Heater=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaunaHeater"));Heater->SetupAttachment(RootComponent);Heater->SetCollisionProfileName(TEXT("BlockAll"));Heater->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);Heater->SetCanEverAffectNavigation(false);
 Display=CreateDefaultSubobject<UTextRenderComponent>(TEXT("EquipmentDisplay"));Display->SetupAttachment(RootComponent);Display->SetWorldSize(4);Display->SetHorizontalAlignment(EHTA_Center);Display->SetTextRenderColor(FColor(172,192,187));
 Feedback=CreateDefaultSubobject<UAudioComponent>(TEXT("OriginalFacilityAudio"));Feedback->SetupAttachment(RootComponent);Feedback->bAutoActivate=false;Feedback->bAllowSpatialization=true;
 CinemaCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("CinemaSeatedEye"));CinemaCamera->SetupAttachment(RootComponent);CinemaCamera->FieldOfView=75;CinemaCamera->SetAutoActivate(false);
 FSoundAttenuationSettings Att;Att.bAttenuate=true;Att.bSpatialize=true;Att.AttenuationShape=EAttenuationShape::Sphere;Att.AttenuationShapeExtents=FVector(180);Att.FalloffDistance=1100;Feedback->AdjustAttenuation(Att);Feedback->SetVolumeMultiplier(.17f);
}
bool AIslandLeisureFacility::CanInteract_Implementation(AActor* Visitor)const
{
 if(!Super::CanInteract_Implementation(Visitor))return false;
 if(IsOccupied() || Kind!=EIslandLeisureKind::ExerciseBike)return true;
 const auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);const FVector Local=GetActorTransform().InverseTransformPosition(P->GetActorLocation());
 return P->GetPlayerDataComponent()->GetStamina()>=20 && FMath::Abs(Local.X+20)<35 && Local.Y>=65 && Local.Y<=105;
}
FText AIslandLeisureFacility::GetInteractionName_Implementation()const
{
 if(IsOccupied())return FText::FromString(FString::Printf(TEXT("F / movement to finish safely | %s %.0f / %.0f sec | F6 safe save"),Kind==EIslandLeisureKind::Cinema?TEXT("Coastal Light"):Kind==EIslandLeisureKind::ExerciseBike?TEXT("Cycling"):TEXT("Sauna"),Elapsed,SessionLength));
 if(Kind==EIslandLeisureKind::Cinema)return FText::FromString(TEXT("F Watch Coastal Light — original 30 sec film"));
 if(Kind==EIslandLeisureKind::ExerciseBike)return FText::FromString(TEXT("F Exercise bike — clear left side, 20 energy required"));
 return FText::FromString(TEXT("F Sauna — 60 sec seated warm-up and rest"));
}
void AIslandLeisureFacility::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 if(IsOccupied()){RequestExit();return;}
 if(Kind==EIslandLeisureKind::ExerciseBike)
 {
  auto* P=Cast<ANeonCityPlayerCharacter>(Visitor);
  for(auto* Clip:{ExerciseEnter.Get(),ExerciseLoop.Get(),ExerciseExit.Get()})if(!Clip || !Clip->GetSkeleton() || !Clip->GetSkeleton()->IsCompatibleMesh(P->GetMesh()->GetSkeletalMeshAsset()))return;
  Occupant->SeatedAnimation=ExerciseEnter;
 }
 Super::Interact_Implementation(Visitor);if(IsOccupied())StartSession();
}
void AIslandLeisureFacility::StartSession()
{
 Elapsed=StageElapsed=0;Temperature=22;CrankRadians=0;bExitRequested=false;
 Stage=Kind==EIslandLeisureKind::ExerciseBike?EIslandLeisureStage::Entering:EIslandLeisureStage::Active;
 if(Kind==EIslandLeisureKind::ExerciseBike){auto* Body=Occupant->GetDriverMesh();Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Body->PlayAnimation(ExerciseEnter,false);}
 if(ScenePanel){SceneBefore=ScenePanel->GetScene();if(Kind==EIslandLeisureKind::Cinema)ScenePanel->SetScene(2);}
 if(Kind==EIslandLeisureKind::Cinema && Screen->GetStaticMesh())
  if(auto* P=Cast<ANeonCityPlayerCharacter>(Occupant->GetRepresentedDriver()))
   if(auto* PC=Cast<APlayerController>(P->GetController()))
   {
    // The standing character boom starts at the seated hip and can pass
    // through the row behind it. Use the actual seated eye for this session,
    // while retaining the player's normal look input and prior view target.
    CinemaCamera->SetRelativeLocation(SeatHipLocal+FVector(0,0,65));
    PC->SetControlRotation((Screen->Bounds.Origin-CinemaCamera->GetComponentLocation()).Rotation());
    CinemaCamera->SetWorldRotation(PC->GetControlRotation());
    PreviousViewTarget=PC->GetViewTarget();CinemaViewer=PC;CinemaCamera->Activate();PC->SetViewTarget(this);
    Occupant->GetDriverMesh()->SetOwnerNoSee(true);
   }
 if(ProceduralSound){ProceduralSound->ResetAudio();ProceduralSound->QueueAudio(reinterpret_cast<const uint8*>(SoundPCM.GetData()),SoundPCM.Num()*sizeof(int16));Feedback->Play();}
 Notice(Kind==EIslandLeisureKind::Cinema?TEXT("Coastal Light. Original animated short and score. F or movement stops playback."):Kind==EIslandLeisureKind::ExerciseBike?TEXT("Mounting the bike. Steady 25 rpm session uses energy gradually. F or movement dismounts safely."):TEXT("Sauna is warming. Rest restores energy gradually through the same home rest system. F or movement leaves."));
 UpdatePresentation();
}
void AIslandLeisureFacility::RequestExit()
{
 bExitRequested=true;
 if(Kind!=EIslandLeisureKind::ExerciseBike){StopSession(false);Leave();return;}
 if(Stage==EIslandLeisureStage::Entering)
 {
  auto* Body=Occupant->GetDriverMesh();auto* Anim=Body?Body->GetSingleNodeInstance():nullptr;
  if(Anim){StageElapsed=Anim->GetCurrentTime();Anim->SetReverse(true);Stage=EIslandLeisureStage::Leaving;}
 }
}
void AIslandLeisureFacility::StopSession(bool Completed)
{
 if(auto* Viewer=CinemaViewer.Get())if(Viewer->GetViewTarget()==this)Viewer->SetViewTarget(PreviousViewTarget.IsValid()?PreviousViewTarget.Get():Viewer->GetPawn());
 CinemaCamera->Deactivate();CinemaViewer.Reset();PreviousViewTarget.Reset();
 if(Kind==EIslandLeisureKind::Cinema)if(auto* Body=Occupant->GetDriverMesh())Body->SetOwnerNoSee(false);
 if(Completed)++CompletedSessions;
 Stage=EIslandLeisureStage::Idle;bExitRequested=false;Feedback->Stop();if(ProceduralSound)ProceduralSound->ResetAudio();
 if(ScenePanel && Kind==EIslandLeisureKind::Cinema)ScenePanel->SetScene(SceneBefore);
 CrankRadians=0;Crank->SetRelativeRotation(FRotator::ZeroRotator);LeftPedal->SetRelativeRotation(FRotator::ZeroRotator);RightPedal->SetRelativeRotation(FRotator::ZeroRotator);Temperature=22;UpdatePresentation();
}
void AIslandLeisureFacility::Tick(float Delta)
{
 // Gym uses its actual finite mount/cycle/dismount clips and gradual energy
 // expenditure. Calling the lounge Tick here would incorrectly award rest.
 if(Kind!=EIslandLeisureKind::ExerciseBike)Super::Tick(Delta);else AActor::Tick(Delta);
 if(!IsOccupied()){if(Stage!=EIslandLeisureStage::Idle)StopSession(false);return;}
 auto* P=Cast<ANeonCityPlayerCharacter>(Occupant->GetRepresentedDriver());auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
 if(!P || !PC || PC->GetPawn()!=P || P->GetAttachParentActor()!=this || P->GetHealthComponent()->IsDead()){StopSession(false);Leave();return;}
 if(Kind==EIslandLeisureKind::Cinema && CinemaViewer==PC)CinemaCamera->SetWorldRotation(PC->GetControlRotation());
 for(const auto K:{EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::SpaceBar})if(PC->IsInputKeyDown(K)){RequestExit();break;}
 if(Stage==EIslandLeisureStage::Idle)return;
 auto* Body=Occupant->GetDriverMesh();auto* Anim=Body?Body->GetSingleNodeInstance():nullptr;
 if(Kind==EIslandLeisureKind::ExerciseBike)
 {
  if(!Anim){StopSession(false);Leave();return;}
  if(Stage==EIslandLeisureStage::Entering)
  {StageElapsed+=Delta;if(StageElapsed>=ExerciseEnter->GetPlayLength()){Body->PlayAnimation(ExerciseLoop,true);Stage=EIslandLeisureStage::Active;StageElapsed=0;}UpdatePresentation();return;}
  if(Stage==EIslandLeisureStage::Leaving)
  {
   const bool ReverseEntry=Anim->IsReverse();StageElapsed+=ReverseEntry?-Delta:Delta;
   if((ReverseEntry && StageElapsed<=0) || (!ReverseEntry && StageElapsed>=ExerciseExit->GetPlayLength()))
   {const bool Completed=Elapsed>=SessionLength;FTransform Exit;if(FindPersistenceExit(Exit)){Notice(Completed?TEXT("Cycling finished. Normal walking restored."):TEXT("Cycling stopped. Normal walking restored."));StopSession(Completed);Leave();}}
   UpdatePresentation();return;
  }
  Elapsed+=Delta;P->GetPlayerDataComponent()->SetStamina(FMath::Max(0.f,P->GetPlayerDataComponent()->GetStamina()-Delta*.35f));
  CrankRadians=FMath::Fmod(Anim->GetCurrentTime()/Leisure::RevolutionSeconds,1.f)*2*PI;Crank->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(CrankRadians),0,0));
  for(auto* Pedal:{LeftPedal.Get(),RightPedal.Get()})Pedal->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(CrankRadians),0,0));
  if(Elapsed>=SessionLength || P->GetPlayerDataComponent()->GetStamina()<5)bExitRequested=true;
  if(bExitRequested && CrankRadians<.35f){Body->PlayAnimation(ExerciseExit,false);Stage=EIslandLeisureStage::Leaving;StageElapsed=0;}
 }
 else
 {
  Elapsed+=Delta;
  if(Kind==EIslandLeisureKind::Sauna)Temperature=FMath::Lerp(22.f,64.f,FMath::Clamp(Elapsed/20,0.f,1.f));
  if(Elapsed>=SessionLength){FTransform Exit;if(FindPersistenceExit(Exit)){Notice(Kind==EIslandLeisureKind::Cinema?TEXT("Coastal Light ended. Room lighting restored."):TEXT("Sauna session finished. Normal walking restored."));StopSession(true);Leave();}}
 }
 if(ProceduralSound && Stage!=EIslandLeisureStage::Idle && ProceduralSound->GetAvailableAudioByteCount()<Leisure::SampleRate)
  ProceduralSound->QueueAudio(reinterpret_cast<const uint8*>(SoundPCM.GetData()),SoundPCM.Num()*sizeof(int16));
 UpdatePresentation();
}
void AIslandLeisureFacility::UpdatePresentation()
{
 const bool Playing=Stage!=EIslandLeisureStage::Idle;
 if(FilmMaterial){FilmMaterial->SetScalarParameterValue(TEXT("FilmSeconds"),Elapsed);FilmMaterial->SetScalarParameterValue(TEXT("Playing"),Playing?1:0);}
 if(HeaterMaterial)HeaterMaterial->SetScalarParameterValue(TEXT("Heat"),Playing?FMath::Clamp((Temperature-22)/42,0.f,1.f):0);
 if(Kind==EIslandLeisureKind::Cinema)Display->SetText(FText::FromString(Playing?FString::Printf(TEXT("COASTAL LIGHT\n%.0f / %.0f"),Elapsed,SessionLength):TEXT("COASTAL LIGHT\nORIGINAL SHORT FILM")));
 else if(Kind==EIslandLeisureKind::ExerciseBike)Display->SetText(FText::FromString(Playing?FString::Printf(TEXT("25 RPM\n%.0f / %.0f SEC"),Elapsed,SessionLength):TEXT("CYCLE\n30 SEC / STEADY")));
 else Display->SetText(FText::FromString(FString::Printf(TEXT("DRY SAUNA\n%.0f C | %.0f SEC"),Temperature,Playing?Elapsed:0)));
}
void AIslandLeisureFacility::PrepareSound()
{
 SoundPCM.SetNum(Leisure::SampleRate*4);FRandomStream Random(205503);float Filtered=0;
 for(int32 I=0;I<SoundPCM.Num();++I)
 {
  const float T=I/float(Leisure::SampleRate);Filtered=.985f*Filtered+.015f*Random.FRandRange(-1,1);
  float Sample=Filtered*.20f;
  if(Kind==EIslandLeisureKind::Cinema)Sample+=.065f*FMath::Sin(2*PI*130.8128f*T)+.035f*FMath::Sin(2*PI*164.8138f*T)+.025f*FMath::Sin(2*PI*195.9977f*T);
  if(Kind==EIslandLeisureKind::ExerciseBike)Sample+=.035f*FMath::Sin(2*PI*90*T)*(.55f+.45f*FMath::Cos(2*PI*T/Leisure::RevolutionSeconds));
  const float Envelope=FMath::Min(1.f,FMath::Min(T/.08f,(4-T)/.08f));SoundPCM[I]=FMath::RoundToInt(FMath::Clamp(Sample*Envelope,-.8f,.8f)*32767);
 }
 ProceduralSound=NewObject<USoundWaveProcedural>(this);ProceduralSound->SetSampleRate(Leisure::SampleRate);ProceduralSound->NumChannels=1;ProceduralSound->Duration=INDEFINITELY_LOOPING_DURATION;ProceduralSound->bLooping=false;Feedback->SetSound(ProceduralSound);
}
void AIslandLeisureFacility::BeginPlay()
{
 Super::BeginPlay();if(Screen->GetStaticMesh())FilmMaterial=Screen->CreateDynamicMaterialInstance(0);if(Heater->GetStaticMesh())HeaterMaterial=Heater->CreateDynamicMaterialInstance(0);PrepareSound();UpdatePresentation();
}
void AIslandLeisureFacility::EndPlay(EEndPlayReason::Type Reason){StopSession(false);Super::EndPlay(Reason);}
bool AIslandLeisureFacility::IsFeedbackPlaying()const{return Feedback && Feedback->IsPlaying();}
void AIslandLeisureFacility::Notice(const FString& Text)
{
 if(auto* P=Occupant->GetRepresentedDriver())if(auto* PC=Cast<APlayerController>(P->GetController()))if(auto* N=PC->FindComponentByClass<UNeonCityNavigationComponent>())N->ShowNotice(FText::FromString(Text),5);
}
