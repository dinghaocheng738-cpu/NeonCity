#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Vehicles/Marine/MarineWorldSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace CurrentEstateSafety
{
class FSurvey:public IAutomationLatentCommand
{
public:
 explicit FSurvey(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  AStaticMeshActor* Floor=nullptr;
  for(TActorIterator<AStaticMeshActor> I(W);I;++I)
  {
   auto* C=I->GetStaticMeshComponent();if(!C || !C->GetStaticMesh())continue;
   const FString Name=C->GetStaticMesh()->GetName();
   if(I->GetName()==TEXT("StaticMeshActor_433"))Floor=*I;
   if(Name==TEXT("SM_TheOne_SupportedLeisureGlass660"))Rail=*I;
   if(Name.StartsWith(TEXT("SM_TheOne_SolidCollision659_")) || Name==TEXT("SM_TheOne_CinemaSolid666"))
    if(I->GetActorEnableCollision())
    {
     ++SolidProxies;Check(C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && C->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block,TEXT("Physical furniture proxy blocks Pawn and Vehicle: ")+Name);
    }
  }
  if(!Floor || !Rail){Test->AddError(TEXT("Current native638 estate floor/660 guard required"));return true;}Pose=Floor->GetActorTransform();
  TSharedPtr<FJsonObject> Rooms,Edges,Site;
  if(!Read(TEXT("Saved/Phase205cUpgrade/TheOneAllRooms641c/receipt.json"),Rooms) || !Read(TEXT("ArtSource/Phase205cUpgrade/TheOne633LeisureAccess/receipt.json"),Edges) || !Read(TEXT("ArtSource/Phase205cUpgrade/TheOne630PhotoLeisure/receipt.json"),Site))return true;
  for(const auto& Value:Rooms->GetArrayField(TEXT("rooms")))
  {
   const auto R=Value->AsObject();const FString ID=R->GetStringField(TEXT("id"));FVector Eye=Vec(R,TEXT("camera_m"));const double Z=Vec(R,TEXT("centre_m")).Z;
   if(ID==TEXT("Lower:Cinema"))Eye=FVector(-9.1,-2.5,1.6);
   if(ID.Contains(TEXT("Wine")))Eye=FVector(-.3,3.2,4.648);
   bool Clear=false;FVector Selected;FString NativeFloor;
   // A finite physical survey around each reviewed camera, not a player pose
   // fixture or a claim that every door was traversed. All solids remain.
   for(int32 Ring=0;Ring<=4 && !Clear;++Ring)for(int32 I=0;I<(Ring?16:1) && !Clear;++I)
   {
    const double A=I*2*PI/16;const FVector Base=S(FVector(Eye.X+Ring*.35*FMath::Cos(A),Eye.Y+Ring*.35*FMath::Sin(A),Z));
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentEstateFullCapsule),false,P);FHitResult Hit;
    if(!W->LineTraceSingleByChannel(Hit,Base+FVector(0,0,40),Base-FVector(0,0,25),ECC_Pawn,Q) || Hit.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ() || FMath::Abs(Hit.ImpactPoint.Z-Base.Z)>15)continue;
    const FVector Stand=Hit.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
    if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))continue;
    Clear=true;Selected=Stand;NativeFloor=GetNameSafe(Hit.GetActor());
   }
   Check(Clear,TEXT("Full player capsule on real dry floor near reviewed room view: ")+ID);
   auto ROut=MakeShared<FJsonObject>();ROut->SetStringField(TEXT("room"),ID);ROut->SetBoolField(TEXT("clear_supported_capsule"),Clear);ROut->SetStringField(TEXT("measured_position"),Selected.ToString());ROut->SetStringField(TEXT("floor"),NativeFloor);RoomRows.Add(MakeShared<FJsonValueObject>(ROut));
  }
  const auto SiteData=Site->GetObjectField(TEXT("site"));const double Yaw=FMath::DegreesToRadians(SiteData->GetNumberField(TEXT("yaw_blender_degrees")));
  int32 Edge=0;
  for(const auto& Value:Edges->GetArrayField(TEXT("checks")))
  {
   const auto R=Value->AsObject();const auto Ends=R->GetArrayField(TEXT("edge_local_m"));const auto AA=Ends[0]->AsArray(),BB=Ends[1]->AsArray();
   const FVector A(AA[0]->AsNumber(),AA[1]->AsNumber(),0),B(BB[0]->AsNumber(),BB[1]->AsNumber(),0),Delta=B-A,Normal(-Delta.Y,Delta.X,0);
   const auto Area=SiteData->GetObjectField(Edge<4?TEXT("terrace"):TEXT("lower_lounge"));const auto Centre=Area->GetArrayField(TEXT("centre_m"));const double FloorZ=Edge<4?3.048:0;
   auto Point=[&](FVector V){return S(FVector(Centre[0]->AsNumber()+V.X*FMath::Cos(Yaw)-V.Y*FMath::Sin(Yaw),Centre[1]->AsNumber()+V.X*FMath::Sin(Yaw)+V.Y*FMath::Cos(Yaw),FloorZ+.61));};
   for(const auto& V:R->GetArrayField(TEXT("guarded_intervals")))
   {
    const auto Interval=V->AsArray();const double Lo=Interval[0]->AsNumber(),Hi=Interval[1]->AsNumber();const int32 Count=FMath::Max(1,FMath::CeilToInt((Hi-Lo)*Delta.Size2D()/.5));
    for(int32 I=0;I<Count;++I)
    {
     const double T=FMath::Lerp(Lo,Hi,(I+.5)/Count);const FVector At=A+Delta*T;
     //660 is a deliberate supported2.4m dry passage, not an exposed edge.
     if(Edge==2 && FMath::Abs(At.X)<1.3)continue;
     const FVector N=Normal.GetSafeNormal()*.4,From=Point(At-N),To=Point(At+N);FHitResult Hit;FCollisionQueryParams Q;Q.bTraceComplex=true;
     const bool OwnPane=Rail->GetStaticMeshComponent()->LineTraceComponent(Hit,From,To,Q);
     bool Blocks=OwnPane;
     for(ECollisionChannel Channel:{ECC_Pawn,ECC_Vehicle})
     {FHitResult Physical;Blocks&=Rail->GetStaticMeshComponent()->GetCollisionResponseToChannel(Channel)==ECR_Block && W->SweepSingleByChannel(Physical,From,To,FQuat::Identity,Channel,FCollisionShape::MakeSphere(3),Q);}
     Check(Blocks,FString::Printf(TEXT("Current actual safety pane edge%d sample%d blocks player/vehicle"),Edge,I));++GuardProbes;
    }
   }
   ++Edge;
  }
  Check(RoomRows.Num()==33,TEXT("All33 current registered rooms surveyed"));Check(SolidProxies>0,TEXT("Imported solid furniture has physical proxies"));
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Run)/TEXT("CurrentEstateSafety");IFileManager::Get().MakeDirectory(*Dir,true);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),!Test->HasAnyErrors());J->SetArrayField(TEXT("room_capsule_samples"),RoomRows);J->SetNumberField(TEXT("native_guard_component_probes"),GuardProbes);J->SetNumberField(TEXT("physical_furniture_proxies"),SolidProxies);J->SetStringField(TEXT("scope"),TEXT("Read-only normal PIE native full player capsule/dry floor at all33 reviewed rooms; actual660 safety pane component presence plus Pawn/Vehicle sweeps across ordinary633 boundaries, existing intended entrances and stepping paths excluded; physical659/666 furniture proxy responses. No gameplay, pose, collision or camera writes. Continuous walks/doors are separate evidence."));
  FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));return true;
 }
