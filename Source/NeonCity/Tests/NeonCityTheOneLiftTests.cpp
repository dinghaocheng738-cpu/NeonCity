#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandResidenceLift.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"

namespace TheOneLiftCheck
{
class FFourStops : public IAutomationLatentCommand
{
public:
 explicit FFourStops(FAutomationTestBase* In,bool RoofOnly=false,bool NativeEstate=false):Test(In),bRoofOnly(RoofOnly),bNativeEstate(NativeEstate){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>200)return End(TEXT("Four-stop lift fix-specific check timed out"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(bNativeEstate?NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254)):NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30480))))return false;
   AIslandResidenceLift* Source=nullptr;
   for(TActorIterator<AIslandResidenceLift> It(W);It;++It)if(It->Floor->GetStaticMesh()){Source=*It;break;}
   if(!Source)return End(TEXT("Existing authored lift meshes required; no replacement lift system"));
   if(bNativeEstate)
   {
    for(TActorIterator<AIslandResidenceLift> It(W);It;++It)
     if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.TheOne478"))){Lift=*It;break;}
    if(!Lift || Lift->GetLandingCount()!=4 || FMath::Abs(Lift->GetLandingHeightCm(3)-1204)>.1)
     return End(TEXT("Actual The One four-stop authoring required"));
    Test->TestEqual(TEXT("Actual estate retired fifth panel cannot block"),Lift->CallPanels[4]->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
    if(!Save->ConfigureSlot(TEXT("NeonCity_Test_TheOneEstate480"),false) || Save->DoesSaveExist())return End(TEXT("Unique estate save slot must be empty"));
    const FVector Landing=Lift->GetActorTransform().TransformPosition(FVector(225,0,0));
    FHitResult Ground;FCollisionQueryParams Query;Query.AddIgnoredActor(P);
    if(!W->LineTraceSingleByChannel(Ground,Landing+FVector(0,0,80),Landing-FVector(0,0,80),ECC_Pawn,Query) || FMath::Abs(Ground.ImpactPoint.Z-Landing.Z)>3)
     return End(TEXT("Actual garage landing lacks physical floor support"));
    P->SetActorLocation(Landing+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
    P->GetCharacterMovement()->StopMovementImmediately();return Next(26);
   }
   Lift=W->SpawnActorDeferred<AIslandResidenceLift>(AIslandResidenceLift::StaticClass(),FTransform(NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,65000))));
   // Validate malformed authoring without allowing an invisible / phantom stop.
   Lift->LandingHeightsCm={0,304.8f,762,1204};
   Lift->LandingNames={FText::FromString(TEXT("Garage")),FText::FromString(TEXT("Main")),FText::FromString(TEXT("Suites")),FText::FromString(TEXT("Roof"))};
   Copy(Lift->Floor,Source->Floor);Copy(Lift->Interior,Source->Interior);Copy(Lift->Glazing,Source->Glazing);Copy(Lift->ControlPanelFace,Source->ControlPanelFace);
   for(int32 I=0;I<2;++I)Copy(Lift->CabinDoors[I],Source->CabinDoors[I]);
   for(int32 I=0;I<10;++I)Copy(Lift->LandingDoors[I],Source->LandingDoors[I]);
   Lift->FinishSpawning(Lift->GetActorTransform());
   if(!Test->TestEqual(TEXT("Explicit floor profile has four stops"),Lift->GetLandingCount(),4))return End(TEXT("Wrong landing count"));
   Test->TestEqual(TEXT("Inactive fifth call panel has no collision"),Lift->CallPanels[4]->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
   Test->TestTrue(TEXT("Inactive fifth door is hidden and cannot block the roof"),!Lift->LandingDoors[8]->IsVisible() && Lift->LandingDoors[8]->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Lift->LandingHeightsCm={0,100,762,1204};
   Lift->OnConstruction(Lift->GetActorTransform());
   Test->TestEqual(TEXT("Invalid nonempty profile cannot create a phantom fifth landing"),Lift->GetLandingCount(),0);
   Test->TestFalse(TEXT("Invalid floor data disables operation"),Lift->CanInteract_Implementation(P));
   for(const auto& Panel:Lift->CallPanels)Test->TestEqual(TEXT("Invalid profile disables every call panel"),Panel->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
   Test->TestTrue(TEXT("Invalid profile does not reactivate the retired fifth door"),!Lift->LandingDoors[8]->IsVisible() && Lift->LandingDoors[8]->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Lift->LandingHeightsCm={0,304.8f,762,1204};
   Lift->OnConstruction(Lift->GetActorTransform());
   auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
   for(int32 I=0;I<4;++I)
   {
    auto* B=W->SpawnActor<AStaticMeshActor>(Lift->GetActorLocation()+FVector(255,0,Lift->GetLandingHeightCm(I)-10),FRotator::ZeroRotator);
    B->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    B->GetStaticMeshComponent()->SetStaticMesh(Cube);B->SetActorScale3D(FVector(2.7,2.4,.2));
    B->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));Fixtures.Add(B);
    B->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
    FHitResult Ground;const FVector Probe=B->GetActorLocation()+FVector(0,0,50);
    if(!W->LineTraceSingleByChannel(Ground,Probe,Probe-FVector(0,0,100),ECC_Pawn) ||
       Ground.GetActor()!=B || FMath::Abs(Ground.ImpactPoint.Z-Lift->GetActorLocation().Z-Lift->GetLandingHeightCm(I))>.1)
      return End(TEXT("Test fixture landing mesh must be physically present at its actual height"));
   }
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Save->ConfigureSlot(bRoofOnly?TEXT("NeonCity_Test_TheOneLiftRoof462"):TEXT("NeonCity_Test_TheOneLift450"),false) || Save->DoesSaveExist())return End(TEXT("Unique source save slot must remain empty"));
   const FVector Fixture=bRoofOnly?Lift->GetActorLocation()+FVector(225,0,1204):Lift->Cabin->GetComponentLocation();
   P->SetActorLocation(Fixture+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();return Next(bRoofOnly?20:1);
  }
  Evidence.Record(W,PC,Stage);
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==26)
  {
   const FVector D=Lift->Cabin->GetComponentLocation()-P->GetActorLocation();
   if(D.Size2D()<25 && P->GetMovementBaseObject()==Lift->Floor){Key(EKeys::W,IE_Released);return Next(1);}
   if(Age>8)return End(TEXT("Actual estate garage-to-cabin normal walking entry blocked"));
   PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Key(EKeys::W,IE_Pressed);
  }
  if(Stage==20 && Age>.7)
  {
   PC->SetControlRotation((Lift->CallPanels[3]->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return End(TEXT("Normal roof call prompt missing"));
   Key(EKeys::F,IE_Pressed);return Next(21);
  }
  if(Stage==21 && Age>.15){Key(EKeys::F,IE_Released);return Next(22);}
  if(Stage==22)
  {
   if(Age>25)return End(TEXT("Roof-only call did not reach1204cm landing"));
   if(Lift->GetCurrentFloor()==3 && Lift->AreDoorsOpen())return Next(23);
  }
  if(Stage==23)
  {
   const FVector D=Lift->Cabin->GetComponentLocation()-P->GetActorLocation();
   if(D.Size2D()<25)
   {
    Key(EKeys::W,IE_Released);
    if(P->GetMovementBaseObject()!=Lift->Floor)return false;
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Lift-RoofSave462.png"));
    Key(EKeys::F6,IE_Pressed);return Next(4);
   }
   if(Age>8)return End(TEXT("Normal roof-to-cabin entry blocked"));
   PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Key(EKeys::W,IE_Pressed);
  }
  if(Stage==1 && Age>.7)
  {
   if(P->GetMovementBaseObject()!=Lift->Floor)return End(TEXT("Real cabin movement base missing"));
   Target=(Lift->GetCurrentFloor()+1)%Lift->GetLandingCount();
   PC->SetControlRotation((Lift->Cabin->GetComponentTransform().TransformPosition(FVector(117,103,140))-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Lift)return End(TEXT("Normal F cabin prompt missing"));
   if(!Lift->GetInteractionName_Implementation().ToString().Contains(Target==0?TEXT("Garage"):Target==1?TEXT("Main"):Target==2?TEXT("Suites"):TEXT("Roof")))return End(TEXT("Wrong next-floor UI name"));
   Key(EKeys::F,IE_Pressed);return Next(2);
  }
  if(Stage==2 && Age>.15){Key(EKeys::F,IE_Released);return Next(3);}
  if(Stage==3)
  {
   SupportError=FMath::Max(SupportError,FMath::Abs(P->GetActorLocation().Z-Lift->Cabin->GetComponentLocation().Z-P->GetSimpleCollisionHalfHeight()));
   if(SupportError>20)return End(TEXT("Rider lost physical moving-base support"));
   if(Lift->GetCurrentFloor()==Target && Lift->AreDoorsOpen())
   {
    if(FMath::Abs(Lift->Cabin->GetRelativeLocation().Z-Lift->GetLandingHeightCm(Target))>.1)return End(TEXT("Cabin stopped at equal-spacing height instead of configured height"));
    ++Rides;
    if(Target==3){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Lift-Roof1204.png"));Key(EKeys::F6,IE_Pressed);return Next(4);}
    if(Target==0)return End(Rides==4?TEXT(""):TEXT("Incomplete four-stop circuit"));
    return Next(1);
   }
   if(Age>28)return End(TEXT("Nonuniform landing could not be reached"));
  }
  if(Stage==4 && Age>.6)
  {
   Key(EKeys::F6,IE_Released);auto* Model=Save->ReadSaveForInspection();
   if(!Model || Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Configured roof landing save failed"));
   const FVector Local=Lift->GetActorTransform().InverseTransformPosition(Model->Player.Transform.GetLocation());
   if(Local.X<200 || Local.X>255 || FMath::Abs(Local.Y)>5 || FMath::Abs(Local.Z-1204-P->GetSimpleCollisionHalfHeight()-3)>5)return End(TEXT("Save checkpoint used retired equal-spacing roof height"));
   RoofSave=true;return bRoofOnly?End(TEXT("")):Next(1);
  }
  return false;
 }
private:
 void Copy(UStaticMeshComponent* To,UStaticMeshComponent* From)
 {To->SetStaticMesh(From->GetStaticMesh());To->SetRelativeTransform(From->GetRelativeTransform());for(int32 I=0;I<From->GetNumMaterials();++I)To->SetMaterial(I,From->GetMaterial(I));}
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 bool End(const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);
  Key(EKeys::W,IE_Released);Key(EKeys::F,IE_Released);Key(EKeys::F6,IE_Released);
  if(Failure.IsEmpty())Test->TestTrue(TEXT("Required native ride / roof save completed"),(bRoofOnly || Rides==4) && RoofSave && SupportError<20);
  UE_LOG(LogTemp,Display,TEXT("TheOneLift450 result=%s rides=%d roof_save=%d support_error_cm=%.3f scope=%s"),Failure.IsEmpty()?TEXT("PASS"):TEXT("FAIL"),Rides,RoofSave,SupportError,bNativeEstate?TEXT("actual The One authored architecture; normal walking entry and four rides"):TEXT("isolated native lift fixture; mansion access remains pending"));
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 AIslandResidenceLift* Lift=nullptr;UNeonCitySaveSubsystem* Save=nullptr;TArray<AStaticMeshActor*> Fixtures;
 double Started=0;float At=0,SupportError=0;int32 Stage=0,Target=0,Rides=0;bool RoofSave=false,bRoofOnly=false,bNativeEstate=false;
 FNeonCityVisualEvidence Evidence{TEXT("TheOneLift450")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneLiftProfile,"NeonCity.Phase205cUpgrade.TheOneLiftProfilePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneLiftProfile::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(TheOneLiftCheck::FFourStops(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneLiftRoofSave,"NeonCity.Phase205cUpgrade.TheOneLiftRoofSavePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneLiftRoofSave::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(TheOneLiftCheck::FFourStops(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneEstateLift,"NeonCity.Phase205cUpgrade.TheOneEstateLiftPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneEstateLift::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(TheOneLiftCheck::FFourStops(this,false,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
