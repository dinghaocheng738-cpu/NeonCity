#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Vehicles/Marine/NeonCityMotorYacht.h"
#include "Vehicles/Marine/MarineVehicleData.h"
#include "Vehicles/Marine/MarineVehicleComponent.h"
#include "Vehicles/Marine/MarineInteractionComponent.h"
#include "Core/Persistence/NeonCityMarineDeckRestore.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageableCharacterComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace MotorYacht814
{
class FFlow:public IAutomationLatentCommand
{
public:explicit FFlow(FAutomationTestBase* T,bool DockReturnOnly=false):Test(T),DockOnly(DockReturnOnly){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();PC=W?W->GetFirstPlayerController():nullptr;
  if(FPlatformTime::Seconds()-Started>150)return End(TEXT("Yacht local native test timed out"));if(!PC || W->GetTimeSeconds()<5)return false;
  const float Now=W->GetTimeSeconds(),Age=Now-At;
  if(Boat && (Boat->GetHealthComponent()->GetCurrentHealth()<InitialHealth-.1 || FMath::Abs(Boat->GetActorRotation().Roll)>20 || FMath::Abs(Boat->GetActorRotation().Pitch)>20))return End(TEXT("Yacht lost health or stable support"));
  if(Stage==0)
  {
   Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(Region->GetFishingBerth().GetLocation()))return false;
   for(TActorIterator<ANeonCityMotorYacht> I(W);I;++I){if(Boat)return End(TEXT("Unique authored yacht required"));Boat=*I;}
   Player=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());if(!Boat || !Player || !Boat->YachtBody->GetStaticMesh() || !Boat->YachtGlass->GetStaticMesh())return End(TEXT("Actual authored yacht mesh/player missing"));
   InitialHealth=Boat->GetHealthComponent()->GetCurrentHealth();Berth=Boat->GetActorTransform();Forward=Boat->GetActorForwardVector().GetSafeNormal2D();
   Player->SetActorLocation(Region->GetActorLocation()+FVector(4845,-3750,330),false,nullptr,ETeleportType::TeleportPhysics);Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);return Next(1);
  }
  if(Stage==1 && Age>4)
  {
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Boat->GetMarineMovement()->IsAfloat() || !Boat->GetMarineMovement()->IsMoored() || Boat->GetSpeed_Implementation()>20)return End(TEXT("Authored pier approach/mooring did not settle"));
   // Find a visible point on the actual authored hull inside the unchanged
   // normal player trace distance. Do not aim at the underwater actor origin.
   FVector Eye;FRotator View;Player->GetActorEyesViewPoint(Eye,View);
   auto* Interaction=Player->GetInteractionComponent();float Closest=Interaction->InteractionDistance+1;FVector Aim;TMap<FString,int32> Obstructions;
   for(float X:{-200.f,-100.f,0.f,100.f,200.f})for(float Y:{160.f,140.f,120.f,100.f,80.f,60.f,40.f,20.f,0.f})for(float Z=120;Z<=320;Z+=10)
   {
    const FVector Direction=(Boat->GetActorTransform().TransformPosition(FVector(X,Y,Z))-Eye).GetSafeNormal();FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(YachtPierVisibility),false,Player);
    if(W->LineTraceSingleByChannel(Hit,Eye,Eye+Direction*Interaction->InteractionDistance,Interaction->TraceChannel,Query))
    {if(Hit.GetActor()==Boat && Hit.Distance<Closest){Closest=Hit.Distance;Aim=Direction;}else ++Obstructions.FindOrAdd(GetNameSafe(Hit.GetActor())+TEXT("/")+GetNameSafe(Hit.GetComponent()));}
   }
   const FString Conditions=FString::Printf(TEXT("eye=%s boat=%s profile=%s reach=%.1f distance=%.1f velocity=%s authority=%d boat_controller=%s driver=%s parent=%s local=%d can_act=%d walkable=%d"),*Eye.ToString(),*Boat->GetActorLocation().ToString(),*Boat->GetProfile()->VehicleType.ToString(),Boat->GetProfile()->InteractionReachCm,FVector::Dist(Boat->GetActorLocation(),Player->GetActorLocation()),*Boat->GetVelocity().ToString(),Boat->HasAuthority(),*GetNameSafe(Boat->GetController()),*GetNameSafe(Boat->GetMarineInteraction()->GetDriver()),*GetNameSafe(Player->GetAttachParentActor()),PC->IsLocalController(),Player->FindComponentByClass<UDamageableCharacterComponent>()->CanAct(),Boat->GetProfile()->bWalkableDeck);
   if(Closest>Interaction->InteractionDistance){for(const auto& O:Obstructions)Test->AddInfo(FString::Printf(TEXT("Yacht boarding view blocked by %s %d rays"),*O.Key,O.Value));return End(FString::Printf(TEXT("No actual yacht surface in normal %.1fcm view trace; %s; body_bounds=%s"),Interaction->InteractionDistance,*Conditions,*Boat->YachtBody->Bounds.ToString()));}
   PC->SetControlRotation(Aim.Rotation());
   if(Interaction->RefreshFocusedInteractable()!=Boat)return End(TEXT("Actual hull point failed normal focused interaction; ")+Conditions);
   Test->AddInfo(FString::Printf(TEXT("Native yacht boarding hull hit %.1fcm inside normal %.1fcm reach"),Closest,Interaction->InteractionDistance));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Yacht814-Pier.png"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==2 && Age>.3)
  {
   Key(EKeys::F,false);if(PC->GetPawn()!=Boat || !Boat->GetMarineMovement()->IsEngineRunning())return End(TEXT("Normal yacht F/engine entry failed"));
   Start=Boat->GetActorLocation();return Next(DockOnly?20:3);
  }
  if(Stage==20 && Age>.3){Boat->SubmitDriverInput_Implementation(PC,0,0,1);Key(EKeys::F,true);return Next(21);}
  if(Stage==21 && Age>.3){Key(EKeys::F,false);if(PC->GetPawn()!=Player)return End(TEXT("Normal moored F disembarkation failed"));return Next(22);}
  if(Stage==22 && Age>1)
  {
   const auto* Base=Player->GetMovementBase();
   if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Base || Base->GetOwner()==Boat)return End(TEXT("Moored F left player on yacht deck instead of supported pier"));
   WalkStart=Player->GetActorLocation();PC->SetControlRotation(FRotator(0,0,0));Key(EKeys::W,true);return Next(23);
  }
  if(Stage==23)
  {
   if(Player->GetActorLocation().X-WalkStart.X>200)
   {Key(EKeys::W,false);const auto* Base=Player->GetMovementBase();if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Base || Base->GetOwner()==Boat)return End(TEXT("Moored pier walking lost native support"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Yacht831-ShoreWalk.png"));return End(TEXT(""));}
   if(Age>4)return End(TEXT("Normal W could not walk inward on the pier after F exit"));return false;
  }
  if(Stage==3 || Stage==5)
  {
   const float Along=FVector::DotProduct(Boat->GetActorLocation()-Start,Forward);PeakSpeed=FMath::Max(PeakSpeed,Boat->GetSpeed_Implementation());MaximumAlong=FMath::Max(MaximumAlong,Along);
   const float Heading=FMath::FindDeltaAngleDegrees(Boat->GetActorRotation().Yaw,Berth.Rotator().Yaw);
   Boat->SubmitDriverInput_Implementation(PC,Stage==3?.32f:-.22f,FMath::Clamp(Heading*(Stage==3?1.f:-1.f)/12.f,-.35f,.35f),0);
   if(Stage==3 && Along>3000){Boat->SubmitDriverInput_Implementation(PC,0,0,1);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Yacht814-Driving.png"));return Next(4);}
   if(Stage==5 && Along<260){Boat->SubmitDriverInput_Implementation(PC,0,0,1);return Next(6);}
   if(Age>65)return End(TEXT("Native yacht throttle/reverse made insufficient progress"));return false;
  }
  if(Stage==4 && Boat->GetSpeed_Implementation()<15)
  {
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();FString Slot;
   if(!Save || !FParse::Value(FCommandLine::Get(),TEXT("NeonCityYachtSlot="),Slot) || !Slot.StartsWith(TEXT("NeonCity_Test_Yacht")) || !Save->ConfigureSlot(Slot,false) || Save->DoesSaveExist())return End(TEXT("Dedicated fresh yacht save slot required; existing files preserved"));
   if(!Save->SaveGame())return End(TEXT("Actual yacht sea save failed: ")+Save->GetLastDetail());return Next(10);
  }
  if(Stage==10){if(Save->IsOperationInProgress())return false;if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Yacht sea save did not complete: ")+Save->GetLastDetail());return Next(5);}
  if(Stage==6 && Boat->GetSpeed_Implementation()<15)
  {
   if(FVector::Dist2D(Boat->GetActorLocation(),Start)>350)return End(TEXT("Yacht returned outside its local berth approach"));
   FTransform Standing;if(!Boat->GetMarineInteraction()->FindExitTransform(Standing))return End(TEXT("Actual source deck exit transform absent"));
   FNeonCityMarineSaveData Record;Record.VehicleID=Boat->VehicleID;Record.DataAssetID=Boat->GetProfile()->VehicleType;Record.Owned=Boat->Owned;Record.Condition=1;Record.Transform=Boat->GetActorTransform();
   if(!NeonCityValidateSavedMarineDeck(W,Player,Record,Standing))return End(TEXT("Actual yacht saved deck/capsule validation failed"));
   auto Roof=Standing;Roof.SetLocation(Boat->GetActorTransform().TransformPosition(FVector(0,0,300)));if(NeonCityValidateSavedMarineDeck(W,Player,Record,Roof))return End(TEXT("Yacht roof incorrectly accepted as saved deck"));
   Key(EKeys::V,true);return Next(7);
  }
  if(Stage==7 && Age>.3){Key(EKeys::V,false);if(!Boat->IsHelmCamera())return End(TEXT("Normal yacht V helm camera failed"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Yacht814-Helm.png"));Key(EKeys::F,true);return Next(8);}
  if(Stage==8 && Age>.3){Key(EKeys::F,false);if(PC->GetPawn()!=Player || Boat->GetMarineInteraction()->GetDriver())return End(TEXT("Yacht F deck exit failed"));return Next(9);}
  if(Stage==9 && Age>1)
  {
   const auto* Base=Player->GetMovementBase();if(!Player->GetCharacterMovement()->IsMovingOnGround() || !Base || Base->GetOwner()!=Boat)return End(TEXT("Yacht exit has no actual supported source deck"));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Yacht814-DeckExit.png"));return End(TEXT(""));
  }
  return false;
 }
private:
 bool Next(int32 S){Stage=S;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){if(PC)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));}
 bool End(FString Error)
 {
  for(FKey K:{EKeys::F,EKeys::V,EKeys::W})Key(K,false);if(Boat)Boat->SubmitDriverInput_Implementation(PC,0,0,1);if(!Error.IsEmpty()){Test->AddError(Error);if(W)FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Yacht814-Failure.png"));}
  FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("maximum_outward_cm"),MaximumAlong);J->SetNumberField(TEXT("peak_speed_cm_s"),PeakSpeed);J->SetNumberField(TEXT("initial_health"),InitialHealth);J->SetNumberField(TEXT("final_health"),Boat?Boat->GetHealthComponent()->GetCurrentHealth():0);J->SetStringField(TEXT("scope"),DockOnly?TEXT("Independent local pier/player start fixture. Unmodified authored yacht; real F boarding, real F dock disembarkation and mapped W supported pier walking only. Prior driving and cold restore are cached, not repeated."):TEXT("Independent local pier/player fixture. Authored yacht unmodified; real F, normal public marine input, outward/reverse approach, V and supported source-deck F exit, genuine sea save. Cold restore and full island route are separate."));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label)/(DockOnly?TEXT("MotorYachtDock831.json"):TEXT("MotorYacht814.json"))));return true;
 }
 FAutomationTestBase* Test;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* Player=nullptr;ANeonCityMotorYacht* Boat=nullptr;ANeonCityCoastalRegion* Region=nullptr;UNeonCitySaveSubsystem* Save=nullptr;FTransform Berth;FVector Start,Forward,WalkStart;double Started=0;float At=0,InitialHealth=0,PeakSpeed=0,MaximumAlong=0;int32 Stage=0;bool DockOnly=false;
};
class FColdRestore:public IAutomationLatentCommand
{
public:explicit FColdRestore(FAutomationTestBase* T):Test(T){}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;if(FPlatformTime::Seconds()-Started>80)return End(TEXT("Yacht cold restore timed out"));if(!PC || W->GetTimeSeconds()<5)return false;
  auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(Region->GetFishingBerth().GetLocation()))return false;
  if(!Requested)
  {
   for(TActorIterator<ANeonCityMotorYacht> I(W);I;++I)Boat=*I;if(!Boat)return End(TEXT("Authored yacht absent"));Berth=Boat->GetActorTransform();Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();FString Slot;
   if(!FParse::Value(FCommandLine::Get(),TEXT("NeonCityYachtSlot="),Slot) || !Slot.StartsWith(TEXT("NeonCity_Test_Yacht")) || !Save->ConfigureSlot(Slot,false) || !Save->DoesSaveExist())return End(TEXT("Existing dedicated real-driving yacht save required"));
   auto* Model=Save->ReadSaveForInspection();if(!Model)return End(TEXT("Real yacht save unreadable"));const auto* Record=Model->Marine.FindByPredicate([&](const auto& R){return R.VehicleID==Boat->VehicleID;});if(!Record)return End(TEXT("Yacht identity absent in sea save"));Expected=*Record;ExpectedPlayer=Model->Player.Transform;
   if(FVector::Dist2D(Expected.Transform.GetLocation(),Berth.GetLocation())<2000)return End(TEXT("Input is not the actual outbound sea save"));Requested=true;if(!Save->LoadGame())return End(TEXT("Yacht cold load refused: ")+Save->GetLastDetail());return false;
  }
  if(Save->IsOperationInProgress())return false;if(Save->GetLastResult()!=ENeonCitySaveResult::Success)return End(TEXT("Yacht cold load failed: ")+Save->GetLastDetail());if(!RestoredAt)RestoredAt=W->GetTimeSeconds();if(W->GetTimeSeconds()-RestoredAt<1)return false;
  auto* P=Cast<ANeonCityPlayerCharacter>(PC->GetPawn());const auto* Base=P?P->GetMovementBase():nullptr;
  if(!P || !P->GetCharacterMovement()->IsMovingOnGround() || !Base || Base->GetOwner()!=Boat || FVector::Dist2D(Boat->GetActorLocation(),Expected.Transform.GetLocation())>50 || FVector::Dist(P->GetActorLocation(),ExpectedPlayer.GetLocation())>60 || Boat->GetMarineMovement()->IsMoored())return End(TEXT("Cold restore lost source deck, saved pose or sea mooring state"));
  ColdPassed=true;
  // Independent production-load fixture for the reviewed 1.5–8m mooring bug.
  // No further driving evidence is inferred after this explicit fixture.
  auto Near=Expected;Near.Transform=Berth;Near.Transform.AddToTranslation(Berth.GetRotation().GetForwardVector()*300);Near.Transform.SetRotation(FRotator(0,Berth.Rotator().Yaw+15,0).Quaternion());
  W->GetSubsystem<UMarineWorldSubsystem>()->ApplyPersistenceSnapshot({Near});
  if(Boat->GetMarineMovement()->IsMoored() || !Boat->GetActorTransform().Equals(Near.Transform,.01))return End(TEXT("Three-metre saved pose incorrectly reattached to authored mooring"));return End(TEXT(""));
 }
