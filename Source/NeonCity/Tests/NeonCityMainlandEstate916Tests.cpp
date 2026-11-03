#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Tests/NeonCityLandFrameMetrics.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandEstateAmenity.h"
#include "Adventure/IslandLandGrill.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#if WITH_EDITOR
#include "Tests/NeonCityVisualEvidence.h"
#endif

namespace MainlandEstate916
{
const FVector Site(-92000,36000,973);
void Key(APlayerController* PC,FKey K,bool Down)
{if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
bool Focus(APlayerController* PC,ANeonCityPlayerCharacter* Player,AActor* Target)
{
 FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);auto* Interaction=Player->GetInteractionComponent();FVector Origin,Extent;Target->GetActorBounds(true,Origin,Extent);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(MainlandEstateNormalFocus),false,Player);
 for(float X:{-.8f,0.f,.8f})for(float Y:{-.8f,0.f,.8f})for(float Z:{-.5f,0.f,.8f})
 {
  const FVector D=(Origin+Extent*FVector(X,Y,Z)-Eye).GetSafeNormal();FHitResult H;
  if(Player->GetWorld()->LineTraceSingleByChannel(H,Eye,Eye+D*Interaction->InteractionDistance,Interaction->TraceChannel,Q) && H.GetActor()==Target)
  {PC->SetControlRotation(D.Rotation());return Interaction->RefreshFocusedInteractable()==Target;}
 }
 return false;
}
// The only pose fixture is the disclosed initial forecourt standing point.
// All subsequent travel uses ordinary CharacterMovement and intact collision.
enum class EAccessScope { Full, Stairs, Gallery, Remaining, DescentCinema, Cinema };
class FAccess : public IAutomationLatentCommand
{
public:explicit FAccess(FAutomationTestBase* T,EAccessScope InScope=EAccessScope::Full):Test(T),Scope(InScope){}
 bool Update()override
 {
  if(Ending)
  {
   // A blocked safe exit is a real failure. Detachment lets the seat's normal
   // next tick restore its complete saved character state before relocation.
   if(Cinema && Cinema->IsOccupied())return false;
   if(P){P->GetCharacterMovement()->StopMovementImmediately();P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);}
   return true;
  }
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>330)return End(TEXT("Estate tour timeout"));
  const float Age=W->GetTimeSeconds()-At;
  if(!P)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal player absent"));Original=P->GetActorTransform();
   for(TActorIterator<AIslandLandGrill> I(W);I;++I)if(I->StationID==TEXT("Mainland916.Estate.BBQ"))Grill=*I;
   for(TActorIterator<AIslandLeisureFacility> I(W);I;++I)if(I->PropertyID==TEXT("Property.Mainland.RivieraEstate916") && I->Kind==EIslandLeisureKind::Cinema)Cinema=*I;
   for(TActorIterator<AIslandEstateAmenity> I(W);I;++I)if(I->AmenityID==TEXT("Mainland916.Estate.Sink"))Sink=*I;
   if(!Grill || !Grill->Visual->GetStaticMesh() || !Grill->Coals->GetStaticMesh() || !Cinema || !Cinema->Screen->GetStaticMesh() || !Sink || !Sink->HasPresentation())return End(TEXT("Estate grill/screen/faucet presentation missing"));
   const FVector Stops[]={FVector(0,21,0),FVector(0,17,0),FVector(0,3,0),FVector(0,-14,0),FVector(15.5,-14,0),FVector(15.5,-26,0),FVector(26,-27,0),FVector(26,-33.95,0),FVector(15.5,-26,0),FVector(15.5,-14,0),FVector(0,-14,0),FVector(0,3,0),FVector(-10,9,0),FVector(-10,17,4.2),FVector(-12,21.4,4.2),FVector(-18,21.4,4.2),FVector(-18,18,4.2),FVector(-23.5,14,4.2),FVector(-18,9,4.2),FVector(-23.5,4,4.2),FVector(-18,-1,4.2),FVector(-23.5,4,4.2),FVector(-23.5,14,4.2),FVector(-18,21.4,4.2),FVector(-12,20.2,4.2),FVector(-10,18.5,4.2),FVector(0,18.5,4.2),FVector(18,21.4,4.2),FVector(18,18,4.2),FVector(23.5,14,4.2),FVector(18,9,4.2),FVector(23.5,4,4.2),FVector(18,-1,4.2),FVector(23.5,4,4.2),FVector(23.5,14,4.2),FVector(18,21.4,4.2),FVector(0,18.5,4.2),FVector(-10,18.5,4.2),FVector(-10,9,0),FVector(0,9,0),FVector(0,3,0),FVector(0,-14,0),FVector(-23.5,-14,0),FVector(-23.5,-3,0),FVector(-23.5,3,0),FVector(-23.5,5.7,0),FVector(-25,5.7,0)};
   for(auto S:Stops)Goals.Add(Site+S*100+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3));
   if(Scope==EAccessScope::Stairs){Goals.SetNum(16);Goals.RemoveAt(0,12);}
   if(Scope==EAccessScope::Remaining)Goals.RemoveAt(0,24);
   if(Scope==EAccessScope::Gallery){Goals.Reset();for(FVector S:{FVector(-12,20.2,4.2),FVector(-10,18.5,4.2),FVector(0,18.5,4.2),FVector(18,21.4,4.2)})Goals.Add(Site+S*100+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3));}
   if(Scope==EAccessScope::DescentCinema)Goals.RemoveAt(0,38);
   if(Scope==EAccessScope::Cinema)Goals.RemoveAt(0,39);
   P->SetActorLocation(Site+((Scope==EAccessScope::Gallery || Scope==EAccessScope::Remaining)?FVector(-1800,2140,515):Scope==EAccessScope::Cinema?FVector(-1000,900,95):Scope==EAccessScope::DescentCinema?FVector(-1000,1850,515):Scope==EAccessScope::Stairs?FVector(0,284,95):FVector(0,2800,100)),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);At=W->GetTimeSeconds();return false;
  }
  Metrics.Sample(W,Stage==0?TEXT("EstateWarmup"):Stage==1?TEXT("EstateContinuousWalking"):TEXT("EstateCinema"));
  if(Stage==0 && Age>2){Stage=1;At=W->GetTimeSeconds();return false;}
  if(Stage==1)
  {
   if(Path.IsEmpty())
   {
    FPhase205cWalkSurvey Survey;
    // Traverse the designed stair centreline with actual StepUp physics.
    const bool StairLeg=Scope!=EAccessScope::Gallery && FMath::Abs(Goals[Stop].Z-P->GetActorLocation().Z)>250 && FMath::Abs(Goals[Stop].X-(Site.X-1000))<1;
    if(StairLeg)Path.Add(Goals[Stop]);
    else
    if(!Phase205cSupportedWalkPath(W,P,Goals[Stop],Path,110,25,&Survey,5000))
    {for(const auto& R:Survey.Rejections)Test->AddInfo(FString::Printf(TEXT("Path rejection %s: %d"),*R.Key,R.Value));return End(FString::Printf(TEXT("Estate physical path failed stop%d: %s at%s"),Stop,*Survey.Failure,*P->GetActorLocation().ToString()));}
    Point=0;At=W->GetTimeSeconds();
   }
   const FVector Delta=Path[Point]-P->GetActorLocation();
   if(Delta.Size2D()<18 && FMath::Abs(Delta.Z)<45)
   {if(++Point>=Path.Num()){Path.Reset();Test->AddInfo(FString::Printf(TEXT("Actual walking reached stop%d at%s"),Stop,*P->GetActorLocation().ToString()));UE_LOG(LogTemp,Display,TEXT("MAINLAND916_ESTATE_STOP %d at %s"),Stop,*P->GetActorLocation().ToString());if(++Stop>=Goals.Num()){if(Scope==EAccessScope::Stairs || Scope==EAccessScope::Gallery)return End(TEXT(""));Stage=2;At=W->GetTimeSeconds();} }return false;}
   if(Age>30)
   {FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(EstateBlockedActual),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+Delta.GetSafeNormal2D()*100,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);return End(FString::Printf(TEXT("Actual estate walking blocked stop%d point%d at%s by %s normal%s impact%s"),Stop,Point,*P->GetActorLocation().ToString(),*GetNameSafe(H.GetActor()),*H.ImpactNormal.ToString(),*H.ImpactPoint.ToString()));}
   P->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(float(Delta.Size2D()/100),.08f,1.f));PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));return false;
  }
  if(Stage==2 && Age>.5)
  {if(!Focus(PC,P,Cinema))return End(TEXT("Actual cinema prompt cannot be focused from its clear approach"));Key(PC,EKeys::F,true);Stage=3;At=W->GetTimeSeconds();return false;}
  if(Stage==3 && Age>.3)
  {Key(PC,EKeys::F,false);if(!Cinema->IsOccupied() || !Cinema->IsFeedbackPlaying())return End(TEXT("Normal F did not start the actual fitted cinema and feedback"));Stage=4;At=W->GetTimeSeconds();return false;}
  if(Stage==4 && Age>3)
  {
#if WITH_EDITOR
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,TEXT("Estate916-Cinema.jpg"));
#endif
   Key(PC,EKeys::W,true);Stage=5;At=W->GetTimeSeconds();return false;
  }
  if(Stage==5 && Age>.4){Key(PC,EKeys::W,false);if(Cinema->IsOccupied() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Cinema exit did not restore normal walking"));return End(TEXT(""));}
  return false;
 }
private:
 bool End(const FString& Error)
 {Key(PC,EKeys::F,false);Key(PC,EKeys::W,false);if(!Error.IsEmpty())Test->AddError(Error);Test->AddInfo(FString::Printf(TEXT("%s reached %d/%d physical walking stops"),Scope==EAccessScope::Full?TEXT("Continuous forecourt/entry/terrace/suites/cinema tour"):TEXT("Disclosed partial access fixture"),Stop,Goals.Num()));Metrics.Flush();Ending=true;if(Cinema && Cinema->IsOccupied() && !Cinema->Leave()){Test->AddError(TEXT("Cinema cleanup found no safe standing exit"));P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);}return false;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandGrill* Grill=nullptr;AIslandLeisureFacility* Cinema=nullptr;AIslandEstateAmenity* Sink=nullptr;FTransform Original;TArray<FVector> Goals,Path;int32 Stage=0,Stop=0,Point=0;double Started=0;float At=0;bool Ending=false;EAccessScope Scope=EAccessScope::Full;FNeonCityLandFrameMetrics Metrics{TEXT("Mainland916-EstateWalk")};
};
class FBBQ : public IAutomationLatentCommand
{
public:explicit FBBQ(FAutomationTestBase* T,bool InIndoor=false):Test(T),Indoor(InIndoor){}
 bool Update()override
 {
  W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;if(!PC || W->GetTimeSeconds()<6)return false;
  if(!Started)Started=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Started>75)return End(TEXT("Fitted estate BBQ timeout"));const float Age=W->GetTimeSeconds()-At;
  if(!P)
  {
   P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!P)return End(TEXT("Normal player absent"));Original=P->GetActorTransform();
   for(TActorIterator<AIslandLandGrill> I(W);I;++I)if(I->StationID==(Indoor?TEXT("Mainland916.Estate.Hob"):TEXT("Mainland916.Estate.BBQ")))Grill=*I;
   Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Fishing=PC->FindComponentByClass<UFishingComponent>();if(!Grill || !Inventory || !Fishing)return End(TEXT("Shared grill inventory unavailable"));
   if(Fishing->IsActive() || P->GetAttachParentActor())return End(TEXT("BBQ fixture requires an idle unseated player"));
   Snapshot=Inventory->BuildPersistenceSnapshot();SnapshotCaptured=true;
   // Explicit ingredient fixture: this tests the new installation, not catching.
   FRandomStream Random(916);auto Fish=Inventory->SampleCatch(Random,TEXT("Coastal"),12,TEXT("Clear"),TEXT("TestFixture.Mainland916"));if(!Inventory->StoreCatch(Fish))return End(TEXT("Ingredient fixture rejected"));FishID=Fish.FishID;
   if(Grill->bIndoorCooktop!=Indoor)return End(TEXT("Installed station cooking mode does not match its identity"));
   P->SetActorLocation(Grill->GetActorTransform().TransformPosition(FVector(0,Indoor?95:-95,P->GetSimpleCollisionHalfHeight()+3)),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);At=W->GetTimeSeconds();return false;
  }
  if(Stage==0 && Age>2)
  {
   if(Indoor)
   {
    const auto* Hob=Cast<AIslandEstateCooktop>(Grill);if(!Hob || !Hob->Pan || !Hob->TargetCooktop)return End(TEXT("Native hob or fitted counter missing"));
    const FVector PanBottom=Hob->Pan->Bounds.Origin-FVector(0,0,Hob->Pan->Bounds.BoxExtent.Z);FHitResult Surface;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(MainlandHobSurface916),true,Grill);
    if(!W->LineTraceSingleByChannel(Surface,PanBottom+FVector(0,0,20),PanBottom-FVector(0,0,20),ECC_Pawn,Q) || Surface.GetActor()!=Hob->TargetCooktop || Surface.ImpactNormal.Z<.7 || FMath::Abs(Surface.ImpactPoint.Z-PanBottom.Z)>3)
     return End(TEXT("Cooking pan is floating or intersects the fitted induction surface"));
    Test->AddInfo(FString::Printf(TEXT("Native pan bottom is%.2fcm from the collidable induction surface"),FMath::Abs(Surface.ImpactPoint.Z-PanBottom.Z)));
   }
   if(!Focus(PC,P,Grill))return End(TEXT("Installed cooking station normal prompt missing"));Key(PC,EKeys::F,true);return Next(1);
  }
  if(Stage==1 && Age>.3){Key(PC,EKeys::F,false);if(Fishing->GetActiveStation()!=Grill)return End(TEXT("Normal F failed to open the installed grill"));return Next(2);}
  if(Stage==2 && Age>.2){Key(PC,EKeys::Enter,true);return Next(3);}
  if(Stage==3 && Age>.3){Key(PC,EKeys::Enter,false);if(Grill->GetReservedFishID()!=FishID)return End(TEXT("Enter failed to reserve the exact fixture ingredient"));return Next(4);}
  if(Stage==4)
  {
   const auto S=Grill->GetCookingStage();Seen.Add(uint8(S));Sound|=Grill->IsCookingAudioPlaying();VisibleFood|=Grill->Food->IsVisible();FTransform R,L;
   // Read evaluated bones after the normal animation graph. Returning targets
   // alone cannot establish that IK actually reached either cooking tool.
   if(Grill->GetCookingHandTargets(R,L) && Grill->GetStageFraction()>.35f && S!=ENeonCityGrillStage::Ready)
   {
    const float RE=FVector::Dist(P->GetMesh()->GetSocketLocation(TEXT("hand_r")),R.GetLocation());
    const float LE=FVector::Dist(P->GetMesh()->GetSocketLocation(TEXT("hand_l")),L.GetLocation());
    const FVector Local=Grill->GetActorTransform().InverseTransformPosition(P->GetActorLocation());
    const bool Contact=RE<12 && LE<12 && FVector::Dist2D(Local,Grill->GetWorkPositionLocal(Local.X))<20;
    float& Best=BestHandErrors.FindOrAdd(uint8(S),MAX_flt);Best=FMath::Min(Best,FMath::Max(RE,LE));
    if(S==ENeonCityGrillStage::Opening && !OpeningLogged && Grill->GetStageFraction()>.55f)
    {
     OpeningLogged=true;Test->AddInfo(FString::Printf(TEXT("Opening evaluated R%.1f L%.1f fraction%.2f; player%s yaw%.1f; bone%s target%s shoulder%s"),RE,LE,Grill->GetStageFraction(),*Local.ToString(),P->GetActorRotation().Yaw,*P->GetMesh()->GetSocketLocation(TEXT("hand_r")).ToString(),*R.GetLocation().ToString(),*P->GetMesh()->GetSocketLocation(TEXT("upperarm_r")).ToString()));
#if WITH_EDITOR
     if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))FNeonCityVisualEvidence::CaptureRenderedJPEG(W,Indoor?TEXT("Estate916-HobPreparation.jpg"):TEXT("Estate916-BBQOpening.jpg"));
#endif
    }
    if(S==ENeonCityGrillStage::Igniting && !IgnitionLogged && Grill->GetStageFraction()>.8f)
    {IgnitionLogged=true;Test->AddInfo(FString::Printf(TEXT("Ignition actual hand error R%.1f L%.1f; player local %s; right bone %s target %s; shoulder %s"),RE,LE,*Local.ToString(),*P->GetMesh()->GetSocketLocation(TEXT("hand_r")).ToString(),*R.GetLocation().ToString(),*P->GetMesh()->GetSocketLocation(TEXT("upperarm_r")).ToString()));}
    if(Contact)ContactStages.Add(uint8(S));
    MaxRightError=FMath::Max(MaxRightError,RE);MaxLeftError=FMath::Max(MaxLeftError,LE);
   }
   if(S!=ENeonCityGrillStage::Ready)return false;
   for(uint8 I=1;I<=8;++I)Test->TestTrue(TEXT("All physical cooking stages observed"),Seen.Contains(I));Test->TestTrue(TEXT("Real cooking sound and visible food"),Sound && VisibleFood);
   for(uint8 I=1;I<=7;++I){Test->TestTrue(*FString::Printf(TEXT("Evaluated hand bones reach both targets in stage %d"),I),ContactStages.Contains(I));Test->AddInfo(FString::Printf(TEXT("Stage%d best evaluated hand error %.1fcm"),I,BestHandErrors.FindRef(I)));}
   Test->AddInfo(FString::Printf(TEXT("Cooking hand maximum sampled error right %.1fcm left %.1fcm"),MaxRightError,MaxLeftError));
   if(!MealRetained())return End(TEXT("Exact fish did not become one unique prepared meal without altering other inventory"));Key(PC,EKeys::One,true);return Next(5);
  }
  if(Stage==5 && Age>.3){Key(PC,EKeys::One,false);if(!MealRetained())return End(TEXT("Store lost or duplicated the exact prepared meal"));if(Grill->GetCookingStage()!=ENeonCityGrillStage::Closing)return End(TEXT("Normal Store key did not plate/store/close"));return Next(6);}
  if(Stage==6)
  {
   FTransform R,L;
   if(Grill->GetStageFraction()>.35f && Grill->GetCookingHandTargets(R,L))
   {
    const float RE=FVector::Dist(P->GetMesh()->GetSocketLocation(TEXT("hand_r")),R.GetLocation()),LE=FVector::Dist(P->GetMesh()->GetSocketLocation(TEXT("hand_l")),L.GetLocation());
    BestClosingError=FMath::Min(BestClosingError,FMath::Max(RE,LE));ClosingContact|=RE<12 && LE<12;
   }
   if(Grill->GetCookingStage()==ENeonCityGrillStage::Idle){Test->TestTrue(TEXT("Evaluated closing hands reach the actual lid and tool"),ClosingContact);Test->AddInfo(FString::Printf(TEXT("Closing best evaluated hand error %.1fcm"),BestClosingError));Key(PC,EKeys::F,true);return Next(7);}
  }
  if(Stage==7 && Age>.3){Key(PC,EKeys::F,false);if(!MealRetained())return End(TEXT("BBQ exit lost or duplicated stored food"));if(Fishing->IsActive() || PC->IsMoveInputIgnored())return End(TEXT("BBQ exit retained input lock"));return End(TEXT(""));}return false;
 }
