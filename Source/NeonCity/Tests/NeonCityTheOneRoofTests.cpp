#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandResidenceLift.h"
#include "Adventure/WingsuitLaunchPoint.h"
#include "Adventure/WingsuitComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"

namespace TheOneRoofCheck
{
/** Continue the genuine four-stop roof save using mapped player controls.
 * No player placement, gate write, gear injection or collision override. */
class FAccess : public IAutomationLatentCommand
{
public:
 explicit FAccess(FAutomationTestBase* T):Test(T){}
 ~FAccess(){if(Driver.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Driver);}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>180)return End(TEXT("Actual roof access exceeded bounded walking budget"));
  if(!P || W->GetTimeSeconds()<4)return false;
  const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,27458))))return false;
   for(TActorIterator<AIslandResidenceLift> It(W);It;++It)if(It->ActorHasTag(TEXT("NeonCity.Phase205cUpgrade.TheOne478")))Lift=*It;
   for(TActorIterator<AWingsuitLaunchPoint> It(W);It;++It)if(It->bRoofSafetyGate)Launch=*It;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)if(It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_TheOne_HighestPad476"))Pad=*It;
   Suit=P->FindComponentByClass<UWingsuitComponent>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Lift || !Launch || !Pad || !Suit || !Save)return End(TEXT("Actual roof lift, flight deck and original public equipment required"));
   Estate=Pad->GetActorTransform();
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_TheOneEstate480"),false) || !Save->DoesSaveExist())return End(TEXT("Retained genuine native roof save missing"));
   Key(EKeys::F7,true);return Next(1);
  }
  Evidence.Record(W,PC,Stage);
  if(Stage==1 && Age>1.5)
  {
   Key(EKeys::F7,false);if(Save->IsOperationInProgress())return false;
   // Lift saves deliberately choose a physical fixed landing. The cabin is
   // transient and correctly starts at Garage in a new process.
   const auto* Saved=Save->ReadSaveForInspection();const FVector Local=Estate.InverseTransformPosition(P->GetActorLocation());
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Saved || !P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist(P->GetActorLocation(),Saved->Player.Transform.GetLocation())>30 || FMath::Abs(Local.Z-1204-P->GetSimpleCollisionHalfHeight()-3)>5)
    return End(TEXT("Independent real roof save did not restore its supported fixed landing"));
   if(!GateBarrier(false))return End(TEXT("Closed roof safety gate does not physically close the authored guard opening"));
   Driver=FWorldDelegates::OnWorldPreActorTick.AddRaw(this,&FAccess::DriveFrame);
   // Survey each real stair flight separately from its long flat deck. A
   // single linear height estimate across both undershoots the upper tread.
   Goals={Estate.TransformPosition(FVector(70,-1050,1204)),Estate.TransformPosition(FVector(-600,-1050,1504)),Estate.TransformPosition(FVector(-1750,-1050,1504)),Estate.TransformPosition(FVector(-600,-1050,1504)),Estate.TransformPosition(FVector(70,-1050,1204))};
   GoalIndex=0;if(!Plan(Goals[0],25))return End(TEXT("Roof cabin has no supported walk to flight-deck stair foot"));return Next(2);
  }
  if(Stage==2)
  {
   if(!Walk())return Failure.IsEmpty()?false:End(Failure);
   if(GoalIndex==2)
   {
    FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneFlightDeckContact),false,P);
    if(!P->GetCharacterMovement()->IsMovingOnGround() || !W->LineTraceSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()-FVector(0,0,150),ECC_Pawn,Q) || H.GetActor()!=Pad || FMath::Abs(Estate.InverseTransformPosition(H.ImpactPoint).Z-1504)>3)
     return End(TEXT("Normal stair ascent did not reach the real highest pad surface"));
    FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Roof-HighestPad.png"));PadWalk=true;
   }
   if(++GoalIndex<Goals.Num())
   {
    const bool Planned=GoalIndex==1?PlanStair(false):GoalIndex==4?PlanStair(true):Plan(Goals[GoalIndex],25);
    if(!Planned)return End(FString::Printf(TEXT("Actual flight deck stairs lack full-capsule access to segment%d"),GoalIndex));return Next(2);
   }
   if(!PlanLocker())return End(TEXT("Public roof locker has no supported, visible normal player approach"));return Next(3);
  }
  if(Stage==3)
  {
   if(!Walk())return Failure.IsEmpty()?false:End(Failure);
   PC->SetControlRotation((Launch->InteractionTarget->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Launch)return End(TEXT("Normal F roof equipment prompt occluded"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Roof-LockerPrompt.png"));Key(EKeys::F,true);return Next(4);
  }
  if(Stage==4 && Age>1.2)
  {
   Key(EKeys::F,false);
   if(!Suit->IsEquipped() || Suit->IsFlying() || !Launch->IsGateOpen() || !GateBarrier(true))return End(TEXT("Mapped F did not equip gear and physically open the actual guard passage"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("TheOne-Roof-OpenSafetyGate.png"));return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Old=Keys.FindOrAdd(K);if(Old!=Down && PC){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool Plan(FVector Floor,float Grid)
 {
  FVector Goal=Floor+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
  if(!Phase205cSupportedWalkPath(W,P,Goal,Path,P->GetSimpleCollisionHalfHeight()+25,Grid))return false;
  FVector Previous=P->GetActorLocation();TArray<FVector> Reduced;
  for(int32 I=0;I<Path.Num();++I)
  {
   if(I+1<Path.Num() && FMath::Abs(Previous.Z-Path[I].Z)<1 && FMath::Abs(Path[I].Z-Path[I+1].Z)<1 && FVector::DotProduct((Path[I]-Previous).GetSafeNormal2D(),(Path[I+1]-Path[I]).GetSafeNormal2D())>.99999)continue;
   Reduced.Add(Path[I]);Previous=Path[I];
  }
  Path=MoveTemp(Reduced);Node=0;Progress=W->GetTimeSeconds();Failure.Reset();
  UE_LOG(LogTemp,Display,TEXT("TheOneRoof517 planned actual path%d start%s goal%s"),Path.Num(),*P->GetActorLocation().ToString(),*Goal.ToString());return !Path.IsEmpty();
 }
 bool PlanLocker()
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneLockerApproach),false,P);
  for(float Radius:{150.f,190.f,220.f})for(int32 I=0;I<16;++I)
  {
   const float Angle=I*UE_TWO_PI/16;const FVector Probe=Launch->GetActorLocation()+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius;FHitResult H,Eye;
   if(!W->LineTraceSingleByChannel(H,Probe+FVector(0,0,150),Probe-FVector(0,0,100),ECC_Pawn,Q) || !P->GetCharacterMovement()->IsWalkable(H))continue;
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(!W->LineTraceSingleByChannel(Eye,Stand+FVector(0,0,P->BaseEyeHeight),Launch->InteractionTarget->GetComponentLocation(),ECC_Visibility,Q) || Eye.GetActor()!=Launch)continue;
   if(Plan(H.ImpactPoint,50))return true;
  }
  return false;
 }
 bool PlanStair(bool Descending)
 {
  // Real stairs support a capsule against several neighbouring treads. A
  // vertical point-ray under its centre underestimates that support and the
  // shared flat-ground planner correctly rejects the resulting overlap.
  // Survey the authored flight with the actual capsule, without moving it.
  Path.Reset();FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneNativeStairCapsule),false,P);
  const auto Shape=FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());
  TArray<FVector> Samples;
  for(int32 I=0;I<18;++I)Samples.Add(Estate.TransformPosition(FVector((-0.5-(I+.5)*.28)*100,-1050,1204+(I+1)*300.f/18)));
  if(Descending)Algo::Reverse(Samples);
  Samples.Add(Goals[GoalIndex]);FVector Previous=P->GetActorLocation();
  for(const FVector& Floor:Samples)
  {
   const FVector Center=Floor+FVector(0,0,P->GetSimpleCollisionHalfHeight());FHitResult H;
   if(!W->SweepSingleByChannel(H,Center+FVector(0,0,100),Center-FVector(0,0,100),FQuat::Identity,ECC_Pawn,Shape,Q) || !P->GetCharacterMovement()->IsWalkable(H))
   {UE_LOG(LogTemp,Warning,TEXT("TheOneRoof517 stair survey floor%s actor%s normal%s"),*Floor.ToString(),*GetNameSafe(H.GetActor()),*H.ImpactNormal.ToString());return false;}
   const FVector Stand=H.Location+FVector(0,0,3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,Shape,Q) || FMath::Abs(Stand.Z-Previous.Z)>P->GetCharacterMovement()->MaxStepHeight)
   {UE_LOG(LogTemp,Warning,TEXT("TheOneRoof517 stair overlap or step source%s target%s"),*Previous.ToString(),*Stand.ToString());return false;}
   const FVector Raised=Previous+FVector(0,0,P->GetCharacterMovement()->MaxStepHeight),Across(Stand.X,Stand.Y,Raised.Z);FHitResult Clearance;
   if(W->SweepSingleByChannel(Clearance,Previous,Raised,FQuat::Identity,ECC_Pawn,Shape,Q) || W->SweepSingleByChannel(Clearance,Raised,Across,FQuat::Identity,ECC_Pawn,Shape,Q))
   {UE_LOG(LogTemp,Warning,TEXT("TheOneRoof517 physical stair headroom actor%s component%s"),*GetNameSafe(Clearance.GetActor()),*GetNameSafe(Clearance.GetComponent()));return false;}
   Path.Add(Stand);Previous=Stand;
  }
  Node=0;Progress=W->GetTimeSeconds();Failure.Reset();return true;
 }
 bool Walk()
 {
  while(Node<Path.Num() && FVector::Dist2D(P->GetActorLocation(),Path[Node])<18 && FMath::Abs(P->GetActorLocation().Z-Path[Node].Z)<25){++Node;Progress=W->GetTimeSeconds();}
  if(Node>=Path.Num()){Key(EKeys::W,false);return true;}
  if(W->GetTimeSeconds()-Progress>12)Failure=FString::Printf(TEXT("Normal roof walk blocked stage%d node%d player%s target%s"),Stage,Node,*P->GetActorLocation().ToString(),*Path[Node].ToString());return false;
 }
 void DriveFrame(UWorld* World,ELevelTick,float)
 {if(World==W && (Stage==2 || Stage==3) && Path.IsValidIndex(Node)){const FVector D=Path[Node]-P->GetActorLocation();PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));P->DoMove(0,FMath::Clamp(float(D.Size2D()/150),.08f,.7f));}}
 bool GateBarrier(bool Open)
 {
  const FVector Middle=Launch->GetActorTransform().TransformPosition(FVector(0,-240,55));
  const FVector Normal=Launch->GetActorTransform().TransformVectorNoScale(FVector(0,1,0));
  for(ECollisionChannel Channel:{ECC_Pawn,ECC_Vehicle})
  {
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TheOneRealGateBarrier),false,P);
   const bool Hit=W->SweepSingleByChannel(H,Middle+Normal*70,Middle-Normal*70,FQuat::Identity,Channel,FCollisionShape::MakeSphere(25),Q);
   if(Open?Hit:(!Hit || H.GetComponent()!=Launch->SafetyGate))
   {UE_LOG(LogTemp,Warning,TEXT("TheOneRoof517 gate open%d channel%d hit%d actor%s component%s"),Open,int32(Channel),Hit,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()));return false;}
  }
  return true;
 }
 bool End(const FString& Why)
 {
  if(Driver.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Driver);Driver.Reset();}
  for(auto& K:Keys)if(K.Value)Key(K.Key,false);
  if(!Why.IsEmpty())Test->AddError(Why);else Test->TestTrue(TEXT("Actual roof save, highest-pad stairs and original physical equipment gate passed"),PadWalk);
  UE_LOG(LogTemp,Display,TEXT("TheOneRoof517 result=%s stage%d pad_walk%d player_pose_writes=0 failure=%s"),Why.IsEmpty()?TEXT("PASS"):TEXT("FAIL"),Stage,PadWalk,*Why);return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;
 AIslandResidenceLift* Lift=nullptr;AWingsuitLaunchPoint* Launch=nullptr;AStaticMeshActor* Pad=nullptr;UWingsuitComponent* Suit=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 FTransform Estate;TArray<FVector> Goals,Path;TMap<FKey,bool> Keys;FDelegateHandle Driver;FString Failure;
 double Started=0;float At=0,Progress=0;int32 Stage=0,Node=0,GoalIndex=0;bool PadWalk=false;FNeonCityVisualEvidence Evidence{TEXT("TheOneRoof517")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneRoofAccess,"NeonCity.Phase205cUpgrade.TheOneRoofAccessPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneRoofAccess::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(TheOneRoofCheck::FAccess(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
