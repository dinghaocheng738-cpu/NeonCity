#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace ProductionSite152
{
class FLocalWalk final : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;TWeakObjectPtr<ACharacter> Player;TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<AActor> OldView;FTransform OldPose;FRotator OldControl;FVector OldVelocity;
 EMovementMode OldMode=MOVE_None;uint8 OldCustomMode=0;bool Started=false,Restored=false;
 double Began=FPlatformTime::Seconds(),StepAt=0;int32 Step=0,InputFrames=0;FVector Previous;double Distance=0;
 TArray<FVector> Goals;FString Rows=TEXT("game_seconds,step,x_cm,y_cm,z_cm,floor_cm,grounded,movement_normal_z,movement_floor_distance_cm,centre_ray_hit,centre_ray_normal_z\n");
 void Restore()
 {
  if(Restored || !Started)return;Restored=true;
  if(Player.IsValid())
  {
   Player->ConsumeMovementInputVector();Player->SetActorTransform(OldPose,false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->SetMovementMode(OldMode,OldCustomMode);Player->GetCharacterMovement()->Velocity=OldVelocity;
  }
  if(Controller.IsValid()){Controller->SetControlRotation(OldControl);if(OldView.IsValid())Controller->SetViewTarget(OldView.Get());}
 }
 bool End(const FString& Error=FString())
 {
  if(!Error.IsEmpty())Test->AddError(Error);
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/ProductionSite152")/FPaths::MakeValidFileName(Label);
  IFileManager::Get().MakeDirectory(*Dir,true);
  Test->TestTrue(TEXT("Actual local movement/floor records retained"),FFileHelper::SaveStringToFile(Rows,*(Dir/TEXT("local-loggia-walk.csv"))));
  Restore();return true;
 }
public:
 explicit FLocalWalk(FAutomationTestBase* In):Test(In){}
 ~FLocalWalk()override{Restore();}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Began>80)return End(TEXT("Local loggia movement timed out"));
  auto* W=NeonCityValidationWorld();if(!W || W->GetTimeSeconds()<8)return false;
  auto* PC=W->GetFirstPlayerController();auto* C=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!C)return false;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(Production152Floor),false,C);FHitResult Hit;
  if(!Started)
  {
   const FTransform Site(FRotator(0,205,0),FVector(12000,-9000,170));
   // Trace through the centre of a closed pane, away from the frame/rails.
   // A nearby wooden mullion blocking the pawn must not hide absent glass
   // collision. The normal walking route below remains the actual input test.
   const FVector GlassStart=Site.TransformPosition(FVector(-160,160,170));
   const FVector GlassEnd=Site.TransformPosition(FVector(-160,300,170));
   if(!W->LineTraceSingleByChannel(Hit,GlassStart,GlassEnd,ECC_Pawn,Q) || !Hit.GetActor()
      || !Hit.GetActor()->GetPathName().EndsWith(TEXT(".StaticMeshActor_852")))
    return End(TEXT("Closed shopfront pane has no actual player-blocking collision"));
   FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);
   const FString GlassDir=FPaths::ProjectSavedDir()/TEXT("Phase205cUpgrade/ProductionSite152")/FPaths::MakeValidFileName(Label);
   IFileManager::Get().MakeDirectory(*GlassDir,true);
   if(!FFileHelper::SaveStringToFile(FString::Printf(TEXT("Actual closed glass hit: %s at %s, distance %.3fcm\nNo frame/rail substitution; no global route or door interaction claim.\n"),
       *Hit.GetActor()->GetPathName(),*Hit.ImpactPoint.ToString(),Hit.Distance),*(GlassDir/TEXT("closed-glazing-query.txt"))))
    return End(TEXT("Closed glass query evidence could not be saved"));
   for(const FVector2D& XY:TArray<FVector2D>{{-2.05,-5.4},{-2.05,-3.3},{-2.05,-.3},{-1.65,1.42},{-2.05,-.3},{2.05,-.3},{2.05,-5.4},
       {7.3,-5.4},{10.9,-5.4},{12,-4.2},{10.9,-5.4},{7.3,-5.4},
       {-7.3,-5.4},{-10.9,-5.4},{-12,-4.2},{-10.9,-5.4},{-7.3,-5.4},
       {-7.3,-6.95},{-10.9,-6.95},{-7.3,-6.95},{7.3,-6.95},{10.9,-6.95},{7.3,-6.95},{-2.05,-5.4}})
   {
    FVector P=Site.TransformPosition(FVector(XY.X*100,XY.Y*100,0));
    if(!W->LineTraceSingleByChannel(Hit,P+FVector(0,0,150),P-FVector(0,0,100),ECC_Pawn,Q) || Hit.ImpactNormal.Z<.94 || Hit.ImpactPoint.Z<170 || Hit.ImpactPoint.Z>245)
     return End(TEXT("Patio/entered loggia route lost its actual finished floor"));
    P.Z=Hit.ImpactPoint.Z+C->GetSimpleCollisionHalfHeight()+2;Goals.Add(P);
   }
   OldPose=C->GetActorTransform();OldVelocity=C->GetVelocity();OldMode=C->GetCharacterMovement()->MovementMode;OldCustomMode=C->GetCharacterMovement()->CustomMovementMode;
   OldControl=PC->GetControlRotation();OldView=PC->GetViewTarget();Player=C;Controller=PC;Started=true;
   // This is a labelled local fixture at the new lot. It makes no claim
   // about continuous travel from the dock or any unchanged city route.
   C->SetActorLocation(Goals[0],false,nullptr,ETeleportType::TeleportPhysics);C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
   PC->SetViewTarget(C);Previous=C->GetActorLocation();Step=1;StepAt=FPlatformTime::Seconds();return false;
  }
  const FVector P=C->GetActorLocation();Distance+=FVector::Dist2D(Previous,P);Previous=P;
  const bool FloorRayHit=W->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,180),ECC_Pawn,Q);
  const auto& MovementFloor=C->GetCharacterMovement()->CurrentFloor;
  // A centre ray may hit the 1.2mm dressed edge (45 degrees) while the
  // character's floor sweep correctly stands on the adjacent slab. Validate
  // actual movement contact and retain the independent ray, including seams.
  if(!MovementFloor.IsWalkableFloor() || MovementFloor.HitResult.ImpactNormal.Z<.94 || !C->GetCharacterMovement()->IsMovingOnGround())
  {
   return End(FString::Printf(TEXT("Actual walking floor diagnostic: leg%d pos%s rayHit%d rayNormal%s rayActor%s movementWalkable%d movementNormal%s movementDistance%.3f grounded%d"),
    Step,*P.ToString(),FloorRayHit?1:0,*Hit.ImpactNormal.ToString(),*GetPathNameSafe(Hit.GetActor()),MovementFloor.IsWalkableFloor()?1:0,*MovementFloor.HitResult.ImpactNormal.ToString(),MovementFloor.FloorDist,C->GetCharacterMovement()->IsMovingOnGround()?1:0));
  }
  Rows+=FString::Printf(TEXT("%.6f,%d,%.3f,%.3f,%.3f,%.3f,%d,%.6f,%.3f,%d,%.6f\n"),W->GetTimeSeconds(),Step,P.X,P.Y,P.Z,MovementFloor.HitResult.ImpactPoint.Z,C->GetCharacterMovement()->IsMovingOnGround()?1:0,MovementFloor.HitResult.ImpactNormal.Z,MovementFloor.FloorDist,FloorRayHit?1:0,Hit.ImpactNormal.Z);
  if(MovementFloor.FloorDist>12 || P.Z-C->GetSimpleCollisionHalfHeight()-MovementFloor.HitResult.ImpactPoint.Z>12)return End(TEXT("Pawn floats above the real finished floor"));
  if(FVector::Dist2D(P,Goals[Step])<20)
  {
   if(++Step==Goals.Num())
   {
    Test->TestTrue(TEXT("Ordinary movement traversed two arches, the shopfront approach, both raised-wing ramps and their new front strip"),InputFrames>40 && Distance>9000);
    Test->TestTrue(TEXT("Player finishes grounded on the patio"),C->GetCharacterMovement()->IsMovingOnGround());return End();
   }
   StepAt=FPlatformTime::Seconds();
  }
  if(FPlatformTime::Seconds()-StepAt>12)return End(FString::Printf(TEXT("Normal local movement blocked at loggia leg%d position%s"),Step,*P.ToString()));
  const FVector To=(Goals[Step]-P).GetSafeNormal2D();PC->SetControlRotation(To.Rotation());C->AddMovementInput(To,1);++InputFrames;return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionSite152Walk,"NeonCity.Phase205c.MainlandC923.Standalone.ProductionLoggiaLocalWalk",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FProductionSite152Walk::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(ProductionSite152::FLocalWalk(this));return true;}
#endif
