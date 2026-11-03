#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Adventure/IslandEstateSport.h"
#include "Adventure/IslandResidenceLift.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"

namespace EstateAccess784
{
class FFlow:public IAutomationLatentCommand
{
public:
 explicit FFlow(FAutomationTestBase* In):Test(In){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>150)return End(TEXT("Estate access audit finite timeout"));
  if(!W || !PC || W->GetTimeSeconds()<4)return false;const float Now=W->GetTimeSeconds();
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44500,-120500,26254))))return false;
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Player)return End(TEXT("Normal player absent"));
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label)/TEXT("EstateAccess784");IFileManager::Get().MakeDirectory(*Dir,true);
   for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetName()==TEXT("StaticMeshActor_433"))Pose=I->GetActorTransform();
   for(TActorIterator<AIslandEstateSport> I(W);I;++I)Court=*I;
   for(TActorIterator<AIslandResidenceLift> I(W);I;++I)Lift=*I;
   if(!Court || !Lift || Pose.GetLocation().IsNearlyZero())return End(TEXT("Delivered current mansion/tennis/lift absent"));
   // Read-only geometric survey: current visual/collision inventory and fixed
   // intended circulation, not pathfinding around an inconvenient obstruction.
   Audit();OriginalPlayer=Player->GetActorTransform();
   Route={S(FVector(-33.5,2.36,0)),S(FVector(-24.026,-2.084,0)),S(FVector(-17,-2.084,0)),S(FVector(-17,7.55,0)),S(FVector(-9,7.55,0)),S(FVector(5.55,7.55,0)),S(FVector(5.55,-2,0)),S(FVector(4.25,-2,0))};
   if(!Place(Route[0]))return End(TEXT("Declared garage pedestrian start is obstructed"));
   Index=1;At=LastProgress=Now;Marker=Player->GetActorLocation();Stage=1;return false;
  }
  if(Stage==1)
  {
   const FVector Delta=Route[Index]-Player->GetActorLocation();
   if(Delta.Size2D()<35){if(++Index==Route.Num()){GarageReached=true;Stage=2;At=Now;}return false;}
   Player->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(float(Delta.Size2D()/100),.2f,.65f));PC->SetControlRotation(FRotator(-5,Delta.Rotation().Yaw,0));
   if(FVector::Dist2D(Marker,Player->GetActorLocation())>20){Marker=Player->GetActorLocation();LastProgress=Now;}
   if(Now-LastProgress>3 || Now-At>35){GarageFailure=FString::Printf(TEXT("Fixed garage-to-lift route blocked waypoint%d at%s"),Index,*Player->GetActorLocation().ToString());Test->AddWarning(GarageFailure);Stage=2;At=Now;}
   return false;
  }
  if(Stage==2)
  {
   // A separate declared local tennis-entry fixture, never a continuous walk
   // from the garage. Native court gate is the east opening in estate metres.
   Player->ConsumeMovementInputVector();Route={S(FVector(-52,-41,.04)),S(FVector(-56,-41,.04)),S(FVector(-56,-33,0)),S(FVector(-87.5,-33,0))};
   if(!Place(Route[0]))return End(TEXT("Actual east court entry capsule blocked"));Index=1;At=LastProgress=Now;Marker=Player->GetActorLocation();Stage=3;return false;
  }
  if(Stage==3)
  {
   const FVector Delta=Route[Index]-Player->GetActorLocation();
   if(Delta.Size2D()<30){if(++Index==Route.Num()){Stage=4;At=Now;}return false;}
   Player->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(float(Delta.Size2D()/80),.15f,.65f));PC->SetControlRotation(FRotator(-7,Delta.Rotation().Yaw,0));
   if(FVector::Dist2D(Marker,Player->GetActorLocation())>15){Marker=Player->GetActorLocation();LastProgress=Now;}
   if(Now-LastProgress>3 || Now-At>25)return End(FString::Printf(TEXT("Normal gate-to-tennis-console walk blocked waypoint%d at%s"),Index,*Player->GetActorLocation().ToString()));
   return false;
  }
  if(Stage==4)
  {
   if(Now-At<.5f)return false;
   PC->SetControlRotation((Court->Interaction->GetComponentLocation()-Player->GetPawnViewLocation()).Rotation());
   if(Player->GetInteractionComponent()->RefreshFocusedInteractable()!=Court)return End(TEXT("Tennis console normal eye trace/prompt unavailable after approach"));
   Key(IE_Pressed);Pressed=true;At=Now;Stage=5;return false;
  }
  if(Stage==5)
  {
   if(Now-At<.25f)return false;Key(IE_Released);Pressed=false;
   TennisStarted=Court->IsActive() && Court->GetOpponent() && PC->GetViewTarget()==Court && Player->FindComponentByClass<UIslandTennisParticipant>();
   if(!TennisStarted)return End(TEXT("Normal F did not start real tennis/opponent/racket after walking through the entry"));
   Key(IE_Pressed);Pressed=true;At=Now;Stage=6;return false;
  }
  if(Stage==6)
  {
   if(Now-At<.25f)return false;Key(IE_Released);Pressed=false;if(Court->IsActive())return End(TEXT("Normal tennis F exit failed"));
   if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityArtStills")))return End(TEXT(""));
   Camera=W->SpawnActor<ACameraActor>();OldView=PC->GetViewTarget();PC->SetViewTarget(Camera);View=0;Stage=7;At=0;return false;
  }
  if(Stage==7)
  {
   if(View==6)return End(TEXT(""));
   const FVector Eyes[]={FVector(-34,2,1.65),FVector(-22,-2,1.65),FVector(-9,1,1.65),FVector(5,-3,1.65),FVector(-10,27.8,4.648),FVector(80,-90,32)};
   const FVector Aims[]={FVector(-24,-2,1.3),FVector(-10,1.6,1.3),FVector(4.25,-2,1.3),FVector(2,-2,1.3),FVector(-12,18,5.4),FVector(10,-10,6)};
   if(At==0){Camera->SetActorLocationAndRotation(S(Eyes[View]),(S(Aims[View])-S(Eyes[View])).Rotation());Camera->GetCameraComponent()->FieldOfView=75;PC->SetViewTarget(Camera);At=Now;return false;}
   if(Now-At<1.5f)return false;
   const bool Paused=UGameplayStatics::IsGamePaused(W);UGameplayStatics::SetGamePaused(W,true);
   const bool OK=FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("EstateAccess784-%d.png"),View));UGameplayStatics::SetGamePaused(W,Paused);
   if(!OK)return End(TEXT("Requested static review image failed"));++View;At=0;return false;
  }
  return false;
 }
