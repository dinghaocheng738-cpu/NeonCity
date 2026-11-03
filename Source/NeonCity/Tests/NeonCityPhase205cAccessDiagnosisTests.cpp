#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandLeisureFacility.h"
#include "Adventure/IslandGarageControl.h"
#include "Adventure/IslandHomeWardrobe.h"
#include "Adventure/IslandPartyControl.h"
#include "Adventure/IslandHomePet.h"
#include "Adventure/IslandActivityJob.h"
#include "Adventure/IslandSupercar.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInterface.h"
namespace AccessDiagnosis
{
class FInspect:public IAutomationLatentCommand
{
public:
 FInspect(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Start)Start=FPlatformTime::Seconds();if(FPlatformTime::Seconds()-Start>45){Test->AddError(TEXT("Access survey initialization timed out"));return true;}if(!P || W->GetTimeSeconds()<4)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-118000,31400))))return false;
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/TEXT("AccessDiagnosis");IFileManager::Get().MakeDirectory(*Dir,true);
  FString Rows=TEXT("target,actor_x,actor_y,actor_z,dx,dy,ground_actor,ground_z,normal_z,blocking_actors,eye_hit,eye_component,clear\n"),Bounds=TEXT("target,nearby_actor,component,asset,min_x,min_y,min_z,max_x,max_y,max_z\n");
  int Count=0;
  for(TActorIterator<AActor> It(W);It;++It)
  {
   AActor* Target=*It;const bool Wanted=Target->IsA<AIslandLeisureFacility>() || Target->IsA<AIslandGarageControl>() || Target->IsA<AIslandHomeWardrobe>() || Target->IsA<AIslandPartyControl>() || Target->IsA<AIslandHomePet>() || Target->IsA<AIslandSupercar>() || Target->IsA<AIslandActivityPoint>();if(!Wanted)continue;
   UBoxComponent* Box=Target->FindComponentByClass<UBoxComponent>();const FVector Aim=Box?Box->GetComponentLocation():Target->GetActorLocation()+FVector(0,0,45);
   const FString ID=Target->GetName();const FVector Center=Target->GetActorLocation();++Count;
   for(const float Radius:{125.f,170.f,215.f,275.f})for(int Angle=0;Angle<360;Angle+=45)
   {
    const FVector Offset=FVector(Radius,0,0).RotateAngleAxis(Angle,FVector::UpVector),Around=Center+Offset;FHitResult H,Eye;FCollisionQueryParams Q(SCENE_QUERY_STAT(AccessSurvey),false,P);
    const bool Floor=W->LineTraceSingleByChannel(H,Around+FVector(0,0,150),Around-FVector(0,0,300),ECC_Pawn,Q);
    FVector Stand=Floor?H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3):Around;
    TArray<FOverlapResult> Overlaps;W->OverlapMultiByChannel(Overlaps,Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);FString Blockers;for(const auto& O:Overlaps)if(O.bBlockingHit && O.GetActor())Blockers+=O.GetActor()->GetName()+TEXT("/")+O.GetComponent()->GetName()+TEXT("|");
    const FVector Eyes=Stand+FVector(0,0,P->BaseEyeHeight);W->LineTraceSingleByChannel(Eye,Eyes,Aim,ECC_Visibility,Q);
    const bool Clear=Floor && H.ImpactNormal.Z>.7 && Blockers.IsEmpty() && Eye.GetActor()==Target;
    Rows+=FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.1f,%.1f,%s,%.2f,%.4f,%s,%s,%s,%d\n"),*ID,Center.X,Center.Y,Center.Z,Offset.X,Offset.Y,*GetNameSafe(H.GetActor()),H.ImpactPoint.Z,H.ImpactNormal.Z,*Blockers,*GetNameSafe(Eye.GetActor()),*GetNameSafe(Eye.GetComponent()),Clear);
   }
   for(TActorIterator<AActor> Other(W);Other;++Other)
   {
    TInlineComponentArray<UPrimitiveComponent*> Parts(*Other);for(auto* C:Parts)if(C->IsCollisionEnabled() && C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && FVector::Dist2D(C->Bounds.Origin,Center)<1500 && FMath::Abs(C->Bounds.Origin.Z-Center.Z)<600)
    {const FBox B=C->Bounds.GetBox();Bounds+=FString::Printf(TEXT("%s,%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n"),*ID,*Other->GetName(),*C->GetName(),*C->GetDetailedInfo(),B.Min.X,B.Min.Y,B.Min.Z,B.Max.X,B.Max.Y,B.Max.Z);}
   }
  }
  FString Materials;auto* Body=P->GetMesh();Materials+=GetNameSafe(Body->GetSkeletalMeshAsset())+TEXT("\n");for(int Slot=0;Slot<Body->GetNumMaterials();++Slot){auto* M=Body->GetMaterial(Slot);FLinearColor Color;Materials+=FString::Printf(TEXT("slot%d %s PaintTint=%d\n"),Slot,*GetPathNameSafe(M),M && M->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Paint Tint")),Color));}
  FFileHelper::SaveStringToFile(Rows,*(Dir/TEXT("access.csv")));FFileHelper::SaveStringToFile(Bounds,*(Dir/TEXT("nearby-collision.csv")));FFileHelper::SaveStringToFile(Materials,*(Dir/TEXT("player-materials.txt")));Test->TestTrue(TEXT("Native actors surveyed; diagnostic only, no gameplay PASS"),Count>12);return true;
 }
 FAutomationTestBase* Test;double Start=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAccessDiagnosis,"NeonCity.Phase205c.AccessDiagnosisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAccessDiagnosis::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(AccessDiagnosis::FInspect(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
