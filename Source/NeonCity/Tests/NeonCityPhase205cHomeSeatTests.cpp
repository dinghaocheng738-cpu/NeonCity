#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Adventure/IslandHomeSeat.h"
#include "Adventure/DivingComponent.h"
#include "Presentation/TransportOccupantComponent.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Player/NeonCityPlayerDataComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Property/PropertySubsystem.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Combat/CombatComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace NativeHomeSeat
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold):Test(T),bCold(Cold){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>110)return End(TEXT("Native seat/rest/save timeout"));if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(48665,-119490,30900))))return false;
   for(TActorIterator<AIslandHomeSeat> It(W);It;++It)if(It->SeatName.ToString()==TEXT("Public lounge"))Seat=*It;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();if(!Seat || !Save->ConfigureSlot(TEXT("NeonCity_Test_HomeSeat01"),false))return End(TEXT("Actual public lounge seat/isolated save domain missing"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine seated-rest standing checkpoint required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist())return End(TEXT("Preserve prior seat checkpoint; use a fresh source slot"));
   FHitResult Floor;const FVector Setup=Seat->GetActorTransform().TransformPosition(FVector(170,0,100));FCollisionQueryParams Q(SCENE_QUERY_STAT(HomeSeatInitialFloor),false,P);Q.AddIgnoredActor(Seat);
   if(!W->LineTraceSingleByChannel(Floor,Setup+FVector(0,0,100),Setup-FVector(0,0,250),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7)return End(TEXT("Real lounge front has no supported access"));
   P->SetActorLocation(Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   // Bounded condition fixture only. This does not inject money, ownership,
   // gear, interaction state or completion; all use/save inputs are real.
   P->GetPlayerDataComponent()->SetStamina(82);return Next(1);
  }
  if(Stage==20 && Age>.8){Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || P->IsHidden() || !P->GetActorEnableCollision() || Seat->IsOccupied() || P->GetAttachParentActor() || !W->GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(Seat->PropertyID))return End(TEXT("Independent restore lost safe visible on-foot state or house ownership"));return Next(1);}
  if(Stage==1 && Age>.3)
  {
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {if(Stow.IsValid())return End(TEXT("Normal weapon stow failed"));const int Slot=Weapon->GetData()->SlotType;const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(Slot<1 || Slot>8)return End(TEXT("Weapon slot unavailable"));Stow=Slots[Slot-1];Key(Stow,true);return Next(30);}
   auto* Kit=P->FindComponentByClass<UDivingComponent>();OwnSuit=Kit->OwnsSuit();OwnDPV=Kit->OwnsDPV();CapsuleHeight=P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();Before=P->GetPlayerDataComponent()->GetStamina();
   PC->SetControlRotation((Seat->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Seat)return End(TEXT("Real lounge F prompt blocked by furniture or partition"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==30 && Age>.2){Key(Stow,false);return Next(1);}
  if(Stage==2 && Age>.3)
  {
   Key(EKeys::F,false);auto* Body=Seat->Occupant->GetDriverMesh();
   if(!Seat->IsOccupied() || Seat->Occupant->GetRepresentedDriver()!=P || !Body || !Body->IsVisible() || Body->GetAnimationMode()!=EAnimationMode::AnimationSingleNode || !P->IsHidden() || P->GetAttachParentActor()!=Seat || PC->GetPawn()!=P || !P->GetActorEnableCollision() || P->GetCapsuleComponent()->GetCollisionEnabled()!=ECollisionEnabled::QueryOnly)return End(TEXT("Actual sitting animation/body or preserved local possession/damage-query capsule missing"));
   const FVector Hip=Seat->GetActorTransform().TransformPosition(Seat->SeatHipLocal);if(FVector::Distance(Body->GetBoneLocation(TEXT("pelvis")),Hip)>6)return End(TEXT("Visible seated pelvis does not meet the real chair anchor"));
   FNeonCityVisualEvidence::CaptureHUD(W,bCold?TEXT("HomeSeat-Cold-Normal-View.png"):TEXT("HomeSeat-Native-Normal-View.png"));
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=65;const FVector Eye=Seat->GetActorTransform().TransformPosition(FVector(220,-190,150));const FVector Aim=Seat->GetActorTransform().TransformPosition(FVector(0,0,95));Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PC->SetViewTarget(Camera);return Next(3);
  }
  if(Stage==3 && Age>(bCold?3:7))
  {
   if(!bCold && P->GetPlayerDataComponent()->GetStamina()<Before+6)return End(TEXT("Actual timed rest did not recover shared stamina"));
   FNeonCityVisualEvidence::CaptureHUD(W,bCold?TEXT("HomeSeat-Cold-Seated-Close.png"):TEXT("HomeSeat-Native-Seated-Close.png"));PC->SetViewTarget(P);Camera->Destroy();Camera=nullptr;
   if(bCold){Key(EKeys::W,true);return Next(9);}if(!Seat->FindPersistenceExit(Safe))return End(TEXT("Actual unoccupied standing save exit missing"));Key(EKeys::F6,true);return Next(4);
  }
  if(Stage==4 && Age>.8)
  {
   Key(EKeys::F6,false);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || FVector::Dist(Model->Player.Transform.GetLocation(),Safe.GetLocation())>1 || Model->Player.OwnsR05!=OwnSuit || Model->Player.OwnsDPV!=OwnDPV || !Seat->IsOccupied())return End(TEXT("Genuine F6 seated save lost safe standing point or existing equipment ownership"));Key(EKeys::F7,true);return Next(5);
  }
  if(Stage==5 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || Seat->IsOccupied() || P->IsHidden() || !P->GetActorEnableCollision() || P->GetAttachParentActor() || FMath::Abs(P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-CapsuleHeight)>.01 || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("F7 failed to release the transient seat and restore normal standing capsule/input"));
   PC->SetControlRotation((Seat->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());if(P->GetInteractionComponent()->RefreshFocusedInteractable()!=Seat)return End(TEXT("Replayed real seat prompt unavailable"));Key(EKeys::F,true);return Next(6);
  }
  if(Stage==6 && Age>.3){Key(EKeys::F,false);if(!Seat->IsOccupied())return End(TEXT("Second normal sit failed"));Key(EKeys::W,true);return Next(9);}
  if(Stage==9 && Age>.3)
  {
   Key(EKeys::W,false);if(Seat->IsOccupied() || P->IsHidden() || !P->GetActorEnableCollision() || P->GetAttachParentActor() || FMath::Abs(P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-CapsuleHeight)>.01 || !P->GetCharacterMovement()->IsMovingOnGround())return End(TEXT("Native W did not stand and restore normal capsule/input"));FNeonCityVisualEvidence::CaptureHUD(W,bCold?TEXT("HomeSeat-Cold-W-Stand.png"):TEXT("HomeSeat-Native-W-Stand.png"));return End(TEXT(""));
  }
  return false;
 }
 bool Next(int S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Error)
 {
  if(PC){for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(P)PC->SetViewTarget(P);}if(Camera)Camera->Destroy();if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("stamina_before"),Before);J->SetNumberField(TEXT("stamina_after"),P?P->GetPlayerDataComponent()->GetStamina():0);J->SetStringField(TEXT("scope"),bCold?TEXT("Independent genuine F7 restore of seated-save standing checkpoint/property/gear, native F sit, real sitting animation, visible body close evidence and mapped W stand. Zero pose/state fixtures."):TEXT("One supported lounge-start player fixture and declared82-stamina exhaustion fixture only. Native weapon stow, F sit, actual seated skeleton, shared timed recovery, damage-query capsule retained, F6 saves actual clear standing point, F7 releases transient seat, repeated F/W stand restores original collision/input. No money/property/gear/reward/completion fixture. Bed sleep/cinema/gym/sauna are separate incomplete facilities."));
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/TEXT("HomeSeatNative");IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));return true;
 }
 FAutomationTestBase* Test;bool bCold;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandHomeSeat* Seat=nullptr;UNeonCitySaveSubsystem* Save=nullptr;ACameraActor* Camera=nullptr;TMap<FKey,bool> Keys;FKey Stow;FTransform Safe;int Stage=0;float At=0,Before=0,CapsuleHeight=0;double Start=0;bool OwnSuit=false,OwnDPV=false;FNeonCityVisualEvidence Evidence{TEXT("NativeHomeSeat")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeSeatNativeTest,"NeonCity.Phase205c.HomeSeat.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeSeatNativeTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeSeat::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeSeatColdTest,"NeonCity.Phase205c.HomeSeat.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeSeatColdTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeSeat::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
