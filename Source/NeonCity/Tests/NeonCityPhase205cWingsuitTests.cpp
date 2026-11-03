#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/WingsuitComponent.h"
#include "Adventure/WingsuitLaunchPoint.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace RoofWingsuit
{
const FVector Origin=NeonCityCoastalLayout::FromLegacyIsland(FVector(48600,-118800,30480));
FVector M(double X,double Y,double Z){return Origin+FVector(X*100,-Y*100,Z*100);}
constexpr const TCHAR* Slot=TEXT("NeonCity_Test_WingUpgrade02");
class FNative:public IAutomationLatentCommand
{
public:
 FNative(FAutomationTestBase* T,bool Cold,bool RoofResume=false):Test(T),bCold(Cold),bRoofResume(RoofResume){}
 ~FNative(){if(Hook.IsValid())FWorldDelegates::OnWorldPreActorTick.Remove(Hook);}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!P || W->GetTimeSeconds()<4)return false;
  if(FPlatformTime::Seconds()-Started>320)return End(TEXT("Roof wingsuit native chain timed out"));
  const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(Origin))return false;
   for(TActorIterator<AWingsuitLaunchPoint> It(W);It;++It)if(It->bRoofSafetyGate)Launch=*It;
   Kit=P->FindComponentByClass<UWingsuitComponent>();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();
   if(!Launch || !Kit || !Save->ConfigureSlot(bCold?TEXT("NeonCity_Test_WingLanding04"):Slot,false))return End(TEXT("Actual roof locker and shared save slot required"));
   if(bRoofResume){if(!Save->DoesSaveExist())return End(TEXT("Actual prior roof equipment checkpoint missing"));Key(EKeys::F7,IE_Pressed);return Next(22);}
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine prior wingsuit landing checkpoint missing"));Key(EKeys::F7,IE_Pressed);return Next(20);}
   if(Save->DoesSaveExist())return End(TEXT("Retain prior wingsuit checkpoint; do not overwrite completed evidence"));
   // The only fixture is inside the public floor beside its real stairs.
   P->SetActorLocation(M(-17.45,2.15,5.18),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   for(int Floor=1;Floor<4;++Floor){const double Z=Floor*4.2;Goals.Add(M(-17.45,-3.7,Z+2.1));Goals.Add(M(-14.65,-3.7,Z+2.1));Goals.Add(M(-14.65,.8,Z+4.2));Goals.Add(M(-11.4,1.75,Z+4.2));if(Floor<3)Goals.Add(M(-17.45,1.75,Z+4.2));}
   Goals.Append({M(-11.4,8,16.8),M(-11.4,17,16.8),M(9,16.5,16.8)});
   Hook=FWorldDelegates::OnWorldPreActorTick.AddLambda([this](UWorld* World,ELevelTick,float Delta){if(World==W && Stage==8)Fly(Delta);});
   return Next(1);
  }
  if(Stage==1)
  {
   const FVector D=Goals[Waypoint]-P->GetActorLocation();
   if(D.Size2D()<35){Key(EKeys::W,IE_Released);HeldW=false;if(++Waypoint==Goals.Num())return Next(2);At=W->GetTimeSeconds();}
   else{PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Hold(EKeys::W,true,HeldW);if(Age>13)return End(FString::Printf(TEXT("Interior-to-roof walk blocked waypoint%d at%s"),Waypoint,*P->GetActorLocation().ToString()));}
  }
  if(Stage==2 && Age>.5)
  {
   if((Kit->IsEquipped() && !bRoofResume) || FMath::Abs(Launch->SafetyGate->GetRelativeRotation().Yaw)>.2)return End(TEXT("Unarmed roof gate did not remain physically closed"));
   PC->SetControlRotation((Launch->GetActorLocation()-P->GetPawnViewLocation()).Rotation());
   if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Launch)return End(TEXT("Real roof wingsuit locker F prompt inaccessible"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wing-Roof-ClosedGate.png"));Key(EKeys::F,IE_Pressed);return Next(3);
  }
  if(Stage==3 && Age>1.2)
  {
   Key(EKeys::F,IE_Released);if(!Kit->IsEquipped() || Kit->IsFlying() || FMath::Abs(Launch->SafetyGate->GetRelativeRotation().Yaw)<90)return End(TEXT("Native F equip or safety-gate animation failed"));
   Key(EKeys::F6,IE_Pressed);return Next(4);
  }
  if(Stage==4 && Age>.5)
  {
   Key(EKeys::F6,IE_Released);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Model->Player.WingsuitEquipped)return End(TEXT("Normal grounded wingsuit equipment save failed"));
   // Use the clear east side of the actual case before facing the edge.
   LaunchGoals={M(9.25,16.5,16.8),M(9.25,19.3,16.8),M(9,19.3,16.8)};return Next(23);
  }
  if(Stage==5)
  {
   if(P->GetActorLocation().Y<=M(9,19.6,16.8).Y){Key(EKeys::SpaceBar,IE_Pressed);return Next(6);}
   if(Age>5){ReportWalkContact();return End(TEXT("Actual open launch edge walk obstructed"));}
  }
  if(Stage==6 && Age>.25){Key(EKeys::SpaceBar,IE_Released);return Next(7);}
  if(Stage==7)
  {
   if(Kit->IsFlying())
   {Key(EKeys::W,IE_Released);HeldW=false;FlightStart=P->GetActorLocation();FlightAt=W->GetTimeSeconds();FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wing-Actual-Launch.png"));Key(EKeys::F6,IE_Pressed);return Next(8);}
   if(Age>5)return End(TEXT("Deliberate native jump did not enter existing wingsuit physics"));
  }
  if(Stage==8)
  {
   if(Age>.4 && !CheckedAirSave){Key(EKeys::F6,IE_Released);CheckedAirSave=true;if(Save->GetLastResult()!=ENeonCitySaveResult::UnsafeState)return End(TEXT("Airborne wingsuit incorrectly accepted normal save"));}
   if(Kit->IsCanopyOpen() && !SawCanopy){SawCanopy=true;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wing-Actual-Canopy.png"));}
   if(W->GetTimeSeconds()-LastCapture>8){LastCapture=W->GetTimeSeconds();FNeonCityVisualEvidence::CaptureHUD(W,FString::Printf(TEXT("Wing-Flight-%03d.png"),++Frames));}
   if(!Kit->IsFlying())
   {
    ReleaseFlight();if(!SawCanopy || !P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist2D(P->GetActorLocation(),Landing)>5000)return End(FString::Printf(TEXT("Native flight landed outside the clear village zone at%s canopy%d movement%d"),*P->GetActorLocation().ToString(),SawCanopy,int(P->GetCharacterMovement()->MovementMode)));
    Landed=P->GetActorLocation();FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wing-Native-Village-Landing.png"));Key(EKeys::F6,IE_Pressed);return Next(9);
   }
   if(Age>230)return End(TEXT("Canopy did not reach real village ground within descent budget"));
  }
  if(Stage==9 && Age>.5)
  {Key(EKeys::F6,IE_Released);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Model->Player.WingsuitEquipped)return End(TEXT("Real safe wingsuit landing snapshot failed"));Key(EKeys::X,IE_Pressed);return Next(10);}
  if(Stage==10 && Age>.25)
  {Key(EKeys::X,IE_Released);if(Kit->IsEquipped())return End(TEXT("Native ground X did not restore ordinary character appearance"));WalkStart=P->GetActorLocation();PC->SetControlRotation(FRotator(0,0,0));Hold(EKeys::W,true,HeldW);return Next(11);}
  if(Stage==11 && Age>.5)
  {Key(EKeys::W,IE_Released);HeldW=false;if(!P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist2D(P->GetActorLocation(),WalkStart)<75)return End(TEXT("Normal walk failed after real landing and equipment removal"));return End(TEXT(""));}
  if(Stage==20 && Age>1.2)
  {Key(EKeys::F7,IE_Released);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Kit->IsEquipped() || Kit->IsFlying() || Kit->IsCanopyOpen() || !P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist2D(P->GetActorLocation(),Landing)>5000 || FMath::Abs(Launch->SafetyGate->GetRelativeRotation().Yaw)>.2)return End(TEXT("Independent process lost grounded equipment or restored unsafe flight/gate state"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wing-Cold-Safe-GearRestore.png"));Key(EKeys::X,IE_Pressed);return Next(21);}
  if(Stage==21 && Age>.3){Key(EKeys::X,IE_Released);if(Kit->IsEquipped())return End(TEXT("Cold-restored normal X removal failed"));return End(TEXT(""));}
  if(Stage==22 && Age>1.2)
  {
   Key(EKeys::F7,IE_Released);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Kit->IsEquipped() || !P->GetCharacterMovement()->IsMovingOnGround() || FVector::Dist2D(P->GetActorLocation(),Launch->GetActorLocation())>250)return End(TEXT("Prior real interior-to-roof checkpoint failed safe restoration"));
   if(!Save->ConfigureSlot(TEXT("NeonCity_Test_WingLanding04"),false) || Save->DoesSaveExist())return End(TEXT("A fresh landing checkpoint is required; retain roof source"));
   Hook=FWorldDelegates::OnWorldPreActorTick.AddLambda([this](UWorld* World,ELevelTick,float Delta){if(World==W && Stage==8)Fly(Delta);});return Next(2);
  }
  if(Stage==23)
  {
   const FVector D=LaunchGoals[LaunchWaypoint]-P->GetActorLocation();
   if(D.Size2D()<25){Hold(EKeys::W,false,HeldW);if(++LaunchWaypoint==LaunchGoals.Num()){PC->SetControlRotation(FRotator(0,-90,0));Hold(EKeys::W,true,HeldW);return Next(5);}At=W->GetTimeSeconds();}
   else{PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));Hold(EKeys::W,true,HeldW);if(Age>5){ReportWalkContact();return End(FString::Printf(TEXT("Actual case-side launch walk blocked waypoint%d"),LaunchWaypoint));}}
  }
  return false;
 }
