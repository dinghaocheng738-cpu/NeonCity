#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Navigation/NeonCityRoadGuide.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Player/NeonCityPlayerController.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Phone/PhoneComponent.h"
#include "Vehicles/Aircraft/NeonCityHelicopterPrototype.h"
#include "Vehicles/Aircraft/AircraftInteractionComponent.h"
#include "VehicleCombat/VehicleCombatComponent.h"
#include "VehicleCombat/VehicleWeaponManager.h"
#include "VehicleCombat/VehicleMountedWeapon.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityRoadGuideTest,"NeonCity.Navigation.DirectedRoadGuide",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityRoadGuideTest::RunTest(const FString&)
{
 FNeonCityRoadGuide Guide;TArray<FVector> Route;
 TestFalse(TEXT("No authored road gives honest direct fallback"),Guide.FindRoute(FVector::ZeroVector,FVector(900,0,0),Route));
 FNeonCityGuideRoad Road;Road.Closed=true;
 Road.Points={FVector(-5000,-280,0),FVector(5000,-280,0),FVector(5500,0,0),FVector(5000,280,0),FVector(-5000,280,0),FVector(-5500,0,0)};
 TestTrue(TEXT("Authored lane loop accepted"),Guide.AddRoad(Road));
 TestTrue(TEXT("Westbound delivery uses the north lane"),Guide.FindRoute(FVector(4500,950,0),FVector(-4000,280,0),Route));
 TestTrue(TEXT("Includes actual parking origin and delivery goal"),Route.Num()>=3 && Route[0].Equals(FVector(4500,950,0)) && Route.Last().Equals(FVector(-4000,280,0)));
 for(const auto& P:Route)TestTrue(TEXT("Westbound route does not cross the opposite lane or ocean"),P.Y>=279);
 TestTrue(TEXT("Opposite lane requires the authored end turn"),Guide.FindRoute(FVector(0,-280,0),FVector(0,280,0),Route));
 TestTrue(TEXT("Guide follows the east turn instead of drawing across lanes"),Route.ContainsByPredicate([](const FVector& P){return P.X>5000;}));
 TestFalse(TEXT("Remote offshore goal cannot invent a road"),Guide.FindRoute(FVector::ZeroVector,FVector(0,-9000,0),Route));
 TestTrue(TEXT("Failed road query leaves no stale route"),Route.IsEmpty());
 Guide.Reset();Road.Closed=false;Road.Points={FVector::ZeroVector,FVector(1000,0,0)};Guide.AddRoad(Road);
 TestFalse(TEXT("One-way open road cannot guide backwards"),Guide.FindRoute(FVector(900,0,0),FVector(100,0,0),Route));
 FNeonCityGuideRoad Invalid;Invalid.Points={FVector::ZeroVector};TestFalse(TEXT("Incomplete road rejected"),Guide.AddRoad(Invalid));
 Invalid.Points.Add(FVector::ZeroVector);TestFalse(TEXT("Collapsed road rejected"),Guide.AddRoad(Invalid));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityDriveDestinationRevisionTest,"NeonCity.Navigation.DriveDestinationRevision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityDriveDestinationRevisionTest::RunTest(const FString&)
{
 UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
 AActor* Owner=World->SpawnActor<AActor>();
 auto* Nav=NewObject<UNeonCityNavigationComponent>(Owner);Nav->RegisterComponent();
 FNeonCityNavigationDestination D;D.OwnerID=TEXT("Home");D.DestinationID=TEXT("Garage");
 D.Label=FText::FromString(TEXT("Garage"));D.Location=FVector(1000,2000,100);D.UseRoadRoute=true;
 const uint64 Initial=Nav->GetDriveDestinationRevision();
 TestTrue(TEXT("Publisher sets a valid native destination"),Nav->SetDestination(D));
 uint64 Accepted=Nav->GetDriveDestinationRevision();
 TestTrue(TEXT("Initial drive destination creates a semantic revision"),Accepted>Initial);
 D.Label=FText::FromString(TEXT("Guest parking"));Nav->SetDestination(D);
 TestEqual(TEXT("Renaming the same drive goal does not cancel acceptance"),Nav->GetDriveDestinationRevision(),Accepted);
 const FVector StaticGoal=D.Location;D.Location=StaticGoal+FVector(.5,0,0);Nav->SetDestination(D);
 TestEqual(TEXT("Subcentimetre static jitter preserves acceptance"),Nav->GetDriveDestinationRevision(),Accepted);
 D.Location=StaticGoal+FVector(1,0,0);Nav->SetDestination(D);
 TestEqual(TEXT("One-centimetre tolerance remains relative to accepted goal"),Nav->GetDriveDestinationRevision(),Accepted);
 D.Location=StaticGoal+FVector(1.5,0,0);Nav->SetDestination(D);
 TestTrue(TEXT("Cumulative small static moves invalidate stale route acceptance"),Nav->GetDriveDestinationRevision()>Accepted);
 Accepted=Nav->GetDriveDestinationRevision();
 D.Location.X+=250;Nav->SetDestination(D);
 TestTrue(TEXT("Same-ID location change invalidates old drive acceptance"),Nav->GetDriveDestinationRevision()>Accepted);
 Accepted=Nav->GetDriveDestinationRevision();D.UseRoadRoute=false;Nav->SetDestination(D);
 TestTrue(TEXT("Same-ID road mode change invalidates old drive acceptance"),Nav->GetDriveDestinationRevision()>Accepted);
 Accepted=Nav->GetDriveDestinationRevision();AActor* Moving=World->SpawnActor<ATargetPoint>();
 Moving->SetActorLocation(D.Location);D.MovingTarget=Moving;Nav->SetDestination(D);
 TestTrue(TEXT("Changing the goal to an actor invalidates old acceptance"),Nav->GetDriveDestinationRevision()>Accepted);
 Accepted=Nav->GetDriveDestinationRevision();Moving->SetActorLocation(D.Location+FVector(50,0,0));
 TestEqual(TEXT("Small moving-target jitter does not repeatedly cancel cruise"),Nav->GetDriveDestinationRevision(),Accepted);
 Moving->SetActorLocation(D.Location+FVector(250,0,0));
 TestTrue(TEXT("A materially moved native target invalidates acceptance without republishing"),Nav->GetDriveDestinationRevision()>Accepted);
 Accepted=Nav->GetDriveDestinationRevision();Moving->Destroy();
 TestTrue(TEXT("A destroyed native goal invalidates acceptance immediately"),Nav->GetDriveDestinationRevision()>Accepted);
 Accepted=Nav->GetDriveDestinationRevision();Nav->ClearDestination(TEXT("Unrelated"));
 TestEqual(TEXT("An unrelated publisher cannot clear this goal"),Nav->GetDriveDestinationRevision(),Accepted);
 Nav->ClearDestination(D.OwnerID);
 TestTrue(TEXT("Clearing the owning destination invalidates acceptance"),Nav->GetDriveDestinationRevision()>Accepted);
 Owner->Destroy();return true;
}

namespace NeonCityNavigationTests
{
void Key(APlayerController* PC,FKey K,EInputEvent E)
{ PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64())); }

