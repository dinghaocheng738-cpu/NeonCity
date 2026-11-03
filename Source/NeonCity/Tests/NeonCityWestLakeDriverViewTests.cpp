#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Adventure/IslandSupercar.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleInteraction.h"
#include "Vehicles/Marine/NeonCityRowboat.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Presentation/RowerPoseComponent.h"
#include "Player/NeonCityPlayerController.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Navigation/NeonCityMapData.h"
#include "Navigation/NeonCityRouteRibbon.h"

namespace NeonCityWestLakeDriverView
{
void Key(APlayerController* PC,FKey K){for(EInputEvent E:{IE_Pressed,IE_Released})PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
void Shot(const TCHAR* Name)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDriverViewCapture")))return;
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("WestLake/DriverView");IFileManager::Get().MakeDirectory(*Dir,true);
 FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);
}

/** First-person driving and rowing, and the route laid in the world:
 *  - on foot at home a waypoint to Jingci temple plans a route and glowing chevrons appear on the ground ahead;
 *  - in the garage car, V switches to the driver's eye: the cockpit camera is active at the driver's head inside
 *    the car, the driver's own head is hidden; V again returns to the chase view; the route follows in the car;
 *  - in the home rowboat, V puts the eye at the rower's head with that head hidden.
 * Stills with the HUD to Saved/WestLake/DriverView with -NeonCityDriverViewCapture. */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>200){Test->AddError(FString::Printf(TEXT("Driver view check timed out at stage %d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?Cast<ANeonCityPlayerController>(W->GetFirstPlayerController()):nullptr;
  if(!PC || !PC->GetPawn() || W->GetTimeSeconds()<4)return false;
  auto* Nav=PC->GetNavigationComponent();
  const double E=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   Player=Cast<ACharacter>(PC->GetPawn());if(!Player.IsValid() || !Player->GetCharacterMovement()->IsMovingOnGround())return false;
   const UNeonCityMapData* Map=Nav?Nav->GetMapData():nullptr;
   if(!Test->TestNotNull(TEXT("map"),Map))return true;
   const auto* Jingci=Map->Places.FindByPredicate([](const auto& X){return X.PlaceID==TEXT("jingci");});
   if(!Test->TestNotNull(TEXT("Jingci place"),Jingci))return true;
   // out of the front gate onto the Beishan Road sidewalk
   FHitResult G;const FVector Gate(3700,44500,0);
   if(W->LineTraceSingleByChannel(G,Gate+FVector(0,0,3000),Gate-FVector(0,0,3000),ECC_Visibility))Player->SetActorLocation(G.ImpactPoint+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
   Test->TestTrue(TEXT("waypoint accepted"),Nav->SetWaypoint(FVector(Jingci->Location,Player->GetActorLocation().Z),Jingci->Name,Jingci->PlaceID));
   return Next(W,1);
  }
  if(Stage==1)
  {
   auto* Ribbon=Nav->GetRouteRibbon();
   if((!Ribbon || Ribbon->GetChevronCount()<5) && E<15)return false;
   Test->TestTrue(FString::Printf(TEXT("route chevrons laid ahead on foot (%d)"),Ribbon?Ribbon->GetChevronCount():0),Ribbon && Ribbon->GetChevronCount()>=5);
   if(Ribbon)
   {
    // the first chevrons lie on the ground near the player, not floating or buried
    FTransform T;Ribbon->GetArm(0,T);
    for(int32 K=0;K<8;K+=2){FTransform A;Ribbon->GetArm(K,A);Test->AddInfo(FString::Printf(TEXT("arm %d at %s scale %s (player %s)"),K,*A.GetLocation().ToCompactString(),*A.GetScale3D().ToCompactString(),*Player->GetActorLocation().ToCompactString()));}
    const auto& R=Nav->GetDisplay().Route;for(int32 K=0;K<FMath::Min(5,R.Num());++K)Test->AddInfo(FString::Printf(TEXT("route %d %s"),K,*R[K].ToCompactString()));
    Test->TestTrue(FString::Printf(TEXT("first chevron near the player (%.0f m)"),FVector::Dist2D(T.GetLocation(),Player->GetActorLocation())/100),FVector::Dist2D(T.GetLocation(),Player->GetActorLocation())<1500);
   }
   // look along the route for the still
   if(Ribbon && Ribbon->GetChevronCount()>3){FTransform T3;Ribbon->GetArm(6,T3);
    PC->SetControlRotation(FRotator(-18,(T3.GetLocation()-Player->GetActorLocation()).Rotation().Yaw,0));}
   return Next(W,2);
  }
  if(Stage==2)
  {
   if(E<1)return false;
   if(!bShotA){bShotA=true;Shot(TEXT("A-RouteOnFoot.png"));return false;}
   if(E<1.5)return false;
   // the garage car: get in
   for(TActorIterator<AIslandSupercar> It(W);It;++It)if(It->bStartsOwned && FVector::Dist2D(It->GetActorLocation(),Player->GetActorLocation())<6000 && It->bRoadster)Car=*It;
   if(!Test->TestNotNull(TEXT("garage roadster"),Car.Get()))return true;
   Player->SetActorLocation(Car->GetActorLocation()+Car->GetActorRightVector()*-220+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
   return Next(W,3);
  }
  if(Stage==3)
  {
   if(E<1)return false;
   if(PC->GetPawn()!=Car.Get()){Car->GetVehicleInteraction()->Enter(Player.Get());if(E<6)return false;}
   if(!Test->TestTrue(TEXT("seated in the roadster"),PC->GetPawn()==Car.Get()))return true;
   Key(PC,EKeys::V);
   return Next(W,4);
  }
  if(Stage==4)
  {
   if(E<1.2)return false;
   Test->TestTrue(TEXT("V switches to the driver's eye"),Car->IsCockpitView() && Car->GetCockpitCamera()->IsActive());
   const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
   const FVector Local=Car->GetActorTransform().InverseTransformPosition(Eye);
   Test->TestTrue(FString::Printf(TEXT("eye inside the cabin at head height (local %s)"),*Local.ToCompactString()),FMath::Abs(Local.X)<150 && FMath::Abs(Local.Y)<80 && Local.Z>40 && Local.Z<160);
   if(auto* Seat=Car->FindComponentByClass<UTransportOccupantComponent>();Seat && Seat->GetDriverMesh())
    Test->TestTrue(TEXT("driver's own head hidden"),Seat->GetDriverMesh()->IsBoneHiddenByName(TEXT("head")));
   Test->TestTrue(TEXT("route still laid while driving"),Nav->GetRouteRibbon() && Nav->GetRouteRibbon()->GetChevronCount()>=3);
   Shot(TEXT("B-DriverView.png"));
   return Next(W,5);
  }
  if(Stage==5)
  {
   if(E<1)return false;
   Key(PC,EKeys::V);
   return Next(W,6);
  }
  if(Stage==6)
  {
   if(E<.6)return false;
   Test->TestTrue(TEXT("V again: chase view"),!Car->IsCockpitView() && Car->GetVehicleCamera()->IsActive() && !Car->GetCockpitCamera()->IsActive());
   if(auto* Seat=Car->FindComponentByClass<UTransportOccupantComponent>();Seat && Seat->GetDriverMesh())
    Test->TestFalse(TEXT("driver's head shown again in the chase view"),Seat->GetDriverMesh()->IsBoneHiddenByName(TEXT("head")));
   Shot(TEXT("C-ChaseView.png"));
   Car->GetVehicleInteraction()->Exit();Nav->ClearWaypoint();
   return Next(W,7);
  }
  if(Stage==7)
  {
   if(E<2 || !Cast<ACharacter>(PC->GetPawn())){if(E>10){Test->AddError(TEXT("never left the car"));return true;}return false;}
   for(TActorIterator<ANeonCityRowboat> It(W);It;++It)if(It->ActorHasTag(TEXT("WL.HomeBoat")))Boat=*It;
   if(!Test->TestNotNull(TEXT("home rowboat"),Boat.Get()))return true;
   // the jetty deck beside the boat
   ACharacter* P=Cast<ACharacter>(PC->GetPawn());FCollisionQueryParams Q(TEXT("DriverViewJetty"),false,P);Q.AddIgnoredActor(Boat.Get());
   for(float Side:{1.f,-1.f})
   {
    FHitResult H;const FVector Spot=Boat->GetActorTransform().TransformPosition(FVector(-60,Side*150,0));
    if(W->LineTraceSingleByChannel(H,Spot+FVector(0,0,300),Spot-FVector(0,0,200),ECC_Pawn,Q) && H.ImpactPoint.Z>Boat->GetActorLocation().Z+20)
    {P->SetActorLocation(H.ImpactPoint+FVector(0,0,95),false,nullptr,ETeleportType::TeleportPhysics);break;}
   }
   return Next(W,8);
  }
  if(Stage==8)
  {
   if(E<1)return false;
   ACharacter* P=Cast<ACharacter>(PC->GetPawn());
   if(Boat->GetPhase()==ANeonCityRowboat::EPhase::Idle && P && Boat->CanInteract_Implementation(P))Boat->Interact_Implementation(P);
   if(Boat->GetPhase()!=ANeonCityRowboat::EPhase::Seated){if(E>8){Test->AddError(TEXT("not seated in the rowboat"));return true;}return false;}
   Key(PC,EKeys::V);
   return Next(W,9);
  }
  if(Stage==9)
  {
   if(E<1.2)return false;
   Test->TestTrue(TEXT("V: rower's eye"),Boat->IsHelmCamera());
   Test->TestTrue(TEXT("rower's own head hidden"),Boat->GetRower()->IsBoneHiddenByName(TEXT("head")));
   Shot(TEXT("D-RowerView.png"));
   return Next(W,10);
  }
  return E>1.2;
 }
private:
 bool Next(UWorld* W,int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 FAutomationTestBase* Test;double Started=0,At=0;int32 Stage=0;bool bShotA=false;
 TWeakObjectPtr<ACharacter> Player;TWeakObjectPtr<AIslandSupercar> Car;TWeakObjectPtr<ANeonCityRowboat> Boat;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeDriverViewPIE,"NeonCity.WestLake.DriverViewPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeDriverViewPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeDriverView::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