private:
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,EInputEvent E){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));}
 void Hold(FKey K,bool Desired,bool& Held){if(Desired!=Held){Key(K,Desired?IE_Pressed:IE_Released);Held=Desired;}}
 void ReleaseFlight(){Hold(EKeys::A,false,HeldA);Hold(EKeys::D,false,HeldD);Hold(EKeys::W,false,HeldW);Hold(EKeys::S,false,HeldS);Key(EKeys::SpaceBar,IE_Released);}
 void ReportWalkContact()
 {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(WingLaunchContact),false,P);
  for(float Height:{-45.f,0.f,45.f}){FHitResult H;const FVector From=P->GetActorLocation()+FVector(0,0,Height);if(W->LineTraceSingleByChannel(H,From,From+PC->GetControlRotation().Vector().GetSafeNormal2D()*200,ECC_Pawn,Q))UE_LOG(LogTemp,Warning,TEXT("Actual wingsuit approach contact: actor=%s component=%s point=%s normal=%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString());}
 }
 void Fly(float Delta)
 {
  if(!Kit || !Kit->IsFlying())return;
  const FVector Position=P->GetActorLocation();const float Distance=FVector::Dist2D(Position,Landing);
  FVector Desired=Landing-Position;Desired.Z=0;
  if(Kit->IsCanopyOpen() && Distance<2200)
  {
   const FVector Radial=(Position-Landing).GetSafeNormal2D(),Tangent(-Radial.Y,Radial.X,0);
   Desired=Tangent+Radial*FMath::Clamp((1600-Distance)/1300.f,-1.f,1.f);
  }
  const float Error=FMath::FindDeltaAngleDegrees(P->GetActorRotation().Yaw,Desired.Rotation().Yaw);
  TurnIntegral=FMath::Clamp(TurnIntegral+FMath::Clamp(Error/18.f,-1.f,1.f)*Delta,-.12f,.12f);
  const bool Right=TurnIntegral>Delta*.25f,Left=TurnIntegral<-Delta*.25f;
  if(Right)TurnIntegral-=Delta;if(Left)TurnIntegral+=Delta;
  Hold(EKeys::D,Right,HeldD);Hold(EKeys::A,Left,HeldA);
  // Existing native S flare keeps the deliberate launch clear of the upper
  // slope; the normal neutral glide and real canopy physics then descend.
  Hold(EKeys::S,!Kit->IsCanopyOpen() && W->GetTimeSeconds()-FlightAt<2,HeldS);
  if(!Kit->IsCanopyOpen() && Distance<3200)Key(EKeys::SpaceBar,IE_Pressed);
  else Key(EKeys::SpaceBar,IE_Released);
 }
 bool End(const FString& Failure)
 {
  if(Hook.IsValid()){FWorldDelegates::OnWorldPreActorTick.Remove(Hook);Hook.Reset();}ReleaseFlight();for(auto K:{EKeys::F,EKeys::F6,EKeys::F7,EKeys::X})Key(K,IE_Released);
  if(!Failure.IsEmpty())Test->AddError(Failure);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Failure.IsEmpty());J->SetStringField(TEXT("failure"),Failure);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("interior_roof_waypoints"),Waypoint);J->SetBoolField(TEXT("roof_checkpoint_resume"),bRoofResume);J->SetBoolField(TEXT("canopy_observed"),SawCanopy);J->SetBoolField(TEXT("airborne_save_rejected"),CheckedAirSave);J->SetStringField(TEXT("landing"),Landed.ToString());J->SetStringField(TEXT("actual_position"),P?P->GetActorLocation().ToString():TEXT("missing"));J->SetStringField(TEXT("scope"),bCold?TEXT("Independent-process genuine shared F7 load; safe ground, actual equipment, closed roof gate and native X restore. No actor placement."):bRoofResume?TEXT("Genuine F7 resume of Upgrade-PhotoWing22's actual 17-waypoint interior-to-roof equipment checkpoint. Original roof source retained; separate fresh landing slot. Native F rearm, W around case, Space jump, A/D/S physics and real canopy/landing. No post-load actor placement, direct flight-state changes or damage suppression."):TEXT("One declared interior fixture. Native W physical stairs to roof, F suit and safety gate, Space jump, native A/D and S glide control, real canopy, actual village landing, F6 grounded shared gear snapshot and X walk restoration. Existing damage remains enabled; no route teleport or direct flight state injection."));
  FString Run,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Run);IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/(bCold?TEXT("WingsuitCold.json"):TEXT("WingsuitNative.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;UWingsuitComponent* Kit=nullptr;AWingsuitLaunchPoint* Launch=nullptr;UNeonCitySaveSubsystem* Save=nullptr;
 const FVector Landing=NeonCityCoastalLayout::FromLegacyIsland(FVector(54600,-156500,412));FVector FlightStart,Landed,WalkStart;TArray<FVector> Goals,LaunchGoals;FDelegateHandle Hook;
 double Started=0;float At=0,FlightAt=0,LastCapture=0,TurnIntegral=0;int Stage=0,Waypoint=0,Frames=0;
 int32 LaunchWaypoint=0;bool bCold=false,bRoofResume=false,HeldW=false,HeldS=false,HeldA=false,HeldD=false,SawCanopy=false,CheckedAirSave=false;FNeonCityVisualEvidence Evidence{TEXT("RoofWingsuit")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoofWingsuitNative,"NeonCity.Phase205c.WingsuitNativePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRoofWingsuitNative::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RoofWingsuit::FNative(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoofWingsuitCold,"NeonCity.Phase205c.WingsuitColdPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRoofWingsuitCold::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RoofWingsuit::FNative(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoofWingsuitResume,"NeonCity.Phase205c.WingsuitLaunchResumePIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRoofWingsuitResume::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(RoofWingsuit::FNative(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
