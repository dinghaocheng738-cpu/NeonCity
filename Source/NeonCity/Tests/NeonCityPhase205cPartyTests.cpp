#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandPartyControl.h"
#include "AI/NeonCityNPCCharacter.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
namespace PartyNative
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold),Evidence(Cold?TEXT("PartyCold"):TEXT("PartySource")){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>145)return End(TEXT("Actual gathering circulation or ending timed out"));if(!P || W->GetTimeSeconds()<4)return false;Evidence.Record(W,PC,Stage);const float Age=W->GetTimeSeconds()-At;
  if(Stage==0)
  {
   auto* R=ANeonCityCoastalRegion::Find(W);if(!R || !R->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(47800,-121000,31000))))return false;
   for(TActorIterator<AIslandPartyControl> I(W);I;++I)Party=*I;Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Party || !Save->ConfigureSlot(TEXT("NeonCity_Test_Party01"),false))return End(TEXT("Authored party and isolated save missing"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine independent gathering F6 source required"));Key(EKeys::F7,true);return Next(10);}
   if(Save->DoesSaveExist())return End(TEXT("Retain genuine prior gathering save; no unchanged repeat"));
   const FVector Around=Party->GetActorLocation()+FVector(88.4,-88.4,0);FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(PartyHostStanding),false,P);
   if(!W->LineTraceSingleByChannel(H,Around+FVector(0,0,180),Around-FVector(0,0,100),ECC_Pawn,Q) || H.ImpactNormal.Z<.7)return End(TEXT("Host approach has no actual deck floor"));
   const FVector Stand=H.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);
   if(W->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q))return End(TEXT("Host approach circulation obstructed"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();return Next(1);
  }
  if(Stage==10 && Age>.8)
  {
   Key(EKeys::F7,false);const auto* M=Save->ReadSaveForInspection();const auto D=Party->BuildPersistenceSnapshot();
   if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !Party->IsActive() || Party->GetGuestCount()!=4 || D.Guests.Num()!=4 || M->ResidenceLife.Party.Guests.Num()!=4)return End(TEXT("Independent F7 failed to restore four stable adult guests"));
   for(const auto& G:D.Guests){const auto* Old=M->ResidenceLife.Party.Guests.FindByPredicate([&](const auto& V){return V.GuestID==G.GuestID;});if(!Old || FVector::Dist(Old->Transform.GetLocation(),G.Transform.GetLocation())>150)return End(TEXT("Guest identity or physical saved position lost"));}
   return Next(2);
  }
  if(Stage==1 && Age>.4)
  {if(!Focus())return End(TEXT("Normal F gathering invitation prompt blocked"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Party-Native-Invitation.png"));Key(EKeys::F,true);return Next(2);}
  if(Stage==2)
  {
   if(Age>.25)Key(EKeys::F,false);
   if(!Party->IsActive() || !Party->Music->IsPlaying())return Age<1?false:End(TEXT("Actual F failed party/music start"));
   for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I)if(I->GetNPCId_Implementation().ToString().StartsWith(TEXT("Aurelia.AdultGuest.")))
   {
    const FName ID=I->GetNPCId_Implementation();if(const FVector* Last=Positions.Find(ID)){Distance.FindOrAdd(ID)+=FVector::Dist2D(*Last,I->GetActorLocation());}Positions.Add(ID,I->GetActorLocation());
    const auto* Walking=I->GetCharacterMovement();const auto& Floor=Walking->CurrentFloor;
    // Validate the real CharacterMovement capsule floor. A point ray through
    // a timber joint can hit a bevel below its physically supported foot.
    if(!Walking->IsMovingOnGround() || Walking->IsFalling() || Walking->IsSwimming() || !Floor.IsWalkableFloor() || Floor.HitResult.ImpactNormal.Z<.7 || !Floor.HitResult.GetActor())return End(TEXT("Guest lost normal supported deck walking"));
   }
   if(Age<26)return false;
   if(Party->GetGuestCount()!=4 || Positions.Num()!=4)return End(TEXT("Actual staggered arrivals failed to produce four adult guests"));
   for(const auto& Pair:Distance)if(Pair.Value<150){if(Age<70)return false;return End(TEXT("A party visitor did not really walk the deck within bounded staggered arrival time"));}
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Party-Native-Actual-Guests.png"));if(bCold){if(!Focus())return End(TEXT("Normal F end-gathering prompt blocked"));Key(EKeys::F,true);return Next(5);}
   Key(EKeys::F6,true);return Next(3);
  }
  if(Stage==3 && Age>.8)
  {
   Key(EKeys::F6,false);const auto* M=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !M || !M->HasResidenceLife || !M->ResidenceLife.Party.Active || M->ResidenceLife.Party.Guests.Num()!=4 || M->ResidenceLife.Party.NextArrival!=4)return End(TEXT("Genuine F6 failed gathering shared persistence"));
   if(!Focus())return End(TEXT("Native party end focus blocked"));Key(EKeys::F,true);return Next(5);
  }
  if(Stage==5 && Age>.25){Key(EKeys::F,false);if(Party->IsActive())return End(TEXT("Actual F did not end gathering"));return Next(6);}
  if(Stage==6)
  {
   if(Party->IsEnding() || Party->GetGuestCount()>0)return false;
   if(Party->Music->IsPlaying())return End(TEXT("Ended gathering music not stopped"));for(TActorIterator<ANeonCityNPCCharacter> I(W);I;++I)if(I->GetNPCId_Implementation().ToString().StartsWith(TEXT("Aurelia.AdultGuest.")))return End(TEXT("Ended gathering left orphaned guests"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Party-Native-Ended-Clear-Deck.png"));return End(TEXT(""));
  }
  return false;
 }
 bool Focus(){PC->SetControlRotation((Party->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());return P->GetInteractionComponent()->RefreshFocusedInteractable()==Party;}
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Held=Keys.FindOrAdd(K);if(Held!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Held=Down;}}
 bool End(const FString& Error)
 {
  if(PC)for(const auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(!Error.IsEmpty())Test->AddError(Error);
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Label/(bCold?TEXT("PartyCold"):TEXT("PartySource"));IFileManager::Get().MakeDirectory(*Dir,true);if(Party)Party->WriteReadOnlyDiagnostics(Dir,true);FString Receipt=FString::Printf(TEXT("passed=%d\nfailure=%s\nstage=%d\nNormal F start/end, four real adult character trajectories and supported floor, original spatial music, shared genuine F6; independent B only F7/no fixture. No guest teleport after arrival, activity/reward/elapsed injection. This limited gathering supplies walking/rest; dance/swim/bikini presentation is not claimed.\n"),Error.IsEmpty(),*Error,Stage);for(const auto& D:Distance)Receipt+=D.Key.ToString()+FString::Printf(TEXT("=%.3fcm\n"),D.Value);FFileHelper::SaveStringToFile(Receipt,*(Dir/TEXT("scope.txt")));return true;
 }
 FAutomationTestBase* Test;bool bCold;int Stage=0;double Start=0;float At=0;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandPartyControl* Party=nullptr;UNeonCitySaveSubsystem* Save=nullptr;TMap<FKey,bool> Keys;TMap<FName,FVector> Positions;TMap<FName,float> Distance;FNeonCityVisualEvidence Evidence;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySource,"NeonCity.Phase205c.Party.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPartySource::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PartyNative::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyCold,"NeonCity.Phase205c.Party.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPartyCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(PartyNative::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
