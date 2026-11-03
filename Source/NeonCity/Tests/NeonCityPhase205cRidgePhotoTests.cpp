#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandActivityJob.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeRidgePhotography
{
/** This is a new authored photography contract, not a rerun of the passed
 * village photo test. Only A declares one floor/capsule-verified player start.
 * All later movement, camera composition, capture, review and settlement use
 * the existing normal gameplay path. B requires an independent real process. */
class FFlow : public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold),Evidence(Cold?TEXT("RidgePhotoCold"):TEXT("RidgePhotoSource")){}
 ~FFlow(){if(Driver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Driver);}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>(bCold?100:1300))return End(TEXT("Ridge photography exceeded its bounded normal-flow timeout"));if(!P || W->GetTimeSeconds()<4)return false;
  const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);Observe();
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(45500,-119200,30300))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Photo.RidgeHouse"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job && It->Index==1)Clerk=*It;
   auto* GI=W->GetGameInstance();Phone=PC->FindComponentByClass<UPhoneComponent>();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=GI->GetSubsystem<UNeonCityEconomySubsystem>();
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCityRidgePhotoRun="),Suffix);if(Suffix.IsEmpty())Suffix=TEXT("01");if(Suffix.Len()>20)return End(TEXT("Ridge photo suffix exceeds20 characters"));for(TCHAR C:Suffix)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))return End(TEXT("Ridge photo suffix must use letters/digits/underscore/hyphen"));
   Slot=TEXT("NeonCity_Test_RidgePhotoDone_")+Suffix;Bridge=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("RidgePhoto-")+Suffix)/TEXT("Source.json");
   if(!Job || Job->Kind!=ENeonCityIslandActivity::Photography || Job->Reward!=220 || !Clerk || !Phone || !Save || !Wallet || !Save->ConfigureSlot(Slot,false))return End(TEXT("Authored ridge photography/220reward/clerk/phone/save domains missing"));
   Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FFlow::DriveFrame);
   if(bCold){if(!Save->DoesSaveExist() || !ReadBridge())return End(TEXT("Independent genuine RidgePhoto source checkpoint and manifest required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist() || IFileManager::Get().FileExists(*Bridge))return End(TEXT("Preserve prior genuine RidgePhoto checkpoint; choose a fresh explicit suffix after a code change"));
   if(Job->BuildOpportunitySnapshot(false).State==1 || Job->BuildOpportunitySnapshot(false).ActivityCompletions!=0)return End(TEXT("Ridge source requires its unused authored contract, not an injected reset"));
   const FVector Pickup=Job->GetActorTransform().TransformPosition(Job->PickupLocal);
   if(!StandingFloor(Pickup+FVector(-350,-250,0),StartPosition) || !StandingFloor(Pickup+FVector(-100,150,0),PhotoPosition))return End(TEXT("Real ridge forecourt photo approach lacks supported full-capsule clearance"));
   // The sole declared player-only content start. No later pose correction.
   P->SetActorLocation(StartPosition,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();PhoneGoal=0;Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();const auto D=Job->BuildOpportunitySnapshot(false);Before=Wallet->GetBalance();FString Hash;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || D.State!=2 || D.ActivityCompletions!=1 || D.ActivityRunID!=Run || D.ActivityPhotoFile!=PhotoFile || Before!=ExpectedBalance || !Wallet->HasCredit(Job->GetReceiptID()) || !ReadPNG(Hash) || Hash!=PhotoHash || FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>25)return End(TEXT("Independent F7 lost original ridge PNG/hash/run/one220credit or used a remote player fallback"));
   const auto* Saved=M->Phone.Opportunities.FindByPredicate([&](const auto& V){return V.OpportunityID==Job->ActivityID;});if(!Saved || Saved->ActivityRunID!=Run || Saved->ActivityPhotoFile!=PhotoFile || Saved->ActivityCompletions!=1)return End(TEXT("Genuine ridge save does not retain its original image and settled run"));
   PhoneGoal=2;Key(EKeys::P,true);return Next(1);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P did not open photography phone"));Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);Browsed=0;return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){if(PhoneGoal==0)FNeonCityVisualEvidence::CaptureHUD(W,TEXT("RidgePhoto-Native-Offer.png"));Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Real P/O list cannot discover the ridge photography contract/action"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);
   if(PhoneGoal==1)return Next(9);
   if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run)return End(TEXT("Native ENTER did not accept a new real ridge photography run"));
   if(PhoneGoal==2){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Ridge replay duplicated prior income"));Key(EKeys::Delete,true);return Next(21);}
   Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==6 && Age>.2){Key(EKeys::P,false);if(Phone->IsPhoneOpen() || !Phase205cSupportedWalkPath(W,P,PhotoPosition,Path))return End(TEXT("Actual ridge photo point has no normal supported walking approach"));Node=0;LastProgress=W->GetTimeSeconds();return Next(7);}
  if(Stage==7)
  {
   if(!WalkPath())return WalkFailure.IsEmpty()?false:End(WalkFailure);if(!P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist2D(P->GetActorLocation(),StartPosition)<180)return End(TEXT("Photo was not reached by a real on-foot approach"));bCompose=true;return Next(8);
  }
  if(Stage==8)
  {
   Compose();if(Age<2)return false;const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(),Target=PhotoTarget();if(FVector::DotProduct((Target-Eye).GetSafeNormal(),PC->PlayerCameraManager->GetCameraRotation().Vector())<.97)return End(TEXT("Normal ridge player camera did not compose the real subject"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("RidgePhoto-Native-Actual-Composition.png"));PhoneGoal=1;Key(EKeys::P,true);return Next(1);
  }
  if(Stage==9)
  {
   const auto D=Job->BuildOpportunitySnapshot(false);if(D.ActivityStage==0 && Age<7)return false;PhotoFile=D.ActivityPhotoFile;
   if(D.State!=1 || D.ActivityStage!=1 || !ReadPNG(PhotoHash) || FPaths::GetBaseFilename(PhotoFile)!=Run.ToString(EGuidFormats::Digits) || Wallet->GetBalance()!=Before || PC->HiddenActors.Contains(P) || Job->GetOpportunityData().PreviewImageFile!=PhotoFile || !Phone->IsPhoneOpen() || !Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString()))return End(TEXT("Real composed ridge capture/review did not produce an unpaid current-run1600x900 PNG or restore normal player visibility"));
   bCompose=false;return Next(10);
  }
  if(Stage==10 && Age>.8){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("RidgePhoto-Native-Actual-PNG-Preview.png"));Key(EKeys::P,true);return Next(11);}
  if(Stage==11 && Age>.2)
  {
   Key(EKeys::P,false);if(Phone->IsPhoneOpen() || !BuildRoadReturn())return End(TEXT("Current surveyed public road lacks a supported ridge-to-road connection"));LastProgress=W->GetTimeSeconds();return Next(12);
  }
  if(Stage==12)
  {
   if(Job->BuildOpportunitySnapshot(false).State!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Actual ridge photo contract expired/paid before normal clerk return"));
   if(!WalkPath())return WalkFailure.IsEmpty()?false:End(WalkFailure);
   if(ReturnRoadStations<FirstRoad-LastRoad-2 || WalkDistance<200000)return End(TEXT("Normal ridge return omitted the actual surveyed mountain road"));
   const FVector Goal=Clerk->GetActorLocation()+FVector(0,-150,0);if(!Phase205cSupportedWalkPath(W,P,Goal,Path))return End(TEXT("Actual road foot has no supported capsule-clear village path to photography clerk"));Node=0;LastProgress=W->GetTimeSeconds();return Next(13);
  }
  if(Stage==13){if(!WalkPath())return WalkFailure.IsEmpty()?false:End(WalkFailure);return Next(14);}
  if(Stage==14 && Age>.5)
  {
   if(!P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Ridge submission requires normal static on-foot clerk approach"));PC->SetControlRotation((Clerk->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Clerk)return End(TEXT("Actual ridge photo clerk normal F eye trace inaccessible"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("RidgePhoto-Native-Submission-Prompt.png"));Key(EKeys::F,true);return Next(15);
  }
  if(Stage==15 && Age>.2){Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityRunID!=Run || D.ActivityCompletions!=1 || D.ActivityPhotoFile!=PhotoFile || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Normal ridge PNG submission did not settle exactly one220credit"));return Next(16);}
  if(Stage==16 && Age>.2){Key(EKeys::F,true);return Next(17);}
  if(Stage==17 && Age>.2){Key(EKeys::F,false);if(Wallet->GetBalance()!=Before+Job->Reward || Job->BuildOpportunitySnapshot(false).ActivityCompletions!=1)return End(TEXT("Separate repeated F duplicated ridge photo payment"));Key(EKeys::F6,true);return Next(18);}
  if(Stage==18 && Age>.8)
  {
   Key(EKeys::F6,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();FString Hash;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityRunID==Run && D.ActivityCompletions==1 && D.ActivityPhotoFile==PhotoFile;}) || !ReadPNG(Hash) || Hash!=PhotoHash || !WriteBridge())return End(TEXT("Genuine ridge F6 lost the actual PNG/run/onepayment checkpoint"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("RidgePhoto-Native-Completed-F6.png"));return End(TEXT(""));
  }
  if(Stage==21 && Age>.2){Key(EKeys::Delete,false);FString Hash;if(Job->BuildOpportunitySnapshot(false).State==1 || Job->BuildOpportunitySnapshot(false).ActivityCompletions!=1 || Wallet->GetBalance()!=Before || !ReadPNG(Hash) || Hash!=PhotoHash)return End(TEXT("Normal ridge replay DELETE cancellation altered the original PNG or income"));return End(TEXT(""));}
  return false;
 }
private:
 bool StandingFloor(FVector Near,FVector& Stand)
 {
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RidgePhotoSupportedFloor),false,P);if(!W->LineTraceSingleByChannel(H,Near+FVector(0,0,500),Near-FVector(0,0,700),ECC_Pawn,Q) || H.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ())return false;
  Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);return !W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
 }
 FVector PhotoTarget() const{return Job->PhotoTarget?Job->PhotoTarget->GetActorLocation():Job->GetActorTransform().TransformPosition(Job->PhotoTargetLocal);}
 void Compose(){if(PC && PC->PlayerCameraManager)PC->SetControlRotation((PhotoTarget()-PC->PlayerCameraManager->GetCameraLocation()).Rotation());}
 bool BuildRoadReturn()
 {
  FString Text;TSharedPtr<FJsonObject> J;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid())return false;
  const auto& Values=J->GetArrayField(TEXT("points"));if(Values.Num()<800)return false;TArray<FVector> Road;for(const auto& Item:Values){const auto A=Item->AsArray();if(A.Num()!=3)return false;Road.Add(Job->GetActorTransform().TransformPosition(FVector(A[0]->AsNumber()*100,-A[1]->AsNumber()*100,A[2]->AsNumber()*100+5.5))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3));}
  FirstRoad=LastRoad=0;for(int I=1;I<Road.Num();++I){if(FVector::DistSquared2D(Road[I],P->GetActorLocation())<FVector::DistSquared2D(Road[FirstRoad],P->GetActorLocation()))FirstRoad=I;if(FVector::DistSquared2D(Road[I],Clerk->GetActorLocation())<FVector::DistSquared2D(Road[LastRoad],Clerk->GetActorLocation()))LastRoad=I;}
  if(FirstRoad<=LastRoad || FirstRoad-LastRoad<500 || !Phase205cSupportedWalkPath(W,P,Road[FirstRoad],Path))return false;
  LocalRoadConnectionNodes=Path.Num();for(int I=FirstRoad-1;I>=LastRoad;--I)Path.Add(Road[I]);Node=0;ReturnRoadStations=0;Test->AddInfo(FString::Printf(TEXT("RIDGE_PHOTO_NATIVE_RETURN first_station=%d last_station=%d actual_normal_walk=true cached_drive_route_not_retested=true"),FirstRoad,LastRoad));return true;
 }
 bool WalkPath()
 {
  while(Node<Path.Num() && FVector::Dist2D(P->GetActorLocation(),Path[Node])<25){if(Stage==12 && Node>=LocalRoadConnectionNodes)++ReturnRoadStations;++Node;LastProgress=W->GetTimeSeconds();}
  if(Node==Path.Num()){bMove=false;return true;}
  PlayerGoal=Path[Node];bMove=true;if(W->GetTimeSeconds()-LastProgress>16)
  {
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RidgePhotoActualWalkBlocker),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+(PlayerGoal-P->GetActorLocation()).GetSafeNormal2D()*180,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
   WalkFailure=FString::Printf(TEXT("Real ridge photo return blocked node%d player%s target%s collider%s component%s normal%s"),Node,*P->GetActorLocation().ToString(),*PlayerGoal.ToString(),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactNormal.ToString());
  }
  return false;
 }
 void DriveFrame(UWorld* World,ELevelTick,float Delta)
 {
  if(World!=W || !PC || !P)return;if(bCompose)Compose();if(!bMove || PC->GetPawn()!=P)return;const FVector Direction=PlayerGoal-P->GetActorLocation();PC->SetControlRotation(FRotator(0,Direction.Rotation().Yaw,0));
  // The same normal character movement port used by mapped held-W; real CMC
  // acceleration/floor/capsule remain responsible for every step.
  const float Scale=FMath::Min(1.f,float(Direction.Size2D())/FMath::Max(120.f,P->GetCharacterMovement()->GetMaxSpeed()*FMath::Max(0.f,Delta)*3));P->AddMovementInput(Direction.GetSafeNormal2D(),Scale);
 }
 bool ReadPNG(FString& Hash) const
 {
  TArray<uint8> Bytes;if(PhotoFile.IsEmpty() || !FFileHelper::LoadFileToArray(Bytes,*PhotoFile) || Bytes.Num()<20000)return false;const uint8 Magic[]={137,80,78,71,13,10,26,10};if(FMemory::Memcmp(Bytes.GetData(),Magic,8)!=0)return false;
  auto U32=[&](int I){return uint32(Bytes[I])<<24|uint32(Bytes[I+1])<<16|uint32(Bytes[I+2])<<8|uint32(Bytes[I+3]);};if(U32(16)!=1600 || U32(20)!=900)return false;uint8 Digest[20];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);Hash=BytesToHex(Digest,20);return true;
 }
 bool ReadBridge()
 {
  FString Text,ID;TSharedPtr<FJsonObject> J;double PID=0;if(!FFileHelper::LoadFileToString(Text,*Bridge) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J.IsValid() || !J->TryGetNumberField(TEXT("process_id"),PID) || uint32(PID)==FPlatformProcess::GetCurrentProcessId() || !J->TryGetNumberField(TEXT("balance"),ExpectedBalance) || !J->TryGetStringField(TEXT("run_id"),ID) || !FGuid::Parse(ID,Run) || !J->TryGetStringField(TEXT("photo_file"),PhotoFile) || !J->TryGetStringField(TEXT("photo_sha1"),PhotoHash))return false;return Run.IsValid() && PhotoHash.Len()==40;
 }
 bool WriteBridge()
 {
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("photo_file"),PhotoFile);J->SetStringField(TEXT("photo_sha1"),PhotoHash);J->SetNumberField(TEXT("balance"),double(Wallet->GetBalance()));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Bridge),true);return FFileHelper::SaveStringToFile(Text,*Bridge);
 }
 void Observe()
 {
  if(!W || !P || W->GetTimeSeconds()<LastObserve+.12f)return;LastObserve=W->GetTimeSeconds();const FVector Here=P->GetActorLocation();if(LastPositionValid && bMove)WalkDistance+=FVector::Dist2D(Here,LastPosition);LastPosition=Here;LastPositionValid=true;
  CSV+=FString::Printf(TEXT("%.4f,%d,%d,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f\n"),W->GetTimeSeconds(),Stage,Node,Here.X,Here.Y,Here.Z,int32(P->GetCharacterMovement()->MovementMode),ReturnRoadStations,PlayerGoal.X,PlayerGoal.Y,PlayerGoal.Z,WalkDistance);
 }
 bool Next(int32 S){bMove=false;Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Failure)
 {
  bMove=bCompose=false;if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Failure.IsEmpty())Test->AddError(Failure);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetBoolField(TEXT("independent_cold_process"),bCold);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("photo_file"),PhotoFile);J->SetStringField(TEXT("photo_sha1"),PhotoHash);J->SetStringField(TEXT("save_slot"),Slot);J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetNumberField(TEXT("actual_walk_cm"),WalkDistance);J->SetNumberField(TEXT("first_road_station"),FirstRoad);J->SetNumberField(TEXT("last_road_station"),LastRoad);J->SetNumberField(TEXT("returned_road_stations"),ReturnRoadStations);
  J->SetStringField(TEXT("scope"),bCold?TEXT("Independent-process genuine F7 restores original completed ridge PNG/hash/run/one220credit/receipt and exact safe saved player. Real P/O/RIGHT/ENTER replay and DELETE cancel retain original PNG and income. No pose/state/reward/photo fixtures; no original route repeat."):TEXT("One explicitly declared current floor/full-capsule supported ridge player start. Real P/O/RIGHT/ENTER acceptance, normal character movement to photo point, normal camera composition and native live-rendered1600x900 PNG/review. Continuous normal walking down the original surveyed mountain road and actual village foot connection, native clerkF, separate repeatedF guard, one shared220credit and genuine completedF6. No post-start pose corrections, car/NPC/target/photo/reward/state injection. Existing village photo/drive PASS not recertified. Final image taste remains user review."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(bCold?TEXT("RidgePhotoCold"):TEXT("RidgePhotoSource"));IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("trajectory.csv")));return true;
 }
 FAutomationTestBase* Test;bool bCold,bMove=false,bCompose=false,LastPositionValid=false;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Clerk=nullptr;UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;FNeonCityVisualEvidence Evidence;FDelegateHandle Driver;TMap<FKey,bool> Keys;TArray<FVector> Path;FGuid Run;int32 Stage=0,PhoneGoal=0,Browsed=0,Node=0,FirstRoad=0,LastRoad=0,LocalRoadConnectionNodes=0,ReturnRoadStations=0;float At=0,LastProgress=0,LastObserve=-1,WalkDistance=0;double Started=0,ExpectedBalance=0;int64 Before=0;FVector StartPosition=FVector::ZeroVector,PhotoPosition=FVector::ZeroVector,PlayerGoal=FVector::ZeroVector,LastPosition=FVector::ZeroVector;FString Slot,Bridge,PhotoFile,PhotoHash,WalkFailure,CSV=TEXT("game_seconds,stage,node,player_x,player_y,player_z,movement_mode,returned_road_stations,target_x,target_y,target_z,actual_walk_cm\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandRidgePhotoSource,"NeonCity.Phase205c.Activity.RidgePhoto.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandRidgePhotoSource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeRidgePhotography::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandRidgePhotoCold,"NeonCity.Phase205c.Activity.RidgePhoto.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandRidgePhotoCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeRidgePhotography::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