/** Focus regression uses an explicitly staged entry, not final player-route evidence. */
class FPhoneFocus : public IAutomationLatentCommand
{
public:
 explicit FPhoneFocus(FAutomationTestBase* T):Test(T) {}
 bool Next(int32 Value) { Stage=Value;At=W->GetTimeSeconds();return false; }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>40) { Test->AddError(FString::Printf(TEXT("Phone/vehicle focus timed out at %d"),Stage));return true; }
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || W->GetTimeSeconds()<3)return false;
  if(Stage==0)
  {
   auto* Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   for(TActorIterator<ANeonCityHelicopterPrototype> It(W);It;++It)Heli=*It;
   if(!Test->TestNotNull(TEXT("Normal-map aircraft for shared-input regression"),Heli))return true;
   Player->SetActorLocation(Heli->GetActorLocation()+FVector(0,-315,10),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();
   if(!Test->TestTrue(TEXT("Existing aircraft interaction enters"),Heli->GetAircraftInteraction()->Enter(Player)))return true;
   return Next(1);
  }
  const float E=W->GetTimeSeconds()-At;
  if(Stage==1 && E>.5f) { Key(PC,EKeys::P,IE_Pressed);return Next(2); }
  if(Stage==2 && E>.3f)
  {
   Key(PC,EKeys::P,IE_Released);Test->TestTrue(TEXT("Phone opens while possessed"),PC->GetPhoneComponent()->IsPhoneOpen());
   Key(PC,EKeys::V,IE_Pressed);return Next(3);
  }
  if(Stage==3 && E>.3f)
  {
   Key(PC,EKeys::V,IE_Released);
   Test->TestEqual(TEXT("V browses the actual phone app"),PC->GetPhoneComponent()->GetActiveApp(),ENeonCityPhoneApp::Vehicles);
   Test->TestFalse(TEXT("Browsing a phone must not arm the vehicle"),Heli->FindComponentByClass<UVehicleCombatComponent>()->IsArmed());
   Key(PC,EKeys::P,IE_Pressed);return Next(4);
  }
  if(Stage==4 && E>.3f) { Key(PC,EKeys::P,IE_Released);Key(PC,EKeys::V,IE_Pressed);return Next(5); }
  if(Stage==5 && E>.3f)
  {
   Key(PC,EKeys::V,IE_Released);
   Test->TestFalse(TEXT("Phone closed"),PC->GetPhoneComponent()->IsPhoneOpen());
   Test->TestTrue(TEXT("Closed-phone V still arms the vehicle"),Heli->FindComponentByClass<UVehicleCombatComponent>()->IsArmed());
   Key(PC,EKeys::P,IE_Pressed);return Next(6);
  }
  if(Stage==6 && E>.3f)
  {
   Key(PC,EKeys::P,IE_Released);
   Ammo=Heli->FindComponentByClass<UVehicleCombatComponent>()->GetManager()->GetWeapon()->GetAmmo();
   Key(PC,EKeys::LeftMouseButton,IE_Pressed);Key(PC,EKeys::RightMouseButton,IE_Pressed);return Next(7);
  }
  if(Stage==7 && E>.5f)
  {
   Key(PC,EKeys::LeftMouseButton,IE_Released);Key(PC,EKeys::RightMouseButton,IE_Released);
   auto* Combat=Heli->FindComponentByClass<UVehicleCombatComponent>();
   Test->TestEqual(TEXT("Reading phone suppresses armed fire"),Combat->GetManager()->GetWeapon()->GetAmmo(),Ammo);
   Test->TestFalse(TEXT("Reading phone suppresses target lock"),Combat->IsLockRequested());
   Key(PC,EKeys::P,IE_Pressed);return Next(8);
  }
  if(Stage==8 && E>.3f) { Key(PC,EKeys::P,IE_Released);Key(PC,EKeys::LeftMouseButton,IE_Pressed);return Next(9); }
  if(Stage==9 && E>.5f)
  {
   Key(PC,EKeys::LeftMouseButton,IE_Released);
   Test->TestTrue(TEXT("Closing phone restores mounted fire"),Heli->FindComponentByClass<UVehicleCombatComponent>()->GetManager()->GetWeapon()->GetAmmo()<Ammo);return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerController* PC=nullptr;ANeonCityHelicopterPrototype* Heli=nullptr;
 int32 Stage=0,Ammo=0;double Started=0;float At=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityPhoneFocusTest,"NeonCity.Navigation.PhoneVehicleFocusPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityPhoneFocusTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<NeonCityNavigationTests::FPhoneFocus>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
