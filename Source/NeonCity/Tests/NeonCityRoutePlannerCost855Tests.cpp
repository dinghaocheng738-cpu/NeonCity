#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityPhase205cPhysicalAccess.h"
#include "Adventure/IslandResidenceLift.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"

namespace RoutePlannerCost855
{
/** Reproduce only validation's synchronous A* query, not another road trip. */
class FProbe final : public IAutomationLatentCommand
{
public:
 explicit FProbe(FAutomationTestBase* In) : Test(In) {}
 ~FProbe(){if(Placed && ProbePlayer.IsValid()){ProbePlayer->SetActorTransform(Original,false,nullptr,ETeleportType::TeleportPhysics);ProbePlayer->GetCharacterMovement()->StopMovementImmediately();}}
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("Planner cost fixture timed out"));return true;}
  if(!P || !W->HasBegunPlay() || W->GetTimeSeconds()<5)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);
  if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(44783,-120500,26254))))return false;
  if(!Placed)
  {
   Original=P->GetActorTransform();ProbePlayer=P;
   // Exact pawn centre retained immediately before854 Stage19 A*. This is
   // one disclosed local setup, never a traversal/exit/performance PASS.
   P->SetActorLocation(NeonCityCoastalLayout::FromLegacyIsland(FVector(42655.3504,-116928.3234,26347.8977)),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetCharacterMovement()->StopMovementImmediately();Placed=true;At=W->GetTimeSeconds();return false;
  }
  if(W->GetTimeSeconds()-At<1)return false;
  AIslandResidenceLift* Lift=nullptr;
  for(TActorIterator<AIslandResidenceLift> I(W);I;++I){if(Lift){Test->AddError(TEXT("Ambiguous native lift"));return true;}Lift=*I;}
  if(!Lift){Test->AddError(TEXT("Native lift missing"));return true;}
  const FVector Goal=Lift->GetActorTransform().TransformPosition(FVector(225,0,0))+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
  TArray<FVector> Path;FPhase205cWalkSurvey Survey;
  const double Begin=FPlatformTime::Seconds();
  const bool Found=Phase205cSupportedWalkPath(W,P,Goal,Path,P->GetSimpleCollisionHalfHeight()+25,25,&Survey,50000);
  const double Milliseconds=(FPlatformTime::Seconds()-Begin)*1000;
  Test->TestTrue(TEXT("Exact garage validation path remains supported"),Found && Path.Num()>0);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("path_found"),Found);J->SetNumberField(TEXT("synchronous_test_planner_ms"),Milliseconds);
  J->SetNumberField(TEXT("probed_nodes"),Survey.Probed);J->SetNumberField(TEXT("path_points"),Path.Num());
  J->SetStringField(TEXT("start_cm"),P->GetActorLocation().ToString());J->SetStringField(TEXT("goal_cm"),Goal.ToString());
  J->SetStringField(TEXT("scope"),TEXT("One exact854 garage-exit pawn fixture, restored on teardown; one synchronous test-only25cm A* query. Does not certify movement, subjective handling or overall performance. No production code uses this validation planner."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Run);
  IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
  Test->TestTrue(TEXT("Planner duration receipt written"),FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("route-planner-cost.json"))));
  Test->AddInfo(FString::Printf(TEXT("Validation-only garage A*: %.3fms, %d nodes, %d points"),Milliseconds,Survey.Probed,Path.Num()));return true;
 }
private:
 FAutomationTestBase* Test;double Started=0;float At=0;bool Placed=false;FTransform Original;TWeakObjectPtr<ANeonCityPlayerCharacter> ProbePlayer;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoutePlannerCost855,"NeonCity.Phase205c.Estate.Standalone.RoutePlannerCost855",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FRoutePlannerCost855::RunTest(const FString&)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Isolated native validation only"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(RoutePlannerCost855::FProbe(this));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSummitCoverSource855,"NeonCity.Phase205c.Estate.Standalone.SummitCoverSource855",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FSummitCoverSource855::RunTest(const FString&)
{
 auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/NeonCity/Art/Phase205cUpgrade/TheOne703Landscape/T_TheOneLandscapeWeights702"));
 FString Revision=TEXT("855");FParse::Value(FCommandLine::Get(),TEXT("NeonCitySummitCoverRevision="),Revision);
 if(Revision!=TEXT("855") && Revision!=TEXT("857") && Revision!=TEXT("858")){AddError(TEXT("Explicit retained mask revision required"));return false;}
 const FString CandidatePath=Revision==TEXT("858")?TEXT("ArtSource/Phase205cUpgrade/SummitRoadCover858/T_TheOneLandscapeWeights858.png"):Revision==TEXT("857")?TEXT("ArtSource/Phase205cUpgrade/SummitRock857/T_TheOneLandscapeWeights857.png"):TEXT("ArtSource/Phase205cUpgrade/SummitCover855/T_TheOneLandscapeWeights855.png");
 FImage Native,Candidate;
 if(!TestNotNull(TEXT("Native shoulder weight texture"),Texture) ||
    !TestTrue(TEXT("Read native decoded source mip"),Texture->Source.GetMipImage(Native,0)) ||
    !TestTrue(TEXT("Decode retained candidate PNG"),FImageUtils::LoadImage(*(FPaths::ProjectDir()/CandidatePath),Candidate)))return false;
 const bool Format=Native.Format==ERawImageFormat::BGRA8 && Candidate.Format==ERawImageFormat::BGRA8;
 if(!TestTrue(TEXT("Both masks retain exact8bit BGRA source"),Format))return false;
 const bool Exact=Native.SizeX==2048 && Native.SizeY==2048 && Native.SizeX==Candidate.SizeX && Native.SizeY==Candidate.SizeY && Native.RawData==Candidate.RawData;
 TestTrue(TEXT("Native source pixels equal candidate byte-for-byte, independent of metadata"),Exact);
 FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("native_source_pixels_byte_exact"),Exact);J->SetNumberField(TEXT("decoded_bytes"),Native.RawData.Num());
 J->SetStringField(TEXT("candidate_revision"),Revision);
 J->SetStringField(TEXT("scope"),TEXT("Read-only comparison of actual UE TextureSource mip against independently decoded candidate PNG, not import metadata or file existence."));
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);
 FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));TestTrue(TEXT("Native pixel receipt written"),FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("summit-cover-source.json"))));return Exact;
}
#endif
