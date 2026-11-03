#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace TheOneSafety
{
class FNativeBoundaries : public IAutomationLatentCommand
{
public:
 explicit FNativeBoundaries(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>140)return End(TEXT("Native estate guard evidence timeout"));
  W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<5)return false;
  if(!Ready)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);
   if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It)
   {
    UStaticMesh* Mesh=It->GetStaticMeshComponent()->GetStaticMesh();if(!Mesh)continue;
    if(Mesh->GetName()==TEXT("SM_TheOne_GuardGlass489")){Glass=*It;Pose=It->GetActorTransform();}
    if(Mesh->GetName()==TEXT("SM_TheOne_GuardMetal484"))Metal=*It;
    if(Mesh->GetName()==TEXT("SM_TheOne_TurnGuardMetal489"))Turns=*It;
   }
   if(!Glass || !Metal || !Turns)return End(TEXT("Reviewed native guard assets missing"));
   if(!CheckGuards())return End(TEXT("An actual native guard or required stair passage failed"));
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=68;
   OldView=PC->GetViewTarget();PC->SetViewTarget(Camera);
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)
   {Clock=*It;OldHour=Clock->GetTimeOfDay();OldCycle=Clock->bCycleEnabled;Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(12);break;}
   Ready=true;SetView();return false;
  }
  Evidence.Record(W,PC,View);
  if(W->GetTimeSeconds()-At<3.5f)return false;
  const FString Name=FString::Printf(TEXT("TheOne-Safety-%02d.png"),View);
  if(!FNeonCityVisualEvidence::CaptureHUD(W,Name))return End(TEXT("Actual native scene capture failed"));
  if(++View==6)return End(TEXT(""));SetView();return false;
 }
