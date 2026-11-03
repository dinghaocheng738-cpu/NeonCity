#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandEstateSport.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityFlashlightComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
namespace EstateTennisPIE
{
class FFlow:public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* In):Test(In),Evidence(TEXT("EstateTennis")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>125)return End(TEXT("Tennis normal input flow timed out"));if(!P || W->GetTimeSeconds()<4)return false;
  Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Court && Court->IsActive())
  {
   if(const auto* Pose=P->FindComponentByClass<UIslandTennisParticipant>();Pose && Pose->Racket)
   {const FVector AtRacket=Pose->Racket->GetComponentTransform().InverseTransformPosition(Court->GetActorTransform().TransformPosition(Court->GetBallLocal())),Local=Court->GetActorTransform().InverseTransformPosition(P->GetActorLocation()),Pred=P->GetActorTransform().InverseTransformPosition(Court->GetActorTransform().TransformPosition(Court->GetBallLocal()+Court->GetBallVelocityLocal()*.2f+FVector(0,0,-19.6f)));ContactCSV+=FString::Printf(TEXT("%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%d\n"),W->GetTimeSeconds(),Pose->SwingAge,AtRacket.X,AtRacket.Y,AtRacket.Z,Court->GetPlayerContacts(),Court->GetAIContacts(),Local.X,Local.Y,Local.Z,Pred.X,Pred.Y,Pred.Z,Pressed,P->GetVelocity().Size2D(),PC->IsMoveInputIgnored());}
   if(auto* AI=Court->GetOpponent())if(auto* Pose=AI->FindComponentByClass<UIslandTennisParticipant>();Pose && Pose->Racket)
   {
    const FVector B=Court->GetBallLocal(),V=Court->GetBallVelocityLocal(),Loc=Court->GetActorTransform().InverseTransformPosition(AI->GetActorLocation());
    const FVector Pred=AI->GetActorTransform().InverseTransformPosition(Court->GetActorTransform().TransformPosition(B+V*.16f));
    const FVector Head=Pose->Racket->GetComponentTransform().InverseTransformPosition(Court->GetActorTransform().TransformPosition(B));
    const FVector AIActivity=AI->GetVelocity();AICSV+=FString::Printf(TEXT("%.4f,%d,%d,%d,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f\n"),W->GetTimeSeconds(),Court->GetRules().Rally,Court->GetRules().Service,Court->GetRules().FirstLanding,Court->GetRules().LastHitter,Pose->SwingAge,B.X,B.Y,B.Z,V.X,V.Y,V.Z,Loc.X,Loc.Y,Loc.Z,Pred.X,Pred.Y,Pred.Z,Head.X,Head.Y,Head.Z,int32(AI->GetCharacterMovement()->MovementMode),AIActivity.X,AIActivity.Y,AIActivity.Z);
   }
  }
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(42200,-112400,26300))))return false;
   for(TActorIterator<AIslandEstateSport> It(W);It;++It)Court=*It;if(!Court)return End(TEXT("Published native tennis owner absent"));
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_EstateTennis_")+FPaths::MakeValidFileName(Run.Left(30)),false) || Save->DoesSaveExist())return End(TEXT("Fresh retained tennis save slot required"));
   const FVector Target=Court->GetActorTransform().TransformPosition(Court->ConsoleLocal+FVector(100,0,0));FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TennisDeclaredApproach),false,P);
   if(!W->LineTraceSingleByChannel(H,Target+FVector(0,0,100),Target-FVector(0,0,250),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Actual court console approach floor missing"));
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual full capsule court approach blocked"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(1);
  }
  if(Stage==1 && Age>.5)
  {
   PC->SetControlRotation((Court->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Court)return End(TEXT("Normal player F tennis prompt not reachable"));
   if(auto* Light=P->FindComponentByClass<UNeonCityFlashlightComponent>())
   {if(!Light->SetEquipped(true))return End(TEXT("Actual flashlight could not be equipped for input conflict check"));IInteractableInterface::Execute_Interact(Court,P);if(Court->IsActive())return End(TEXT("Tennis allowed occupied flashlight hand"));Light->SetEquipped(false);}
   OrientBefore=P->GetCharacterMovement()->bOrientRotationToMovement;FacingBefore=P->GetCharacterMovement()->bUseControllerDesiredRotation;
   Capture(TEXT("Prompt"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.25)
  {
   Key(EKeys::F,false);if(!Court->IsActive() || !Court->GetOpponent() || PC->GetViewTarget()!=Court || !P->FindComponentByClass<UIslandTennisParticipant>())return End(TEXT("Normal F did not create an actual animated practice opponent/rackets/scoped view"));
   if(!ExitChecked){Key(EKeys::F,true);return Next(9);}
   PC->SetControlRotation(Court->GetActorRotation());return Next(3);
  }
  if(Stage==3)
  {
   const FVector Local=Court->GetActorTransform().InverseTransformPosition(P->GetActorLocation());const FVector Delta=Court->GetActorTransform().TransformVectorNoScale(FVector(-1280,-140,Local.Z)-Local);
   if(Delta.Size2D()>16){P->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(Delta.Size2D()/80.f,.15f,1.f));return false;}
   P->GetCharacterMovement()->StopMovementImmediately();Key(EKeys::R,true);return Next(4);
  }
  if(Stage==4)
  {
   if(Age>.1)Key(EKeys::R,false);
   if(Court->GetPlayerContacts()>0){Capture(TEXT("Serve"));return Next(5);}
   if(Age>2)return End(TEXT("Normal R serve failed actual racket/ball contact"));return false;
  }
  if(Stage==5)
  {
   if(Court->GetAIContacts()>0){Capture(TEXT("AI-Return"));return Next(6);}
   if(!Court->GetRules().Rally && Age>2)return End(TEXT("First served ball never produced a reachable AI return"));return false;
  }
  if(Stage==6)
  {
   const FVector Local=Court->GetActorTransform().InverseTransformPosition(P->GetActorLocation());const FVector Ball=Court->GetBallLocal(),V=Court->GetBallVelocityLocal();
   if(Court->GetRules().Rally && Court->GetRules().LastHitter==1)
   {
    // Drive the real character to the descending, finite-arm intercept. The
    // robot varies its return, so a fixed baseline position can sit beneath
    // the apex and never offer a reachable shot. Predict one actual bounce
    // with the production restitution, without writing either body's state.
    FVector Forecast=Ball,Velocity=V;
    if(Court->GetRules().FirstLanding)
    {
     const float Fall=(V.Z+FMath::Sqrt(FMath::Max(0.f,V.Z*V.Z+1960.f*(Ball.Z-6.5f))))/980.f;
     Forecast=Ball+V*Fall+FVector(0,0,-490.f*Fall*Fall);
     Velocity=FVector(V.X*.84f,V.Y*.84f,FMath::Abs(V.Z-980.f*Fall)*.76f);
    }
    const float InterceptHeight=Local.Z+40.f;
    const float Root=FMath::Sqrt(FMath::Max(0.f,Velocity.Z*Velocity.Z+1960.f*(Forecast.Z-InterceptHeight)));
    const float Rising=(Velocity.Z-Root)/980.f;
    // Take the first reachable rising-bounce intercept. A descending-only
    // driver can chase a deep return through the actual retained back fence.
    const float Fall=Rising>.02f?Rising:(Velocity.Z+Root)/980.f;
    const float AimX=FMath::Clamp(Forecast.X+Velocity.X*Fall-85.f,-2150.f,-750.f);
    const float AimY=FMath::Clamp(Forecast.Y+Velocity.Y*Fall,-450.f,450.f);
    const FVector D=Court->GetActorTransform().TransformVectorNoScale(FVector(AimX,AimY,Local.Z)-Local);
    if(D.Size2D()>12)P->AddMovementInput(D.GetSafeNormal2D(),FMath::Clamp(D.Size2D()/80.f,.1f,1.f));
    const FVector Predicted=Court->GetActorTransform().TransformPosition(Ball+V*.20f+FVector(0,0,-.5f*980.f*.20f*.20f));const FVector Contact=P->GetActorTransform().InverseTransformPosition(Predicted);
    if(!Pressed && Contact.X>65 && Contact.X<100 && FMath::Abs(Contact.Y)<20 && Contact.Z>20 && Contact.Z<65 && P->GetVelocity().Size2D()<25){Key(EKeys::LeftMouseButton,true);Pressed=true;PressAt=W->GetTimeSeconds();}
    if(Pressed && W->GetTimeSeconds()-PressAt>.04)Key(EKeys::LeftMouseButton,false);
   }
   if(Court->GetPlayerContacts()>=2)
   {
    if(Court->GetOpponentDistance()<30)return End(TEXT("Opponent returned without actual CharacterMovement court travel"));
    if(Court->GetMaxContactPlaneDistance()>3.86f || Court->GetMaxContactEllipse()>1)return End(TEXT("A scored contact lies outside the actual moving string plane/ellipse"));
    const auto* Pose=P->FindComponentByClass<UIslandTennisParticipant>();
    if(!Pose || !Pose->Racket || Pose->Racket->GetAttachParent()!=P->GetMesh() || Pose->Racket->GetAttachSocketName()!=TEXT("hand_r"))return End(TEXT("Player racket is not actually attached to the finite IK hand bone"));
    Capture(TEXT("Player-Return"));
    const FTransform NormalCamera=Court->CourtCamera->GetComponentTransform();const FVector Grip=Pose->Racket->GetComponentTransform().TransformPosition(FVector(8,0,0));
    const FVector Close=Grip-P->GetActorForwardVector()*100+P->GetActorRightVector()*90+FVector(0,0,35);
    Court->CourtCamera->SetWorldLocationAndRotation(Close,(Grip-Close).Rotation());Capture(TEXT("Declared-Hand-Racket-Close"));Court->CourtCamera->SetWorldTransform(NormalCamera);
    Key(EKeys::F6,true);return Next(7);
   }
   if(!Court->GetRules().Rally && Age>2)return End(TEXT("Timed normal LMB player return failed actual contact"));return false;
  }
  if(Stage==7 && Age>.6)
  {
   Key(EKeys::F6,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Save->ReadSaveForInspection())return End(TEXT("Active sport F6 failed normal standing-safe save"));Key(EKeys::F7,true);return Next(8);
  }
  if(Stage==8 && Age>.7)
  {
   Key(EKeys::F7,false);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || Court->IsActive() || Court->GetOpponent() || P->FindComponentByClass<UIslandTennisParticipant>() || PC->GetViewTarget()!=P || PC->IsMoveInputIgnored() || !P->GetCharacterMovement()->IsMovingOnGround() || P->GetCharacterMovement()->bOrientRotationToMovement!=OrientBefore || P->GetCharacterMovement()->bUseControllerDesiredRotation!=FacingBefore)return End(TEXT("F7 did not clean transient tennis/input/camera/AI/facing and resume normal on-foot player"));
   Capture(TEXT("Load-Cleanup"));return End(TEXT(""));
  }
  if(Stage==9 && Age>.25)
  {
   Key(EKeys::F,false);if(Court->IsActive() || Court->GetOpponent() || PC->GetViewTarget()!=P || P->FindComponentByClass<UIslandTennisParticipant>() || P->GetCharacterMovement()->bOrientRotationToMovement!=OrientBefore || P->GetCharacterMovement()->bUseControllerDesiredRotation!=FacingBefore)return End(TEXT("Normal F exit did not restore original on-foot input/camera/facing"));return Next(10);
  }
  if(Stage==10 && Age>.25)
  {
   PC->SetControlRotation((Court->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Court)return End(TEXT("Normal tennis re-entry prompt lost after F exit"));ExitChecked=true;Key(EKeys::F,true);return Next(2);
  }
  return false;
 }
 void Key(FKey K,bool Down){bool& Held=Keys.FindOrAdd(K);if(Held!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Capture(const TCHAR* Label){FNeonCityVisualEvidence::CaptureHUD(W,FString(TEXT("Tennis-"))+Label+TEXT(".png"));}
 bool End(const FString& Error)
 {
  if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));
  if(!Error.IsEmpty())Test->AddError(Error);FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("EstateTennis");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(AICSV,*(Dir/TEXT("actual-ai-trajectory.csv")));
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nstage=%d\nerror=%s\nplayer_contacts=%d\nai_contacts=%d\nOne declared supported initial approach fixture and one explicitly labelled temporary fixed hand/racket comparison camera. No player/AI/ball pose or velocity fixtures after normal F entry. Real CharacterMovement, input actions, animation, substepped world sweeps, scoring and F6/F7.\n"),Error.IsEmpty(),Stage,*Error,Court?Court->GetPlayerContacts():0,Court?Court->GetAIContacts():0),*(Dir/TEXT("scope.txt")));FFileHelper::SaveStringToFile(ContactCSV,*(Dir/TEXT("actual-racket-contact.csv")));if(Court)Court->EndSession();return true;
 }
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandEstateSport* Court=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 double Start=0;float At=0,PressAt=0;int32 Stage=0;bool Pressed=false,ExitChecked=false,OrientBefore=true,FacingBefore=false;TMap<FKey,bool> Keys;
 FString ContactCSV=TEXT("seconds,swing_age,ball_racket_x_cm,ball_racket_y_cm,ball_plane_distance_cm,player_contacts,ai_contacts,player_x,player_y,player_z,predicted_relative_x,predicted_relative_y,predicted_relative_z,pressed,player_speed,move_ignored\n");
 FString AICSV=TEXT("seconds,rally,service,first_landing,last_hitter,ai_swing_age,ball_x,ball_y,ball_z,vx,vy,vz,ai_x,ai_y,ai_z,predicted_relative_x,predicted_relative_y,predicted_relative_z,ball_string_x,ball_string_y,ball_string_z,ai_movement_mode,ai_vx,ai_vy,ai_vz\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEstateTennisPIE,"NeonCity.Phase205c.Estate.TennisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEstateTennisPIE::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(EstateTennisPIE::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
