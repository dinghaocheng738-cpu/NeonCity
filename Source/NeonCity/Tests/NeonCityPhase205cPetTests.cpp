#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandHomePet.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
namespace PetNative
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold),Evidence(Cold?TEXT("PetCold"):TEXT("PetSource")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>90)return End(TEXT("Native pet interaction/actual motion timed out"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(49050,-119500,30952))))return false;for(TActorIterator<AIslandHomePet> I(W);I;++I)Pet=*I;Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Pet || !Pet->GetMesh()->GetSkeletalMeshAsset() || !Pet->IdleAnimation || !Pet->TrotAnimation || !Save->ConfigureSlot(TEXT("NeonCity_Test_Pet01"),false))return End(TEXT("Native pet mesh/clips/shared save missing"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine prior pet F6 required"));Key(EKeys::F7,true);return Next(10);}if(Save->DoesSaveExist())return End(TEXT("Retain previous pet checkpoint; unchanged source cannot rerun"));
   bool Clear=false;FCollisionQueryParams Q(SCENE_QUERY_STAT(PetActualPlayerStart),false,P);Q.AddIgnoredActor(Pet);
   for(const FVector Delta:{FVector(-150,0,0),FVector(150,0,0),FVector(0,150,0),FVector(0,-150,0)})
   {FHitResult H;const FVector Around=Pet->GetActorLocation()+Delta;if(!W->LineTraceSingleByChannel(H,Around+FVector(0,0,100),Around-FVector(0,0,150),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)continue;const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))continue;P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();Clear=true;break;}
   if(!Clear)return End(TEXT("Companion has no actual collision-clear supported on-foot interaction approach"));return Next(1);
  }
  if(Stage==10 && Age>.8)
  {
   Key(EKeys::F7,false);const auto* M=Save->ReadSaveForInspection();const auto D=Pet->BuildPersistenceSnapshot();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->HasResidenceLife || D.PetID!=M->ResidenceLife.Pet.PetID || !D.Owned || D.Mode!=2 || FVector::Dist(D.Transform.GetLocation(),M->ResidenceLife.Pet.Transform.GetLocation())>5 || P->IsHidden() || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Independent F7 lost stable pet identity/stay/physical pose or safe player"));
   for(TActorIterator<AIslandHomePet> I(W);I;++I)++PetCount;if(PetCount!=1)return End(TEXT("Cold load duplicated the household pet"));Hold=Pet->GetActorLocation();FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pet-Cold-Stable-Saved-Stay.png"));return Next(11);
  }
  if(Stage==11 && Age>3){if(FVector::Dist(Pet->GetActorLocation(),Hold)>8)return End(TEXT("Cold pet stay moved without input"));return End(TEXT(""));}
  if(Stage==1 && Age>.3){if(!Focus())return End(TEXT("Native F companion prompt inaccessible"));Key(EKeys::F,true);return Next(2);}
  if(Stage==2 && Age>.25)
  {Key(EKeys::F,false);if(Pet->GetActivityMode()!=1)return End(TEXT("Mapped F did not select follow"));From=Pet->GetActorLocation();PlayerFrom=P->GetActorLocation();PC->SetControlRotation(FRotator(0,90,0));Key(EKeys::W,true);return Next(3);}
  if(Stage==3)
  {
   if(Age<1.75)return false;Key(EKeys::W,false);return Next(4);
  }
  if(Stage==4 && Age>1)
  {
   FollowDistance=FVector::Dist2D(From,Pet->GetActorLocation());if(FVector::Dist2D(PlayerFrom,P->GetActorLocation())<250 || FollowDistance<120 || Pet->GetCharacterMovement()->IsFalling() || Pet->GetCharacterMovement()->IsSwimming() || FVector::Dist2D(P->GetActorLocation(),Pet->GetActorLocation())>230)return End(FString::Printf(TEXT("Actual W/follow movement or supported reach failed player%.2f pet%.2f gap%.2f"),FVector::Dist2D(PlayerFrom,P->GetActorLocation()),FollowDistance,FVector::Dist2D(P->GetActorLocation(),Pet->GetActorLocation())));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pet-Native-Actual-Follow.png"));if(!Focus())return End(TEXT("Real followed pet cannot receive F stay"));Key(EKeys::F,true);return Next(5);
  }
  if(Stage==5 && Age>.25){Key(EKeys::F,false);if(Pet->GetActivityMode()!=2)return End(TEXT("Mapped F did not select stay"));Hold=Pet->GetActorLocation();return Next(6);}
  if(Stage==6 && Age>3)
  {if(FVector::Dist(Pet->GetActorLocation(),Hold)>8)return End(TEXT("Stay mode failed to stop actual companion"));Key(EKeys::F6,true);return Next(7);}
  if(Stage==7 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->HasResidenceLife || M->ResidenceLife.Pet.PetID!=Pet->PetID || M->ResidenceLife.Pet.Mode!=2 || FVector::Dist(M->ResidenceLife.Pet.Transform.GetLocation(),Hold)>5)return End(TEXT("Real F6 failed existing residence pet save domain"));if(!Focus())return End(TEXT("Stay pet home interaction blocked"));Key(EKeys::F,true);return Next(8);
  }
  if(Stage==8 && Age>.25){Key(EKeys::F,false);if(Pet->GetActivityMode()!=0)return End(TEXT("Mapped F did not resume home activity"));From=Pet->GetActorLocation();return Next(9);}
  if(Stage==9 && Age>8){HomeDistance=FVector::Dist2D(From,Pet->GetActorLocation());if(HomeDistance<50 || Pet->GetCharacterMovement()->IsFalling() || Pet->GetCharacterMovement()->IsSwimming())return End(TEXT("Home activity failed actual supported household movement"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Pet-Native-Home-Activity.png"));return End(TEXT(""));}
  return false;
 }
 bool Focus(){PC->SetControlRotation((Pet->GetActorLocation()-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Pet;}
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Held=Keys.FindOrAdd(K);if(Held!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/(bCold?TEXT("PetCold"):TEXT("PetSource"));IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(FString::Printf(TEXT("passed=%d\nstage=%d\nfailure=%s\nfollow_displacement_cm=%.3f\nhome_displacement_cm=%.3f\nOnly A has one supported player start. Mapped F/W/F/F6/home; ordinary CMC and full-body floor/obstacle guards, existing residence persistence. Cold B genuine F7 only. No pet pose/state/input/reward injection. Realistic hero mesh/true walk clip quality remains PARTIAL independently of this mechanism.\n"),Error.IsEmpty(),Stage,*Error,FollowDistance,HomeDistance),*(Dir/TEXT("scope.txt")));return true;
 }
 FAutomationTestBase* Test;bool bCold;int Stage=0,PetCount=0;double Start=0;float At=0,FollowDistance=0,HomeDistance=0;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandHomePet* Pet=nullptr;UNeonCitySaveSubsystem* Save=nullptr;FVector From,PlayerFrom,Hold;TMap<FKey,bool> Keys;FNeonCityVisualEvidence Evidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetSource,"NeonCity.Phase205c.Pet.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPetSource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PetNative::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPetCold,"NeonCity.Phase205c.Pet.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPetCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PetNative::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
