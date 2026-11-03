#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/Foliage/NeonCityFoliageInteractionComponent.h"
#include "World/Foliage/NeonCityPalmContact.h"
#include "World/Foliage/NeonCityFoliageContactAudioComponent.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Audio.h"
#include "Sound/SoundWave.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/VehicleComponent.h"
#include "Vehicles/VehicleData.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatSurface.h"
#include "Combat/CombatTestTarget.h"
#include "Combat/Weapons/NeonCityPistol.h"
#include "Physics/NeonCityPhysicalSurfaces.h"
#include "Physics/PhysicalImpactComponent.h"
#include "Physics/PhysicalImpactSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFoliageWoodData,"NeonCity.Foliage.WoodCollisionData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFoliageWoodData::RunTest(const FString&)
{
 auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_TreeWoodCollision"));
 if(!TestNotNull(TEXT("Cooked source wood geometry"),Mesh))return false;
 auto* Body=Mesh->GetBodySetup();if(!TestNotNull(TEXT("Native collision cook"),Body))return false;
 TestEqual(TEXT("Wood triangles provide query and non-simulated physical contacts"),Body->GetCollisionTraceFlag(),CTF_UseComplexAsSimple);
 TestTrue(TEXT("Thin branches support either contact direction"),bool(Body->bDoubleSidedGeometry));
 TestTrue(TEXT("No crown box or simple hull encloses the tree"),Body->AggGeom.GetElementCount()==0);
 TestTrue(TEXT("Full trunk and major branches retained"),Mesh->GetBounds().BoxExtent.Z>350 && Mesh->GetBounds().BoxExtent.X>150);
 auto* Brush=LoadObject<USoundWave>(nullptr,TEXT("/Game/NeonCity/World/Foliage/Audio/SW_NC_FoliageBrush"));
 if(TestNotNull(TEXT("Individually reviewed CC0 contact sound"),Brush))
 {TestTrue(TEXT("Foliage contact is a finite mono one-shot"),!Brush->bLooping && Brush->NumChannels==1 && FMath::Abs(Brush->Duration-1.11458f)<.003f);TestEqual(TEXT("Contact gain is owned by its player"),Brush->Volume,1.f);}
 return true;
}

