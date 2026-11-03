#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/IslandLandDoor.h"
#include "Interaction/InteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace TheOnePhotoEntry
{
class FMinimum final : public IAutomationLatentCommand
{
public:
 explicit FMinimum(FAutomationTestBase* In,bool InDiagnosis=false):Test(In),Diagnosis(InDiagnosis){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>110)return Finish(TEXT("Photo entry minimum exceeded110seconds"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_GroundLiftPit481")){Pose=It->GetActorTransform();IslandLevel=It->GetLevel();break;}
   if(!IslandLevel)return Finish(TEXT("Estate source frame missing"));
   for(TActorIterator<AIslandLandDoor> It(W);It;++It)if(It->GetLevel()==IslandLevel && It->GetFName()==TEXT("IslandLandDoor_3")){Door=*It;break;}
   if(!Door || !FMath::IsNearlyEqual(Door->Width,230.f,.1f))return Finish(TEXT("Original230cm main entry door missing"));
   int32 MeshCount=0;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
   {
    if(It->GetLevel()!=IslandLevel || !It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.PhotoEntry575")))continue;
    auto* C=It->GetStaticMeshComponent();if(!C->GetStaticMesh())return Finish(TEXT("Entry actor has no mesh"));const FString N=C->GetStaticMesh()->GetName();
    if(!N.StartsWith(TEXT("SM_TheOne_PhotoEntry")) || !N.EndsWith(TEXT("574")))return Finish(TEXT("Superseded entry geometry installed"));++MeshCount;
    const bool Visual=N.Contains(TEXT("Water")) || N.Contains(TEXT("GlassArt"));
    if(Visual?C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision:C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block)return Finish(TEXT("Entry material/physical collision role mismatch"));
    for(int32 I=0;I<C->GetNumMaterials();++I)if(!C->GetMaterial(I))return Finish(TEXT("Entry material slot empty"));
   }
   if(MeshCount!=8)return Finish(FString::Printf(TEXT("Expected eight current entry meshes, found%d"),MeshCount));
   FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOnePhotoEntry),true,P);
   for(FVector Here:{FVector(-26.5,24.7,3.048),FVector(-26.5,28,3.048),FVector(-24,28,3.048),FVector(-22,28,3.048),FVector(-20,28,3.048),FVector(-18.4,28,3.048),FVector(-16,28,3.048)})
   {FHitResult Hit;if(!W->LineTraceSingleByChannel(Hit,S(Here)+FVector(0,0,40),S(Here)-FVector(0,0,30),ECC_Pawn,Q) || FMath::Abs(Hit.ImpactPoint.Z-S(Here).Z)>2.2)return Finish(FString::Printf(TEXT("Supported entry floor missing at%s, hit%s"),*Here.ToString(),*GetNameSafe(Hit.GetActor())));++Floors;}
   for(FVector Here:{FVector(-26.5,24.7,3.048),FVector(-26.5,28,3.048),FVector(-24,28,3.048),FVector(-22,28,3.048),FVector(-20,28,3.048),FVector(-19.5,28,3.048)})
   {const FVector C=S(Here)+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);FHitResult Hit;if(W->SweepSingleByChannel(Hit,C,C+FVector(.1,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return Finish(FString::Printf(TEXT("Entry bridge clearance blocked by%s at%s"),*GetNameSafe(Hit.GetActor()),*Here.ToString()));++Clearances;}
   for(ECollisionChannel Channel:{ECC_Pawn,ECC_Vehicle})
   {FHitResult Hit;if(!W->SweepSingleByChannel(Hit,S(FVector(-26.4,26,3.65)),S(FVector(-28,26,3.65)),FQuat::Identity,Channel,FCollisionShape::MakeSphere(35),Q) || !Hit.GetActor() || !Hit.GetActor()->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.PhotoEntry575")))return Finish(TEXT("Transparent exposed entry edge has no real player/vehicle collision"));++Guards;}
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OldTime=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);break;}
   if(!Clock)return Finish(TEXT("Native time-of-day cycle missing"));
   P->SetActorLocation(S(Diagnosis?FVector(-19.7,28,3.048):FVector(-26.5,24.7,3.048))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Last=P->GetActorLocation();return Next(Diagnosis?2:1);
  }
  WalkDistance+=FVector::Dist2D(Last,P->GetActorLocation());Last=P->GetActorLocation();const float Age=W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   const FVector Goals[]={FVector(-26.5,28,3.048),FVector(-19.5,28,3.048)};
   if(Age>25)return Finish(FString::Printf(TEXT("Physical entry bridge walk stalled at%s"),*P->GetActorLocation().ToString()));
   if(Walk(Goals[Waypoint])){Key(EKeys::W,false);if(++Waypoint==2)return Next(2);}return false;
  }
  if(Stage==2)
  {
   FVector Eye;FRotator Rotation;P->GetActorEyesViewPoint(Eye,Rotation);PC->SetControlRotation((Door->Leaf->Bounds.Origin-Eye).Rotation());
   if(Age<.4)return false;
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Door)return Finish(TEXT("Normal player eye trace cannot focus main door through the entry composition"));
   TArray<UTextRenderComponent*> Prompts;Door->GetComponents(Prompts);for(auto* T:Prompts)if(T->IsVisible() && T->Text.ToString().Contains(TEXT("F")))PromptVisible=true;
   if(!PromptVisible && Age<1)return false;if(!PromptVisible)return Finish(TEXT("Physical main-door interaction prompt did not appear"));
   if(!FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-PhotoEntry-NativeDoorPrompt.png")))return Finish(TEXT("Normal door prompt capture unavailable"));
   Key(EKeys::F,true);return Next(3);
  }
  if(Stage==3)
  {
   if(Age>.15)Key(EKeys::F,false);
   if(Diagnosis && Age>3)
   {
    const float Yaw=Door->Leaf->GetRelativeRotation().Yaw;
    const FTransform NextPose=FTransform(FRotator(0,Yaw+FMath::Sign(Door->OpenAngle)*2,0))*Door->GetActorTransform();FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneEntrySwingDiagnosis),false,Door);FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);TArray<FOverlapResult> Hits;
    W->OverlapMultiByObjectType(Hits,NextPose.TransformPosition(FVector(Door->Width*.5f,0,Door->Height*.5f)),NextPose.GetRotation(),Objects,FCollisionShape::MakeBox(FVector(Door->Width*.5f,6,Door->Height*.5f)),Q);
    for(const auto& Hit:Hits)if(Hit.GetActor()==P)OpeningPlayerBlocked=true;
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-PhotoEntry-DoorSwingDiagnosis.png"));
    return Finish(OpeningPlayerBlocked && FMath::Abs(Yaw)>1 && FMath::Abs(Yaw)<80?TEXT(""):FString::Printf(TEXT("Door failure had another cause: yaw%.3f opening-player-overlap%d"),Yaw,OpeningPlayerBlocked));
   }
   if(Age>6)return Finish(FString::Printf(TEXT("Mapped F door did not open: actual leaf yaw%.3f player%s"),Door->Leaf->GetRelativeRotation().Yaw,*P->GetActorLocation().ToString()));if(!Door->IsOpen())return false;DoorOpened=true;return Next(4);
  }
  if(Stage==4)
  {
   if(Age>15)return Finish(FString::Printf(TEXT("Open main doorway physically blocked at%s"),*P->GetActorLocation().ToString()));
   if(!Walk(FVector(-15.7,28,3.048)))return false;Key(EKeys::W,false);if(!P->GetCharacterMovement()->IsMovingOnGround() || WalkDistance<1200)return Finish(TEXT("Continuous native entry walk lost floor support or did not cross into mansion"));
   Entered=true;Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=62;PC->SetViewTarget(Camera);return SetShot(0);
  }
  if(Stage==5)
  {
   if(Age<3.5)return false;const TCHAR* Views[]={TEXT("Court"),TEXT("Bridge"),TEXT("Aerial")};const TCHAR* Times[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};
   if(!FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("TheOne-PhotoEntry-%s-%s.png"),Views[Shot%3],Times[Shot/3])))return Finish(TEXT("Native photo entry comparison capture unavailable"));++Images;
   return Shot<8?SetShot(Shot+1):Finish(TEXT(""));
  }
  return false;
 }
