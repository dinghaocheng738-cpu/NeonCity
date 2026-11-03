#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandLandGrill.h"
#include "Adventure/IslandResidenceLift.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageSystem.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace RidgeBBQ
{
class FAccessSurvey:public IAutomationLatentCommand
{
public:
 explicit FAccessSurvey(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  AIslandResidenceLift* Lift=nullptr;AIslandLandGrill* Grill=nullptr;for(TActorIterator<AIslandResidenceLift> I(W);I;++I)if(I->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.TheOne478")))Lift=*I;for(TActorIterator<AIslandLandGrill> I(W);I;++I)if(!I->bIndoorCooktop)Grill=*I;
  if(!Lift || !Grill){Test->AddError(TEXT("Current native owners missing"));return true;}
  // Reproduce the measured normal668 cabin exit, including actual CMC floor
  // gap and lateral offset. Compare coarse/fine read-only grids in one run.
  float Exit=230.684f,Gap=2.15f;FParse::Value(FCommandLine::Get(),TEXT("NeonCityLiftExitCm="),Exit);FParse::Value(FCommandLine::Get(),TEXT("NeonCityLiftFloorGapCm="),Gap);
  const FVector Start=Lift->GetActorTransform().TransformPosition(FVector(Exit,-.2f,304.8))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+Gap);
  // One declared actual Main landing fixture; read-only collision survey only.
  P->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
  const FVector Goal=Grill->GetActorTransform().TransformPosition(FVector(0,-95,0))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
  TArray<FVector> Path;FPhase205cWalkSurvey S;const bool Found=Phase205cSupportedWalkPath(W,P,Goal,Path,P->GetSimpleCollisionHalfHeight()+25.f,50.f,&S,30000);
  FString Report=FString::Printf(TEXT("Read-only current landing-to-BBQ full capsule survey. No gameplay PASS claim.\nstart=%s\ngoal=%s\nfound=%d nodes=%d expanded=%d probed=%d open=%d\nfailure=%s\n"),*Start.ToString(),*Goal.ToString(),Found,Path.Num(),S.Expanded,S.Probed,S.OpenAtExit,*S.Failure);
  TArray<FString> Keys;S.Rejections.GetKeys(Keys);Keys.Sort();for(const auto& K:Keys)Report+=FString::Printf(TEXT("%s=%d\n"),*K,S.Rejections[K]);
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BBQAccessEndpoint),false,P);for(const auto& Pair:{TPair<FString,FVector>(TEXT("start"),Start),TPair<FString,FVector>(TEXT("goal"),Goal)})
  {TArray<FOverlapResult> H;W->OverlapMultiByChannel(H,Pair.Value,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);for(const auto& O:H)if(O.bBlockingHit && O.GetActor())Report+=Pair.Key+TEXT(" capsule:")+O.GetActor()->GetName()+TEXT("\n");}
  TArray<FVector> FinePath;FPhase205cWalkSurvey Fine;const bool FineFound=Phase205cSupportedWalkPath(W,P,Goal,FinePath,P->GetSimpleCollisionHalfHeight()+25.f,25.f,&Fine,50000);
  Report+=FString::Printf(TEXT("Fine25cm measured exit: found=%d nodes=%d expanded=%d probed=%d open=%d reason%s\n"),FineFound,FinePath.Num(),Fine.Expanded,Fine.Probed,Fine.OpenAtExit,*Fine.Failure);
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const auto Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("BBQAccess.txt")));FString Cells=TEXT("x_cm,y_cm,z_cm\n");for(const auto& V:S.Explored)Cells+=FString::Printf(TEXT("%.3f,%.3f,%.3f\n"),V.X,V.Y,V.Z);FFileHelper::SaveStringToFile(Cells,*(Dir/TEXT("BBQAccessCells.csv")));Cells=TEXT("x_cm,y_cm,z_cm\n");for(const auto& V:FinePath)Cells+=FString::Printf(TEXT("%.3f,%.3f,%.3f\n"),V.X,V.Y,V.Z);FFileHelper::SaveStringToFile(Cells,*(Dir/TEXT("BBQFinePath.csv")));UE_LOG(LogTemp,Display,TEXT("%s"),*Report);return true;
 }