private:
 bool MealRetained() const
 {
  const auto Current=Inventory->BuildPersistenceSnapshot();
  if(Current.Fish.Num()!=Snapshot.Fish.Num() || Current.Meals.Num()!=Snapshot.Meals.Num()+1 || Inventory->FindFish(FishID))return false;
  int32 Matches=0;for(const auto& M:Current.Meals)if(M.SourceFishID==FishID){if(M.MealID!=FishID)return false;++Matches;}
  if(Matches!=1)return false;
  // Compare every retained field, not just the total count or IDs.
  for(const auto& Expected:Snapshot.Fish){const auto* Actual=Inventory->FindFish(Expected.FishID);if(!Actual || !FNeonCityCaughtFish::StaticStruct()->CompareScriptStruct(&Expected,Actual,0))return false;}
  for(const auto& Expected:Snapshot.Meals){const auto* Actual=Current.Meals.FindByPredicate([&](const auto& M){return M.MealID==Expected.MealID;});if(!Actual || !FNeonCityFishMeal::StaticStruct()->CompareScriptStruct(&Expected,Actual,0))return false;}
  return true;
 }
 bool Next(int32 N){Stage=N;At=W->GetTimeSeconds();return false;}
 bool End(const FString& Error)
 {for(FKey K:{EKeys::F,EKeys::Enter,EKeys::One})Key(PC,K,false);if(SnapshotCaptured){Fishing->Stop();Inventory->ApplyPersistenceSnapshot(Snapshot);if(P)P->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);}if(!Error.IsEmpty())Test->AddError(Error);return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandGrill* Grill=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;UFishingComponent* Fishing=nullptr;FNeonCityFishingSaveData Snapshot;FTransform Original;FGuid FishID;TSet<uint8> Seen,ContactStages;TMap<uint8,float> BestHandErrors;bool SnapshotCaptured=false,Sound=false,VisibleFood=false,IgnitionLogged=false,OpeningLogged=false,ClosingContact=false,Indoor=false;float MaxRightError=0,MaxLeftError=0,BestClosingError=MAX_flt;int32 Stage=0;double Started=0;float At=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateAccess916,"NeonCity.Phase205c.Mainland916.Standalone.EstateAccess",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateAccess916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAccess(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateBBQ916,"NeonCity.Phase205c.Mainland916.Standalone.EstateBBQ",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateBBQ916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FBBQ(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateHob916,"NeonCity.Phase205c.Mainland916.Standalone.EstateHob",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateHob916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FBBQ(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateStairs916,"NeonCity.Phase205c.Mainland916.Standalone.EstateStairsRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateStairs916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAccess(this,MainlandEstate916::EAccessScope::Stairs));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateRemaining916,"NeonCity.Phase205c.Mainland916.Standalone.EstateRemainingRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateRemaining916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAccess(this,MainlandEstate916::EAccessScope::Remaining));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateGallery916,"NeonCity.Phase205c.Mainland916.Standalone.EstateGalleryRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateGallery916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAccess(this,MainlandEstate916::EAccessScope::Gallery));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateDescent916,"NeonCity.Phase205c.Mainland916.Standalone.EstateDescentCinemaRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateDescent916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAccess(this,MainlandEstate916::EAccessScope::DescentCinema));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainlandEstateCinema916,"NeonCity.Phase205c.Mainland916.Standalone.EstateCinemaRepro",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMainlandEstateCinema916::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")))return false;ADD_LATENT_AUTOMATION_COMMAND(MainlandEstate916::FAccess(this,MainlandEstate916::EAccessScope::Cinema));return true;}
#endif