namespace
{
/** Actual shot, not a substitute visibility trace. A matching clear lane proves
 *  health remains unchanged behind wood because of occlusion, not a broken gun. */
void VerifyWoodShot(FAutomationTestBase* Test,UWorld* World,APlayerController* Controller,
 const FVector& Start,const FVector& End,AActor* Wood)
{
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Host=World->SpawnActor<AActor>(Spawn);
 auto* Mount=NewObject<USceneComponent>(Host);Host->SetRootComponent(Mount);Host->AddInstanceComponent(Mount);
 Mount->RegisterComponent();Host->SetActorLocation(Start);
 auto* Target=World->SpawnActor<ACombatTestTarget>(End,FRotator::ZeroRotator,Spawn);
 Spawn.Owner=Host;auto* Weapon=World->SpawnActor<ANeonCityPistol>(Spawn);
 if(Test->TestTrue(TEXT("Native pistol and target fixture"),IsValid(Weapon) && IsValid(Target)))
 {
  Test->TestTrue(TEXT("Unmodified pistol equips on generic mount"),Weapon->Equip(Mount));
  FCombatAttackRequest Shot;Shot.ViewOrigin=Start;Shot.Direction=(End-Start).GetSafeNormal();Shot.Instigator=Controller;Shot.Aiming=true;
  Test->TestTrue(TEXT("Actual weapon accepts forest shot"),Weapon->Fire(Shot));
  Test->TestEqual(TEXT("One real round consumed"),Weapon->GetAmmo(),Weapon->GetData()->MagazineSize-1);
  if(Wood)
  {
   Test->TestTrue(TEXT("Real ballistic trace terminates at solid wood"),Weapon->GetLastHit().GetActor()==Wood);
   Test->TestTrue(TEXT("Wood feedback is not mislabeled concrete"),NeonCityCombat::ResolveImpactSurface(Weapon->GetLastHit())==ENeonCityImpactSurface::Wood);
   Test->TestEqual(TEXT("Target behind wood receives no damage"),Target->GetHealthComponent()->GetCurrentHealth(),100.f);
   Test->TestEqual(TEXT("Rooted forest has no invented damage recipient"),Weapon->GetLastAppliedDamage(),0.f);
  }
  else
  {
   Test->TestTrue(TEXT("Clear lane beside the trunk reaches the target"),Weapon->GetLastHit().GetActor()==Target);
   Test->TestEqual(TEXT("Same pistol damages an unobstructed target"),Target->GetHealthComponent()->GetCurrentHealth(),100.f-Weapon->GetData()->Damage);
  }
 }
 if(Weapon)Weapon->Destroy();if(Target)Target->Destroy();Host->Destroy();
}

/** Controlled contact fixture in the real game world. It complements the saved
 *  island walk, and does not masquerade as the authored forest route. */
class FFoliageContacts : public IAutomationLatentCommand
{
public:
 explicit FFoliageContacts(FAutomationTestBase* In):Test(In) {}
 ~FFoliageContacts() override { if(AppVolume)AppVolume->Set(PreviousAppVolume,ECVF_SetByCode); }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>130){Test->AddError(FString::Printf(TEXT("Foliage contact timeout stage=%d"),Stage));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* Player=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;
  if(!Player || W->GetTimeSeconds()<1)return false;const float Now=W->GetTimeSeconds();
  auto Next=[&](int32 S){Stage=S;Phase=Now;return false;};
  if(Stage==0)
  {
   for(TActorIterator<ANeonCityBaseVehicle> It(W);It;++It)if(It->GetVehicleProfile()->VehicleType==ENeonCityVehicleType::Sedan){Car=*It;break;}
   if(!Test->TestNotNull(TEXT("Actual Chaos sedan fixture"),Car))return true;
   auto* Source=LoadObject<UNeonCityReferenceVegetationData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Vegetation/DA_NCTW_Vegetation"));
   if(!Test->TestNotNull(TEXT("Reviewed source vegetation"),Source))return true;
   auto* D=NewObject<UNeonCityReferenceVegetationData>();D->ReferenceCommit=Source->ReferenceCommit;
   for(const TCHAR* Family:{TEXT("trees"),TEXT("shrubs"),TEXT("palms")})
   {
    const auto* G=Source->Groups.FindByPredicate([&](const auto& P){return P.SourceFamily==Family;});if(!G)return true;
    auto Copy=*G;Copy.Instances.Reset();FNeonCityReferencePlant P;
    const bool Tree=Copy.SourceFamily==TEXT("trees"),Palm=Copy.SourceFamily==TEXT("palms");
    const FVector Local(Palm?1200:0,Tree?0:500,0);
    P.Transform=FTransform(Local);P.ShaderData0=FLinearColor(.4f,Tree?12.f:1.5f,1,0);
    P.ShaderData1=FLinearColor(Tree?1:-1,Local.X/100,Local.Z/100,-Local.Y/100);Copy.Instances.Add(P);D->Groups.Add(Copy);
    if(Palm){auto& Record=D->Groups.Last().Instances[0];Record.ShaderData0.G=10;Record.ShaderData1.R=.35f;}
   }
   auto* Floor=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);Floor->AddInstanceComponent(Box);
   Box->SetBoxExtent(FVector(8000,4000,20));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Floor->SetActorLocation(Origin-FVector(0,0,20));
   Layer=W->SpawnActorDeferred<ANeonCityReferenceVegetation>(ANeonCityReferenceVegetation::StaticClass(),FTransform(Origin));
   Layer->Data=D;Layer->WindSpeed=0;Layer->FinishSpawning(FTransform(Origin));
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
   Player->SetActorLocation(Origin+FVector(-550,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation(FRotator(-8,0,0));
   return Next(1);
  }
  auto* Interaction=Layer->GetFoliageInteraction();Evidence.Record(W,PC,Stage);
  auto* ContactAudio=Layer->FindComponentByClass<UNeonCityFoliageContactAudioComponent>();
  if(!ContactAudio){Test->AddError(TEXT("Missing contact audio adapter"));return true;}
  PeakPlaying=FMath::Max(PeakPlaying,ContactAudio->GetPlayingVoiceCount());
  Test->TestTrue(TEXT("Contact audio allocation is bounded"),ContactAudio->GetVoiceAllocationCount()<=2);
  PeakBodies=FMath::Max(PeakBodies,Interaction->GetAllocatedWoodCount());
  PeakBend=FMath::Max(PeakBend,Interaction->GetPlantBend(1,0).Size());
  auto Guide=[&](FVector2D Local)
  {
   const FVector2D To=FVector2D(Origin)+Local-FVector2D(Player->GetActorLocation());
   PC->SetControlRotation(FRotator(-8,FMath::RadiansToDegrees(FMath::Atan2(To.Y,To.X)),0));
   if(To.Size()>12)Player->DoMove(0,FMath::Clamp(To.Size()/120.,.2,1.));return To.Size()<20;
  };
  if(Stage==1 && Now-Phase>2)
  {
   Test->TestEqual(TEXT("Initial observer activates nearby broadleaf wood"),Interaction->GetActiveWoodCount(),1);
   Test->TestEqual(TEXT("More distant palm is not needlessly active"),Interaction->GetActivePalmCount(),0);
   FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(WoodFixture),false,Player);
   Test->TestTrue(TEXT("Native visibility trace hits actual wood"),W->LineTraceSingleByChannel(Hit,Origin+FVector(-200,0,100),Origin+FVector(200,0,100),ECC_Visibility,Params) && Hit.GetActor()==Layer);
   Test->TestFalse(TEXT("Empty space beside trunk is not a giant crown collider"),W->LineTraceSingleByChannel(Hit,Origin+FVector(-200,250,100),Origin+FVector(200,250,100),ECC_Visibility,Params));
   VerifyWoodShot(Test,W,PC,Origin+FVector(-400,0,100),Origin+FVector(400,0,100),Layer);
   VerifyWoodShot(Test,W,PC,Origin+FVector(-400,250,100),Origin+FVector(400,250,100),nullptr);
   return Next(2);
  }
  if(Stage==2)
  {
   Guide(FVector2D(0,0));
   if(Now-Phase<3)return false;
   const FVector Delta=Player->GetActorLocation()-Origin;
   Test->TestTrue(TEXT("Normal template movement reaches trunk but cannot pass through"),Delta.Size2D()>50 && Delta.Size2D()<140 && Delta.X<0);
   Test->TestEqual(TEXT("Ordinary tree contact cannot cause vehicle impact injury"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wood-CharacterBlocked.png"));
   bAudio=FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck"));
   if(bAudio)
   {
    AppVolume=IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume"));
    if(!AppVolume){Test->AddError(TEXT("Missing native audio focus test control"));return true;}
    PreviousAppVolume=AppVolume->GetInt();AppVolume->Set(1,ECVF_SetByCode);
    UAudioMixerBlueprintLibrary::StartRecordingOutput(W,25);
   }
   return Next(3);
  }
  if(Stage==3){if(!Guide(FVector2D(-240,500)))return false;return Next(4);}
  if(Stage==4){if(!Guide(FVector2D(-25,500)))return false;return Next(5);}
  if(Stage==5 && Now-Phase>1)
  {
   Test->TestTrue(TEXT("Physical proximity bends the soft instance"),Interaction->GetPlantBend(1,0).Size()>8);
   auto* C=Layer->GetPlantGroup(1);Test->TestEqual(TEXT("Preserved eight source floats plus native spring vector"),C->NumCustomDataFloats,11);
   Test->TestTrue(TEXT("The actual rendered instance receives deformation"),FMath::Abs(C->PerInstanceSMCustomData[8])+FMath::Abs(C->PerInstanceSMCustomData[9])>8);
   Test->TestTrue(TEXT("Moving through the shrub emits actual contact feedback"),ContactAudio->GetContactEventCount()>0);
   StandingEvents=ContactAudio->GetContactEventCount();
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SoftPlant-Contact.png"));return Next(12);
  }
  if(Stage==12 && Now-Phase>.8f)
  {
   Test->TestEqual(TEXT("Stationary contact does not repeatedly play brush sounds"),ContactAudio->GetContactEventCount(),StandingEvents);
   return Next(6);
  }
  if(Stage==6){if(!Guide(FVector2D(700,500)))return false;return Next(7);}
  if(Stage==7 && Now-Phase>3)
  {
   if(bAudio && !RecordingStopped)
   {
    FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
    const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("Audio"));
    IFileManager::Get().MakeDirectory(*Dir,true);Recording=Dir/TEXT("Foliage-Contact-Native.wav");
    UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,TEXT("Foliage-Contact-Native"),Dir);
    RecordingStopped=true;RecordingUntil=Now+6;return false;
   }
   if(bAudio)
   {
    TArray<uint8> Bytes;FWaveModInfo Info;
    if(!FFileHelper::LoadFileToArray(Bytes,*Recording) || !Info.ReadWaveInfo(Bytes.GetData(),Bytes.Num()))
    {if(Now<RecordingUntil)return false;Test->AddError(TEXT("No readable native brush recording"));return true;}
    if(!Info.pBitsPerSample || *Info.pBitsPerSample!=16 || !Info.pChannels || !Info.pSamplesPerSec)
    {Test->AddError(TEXT("Unexpected native brush PCM format"));return true;}
    const int32 Count=Info.SampleDataSize/sizeof(int16),Second=int32(*Info.pChannels)*int32(*Info.pSamplesPerSec);
    double Energy=0,Peak=0;int32 Nonzero=0;
    for(int32 I=0;I<Count;++I)
    {
     int16 PCM=0;FMemory::Memcpy(&PCM,Info.SampleDataStart+I*sizeof(int16),sizeof(int16));
     const double V=double(PCM)/32768.;Energy+=V*V;Peak=FMath::Max(Peak,FMath::Abs(V));Nonzero+=FMath::Abs(V)>1e-5;
    }
    const double RMS=Count?FMath::Sqrt(Energy/Count):0;
    Test->TestTrue(TEXT("Native brush recording has sustained contact sound, not a silent file or click"),
     Second>0 && Count/Second>=2 && Nonzero>Second/2 && RMS>1e-5 && Peak>1e-4 && PeakPlaying>0);
    Test->AddInfo(FString::Printf(TEXT("Native contact events=%d peak_voices=%d RMS=%.8f peak=%.8f recording=%s"),
     ContactAudio->GetContactEventCount(),PeakPlaying,RMS,Peak,*Recording));
   }
   Test->TestTrue(TEXT("Soft foliage returns with a damped spring after departure"),Interaction->GetPlantBend(1,0).Size()<.5);
   Test->TestTrue(TEXT("Plant roots never move or accumulate deformation"),Layer->Data->Groups[1].Instances[0].Transform.GetLocation().Equals(FVector(0,500,0)));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("SoftPlant-Recovered.png"));
   Car->GetDrivingComponent()->SetDrivingEnabled(false);Car->GetChaosMovement()->ResetVehicle();
   Car->SetActorLocationAndRotation(Origin+FVector(-1900,0,150),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Car->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);Car->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
   Car->FindComponentByClass<UPhysicalImpactComponent>()->Suppress(1);return Next(8);
  }
  if(Stage==8 && Now-Phase>2)
  {
   BeforeHealth=Car->GetHealthComponent()->GetCurrentHealth();W->GetSubsystem<UPhysicalImpactSubsystem>()->ResetForLoad();
   Car->GetDrivingComponent()->SetDrivingEnabled(true);Car->GetMesh()->SetPhysicsLinearVelocity(FVector(1800,0,0));return Next(9);
  }
  if(Stage==9 && Now-Phase>3)
  {
   bool Impact=false;float Speed=0;
   for(const auto& E:W->GetSubsystem<UPhysicalImpactSubsystem>()->GetRecentEvents())if(E.SourceActor.Get()==Layer && E.TargetActor.Get()==Car)
   {Impact=true;Speed=FMath::Max(Speed,E.ImpactSpeed);Test->TestTrue(TEXT("Chaos contact identifies the same wood physical surface"),E.SurfaceType==NeonCityPhysicalSurfaces::Wood);}
   Test->TestTrue(TEXT("Real Chaos collision feeds the existing impact pipeline"),Impact && Speed>700);
   Test->TestTrue(TEXT("Tree collision damages the real vehicle through shared health"),Car->GetHealthComponent()->GetCurrentHealth()<BeforeHealth);
   Test->TestTrue(TEXT("Vehicle cannot drive through the trunk"),Car->GetActorLocation().X<Origin.X-100);
   Test->TestTrue(TEXT("Contact stops/deflects vehicle velocity"),Car->GetVelocity().X<800);
   Test->TestTrue(TEXT("Bounded collision pool, no capacity misses"),PeakBodies<=UNeonCityFoliageInteractionComponent::MaximumWoodBodies && Interaction->GetCapacityMissCount()==0);
   Test->AddInfo(FString::Printf(TEXT("Wood bodies=%d, soft bend peak=%.2f cm, impact=%.2f cm/s, car health %.2f -> %.2f"),PeakBodies,PeakBend,Speed,BeforeHealth,Car->GetHealthComponent()->GetCurrentHealth()));
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Wood-ChaosContact.png"));Car->GetDrivingComponent()->SetDrivingEnabled(false);
   return Next(10);
  }
  if(Stage==10){if(!Guide(FVector2D(950,500)))return false;return Next(11);}
  if(Stage==11)
  {
   Guide(FVector2D(1450,500));if(Now-Phase<3)return false;
   const FVector Delta=Player->GetActorLocation()-Origin;
   Test->TestTrue(TEXT("Normal character movement cannot pass through the curved palm stem"),Delta.X>1100 && Delta.X<1300 && FMath::Abs(Delta.Y-500)<40);
   Test->TestTrue(TEXT("Capsule joints cannot become invisible steps up a palm"),Player->GetCharacterMovement()->IsMovingOnGround() && Delta.Z<Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+6);
   Test->TestEqual(TEXT("Approaching the actual palm activates one curved adapter"),Interaction->GetActivePalmCount(),1);
   FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(PalmFixture),false,Player);
   for(float U:{.1f,.35f,.75f})
   {
    const FVector Center=NeonCityPalmContact::Center(Origin+FVector(1200,500,0),1000,.35f,0,.4f,U);
    const bool Contact=W->LineTraceSingleByChannel(Hit,Center-FVector(0,100,0),Center+FVector(0,100,0),ECC_Visibility,Params);
    Test->TestTrue(TEXT("Palm collision follows actual source stem curve"),Contact && Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("NeonCity.Foliage.PalmWood")));
   }
   const FVector PalmShot=NeonCityPalmContact::Center(Origin+FVector(1200,500,0),1000,.35f,0,.4f,.35f);
   VerifyWoodShot(Test,W,PC,PalmShot-FVector(0,350,0),PalmShot+FVector(0,350,0),Layer);
   Test->TestFalse(TEXT("Curved palm does not block the empty vertical column above its root"),
    W->LineTraceSingleByChannel(Hit,Origin+FVector(1200,400,750),Origin+FVector(1200,600,750),ECC_Visibility,Params));
   Test->TestEqual(TEXT("Ordinary palm contact retains player health"),Player->GetHealthComponent()->GetCurrentHealth(),100.f);
   Test->TestTrue(TEXT("Combined wood and palm bodies stay inside the shared allocation cap"),Interaction->GetAllocatedWoodCount()<=UNeonCityFoliageInteractionComponent::MaximumWoodBodies);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("Palm-CharacterBlocked.png"));return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Started=0,PeakBend=0;float Phase=0,BeforeHealth=0;int32 Stage=0,PeakBodies=0;
 bool bAudio=false,RecordingStopped=false;int32 StandingEvents=0,PeakPlaying=0,PreviousAppVolume=0;
 IConsoleVariable* AppVolume=nullptr;FString Recording;float RecordingUntil=0;
 const FVector Origin=FVector(220000,0,300);
 ANeonCityReferenceVegetation* Layer=nullptr;ANeonCityBaseVehicle* Car=nullptr;
 FNeonCityVisualEvidence Evidence{TEXT("FoliagePhysicalContact")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFoliageContactPIE,"NeonCity.Foliage.PhysicalContactPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFoliageContactPIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFoliageContacts>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
