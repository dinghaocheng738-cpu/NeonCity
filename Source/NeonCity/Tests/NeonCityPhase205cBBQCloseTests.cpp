#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandGrill.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
namespace BBQClose
{
class FReview:public IAutomationLatentCommand
{
public:
 explicit FReview(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>65)return End(TEXT("Native BBQ close inspection timeout"));if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30500))))return false;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Fishing=PC->FindComponentByClass<UFishingComponent>();for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(!It->bIndoorCooktop)Grill=*It;
   if(!Grill || !Fishing || !Save->ConfigureSlot(TEXT("NeonCity_Test_LandFinal_BoatCatch"),false) || !Save->DoesSaveExist())return End(TEXT("Actual retained boat catch required; no fish or cooking fixture allowed"));Key(EKeys::F7,true);return Next(1);
  }
  if(Stage==1 && Age>.5)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(Save->GetLastDetail());
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {if(Stow.IsValid())return End(TEXT("Normal BBQ weapon stow failed"));const int32 Slot=Weapon->GetData()?Weapon->GetData()->SlotType:0;const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(Slot<1 || Slot>8)return End(TEXT("Normal weapon stow mapping absent"));Stow=Slots[Slot-1];Key(Stow,true);return Next(8);}
   for(const auto& R:Inventory->GetFish())if(R.Storage==ENeonCityFishStorage::Pouch && !R.FishingPointID.IsNone() && R.CaughtAt.GetTicks()>0){Fish=R.FishID;break;}
   if(!Fish.IsValid())return End(TEXT("Retained source has no actual kept fish with catch provenance"));MealsBefore=Inventory->GetMeals().Num();
   // One declared local supported start; never claim a repeated estate route.
   const FVector Ground=Grill->GetActorTransform().TransformPosition(Grill->GetWorkPositionLocal());FHitResult Floor;FCollisionQueryParams Q(SCENE_QUERY_STAT(BBQCloseSupportedStart),false,P);
   if(!W->LineTraceSingleByChannel(Floor,Ground+FVector(0,0,150),Ground-FVector(0,0,100),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7f || FMath::Abs(Floor.ImpactPoint.Z-Ground.Z)>12)return End(TEXT("Native BBQ local start lacks supported floor"));
   const FVector Stand=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Native BBQ local start blocks the full player capsule"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(9);
  }
  if(Stage==8 && Age>.2f){Key(Stow,false);return Next(1);}
  if(Stage==9 && Age>.4f)
  {
   PC->SetControlRotation((Grill->InteractionBounds->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   return Next(6);
  }
  if(Stage==6 && Age>.25f)
  {
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Grill)
   {
    FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);FHitResult Hit;auto* Interaction=P->GetInteractionComponent();Interaction->DetectInteractable(Eye,Eye+View.Vector()*Interaction->InteractionDistance,Hit);
    return End(FString::Printf(TEXT("Restored BBQ F focus: hit%s/%s reach%d canStart%d grounded%d speed%.2f eye%s target%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Grill->IsInReach(P),Fishing->CanStart(Grill),P->GetCharacterMovement()->IsMovingOnGround(),P->GetVelocity().Size2D(),*Eye.ToString(),*Grill->InteractionBounds->GetComponentLocation().ToString()));
   }
   OrientBefore=P->GetCharacterMovement()->bOrientRotationToMovement;Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.2)
  {
   Key(EKeys::F,false);if(Fishing->GetActiveStation()!=Grill)return End(TEXT("Native F did not enter the real cooking station"));Key(EKeys::Enter,true);return Next(7);
  }
  if(Stage==7 && Age>.2f)
  {
   Key(EKeys::Enter,false);if(Grill->GetReservedFishID()!=Fish)return End(TEXT("Native Enter did not reserve the exact genuine caught fish"));Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=60;
   const FVector Eye=Grill->GetActorTransform().TransformPosition(FVector(-150,-170,160)),Aim=Grill->GetActorTransform().TransformPosition(FVector(0,-15,110));Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PC->SetViewTarget(Camera);return Next(3);
  }
  if(Stage==3)
  {
   const auto Cooking=Grill->GetCookingStage();const float Fraction=Grill->GetStageFraction();FTransform Right,Left;
   if(Cooking==ENeonCityGrillStage::Ready)
   {
    if(!Seen.Contains(uint8(ENeonCityGrillStage::Turning)) || !Seen.Contains(uint8(ENeonCityGrillStage::Plating)))return End(TEXT("Actual flip/plating close captures missing"));
    if(Inventory->FindFish(Fish) || Inventory->GetMeals().Num()!=MealsBefore+1 || !Inventory->GetMeals().ContainsByPredicate([&](const auto& M){return M.SourceFishID==Fish;}))return End(TEXT("Close presentation lost original single meal identity"));
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("BBQ-Close-Ready-Plate.png"));Key(EKeys::One,true);return Next(4);
   }
   if(Age>.25 && Grill->GetCookingHandTargets(Right,Left))
   {
    const float R=FVector::Dist(P->GetMesh()->GetBoneLocation(TEXT("hand_r")),Right.GetLocation()),L=FVector::Dist(P->GetMesh()->GetBoneLocation(TEXT("hand_l")),Left.GetLocation());
    const FVector RS=P->GetMesh()->GetBoneLocation(TEXT("upperarm_r")),LS=P->GetMesh()->GetBoneLocation(TEXT("upperarm_l"));const auto* Anim=Cast<UCharacterPresentationAnimInstance>(P->GetMesh()->GetAnimInstance());
    CSV+=FString::Printf(TEXT("%.4f,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),W->GetTimeSeconds(),uint8(Cooking),Fraction,R,L,Grill->Food->GetComponentLocation().X,Grill->Food->GetComponentLocation().Y,Grill->Food->GetComponentLocation().Z,P->GetActorRotation().Yaw,Anim?Anim->FishingAlpha:-1,RS.X,RS.Y,RS.Z,Right.GetLocation().X,Right.GetLocation().Y,Right.GetLocation().Z,LS.X,LS.Y,LS.Z,Left.GetLocation().X,Left.GetLocation().Y,Left.GetLocation().Z,FVector::Dist(RS,Right.GetLocation()));
    if((Cooking==ENeonCityGrillStage::Turning || Cooking==ENeonCityGrillStage::Plating) && Fraction>.25 && Fraction<.8){MaxRight=FMath::Max(MaxRight,R);MaxLeft=FMath::Max(MaxLeft,L);++Samples;}
   }
   if(Fraction>.35 && !Seen.Contains(uint8(Cooking)))
   {Seen.Add(uint8(Cooking));FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("BBQ-Close-Stage%d.png"),uint8(Cooking)));}
   return false;
  }
  if(Stage==4 && Age>.2){Key(EKeys::One,false);if(Grill->GetCookingStage()!=ENeonCityGrillStage::Closing)return End(TEXT("Real1 Store did not acknowledge plated result"));Key(EKeys::F,true);return Next(5);}
  if(Stage==5 && Age>.25)
  {Key(EKeys::F,false);if(Fishing->IsActive() || PC->IsMoveInputIgnored() || P->GetCharacterMovement()->bOrientRotationToMovement!=OrientBefore)return End(TEXT("Close cooking inspection did not restore normal input and walking orientation"));if(Samples<3 || MaxRight>12 || MaxLeft>12)return End(FString::Printf(TEXT("Actual skeletal hands miss physical cooking targets: samples%d right%.2f left%.2fcm"),Samples,MaxRight,MaxLeft));return End(TEXT(""));}
  return false;
 }
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(const FString& Error)
 {
  for(auto K:{EKeys::F7,EKeys::F,EKeys::Enter,EKeys::One,Stow})if(K.IsValid())Key(K,false);if(PC && Camera)PC->SetViewTarget(P);if(Camera)Camera->Destroy();if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetStringField(TEXT("source_fish_id"),Fish.ToString());J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("hand_samples"),Samples);J->SetNumberField(TEXT("maximum_right_hand_error_cm"),MaxRight);J->SetNumberField(TEXT("maximum_left_hand_error_cm"),MaxLeft);J->SetStringField(TEXT("scope"),TEXT("Genuine native F7 raw boat-catch checkpoint; one declared supported full-capsule local start. Normal stow/F/Enter starts real cooking. Actual visible hand bones versus tool/lid targets during flip and plating, close normal rendered PIE camera, source-fish single result, mapped1 Store/F leave. No pose writes after entry, ingredient/cooking-state/completion injection or save write. Old full estate walk/Store/Eat/cold recovery remain cached; this accepts hand/tool close evidence, never a repeated route claim."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run)/TEXT("BBQClose");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("actual-hand-food-trajectory.csv")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandLandGrill* Grill=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;UFishingComponent* Fishing=nullptr;ACameraActor* Camera=nullptr;
 int Stage=0,MealsBefore=0,Samples=0;bool OrientBefore=false;float At=0,MaxRight=0,MaxLeft=0;double Start=0;FGuid Fish;FKey Stow;TSet<uint8> Seen;FString CSV=TEXT("game_seconds,cooking_stage,fraction,right_hand_error_cm,left_hand_error_cm,food_x,food_y,food_z,player_yaw,fishing_alpha,right_shoulder_x,right_shoulder_y,right_shoulder_z,right_target_x,right_target_y,right_target_z,left_shoulder_x,left_shoulder_y,left_shoulder_z,left_target_x,left_target_y,left_target_z,right_requested_reach_cm\n");FNeonCityVisualEvidence Evidence{TEXT("BBQClose")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBQCloseTest,"NeonCity.Phase205c.BBQClosePresentationPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBQCloseTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(BBQClose::FReview(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