private:
 FVector S(FVector V)const{return Pose.TransformPosition(FVector(V.X*100,-V.Y*100,V.Z*100));}
 bool Next(int32 V){Stage=V;At=W->GetTimeSeconds();return false;}
 bool Walk(FVector Goal){const FVector Delta=S(Goal)-P->GetActorLocation();if(Delta.Size2D()<35)return true;PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));Key(EKeys::W,true);return false;}
 bool SetShot(int32 Index)
 {Shot=Index;const FVector Eyes[]={FVector(-38,35,6.2),FVector(-27,31.5,4.8),FVector(-35,37,17)};const FVector Aims[]={FVector(-19,26,7),FVector(-18.5,27.2,4.8),FVector(-22,27,4)};Camera->SetActorLocation(S(Eyes[Shot%3]));Camera->SetActorRotation((S(Aims[Shot%3])-Camera->GetActorLocation()).Rotation());Clock->SetTimeOfDay(Shot/3==0?12:Shot/3==1?17.8:22);return Next(5);}
 void Key(FKey K,bool Down){bool& Held=K==EKeys::W?HeldW:HeldF;if(Held==Down || !PC)return;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}
 bool Finish(const FString& Error)
 {
  Key(EKeys::W,false);Key(EKeys::F,false);if(!Error.IsEmpty())Test->AddError(Error);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldTime);Clock->SetCycleEnabled(OldCycle);}
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetBoolField(TEXT("diagnosis_only"),Diagnosis);J->SetBoolField(TEXT("opening_leaf_next_volume_contains_player"),OpeningPlayerBlocked);if(Door)J->SetNumberField(TEXT("actual_door_leaf_yaw"),Door->Leaf->GetRelativeRotation().Yaw);J->SetNumberField(TEXT("floor_probes"),Floors);J->SetNumberField(TEXT("capsule_clearance_probes"),Clearances);J->SetNumberField(TEXT("transparent_guard_channel_sweeps"),Guards);J->SetBoolField(TEXT("native_prompt_visible"),PromptVisible);J->SetBoolField(TEXT("mapped_F_door_animated_open"),DoorOpened);J->SetBoolField(TEXT("normal_W_crossed_into_mansion"),Entered);J->SetNumberField(TEXT("native_walk_cm"),WalkDistance);J->SetNumberField(TEXT("comparison_images"),Images);J->SetNumberField(TEXT("initial_player_fixtures"),1);J->SetNumberField(TEXT("route_player_pose_writes"),0);J->SetStringField(TEXT("scope"),TEXT("Photo entry region only; complete photographed estate and final visual acceptance pending"));
  FString Text;const auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(J,Writer);FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("UI");IFileManager::Get().MakeDirectory(*Directory,true);FFileHelper::SaveStringToFile(Text,*(Directory/(Diagnosis?TEXT("TheOne-PhotoEntry-DoorDiagnosis.json"):TEXT("TheOne-PhotoEntry.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;ULevel* IslandLevel=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandDoor* Door=nullptr;ANeonCityDayNightCycle* Clock=nullptr;ACameraActor* Camera=nullptr;FTransform Pose;FVector Last;double Started=0,WalkDistance=0;float At=0,OldTime=12;bool OldCycle=true,HeldW=false,HeldF=false,PromptVisible=false,DoorOpened=false,Entered=false,Diagnosis=false,OpeningPlayerBlocked=false;int32 Stage=0,Waypoint=0,Shot=0,Images=0,Floors=0,Clearances=0,Guards=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOnePhotoEntryMinimum,"NeonCity.Phase205cUpgrade.TheOnePhotoEntryMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOnePhotoEntryMinimum::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePhotoEntry::FMinimum(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOnePhotoEntryDoorDiagnosis,"NeonCity.Phase205cUpgrade.TheOnePhotoEntryDoorDiagnosisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOnePhotoEntryDoorDiagnosis::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePhotoEntry::FMinimum(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
namespace TheOnePhotoEntry
{
class FSetback final : public IAutomationLatentCommand
{
public:
 explicit FSetback(FAutomationTestBase* In,bool InVisualOnly=false):Test(In),VisualOnly(InVisualOnly){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>85)return Finish(TEXT("Entry setback check exceeded85seconds"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_GroundLiftPit481")){Pose=It->GetActorTransform();IslandLevel=It->GetLevel();break;}
   if(!IslandLevel)return Finish(TEXT("Estate source frame unavailable"));
   if(VisualOnly)
   {
    int32 Frames=0;for(TActorIterator<AStaticMeshActor> It(W);It;++It)
    {
     auto* Component=It->GetStaticMeshComponent();if(It->GetLevel()!=IslandLevel || !Component->GetStaticMesh() || !Component->GetStaticMesh()->GetName().EndsWith(TEXT("OpenFrames614")))continue;
     ++Frames;for(int32 I=0;I<Component->GetNumMaterials();++I)if(!Component->GetMaterial(I))return Finish(TEXT("Current facade profile material missing"));
    }
    if(Frames!=3)return Finish(TEXT("Three current open facade profile assets required"));
    if(!InitClock())return Finish(TEXT("Native daylight cycle missing"));
    Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=62;PC->SetViewTarget(Camera);return Shot(0);
   }
   int32 Count=0;for(TActorIterator<AStaticMeshActor> It(W);It;++It)
   {
    auto* C=It->GetStaticMeshComponent();if(It->GetLevel()!=IslandLevel || !C->GetStaticMesh() || !C->GetStaticMesh()->GetName().EndsWith(TEXT("Setback600")) && !C->GetStaticMesh()->GetName().EndsWith(TEXT("SetbackGlass600")) && !C->GetStaticMesh()->GetName().EndsWith(TEXT("SetbackMetal600")))continue;
    ++Count;if(C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block || C->GetCollisionResponseToChannel(ECC_Vehicle)!=ECR_Block)return Finish(TEXT("Setback structural/guard collision channel missing"));for(int32 I=0;I<C->GetNumMaterials();++I)if(!C->GetMaterial(I))return Finish(TEXT("Setback material slot empty"));
   }
   if(Count!=6)return Finish(FString::Printf(TEXT("Six current setback assets required, found%d"),Count));
   FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneEntrySetback),true,P);
   for(FVector Here:{FVector(-33,20.9,7.62),FVector(-27,20.9,7.62),FVector(-10,28,7.62),FVector(-31,20,12.04),FVector(-3,30.1,12.04),FVector(-26,2,15.04),FVector(-9,19,15.04),FVector(-22,28,3.048)})
   {FHitResult H;if(!W->LineTraceSingleByChannel(H,S(Here)+FVector(0,0,30),S(Here)-FVector(0,0,30),ECC_Pawn,Q) || FMath::Abs(H.ImpactPoint.Z-S(Here).Z)>2.2)return Finish(FString::Printf(TEXT("Preserved supported floor missing at%s, hit%s"),*Here.ToString(),*GetNameSafe(H.GetActor())));++Floors;}
   for(FVector Here:{FVector(-33,25,4.8),FVector(-25,25,4.8),FVector(-25,30,4.8)})
   {FHitResult H;if(W->LineTraceSingleByChannel(H,S(Here),S(FVector(Here.X,Here.Y,14)),ECC_Pawn,Q))return Finish(FString::Printf(TEXT("Photographed entry court still covered at%s by%s"),*Here.ToString(),*GetNameSafe(H.GetActor())));++OpenSky;}
   FString Text;TSharedPtr<FJsonObject> Source;
   if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/TheOne600cEntrySetback/receipt.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Source))return Finish(TEXT("Proven setback boundary receipt missing"));
   const auto EdgeGroups=Source->GetObjectField(TEXT("new_guard_edges_m"));
   for(const auto& Group:EdgeGroups->Values)for(const auto& Item:Group.Value->AsArray())
   {
    const auto Pair=Item->AsArray();auto V=[](const TSharedPtr<FJsonValue>& Value){const auto A=Value->AsArray();return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());};const FVector A=V(Pair[0]),B=V(Pair[1]);const int32 N=FMath::Max(1,FMath::CeilToInt(FVector::Dist(A,B)/1.5));const FVector Inside=FMath::IsNearlyEqual(A.X,B.X,.001)?FVector(1,0,0):FVector(0,-1,0);
    for(int32 I=0;I<N;++I)
    {
     const FVector Mid=FMath::Lerp(A,B,(I+.5f)/N);FHitResult Floor;
     if(!W->LineTraceSingleByChannel(Floor,S(Mid+Inside*.15f)+FVector(0,0,25),S(Mid+Inside*.15f)-FVector(0,0,30),ECC_Pawn,Q) || FMath::Abs(Floor.ImpactPoint.Z-S(Mid).Z)>2.2)return Finish(FString::Printf(TEXT("Setback guard support missing at%s inside%s hit%s actualZ%.3f expectedZ%.3f"),*Mid.ToString(),*Inside.ToString(),*GetNameSafe(Floor.GetActor()),Floor.ImpactPoint.Z,S(Mid).Z));++EdgeSupport;
     const FVector Eye=Mid+FVector(0,0,.60);
     // Query the actual pane's physics body directly. The preserved window
     // lies17.6cm behind it and the black feature covers part of its front;
     // neither must substitute for proving that this glass really collides.
     UStaticMeshComponent* Pane=nullptr;
     for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetLevel()==IslandLevel && It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==(Mid.Z>10?TEXT("SM_TheOne_RoofSetbackGlass600"):TEXT("SM_TheOne_TerraceSetbackGlass600"))){Pane=It->GetStaticMeshComponent();break;}
     FHitResult Glass;if(!Pane || !Pane->LineTraceComponent(Glass,S(Eye-Inside*.65f),S(Eye+Inside*.65f),Q))return Finish(TEXT("Visible new setback glazing has no physical trace"));
     const auto* GA=Cast<AStaticMeshActor>(Glass.GetActor());if(!GA || !GA->GetStaticMeshComponent()->GetStaticMesh()->GetName().Contains(TEXT("SetbackGlass600")))return Finish(FString::Printf(TEXT("Expected actual glass pane at%s, got%s"),*Mid.ToString(),*GetNameSafe(Glass.GetActor())));++GlassTraces;
     for(ECollisionChannel Channel:{ECC_Pawn,ECC_Vehicle})
     {FHitResult H;if(!W->SweepSingleByChannel(H,S(Eye-Inside*.8f),S(Eye+Inside*.8f),FQuat::Identity,Channel,FCollisionShape::MakeSphere(35),Q))return Finish(TEXT("Setback fall protection has a capsule-width collision gap"));++GuardSweeps;}
    }
   }
   if(!InitClock())return Finish(TEXT("Native daylight cycle missing"));
   P->SetActorLocation(S(FVector(-33,20.9,7.62))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();++Fixtures;Last=P->GetActorLocation();return Next(1);
  }
  if(!VisualOnly){WalkDistance+=FVector::Dist2D(Last,P->GetActorLocation());Last=P->GetActorLocation();}const float Age=W->GetTimeSeconds()-At;
  if(Stage==1)
  {
   const FVector Delta=S(FVector(-27,20.9,7.62))-P->GetActorLocation();if(Age>14)return Finish(FString::Printf(TEXT("Continuous W setback circulation blocked at%s"),*P->GetActorLocation().ToString()));
   if(Delta.Size2D()>35){PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));Key(true);return false;}Key(false);
   if(!P->GetCharacterMovement()->IsMovingOnGround() || WalkDistance<550)return Finish(TEXT("Setback circulation lost support"));Walked=true;Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=62;PC->SetViewTarget(Camera);return Shot(0);
  }
  if(Stage==2)
  {
   if(Age<3.5)return false;const TCHAR* Views[]={TEXT("Court"),TEXT("Bridge"),TEXT("Aerial")};const TCHAR* Times[]={TEXT("Day"),TEXT("Sunset"),TEXT("Night")};
   const TCHAR* Rooms[]={TEXT("EntryGallery"),TEXT("FamilyLiving"),TEXT("WestOffice"),TEXT("PrincipalBedroom"),TEXT("SpaWing")};
   const FString Name=Index<9?FString::Printf(TEXT("TheOne-%s-%s-%s.png"),VisualOnly?TEXT("FacadeStyle"):TEXT("EntrySetback"),Views[Index%3],Times[Index/3]):FString::Printf(TEXT("TheOne-FacadeStyle-%s-Day.png"),Rooms[Index-9]);
   if(!FNeonCityVisualEvidence::CaptureHUD(W,Name))return Finish(TEXT("Rendered evidence unavailable"));++Images;return Index<(VisualOnly?13:8)?Shot(Index+1):Finish(TEXT(""));
  }
  return false;
 }
