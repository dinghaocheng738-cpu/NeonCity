#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandEstateAmenity.h"
#include "Adventure/IslandScenePanel.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Fishing/FishingComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
namespace EstateAmenity701
{
FString CapsuleBlockers(UWorld* W,ANeonCityPlayerCharacter* P,const FVector& At,const FCollisionQueryParams& Q)
{
 TArray<FOverlapResult> Hits;W->OverlapMultiByChannel(Hits,At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
 FString Result;for(const auto& Hit:Hits)if(Hit.bBlockingHit)Result+=FString::Printf(TEXT(" %s/%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));return Result;
}
// Declared, supported local start fixtures only. These tests claim neither a
// walking route nor player subjective animation/audio acceptance.
bool Approach(UWorld* W,ANeonCityPlayerCharacter* P,APlayerController* PC,AIslandEstateAmenity* Use)
{
 const FVector Centre=Use->Interaction->GetComponentLocation();
 const float Radius=P->GetSimpleCollisionRadius(),Half=P->GetSimpleCollisionHalfHeight();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(EstateAmenityDeclaredApproach),false,P);Q.AddIgnoredActor(Use);
 // The five ring benches open south. Their north seat is128cm from the
 // centre; the genuine south entrance is90cm beyond it. Generic175cm radial
 // fixtures stop against the central table and never cover that entrance.
 if(Use->AmenityID.ToString().StartsWith(TEXT("Estate701.Ground.LowerWaterLounge")))
 {
  const FVector At=Use->GetActorTransform().TransformPosition(FVector(218,0,0));FHitResult H;
  if(W->LineTraceSingleByChannel(H,At+FVector(0,0,190),At-FVector(0,0,40),ECC_Pawn,Q) && H.ImpactNormal.Z>.7f && FMath::Abs(H.ImpactPoint.Z-At.Z)<18)
  {
   const FVector Stand=H.ImpactPoint+FVector(0,0,Half+3);
   if(!W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))
   {P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((Centre-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()==Use)return true;FVector Eye;FRotator Look;P->GetActorEyesViewPoint(Eye,Look);FHitResult Hit;W->LineTraceSingleByChannel(Hit,Eye,Eye+Look.Vector()*250,ECC_Visibility,Q);UE_LOG(LogTemp,Warning,TEXT("Lower entrance %s focus blocked%s/%s canInteract%d owner%s stand%s eye%s interaction%s"),*Use->AmenityID.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),IInteractableInterface::Execute_CanInteract(Use,P),*Use->GetActorLocation().ToString(),*Stand.ToString(),*Eye.ToString(),*Centre.ToString());}
   else UE_LOG(LogTemp,Warning,TEXT("Actual lower lounge entrance %s blocked%s"),*Use->AmenityID.ToString(),*CapsuleBlockers(W,P,Stand,Q));
  }
  else UE_LOG(LogTemp,Warning,TEXT("Lower entrance %s floor desired%s actual%s normal%.3f hit%s"),*Use->AmenityID.ToString(),*At.ToString(),*H.ImpactPoint.ToString(),H.ImpactNormal.Z,*GetNameSafe(H.GetActor()));
 }
 if(!Use->bUseSeatedPose && (Use->Kind==EIslandEstateAmenityKind::Wash || Use->Kind==EIslandEstateAmenityKind::Shower))
 {
  const FVector At=Use->GetActorTransform().TransformPosition(Use->StandLocal);FHitResult H;
  if(!W->LineTraceSingleByChannel(H,At+FVector(0,0,190),At-FVector(0,0,40),ECC_Pawn,Q) || H.ImpactNormal.Z<.7f || FMath::Abs(H.ImpactPoint.Z-At.Z)>15){UE_LOG(LogTemp,Warning,TEXT("Amenity approach %s floor desired%s actual%s normal%.3f hit%s"),*Use->AmenityID.ToString(),*At.ToString(),*H.ImpactPoint.ToString(),H.ImpactNormal.Z,*GetNameSafe(H.GetActor()));return false;}
  const FVector Stand=H.ImpactPoint+FVector(0,0,Half+3);if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q)){UE_LOG(LogTemp,Warning,TEXT("Amenity approach %s capsule%s radius%.1f half%.1f blocked%s"),*Use->AmenityID.ToString(),*Stand.ToString(),Radius,Half,*CapsuleBlockers(W,P,Stand,Q));return false;}
  const FRotator Facing=Use->GetActorTransform().TransformVectorNoScale(Use->FacingLocal).Rotation();P->SetActorLocationAndRotation(Stand,Facing,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((Centre-P->GetPawnViewLocation()).Rotation());
  const bool Reach=Use->CanReachFixture(P);auto* Focus=P->GetInteractionComponent()->RefreshFocusedInteractable();if(!Reach || Focus!=Use){UE_LOG(LogTemp,Warning,TEXT("Amenity approach %s reach%d focus%s stand%s right%s left%s"),*Use->AmenityID.ToString(),Reach,*GetNameSafe(Focus),*Stand.ToString(),*Use->GetActorTransform().TransformPosition(Use->RightHandLocal).ToString(),*Use->GetActorTransform().TransformPosition(Use->LeftHandLocal).ToString());for(const FName Bone:{FName(TEXT("spine_01")),FName(TEXT("upperarm_r")),FName(TEXT("lowerarm_r")),FName(TEXT("hand_r")),FName(TEXT("upperarm_l")),FName(TEXT("lowerarm_l")),FName(TEXT("hand_l"))})UE_LOG(LogTemp,Warning,TEXT("Amenity reach %s %s=%s radius%.1f half%.1f facing%s lean%.1f"),*Use->AmenityID.ToString(),*Bone.ToString(),*P->GetMesh()->GetSocketLocation(Bone).ToString(),Radius,Half,*P->GetActorForwardVector().ToString(),Use->WashLeanDegrees);}return Reach && Focus==Use;
 }
 for(const float Distance:{95.f,135.f,175.f})for(int32 I=0;I<12;++I)
 {
  const float Angle=I*PI/6.f;const FVector At=Centre+FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,0);FHitResult H;
  if(!W->LineTraceSingleByChannel(H,At+FVector(0,0,70),At-FVector(0,0,250),ECC_Pawn,Q) || H.ImpactNormal.Z<.7f)continue;
  if(Use->Kind!=EIslandEstateAmenityKind::Inspect && FMath::Abs(H.ImpactPoint.Z-Use->GetActorLocation().Z)>18){UE_LOG(LogTemp,Verbose,TEXT("Amenity approach %s floor candidate%d/%.0f delta%.1f hit%s"),*Use->AmenityID.ToString(),I,Distance,H.ImpactPoint.Z-Use->GetActorLocation().Z,*GetNameSafe(H.GetActor()));continue;}
  const FVector Stand=H.ImpactPoint+FVector(0,0,Half+3);
  if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q)){UE_LOG(LogTemp,Verbose,TEXT("Amenity approach %s capsule candidate%d/%.0f blocked%s"),*Use->AmenityID.ToString(),I,Distance,*CapsuleBlockers(W,P,Stand,Q));continue;}
  P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
  PC->SetControlRotation((Centre-P->GetPawnViewLocation()).Rotation());
  if(P->GetInteractionComponent()->RefreshFocusedInteractable()==Use)return true;
 }
 return false;
}
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,EIslandEstateAmenityKind K,bool Bindings=false):Test(T),Kind(K),bBindings(Bindings),Evidence(TEXT("EstateAmenity701")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>130)return End(TEXT("Estate furnishing input flow timed out"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
   TSet<FName> IDs;TSet<FName> Zones;
   for(TActorIterator<AIslandEstateAmenity> It(W);It;++It)
   {
    if(!It->HasPresentation() || IDs.Contains(It->AmenityID) || It->IsActorTickEnabled() || It->IsUsing())return End(TEXT("Amenity lacks a real fixture/props, stable unique ID or idle tick suspension"));
    IDs.Add(It->AmenityID);Uses.Add(*It);
   }
   for(TActorIterator<AIslandScenePanel> It(W);It;++It){if(It->ZoneID.IsNone() || Zones.Contains(It->ZoneID))return End(TEXT("Scene zone identity duplicates existing shared save state"));Zones.Add(It->ZoneID);}
   if(Uses.IsEmpty() || Zones.Num()>32)return End(TEXT("Native amenities absent or authored lighting exceeds the retained32 zone domain"));
   Uses.Sort([](const AIslandEstateAmenity& A,const AIslandEstateAmenity& B){return A.AmenityID.LexicalLess(B.AmenityID);});return Next(1);
  }
  if(Stage==1 && Age>.15)
  {
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {
    if(Stow.IsValid())return End(TEXT("Normal player weapon stow failed"));const int32 Slot=Weapon->GetData()->SlotType;const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(Slot<1 || Slot>8)return End(TEXT("Weapon slot cannot be stowed"));Stow=Slots[Slot-1];Key(Stow,true);return Next(10);
   }
   if(!bBindings && Kind==EIslandEstateAmenityKind::Meal && !MealLoaded)
   {
    FString Slot;FParse::Value(FCommandLine::Get(),TEXT("NeonCityEstateMealSlot="),Slot);Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
    if(Slot.IsEmpty() || !Save->ConfigureSlot(Slot,false) || !Save->DoesSaveExist())return End(TEXT("MealPIE requires -NeonCityEstateMealSlot=<retained genuine stored BBQ/cooktop meal checkpoint>; no food injection allowed"));Key(EKeys::F7,true);return Next(11);
   }
   return Next(bBindings?20:2);
  }
  if(Stage==10 && Age>.2){Key(Stow,false);return Next(1);}
  if(Stage==11 && Age>.7)
  {Key(EKeys::F7,false);auto* Food=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || Food->GetMeals().IsEmpty())return End(TEXT("Genuine meal checkpoint did not restore a stored cooked result"));MealLoaded=true;MealCountBefore=Food->GetMeals().Num();P->GetPlayerDataComponent()->SetStamina(82);return Next(1);}
  if(Stage==20)
  {
   if(Index>=Uses.Num())return End(Blocked.IsEmpty()?TEXT(""):TEXT("Actual full-capsule approach/F focus blocked: ")+FString::Join(Blocked,TEXT(", ")));Use=Uses[Index++];
   if(!Approach(W,P,PC,Use)){Blocked.Add(Use->AmenityID.ToString());UE_LOG(LogTemp,Warning,TEXT("Estate binding blocked: %s"),*Use->AmenityID.ToString());return false;}
   ++Approached;return false;
  }
  if(Stage==2)
  {
   for(auto* Candidate:Uses)if(Candidate->Kind==Kind && Approach(W,P,PC,Candidate)){Use=Candidate;break;}
   if(!Use)return End(TEXT("No actual supported F-focus candidate for requested adapter"));
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_EstateAmenity_")+FString::FromInt(int32(Kind))+TEXT("_")+FPaths::MakeValidFileName(Run.Left(25)),false) || Save->DoesSaveExist())return End(TEXT("Fresh retained furnishing checkpoint required"));
   HalfBefore=P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();CompletedBefore=Use->GetCompletedUses();MeshVisibleBefore=P->GetMesh()->IsVisible();Capture(TEXT("Prompt"));Key(EKeys::F,true);return Next(3);
  }
  if(Stage==3 && Age>.3)
  {
   Key(EKeys::F,false);if(!Use->IsUsing() || !Use->IsActorTickEnabled() || PC->GetPawn()!=P || (Use->bUseSeatedPose && !Use->IsOccupied()))return End(TEXT("Native F failed real fixture entry"));return Next(4);
  }
  if(Stage==4 && Age>1.4)
  {
   if(!Use->IsUsing() || !Use->IsFeedbackPlaying())return End(TEXT("Actual use lacks finite state/spatial audio"));
   if(Kind==EIslandEstateAmenityKind::Read)
   {if(!Use->HeldObject->IsVisible() || !Use->TaskBody->IsVisible())return End(TEXT("Read lacks real book and existing character task pose"));Key(EKeys::Right,true);return Next(5);}
   if(Kind==EIslandEstateAmenityKind::Drink)
   {if(!Use->HeldObject->IsVisible() || !Use->WaterSurface->IsVisible() || !Use->TaskBody->IsVisible())return End(TEXT("Drink lacks real cup/liquid/character presentation"));Capture(TEXT("Raise"));return Next(7);}
   if(Kind==EIslandEstateAmenityKind::Meal)
   {if(!Use->HeldObject->IsVisible() || !Use->WaterSurface->IsVisible() || !Use->TaskBody->IsVisible())return End(TEXT("Dining lacks actual cooked fish, serving plate and character"));Capture(TEXT("Meal"));return Next(7);}
   if(!Use->WaterStream->IsVisible() || Use->GetWaterFraction()<=0)return End(TEXT("Wet fixture has no visible running/filling water"));
   if(Kind==EIslandEstateAmenityKind::Wash){if(P->IsHidden() || PC->IsMoveInputIgnored() || !P->GetCharacterMovement()->IsMovingOnGround() || !Use->TaskBody->IsVisible())return End(TEXT("Standing wash hijacked normal movement or lacks hands"));for(int32 Side=0;Side<2;++Side){const FVector Actual=Use->TaskBody->GetBoneLocationByName(Side?TEXT("hand_l"):TEXT("hand_r"),EBoneSpaces::WorldSpace),Target=Use->GetActorTransform().TransformPosition(Side?Use->LeftHandLocal:Use->RightHandLocal);if(FVector::Dist(Actual,Target)>8)return End(TEXT("Actual wash hands do not reach the fitted water stream"));}Capture(TEXT("Wash"));Key(EKeys::W,true);return Next(6);}
   Capture(TEXT("Fill"));Key(EKeys::F6,true);return Next(8);
  }
  if(Stage==5 && Age>.2)
  {Key(EKeys::Right,false);if(Use->GetPageIndex()!=1 || Use->Details->Text.IsEmpty())return End(TEXT("Normal Right input failed actual readable page change"));Capture(TEXT("Page"));Key(EKeys::W,true);return Next(6);}
  if(Stage==6 && Age>.35){Key(EKeys::W,false);if(!Clean())return End(TEXT("Mapped movement cancel failed complete prop/pose/audio/input cleanup"));Capture(TEXT("Cancel"));return End(TEXT(""));}
  if(Stage==7)
  {
   if(Kind==EIslandEstateAmenityKind::Meal)
   {
    auto* Food=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
    if(Use->IsUsing()){if(Use->GetUseSeconds()>4.5f && Food->GetMeals().Num()!=MealCountBefore-1)return End(TEXT("Actual dining did not consume exactly one stored cooked meal"));return false;}
    if(Food->GetMeals().Num()!=MealCountBefore-1 || Use->GetCompletedUses()!=CompletedBefore+1 || P->GetPlayerDataComponent()->GetStamina()<99 || !Clean())return End(TEXT("Dining failed existing EatMeal effect, exactly-once inventory or cleanup"));Capture(TEXT("Meal-Finished"));return End(TEXT(""));
   }
   if(Use->IsUsing())
   {if(Use->GetUseSeconds()>6.5f && !Consumed){if(Use->GetDrinkFraction()>.2f)return End(TEXT("Actual liquid did not decrease while drinking"));Consumed=true;Capture(TEXT("Liquid-Decrease"));}return false;}
   if(!Consumed || Use->GetCompletedUses()!=CompletedBefore+1 || !Clean())return End(TEXT("Timed drink lacked consumption/completion/safe exit"));Capture(TEXT("Finished"));return End(TEXT(""));
  }
  if(Stage==8 && Age>.7)
  {
   Key(EKeys::F6,false);auto* Model=Save->ReadSaveForInspection();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model)return End(TEXT("Normal F6 failed standing-safe bath checkpoint"));
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(AmenitySavedFloor),false,P);Q.AddIgnoredActor(Use);const FVector AtSave=Model->Player.Transform.GetLocation();
   if(!W->LineTraceSingleByChannel(H,AtSave,AtSave-FVector(0,0,HalfBefore+12),ECC_Pawn,Q) || H.ImpactNormal.Z<.65f)return End(TEXT("Saved player exit lacks real supported floor"));Key(EKeys::F7,true);return Next(9);
  }
  if(Stage==9 && Age>.7){Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Clean())return End(TEXT("F7 failed fixture transient cleanup and normal walking restore"));Capture(TEXT("Load-Cleanup"));return End(TEXT(""));}
  return false;
 }
 bool Clean()const
 {
  return Use && !Use->IsUsing() && !Use->IsOccupied() && !Use->IsActorTickEnabled() && !Use->HeldObject->IsVisible() && !Use->WaterStream->IsVisible() && !Use->WaterSurface->IsVisible() && !Use->TaskBody->IsVisible() && !Use->IsFeedbackPlaying() && !P->IsHidden() && !P->GetAttachParentActor() && P->GetMesh()->IsVisible()==MeshVisibleBefore && !PC->IsMoveInputIgnored() && P->GetCharacterMovement()->IsMovingOnGround() && FMath::Abs(P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-HalfBefore)<.01f;
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 void Capture(const TCHAR* Label){FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Amenity701-%d-%s.png"),int32(Kind),Label));}
 bool End(const FString& Error)
 {
  if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("EstateAmenity701");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nstage=%d\nerror=%s\nfixture=%s\nauthored_amenities=%d\napproached=%d\nDeclared supported local start fixtures only. Binding mode checks every actual capsule/focus; adapter mode uses normal weapon stow/F/Right/W/F6/F7, actual body/props/water/audio/finite completion and cleanup. No ownership, elapsed, food, completion or saved-state injection. No route or subjective acceptance claim.\n"),Error.IsEmpty(),Stage,*Error,Use?*Use->AmenityID.ToString():TEXT("none"),Uses.Num(),Approached),*(Dir/(bBindings?TEXT("Bindings.txt"):FString::Printf(TEXT("Adapter%d.txt"),int32(Kind)))));
  if(Use && Use->IsUsing())Use->FinishUse();return true;
 }
 FAutomationTestBase* Test;EIslandEstateAmenityKind Kind;bool bBindings=false,Consumed=false,MeshVisibleBefore=true,MealLoaded=false;double Start=0;float At=0,HalfBefore=0;int32 Stage=0,Index=0,Approached=0,CompletedBefore=0,MealCountBefore=0;FKey Stow;TMap<FKey,bool> Keys;TArray<AIslandEstateAmenity*> Uses;
 TArray<FString> Blocked;
 UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandEstateAmenity* Use=nullptr;UNeonCitySaveSubsystem* Save=nullptr;FNeonCityVisualEvidence Evidence;
};
}
#define ESTATE_AMENITY_TEST(Class,Name,Kind,Bindings) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(EstateAmenity701::FFlow(this,EIslandEstateAmenityKind::Kind,Bindings));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
ESTATE_AMENITY_TEST(FEstateAmenityBindings,"NeonCity.Phase205c.Estate.Amenity.BindingsPIE",Read,true)
ESTATE_AMENITY_TEST(FEstateAmenityRead,"NeonCity.Phase205c.Estate.Amenity.ReadPIE",Read,false)
ESTATE_AMENITY_TEST(FEstateAmenityDrink,"NeonCity.Phase205c.Estate.Amenity.DrinkPIE",Drink,false)
ESTATE_AMENITY_TEST(FEstateAmenityWash,"NeonCity.Phase205c.Estate.Amenity.WashPIE",Wash,false)
ESTATE_AMENITY_TEST(FEstateAmenityBath,"NeonCity.Phase205c.Estate.Amenity.BathPIE",Bath,false)
ESTATE_AMENITY_TEST(FEstateAmenityMeal,"NeonCity.Phase205c.Estate.Amenity.MealPIE",Meal,false)
namespace EstateCooktop701
{
class FFlow:public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* T):Test(T),Evidence(TEXT("EstateCooktop701")){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>100)return End(TEXT("Minimal kitchen cooktop input flow timed out"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
   for(TActorIterator<AIslandEstateCooktop> It(W);It;++It)Hob=*It;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();Fishing=PC->FindComponentByClass<UFishingComponent>();
   if(!Hob || !IsValid(Hob->TargetCooktop) || !Hob->Pan->GetStaticMesh() || !Fishing)return End(TEXT("Actual retained kitchen hob, real pan or normal fishing input absent"));
   FString Slot=TEXT("NeonCity_Test_LandFinal_BoatCatch");FParse::Value(FCommandLine::Get(),TEXT("NeonCityEstateCatchSlot="),Slot);
   if(!Save->ConfigureSlot(Slot,false) || !Save->DoesSaveExist())return End(TEXT("Retained genuine raw BoatCatch checkpoint required; no synthetic food fixture"));Key(EKeys::F7,true);return Next(1);
  }
  if(Stage==1 && Age>.7)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Normal F7 failed genuine caught-fish restore"));
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {if(Stow.IsValid())return End(TEXT("Native weapon stow failed"));const int32 Slot=Weapon->GetData()?Weapon->GetData()->SlotType:0;const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(Slot<1 || Slot>8)return End(TEXT("Current weapon has no normal stow mapping"));Stow=Slots[Slot-1];Key(Stow,true);return Next(10);}
   for(const auto& F:Inventory->GetFish())if(F.Storage==ENeonCityFishStorage::Pouch && !F.FishingPointID.IsNone() && F.CaughtAt.GetTicks()>0){Ingredient=F;break;}
   if(!Ingredient.FishID.IsValid())return End(TEXT("Checkpoint contains no actual kept raw fish with catch provenance"));
   FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);OutputSlot=TEXT("NeonCity_Test_EstateCooktop_")+FPaths::MakeValidFileName(Run.Left(25));
   if(!Save->ConfigureSlot(OutputSlot,false) || Save->DoesSaveExist())return End(TEXT("Fresh isolated kitchen output slot required"));
   const FVector Ground=Hob->GetActorTransform().TransformPosition(Hob->GetWorkPositionLocal());FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CooktopDeclaredSupportedStart),false,P);
   if(!W->LineTraceSingleByChannel(H,Ground+FVector(0,0,150),Ground-FVector(0,0,100),ECC_Pawn,Q) || H.ImpactNormal.Z<.7f || FMath::Abs(H.ImpactPoint.Z-Ground.Z)>12)return End(TEXT("Actual +Y kitchen stance has no supported floor"));
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Actual kitchen stance blocks the normal full capsule:")+EstateAmenity701::CapsuleBlockers(W,P,Stand,Q));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(2);
  }
  if(Stage==10 && Age>.2){Key(Stow,false);return Next(1);}
  if(Stage==2 && Age>.4)
  {
   PC->SetControlRotation((Hob->InteractionBounds->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   return Next(11);
  }
  if(Stage==11 && Age>.25f)
  {
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Hob)
   {
    FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);FHitResult Hit;auto* Interaction=P->GetInteractionComponent();Interaction->DetectInteractable(Eye,Eye+View.Vector()*Interaction->InteractionDistance,Hit);
    return End(FString::Printf(TEXT("Kitchen F focus: hit%s/%s reach%d canStart%d grounded%d speed%.2f eye%s target%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hob->IsInReach(P),Fishing->CanStart(Hob),P->GetCharacterMovement()->IsMovingOnGround(),P->GetVelocity().Size2D(),*Eye.ToString(),*Hob->InteractionBounds->GetComponentLocation().ToString()));
   }
   Capture(TEXT("Prompt"));Key(EKeys::F,true);return Next(3);
  }
  if(Stage==3 && Age>.2){Key(EKeys::F,false);if(Fishing->GetActiveStation()!=Hob)return End(TEXT("Normal F did not open the actual cooktop"));MealsBefore=Inventory->GetMeals().Num();Key(EKeys::Enter,true);return Next(4);}
  if(Stage==4 && Age>.2){Key(EKeys::Enter,false);if(Hob->GetReservedFishID()!=Ingredient.FishID)return End(TEXT("Normal Enter did not reserve the exact first genuine pouch fish"));return Next(5);}
  if(Stage==5)
  {
   const auto S=Hob->GetCookingStage();const uint8 I=uint8(S);if(!Seen.Contains(I)){Seen.Add(I);Capture(*FString::Printf(TEXT("Stage%d"),I));}
   if(S!=ENeonCityGrillStage::Ready)
   {
    const auto* F=Inventory->FindFish(Ingredient.FishID);if(!F || F->Storage!=ENeonCityFishStorage::Grill || F->ContainerID!=Hob->StationID || Inventory->GetMeals().Num()!=MealsBefore)return End(TEXT("Kitchen reservation lost exact fish or created food before completion"));
    if(S==ENeonCityGrillStage::CookingFirst || S==ENeonCityGrillStage::CookingSecond)
    {
     Audio|=Hob->IsCookingAudioPlaying();Heat|=Hob->IsLit();FTransform R,L;Hands|=Hob->GetCookingHandTargets(R,L);
     if(!Hob->Food->IsVisible() || !Hob->Food->GetStaticMesh() || !Hob->Pan->IsVisible() || !Hob->Pan->GetStaticMesh() || FVector::Dist(Hob->Food->GetRelativeLocation(),FVector(0,0,94))>1)return End(TEXT("Real fish is not visibly cooking inside the actual measured pan"));
     TInlineComponentArray<UInstancedStaticMeshComponent*> Effects(Hob);for(const auto* Effect:Effects)Steam|=Effect->GetFName()==TEXT("CookingSteam") && Effect->GetInstanceCount()>0 && Effect->IsVisible();
     if(Hands){const float Right=FVector::Dist(P->GetMesh()->GetSocketLocation(TEXT("hand_r")),R.GetLocation()),Left=FVector::Dist(P->GetMesh()->GetSocketLocation(TEXT("hand_l")),L.GetLocation());MaxHand=FMath::Max(MaxHand,FMath::Max(Right,Left));}
    }
    if(Age>35)return End(TEXT("Finite native kitchen preparation did not reach plating"));return false;
   }
   for(uint8 K=1;K<=8;++K)if(!Seen.Contains(K))return End(TEXT("Actual ordered kitchen preparation stage was skipped"));
   if(!Audio || !Heat || !Hands || !Steam || MaxHand>18)return End(FString::Printf(TEXT("Actual kitchen audio/heat/hands/steam failed: audio%d heat%d hands%d steam%d hand%.2fcm"),Audio,Heat,Hands,Steam,MaxHand));
   const auto* Meal=Inventory->GetMeals().FindByPredicate([&](const auto& M){return M.SourceFishID==Ingredient.FishID;});
   if(Inventory->FindFish(Ingredient.FishID) || Inventory->GetMeals().Num()!=MealsBefore+1 || !Meal || Meal->SourceSizeCm!=Ingredient.SizeCm || Meal->SpeciesID!=Ingredient.SpeciesID)return End(TEXT("Kitchen did not produce exactly one genuine cooked result with source provenance"));
   MealID=Meal->MealID;Key(EKeys::One,true);return Next(6);
  }
  if(Stage==6 && Age>.2){Key(EKeys::One,false);if(Hob->GetCookingStage()!=ENeonCityGrillStage::Closing || !Inventory->GetMeals().ContainsByPredicate([&](const auto& M){return M.MealID==MealID;}))return End(TEXT("Normal Store did not retain exact cooked result and close tools"));return Next(7);}
  if(Stage==7){if(Hob->GetCookingStage()!=ENeonCityGrillStage::Idle)return false;Key(EKeys::F,true);return Next(8);}
  if(Stage==8 && Age>.2){Key(EKeys::F,false);if(Fishing->IsActive() || PC->IsMoveInputIgnored() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Leaving kitchen did not restore ordinary movement/input"));Key(EKeys::F6,true);return Next(9);}
  if(Stage==9 && Age>.7)
  {
   Key(EKeys::F6,false);const auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Model->HasFishing || !Model->Fishing.Meals.ContainsByPredicate([&](const auto& M){return M.MealID==MealID && M.SourceFishID==Ingredient.FishID;}))return End(TEXT("Normal F6 did not persist the actual stored cooked meal"));Capture(TEXT("Stored-Saved"));return End(TEXT(""));
  }
  return false;
 }
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Held=Keys.FindOrAdd(K);if(Held!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 void Capture(const TCHAR* L){FNeonCityVisualEvidence::CaptureHUD(W,FString(TEXT("Cooktop701-"))+L+TEXT(".png"));}
 bool End(const FString& Error)
 {
  if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const auto Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("EstateCooktop701");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nstage=%d\nerror=%s\nsource_fish=%s\nstored_meal=%s\noutput_slot=%s\nmax_hand_target_distance_cm=%.3f\nOne declared supported full-capsule kitchen local fixture. Native F7 raw BoatCatch restore, stow/F/Enter physical pan preparation, ordered stages, audio/heat/steam/real hand contact, exact source-fish result, Store/leave/F6. Zero catch/meal/time/completion injection. No route or subjective acceptance claim.\n"),Error.IsEmpty(),Stage,*Error,*Ingredient.FishID.ToString(),*MealID.ToString(),*OutputSlot,MaxHand),*(Dir/TEXT("scope.txt")));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandEstateCooktop* Hob=nullptr;UNeonCitySaveSubsystem* Save=nullptr;UNeonCityFishingSubsystem* Inventory=nullptr;UFishingComponent* Fishing=nullptr;
 FNeonCityCaughtFish Ingredient;FGuid MealID;FString OutputSlot;FKey Stow;TMap<FKey,bool> Keys;TSet<uint8> Seen;double Started=0;float At=0,MaxHand=0;int32 Stage=0,MealsBefore=0;bool Audio=false,Heat=false,Hands=false,Steam=false;FNeonCityVisualEvidence Evidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEstateCooktopPIE,"NeonCity.Phase205c.Estate.Amenity.CooktopPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEstateCooktopPIE::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(EstateCooktop701::FFlow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