private:
 bool End(FString Error){if(!Error.IsEmpty())Test->AddError(Error);FString Label;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetBoolField(TEXT("actual_sea_save_cold_restore_passed"),ColdPassed);J->SetBoolField(TEXT("three_metre_persistence_fixture_passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetStringField(TEXT("scope"),TEXT("Fresh process loads actual prior native-driving sea save and checks player source deck/boat pose. Afterwards, a separate explicit three-metre snapshot fixture tests production ApplyPersistenceSnapshot mooring classification; no route proof is inferred from that fixture."));FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("IslandLandPass1")/FPaths::MakeValidFileName(Label)/TEXT("MotorYachtCold815.json")));return true;}
 FAutomationTestBase* Test;UWorld* W=nullptr;ANeonCityMotorYacht* Boat=nullptr;UNeonCitySaveSubsystem* Save=nullptr;FNeonCityMarineSaveData Expected;FTransform ExpectedPlayer,Berth;double Started=0;float RestoredAt=0;bool Requested=false,ColdPassed=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotorYacht814,"NeonCity.Phase205c.Estate.Standalone.MotorYacht814",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMotorYacht814::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Local yacht test must preserve saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MotorYacht814::FFlow(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotorYachtDock831,"NeonCity.Phase205c.Estate.Standalone.MotorYachtDock831",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMotorYachtDock831::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Dock exit test must preserve saves"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MotorYacht814::FFlow(this,true));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotorYachtCold815,"NeonCity.Phase205c.Estate.Standalone.MotorYachtCold815",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FMotorYachtCold815::RunTest(const FString&){if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence"))){AddError(TEXT("Cold yacht test must preserve player slots"));return false;}ADD_LATENT_AUTOMATION_COMMAND(MotorYacht814::FColdRestore(this));return true;}
#endif
