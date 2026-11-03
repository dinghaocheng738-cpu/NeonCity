#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/Marine/CoastalRegionLayout.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Tests/NeonCityStandaloneValidation.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Adventure/DivingComponent.h"
#include "Water/DiveBreathingComponent.h"
#include "Water/UnderwaterViewComponent.h"
#include "Water/SwimmingComponent.h"
#include "Water/WaterInteractionComponent.h"
#include "Water/OceanSoundscapeComponent.h"
#include "Water/CoastalOceanSurface.h"
#include "Adventure/OceanHabitat.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "World/Marine/NeonCityCoastalRegion.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Audio.h"
namespace
{
// UE's tolerant reader accepts truncated data chunks. An asynchronous mixer
// export is ready only after the declared RIFF extent has actually arrived.
bool CompleteWaveExport(const TArray<uint8>& Bytes)
{
 if(Bytes.Num()<12 || FMemory::Memcmp(Bytes.GetData(),"RIFF",4) || FMemory::Memcmp(Bytes.GetData()+8,"WAVE",4))return false;
 auto LE32=[&](int32 Offset){return uint32(Bytes[Offset])|(uint32(Bytes[Offset+1])<<8)|(uint32(Bytes[Offset+2])<<16)|(uint32(Bytes[Offset+3])<<24);};
 if(uint64(LE32(4))+8!=uint64(Bytes.Num()))return false;
 bool HasFormat=false,HasData=false;uint32 BlockAlign=0;
 for(uint64 Offset=12;Offset<uint64(Bytes.Num());)
 {
  if(Offset+8>uint64(Bytes.Num()))return false;
  const uint32 Size=LE32(int32(Offset)+4);const uint64 End=Offset+8+Size;
  if(End+(Size&1)>uint64(Bytes.Num()))return false;
  const uint8* Chunk=Bytes.GetData()+Offset;
  if(!FMemory::Memcmp(Chunk,"fmt ",4))
  {
   if(HasFormat || Size<16 || Chunk[8]!=1 || Chunk[9]!=0)return false;
   const uint32 Channels=uint32(Chunk[10])|(uint32(Chunk[11])<<8);
   BlockAlign=uint32(Chunk[20])|(uint32(Chunk[21])<<8);
   if(!Channels || !LE32(int32(Offset)+12) || BlockAlign!=Channels*2 || Chunk[22]!=16 || Chunk[23]!=0)return false;
   HasFormat=true;
  }
  if(!FMemory::Memcmp(Chunk,"data",4))
  {if(!HasFormat || HasData || !Size || Size%BlockAlign)return false;HasData=true;}
  Offset=End+(Size&1);
 }
 return HasFormat && HasData;
}
class FOceanExperienceCheck:public IAutomationLatentCommand
{
public:
 explicit FOceanExperienceCheck(FAutomationTestBase* T):Test(T),Evidence(TEXT("OceanExperience")){}
 ~FOceanExperienceCheck(){if(AppVolume)AppVolume->Set(PreviousAppVolume,ECVF_SetByCode);}
 bool Update()override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>150){Test->AddError(FString::Printf(TEXT("Ocean experience fixture timeout, stage %d"),Stage));return true;}
  auto* W=NeonCityValidationWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<3)return false;
  auto* Dive=P->FindComponentByClass<UDivingComponent>();auto* Swim=P->FindComponentByClass<USwimmingComponent>();
  auto* Breath=P->FindComponentByClass<UDiveBreathingComponent>();auto* Optics=P->FindComponentByClass<UUnderwaterViewComponent>();
  auto* Water=P->FindComponentByClass<UWaterInteractionComponent>();auto* Move=P->GetCharacterMovement();
  auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
  const float Now=W->GetTimeSeconds();Evidence.Record(W,PC,Stage);
  if(Stage==0)
  {
   auto* Region=ANeonCityCoastalRegion::Find(W);if(!Region || !Region->EnsureDestinationReady(NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,0))))return false;
   for(TActorIterator<AOceanHabitat> It(W);It;++It)
   {
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Groups(*It);
    Test->TestEqual(TEXT("Habitat groups do not duplicate across editor/PIE construction"),Groups.Num(),It->HabitatLayers.Num());
   }
   for(TActorIterator<ANeonCityDayNightCycle> It(W);It;++It){It->SetCycleEnabled(false);It->SetTimeOfDay(11);}
   if(!Test->TestTrue(TEXT("Real player equips the ordinary dive suit on land"),Dive && Dive->Equip()))return true;
   Land=P->GetActorTransform();OriginalWalk=Move->MaxWalkSpeed;OriginalFriction=Move->GroundFriction;
   FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(OceanTestFloor),false,P);
   if(!Test->TestTrue(TEXT("Authored seabed has actual support collision"),W->LineTraceSingleByChannel(Hit,NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,-300)),NeonCityCoastalLayout::FromLegacyIsland(FVector(55000,-178000,-8000)),ECC_WorldStatic,Q)))return true;
   Dive->SetFirstPerson(true);P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
   Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Swimming);PC->SetControlRotation(FRotator(-5,60,0));
   bAudio=FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck"));
   if(bAudio)
   {
    AppVolume=IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume"));
    if(!AppVolume){Test->AddError(TEXT("Missing native app-volume control"));return true;}
    PreviousAppVolume=AppVolume->GetInt();AppVolume->Set(1,ECVF_SetByCode);
    UAudioMixerBlueprintLibrary::StartRecordingOutput(W,45);
   }
   At=Now;Stage=1;return false;
  }
  if(Stage==1 && Now-At>5)
  {
   Test->TestTrue(TEXT("Real water volume recognizes full submersion"),Water->IsFullySubmerged());
   Test->TestTrue(TEXT("Equipped ballast settles into supported native walking"),Swim->IsSeafloorStanding() && Move->IsMovingOnGround());
   Test->TestTrue(TEXT("Water resistance limits walking speed"),Move->MaxWalkSpeed<130);
   Test->TestTrue(TEXT("Recorded breathing cycle is running"),Breath->IsBreathing());
   for(TActorIterator<ACoastalOceanSurface> I(W);I;++I)if(auto* Sound=I->FindComponentByClass<UOceanSoundscapeComponent>())
   {Test->TestTrue(TEXT("Ocean source banks completed asynchronous residency"),Sound->IsAudioReady());PlayingOceanVoices=FMath::Max(PlayingOceanVoices,Sound->GetPlayingVoiceCount());}
   Test->TestTrue(TEXT("Regulator emitted actual Niagara bursts"),Breath->GetExhaustCount()>0);
   Test->TestTrue(TEXT("Exhaust originates beside animated head, not torso"),FVector::Distance(Breath->GetRegulatorLocation(),P->GetMesh()->GetSocketLocation(TEXT("head")))<38);
   Test->TestTrue(TEXT("Immersive optical path is active"),Optics->GetImmersionWeight()>.95f);
   Test->TestEqual(TEXT("No lens droplets while underwater"),Optics->GetLensWetness(),0.f);
   FNeonCityVisualEvidence::CaptureHUD(W,TEXT("OceanExperience-Seafloor-Regulator.png"));StartWalk=P->GetActorLocation();At=Now;Stage=2;
   if(FParse::Param(FCommandLine::Get(),TEXT("NeonCityOceanGPUProfile")))
   {
    PC->ConsoleCommand(TEXT("r.ProfileGPU.ShowUI 0"));
    PC->ConsoleCommand(TEXT("ProfileGPU"));
   }
  }
  if(Stage==2)
  {
   P->AddMovementInput(FVector(1,0,0),1);
   if(Now-At>2)
   {
    const float Travel=FVector::Dist2D(P->GetActorLocation(),StartWalk);
    Test->TestTrue(TEXT("Seabed walking moves under ordinary input with water resistance"),Travel>30 && Travel<260);
    StartJumpZ=P->GetActorLocation().Z;PeakRise=0;bCheckedReleasedMomentum=false;
    Key(EKeys::SpaceBar,IE_Pressed);At=Now;Stage=20;
   }
  }
  else if(Stage==20 && Now-At>.08f)
  {
   Key(EKeys::SpaceBar,IE_Released);Stage=21;
  }
  else if(Stage==21)
  {
   const float AirTime=Now-At;
   PeakRise=FMath::Max(PeakRise,P->GetActorLocation().Z-StartJumpZ);
   if(!bCheckedReleasedMomentum && AirTime>.4f)
   {
    Test->TestTrue(TEXT("Brief seabed push retains upward momentum after releasing Space"),Move->IsSwimming() && Move->Velocity.Z>10);
    bCheckedReleasedMomentum=true;
   }
   if(AirTime>.8f && Swim->IsSeafloorStanding())
   {
    Test->TestTrue(TEXT("Fin stroke visibly clears the seafloor without a land jump"),PeakRise>45 && PeakRise<150);
    Test->TestTrue(TEXT("Buoyancy produces a slow return to supported walking"),AirTime>3.f && AirTime<12);
    Test->TestTrue(TEXT("Landing restores actual supported walking, not a swimming slide"),Move->IsMovingOnGround());
    Test->AddInfo(FString::Printf(TEXT("Released seabed push: peak %.1f cm, supported again after %.2f s"),PeakRise,AirTime));
    Key(EKeys::SpaceBar,IE_Pressed);At=Now;Stage=3;
   }
   else if(AirTime>12)
   {
    Test->AddError(TEXT("Brief underwater jump failed to settle back onto the seabed within twelve seconds"));
    Key(EKeys::SpaceBar,IE_Pressed);At=Now;Stage=3;
   }
  }
  else if(Stage==3 && Now-At>.3f)
  {
   Test->TestTrue(TEXT("Underwater push-off enters native swimming"),Move->IsSwimming() && !Swim->IsSeafloorStanding());
   Test->TestTrue(TEXT("Buoyant ascent avoids a land-speed jump"),Move->Velocity.Z>0 && Move->Velocity.Z<280);
   Test->TestNearlyEqual(TEXT("Leaving floor restores ground speed"),Move->MaxWalkSpeed,OriginalWalk,.1f);
   Stage=4;At=Now;
  }
  else if(Stage==4)
  {
   if(Optics->GetLensWetness()>.15f && Optics->GetImmersionWeight()<.05f)
   {
    Test->TestTrue(TEXT("Continuous real emergence wets the lens"),true);At=Now;Stage=7;
   }
   else if(Now-At>20){Key(EKeys::SpaceBar,IE_Released);Test->AddError(TEXT("Real ascent did not produce an observable lens-water exit"));Stage=5;At=Now;}
  }
  else if(Stage==7 && Now-At>.4f)
  {
   Key(EKeys::SpaceBar,IE_Released);FNeonCityVisualEvidence::CaptureHUD(W,TEXT("OceanExperience-Surface-Lens.png"));
   Test->AddInfo(FString::Printf(TEXT("Emergence eye=%s body=%s water=%.1f wetness=%.2f"),*P->GetFollowCamera()->GetComponentLocation().ToString(),*P->GetActorLocation().ToString(),Water->GetSample().SurfacePoint.Z,Optics->GetLensWetness()));
   At=Now;Stage=5;
  }
  else if(Stage==5 && Now-At>1)
  {
   Dive->Unequip();P->SetActorTransform(Land,false,nullptr,ETeleportType::TeleportPhysics);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);At=Now;Stage=6;
  }
  else if(Stage==6 && Now-At>2)
  {
   Test->TestFalse(TEXT("Removing equipment stops regulator breathing"),Breath->IsBreathing());
   Test->TestNearlyEqual(TEXT("Dry return restores ground friction"),Move->GroundFriction,OriginalFriction,.01f);
   Test->TestEqual(TEXT("Teleport is not mistaken for a wet surface exit"),Optics->GetLensWetness(),0.f);
   for(TActorIterator<ACoastalOceanSurface> It(W);It;++It)
   {
    auto* Sound=It->FindComponentByClass<UOceanSoundscapeComponent>();Test->TestNotNull(TEXT("Shared ocean has the actual sound adapter"),Sound);
    if(Sound){Test->TestTrue(TEXT("Ocean audio allocation is bounded to six voices"),Sound->GetVoiceCount()<=6);PlayingOceanVoices=FMath::Max(PlayingOceanVoices,Sound->GetPlayingVoiceCount());}
   }
   Test->AddInfo(TEXT("Automated integration fixture only. Manual A-P playthrough, listening, near-view asset acceptance and laptop performance remain separate gates."));
   if(bAudio)Test->TestTrue(TEXT("Ocean adapter has actual playing audio, not just a six-element array"),PlayingOceanVoices>0);
   if(bAudio)
   {
    FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
    const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("Audio"));
    IFileManager::Get().MakeDirectory(*Dir,true);Recording=Dir/TEXT("Ocean-Dive-Native-Master.wav");
    UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,TEXT("Ocean-Dive-Native-Master"),Dir);
    Stage=8;At=Now;return false;
   }
   return true;
  }
  else if(Stage==8)
  {
   if(IFileManager::Get().FileSize(*Recording)>32000)
   {
    TArray<uint8> Bytes;FWaveModInfo Info;
    if(!FFileHelper::LoadFileToArray(Bytes,*Recording) || !CompleteWaveExport(Bytes) || !Info.ReadWaveInfo(Bytes.GetData(),Bytes.Num())
     || !Info.pBitsPerSample || *Info.pBitsPerSample!=16 || !Info.pChannels || !*Info.pChannels || !Info.pSamplesPerSec || !*Info.pSamplesPerSec
     || Info.SampleDataSize%(sizeof(int16)*int32(*Info.pChannels)))
    {
     // StopRecordingOutput exports on a worker. File existence/size alone
     // does not mean its header and sample data have finished writing.
     if(Now-At<=6)return false;
     Test->AddError(TEXT("Ocean mixer did not finish exporting readable PCM16"));return true;
    }
    const int32 N=Info.SampleDataSize/sizeof(int16),PerSecond=int32(*Info.pChannels)*int32(*Info.pSamplesPerSec);
    double Energy=0,Peak=0;int32 Clipped=0;
    for(int32 I=0;I<N;++I)
    {
     int16 PCM=0;FMemory::Memcpy(&PCM,Info.SampleDataStart+I*sizeof(int16),sizeof(int16));
     const double V=double(PCM)/32768.;Energy+=V*V;Peak=FMath::Max(Peak,FMath::Abs(V));if(FMath::Abs(V)>.999)++Clipped;
    }
    const double RMS=N?FMath::Sqrt(Energy/N):0;
    Test->TestTrue(TEXT("Mixed diving output contains sustained non-silent audio"),PerSecond>0 && N/PerSecond>=8 && RMS>1e-5 && Peak>1e-4);
    Test->TestTrue(TEXT("Mixed diving output has no sustained digital clipping"),Clipped<FMath::Max(1,N/10000));
    Test->AddInfo(FString::Printf(TEXT("Ocean PCM RMS=%.8f peak=%.8f clipped=%d: %s; listening acceptance remains open"),RMS,Peak,Clipped,*Recording));return true;
   }
   if(Now-At>6){Test->AddError(TEXT("Ocean mixer recording was not exported"));return true;}
  }
  return false;
 }
