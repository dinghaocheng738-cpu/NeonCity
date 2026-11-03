#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandActivityJob.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace NativeHike
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold,bool Clearance=false):Test(T),bCold(Cold),bClearance(Clearance),FirstStation(700){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>360)return End(TEXT("Native authored hike timeout"));
  if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==98 && Age>5){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hike-Changed-Bank-Diagnostic.png"));return End(DiagnosticFailure);}
  if(Stage==90)
  {
   if(!ViewSet){const FVector Mid=Path[4];const FVector Direction=(Path[5]-Path[3]).GetSafeNormal2D();const FVector Side(-Direction.Y,Direction.X,0);const FVector Eye=View==0?Path[0]-Direction*250+Side*180+FVector(0,0,165):View==1?Mid+Side*220+FVector(0,0,165):Mid-Side*850-Direction*500+FVector(0,0,650);Camera->SetActorLocationAndRotation(Eye,(Mid+FVector(0,0,90)-Eye).Rotation());At=W->GetTimeSeconds();ViewSet=true;return false;}
   if(Age<5)return false;const TCHAR* Names[]={TEXT("Hike-Changed-Bank-Front.png"),TEXT("Hike-Changed-Bank-Side.png"),TEXT("Hike-Changed-Bank-Overview.png")};FNeonCityVisualEvidence::CaptureHUD(W,Names[View]);ViewSet=false;if(++View==3)return End(SurveyFailures?TEXT("Changed road39lane support probes found rock above pavement; see retained CSV"):TEXT(""));return false;
  }
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-130000,27000))))return false;
   for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Hike.Summit"))Job=*It;
   for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job){if(It->Index==0)Begin=*It;else if(It->Index==1)Goal=*It;}
   Phone=PC->FindComponentByClass<UPhoneComponent>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=W->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
   if(!Job || !Begin || !Goal || !Phone || !Save->ConfigureSlot(TEXT("NeonCity_Test_Hike01"),false))return End(TEXT("Real authored hike or isolated save domain missing"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine completed hike checkpoint required"));Key(EKeys::F7,true);return Next(20);}
   if(!bClearance && Save->DoesSaveExist())return End(TEXT("Preserve earlier hike evidence; source slot must be fresh"));
   FString Text;TSharedPtr<FJsonObject> Data;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade/summit-road-alignment.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return End(TEXT("Authored road survey missing"));
   const auto& Points=Data->GetArrayField(TEXT("points"));
   auto Source=[&](int I){const auto V=Points[I]->AsArray();return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());};
   if(bClearance)
   {
    FCollisionQueryParams Probe(SCENE_QUERY_STAT(HikeChangedBankRoad),true,P);
    for(int I=700;I<=720;++I)for(double Offset:{-1.5,0.,1.5})
    {
     const FVector V=Source(I),T=(Source(I+1)-Source(I-1)).GetSafeNormal2D();const FVector Road=Job->GetActorTransform().TransformPosition(FVector((V.X-T.Y*Offset)*100,-(V.Y+T.X*Offset)*100,V.Z*100+.055*(1-FMath::Abs(Offset)/3.5)*100));FHitResult Hit;
     const bool Found=W->LineTraceSingleByChannel(Hit,Road+FVector(0,0,300),Road-FVector(0,0,800),ECC_Pawn,Probe);
     Survey+=FString::Printf(TEXT("%d,%.1f,%d,%s,%.3f,%.3f,%.3f,%.3f\n"),I,Offset,Found?1:0,*GetNameSafe(Hit.GetActor()),Road.Z,Hit.ImpactPoint.Z,Hit.ImpactPoint.Z-Road.Z,Hit.ImpactNormal.Z);
     if(!Found || FMath::Abs(Hit.ImpactPoint.Z-Road.Z)>12 || Hit.ImpactNormal.Z<.65)++SurveyFailures;
    }
   }
   // Walk2m inward from the authored view/checkpoint shoulder. A capsule needs
   // clearance from the existing road-edge furniture and guard. All actual
   // checkpoints remain in their retained3m shoulder positions and must pass.
   for(int I=FirstStation;I<=(bClearance?720:765);++I){const FVector V=Source(I),T=(Source(I+1)-Source(I-1)).GetSafeNormal2D();Path.Add(Job->GetActorTransform().TransformPosition(FVector((V.X-T.Y)*100,-(V.Y+T.X)*100,V.Z*100+.055*(1-1./3.5)*100+2)));}
   FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(HikeStartFloor),false,P);const FVector Setup=Path[0]-(Path[1]-Path[0]).GetSafeNormal2D()*100;
   if(!W->LineTraceSingleByChannel(Floor,Setup+FVector(0,0,200),Setup-FVector(0,0,500),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.65)
   {
    DiagnosticFailure=FString::Printf(TEXT("Actual hike start has no safe supported player access: setup%s hit%s impact%s normal%s"),*Setup.ToString(),*GetNameSafe(Floor.GetActor()),*Floor.ImpactPoint.ToString(),*Floor.ImpactNormal.ToString());
    if(!bClearance)return End(DiagnosticFailure);Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;const FVector Eye=Setup+FVector(500,-500,300);Camera->SetActorLocationAndRotation(Eye,(Setup-Eye).Rotation());PC->SetViewTarget(Camera);return Next(98);
   }
   P->SetActorLocation(Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Before=Wallet->GetBalance();
   if(bClearance){LastNodeAt=W->GetTimeSeconds();return Next(8);}Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>.8){Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityCompletions!=1 || D.ActivityBestSeconds<=0 || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Cold restore lost real hike score/unique shared payment"));OldRun=D.ActivityRunID;Before=Wallet->GetBalance();Key(EKeys::P,true);return Next(1);}
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P did not open phone"));Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Hike unavailable in real P/O phone"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==OldRun)return End(TEXT("Native acceptance/replay failed"));Run=D.ActivityRunID;
   if(bCold){Key(EKeys::Delete,true);return Next(21);}Key(EKeys::P,true);return Next(6);
  }
  if(Stage==21 && Age>.2){Key(EKeys::Delete,false);if(Job->BuildOpportunitySnapshot(false).State==1 || Wallet->GetBalance()!=Before)return End(TEXT("Cold replay/cancel duplicated payment"));return End(TEXT(""));}
  if(Stage==6 && Age>.2){Key(EKeys::P,false);if(!Focus(Begin))return End(TEXT("Native hike start F prompt blocked"));Key(EKeys::F,true);return Next(7);}
  if(Stage==7 && Age>.2){Key(EKeys::F,false);if(Job->GetActivityStage()!=1)return End(TEXT("F did not start timed hike"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hike-Native-Start.png"));LastNodeAt=W->GetTimeSeconds();return Next(8);}
  if(Stage==8)
  {
   while(Node<Path.Num() && FVector::Dist2D(P->GetActorLocation(),Path[Node])<45){++Node;LastNodeAt=W->GetTimeSeconds();}
   if(Node==Path.Num()){Key(EKeys::W,false);if(bClearance){Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;PC->SetViewTarget(Camera);return Next(90);}return Next(9);}
   if(W->GetTimeSeconds()-LastNodeAt>12)
   {
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(HikeActualBlocker),false,P);
    W->SweepSingleByChannel(Hit,P->GetActorLocation(),P->GetActorLocation()+(Path[Node]-P->GetActorLocation()).GetSafeNormal2D()*180,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
    return End(FString::Printf(TEXT("Normal walking blocked at actual road station%d; player%s target%s collider%s component%s normal%s"),FirstStation+Node,*P->GetActorLocation().ToString(),*Path[Node].ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactNormal.ToString()));
   }
   const FVector Direction=Path[Node]-P->GetActorLocation();PC->SetControlRotation(FRotator(0,Direction.Rotation().Yaw,0));Key(EKeys::W,true);
   const auto D=Job->BuildOpportunitySnapshot(false);CSV+=FString::Printf(TEXT("%.4f,%d,%.3f,%.3f,%.3f,%d,%.3f\n"),W->GetTimeSeconds(),FirstStation+Node,P->GetActorLocation().X,P->GetActorLocation().Y,P->GetActorLocation().Z,D.ActivityCheckpoint,D.ActivitySeconds);
   if(!bClearance && D.State!=1)return End(TEXT("Hike failed/cancelled during real traversal"));return false;
  }
  if(Stage==9 && Age>.5){const auto D=Job->BuildOpportunitySnapshot(false);if(D.ActivityCheckpoint!=Job->RoutePointsLocal.Num() || !P->GetCharacterMovement()->IsMovingOnGround() || !Focus(Goal))return End(TEXT("Actual ordered stops or safe on-foot finish missing"));Key(EKeys::F,true);return Next(10);}
  if(Stage==10 && Age>.2){Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=2 || D.ActivityCompletions!=1 || D.ActivityBestSeconds<=0 || Wallet->GetBalance()!=Before+Job->Reward || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Hike score/single payment settlement failed"));Key(EKeys::F,true);return Next(11);}
  if(Stage==11 && Age>.2){Key(EKeys::F,false);if(Wallet->GetBalance()!=Before+Job->Reward)return End(TEXT("Repeated finish duplicated payment"));Key(EKeys::F6,true);return Next(12);}
  if(Stage==12 && Age>.8){Key(EKeys::F6,false);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Model->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityRunID==Run && D.ActivityCompletions==1 && D.ActivityBestSeconds>0;}))return End(TEXT("F6 did not preserve actual completed hike score"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Hike-Native-Finish-Saved.png"));return End(TEXT(""));}
  return false;
 }
 bool Focus(AIslandActivityPoint* Point){PC->SetControlRotation((Point->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Point;}
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Error)
 {
  if(PC){for(auto& K:Keys)if(K.Value){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));K.Value=false;}if(Camera)PC->SetViewTarget(P);}if(Camera)Camera->Destroy();
  if(!Error.IsEmpty())Test->AddError(Error);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("last_station"),FirstStation+Node);J->SetStringField(TEXT("run_id"),Run.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);if(bClearance){J->SetNumberField(TEXT("changed_road_probes"),39);J->SetNumberField(TEXT("road_probe_failures"),SurveyFailures);}
  J->SetStringField(TEXT("scope"),bCold?TEXT("Independent-process genuine F7 score/payment restore, native P/O/RIGHT/ENTER replay, DELETE cancellation; zero pose/reward/job-state fixtures."):TEXT("One declared supported start player fixture. Native P/O/RIGHT/ENTER, F start, held mapped W normal walking along all actual surveyed shoulder stations700..765 inside retained guard, ordered checkpoints, on-foot F finish, repeat reward exclusion and F6. Zero post-setup player pose corrections or completion/reward injection. This scoped hike is not Dock continuity or subjective scenery acceptance."));
  if(bClearance){J->SetNumberField(TEXT("changed_road_probes"),63);J->SetStringField(TEXT("scope"),TEXT("Changed bounded cut/fill reproduction:63 native lane support probes700..720; one supported station700 fixture and actual held W walks21 road points700..720 at1m in-road offset, including previous703 and706 blockers; three changed-region real PIE daylight views. Physical Landscape and full visible rock derivative change together; road/guard/nav geometry retained. No job/reward/save actions or post-setup pose changes. This only proves changed walker clearance, not the whole hike."));}
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("NativeHike");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));if(!bCold)FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("trajectory.csv")));if(bClearance)FFileHelper::SaveStringToFile(Survey,*(Dir/TEXT("changed-bank-lane-survey.csv")));return true;
 }
 FAutomationTestBase* Test;bool bCold,bClearance;int FirstStation;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Begin=nullptr;AIslandActivityPoint* Goal=nullptr;UPhoneComponent* Phone=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;
 TArray<FVector> Path;TMap<FKey,bool> Keys;int Stage=0,Browsed=0,Node=0;float At=0,LastNodeAt=0;double Start=0;int64 Before=0;FGuid Run,OldRun;FString CSV=TEXT("game_seconds,station,x,y,z,checkpoint,activity_seconds\n");FNeonCityVisualEvidence Evidence{TEXT("NativeHike")};
 ACameraActor* Camera=nullptr;int SurveyFailures=0,View=0;bool ViewSet=false;FString DiagnosticFailure,Survey=TEXT("station,offset_m,hit,actor,expected_road_z,impact_z,difference_cm,normal_z\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHikeNativeTest,"NeonCity.Phase205c.Activity.Hike.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHikeNativeTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHike::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHikeColdTest,"NeonCity.Phase205c.Activity.Hike.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHikeColdTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHike::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHikeClearanceTest,"NeonCity.Phase205c.Activity.HikeShoulderClearancePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHikeClearanceTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHike::FFlow(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
