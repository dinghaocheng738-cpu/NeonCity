#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandScenePanel.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
namespace LeisureNative
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,EIslandLeisureKind K,bool HandsOnly=false):Test(T),Kind(K),Hands(HandsOnly),Evidence(HandsOnly?TEXT("GymHands"):K==EIslandLeisureKind::Cinema?TEXT("Cinema"):K==EIslandLeisureKind::ExerciseBike?TEXT("Gym"):TEXT("Sauna")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>140)return End(TEXT("Native leisure use timed out"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
   for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)if(I->Kind==Kind)Facility=*I;if(!Facility || !Facility->ScenePanel || !Facility->Furniture->GetStaticMesh())return End(TEXT("Native leisure facility/lighting/furniture missing"));
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();const FString S=Hands?TEXT("GymHands245"):Kind==EIslandLeisureKind::Cinema?TEXT("Cinema"):Kind==EIslandLeisureKind::ExerciseBike?TEXT("Gym"):TEXT("Sauna");
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_Leisure")+S+TEXT("_")+FPaths::MakeValidFileName(Run.Left(22)),false) || Save->DoesSaveExist())return End(TEXT("Fresh leisure checkpoint required; retained evidence must not be overwritten"));
   FVector Local=Kind==EIslandLeisureKind::ExerciseBike?FVector(-20,85,0):Kind==EIslandLeisureKind::Sauna?FVector(0,-125,0):FVector(125,0,0);
   const FVector Ground=Facility->GetActorTransform().TransformPosition(Local);FCollisionQueryParams Q(SCENE_QUERY_STAT(LeisureActualStart),false,P);FHitResult H;
   if(!W->LineTraceSingleByChannel(H,Ground+FVector(0,0,200),Ground-FVector(0,0,100),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Leisure approach lacks real floor support"));
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Leisure actual approach obstructed"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Entry=Stand;Energy=P->GetPlayerDataComponent()->GetStamina();SceneBefore=Facility->ScenePanel->GetScene();return Next(1);
  }
  if(Stage==1 && Age>.4)
  {
   PC->SetControlRotation((Facility->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Facility)return End(TEXT("Normal F leisure prompt blocked"));Capture(TEXT("F-Prompt"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   Key(EKeys::F,false);if(!Facility->IsOccupied() || P->GetAttachParentActor()!=Facility || !Facility->Occupant->GetDriverMesh() || !Facility->IsFeedbackPlaying())return End(TEXT("Actual F failed visible occupied/audio gameplay chain"));
   if(Kind==EIslandLeisureKind::Cinema && Facility->ScenePanel->GetScene()!=2)return End(TEXT("Cinema did not dim its real room lighting"));return Next(3);
  }
  if(Stage==3)
  {
   if(Facility->GetStage()==EIslandLeisureStage::Active)
   {
    if(!Captured && Facility->GetElapsed()>6)
    {
     if(Kind==EIslandLeisureKind::Cinema)
     {
      FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);const FVector Target=Facility->Screen->Bounds.Origin;
      FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(CinemaNormalSeatedSightline),false,P);Q.AddIgnoredActor(Facility);
      // Interaction-only boxes are invisible prompt targets, not optical
      // obstructions. Keep all actual furniture/wall mesh collision in sight.
      for(TActorIterator<AIslandHomeSeat> Seat(W);Seat;++Seat)Q.AddIgnoredComponent(Seat->Interaction.Get());
      if(FVector::DotProduct(View.Vector(),(Target-Eye).GetSafeNormal())<.70 || W->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Q))return End(FString::Printf(TEXT("Actual seated cinema camera misses/occludes screen: eye%s view%s screen%s hit%s"),*Eye.ToString(),*View.ToString(),*Target.ToString(),*GetNameSafe(Hit.GetActor())));
     }
     Capture(TEXT("Active"));Captured=true;if(Hands){Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;Camera->SetActorLocation(Facility->GetActorTransform().TransformPosition(FVector(90,-90,130)));Camera->SetActorRotation((Facility->GetActorTransform().TransformPosition(FVector(42,0,109))-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);}
    }
    if(Hands && Camera && Facility->GetElapsed()>7 && !LeftCloseCaptured){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Gym245-Actual-Left-Grip-Sleeve.png"));LeftCloseCaptured=true;}
    if(Hands && Camera && Facility->GetElapsed()>9 && !RightCamera){Camera->SetActorLocation(Facility->GetActorTransform().TransformPosition(FVector(90,90,130)));Camera->SetActorRotation((Facility->GetActorTransform().TransformPosition(FVector(42,0,109))-Camera->GetActorLocation()).Rotation());RightCamera=true;}
    if(Hands && Camera && Facility->GetElapsed()>9.5f && !RightCloseCaptured){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Gym245-Actual-Right-Grip-Sleeve.png"));RightCloseCaptured=true;}
    if(Kind==EIslandLeisureKind::ExerciseBike && W->GetTimeSeconds()-SampleAt>.1)
    {
     SampleAt=W->GetTimeSeconds();const auto* B=Facility->Occupant->GetDriverMesh();const float L=FVector::Dist(B->GetSocketLocation(TEXT("foot_l")),Facility->LeftPedal->GetComponentLocation()),R=FVector::Dist(B->GetSocketLocation(TEXT("foot_r")),Facility->RightPedal->GetComponentLocation());
     Contact+=FString::Printf(TEXT("%.3f,%.3f,%.3f,%.4f\n"),Facility->GetElapsed(),L,R,Facility->GetCrankRadians());MaxContact=FMath::Max(MaxContact,FMath::Max(L,R));++Samples;
     if(Hands){for(const auto Side:{FString(TEXT("l")),FString(TEXT("r"))}){const float Y=Side==TEXT("l")?-27.f:27.f;const auto Pose=Facility->Furniture->GetComponentTransform();const FVector A=Pose.TransformPosition(FVector(36,Y,108)),C=Pose.TransformPosition(FVector(50,Y,110.4));for(const auto Digit:{FString(TEXT("thumb")),FString(TEXT("index")),FString(TEXT("middle")),FString(TEXT("ring")),FString(TEXT("pinky"))}){const FName Bone(*(Digit+TEXT("_03_")+Side));if(!B->DoesSocketExist(Bone))return End(TEXT("Actual rider lacks required finger bones"));const float Axis=FMath::PointDistToSegment(B->GetSocketLocation(Bone),A,C);HandsContact+=FString::Printf(TEXT("%.3f,%s,%.3f\n"),Facility->GetElapsed(),*Bone.ToString(),Axis);MaxHandAxis=FMath::Max(MaxHandAxis,Axis);}}}
    }
    if(!Saved && Facility->GetElapsed()>8){Key(EKeys::F6,true);Saved=true;SaveAt=W->GetTimeSeconds();}
    if(Saved && !SaveChecked && W->GetTimeSeconds()-SaveAt>.7)
    {
     Key(EKeys::F6,false);auto* Model=Save->ReadSaveForInspection();FTransform Exit;
     if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Facility->FindPersistenceExit(Exit) || FVector::Dist(Model->Player.Transform.GetLocation(),Exit.GetLocation())>5)return End(TEXT("Occupied facility F6 did not save real safe standing exit"));SaveChecked=true;
    }
   }
   if(Facility->IsOccupied())return false;
   if(!Captured || !SaveChecked || Facility->GetCompletedSessions()!=1 || P->IsHidden() || !P->GetActorEnableCollision() || !P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist(P->GetActorLocation(),Entry)>35)return End(TEXT("Finite leisure session failed completion or actual safe walking restore"));
   if(Kind==EIslandLeisureKind::Cinema && Facility->ScenePanel->GetScene()!=SceneBefore)return End(TEXT("Cinema failed to restore previous lighting scene"));
   if(Kind==EIslandLeisureKind::Cinema && PC->GetViewTarget()!=P)return End(TEXT("Cinema finite completion failed to restore normal player view"));
   if(Kind==EIslandLeisureKind::ExerciseBike && (Samples<100 || MaxContact>12 || P->GetPlayerDataComponent()->GetStamina()>Energy-8))return End(FString::Printf(TEXT("Actual cycling ankle/pedal contact or real energy expenditure failed: samples%d max%.2fcm energy%.2f"),Samples,MaxContact,P->GetPlayerDataComponent()->GetStamina()));
   if(Hands && MaxHandAxis>5.5f)return End(FString::Printf(TEXT("Native finger-bone proxy moved outside expected real grip axis: max%.3fcm"),MaxHandAxis));
   Capture(TEXT("Finished-Walking"));return Next(4);
  }
  if(Stage==4 && Age>.3){Key(EKeys::F7,true);return Next(5);}
  if(Stage==5 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || Facility->IsOccupied() || P->IsHidden() || P->GetAttachParentActor() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("F7 failed to clear transient facility use safely"));return End(TEXT(""));
  }
  return false;
 }
 void Capture(const TCHAR* S){FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Leisure-%s-%s.png"),Kind==EIslandLeisureKind::Cinema?TEXT("Cinema"):Kind==EIslandLeisureKind::ExerciseBike?TEXT("Gym"):TEXT("Sauna"),S));}
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Held=Keys.FindOrAdd(K);if(Held!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 bool End(const FString& Error)
 {
  if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/(Hands?TEXT("GymHands"):Kind==EIslandLeisureKind::Cinema?TEXT("Cinema"):Kind==EIslandLeisureKind::ExerciseBike?TEXT("Gym"):TEXT("Sauna"));IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(Contact,*(Dir/TEXT("pedal-contact.csv")));if(Hands){FFileHelper::SaveStringToFile(HandsContact,*(Dir/TEXT("hands-contact.csv")));FFileHelper::SaveStringToFile(FString::Printf(TEXT("max_actual_03_bone_head_distance_to_grip_axis_cm=%.3f\nreal_grip_radius_cm=2.7\nBone HEAD proxies are not skin fingertips; native close-frame skin review is separately required.\n"),MaxHandAxis),*(Dir/TEXT("hands-scope.txt")));}FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nstage=%d\nfailure=%s\nOne declared supported start only. Normal F use, actual finite completion/audio/lighting/equipment motion, real F6 standing-safe checkpoint and F7 transient cleanup. No elapsed/energy/state injection. Independent cold process and manual subjective review remain separate.\n"),Error.IsEmpty(),Stage,*Error),*(Dir/TEXT("scope.txt")));return true;
 }
 FAutomationTestBase* Test;EIslandLeisureKind Kind;bool Hands=false;ACameraActor* Camera=nullptr;float MaxHandAxis=0;FString HandsContact=TEXT("seconds,bone_head,distance_to_grip_axis_cm\n");int Stage=0,SceneBefore=0,Samples=0;double Start=0;float At=0,Energy=0,SampleAt=0,MaxContact=0,SaveAt=0;bool Captured=false,Saved=false,SaveChecked=false,LeftCloseCaptured=false,RightCloseCaptured=false,RightCamera=false;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLeisureFacility* Facility=nullptr;UNeonCitySaveSubsystem* Save=nullptr;FVector Entry;TMap<FKey,bool> Keys;FString Contact=TEXT("seconds,left_cm,right_cm,crank_radians\n");FNeonCityVisualEvidence Evidence;
};
}
#define LEISURE_TEST(Class,Name,Kind) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(LeisureNative::FFlow(this,EIslandLeisureKind::Kind));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
LEISURE_TEST(FCinemaNative,"NeonCity.Phase205c.Leisure.CinemaPIE",Cinema)
LEISURE_TEST(FGymNative,"NeonCity.Phase205c.Leisure.GymPIE",ExerciseBike)
LEISURE_TEST(FSaunaNative,"NeonCity.Phase205c.Leisure.SaunaPIE",Sauna)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGymHandsNative,"NeonCity.Phase205c.Leisure.GymHandsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGymHandsNative::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(LeisureNative::FFlow(this,EIslandLeisureKind::ExerciseBike,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