private:
 FAutomationTestBase* Test;FNeonCityVisualEvidence Evidence;double Started=0;float At=0,OriginalWalk=0,OriginalFriction=0;int32 Stage=0;FTransform Land;FVector StartWalk;
 float StartJumpZ=0,PeakRise=0;bool bCheckedReleasedMomentum=false;
 bool bAudio=false;IConsoleVariable* AppVolume=nullptr;int32 PreviousAppVolume=0;FString Recording;
 int32 PlayingOceanVoices=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOceanExperiencePIE,"NeonCity.OceanExperience.DivingIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOceanExperiencePIE::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/NeonCity_SouthBeach_Test")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());ADD_LATENT_AUTOMATION_COMMAND(FOceanExperienceCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOceanExperienceStandalone,"NeonCity.Phase205c.Estate.Standalone.OceanAudio877",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FOceanExperienceStandalone::RunTest(const FString&)
{if(!FParse::Param(FCommandLine::Get(),TEXT("NeonCityStandaloneValidation")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityDisableAutomaticPersistence")) || !FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck"))){AddError(TEXT("Isolated native audio validation with actual playback and PCM export required"));return false;}ADD_LATENT_AUTOMATION_COMMAND(FOceanExperienceCheck(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOceanCompleteWaveExport,"NeonCity.OceanExperience.CompleteWaveExport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOceanCompleteWaveExport::RunTest(const FString&)
{
 const uint8 Header[]={82,73,70,70,52,0,0,0,87,65,86,69,102,109,116,32,16,0,0,0,1,0,2,0,128,187,0,0,0,238,2,0,4,0,16,0,100,97,116,97,16,0,0,0};
 TArray<uint8> Bytes;Bytes.Append(Header,UE_ARRAY_COUNT(Header));Bytes.AddZeroed(16);
 TestTrue(TEXT("Complete PCM16 stereo export accepted"),CompleteWaveExport(Bytes));
 TArray<uint8> Partial=Bytes;Partial.SetNum(52);TestFalse(TEXT("Partially written data rejected before tolerant parser"),CompleteWaveExport(Partial));
 Bytes[40]=20;TestFalse(TEXT("RIFF-complete but truncated data declaration rejected"),CompleteWaveExport(Bytes));
 Bytes[40]=15;TestFalse(TEXT("Data not aligned to a stereo PCM16 frame rejected"),CompleteWaveExport(Bytes));
 return true;
}
#endif