private:
 void Check(bool B,const FString& Label){Test->TestTrue(Label,B);}
 FVector S(FVector M)const{return Pose.TransformPosition(FVector(M.X*100,-M.Y*100,M.Z*100));}
 FVector Vec(const TSharedPtr<FJsonObject>& O,const TCHAR* Name)const{const auto V=O->GetArrayField(Name);return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());}
 bool Read(const TCHAR* Path,TSharedPtr<FJsonObject>& O){FString T;if(!FFileHelper::LoadFileToString(T,*(FPaths::ProjectDir()/Path)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T),O)){Test->AddError(FString(TEXT("Required current receipt absent: "))+Path);return false;}return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityPlayerCharacter* P=nullptr;AStaticMeshActor* Rail=nullptr;FTransform Pose;int32 SolidProxies=0,GuardProbes=0;TArray<TSharedPtr<FJsonValue>> RoomRows;
};
}
namespace CurrentEstateSafety
{
class FDoorsWater:public IAutomationLatentCommand
{
public:
 explicit FDoorsWater(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  AStaticMeshActor* Floor=nullptr;for(TActorIterator<AStaticMeshActor> I(W);I;++I)if(I->GetName()==TEXT("StaticMeshActor_433"))Floor=*I;
  auto* Marine=W->GetSubsystem<UMarineWorldSubsystem>();if(!Floor || !Marine){Test->AddError(TEXT("Native estate frame/water subsystem absent"));return true;}
  const FTransform Pose=Floor->GetActorTransform();auto S=[&](FVector M){return Pose.TransformPosition(FVector(M.X*100,-M.Y*100,M.Z*100));};
  auto Guest=[](double X,double Y,double Z){const double A=FMath::DegreesToRadians(-18.);return FVector(-67+X*FMath::Cos(A)-Y*FMath::Sin(A),13.5+X*FMath::Sin(A)+Y*FMath::Cos(A),Z);};
  FString Report=FString::Printf(TEXT("Read-only native PIE; full normal capsule guest doorway sweeps and finite water/aperture floor queries. No player fixtures or collision changes. Not a swim-handling claim. radius=%.2f half-height=%.2f\n"),P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CurrentEstateDoorsWater),false,P);const auto Shape=FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight());
  for(double X:{-1.85,4.45,10.75})
  {
   FVector Previous;bool Clear=true;
   // Cross the actual doorway, then follow the clear left aisle around the
   // fitted vanity facing it. A straight line through furniture is not the
   // authored bedroom circulation path.
   TArray<FVector> Points;
   for(int32 I=0;I<=8;++I)Points.Add(Guest(X,-3+I*.2,0));
   for(int32 I=1;I<=8;++I)Points.Add(Guest(X-I*.2,-1.4,0));
   for(int32 I=1;I<=10;++I)Points.Add(Guest(X-1.6,-1.4+I*.2,0));
   for(int32 I=0;I<Points.Num();++I)
   {
    const FVector Base=S(Points[I]);FHitResult Hit;
    if(!W->LineTraceSingleByChannel(Hit,Base+FVector(0,0,40),Base-FVector(0,0,25),ECC_Pawn,Q) || FMath::Abs(Hit.ImpactPoint.Z-Base.Z)>10 || Hit.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ()){Clear=false;break;}
    const FVector Stand=Hit.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
    if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,Shape,Q)){Clear=false;TArray<FOverlapResult> Overlaps;W->OverlapMultiByChannel(Overlaps,Stand,FQuat::Identity,ECC_Pawn,Shape,Q);Report+=TEXT("door overlap at ")+Pose.InverseTransformPosition(Stand).ToString()+TEXT("\n");for(const auto& O:Overlaps)if(O.bBlockingHit)Report+=TEXT("blocking ")+GetNameSafe(O.GetActor())+TEXT(" / ")+GetNameSafe(O.GetComponent())+TEXT("\n");break;}
    FHitResult Block;if(I && W->SweepSingleByChannel(Block,Previous,Stand,FQuat::Identity,ECC_Pawn,Shape,Q)){Clear=false;Report+=TEXT("door sweep ")+GetNameSafe(Block.GetActor())+TEXT("\n");break;}Previous=Stand;
   }
   Test->TestTrue(FString::Printf(TEXT("Guest bedroom %.2fm full supported capsule doorway"),X),Clear);Report+=FString::Printf(TEXT("guest-door %.2f %s\n"),X,Clear?TEXT("PASS"):TEXT("FAIL"));
  }
  auto Basin=[&](const TCHAR* Label,FVector Centre,double Bottom,double Surface)
  {
   FVector At=S(FVector(Centre.X,Centre.Y,Surface-.4));FWaterSurfaceSample Sample;const bool Water=Marine->QueryWater(At,Sample) && Sample.ContainedWater && Sample.InsideVolume && FMath::Abs(Sample.SurfacePoint.Z-S(FVector(Centre.X,Centre.Y,Surface)).Z)<3;
   Test->TestTrue(FString(Label)+TEXT(" native contained water"),Water);
   FHitResult Hit;const bool Open=W->LineTraceSingleByChannel(Hit,S(FVector(Centre.X,Centre.Y,Surface+1)),S(FVector(Centre.X,Centre.Y,Bottom-.5)),ECC_Pawn,Q) && Hit.ImpactNormal.Z>.7 && FMath::Abs(Hit.ImpactPoint.Z-S(FVector(Centre.X,Centre.Y,Bottom)).Z)<5;
   Test->TestTrue(FString(Label)+TEXT(" true aperture and submerged collision floor"),Open);Report+=FString::Printf(TEXT("%s water=%d aperture=%d floor=%s hit=%s\n"),Label,Water,Open,*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString());
  };
  Basin(TEXT("Indoor pool"),FVector(20,18.4,0),-1.55,-.1);
  Basin(TEXT("Main infinity"),FVector(45.8,-30.9,0),3.08,4.53);
  const double A=FMath::DegreesToRadians(-32.);Basin(TEXT("Lower five lounges water"),FVector(40+2.6*FMath::Cos(A),-44.5+2.6*FMath::Sin(A),0),-1.25,-.1);
  for(double X:{-7.,0.,7.})
  {
   const FVector Point=Guest(X,.9*FMath::Cos(X/12*PI/2)-10.3,0);Basin(*FString::Printf(TEXT("Guest curve %.1f"),X),Point,-1.45,-.1);
   FWaterSurfaceSample Dry;const bool Acquired=Marine->QueryWater(S(Guest(X,-5,-.2)),Dry) && Dry.ContainedWater && Dry.InsideVolume;Test->TestFalse(TEXT("Guest dry return remains outside finite pool water"),Acquired);
  }
  // Read-only aerial follow-up inventory: identify small visible native meshes
  // below the estate apron before deciding whether a distant white shape is
  // an intended fitting or unsupported geometry. No automatic deletion.
  for(TActorIterator<AStaticMeshActor> I(W);I;++I)
  {
   auto* C=I->GetStaticMeshComponent();if(!C || !C->GetStaticMesh() || I->IsHidden() || !C->IsVisible())continue;
   const FVector At=C->Bounds.Origin;const FVector L=Pose.InverseTransformPosition(At);
   if(L.Z<-20 && L.Z>-1800 && L.Size2D()<12000 && C->Bounds.BoxExtent.Size2D()<600)
    Report+=FString::Printf(TEXT("aerial-candidate %s %s local=%s extent=%s\n"),*I->GetName(),*C->GetStaticMesh()->GetPathName(),*L.ToString(),*C->Bounds.BoxExtent.ToString());
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityAerialIdentityDiagnostic")))
  {
  const FVector Flight=NeonCityCoastalLayout::FromLegacyIsland(FVector(52343.923,-112234.064,28854.025)),Aim(45843.804,-119234.008,27509.198);
  const FRotationMatrix Basis((Aim-Flight).Rotation());const FVector Forward=Basis.GetUnitAxis(EAxis::X),Right=Basis.GetUnitAxis(EAxis::Y),Up=Basis.GetUnitAxis(EAxis::Z);const FVector Eye=Flight-Forward*1050+FVector(0,0,200);
  TSet<FString> AerialHits;FCollisionQueryParams RayQ(SCENE_QUERY_STAT(CurrentEstateAerialIdentity),true,P);
  for(int32 X=1000;X<=1240;X+=8)for(int32 Y=700;Y<=896;Y+=8)
  {
   const FVector Direction=(Forward+Right*((X-800)/800.)+Up*((450-Y)/800.)).GetSafeNormal();const FVector To=Eye+Direction*100000;FHitResult Hit;
   if(W->LineTraceSingleByChannel(Hit,Eye,To,ECC_Visibility,RayQ))
   {
    auto* C=Cast<UStaticMeshComponent>(Hit.GetComponent());
    if(C && C->GetStaticMesh())AerialHits.Add(GetNameSafe(Hit.GetActor())+TEXT(" ")+C->GetStaticMesh()->GetPathName()+TEXT(" local=")+Pose.InverseTransformPosition(Hit.ImpactPoint).ToString());
   }
  }
  for(const auto& Row:AerialHits)Report+=TEXT("aerial-ray ")+Row+TEXT("\n");
  }
  FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Run)/TEXT("CurrentEstateDoorsWater");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("scope.txt")));return true;
 }
private:FAutomationTestBase* Test;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentEstateDoorsWater,"NeonCity.Phase205cUpgrade.CurrentEstateDoorsWaterMinimumPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentEstateDoorsWater::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentEstateSafety::FDoorsWater(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCurrentEstateSafety,"NeonCity.Phase205cUpgrade.CurrentEstateSafetyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCurrentEstateSafety::RunTest(const FString&)
{ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(CurrentEstateSafety::FSurvey(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