private:
 FVector S(FVector V)const{return Pose.TransformPosition(FVector(V.X*100,-V.Y*100,V.Z*100));}
 bool Next(int32 Value){Stage=Value;At=W->GetTimeSeconds();return false;}
 bool InitClock(){for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){Clock=*It;OldTime=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);break;}return Clock!=nullptr;}
 bool Shot(int32 Value)
 {
  Index=Value;const FVector Eyes[]={FVector(-38,35,6.2),FVector(-27,31.5,4.8),FVector(-35,37,17)};const FVector Aims[]={FVector(-19,26,7),FVector(-18.5,27.2,4.8),FVector(-22,27,4)};
  const FVector RoomEyes[]={FVector(-12,27,4.65),FVector(6,-5,4.65),FVector(-17,26,4.65),FVector(20,-6.8,9.25),FVector(29,-35,1.6)};
  const FVector RoomAims[]={FVector(-12,15,4.65),FVector(12,-10,4.6),FVector(-11,22,4.65),FVector(17,-11,8.8),FVector(32,-40,1.3)};
  Camera->SetActorLocation(S(Index<9?Eyes[Index%3]:RoomEyes[Index-9]));Camera->SetActorRotation((S(Index<9?Aims[Index%3]:RoomAims[Index-9])-Camera->GetActorLocation()).Rotation());Clock->SetTimeOfDay(Index>=9 || Index/3==0?12:Index/3==1?17.8:22);return Next(2);
 }
 void Key(bool Down){if(HeldW==Down || !PC)return;PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));HeldW=Down;}
 bool Finish(const FString& Error)
 {
  Key(false);if(!Error.IsEmpty())Test->AddError(Error);if(PC && P)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(Clock){Clock->SetTimeOfDay(OldTime);Clock->SetCycleEnabled(OldCycle);}
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("preserved_floor_probes"),Floors);J->SetNumberField(TEXT("open_sky_probes"),OpenSky);J->SetNumberField(TEXT("new_guard_supported_samples"),EdgeSupport);J->SetNumberField(TEXT("new_glass_physical_traces"),GlassTraces);J->SetNumberField(TEXT("new_guard_pawn_vehicle_sweeps"),GuardSweeps);J->SetBoolField(TEXT("native_W_supported_edge_walk"),Walked);J->SetNumberField(TEXT("native_walk_cm"),WalkDistance);J->SetNumberField(TEXT("executed_initial_player_fixtures"),Fixtures);J->SetNumberField(TEXT("route_player_pose_writes"),0);J->SetNumberField(TEXT("comparison_images"),Images);J->SetBoolField(TEXT("visual_evidence_only"),VisualOnly);J->SetStringField(TEXT("scope"),VisualOnly?TEXT("Facade lighting and 5 current interior comparison views only. Cached gameplay tests not repeated; whole estate photograph acceptance pending."):TEXT("Changed external entry setback only; unchanged normal entry598 cached. Whole estate photograph acceptance pending."));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("UI");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Text,*(Dir/(VisualOnly?TEXT("TheOne-FacadeStyle.json"):TEXT("TheOne-EntrySetback.json"))));return true;
 }
 FAutomationTestBase* Test;const bool VisualOnly;UWorld* W=nullptr;ULevel* IslandLevel=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;ANeonCityDayNightCycle* Clock=nullptr;ACameraActor* Camera=nullptr;FTransform Pose;FVector Last;double Started=0,WalkDistance=0;float At=0,OldTime=12;bool OldCycle=true,HeldW=false,Walked=false;int32 Stage=0,Index=0,Images=0,Floors=0,OpenSky=0,EdgeSupport=0,GlassTraces=0,GuardSweeps=0,Fixtures=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneEntrySetbackMinimum,"NeonCity.Phase205cUpgrade.TheOneEntrySetbackMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneEntrySetbackMinimum::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePhotoEntry::FSetback(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneFacadeStyleEvidence,"NeonCity.Phase205cUpgrade.TheOneFacadeStyleEvidencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneFacadeStyleEvidence::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(TheOnePhotoEntry::FSetback(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
