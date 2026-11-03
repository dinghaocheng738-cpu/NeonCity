#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLandDoor.h"
#include "Adventure/IslandLandGrill.h"
#include "Adventure/IslandLandResidence.h"
#include "Adventure/IslandAdventureLighting.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/Marine/NeonCityReferenceVillage.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Fishing/FishingComponent.h"
#include "Fishing/NeonCityFishingSubsystem.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/Camera/NeonCityCameraExtensionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "InputKeyEventArgs.h"
namespace {
class FIslandLandAudit : public IAutomationLatentCommand
{
public:
 explicit FIslandLandAudit(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(FString::Printf(TEXT("Land audit timed out stage %d"),Stage));return true;}
  auto* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30580))))return false;
   auto* Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();Save->ConfigureSlot(TEXT("NeonCity_Test_IslandLand"));Save->DeleteSave();
   for(TActorIterator<AIslandLandDoor> It(W);It;++It)Doors.Add(*It);
   Test->TestEqual(TEXT("Three village doors and residence entry are authored"),Doors.Num(),4);
   if(Doors.Num()!=4)return true;
   for(TActorIterator<AIslandLandGrill> It(W);It;++It)if(!It->bIndoorCooktop)Grill=*It;
   int32 Homes=0;for(TActorIterator<AIslandLandResidence> It(W);It;++It){++Homes;Test->TestTrue(TEXT("Residence has physical property component"),bool(It->Property));}
   Test->TestEqual(TEXT("Only one hero residence added"),Homes,1);
   if(!Test->TestNotNull(TEXT("Terrace has native grill"),Grill))return true;
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)
   {
    const float OriginalHour=It->GetTimeOfDay();auto* Sun=It->GetSun();
    TArray<UDirectionalLightComponent*> Directions;It->GetComponents(Directions);
    auto** FoundMoon=Directions.FindByPredicate([](auto* C){return C->GetFName()==TEXT("Moon");});
    if(!Test->TestNotNull(TEXT("Night water and glass have the existing moon source"),FoundMoon?*FoundMoon:nullptr))return true;
    auto* Moon=*FoundMoon;
    for(float Hour:{12.f,17.f,22.f})
    {
     It->SetTimeOfDay(Hour);const bool SunDominant=Sun->Intensity>=Moon->Intensity;
     Test->TestEqual(TEXT("Day, sunset and night select one active forward light"),(SunDominant?Sun:Moon)->ForwardShadingPriority,1);
     Test->TestEqual(TEXT("Secondary directional light cannot compete for water or translucency"),(SunDominant?Moon:Sun)->ForwardShadingPriority,0);
     for(TActorIterator<AIslandAdventureLighting> Mountain(W);Mountain;++Mountain)
     {
      Mountain->Tick(0);auto* Adaptation=Mountain->FindComponentByClass<UPostProcessComponent>();
      if(!Test->TestNotNull(TEXT("Mountain retains its bounded exposure policy"),Adaptation))return true;
      const float Day=FMath::Max(0.f,FMath::Sin((Hour-6)*PI/12));
      Test->TestTrue(TEXT("Shaded road cuts can adapt during day and sunset without raising night"),FMath::IsNearlyEqual(Adaptation->Settings.AutoExposureMinBrightness,Day>.01f?9.f+FMath::Log2(Day):-1.5f,.001f));
      Test->TestEqual(TEXT("Mountain bright-sky exposure ceiling remains bounded"),Adaptation->Settings.AutoExposureMaxBrightness,16.f);
     }
    }
    It->SetTimeOfDay(OriginalHour);
    for(TActorIterator<AIslandAdventureLighting> Mountain(W);Mountain;++Mountain)Mountain->Tick(0);
   }
   // Measured native contacts complement the separate ordinary-player walk.
   // Probe the retained glazing and each newly completed terrace edge through
   // the pawn channel, including the actual saved collision fallback geometry.
   UStaticMeshComponent* Metal=nullptr;UStaticMeshComponent* Glass=nullptr;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
   {
    auto* C=It->GetStaticMeshComponent();if(!C || !C->GetStaticMesh())continue;
    const FName Name=C->GetStaticMesh()->GetFName();
    if(Name==TEXT("SM_LandMansionMetal"))Metal=C;
    if(Name==TEXT("SM_LandMansionGlass"))Glass=C;
   }
   if(!Test->TestNotNull(TEXT("Mansion has the exact railing mesh"),Metal) || !Test->TestNotNull(TEXT("Mansion has the retained glazing"),Glass))return true;
   FCollisionQueryParams RailQuery(SCENE_QUERY_STAT(LandRailContact),true);
   for(const FVector RailAt:{NeonCityCoastalLayout::FromLegacyIsland(FVector(47850,-120100,30542)),NeonCityCoastalLayout::FromLegacyIsland(FVector(50250,-120100,30542)),NeonCityCoastalLayout::FromLegacyIsland(FVector(49000,-120692.5,30542))})
   {
    const FVector Across=RailAt.Y<NeonCityCoastalLayout::FromLegacyIsland(FVector(0,-120600,0)).Y?FVector(0,100,0):FVector(100,0,0);FHitResult Contact;
    if(!Test->TestTrue(TEXT("Visible terrace middle rail supplies a real pawn contact"),Metal->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && Metal->LineTraceComponent(Contact,RailAt-Across,RailAt+Across,RailQuery)))return true;
   }
   FHitResult PaneContact;
   if(!Test->TestTrue(TEXT("Sea-view glass stops the player instead of allowing a fall"),Glass->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && Glass->LineTraceComponent(PaneContact,NeonCityCoastalLayout::FromLegacyIsland(FVector(49300,-119700,30600)),NeonCityCoastalLayout::FromLegacyIsland(FVector(49300,-119900,30600)),RailQuery)))return true;
   for(TActorIterator<ANeonCityReferenceVillage> It(W);It;++It)
   {
    Test->TestEqual(TEXT("Exactly three sealed house contacts opened"),It->OpenBuildingContacts.Num(),3);
    Test->TestTrue(TEXT("All opaque village wall skins have real doorway derivatives"),It->LandSurfaceOverrides.Contains(0) && It->LandSurfaceOverrides.Contains(1) && It->LandSurfaceOverrides.Contains(4));
    for(int32 I:It->OpenBuildingContacts){TArray<UBoxComponent*> Boxes;It->GetComponents(Boxes);for(auto* B:Boxes)if(B->GetName()==FString::Printf(TEXT("VillageContact_%d"),I))Test->TestEqual(TEXT("Whole-house blocker disabled"),B->GetCollisionEnabled(),ECollisionEnabled::NoCollision);}
   }
   for(auto* D:Doors)
   {
    Test->TestTrue(TEXT("Each door has finished mesh and blocking physics"),D->Leaf->GetStaticMesh() && D->Leaf->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
    Test->TestTrue(TEXT("Each authored door has distinct surveyed position"),FVector::Dist(D->GetActorLocation(),NeonCityCoastalLayout::FromLegacyIsland(FVector(50510,-169470,340)))>500);
   }
   // This is an automated isolated fixture setup, never manual route evidence.
   OutdoorArmLength=P->GetCameraBoom()->TargetArmLength;
   P->SetActorLocation(Doors[0]->GetActorTransform().TransformPosition(FVector(65,-190,100)),false,nullptr,ETeleportType::TeleportPhysics);
   At=W->GetTimeSeconds();Stage=6;return false;
  }
  if(Stage==6)
  {
   if(W->GetTimeSeconds()-At<.6f || !P->GetCharacterMovement()->IsMovingOnGround())return false;
   auto* D=Doors[DoorIndex];const FVector Target=D->GetActorTransform().TransformPosition(FVector(65,0,110));
   PC->SetControlRotation((Target-P->GetPawnViewLocation()).Rotation());
   if(!Test->TestEqual(TEXT("Ordinary eye trace focuses each finished doorway"),P->GetInteractionComponent()->RefreshFocusedInteractable(),static_cast<AActor*>(D)))return true;
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Pressed,FPlatformTime::Cycles64()));
   At=W->GetTimeSeconds();Stage=7;return false;
  }
  if(Stage==7)
  {
   if(W->GetTimeSeconds()-At<.2f)return false;
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,IE_Released,FPlatformTime::Cycles64()));
   if(W->GetTimeSeconds()-At<2.f)return false;
   if(!Test->TestTrue(TEXT("F input opens each physical door"),FMath::Abs(Doors[DoorIndex]->Leaf->GetRelativeRotation().Yaw)>90))return true;
   auto* D=Doors[DoorIndex];
   // Inspect the visible wall skins too. Their normal render-only collision
   // policy must not conceal an opaque surface across the physical opening.
   if(D->GetActorLocation().Z<1000)
    for(TActorIterator<ANeonCityReferenceVillage> It(W);It;++It)
    {
     TArray<UStaticMeshComponent*> Surfaces;It->GetComponents(Surfaces);
     for(auto* C:Surfaces)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_LandVillageOpen")))
     {
      const auto Previous=C->GetCollisionEnabled();C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
      // Include the front porch: a wall-only ray previously missed a central
      // shop support standing in front of the otherwise correctly open door.
      FHitResult Hit;const bool Blocked=C->LineTraceComponent(Hit,D->GetActorTransform().TransformPosition(FVector(65,-240,110)),D->GetActorTransform().TransformPosition(FVector(65,40,110)),FCollisionQueryParams(SCENE_QUERY_STAT(LandVisibleDoorway),true));
      C->SetCollisionEnabled(Previous);
      if(!Test->TestFalse(TEXT("Opened doorway has no original opaque wall skin"),Blocked))return true;
     }
    }
   PC->SetControlRotation(FRotator(-5,D->GetActorRotation().Yaw+90,0));
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Pressed,FPlatformTime::Cycles64()));
   MovementReleased=false;At=W->GetTimeSeconds();Stage=8;return false;
  }
  if(Stage==8)
  {
   if(W->GetTimeSeconds()-At<.8f)return false;
   if(!MovementReleased){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Released,FPlatformTime::Cycles64()));MovementReleased=true;return false;}
   if(W->GetTimeSeconds()-At<1.2f)return false;
   auto* D=Doors[DoorIndex];
   UE_LOG(LogTemp,Display,TEXT("Land doorway entry %s local=%s velocity=%s"),*D->GetName(),*D->GetActorTransform().InverseTransformPosition(P->GetActorLocation()).ToString(),*P->GetVelocity().ToString());
   if(!Test->TestTrue(TEXT("Normal W input walks into each actual room"),D->GetActorTransform().InverseTransformPosition(P->GetActorLocation()).Y>65 && P->GetCharacterMovement()->IsMovingOnGround()))return true;
   const float ExpectedRoomLimit=D->GetActorLocation().Z<1000?190.f:290.f;
   if(!Test->TestTrue(TEXT("Room limits the ordinary orbit camera"),P->GetCameraBoom()->TargetArmLength<ExpectedRoomLimit))return true;
   Test->TestEqual(TEXT("Indoor adaptation leaves mature camera contexts available"),P->GetCameraExtensionComponent()->GetCameraContext(),ENeonCityCameraContext::Default);
   PC->SetControlRotation(FRotator(-5,D->GetActorRotation().Yaw-90,0));
   At=W->GetTimeSeconds();Stage=10;return false;
  }
  if(Stage==10)
  {
   if(W->GetTimeSeconds()-At<.4f)return false;
   if(FVector::Dist(P->GetFollowCamera()->GetComponentLocation(),P->GetActorLocation())<120)
    Test->TestTrue(TEXT("Retracted indoor camera does not render the owner's body"),P->GetMesh()->bOwnerNoSee);
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Pressed,FPlatformTime::Cycles64()));
   MovementReleased=false;At=W->GetTimeSeconds();Stage=9;return false;
  }
  if(Stage==9)
  {
   if(W->GetTimeSeconds()-At<.8f)return false;
   if(!MovementReleased){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Released,FPlatformTime::Cycles64()));MovementReleased=true;return false;}
   if(W->GetTimeSeconds()-At<1.2f)return false;
   auto* D=Doors[DoorIndex];
   UE_LOG(LogTemp,Display,TEXT("Land doorway exit %s local=%s velocity=%s control=%s"),*D->GetName(),*D->GetActorTransform().InverseTransformPosition(P->GetActorLocation()).ToString(),*P->GetVelocity().ToString(),*PC->GetControlRotation().ToString());
   if(!Test->TestTrue(TEXT("Normal W input exits through each actual doorway"),D->GetActorTransform().InverseTransformPosition(P->GetActorLocation()).Y<-65))
   {
    FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(LandExitBlocker),true,P);
    W->SweepSingleByChannel(Hit,P->GetActorLocation(),D->GetActorTransform().TransformPosition(FVector(65,-190,100)),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(42,92),Q);
    UE_LOG(LogTemp,Display,TEXT("Land exit blocker actor=%s component=%s point=%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString());
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Island-Land-Door-ExitBlocked.png"));return true;
   }
   if(++DoorIndex<Doors.Num())
   {
    Test->TestEqual(TEXT("Exiting restores the original outdoor orbit"),P->GetCameraBoom()->TargetArmLength,OutdoorArmLength);
    Test->TestFalse(TEXT("Exiting restores visible player body"),P->GetMesh()->bOwnerNoSee);
    P->GetCharacterMovement()->StopMovementImmediately();
    P->SetActorLocation(Doors[DoorIndex]->GetActorTransform().TransformPosition(FVector(65,-190,100)),false,nullptr,ETeleportType::TeleportPhysics);
    At=W->GetTimeSeconds();Stage=6;return false;
   }
   Stage=1;
  }
  if(Stage==1)
  {
   // Valid test catch exercises existing inventory transactions. It is not
   // represented as a manually caught fish or a completed gameplay tour.
   auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();FRandomStream Random(20505);
   Catch=Inventory->SampleCatch(Random,TEXT("Coastal"),10,TEXT("Clear"),TEXT("Test.IslandLand"));
   Test->TestTrue(TEXT("Valid fixture catch accepted"),Inventory->StoreCatch(Catch));MealsBefore=Inventory->GetMeals().Num();
   P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(Grill->GetActorLocation()+FVector(0,-170,100),false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation(FRotator(-8,90,0));At=W->GetTimeSeconds();Stage=2;return false;
  }
  if(Stage==2)
  {
   if(W->GetTimeSeconds()-At<3 || !P->GetCharacterMovement()->IsMovingOnGround())return false;
   PC->SetControlRotation((Grill->GetActorLocation()+FVector(0,-18,95)-P->GetPawnViewLocation()).Rotation());
   if(!Test->TestTrue(TEXT("Terrace grill reachable through physical sightline"),Grill->CanInteract_Implementation(P)))return true;
   Grill->Interact_Implementation(P);Test->TestTrue(TEXT("First interaction ignites actual grill"),Grill->IsLit());
   Grill->Interact_Implementation(P);auto* Cooking=PC->FindComponentByClass<UFishingComponent>();
   if(!Test->TestTrue(TEXT("Second interaction opens existing fish menu"),Cooking && Cooking->GetActiveStation()==Grill))return true;
   // AddMappingContext rebuilds on the next input pass. Confirm only after the
   // same visible menu a normal player sees has taken ownership of input.
   At=W->GetTimeSeconds();Stage=5;return false;
  }
  if(Stage==5)
  {
   if(W->GetTimeSeconds()-At<.3f)return false;
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Enter,IE_Pressed,FPlatformTime::Cycles64()));
   At=W->GetTimeSeconds();Stage=3;return false;
  }
  if(Stage==3)
  {
   if(W->GetTimeSeconds()-At<.5)return false;
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Enter,IE_Released,FPlatformTime::Cycles64()));
   if(!Test->TestTrue(TEXT("Native cooking timer drives grill feedback"),Grill->IsCooking()))return true;
   Test->TestTrue(TEXT("Real fish cooking starts spatial sizzle audio"),Grill->IsCookingAudioPlaying());
   At=W->GetTimeSeconds();Stage=4;return false;
  }
  if(Stage==4)
  {
   if(W->GetTimeSeconds()-At<5)return false;
   auto* Inventory=W->GetGameInstance()->GetSubsystem<UNeonCityFishingSubsystem>();
   Test->TestFalse(TEXT("Sizzle stops after cooking finishes"),Grill->IsCookingAudioPlaying());
   if(!Test->TestEqual(TEXT("Cooking produces one finished meal"),Inventory->GetMeals().Num(),MealsBefore+1))return true;
   Test->TestNull(TEXT("Cooked catch removed exactly once"),Inventory->FindFish(Catch.FishID));
   Test->TestFalse(TEXT("Same catch cannot be consumed twice"),Inventory->CookFish(Catch.FishID));
   const FGuid Meal=Inventory->GetMeals().Last().MealID;
   Test->TestFalse(TEXT("Full player cannot waste the finished meal"),Inventory->EatMeal(Meal,P));
   P->GetPlayerDataComponent()->SetStamina(40);
   Test->TestTrue(TEXT("Finished terrace meal can be eaten"),Inventory->EatMeal(Meal,P));
   Test->TestEqual(TEXT("Meal restores native bounded stamina"),P->GetPlayerDataComponent()->GetStamina(),60.f);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Island-Land-Grill-Transaction.png"));
   return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;TArray<AIslandLandDoor*> Doors;AIslandLandGrill* Grill=nullptr;
 FNeonCityCaughtFish Catch;double Started=0;float At=0,OutdoorArmLength=400;int32 Stage=0,MealsBefore=0,DoorIndex=0;bool MovementReleased=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIslandLandPassPIE,"NeonCity.IslandLand.Pass1PIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIslandLandPassPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FIslandLandAudit(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