private:
 FVector World(const FVector& Source)const{return Pose.TransformPosition(FVector(Source.X*100,-Source.Y*100,Source.Z*100));}
 bool Read(const FString& Relative,TSharedPtr<FJsonObject>& Obj)
 {
  FString Text;
  if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Phase205cUpgrade")/Relative)))return false;
  return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Obj);
 }
 FVector Vec(const TArray<TSharedPtr<FJsonValue>>& V){return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());}
 bool Guard(const FVector& A,const FVector& B,const FString& Name)
 {
  const FVector Mid=(A+B)*.5+FVector(0,0,.6);
  const FVector Normal=FVector(-(B-A).Y,(B-A).X,0).GetSafeNormal();bool Good=true;
  const FVector Start=World(Mid-Normal*.4),Finish=World(Mid+Normal*.4);
  // Two union edges coincide with the original upper fascia. A world sweep
  // correctly meets that wall first; also trace the guard component itself
  // so an unrelated occluder cannot conceal a missing/uncooked rail.
  FCollisionQueryParams Query;Query.bTraceComplex=true;bool GuardPresent=false;
  for(AStaticMeshActor* Actor:{Glass,Metal,Turns})
  {
   FHitResult Hit;
   if(Actor->GetStaticMeshComponent()->LineTraceComponent(Hit,Start,Finish,Query))
   {
    auto* C=Actor->GetStaticMeshComponent();
    GuardPresent|=C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && C->GetCollisionResponseToChannel(ECC_Vehicle)==ECR_Block;
   }
  }
  for(ECollisionChannel Channel:{ECC_Pawn,ECC_Vehicle})
  {
   FHitResult Hit;
   const bool Blocked=W->SweepSingleByChannel(Hit,Start,Finish,FQuat::Identity,Channel,FCollisionShape::MakeSphere(3));
   const bool NativeGuardHit=Hit.GetActor()==Glass || Hit.GetActor()==Metal || Hit.GetActor()==Turns;
   const bool Correct=Blocked && (NativeGuardHit || GuardPresent);
   Test->TestTrue(FString::Printf(TEXT("%s blocks %s physically"),*Name,Channel==ECC_Pawn?TEXT("player"):TEXT("vehicle")),Correct);Good&=Correct;
   if(!Correct)UE_LOG(LogTemp,Error,TEXT("TheOne guard %s channel%d mid%s hit%s component%s"),*Name,int32(Channel),*Mid.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
  }
  ++Probes;return Good;
 }
 bool CheckGuards()
 {
  TSharedPtr<FJsonObject> Slab,Stair;
  if(!Read(TEXT("TheOne484Safety/receipt.json"),Slab) || !Read(TEXT("TheOne489StairFix/receipt.json"),Stair))return false;
  bool Good=true;int32 Index=0;
  for(const auto& Value:Slab->GetArrayField(TEXT("actual_slab_edges")))
  {
   const auto Row=Value->AsObject();Good&=Guard(Vec(Row->GetArrayField(TEXT("a"))),Vec(Row->GetArrayField(TEXT("b"))),FString::Printf(TEXT("SlabEdge%d"),Index++));
  }
  Index=0;
  for(const auto& Value:Stair->GetArrayField(TEXT("turn_landings")))
  {
   const auto Row=Value->AsObject();const auto B=Row->GetArrayField(TEXT("bounds_m"));const double Z=Row->GetNumberField(TEXT("z_m"));
   const double X0=B[0]->AsNumber(),Y0=B[1]->AsNumber(),X1=B[2]->AsNumber(),Y1=B[3]->AsNumber();
   Good&=Guard(FVector(X0,Y1,Z),FVector(X0,Y0,Z),FString::Printf(TEXT("Landing%dLeft"),Index));
   Good&=Guard(FVector(X0,Y0,Z),FVector(X1,Y0,Z),FString::Printf(TEXT("Landing%dBack"),Index));
   Good&=Guard(FVector(X1,Y0,Z),FVector(X1,Y1,Z),FString::Printf(TEXT("Landing%dRight"),Index));
   // The two flight mouths remain open above the actual landing tread.
   for(double X:{X0+.875,X1-.875})
   {
    FHitResult Hit;const FVector Centre(X,Y1,Z+.7);
    const bool Blocked=W->SweepSingleByChannel(Hit,World(Centre+FVector(0,-.25,0)),World(Centre+FVector(0,.25,0)),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(28));
    Test->TestFalse(FString::Printf(TEXT("Landing%d flight mouth %.2f remains open"),Index,X),Blocked);Good&=!Blocked;
   }
   ++Index;
  }
  return Good && Probes==52;
 }
 void SetView()
 {
  // Fixed native scene cameras; these do not certify a driven or flown route.
  const FVector Eye[]={FVector(-88,-100,30),FVector(-5,22,3.1),FVector(-5,22,6.9),FVector(-5,22,11.4),FVector(-7,27.5,13.8),FVector(24,-18,15)};
  const FVector Aim[]={FVector(0,0,6),FVector(-3,24.3,1.8),FVector(-3,23.7,5.8),FVector(-3,23.7,10.3),FVector(-35,10,7),FVector(0,0,6)};
  const FVector E=World(Eye[View]),T=World(Aim[View]);Camera->SetActorLocationAndRotation(E,(T-E).Rotation());At=W->GetTimeSeconds();
 }
 bool End(const FString& Failure)
 {
  if(!Failure.IsEmpty())Test->AddError(Failure);
  if(PC && OldView.IsValid())PC->SetViewTarget(OldView.Get());if(Camera)Camera->Destroy();
  if(Clock){Clock->SetTimeOfDay(OldHour);Clock->SetCycleEnabled(OldCycle);}
  UE_LOG(LogTemp,Display,TEXT("TheOneSafety494 result=%s boundaries=%d player_vehicle_sweeps=%d images=%d scope=actual authored native guards; fixed scene cameras"),Failure.IsEmpty()?TEXT("PASS"):TEXT("FAIL"),Probes,Probes*2,View);
  return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;AStaticMeshActor* Glass=nullptr;AStaticMeshActor* Metal=nullptr;AStaticMeshActor* Turns=nullptr;
 ACameraActor* Camera=nullptr;ANeonCityDayNightCycle* Clock=nullptr;TWeakObjectPtr<AActor> OldView;FTransform Pose;
 double Started=0;float At=0,OldHour=0;int32 View=0,Probes=0;bool Ready=false,OldCycle=false;
 FNeonCityVisualEvidence Evidence{TEXT("TheOneSafety494")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOneNativeSafety,"NeonCity.Phase205cUpgrade.TheOneNativeSafetyPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTheOneNativeSafety::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 ADD_LATENT_AUTOMATION_COMMAND(TheOneSafety::FNativeBoundaries(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