private:FAutomationTestBase* Test;
};
FVector M(double X,double Y,double Z){return NeonCityCoastalLayout::FromLegacyIsland(FVector(48600+X*100,-118800-Y*100,30480+Z*100));}
class FSequence:public IAutomationLatentCommand
{
public:
 explicit FSequence(FAutomationTestBase* T,bool NativeEstate=false):Test(T),bNativeEstate(NativeEstate){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!Player || W->GetTimeSeconds()<4)return false;
  if(FPlatformTime::Seconds()-Started>200)return End(TEXT("BBQ sequence timed out"));
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(bNativeEstate?NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254)):M(0,0,0)))return false;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Fishing=PC->FindComponentByClass<UFishingComponent>();
   for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(!It->bIndoorCooktop)Grill=*It;
   if(bNativeEstate)for(TActorIterator<AIslandResidenceLift> It(W);It;++It)if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.TheOne478")))Lift=*It;
   if(!Grill || !Fishing)return End(TEXT("Actual grill or fishing input component missing"));
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->DoesSaveExist())return End(TEXT("Real caught-fish source save missing; no synthetic fish fixture allowed"));
   if(!Save->LoadGame())return End(Save->GetLastDetail());return Next(1);
  }
  if(Stage==1 && Age>.7)
  {
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Real catch checkpoint load failed"));
   if(auto* Weapon=Player->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {
    if(Stowed)return End(TEXT("Native weapon slot failed to stow"));Stowed=true;
    const int32 Slot=Weapon->GetData()?Weapon->GetData()->SlotType:0;const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};
    if(Slot<1 || Slot>8)return End(TEXT("No real weapon slot mapping"));TapKey=Keys[Slot-1];Key(TapKey,IE_Pressed);return Next(20);
   }
   for(const auto& F:Inventory->GetFish())if(F.Storage==ENeonCityFishStorage::Pouch && !F.FishingPointID.IsNone() && F.CaughtAt.GetTicks()>0)Fish.Add(F);
   if(Fish.Num()<2)return End(TEXT("Two genuine kept ingredients required"));
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
   FocusedSlot=bNativeEstate?TEXT("NeonCity_Test_TheOneBBQ_")+FPaths::MakeValidFileName(Run.Left(24)):TEXT("NeonCity_Test_RidgeBBQSequence");
   if(!Save->ConfigureSlot(FocusedSlot,false) || Save->DoesSaveExist())return End(TEXT("Fresh focused BBQ slot required"));
   if(bNativeEstate)
   {
    if(!Lift || Lift->GetLandingCount()!=4)return End(TEXT("Actual authored four-floor estate lift missing"));
    const FVector Landing=Lift->GetActorTransform().TransformPosition(FVector(225,0,0));FHitResult Hit;
    if(!W->LineTraceSingleByChannel(Hit,Landing+FVector(0,0,40),Landing-FVector(0,0,40),ECC_Pawn) || FMath::Abs(Hit.ImpactPoint.Z-Landing.Z)>3)return End(TEXT("Actual initial garage landing lacks support"));
    Player->SetActorLocation(Landing+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
    return Next(28);
   }
   // One dry public-floor fixture; movement from here uses normal CMC input.
   Player->SetActorLocation(M(-11.4,1.75,4.2)+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();
   if(Player->GetHealthComponent()->GetCurrentHealth()>=Player->GetHealthComponent()->GetMaxHealth() && Player->GetPlayerDataComponent()->GetStamina()>=100)
   {
    // Declared nonlethal native damage stimulus for the Eat positive case;
    // no inventory, stamina, animation, reward or completion state is injected.
    FNeonCityDamageEvent D;D.DamageAmount=20;D.DamageType=ENeonCityDamageType::Environmental;D.HitLocation=Player->GetActorLocation();DamageStimulus=W->GetSubsystem<UDamageSystem>()->ApplyDamage(Player,D);
   }
   // Use the authored public door (x=1..3.8), then the dry deck between
   // the facade and basin. The west sofa/glass wall is not an exit.
   Path={M(-11.4,10,4.2),M(2.4,10,4.2),M(2.4,20,4.2),M(-10,20,4.2),Grill->GetActorTransform().TransformPosition(FVector(0,-95,0))};GoalAt=Now;return Next(2);
  }
  if(Stage==20 && Age>.2){Key(TapKey,IE_Released);return Next(1);}
  if(Stage==28)
  {
   if(Lift->GetCurrentFloor()!=0 || !Lift->AreDoorsOpen())
   {
    if(Age>25)return End(TEXT("Actual garage lift call did not arrive"));
    if(!Called){PC->SetControlRotation((Lift->CallPanels[0]->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());
     if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return End(TEXT("Actual garage call prompt inaccessible"));Key(EKeys::F,IE_Pressed);Called=true;}
    else Key(EKeys::F,IE_Released);return false;
   }
   const FVector D=Lift->Cabin->GetComponentLocation()-Player->GetActorLocation();
   if(D.Size2D()>25){if(Age>30)return End(TEXT("Reflowed garage to cabin entry blocked"));PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Player->DoMove(0,FMath::Clamp(float(D.Size2D()/180),.1f,1.f));return false;}
   if(Player->GetMovementBaseObject()!=Lift->Floor)return false;
   PC->SetControlRotation((Lift->Cabin->GetComponentTransform().TransformPosition(FVector(117,103,140))-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return End(TEXT("Actual main floor ride prompt missing"));Key(EKeys::F,IE_Pressed);return Next(29);
  }
  if(Stage==29)
  {
   if(Age>.2)Key(EKeys::F,IE_Released);
   if(Age>25)return End(TEXT("Actual garage to main ride failed"));
   if(Lift->GetCurrentFloor()==1 && Lift->AreDoorsOpen())return Next(30);return false;
  }
  if(Stage==30)
  {
   const FVector Landing=Lift->GetActorTransform().TransformPosition(FVector(255,0,304.8));const FVector D=Landing-Player->GetActorLocation();
   if(D.Size2D()>25){if(Age>10)return End(TEXT("Actual main floor cabin exit blocked by architecture"));PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Player->DoMove(0,FMath::Clamp(float(D.Size2D()/180),.1f,1.f));return false;}
   const FVector Goal=Grill->GetActorTransform().TransformPosition(FVector(0,-95,0));
   if(!FindWalk(Goal))return End(TEXT("No physically supported main-floor walk from lift to native terrace BBQ"));
   Waypoint=0;GoalAt=Now;return Next(2);
  }
  if(Stage==2)
  {
   const FVector Delta=Path[Waypoint]-Player->GetActorLocation();
   if(Delta.Size2D()<35){if(++Waypoint==Path.Num())return Next(3);GoalAt=Now;}
   else
   {if(Now-GoalAt>18)return End(FString::Printf(TEXT("Actual interior/terrace walk blocked waypoint%d at%s"),Waypoint,*Player->GetActorLocation().ToString()));PC->SetControlRotation(FRotator(0,Delta.Rotation().Yaw,0));Player->DoMove(0,FMath::Clamp(float(Delta.Size2D()/220),.08f,1.f));}
  }
  if(Stage==3 && Age>.5)
  {
   PC->SetControlRotation((Grill->InteractionBounds->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Grill)return End(TEXT("Real grill prompt cannot be focused"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BBQ-Upgrade-Prompt.png"));Key(EKeys::F,IE_Pressed);return Next(4);
  }
  if(Stage==4 && Age>.2)
  {Key(EKeys::F,IE_Released);if(Fishing->GetActiveStation()!=Grill)return End(TEXT("Mapped F failed to open actual grill"));Selected=0;return Next(5);}
  if(Stage==5 && Age>.15)
  {
   TArray<FNeonCityCaughtFish> Pouch;for(const auto& F:Inventory->GetFish())if(F.Storage==ENeonCityFishStorage::Pouch)Pouch.Add(F);
   const int32 Wanted=Pouch.IndexOfByPredicate([&](const auto& F){return F.FishID==Fish[CookIndex].FishID;});if(Wanted<0)return End(TEXT("Genuine ingredient disappeared"));
   if(Selected<Wanted){++Selected;Key(EKeys::Right,IE_Pressed);return Next(21);}
   MealsBefore=Inventory->GetMeals().Num();Key(EKeys::Enter,IE_Pressed);return Next(6);
  }
  if(Stage==21 && Age>.15){Key(EKeys::Right,IE_Released);return Next(5);}
  if(Stage==6 && Age>.2)
  {
   Key(EKeys::Enter,IE_Released);if(Grill->GetReservedFishID()!=Fish[CookIndex].FishID)return End(TEXT("ENTER did not reserve the exact selected fish"));
   SeenStages.Reset();LastFood=FVector::ZeroVector;FoodMovement=0;return Next(7);
  }
  if(Stage==7)
  {
   const auto S=Grill->GetCookingStage();const uint8 I=uint8(S);if(!SeenStages.Contains(I))
   {SeenStages.Add(I);FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("BBQ-Upgrade-Fish%d-Stage%d.png"),CookIndex,I));}
   if(S!=ENeonCityGrillStage::Ready)
   {
    const auto* Record=Inventory->FindFish(Fish[CookIndex].FishID);
    if(!Record || Record->Storage!=ENeonCityFishStorage::Grill || Inventory->GetMeals().Num()!=MealsBefore)return End(TEXT("Cooking reservation duplicated or consumed too early"));
    if(S==ENeonCityGrillStage::CookingFirst || S==ENeonCityGrillStage::Turning || S==ENeonCityGrillStage::CookingSecond)
    {AudioSeen|=Grill->IsCookingAudioPlaying();HandsSeen|=Grill->GetCookingHandTargets(Right,Left);if(!Grill->Food->IsVisible())return End(TEXT("Actual fish is not visible on the grate"));}
    const FVector P=Grill->Food->GetComponentLocation();if(!LastFood.IsZero())FoodMovement+=FVector::Dist(P,LastFood);LastFood=P;
    if(Age>35)return End(TEXT("Physical cooking sequence did not reach plating"));return false;
   }
   for(uint8 StageID=1;StageID<=8;++StageID)Test->TestTrue(TEXT("All ordered physical cooking stages observed"),SeenStages.Contains(StageID));
   Test->TestTrue(TEXT("Real spatial sizzle played"),AudioSeen);Test->TestTrue(TEXT("Hand targets follow actual cooking state"),HandsSeen);Test->TestTrue(TEXT("Fish physically placed, turned and plated"),FoodMovement>70);
   if(Inventory->FindFish(Fish[CookIndex].FishID) || Inventory->GetMeals().Num()!=MealsBefore+1)return End(TEXT("Completion did not produce exactly one meal from the source fish"));
   const auto* Meal=Inventory->GetMeals().FindByPredicate([&](const auto& F){return F.SourceFishID==Fish[CookIndex].FishID;});if(!Meal || Meal->SourceSizeCm!=Fish[CookIndex].SizeCm)return End(TEXT("Plated food lost individual fish provenance"));
   MealIDs.Add(Meal->MealID);BeforeEating=Player->GetHealthComponent()->GetCurrentHealth();Key(CookIndex==0?EKeys::One:EKeys::Two,IE_Pressed);return Next(8);
  }
  if(Stage==8 && Age>.2)
  {
   Key(CookIndex==0?EKeys::One:EKeys::Two,IE_Released);
   if(Grill->GetCookingStage()!=ENeonCityGrillStage::Closing)return End(TEXT("Mapped Store/Eat did not close and clean the grill"));
   const bool Found=Inventory->GetMeals().ContainsByPredicate([&](const auto& F){return F.MealID==MealIDs.Last();});
   if(Found!=(CookIndex==0))return End(TEXT("Store/Eat shared food inventory result incorrect"));return Next(9);
  }
  if(Stage==9)
  {
   if(Grill->GetCookingStage()!=ENeonCityGrillStage::Idle)return false;
   if(CookIndex==0 && !bNativeEstate){++CookIndex;Selected=0;AudioSeen=HandsSeen=false;return Next(5);}
   Key(EKeys::F,IE_Pressed);return Next(10);
  }
  if(Stage==10 && Age>.2){Key(EKeys::F,IE_Released);if(Fishing->IsActive() || PC->IsMoveInputIgnored())return End(TEXT("Leaving grill failed to restore player movement"));Key(EKeys::F6,IE_Pressed);return Next(11);}
  if(Stage==11 && Age>.8)
  {Key(EKeys::F6,IE_Released);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());return End(TEXT(""));}
  return false;
 }
private:
 bool FindWalk(const FVector& Goal)
 {
  // Reuse the existing full-capsule/native-floor planner, including supported
  // StepUp. Probe below the next storey, and retain every real collision.
  FVector SupportedGoal=Goal;SupportedGoal.Z+=Player->GetSimpleCollisionHalfHeight()+3;
  FPhase205cWalkSurvey Survey;
  //670 measured the same real exit on both grids:50cm exhausts its connected
  // component;25cm finds282 supported capsule nodes. Resolve narrow clearances
  // without moving the player, changing a guard or bypassing native collision.
  if(!Phase205cSupportedWalkPath(W,Player,SupportedGoal,Path,Player->GetSimpleCollisionHalfHeight()+25.f,25.f,&Survey,50000))
  {
   UE_LOG(LogTemp,Error,TEXT("TheOneBBQ access failed real start%s liftLocal%s goal%s nodes%d probed%d open%d reason%s"),*Player->GetActorLocation().ToString(),Lift?*Lift->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString():TEXT("none"),*SupportedGoal.ToString(),Survey.Expanded,Survey.Probed,Survey.OpenAtExit,*Survey.Failure);
   for(const auto& Pair:Survey.Rejections)UE_LOG(LogTemp,Display,TEXT("BBQ rejection%s count%d"),*Pair.Key,Pair.Value);
   return false;
  }
  FVector Previous=Player->GetActorLocation();TArray<FVector> Reduced;
  for(int32 I=0;I<Path.Num();++I)
  {
   if(I+1<Path.Num() && FMath::Abs(Previous.Z-Path[I].Z)<1 && FMath::Abs(Path[I].Z-Path[I+1].Z)<1 &&
      FVector::DotProduct((Path[I]-Previous).GetSafeNormal2D(),(Path[I+1]-Path[I]).GetSafeNormal2D())>.99999)continue;
   Reduced.Add(Path[I]);Previous=Path[I];
  }
  Path=MoveTemp(Reduced);
  UE_LOG(LogTemp,Display,TEXT("TheOneBBQ503 shared supported walk nodes%d start%s goal%s"),Path.Num(),*Player->GetActorLocation().ToString(),*SupportedGoal.ToString());return !Path.IsEmpty();
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);if(PC)for(auto K:{EKeys::F,EKeys::F6,EKeys::Enter,EKeys::One,EKeys::Two,EKeys::Right,TapKey})if(K.IsValid())Key(K,IE_Released);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetNumberField(TEXT("declared_native_damage_stimulus"),DamageStimulus);J->SetNumberField(TEXT("health_before_eat"),BeforeEating);J->SetNumberField(TEXT("health_after_eat"),Player?Player->GetHealthComponent()->GetCurrentHealth():0);J->SetStringField(TEXT("scope"),bNativeEstate?TEXT("Real previous boat-caught fish load; one supported garage landing fixture; normal cabin entry, F garage-to-main ride, cabin exit and native supported walk to terrace; mapped F/ENTER/1/F6; one complete physical cook, exact source GUID and Store. Unchanged Eat mechanics and cold recovery retain prior evidence."):TEXT("Real previous boat-caught fish load; one public-floor fixture; normal walk, mapped F/ENTER/1/2/F6; physical cooking stages, source GUIDs and shared inventory; nonlethal native damage stimulus only if full to exercise Eat. Cross-process recovery is separate."));J->SetStringField(TEXT("save_slot"),bNativeEstate?TEXT("NeonCity_Test_TheOneBBQ503"):TEXT("NeonCity_Test_RidgeBBQSequence"));
  J->SetNumberField(TEXT("last_stage"),Stage);J->SetStringField(TEXT("last_player_pose"),Player?Player->GetActorLocation().ToString():TEXT("missing"));J->SetStringField(TEXT("last_lift_local"),Player && Lift?Lift->GetActorTransform().InverseTransformPosition(Player->GetActorLocation()).ToString():TEXT("missing"));
  J->SetStringField(TEXT("save_slot"),FocusedSlot);TArray<TSharedPtr<FJsonValue>> IDs;for(const auto& F:Fish)IDs.Add(MakeShared<FJsonValueString>(F.FishID.ToString()));J->SetArrayField(TEXT("source_fish_ids"),IDs);IDs.Reset();for(const auto& ID:MealIDs)IDs.Add(MakeShared<FJsonValueString>(ID.ToString()));J->SetArrayField(TEXT("meal_ids"),IDs);
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const auto Folder=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Folder,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Folder/TEXT("BBQSequence.json")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandLandGrill* Grill=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;UFishingComponent* Fishing=nullptr;
 AIslandResidenceLift* Lift=nullptr;bool bNativeEstate=false,Called=false;
 FString FocusedSlot;double Started=0;float At=0,GoalAt=0,FoodMovement=0,DamageStimulus=0,BeforeEating=0;int32 Stage=0,CookIndex=0,Selected=0,Waypoint=0,MealsBefore=0;bool Stowed=false,AudioSeen=false,HandsSeen=false;FKey TapKey;TArray<FNeonCityCaughtFish> Fish;TArray<FGuid> MealIDs;TArray<uint8> SeenStages;TArray<FVector> Path;FVector LastFood;FTransform Right,Left;FNeonCityVisualEvidence Evidence{TEXT("BBQSequence")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRidgeBBQSequence,"NeonCity.Phase205c.BBQSequencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRidgeBBQSequence::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeBBQ::FSequence(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneBBQ503,"NeonCity.Phase205cUpgrade.TheOneBBQWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneBBQ503::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeBBQ::FSequence(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneBBQAccess657,"NeonCity.Phase205cUpgrade.TheOneBBQAccessMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneBBQAccess657::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RidgeBBQ::FAccessSurvey(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
