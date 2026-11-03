#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/DiveLocker.h"
#include "Adventure/DivingComponent.h"
#include "Adventure/DiveEquipmentComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Phone/PhoneComponent.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OxygenComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformProcess.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace NativeIslandSalvage
{
enum class EMove:uint8 {None,Ground,Swim,DPV};
/** A loads byte-identical retained real PlayerEntry gear ownership, then uses
 * ordinary inputs for the new salvage content. B/C require independent actual
 * F6 files and process IDs. No actor, gear, item, reward or run-state fixture. */
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,int32 S):Test(T),Session(S),Evidence(S==0?TEXT("SalvageSource"):S==1?TEXT("SalvageResume"):TEXT("SalvageCold")){}
 ~FFlow(){Unhook();}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>420)return End(TEXT("Salvage content exceeded its 420-second bounded route"));
  if(!Failure.IsEmpty())return End(Failure);if(!W || !PC || W->GetTimeSeconds()<4)return false;
  const float Now=W->GetTimeSeconds(),Age=Now-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(56450,-182650,-1000))))return false;
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());for(TActorIterator<AIslandActivityJob> It(W);It;++It)if(It->ActivityID==TEXT("Island.Activity.Salvage.Toolbox"))Job=*It;
   if(Job)for(TActorIterator<AIslandActivityPoint> It(W);It;++It)if(It->Job==Job){if(It->Index==0)Pickup=*It;else if(It->Index==1)Clerk=*It;}
   auto* GI=W->GetGameInstance();Save=GI->GetSubsystem<UNeonCitySaveSubsystem>();Wallet=GI->GetSubsystem<UNeonCityEconomySubsystem>();Phone=PC->FindComponentByClass<UPhoneComponent>();
   Kit=P?P->FindComponentByClass<UDivingComponent>():nullptr;Equipment=P?P->FindComponentByClass<UDiveEquipmentComponent>():nullptr;Water=P?P->FindComponentByClass<UWaterInteractionComponent>():nullptr;Air=P?P->FindComponentByClass<UOxygenComponent>():nullptr;Health=P?P->FindComponentByClass<UHealthComponent>():nullptr;
   FString Suffix;FParse::Value(FCommandLine::Get(),TEXT("NeonCitySalvageRun="),Suffix);if(Suffix.IsEmpty())Suffix=TEXT("01");
   if(Suffix.Len()>20)return End(TEXT("Salvage run suffix exceeds20 characters"));for(const TCHAR C:Suffix)if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))return End(TEXT("Salvage run suffix must use letters/digits/underscore/hyphen"));
   MidSlot=TEXT("NeonCity_Test_SalvageMid_")+Suffix;DoneSlot=TEXT("NeonCity_Test_SalvageDone_")+Suffix;Bridge=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade")/(TEXT("Salvage-")+Suffix);
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);Directory=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/(Session==0?TEXT("SalvageSource"):Session==1?TEXT("SalvageResume"):TEXT("SalvageCold"));IFileManager::Get().MakeDirectory(*Directory,true);
   CSV=TEXT("seconds,stage,move,route_node,x,y,z,velocity_x,velocity_y,velocity_z,depth,water_state,movement_mode,suit,changing,dpv,motor,oxygen,cargo,checkpoint,wallet,key_W,key_C,key_Space,key_LMB\n");
   if(!P || !Job || !Pickup || !Clerk || !Save || !Wallet || !Phone || !Kit || !Equipment || !Water || !Air || !Health || Job->Kind!=ENeonCityIslandActivity::DiveSalvage || Job->Reward!=240)return End(TEXT("Actual salvage/toolbox/clerk/gear/water/player/save domains missing"));
   Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FFlow::Frame);
   if(Session>0)
   {if(!Save->ConfigureSlot(Session==1?MidSlot:DoneSlot,false) || !Save->DoesSaveExist() || !ReadBridge(Session==1?TEXT("Source.json"):TEXT("Completed.json")))return End(TEXT("Independent genuine prior salvage F6/source manifest required"));Key(EKeys::F7,true);return Next(20);}
   if(!Save->ConfigureSlot(MidSlot,false) || Save->DoesSaveExist() || IFileManager::Get().FileExists(*(Bridge/TEXT("Source.json"))))return End(TEXT("Preserve prior actual salvage save/evidence; select a fresh explicit suffix after changes"));
   InputSlot=TEXT("NeonCity_Test_R05DPVPlayerEntry_20261003");InputFile=FPaths::ProjectSavedDir()/TEXT("SaveGames")/(InputSlot+TEXT(".sav"));
   if(!HashFile(InputFile,InputHash) || InputHash!=TEXT("8006D3E52C5342570AFAA089F95BA2470BD38287") || !Save->ConfigureSlot(InputSlot,false) || !Save->DoesSaveExist())return End(TEXT("Byte-identical retained real DPV PlayerEntry input required; no ownership/save fixture was invented"));
   Key(EKeys::F7,true);return Next(40);
  }
  Observe();if(Health->IsDead() || (InitialHealth>0 && Health->GetCurrentHealth()<InitialHealth-.5f))return End(TEXT("Normal salvage route damaged/downed the player; no health/air correction is applied"));
  if(Stage==40 && Age>1)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();FGuid Official;FGuid::Parse(TEXT("58223D90475ACD5AE94904B56FA77B77"),Official);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || M->SaveID!=Official || !Kit->OwnsSuit() || !Kit->OwnsDPV() || Kit->IsEquipped() || Equipment->IsDPVDeployed() ||
      FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>25 || !P->GetCharacterMovement()->IsMovingOnGround() || Water->GetWaterState()!=EWaterInteractionState::Dry)
    return End(TEXT("Genuine F7 did not retain the original owned/stowed gear and exact supported station position"));
   InputSaveID=M->SaveID;StationPosition=M->Player.Transform.GetLocation();Before=Wallet->GetBalance();InitialHealth=Health->GetCurrentHealth();Travel=0;LastPosition=P->GetActorLocation();LastPositionValid=true;
   double Best=FMath::Square(500.);for(TActorIterator<ADiveLocker> It(W);It;++It){const double D=FVector::DistSquared(It->GetActorLocation(),StationPosition);if(D<Best){Best=D;Locker=*It;}}
   if(!Locker || !Save->ConfigureSlot(MidSlot,false))return End(TEXT("Actual saved diving station or dedicated output slot unavailable"));
   Key(EKeys::P,true);return Next(1);
  }
  if(Stage==20 && Age>1)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();const auto D=Job->BuildOpportunitySnapshot(false);
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || M->SaveID!=SavedID || D.ActivityRunID!=Run || D.ActivityItemID!=Item || Wallet->GetBalance()!=ExpectedBalance ||
      FVector::Dist(P->GetActorLocation(),M->Player.Transform.GetLocation())>25 || !P->GetCharacterMovement()->IsMovingOnGround() || Water->GetWaterState()!=EWaterInteractionState::Dry)
    return End(TEXT("Independent F7 lost genuine salvage run/item/wallet/exact dry player or used a fallback"));
   Before=Wallet->GetBalance();InitialHealth=Health->GetCurrentHealth();Travel=0;LastPosition=P->GetActorLocation();LastPositionValid=true;
   if(Session==2){if(D.State!=2 || D.ActivityCompletions!=1 || Cargo() || !Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Settled cold salvage lost its one receipt/count or retained cargo"));Key(EKeys::P,true);return Next(1);}
   if(D.State!=1 || D.ActivityStage!=1 || D.ActivityCheckpoint!=1 || !Item.IsValid() || !Cargo() || Cargo()->GetAttachParent()!=P->GetRootComponent() || Wallet->HasCredit(Job->GetReceiptID()))return End(TEXT("Independent unfinished salvage lost the actual carried item identity/attachment/unpaid checkpoint"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Salvage-Cold-Actual-Unpaid-Toolbox.png"));FVector Stand;if(!ClerkApproach(Stand) || !GroundRoute({Stand}))return End(TEXT("Actual returning clerk has no supported full-capsule normal approach"));return Next(30);
  }
  if(Stage==1 && Age>.2){Key(EKeys::P,false);if(!Phone->IsPhoneOpen())return End(TEXT("Native P phone input failed"));if(Phone->GetActiveApp()==ENeonCityPhoneApp::Opportunities){Browsed=0;return Next(3);}Key(EKeys::O,true);return Next(2);}
  if(Stage==2 && Age>.2){Key(EKeys::O,false);Browsed=0;return Next(3);}
  if(Stage==3 && Age>.15){if(Phone->GetDisplayedText().ToString().Contains(Job->Title.ToString())){Key(EKeys::Enter,true);return Next(5);}if(++Browsed>30)return End(TEXT("Actual salvage offer not discoverable through P/O"));Key(EKeys::Right,true);return Next(4);}
  if(Stage==4 && Age>.15){Key(EKeys::Right,false);return Next(3);}
  if(Stage==5 && Age>.2)
  {
   Key(EKeys::Enter,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || !D.ActivityRunID.IsValid() || D.ActivityRunID==Run || D.ActivityStage!=0 || D.ActivityItemID.IsValid() || Cargo())return End(TEXT("Actual ENTER did not create a fresh unpaid salvage run"));
   if(Session==2){if(D.ActivityCompletions!=1 || Wallet->GetBalance()!=Before)return End(TEXT("Replay acceptance duplicated settled salvage income"));Key(EKeys::Delete,true);return Next(38);}
   Run=D.ActivityRunID;Key(EKeys::P,true);return Next(6);
  }
  if(Stage==6 && Age>.2){Key(EKeys::P,false);if(!Focus(Locker,Locker->InteractionTarget->GetComponentLocation()))return End(TEXT("Actual saved diving-station F prompt unavailable"));Key(EKeys::F,true);return Next(7);}
  if(Stage==7)
  {
   if(Age>.2)Key(EKeys::F,false);GearTransitionSeen|=Kit->IsChangingGear();if(!Kit->IsEquipped() || Kit->IsChangingGear())return Age>6?End(TEXT("Normal station F did not complete the finite suit equipment transition")):false;
   if(!GearTransitionSeen || Equipment->IsDPVDeployed() || !Kit->OwnsSuit() || !Kit->OwnsDPV())return End(TEXT("Actual gear transition/ownership/stowed hands invalid"));
   if(!GroundRoute({NeonCityCoastalLayout::FromLegacyIsland(FVector(56110,-173510,StationPosition.Z))}))return End(TEXT("Current panel/cabinet geometry blocks the real east pier exit"));return Next(8);
  }
  if(Stage==8){if(!Follow(EMove::Ground,45))return false;Route={NeonCityCoastalLayout::FromLegacyIsland(FVector(56250,-173510,-250)),NeonCityCoastalLayout::FromLegacyIsland(FVector(56250,-173000,-350)),NeonCityCoastalLayout::FromLegacyIsland(FVector(57700,-173000,-500))};RouteNode=0;return Next(9);}
  if(Stage==9)
  {
   if(Water->IsFullySubmerged() && Water->GetSample().DepthAt(P->GetActorLocation())>280){Stop();Key(EKeys::Q,true);return Next(10);}
   Follow(EMove::Swim,65);return false;
  }
  if(Stage==10 && Age>.2)
  {
   Key(EKeys::Q,false);if(!Equipment->IsDPVDeployed())return Age>2?End(TEXT("Normal underwater Q failed to deploy owned DPV")):false;
   Route={NeonCityCoastalLayout::FromLegacyIsland(FVector(57700,-181900,-500)),Pickup->GetActorLocation()+FVector(0,550,400)};RouteNode=0;return Next(11);
  }
  if(Stage==11){if(!Follow(EMove::DPV,180))return false;Key(EKeys::Q,true);return Next(12);}
  if(Stage==12 && Age>.2){Key(EKeys::Q,false);if(Equipment->IsDPVDeployed())return Age>2?End(TEXT("Normal Q failed to stow DPV beside the real seabed objective")):false;Route={Pickup->GetActorLocation()+FVector(0,140,125)};RouteNode=0;return Next(13);}
  if(Stage==13)
  {
   if(!Follow(EMove::Swim,55))return false;
   if(!Water->IsFullySubmerged() || Water->GetSample().DepthAt(P->GetActorLocation())<500 || !Kit->IsEquipped())return End(TEXT("Toolbox approach did not actually reach equipped deep underwater gameplay"));
   if(P->GetVelocity().Size()>75)return false;if(!Focus(Pickup,Pickup->Interaction->GetComponentLocation()))return End(TEXT("Normal underwater F eye trace cannot focus the physical toolbox"));
   RecoveryDepth=Water->GetSample().DepthAt(P->GetActorLocation());FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Salvage-Native-Seabed-F-Prompt.png"));Key(EKeys::F,true);return Next(14);
  }
  if(Stage==14 && Age>.2)
  {
   Key(EKeys::F,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State!=1 || D.ActivityStage!=1 || !D.ActivityItemID.IsValid() || D.ActivityRunID!=Run || !Cargo() || Cargo()->GetAttachParent()!=P->GetRootComponent() || Wallet->GetBalance()!=Before)return End(TEXT("Underwater F did not produce the actual unpaid recovered item identity/attachment"));
   Item=D.ActivityItemID;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Salvage-Native-Carried-Toolbox.png"));Key(EKeys::Q,true);return Next(15);
  }
  if(Stage==15 && Age>.2){Key(EKeys::Q,false);if(!Equipment->IsDPVDeployed())return Age>2?End(TEXT("Normal Q could not prepare the loaded diver's real return")):false;Route={NeonCityCoastalLayout::FromLegacyIsland(FVector(57700,-173000,-450))};RouteNode=0;return Next(16);}
  if(Stage==16){if(!CarriedValid())return End(TEXT("Actual DPV return lost recovered item/run or paid prematurely"));if(!Follow(EMove::DPV,180))return false;Key(EKeys::Q,true);return Next(17);}
  if(Stage==17 && Age>.2)
  {Key(EKeys::Q,false);if(Equipment->IsDPVDeployed())return Age>2?End(TEXT("Normal Q failed to stow motor for the retained physical beach return")):false;Route={NeonCityCoastalLayout::FromLegacyIsland(FVector(56000,-163000,180)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-163000,180))};RouteNode=0;return Next(18);}
  if(Stage==18)
  {
   if(!CarriedValid())return End(TEXT("Real surface swim/wading return lost the recovered toolbox"));if(!Follow(EMove::Swim,65))return false;
   if(!P->GetCharacterMovement()->IsMovingOnGround() || Water->GetWaterState()!=EWaterInteractionState::Dry)return false;
   // The accepted native PlayerEntry beach return crosses a shallow tidal
   // channel before the original pier. It is not an entirely dry A* corridor.
   // Keep its real W/Space swimming/wading chain and let native water/floor
   // modes change normally; do not demand a dry floor beneath the channel.
   Route={NeonCityCoastalLayout::FromLegacyIsland(FVector(55500,-173550,180)),StationPosition};RouteNode=0;return Next(23);
  }
  if(Stage==23)
  {
   if(!CarriedValid())return End(TEXT("Actual tidal-channel return lost the recovered toolbox"));
   if(!Follow(EMove::Swim,45))return false;
   if(!P->GetCharacterMovement()->IsMovingOnGround() || Water->GetWaterState()!=EWaterInteractionState::Dry)
    return End(TEXT("Actual retained channel/pier route did not finish grounded and dry at the original station"));
   Route={StationPosition};RouteNode=0;return Next(19);
  }
  if(Stage==19)
  {
   if(!CarriedValid())return End(TEXT("Actual ground return lost recovered item/checkpoint"));if(!Follow(EMove::Ground,45))return false;
   if(P->GetVelocity().Size2D()>60)return false;if(!Focus(Locker,Locker->InteractionTarget->GetComponentLocation()))return End(TEXT("Normal returned-player F trace cannot focus original equipment station"));Key(EKeys::F,true);return Next(21);
  }
  if(Stage==21)
  {
   if(Age>.2)Key(EKeys::F,false);if(Kit->IsEquipped() || Kit->IsChangingGear())return Age>6?End(TEXT("Real finite station gear-removal transition did not complete")):false;
   if(!CarriedValid() || Equipment->IsDPVDeployed() || !Kit->OwnsSuit() || !Kit->OwnsDPV() || !P->GetCharacterMovement()->IsMovingOnGround() || Water->GetWaterState()!=EWaterInteractionState::Dry || Job->BuildOpportunitySnapshot(false).ActivityCheckpoint!=1)return End(TEXT("Returned unpaid salvage lacks its genuine transported checkpoint/dry player/stowed owned gear"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Salvage-Native-Dry-Unpaid-Return.png"));Key(EKeys::F6,true);return Next(22);
  }
  if(Stage==22 && Age>.8)
  {
   Key(EKeys::F6,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();const auto* D=M?M->Phone.Opportunities.FindByPredicate([&](const auto& V){return V.OpportunityID==Job->ActivityID;}):nullptr;
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !D || D->State!=1 || D->ActivityStage!=1 || D->ActivityCheckpoint!=1 || D->ActivityItemID!=Item || D->ActivityRunID!=Run || D->ActivityCompletions!=0 || M->Player.R05Equipped || !CarriedValid())return End(TEXT("Genuine returned-ground F6 did not retain exact recovered item/run/unpaid checkpoint"));
   SavedID=M->SaveID;if(!WriteBridge(TEXT("Source.json")))return End(TEXT("Could not preserve genuine unpaid salvage source manifest"));return End(TEXT(""));
  }
  if(Stage==30)
  {
   if(!CarriedValid())return End(TEXT("Restored real item/run lost during normal clerk approach"));if(!Follow(EMove::Ground,45))return false;
   if(P->GetVelocity().Size2D()>60)return false;if(!Focus(Clerk,Clerk->Interaction->GetComponentLocation()))return End(TEXT("Normal clerk F prompt unavailable from physically supported return"));Key(EKeys::F,true);return Next(31);
  }
  if(Stage==31 && Age>.2)
  {
   Key(EKeys::F,false);if(!SettledValid())return End(TEXT("Actual clerk F failed single240-credit completion/item cleanup"));Key(EKeys::F,true);return Next(32);
  }
  if(Stage==32 && Age>.2)
  {
   Key(EKeys::F,false);if(!SettledValid())return End(TEXT("Repeated actual F duplicated salvage settlement"));if(!Save->ConfigureSlot(DoneSlot,false) || Save->DoesSaveExist())return End(TEXT("Preserve prior genuine completed salvage slot"));Key(EKeys::F6,true);return Next(33);
  }
  if(Stage==33 && Age>.8)
  {
   Key(EKeys::F6,false);if(Save->IsOperationInProgress())return false;const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !SettledValid() || !M->Phone.Opportunities.ContainsByPredicate([&](const auto& D){return D.OpportunityID==Job->ActivityID && D.State==2 && D.ActivityCompletions==1 && D.ActivityRunID==Run && D.ActivityItemID==Item;}))return End(TEXT("Genuine completed salvage F6 failed to retain original settled item/run"));
   SavedID=M->SaveID;if(!WriteBridge(TEXT("Completed.json")))return End(TEXT("Could not preserve actual settled salvage manifest"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Salvage-Native-One-Credit-Completed.png"));return End(TEXT(""));
  }
  if(Stage==38 && Age>.2){Key(EKeys::Delete,false);const auto D=Job->BuildOpportunitySnapshot(false);if(D.State==1 || D.ActivityCompletions!=1 || Cargo() || Wallet->GetBalance()!=Before)return End(TEXT("Native settled replay/DELETE cancellation left cargo or paid again"));return End(TEXT(""));}
  return false;
 }
private:
 UStaticMeshComponent* Cargo()const
 {if(!Job)return nullptr;TArray<UStaticMeshComponent*> Parts;Job->GetComponents(Parts);for(auto* C:Parts)if(IsValid(C) && C->IsRegistered() && C->GetFName()==TEXT("CarriedIslandParcel"))return C;return nullptr;}
 bool CarriedValid()const
 {const auto D=Job->BuildOpportunitySnapshot(false);return D.State==1 && D.ActivityStage==1 && D.ActivityRunID==Run && D.ActivityItemID==Item && Cargo() && Cargo()->GetAttachParent()==P->GetRootComponent() && Wallet->GetBalance()==Before && !Wallet->HasCredit(Job->GetReceiptID());}
 bool SettledValid()const
 {const auto D=Job->BuildOpportunitySnapshot(false);return D.State==2 && D.ActivityCompletions==1 && D.ActivityRunID==Run && D.ActivityItemID==Item && !Cargo() && Wallet->GetBalance()==Before+240 && Wallet->HasCredit(Job->GetReceiptID());}
 bool Focus(AActor* Actor,FVector Aim){PC->SetControlRotation((Aim-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Actor;}
 bool GroundRoute(const TArray<FVector>& Goals)
 {Stop();GroundGoals=Goals;GroundGoalNode=0;RouteNode=0;return Goals.Num()>0 && Phase205cSupportedWalkPath(W,P,Goals[0],Route);}
 bool Follow(EMove Kind,float Radius)
 {
  if(!Route.IsValidIndex(RouteNode)){Stop();return true;}
  const FVector Delta=Route[RouteNode]-P->GetActorLocation();
  const bool DryGround=Water->GetWaterState()==EWaterInteractionState::Dry && P->GetCharacterMovement()->IsMovingOnGround();
  const bool Arrived=Delta.Size2D()<Radius && (Kind==EMove::Ground || (Kind==EMove::Swim && DryGround) || FMath::Abs(Delta.Z)<Radius);
  if(Arrived)
  {
   Stop();if(++RouteNode>=Route.Num())
   {
    if(Kind==EMove::Ground && ++GroundGoalNode<GroundGoals.Num()){RouteNode=0;if(!Phase205cSupportedWalkPath(W,P,GroundGoals[GroundGoalNode],Route)){Failure=TEXT("Actual segmented ground return floor/capsule path blocked");return false;}}
    else return true;
   }
  }
  if(!Route.IsValidIndex(RouteNode))return false;Move=Kind;MoveGoal=Route[RouteNode];
  if(!ProgressValid || FVector::Dist(P->GetActorLocation(),ProgressMarker)>20){ProgressValid=true;ProgressMarker=P->GetActorLocation();LastProgress=W->GetTimeSeconds();}
  if(W->GetTimeSeconds()-LastProgress>18)
  {FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(SalvageNormalRouteBlocker),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+(MoveGoal-P->GetActorLocation()).GetSafeNormal()*180,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);Failure=FString::Printf(TEXT("Actual salvage route blocked stage%d node%d player%s target%s collider%s/%s"),Stage,RouteNode,*P->GetActorLocation().ToString(),*MoveGoal.ToString(),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()));}
  return false;
 }
 bool ClerkApproach(FVector& Stand)const
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(SalvageRealClerkApproach),false,P);const auto Shape=FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());
  for(const FVector Offset:{FVector(-140,0,0),FVector(140,0,0),FVector(0,-140,0),FVector(0,140,0)})
  {const FVector AtPoint=Clerk->GetActorLocation()+Offset;FHitResult H,Eye;if(!W->LineTraceSingleByChannel(H,AtPoint+FVector(0,0,350),AtPoint-FVector(0,0,250),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)continue;const FVector Candidate=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);if(W->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,Shape,Q))continue;W->LineTraceSingleByChannel(Eye,Candidate+FVector(0,0,P->BaseEyeHeight),Clerk->Interaction->GetComponentLocation(),ECC_Visibility,Q);if(Eye.GetActor()!=Clerk || FVector::Dist(Candidate,Clerk->GetActorLocation()+FVector(0,0,90))>=240)continue;Stand=Candidate;return true;}
  return false;
 }
 void Frame(UWorld* World,ELevelTick,float DeltaSeconds)
 {
  if(World!=W || !PC || !P || PC->GetPawn()!=P || Move==EMove::None)return;
  const FVector Delta=MoveGoal-P->GetActorLocation();const FRotator View=Delta.Rotation();const float Horizontal=Delta.Size2D();
  PC->SetControlRotation(FRotator(Move==EMove::DPV?FMath::Clamp(View.Pitch,-65.,65.):0,View.Yaw,0));
  if(Move==EMove::DPV)
  {
   Key(EKeys::W,false);Key(EKeys::C,false);Key(EKeys::SpaceBar,false);if(!Equipment->IsDPVDeployed()){Failure=TEXT("Actual powered salvage leg unexpectedly lost its deployed DPV");return;}
   // Follow accepts180cm. Keep a positive dead-man demand until that native
   // waypoint is reached; a zero lower bound stopped at181.8cm indefinitely.
   const float Desired=FMath::Clamp(float(Delta.Size()-160)*.65f,35.f,350.f),Along=FVector::DotProduct(P->GetVelocity(),Delta.GetSafeNormal());Key(EKeys::LeftMouseButton,Along<Desired-15);return;
  }
  Key(EKeys::LeftMouseButton,false);const float Scale=FMath::Min(1.f,Horizontal/FMath::Max(120.f,P->GetCharacterMovement()->GetMaxSpeed()*FMath::Max(0.f,DeltaSeconds)*3));
  // Ordinary held-W movement port, fed every actual game frame. C/Space/Q and
  // the dead-man paddles remain native keys; CMC owns all motion/collision.
  Key(EKeys::W,true);P->AddMovementInput(Delta.GetSafeNormal2D(),Scale);
  const bool Wet=Water->GetWaterState()!=EWaterInteractionState::Dry;
  Key(EKeys::C,Move==EMove::Swim && Wet && Delta.Z<-50);Key(EKeys::SpaceBar,Move==EMove::Swim && Wet && Delta.Z>50);
 }
 void Stop(){Move=EMove::None;Key(EKeys::W,false);Key(EKeys::C,false);Key(EKeys::SpaceBar,false);Key(EKeys::LeftMouseButton,false);}
 bool Next(int32 S){Stop();Stage=S;At=W->GetTimeSeconds();ProgressValid=false;LastProgress=At;return false;}
 void Key(FKey K,bool Down){if(!PC)return;bool& Was=Keys.FindOrAdd(K);if(Was!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Was=Down;}}
 void Unhook(){if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}}
 static bool HashFile(const FString& File,FString& Hash)
 {TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File) || Bytes.IsEmpty())return false;uint8 Digest[20];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);Hash=BytesToHex(Digest,20);return true;}
 bool ReadBridge(const TCHAR* Name)
 {
  FString Text,R,I,S;TSharedPtr<FJsonObject> J;double PID=0;if(!FFileHelper::LoadFileToString(Text,*(Bridge/Name)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J) || !J || !J->TryGetNumberField(TEXT("process_id"),PID) || uint32(PID)==FPlatformProcess::GetCurrentProcessId() || !J->TryGetNumberField(TEXT("balance"),ExpectedBalance) || !J->TryGetStringField(TEXT("run_id"),R) || !J->TryGetStringField(TEXT("item_id"),I) || !J->TryGetStringField(TEXT("save_id"),S))return false;
  FString Original;double Depth=0;
  if(!J->TryGetStringField(TEXT("input_save_id"),Original) || !FGuid::Parse(Original,InputSaveID) || !J->TryGetStringField(TEXT("input_sha1"),InputHash) || InputHash!=TEXT("8006D3E52C5342570AFAA089F95BA2470BD38287") || !J->TryGetNumberField(TEXT("recovery_depth_cm"),Depth) || Depth<500)return false;RecoveryDepth=Depth;
  return FGuid::Parse(R,Run) && FGuid::Parse(I,Item) && FGuid::Parse(S,SavedID) && Run.IsValid() && Item.IsValid();
 }
 bool WriteBridge(const TCHAR* Name)
 {
  auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("save_id"),SavedID.ToString());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("item_id"),Item.ToString());J->SetNumberField(TEXT("balance"),double(Wallet->GetBalance()));J->SetStringField(TEXT("input_save_id"),InputSaveID.ToString());J->SetStringField(TEXT("input_sha1"),InputHash);J->SetNumberField(TEXT("recovery_depth_cm"),RecoveryDepth);
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Bridge,true);return FFileHelper::SaveStringToFile(Text,*(Bridge/Name));
 }
 void Observe()
 {
  if(!P || W->GetTimeSeconds()-LastRecord<.15f)return;LastRecord=W->GetTimeSeconds();const FVector Here=P->GetActorLocation(),V=P->GetVelocity();if(LastPositionValid)Travel+=FVector::Dist(Here,LastPosition);LastPosition=Here;LastPositionValid=true;
  const auto D=Job->BuildOpportunitySnapshot(false);CSV+=FString::Printf(TEXT("%.3f,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%.3f,%.3f,%d,%d,%lld,%d,%d,%d,%d\n"),LastRecord,Stage,int32(Move),RouteNode,Here.X,Here.Y,Here.Z,V.X,V.Y,V.Z,Water->GetSample().DepthAt(Here),int32(Water->GetWaterState()),int32(P->GetCharacterMovement()->MovementMode),Kit->IsEquipped(),Kit->IsChangingGear(),Equipment->IsDPVDeployed(),Equipment->GetMotorSpool(),Air->CurrentOxygen,Cargo()?1:0,D.ActivityCheckpoint,Wallet->GetBalance(),Keys.FindRef(EKeys::W),Keys.FindRef(EKeys::C),Keys.FindRef(EKeys::SpaceBar),Keys.FindRef(EKeys::LeftMouseButton));
 }
 bool End(FString Error)
 {
  Stop();Unhook();if(PC)for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));
  bool InputUnchanged=true;if(Session==0 && !InputHash.IsEmpty()){FString Current;InputUnchanged=HashFile(InputFile,Current) && Current==InputHash;if(!InputUnchanged)Error=TEXT("Protected original DPV input file changed");}
  if(!Error.IsEmpty())Test->AddError(Error);if(Directory.IsEmpty())return true;
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("session"),Session);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetStringField(TEXT("run_id"),Run.ToString());J->SetStringField(TEXT("item_id"),Item.ToString());J->SetStringField(TEXT("save_id"),SavedID.ToString());J->SetNumberField(TEXT("wallet_before"),double(Before));J->SetNumberField(TEXT("wallet_after"),Wallet?double(Wallet->GetBalance()):0);J->SetNumberField(TEXT("recovery_depth_cm"),RecoveryDepth);J->SetNumberField(TEXT("actual_travel_cm"),Travel);J->SetBoolField(TEXT("input_bytes_unchanged"),InputUnchanged);J->SetNumberField(TEXT("harness_pose_writes"),0);J->SetNumberField(TEXT("item_gear_reward_state_fixtures"),0);
  J->SetStringField(TEXT("scope"),Session==0?TEXT("Genuine F7 of retained byte-identical R05DPVPlayerEntry source, exact normal station/owned-stowed gear; P/O acceptance, finite native F equipment transition, current physical pier exit and W/C/Q/LMB DPV travel to authored real seabed toolbox, underwater F generates real item GUID, actual DPV/surface swim/proven beach and segmented ground return, native finite station gear removal, unpaid ground F6. No fixture poses/ownership/state/item/reward injection; cached generic DPV features not rerun."):Session==1?TEXT("Independent source PID/genuine F7 restores exact original carried toolbox GUID/run/unpaid checkpoint/dry player without fallback. Native ground W and clerk F produce one240credit and real cargo cleanup; repeated F cannot duplicate payment; genuine completed F6."):TEXT("Independent genuine completed F7 preserves original item/run/one credit/count/no cargo. P/O/ENTER new unpaid replay then native DELETE cancellation without another payment."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("receipt.json")));FFileHelper::SaveStringToFile(CSV,*(Directory/TEXT("trajectory.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);return true;
 }
 FAutomationTestBase* Test;int32 Session=0,Stage=0,Browsed=0,RouteNode=0,GroundGoalNode=0;double Started=0,ExpectedBalance=0;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandActivityJob* Job=nullptr;AIslandActivityPoint* Pickup=nullptr;AIslandActivityPoint* Clerk=nullptr;ADiveLocker* Locker=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityEconomySubsystem* Wallet=nullptr;UPhoneComponent* Phone=nullptr;UDivingComponent* Kit=nullptr;UDiveEquipmentComponent* Equipment=nullptr;UWaterInteractionComponent* Water=nullptr;UOxygenComponent* Air=nullptr;UHealthComponent* Health=nullptr;
 TArray<FVector> Route,GroundGoals;TMap<FKey,bool> Keys;FDelegateHandle Driver;FNeonCityVisualEvidence Evidence;EMove Move=EMove::None;FVector MoveGoal=FVector::ZeroVector,StationPosition=FVector::ZeroVector,ProgressMarker=FVector::ZeroVector,LastPosition=FVector::ZeroVector;FString MidSlot,DoneSlot,InputSlot,InputFile,InputHash,Bridge,Directory,CSV,Failure;FGuid Run,Item,SavedID,InputSaveID;int64 Before=0;float At=0,LastProgress=0,LastRecord=0,InitialHealth=0,RecoveryDepth=0,Travel=0;bool ProgressValid=false,LastPositionValid=false,GearTransitionSeen=false;
};
}
#define NEONCITY_SALVAGE_TEST(Class,Name,Session) \
 IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
 bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeIslandSalvage::FFlow(this,Session));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
NEONCITY_SALVAGE_TEST(FIslandSalvageA,"NeonCity.Phase205c.Activity.Salvage.A",0)
NEONCITY_SALVAGE_TEST(FIslandSalvageB,"NeonCity.Phase205c.Activity.Salvage.B",1)
NEONCITY_SALVAGE_TEST(FIslandSalvageC,"NeonCity.Phase205c.Activity.Salvage.C",2)
#undef NEONCITY_SALVAGE_TEST
#endif
