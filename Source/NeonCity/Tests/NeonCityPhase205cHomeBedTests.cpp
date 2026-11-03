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
#include "World/NeonCityDayNightCycle.h"
#include "Adventure/IslandLandLight.h"
#include "Components/PointLightComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/Weapons/WeaponBase.h"
#include "Combat/Weapons/WeaponData.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace NativeHomeBed
{
class FFlow:public IAutomationLatentCommand
{
public:
 FFlow(FAutomationTestBase* T,bool Cold,bool PromptOnly=false,bool CancelLoad=false,bool LightingReview=false):Test(T),bCold(Cold),bPromptOnly(PromptOnly),bCancelLoad(CancelLoad),Evidence(LightingReview?TEXT("HomeBedLighting"):CancelLoad?TEXT("HomeBedCancelLoad"):PromptOnly?TEXT("HomeBedPrompt"):Cold?TEXT("HomeBedCold"):TEXT("HomeBedSource")),bLightingReview(LightingReview){}
 bool Update()override
 {
  if(!Start)Start=FPlatformTime::Seconds();W=GEditor?GEditor->PlayWorld.Get():nullptr;PC=W?W->GetFirstPlayerController():nullptr;P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(FPlatformTime::Seconds()-Start>140)return End(TEXT("Bed native sequence timed out"));if(!P || W->GetTimeSeconds()<4)return false;const float Age=W->GetTimeSeconds()-At;Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Coast=ANeonCityCoastalRegion::Find(W);if(!Coast || !Coast->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(47892,-118360,31740))))return false;
   for(TActorIterator<AIslandHomeSeat> It(W);It;++It)if(It->bSleepBed)Bed=*It;for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It)Clock=*It;
   Save=W->GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>();if(!Bed || !Clock || !Save->ConfigureSlot(TEXT("NeonCity_Test_HomeBed01"),false))return End(TEXT("Native bed/clock/isolated save domain unavailable"));
   if(bCold){if(!Save->DoesSaveExist())return End(TEXT("Genuine source sleeping checkpoint required"));Key(EKeys::F7,true);return Next(20);}
   if(Save->DoesSaveExist())return End(TEXT("Preserve existing bedtime evidence; no overwrite of a prior source run"));
   FVector Stand=FVector::ZeroVector;FCollisionQueryParams Q(SCENE_QUERY_STAT(HomeBedSupportedSetup),false,P);Q.AddIgnoredActor(Bed);bool Supported=false;
   for(const FVector Local:{FVector(0,130,100),FVector(20,130,100),FVector(-20,130,100)})
   {
    const FVector Setup=Bed->GetActorTransform().TransformPosition(Local);FHitResult Floor;
    if(!W->LineTraceSingleByChannel(Floor,Setup+FVector(0,0,100),Setup-FVector(0,0,250),ECC_Pawn,Q) || Floor.ImpactNormal.Z<.7 || FMath::Abs(Floor.ImpactPoint.Z-Bed->GetActorLocation().Z)>12)continue;
    const FVector Candidate=Floor.ImpactPoint+FVector(0,0,P->GetSimpleCollisionHalfHeight()+3);TArray<FOverlapResult> Blocks;W->OverlapMultiByChannel(Blocks,Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetSimpleCollisionRadius(),P->GetSimpleCollisionHalfHeight()),Q);
    bool Blocked=false;for(const auto& Hit:Blocks)if(Hit.bBlockingHit){Blocked=true;UE_LOG(LogTemp,Warning,TEXT("Actual bed access %s blocked by%s/%s"),*Local.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));}
    if(Blocked)continue;
    // A capsule-clear point on the other side of a room partition is not a
    // usable bedside entrance. Keep the real visibility channel and require
    // the normal player eye ray to reach the bed's interaction component.
    const FVector Eye=Candidate+FVector(0,0,P->BaseEyeHeight);FHitResult Prompt;FCollisionQueryParams PromptQ(SCENE_QUERY_STAT(HomeBedSupportedPrompt),false,P);
    W->LineTraceSingleByChannel(Prompt,Eye,Bed->Interaction->GetComponentLocation(),ECC_Visibility,PromptQ);
    if(Prompt.GetActor()!=Bed){UE_LOG(LogTemp,Display,TEXT("Actual bed access %s rejected: eye ray hits%s/%s"),*Local.ToString(),*GetNameSafe(Prompt.GetActor()),*GetNameSafe(Prompt.GetComponent()));continue;}
    Stand=Candidate;Supported=true;break;
   }
   if(!Supported)return End(TEXT("All actual bedside standing candidates lack safe physical clearance"));
   P->SetActorLocation(Stand,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->StopMovementImmediately();
   // Explicit local fatigue/health condition fixtures only, never possessions,
   // world time, rewards, ownership, sleep stage or completion.
   if(!bPromptOnly){P->GetPlayerDataComponent()->SetStamina(45);P->GetHealthComponent()->ApplyPersistenceHealth(85,P->GetHealthComponent()->GetMaxHealth());}return Next(1);
  }
  if(Stage==20 && Age>.8)
  {
   Key(EKeys::F7,false);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Standing() || !W->GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(Bed->PropertyID) || FMath::Abs(Clock->GetTimeOfDay()-Model->World.TimeOfDay)>.08 || FMath::Abs(P->GetPlayerDataComponent()->GetStamina()-Model->Player.Stamina)>2)return End(TEXT("Independent F7 lost saved time/rest, safe player or house ownership"));return Next(1);
  }
  if(Stage==1 && Age>.3)
  {
   if(auto* Weapon=P->GetCombatComponent()->GetWeapon();Weapon && Weapon->IsEquipped())
   {if(Stow.IsValid())return End(TEXT("Mapped weapon stow failed before sleep"));const int Slot=Weapon->GetData()->SlotType;const FKey Slots[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight};if(Slot<1 || Slot>8)return End(TEXT("Weapon slot unavailable"));Stow=Slots[Slot-1];Key(Stow,true);return Next(30);}
   const FVector Approach=Bed->GetActorTransform().TransformPosition(FVector(0,130,100));
   if(FVector::Dist2D(P->GetActorLocation(),Approach)>8)
   {
    if(!ApproachStarted)ApproachStarted=W->GetTimeSeconds();
    if(W->GetTimeSeconds()-ApproachStarted>6)return End(TEXT("Actual mapped walking cannot reach the clear left bedside"));
    PC->SetControlRotation(FRotator(0,(Approach-P->GetActorLocation()).Rotation().Yaw,0));Key(EKeys::W,true);return false;
   }
   Key(EKeys::W,false);ApproachStarted=0;
   auto* Kit=P->FindComponentByClass<UDivingComponent>();OwnSuit=Kit->OwnsSuit();OwnDPV=Kit->OwnsDPV();CapsuleHeight=P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();TimeBefore=Clock->GetTimeOfDay();StaminaBefore=P->GetPlayerDataComponent()->GetStamina();HealthBefore=P->GetHealthComponent()->GetCurrentHealth();const bool Focused=Focus();if(bPromptOnly){Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=70;const FVector Eye=Bed->GetActorTransform().TransformPosition(FVector(0,260,200));Camera->SetActorLocationAndRotation(Eye,(Bed->GetActorLocation()+FVector(0,0,85)-Eye).Rotation());PC->SetViewTarget(Camera);return Next(40);}if(!Focused)return End(TEXT("Actual bedside F prompt blocked"));Key(EKeys::F,true);return Next(2);
  }
  if(Stage==30 && Age>.2){Key(Stow,false);return Next(1);}
  if(Stage==40 && Age>4){FNeonCityVisualEvidence::CaptureHUD(W,TEXT("HomeBed-Prompt-Diagnosis.png"));return End(TEXT(""));}
  if(Stage==2 && Age>.3)
  {
   Key(EKeys::F,false);auto* Body=Bed->Occupant->GetDriverMesh();if(!Bed->IsOccupied() || !Body || !Body->IsVisible() || PC->GetPawn()!=P || !P->GetActorEnableCollision() || P->GetCapsuleComponent()->GetCollisionEnabled()!=ECollisionEnabled::QueryOnly)return End(TEXT("Bed lacks actual visible animated body or real possession/damage-query capsule"));
   for(TActorIterator<AIslandLandLight> It(W);It;++It)if(FVector::Dist(It->GetActorLocation(),Bed->GetActorLocation())<600)
   {const auto* Light=Cast<UPointLightComponent>(It->GetLightComponent());LightingDiagnostic+=FString::Printf(TEXT("%s position%s lumens%.1f day%.2f runtime%.1f radius%.1f visible%d hidden%d registered%d mobility%d\n"),*It->GetName(),*It->GetActorLocation().ToString(),It->Lumens,It->DayMultiplier,Light->Intensity,Light->AttenuationRadius,Light->IsVisible(),It->IsHidden(),Light->IsRegistered(),int(Light->Mobility));}UE_LOG(LogTemp,Display,TEXT("Bed actual fixture audit: %s"),*LightingDiagnostic);
   Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=63;const FVector Eye=Bed->GetActorTransform().TransformPosition(FVector(80,270,190)),Aim=Bed->GetActorTransform().TransformPosition(FVector(0,0,100));Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PC->SetViewTarget(Camera);return Next(bCancelLoad?31:3);
  }
  if(Stage==3 && Age>3.6)
  {
   if(!Bed->IsSleeping())return End(TEXT("Real entry animation did not lead to sleeping"));auto* Body=Bed->Occupant->GetDriverMesh();const FTransform T=Bed->GetActorTransform();Hip=T.InverseTransformPosition(Body->GetBoneLocation(TEXT("pelvis")));Head=T.InverseTransformPosition(Body->GetBoneLocation(TEXT("head")));FootLeft=T.InverseTransformPosition(Body->GetBoneLocation(TEXT("foot_l")));FootRight=T.InverseTransformPosition(Body->GetBoneLocation(TEXT("foot_r")));
   if(Hip.ContainsNaN() || Head.ContainsNaN() || FMath::Abs(Hip.X)>5 || FMath::Abs(Hip.Y)>5 || Hip.Z<61 || Hip.Z>74 || Head.X>-45 || FMath::Abs(Head.Y)>35 || Head.Z<50 || Head.Z>105 || FootLeft.X<35 || FootRight.X<35)return End(FString::Printf(TEXT("Actual sleeping skeleton misses mattress/pillows: hip%s head%s feet%s/%s"),*Hip.ToString(),*Head.ToString(),*FootLeft.ToString(),*FootRight.ToString()));
   FNeonCityVisualEvidence::CaptureHUD(W,bCold?TEXT("HomeBed-Cold-Sleep-Close.png"):TEXT("HomeBed-Native-Sleep-Close.png"));if(bCold){PC->SetViewTarget(P);Camera->Destroy();Camera=nullptr;Key(EKeys::W,true);return Next(21);}return Next(4);
  }
  if(Stage==4 && Bed->GetSleptHours()>=2)
  {
   if(!Bed->FindPersistenceExit(Safe))return End(TEXT("Actual sleep standing save point unavailable"));Key(EKeys::F6,true);return Next(5);
  }
  if(Stage==5 && Age>.8)
  {
   Key(EKeys::F6,false);auto* Model=Save->ReadSaveForInspection();if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Model || !Bed->IsOccupied() || FVector::Dist(Model->Player.Transform.GetLocation(),Safe.GetLocation())>1 || Model->Player.OwnsR05!=OwnSuit || Model->Player.OwnsDPV!=OwnDPV || Model->World.TimeOfDay==TimeBefore)return End(TEXT("F6 while sleeping failed safe standing/current shared time/rest/owned equipment"));SavedTime=Model->World.TimeOfDay;return Next(6);
  }
  if(Stage==6 && !Bed->IsOccupied())
  {
   const float Advance=FMath::Fmod(Clock->GetTimeOfDay()-TimeBefore+24,24);if(!Standing() || Bed->GetSleptHours()<5.99 || Advance<6 || Advance>6.4 || P->GetPlayerDataComponent()->GetStamina()<FMath::Min(100.f,StaminaBefore+59) || P->GetHealthComponent()->GetCurrentHealth()<FMath::Min(P->GetHealthComponent()->GetMaxHealth(),HealthBefore+11.9f))return End(TEXT("Complete actual sleep/exit did not recover shared health/stamina and advance six hours"));
   PC->SetViewTarget(P);Camera->Destroy();Camera=nullptr;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("HomeBed-Native-SixHours-Stand.png"));if(!Focus())return End(TEXT("Repeated bedside prompt unavailable"));Key(EKeys::F,true);return Next(7);
  }
  if(Stage==7 && Age>.35){Key(EKeys::F,false);return Next(31);}
  // Enhanced Input must observe the released frame before another Started
  // event. A release and press in the same automation update is no key tap.
  if(Stage==31 && Age>.15){if(!Bed->IsOccupied() || !Focus())return End(TEXT("Entry cancellation F focus lost"));Key(EKeys::F,true);return Next(8);}
  if(Stage==8 && Age>.2){Key(EKeys::F,false);return Next(9);}
  if(Stage==9 && Age>1)
  {
   if(!Standing())return End(TEXT("Native F entry cancellation failed to reverse and restore standing"));if(bCancelLoad){PC->SetViewTarget(P);Camera->Destroy();Camera=nullptr;FNeonCityVisualEvidence::CaptureHUD(W,TEXT("HomeBed-F-Cancel-Safe-Standing.png"));SavedTime=Save->ReadSaveForInspection()->World.TimeOfDay;}if(!Focus())return End(TEXT("Replayed sleep F prompt unavailable"));Key(EKeys::F,true);return Next(10);
  }
  if(Stage==10 && Age>.2){Key(EKeys::F,false);return Next(11);}
  if(Stage==11 && Age>Bed->BedEnter->GetPlayLength()+.5){if(!Bed->IsSleeping())return End(TEXT("Second entry failed before real load cancellation"));Key(EKeys::F7,true);return Next(12);}
  if(Stage==12 && Age>.8)
  {
   Key(EKeys::F7,false);if(Save->GetLastResult()!=ENeonCitySaveResult::Success || !Standing() || FMath::Abs(Clock->GetTimeOfDay()-SavedTime)>.08)return End(TEXT("F7 during sleep failed safe transient release and saved shared time"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("HomeBed-Native-F7-Safe-Standing.png"));return End(TEXT(""));
  }
  if(Stage==21 && Age>.2){Key(EKeys::W,false);return Next(22);}
  if(Stage==22 && Age>Bed->BedExit->GetPlayLength()+.5){if(!Standing())return End(TEXT("Cold restored native W did not play exit and restore normal capsule/input"));FNeonCityVisualEvidence::CaptureHUD(W,TEXT("HomeBed-Cold-W-Safe-Standing.png"));if(bLightingReview){VisualOriginalHour=Clock->GetTimeOfDay();Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=63;PC->SetViewTarget(Camera);return Next(41);}return End(TEXT(""));}
  if(Stage==41)
  {
   if(!bVisualPlaced){const float Hours[]={10,17.8f,22};Clock->SetTimeOfDay(Hours[VisualIndex/2]);const FVector Eye=Bed->GetActorTransform().TransformPosition(FVector(80,270,190));const FVector Aim=VisualIndex%2==0?Bed->GetActorLocation()+FVector(0,0,100):NeonCityCoastalLayout::FromLegacyIsland(FVector(48200,-120400,31935));Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());bVisualPlaced=true;At=W->GetTimeSeconds();return false;}
   if(Age<4)return false;const TCHAR* Names[]={TEXT("HomeBed-Room-Day.png"),TEXT("HomeBed-Window-Day.png"),TEXT("HomeBed-Room-Sunset.png"),TEXT("HomeBed-Window-Sunset.png"),TEXT("HomeBed-Room-Night.png"),TEXT("HomeBed-Window-Night.png")};FNeonCityVisualEvidence::CaptureHUD(W,Names[VisualIndex]);VisualHours.Add(Clock->GetTimeOfDay());bVisualPlaced=false;if(++VisualIndex<6)return false;Clock->SetTimeOfDay(VisualOriginalHour);return End(TEXT(""));
  }
  return false;
 }
 bool Standing()const{return Bed && !Bed->IsOccupied() && P && !P->IsHidden() && P->GetActorEnableCollision() && !P->GetAttachParentActor() && (CapsuleHeight==0 || FMath::Abs(P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-CapsuleHeight)<.01) && P->GetCharacterMovement()->IsMovingOnGround();}
 bool Focus()
 {
  PC->SetControlRotation((Bed->Interaction->GetComponentLocation()-P->GetPawnViewLocation()).Rotation());FVector Eye;FRotator View;P->GetActorEyesViewPoint(Eye,View);FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(HomeBedRealPromptDiagnosis),false,P);W->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*P->GetInteractionComponent()->InteractionDistance,ECC_Visibility,Q);
  PromptDiagnostic=FString::Printf(TEXT("player%s bed%s eye%s aim%s hit%s/%s distance%.2f allowed%d ground%d mode%d hidden%d property%d"),*P->GetActorLocation().ToString(),*Bed->GetActorLocation().ToString(),*Eye.ToString(),*Bed->Interaction->GetComponentLocation().ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.Distance,Bed->CanInteract_Implementation(P),P->GetCharacterMovement()->IsMovingOnGround(),int(P->GetCharacterMovement()->MovementMode),P->IsHidden(),W->GetGameInstance()->GetSubsystem<UPropertySubsystem>()->IsOwned(Bed->PropertyID));UE_LOG(LogTemp,Display,TEXT("Bed prompt native diagnosis: %s"),*PromptDiagnostic);
  return P->GetInteractionComponent()->RefreshFocusedInteractable()==Bed;
 }
 bool Next(int N){Stage=N;At=W->GetTimeSeconds();return false;}
 void Key(FKey K,bool Down){bool& Old=Keys.FindOrAdd(K);if(Old!=Down){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));Old=Down;}}
 bool End(const FString& Error)
 {
  if(PC){for(auto& K:Keys)if(K.Value)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K.Key,IE_Released,FPlatformTime::Cycles64()));if(P)PC->SetViewTarget(P);}if(Camera)Camera->Destroy();if(!Error.IsEmpty())Test->AddError(Error);
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("passed"),Error.IsEmpty());J->SetStringField(TEXT("failure"),Error);J->SetNumberField(TEXT("stage"),Stage);J->SetNumberField(TEXT("time_before"),TimeBefore);J->SetNumberField(TEXT("time_saved"),SavedTime);J->SetNumberField(TEXT("time_after"),Clock?Clock->GetTimeOfDay():0);J->SetStringField(TEXT("visible_hip_local"),Hip.ToString());J->SetStringField(TEXT("visible_head_local"),Head.ToString());J->SetStringField(TEXT("visible_feet_local"),FootLeft.ToString()+TEXT(" / ")+FootRight.ToString());J->SetStringField(TEXT("native_prompt_diagnosis"),PromptDiagnostic);
  J->SetStringField(TEXT("scope"),bCold?TEXT("Independent genuine F7 sleeping-save safe bedside restore of clock/shared rest/property/owned gear; native F sleep/visible articulated body and mapped W exit. Zero pose/state fixtures."):TEXT("One real supported bedside player setup; explicit45stamina/85health initial-condition fixtures. Actual native weapon stow/F entry, finite original articulated enter/sleep/exit, measured visible body/mattress/pillow direction, six-hour shared clock/rest, F6 standing-safe sleeping save, native F mid-entry reversal and F7 release on foot. No completion/reward/property/inventory/time fixture or new save schema. This is objective facility evidence, not subjective motion approval."));
  if(bPromptOnly)J->SetStringField(TEXT("scope"),TEXT("Minimum real player eye-to-bed prompt diagnosis. One supported physical bedside start and native weapon stow. Actual collision/ownership/movement/visibility traces and rendered bed surroundings; no use/rest/save/time/reward actions. Diagnostic completion is never a bed gameplay PASS."));
  if(bCancelLoad)J->SetStringField(TEXT("scope"),TEXT("Minimum cancellation/load reproduction from the genuine sleeping F6 checkpoint129 in a separate process. Native F7 safe standing; normal weapon stow/F entry; separate real released frame before F reverse-entry cancellation; normal F sleep and real F7 safe release. No position, time, state, ownership, equipment or rest fixtures, and no save overwrite. Earlier six-hour source evidence129 is retained, not repeated."));
  J->SetStringField(TEXT("actual_bedroom_light_audit"),LightingDiagnostic);
  if(bLightingReview){TArray<TSharedPtr<FJsonValue>> Hours;for(float H:VisualHours)Hours.Add(MakeShared<FJsonValueNumber>(H));J->SetArrayField(TEXT("actual_fixed_view_hours"),Hours);J->SetStringField(TEXT("scope"),TEXT("Fix-specific normal-rendered lighting review from genuine sleep checkpoint129. Real F7/F/body enter/sleep/mapped W exit, measured mattress bones; no six-hour/save/cancellation rerun. Six explicit fixed-camera visual fixtures set shared clock Day10/Sunset17.8/Night22 only after safe standing, then restore prior hour without saving. Real unchanged2800lm lamps, no capture-only light, global exposure/Bloom or material adjustment. Native body and room/window PNGs require visual inspection; technical capture completion is not whole-house luxury or subjective PASS."));}
  FString Label,Text;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Label);const TCHAR* Kind=bLightingReview?TEXT("HomeBedLighting"):bCancelLoad?TEXT("HomeBedCancelLoad"):bPromptOnly?TEXT("HomeBedPrompt"):bCold?TEXT("HomeBedCold"):TEXT("HomeBedNative");const FString Dir=FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/FPaths::MakeValidFileName(Label)/Kind;IFileManager::Get().MakeDirectory(*Dir,true);FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("receipt.json")));return true;
 }
 FAutomationTestBase* Test;bool bCold,bPromptOnly,bCancelLoad;FString PromptDiagnostic;UWorld* W=nullptr;APlayerController* PC=nullptr;ANeonCityPlayerCharacter* P=nullptr;AIslandHomeSeat* Bed=nullptr;ANeonCityDayNightCycle* Clock=nullptr;UNeonCitySaveSubsystem* Save=nullptr;ACameraActor* Camera=nullptr;TMap<FKey,bool> Keys;FKey Stow;FTransform Safe;FVector Hip=FVector::ZeroVector,Head=FVector::ZeroVector,FootLeft=FVector::ZeroVector,FootRight=FVector::ZeroVector;int Stage=0;float At=0,TimeBefore=0,SavedTime=0,StaminaBefore=0,HealthBefore=0,CapsuleHeight=0;double Start=0;bool OwnSuit=false,OwnDPV=false;FNeonCityVisualEvidence Evidence;
 FString LightingDiagnostic;
 float ApproachStarted=0;
 bool bLightingReview=false,bVisualPlaced=false;int VisualIndex=0;float VisualOriginalHour=0;TArray<float> VisualHours;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeBedNativeTest,"NeonCity.Phase205c.HomeBed.A",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeBedNativeTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeBed::FFlow(this,false));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeBedColdTest,"NeonCity.Phase205c.HomeBed.B",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeBedColdTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeBed::FFlow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeBedPromptTest,"NeonCity.Phase205c.HomeBed.PromptDiagnosisPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeBedPromptTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeBed::FFlow(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeBedCancelLoadTest,"NeonCity.Phase205c.HomeBed.CancelLoadPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeBedCancelLoadTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeBed::FFlow(this,true,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeBedLightingTest,"NeonCity.Phase205c.HomeBed.LightingViewsPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHomeBedLightingTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(NativeHomeBed::FFlow(this,true,false,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
