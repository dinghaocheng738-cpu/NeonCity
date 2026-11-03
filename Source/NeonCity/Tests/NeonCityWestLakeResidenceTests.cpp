#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Adventure/IslandLandResidence.h"
#include "Adventure/IslandGarageControl.h"
#include "Adventure/IslandGarageDoor.h"
#include "Property/PropertyComponent.h"
#include "Property/PropertySubsystem.h"
#include "Interaction/InteractableInterface.h"

namespace NeonCityWestLakeResidence
{
const FName Home(TEXT("Property.WestLake.Residence"));

/** The lakeside residence in normal PIE: the player starts at its arrival point, it is the owned home, its garage
 * door answers the door control, both garage bays hold a car, and the driveway down to Beishan Road is continuous
 * ground (no step a car could not climb). */
class FCheck:public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(TEXT("Residence check timed out"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;APlayerController* PC=W?W->GetFirstPlayerController():nullptr;
  APawn* P=PC?PC->GetPawn():nullptr;
  if(!P || W->GetTimeSeconds()<3)return false;
  if(Stage==0)
  {
   APlayerStart* Start=nullptr;int32 Starts=0;
   for(TActorIterator<APlayerStart> It(W);It;++It){Start=*It;++Starts;}
   Test->TestEqual(TEXT("exactly one PlayerStart"),Starts,1);
   if(Start)
   {
    const double D=FVector::Dist2D(P->GetActorLocation(),Start->GetActorLocation());
    Test->TestTrue(FString::Printf(TEXT("player starts at the residence arrival (%.0f cm away, start '%s')"),D,*Start->GetActorNameOrLabel()),D<300);
   }
   UPropertySubsystem* Props=W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<UPropertySubsystem>():nullptr;
   Test->TestNotNull(TEXT("property subsystem"),Props);
   if(Props)
   {
    Test->TestEqual(TEXT("home is the lakeside residence"),Props->GetHomePropertyID(),Home);
    Test->TestTrue(TEXT("the residence is owned"),Props->IsOwned(Home));
   }
   for(TActorIterator<AIslandLandResidence> It(W);It;++It)
    if(It->Property && It->Property->GetPropertyID()==Home)Residence=*It;
   Test->TestNotNull(TEXT("residence actor"),Residence.Get());
   for(TActorIterator<AIslandGarageControl> It(W);It;++It)
    if(It->PropertyID==Home && It->Action==EIslandGarageAction::Door && It->Door){DoorControl=*It;break;}
   Test->TestNotNull(TEXT("garage door control"),DoorControl.Get());
   if(DoorControl.IsValid())
   {
    // walk up to the control: stand on clear ground 1.2 m from it (the control answers within 2.4 m)
    const FVector C=DoorControl->GetActorLocation();const float Half=P->GetSimpleCollisionHalfHeight(),R=P->GetSimpleCollisionRadius();
    FCollisionQueryParams Q(TEXT("ResidenceControl"),false,P);bool bPlaced=false;
    for(int32 K=0;K<16 && !bPlaced;++K)
    {
     const FVector S=C+FVector(FMath::Cos(K*PI/8),FMath::Sin(K*PI/8),0)*(K<8?120.f:180.f);FHitResult H;
     if(!W->LineTraceSingleByChannel(H,S+FVector(0,0,150),S-FVector(0,0,300),ECC_Visibility,Q))continue;
     const FVector At=H.ImpactPoint+FVector(0,0,Half+3);
     if(W->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(R,Half),Q))continue;
     bPlaced=P->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);
    }
    Test->TestTrue(TEXT("found standing room at the garage door control"),bPlaced);
   }
   Stage=1;StageAt=FPlatformTime::Seconds();return false;
  }
  if(Stage==1)
  {
   if(FPlatformTime::Seconds()-StageAt<1.5)return false;                    // land on the ground first
   if(DoorControl.IsValid())
   {
    Before=DoorControl->Door->IsTargetOpen();
    Test->TestTrue(TEXT("the owner can use the door control"),IInteractableInterface::Execute_CanInteract(DoorControl.Get(),P));
    IInteractableInterface::Execute_Interact(DoorControl.Get(),P);
    Test->TestNotEqual(TEXT("door control toggles the door target"),DoorControl->Door->IsTargetOpen(),Before);
   }
   Stage=2;StageAt=FPlatformTime::Seconds();DoorAt=W->GetTimeSeconds();return false;
  }
  if(Stage==2)
  {
   // the door travels 4 s (its per-frame step is clamped, so at a low frame rate it takes longer): wait for it
   if(DoorControl.IsValid() && W->GetTimeSeconds()-DoorAt<10)
   {const float F=DoorControl->Door->GetOpenFraction();if(DoorControl->Door->IsTargetOpen()?F<=.95f:F>=.05f)return false;}
   if(DoorControl.IsValid())
   {
    const float F=DoorControl->Door->GetOpenFraction();
    Test->TestTrue(FString::Printf(TEXT("door moved to its target (open fraction %.2f, target %s%s%s)"),F,DoorControl->Door->IsTargetOpen()?TEXT("open"):TEXT("closed"),
     DoorControl->Door->IsObstructed()?TEXT(", stopped by "):TEXT(""),DoorControl->Door->IsObstructed()?*DoorControl->Door->GetLastBlocker():TEXT("")),
     DoorControl->Door->IsTargetOpen()?F>.95f:F<.05f);
    if(!DoorControl->Door->IsTargetOpen())IInteractableInterface::Execute_Interact(DoorControl.Get(),P);    // leave it open
   }
   Stage=3;StageAt=FPlatformTime::Seconds();return false;
  }
  if(Stage==3)
  {
   if(FPlatformTime::Seconds()-StageAt<5.5)return false;                    // fully open before tracing under it
   if(Residence.IsValid())
   {
    UPropertyComponent* C=Residence->Property;
    for(int32 I=0;I<C->GarageSlotsLocal.Num();++I)
    {
     const FVector Bay=C->GetGarageLocation(I);APawn* Car=nullptr;double Best=1e9;
     for(TActorIterator<APawn> It(W);It;++It)
      if(*It!=P){const double D=FVector::Dist2D(It->GetActorLocation(),Bay);if(D<Best){Best=D;Car=*It;}}
     Test->TestTrue(FString::Printf(TEXT("garage bay %d holds a car (%s, %.0f cm)"),I,Car?*Car->GetActorNameOrLabel():TEXT("none"),Best),Car && Best<250);
    }
    // the driveway: from between the bays out of the door to the Beishan Road kerb, ground every 1.5 m
    const FVector A=(C->GetGarageLocation(0)+C->GetGarageLocation(1))*.5,B(2940,43230,0);
    FCollisionQueryParams Q(TEXT("ResidenceDriveway"),true);Q.AddIgnoredActor(P);
    for(TActorIterator<APawn> It(W);It;++It)Q.AddIgnoredActor(*It);
    // a 15 cm sphere (a wheel, it does not fall into paving joints) swept down from 1.5 m above the previous ground
    // (under the garage ceiling, over any kerb a car could meet)
    double LastZ=A.Z,MaxStep=0;FVector At=A;int32 Misses=0;const int32 N=FMath::CeilToInt(FVector::Dist2D(A,B)/150.);
    for(int32 K=0;K<=N;++K)
    {
     FVector S=FMath::Lerp(A,B,double(K)/N);FHitResult H;
     if(!W->SweepSingleByChannel(H,FVector(S.X,S.Y,LastZ+150),FVector(S.X,S.Y,LastZ-600),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(15),Q)){++Misses;continue;}
     if(K>0 && FMath::Abs(H.ImpactPoint.Z-LastZ)>MaxStep){MaxStep=FMath::Abs(H.ImpactPoint.Z-LastZ);At=H.ImpactPoint;}
     LastZ=H.ImpactPoint.Z;
    }
    Test->TestEqual(TEXT("driveway has ground all the way"),Misses,0);
    Test->TestTrue(FString::Printf(TEXT("driveway has no step over 20 cm (max %.1f cm at %s)"),MaxStep,*At.ToCompactString()),MaxStep<20);
   }
   return true;
  }
  return true;
 }
private:
 FAutomationTestBase* Test;double Started=0,StageAt=0,DoorAt=0;int32 Stage=0;bool Before=false;
 TWeakObjectPtr<AIslandLandResidence> Residence;TWeakObjectPtr<AIslandGarageControl> DoorControl;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNeonCityWestLakeResidencePIE,"NeonCity.WestLake.ResidencePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNeonCityWestLakeResidencePIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(NeonCityWestLakeResidence::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