private:
 FVector S(FVector V)const{return Pose.TransformPosition(FVector(V.X*100,-V.Y*100,V.Z*100));}
 bool Place(FVector Base)
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(Estate784FixtureFloor),false,Player);FHitResult H;
  if(!W->LineTraceSingleByChannel(H,Base+FVector(0,0,40),Base-FVector(0,0,100),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return false;
  const FVector Stand=H.ImpactPoint+FVector(0,0,Player->GetSimpleCollisionHalfHeight()+3);
  if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight()),Q))return false;
  Player->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();return true;
 }
 void Key(EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::F,E,FPlatformTime::Cycles64()));}
 void Audit()
 {
  TArray<TSharedPtr<FJsonValue>> Actors,Paths;
  for(TActorIterator<AActor> I(W);I;++I)
  {
   if(FVector::DistSquared2D(I->GetActorLocation(),Pose.GetLocation())>FMath::Square(22000.))continue;
   TArray<UPrimitiveComponent*> Components;I->GetComponents(Components);
   for(auto* P:Components)
   {
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("actor"),I->GetName());R->SetStringField(TEXT("label"),I->GetActorLabel());R->SetStringField(TEXT("component"),P->GetName());R->SetStringField(TEXT("location"),P->GetComponentLocation().ToString());R->SetStringField(TEXT("bounds_origin"),P->Bounds.Origin.ToString());R->SetStringField(TEXT("bounds_extent"),P->Bounds.BoxExtent.ToString());
    R->SetBoolField(TEXT("actor_hidden"),I->IsHidden());R->SetBoolField(TEXT("component_visible"),P->IsVisible());R->SetBoolField(TEXT("actor_collision"),I->GetActorEnableCollision());R->SetNumberField(TEXT("collision_enabled"),static_cast<int32>(P->GetCollisionEnabled()));R->SetBoolField(TEXT("blocks_pawn"),P->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block);
    if(auto* M=Cast<UStaticMeshComponent>(P))R->SetStringField(TEXT("mesh"),GetPathNameSafe(M->GetStaticMesh()));Actors.Add(MakeShared<FJsonValueObject>(R));
   }
  }
  const TArray<FVector> Way={FVector(-33.5,2.36,0),FVector(-24.026,-2.084,0),FVector(-17,-2.084,0),FVector(-17,7.55,0),FVector(-9,7.55,0),FVector(5.55,7.55,0),FVector(5.55,-2,0),FVector(4.25,-2,0)};
  for(int32 I=1;I<Way.Num();++I)
  {
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(Estate784FixedCorridor),false,Player);const FVector Up(0,0,Player->GetSimpleCollisionHalfHeight()+3);
   const bool Hit=W->SweepSingleByChannel(H,S(Way[I-1])+Up,S(Way[I])+Up,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Player->GetSimpleCollisionRadius(),Player->GetSimpleCollisionHalfHeight()),Q);
   auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("segment"),I);R->SetBoolField(TEXT("blocked"),Hit);R->SetStringField(TEXT("actor"),GetNameSafe(H.GetActor()));R->SetStringField(TEXT("component"),GetNameSafe(H.GetComponent()));R->SetStringField(TEXT("impact"),H.ImpactPoint.ToString());Paths.Add(MakeShared<FJsonValueObject>(R));
  }
  auto J=MakeShared<FJsonObject>();J->SetArrayField(TEXT("components"),Actors);J->SetArrayField(TEXT("fixed_garage_corridor"),Paths);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("native-survey.json")));
 }
 bool End(FString Error)
 {
  if(Error.IsEmpty() && !GarageReached)Error=GarageFailure.IsEmpty()?TEXT("Fixed garage route incomplete"):GarageFailure;
  if(!Error.IsEmpty())Test->AddError(Error);if(PC && Pressed)Key(IE_Released);if(Court && Court->IsActive())Court->EndSession();if(Player)Player->ConsumeMovementInputVector();if(PC && Camera)PC->SetViewTarget(OldView.IsValid()?OldView.Get():Player);if(Camera)Camera->Destroy();
  if(!Dir.IsEmpty()){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("tennis_normal_approach_and_start"),TennisStarted);J->SetBoolField(TEXT("fixed_garage_route_reached_lift"),GarageReached);J->SetStringField(TEXT("garage_issue"),GarageFailure);J->SetStringField(TEXT("failure"),Error);J->SetStringField(TEXT("scope"),TEXT("Separate declared initial garage and tennis-entry fixtures. Fixed normal-input local walks; no continuous Dock route claim. Gameplay tennis/rally mechanics cached760."));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));}
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;AIslandEstateSport* Court=nullptr;AIslandResidenceLift* Lift=nullptr;ACameraActor* Camera=nullptr;TWeakObjectPtr<AActor> OldView;
 FTransform Pose,OriginalPlayer;TArray<FVector> Route;FVector Marker;FString Dir,GarageFailure;double Started=0;float At=0,LastProgress=0;int32 Stage=0,Index=0,View=0;bool Pressed=false,GarageReached=false,TennisStarted=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEstateAccess784,"NeonCity.Phase205c.Estate.Standalone.Access784",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEstateAccess784::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Access audit must not change player saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(EstateAccess784::FFlow(this));return true;}
#endif
