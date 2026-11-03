#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include <limits>
#include "World/Audio/NeonCityReferenceSoundscape.h"
#include "World/Marine/NeonCityReferenceVegetation.h"
#include "World/NeonCityDayNightCycle.h"
#include "Player/NeonCityPlayerCharacter.h"
#include "Combat/HealthComponent.h"
#include "Tests/NeonCityVisualEvidence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Audio.h"
#include "EngineUtils.h"

namespace
{
TSharedPtr<FJsonObject> ReadSoundOracle(const TCHAR* Name,FAutomationTestBase* Test)
{
 FString Text;TSharedPtr<FJsonObject> Data;
 const FString Path=FPaths::ProjectDir()/TEXT("ArtSource/VerticalSlice/Tidewater/Audio")/Name;
 if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))
 {Test->AddError(TEXT("Missing reviewed sound oracle: ")+Path);return nullptr;}
 Test->TestEqual(TEXT("Pinned audio source"),Data->GetStringField(TEXT("source_commit")),FString(TEXT("4811ba48d795197de5621985f404e765c0b7c0ef")));
 return Data;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceSoundMathTest,"NeonCity.Audio.ReferenceCoastAndMixer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceSoundMathTest::RunTest(const FString&)
{
 const auto Coast=ReadSoundOracle(TEXT("CoastReference.json"),this);
 const auto Sound=ReadSoundOracle(TEXT("SoundscapeReference.json"),this);if(!Coast || !Sound)return false;
 double MaxCoast=0,MaxMix=0;
 for(const auto& Value:Coast->GetArrayField(TEXT("samples")))
 {
  const auto C=Value->AsObject();double D=0,B=0;
  if(!NeonCityReferenceCoast::Query(C->GetNumberField(TEXT("x")),C->GetNumberField(TEXT("z")),D,B))return false;
  MaxCoast=FMath::Max(MaxCoast,FMath::Abs(D-C->GetNumberField(TEXT("d"))));
  MaxCoast=FMath::Max(MaxCoast,FMath::Abs(B-C->GetNumberField(TEXT("beachZone"))));
 }
 const auto Channels=Sound->GetObjectField(TEXT("channels"));
 auto* Data=LoadObject<UNeonCityReferenceSoundscapeData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape"));
 if(!TestNotNull(TEXT("Native source sound metadata"),Data))return false;
 for(const auto& Value:Sound->GetArrayField(TEXT("cases")))
 {
  const auto C=Value->AsObject();NeonCityForestSound::FEnvironment E;
  E.Hour=C->GetNumberField(TEXT("hour"));E.Day=C->GetNumberField(TEXT("day"));
  E.Wind=C->GetNumberField(TEXT("wind"));const double D=C->GetNumberField(TEXT("coast"));E.bOnLand=D<0;E.DistanceInland=FMath::Abs(D);
  const double Gust=NeonCityForestSound::AdvanceGust(C->GetNumberField(TEXT("gust_before")),C->GetNumberField(TEXT("gust_target")),C->GetNumberField(TEXT("dt")));
  const auto Mix=NeonCityForestSound::Evaluate(E,Gust);
  MaxMix=FMath::Max(MaxMix,FMath::Abs(Gust-C->GetNumberField(TEXT("gust_after"))));
  const FName Names[]={TEXT("wind"),TEXT("palms"),TEXT("crickets"),TEXT("birds_dawn")};
  const double Targets[]={Mix.Wind,Mix.Palms,Mix.Crickets,Mix.Dawn};
  for(int32 I=0;I<4;++I)
  {
   const auto* B=Data->Find(Names[I]);if(!B){AddError(TEXT("Missing loop metadata"));return false;}
   const double Gain=Targets[I]/NeonCityForestSound::Db(B->LoudnessLUFS[0])*(B->Channels==1?NeonCityForestSound::Db(-3):1);
   const double Expected=C->GetObjectField(TEXT("beds"))->GetObjectField(Names[I].ToString())->GetNumberField(TEXT("gain"));
   MaxMix=FMath::Max(MaxMix,FMath::Abs(Gain-Expected));
  }
  const auto Bird=C->GetObjectField(TEXT("bird_activity"));
  MaxMix=FMath::Max(MaxMix,FMath::Abs(Mix.BirdActivity-Bird->GetNumberField(TEXT("act"))));
  MaxMix=FMath::Max(MaxMix,FMath::Abs(Mix.Lull-Bird->GetNumberField(TEXT("lull"))));
  MaxMix=FMath::Max(MaxMix,FMath::Abs(Mix.WindLowPass-C->GetObjectField(TEXT("filters"))->GetNumberField(TEXT("wind_lowpass"))));
 }
 for(const auto& Value:Sound->GetArrayField(TEXT("footsteps")))
 {
  const auto Row=Value->AsObject();const auto* B=Data->Find(FName(*Row->GetStringField(TEXT("bank"))));
  if(!B)return false;const auto& Gains=Row->GetArrayField(TEXT("slice_gains"));
  TestEqual(TEXT("All surface slices retained"),Gains.Num(),B->Clips.Num());
  for(int32 I=0;I<Gains.Num() && I<B->LoudnessLUFS.Num();++I)
   MaxMix=FMath::Max(MaxMix,FMath::Abs(NeonCityForestSound::SourceGain(Row->GetNumberField(TEXT("target_lufs")),B->LoudnessLUFS[I],B->Channels)-Gains[I]->AsNumber()));
 }
 TestTrue(TEXT("521 coast queries match the source in metres"),MaxCoast<1e-5);
 TestTrue(TEXT("162 environment cases and six footstep banks match source loudness"),MaxMix<1e-6);
 double D=0,B=0;TestFalse(TEXT("Invalid coast input is rejected"),NeonCityReferenceCoast::Query(std::numeric_limits<double>::infinity(),0,D,B));
 AddInfo(FString::Printf(TEXT("Source differential max coast %.12g m, mix %.12g"),MaxCoast,MaxMix));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceSoundLibraryTest,"NeonCity.Audio.ReferenceLibrary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceSoundLibraryTest::RunTest(const FString&)
{
 auto* Data=LoadObject<UNeonCityReferenceSoundscapeData>(nullptr,TEXT("/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape"));
 if(!TestNotNull(TEXT("Native reviewed sound library"),Data))return false;
 TestTrue(TEXT("Complete pinned bank validates before use"),Data->IsValidLibrary());
 int32 Loops=0,Shots=0;
 for(const auto& B:Data->Banks)
 {
  if(B.bLoop)++Loops;else Shots+=B.Clips.Num();
  for(int32 I=0;I<B.Clips.Num();++I)
  {
   auto* Wave=B.Clips[I].LoadSynchronous();if(!TestNotNull(TEXT("Licensed clip is present"),Wave))continue;
   TestEqual(TEXT("Decoded channels match loudness correction"),Wave->NumChannels,B.Channels);
   TestEqual(TEXT("Loop behavior is native"),bool(Wave->bLooping),B.bLoop);
   TestTrue(TEXT("Original cut timing retained"),FMath::Abs(Wave->Duration-B.Durations[I])<.003f);
  }
 }
 TestEqual(TEXT("Original loop count"),Loops,14);TestEqual(TEXT("Original phrase/step count"),Shots,130);
 TestNull(TEXT("Unknown bank is safe"),Data->Find(TEXT("UnknownClip")));return true;
}

namespace
{
class FForestSoundWalk : public IAutomationLatentCommand
{
public:
 explicit FForestSoundWalk(FAutomationTestBase* In):Test(In){}
 ~FForestSoundWalk() override
 {
  if(AppVolume)AppVolume->Set(PreviousAppVolume,ECVF_SetByCode);
 }
 bool Update() override
 {
  if(!Started)Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Forest audio route timeout"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;auto* PC=W?W->GetFirstPlayerController():nullptr;
  auto* P=PC?Cast<ANeonCityPlayerCharacter>(PC->GetPawn()):nullptr;if(!P || W->GetTimeSeconds()<.2f)return false;
  auto* M=P->GetCharacterMovement();const float Now=W->GetTimeSeconds();
  if(Stage==0)
  {
   FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestAudioStart),false,P);int32 Owners=0;
   for(TActorIterator<ANeonCityReferenceVegetation> It(W);It;++It)
   {
    Params.AddIgnoredActor(*It);
    if(It->GrassData){++Owners;Audio=It->FindComponentByClass<UNeonCityReferenceSoundscapeComponent>();}
   }
   Test->TestEqual(TEXT("Exactly one canonical audio owner"),Owners,1);
   if(!Test->TestNotNull(TEXT("Island soundscape component"),Audio.Get()))return true;
   FHitResult Hit;if(!W->LineTraceSingleByChannel(Hit,FVector(4900,15000,40000),FVector(4900,15000,-10000),ECC_Visibility,Params))
   {Test->AddError(TEXT("Missing authored forest floor"));return true;}
   M->SetMovementMode(MOVE_Falling);P->SetActorLocation(Hit.ImpactPoint+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+5),false,nullptr,ETeleportType::TeleportPhysics);
   M->StopMovementImmediately();PC->SetControlRotation(FRotator(-8,90,0));
   Clock=W->SpawnActor<ANeonCityDayNightCycle>();Clock->SetCycleEnabled(false);Clock->SetTimeOfDay(6);
   W->SpawnActor<ASkyAtmosphere>();Stage=1;Until=Now+10;
   bAudio=FParse::Param(FCommandLine::Get(),TEXT("NeonCityAudioCheck"));
   if(bAudio)
   {
    // Offscreen PIE never acquires desktop focus. Bypass only the app-focus
    // mute during this explicit mixer test; don't edit user/editor preferences.
    AppVolume=IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume"));
    if(!AppVolume){Test->AddError(TEXT("Missing native app-volume test control"));return true;}
    PreviousAppVolume=AppVolume->GetInt();AppVolume->Set(1,ECVF_SetByCode);
    Test->TestTrue(TEXT("Real audio device enabled"),Audio->IsAudioEnabled());
    UAudioMixerBlueprintLibrary::StartRecordingOutput(W,45);
   }
   return false;
  }
  Evidence.Record(W,PC,Stage);
  if(!Audio.IsValid()){Test->AddError(TEXT("Soundscape owner vanished during route"));return true;}
  if(Audio->GetVoiceAllocationCount()>UNeonCityReferenceSoundscapeComponent::VoiceBudget)
  {Test->AddError(TEXT("Sound voice budget exceeded"));return true;}
  if(Stage==1 && Now>=Until)
  {
   Test->TestTrue(TEXT("Dawn chorus follows the shared clock"),Audio->GetLoopGain(3)>.01f);
   Test->TestTrue(TEXT("Bird bouts occur at source dawn activity"),Audio->GetBirdEventCount()>0);
   Test->TestEqual(TEXT("Standing among trees does not produce footsteps"),Audio->GetStepEventCount(),0);
   if(bAudio)Test->TestTrue(TEXT("Real source sound playback"),Audio->GetPlayingVoiceCount()>0);
   Origin=P->GetActorLocation();Stage=2;Until=Now+10;return false;
  }
  if(Stage==2)
  {
   const FVector2D To=FVector2D(4950,16200)-FVector2D(P->GetActorLocation());
   if(To.Size()>55 && Now<Until){PC->SetControlRotation(FRotator(-8,FMath::RadiansToDegrees(FMath::Atan2(To.Y,To.X)),0));P->DoMove(0,1);return false;}
   Test->TestTrue(TEXT("Normal input traverses the forest floor"),FVector::Dist2D(Origin,P->GetActorLocation())>900 && M->IsMovingOnGround());
   Test->TestTrue(TEXT("Actual grounded travel triggers steps"),Audio->GetStepEventCount()>4);
   Test->TestEqual(TEXT("High inland ground uses original grass bank"),Audio->GetLastStepSurface(),FName(TEXT("grass")));
   Clock->SetTimeOfDay(11);Stage=3;Until=Now+1;return false;
  }
  if(Stage==3 && Now>=Until){P->Jump();Stage=4;Until=Now+1;return false;}
  if(Stage==4)
  {
   if(!M->IsFalling() && Now<Until)return false;
   Test->TestTrue(TEXT("Template jump remains available"),M->IsFalling());AirSteps=Audio->GetStepEventCount();Stage=5;Until=Now+.25f;return false;
  }
  if(Stage==5 && Now>=Until)
  {
   Test->TestEqual(TEXT("No steps while jumping"),Audio->GetStepEventCount(),AirSteps);P->StopJumping();
   Clock->SetTimeOfDay(0);NightBirds=Audio->GetBirdEventCount();Stage=6;Until=Now+8;return false;
  }
  if(Stage==6 && Now>=Until)
  {
   Test->TestTrue(TEXT("Night crickets replace dawn bed"),Audio->GetLoopGain(2)>.03f && Audio->GetLoopGain(3)<.01f);
   Test->TestTrue(TEXT("Night does not start new bird phrases"),Audio->GetBirdEventCount()<=NightBirds+1);
   Test->TestEqual(TEXT("Sound does not injure the player"),P->GetHealthComponent()->GetCurrentHealth(),100.f);
   Test->AddInfo(FString::Printf(TEXT("Forest sound events steps=%d birds=%d allocated=%d"),Audio->GetStepEventCount(),Audio->GetBirdEventCount(),Audio->GetVoiceAllocationCount()));
   if(bAudio)
   {
    FString Run;FParse::Value(FCommandLine::Get(),TEXT("NeonCityArtRun="),Run);
    const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VisualOverhaul")/Run/TEXT("Audio"));
    IFileManager::Get().MakeDirectory(*Dir,true);Recording=Dir/TEXT("Forest-Native-Master.wav");
    UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,TEXT("Forest-Native-Master"),Dir);
    Stage=7;Until=Now+6;return false;
   }
   return true;
  }
  if(Stage==7)
  {
   if(IFileManager::Get().FileSize(*Recording)>32000)
   {
    TArray<uint8> Bytes;FWaveModInfo Info;
    if(!FFileHelper::LoadFileToArray(Bytes,*Recording) || !Info.ReadWaveInfo(Bytes.GetData(),Bytes.Num())
     || !Info.pBitsPerSample || *Info.pBitsPerSample!=16 || !Info.pChannels || !Info.pSamplesPerSec)
    {Test->AddError(TEXT("Native mixer recording is not readable PCM16"));return true;}
    const int32 Samples=Info.SampleDataSize/sizeof(int16);
    const int32 PerSecond=int32(*Info.pChannels)*int32(*Info.pSamplesPerSec);
    double Energy=0,Peak=0,WindowEnergy=0;int32 AudibleSeconds=0;
    for(int32 I=0;I<Samples;++I)
    {
     int16 PCM=0;FMemory::Memcpy(&PCM,Info.SampleDataStart+I*sizeof(int16),sizeof(int16));
     const double V=double(PCM)/32768.;Energy+=V*V;WindowEnergy+=V*V;Peak=FMath::Max(Peak,FMath::Abs(V));
     if(PerSecond>0 && (I+1)%PerSecond==0)
     {if(FMath::Sqrt(WindowEnergy/PerSecond)>1e-5)++AudibleSeconds;WindowEnergy=0;}
    }
    const double RMS=Samples>0?FMath::Sqrt(Energy/Samples):0;
    Test->TestTrue(TEXT("Native output has sustained non-silent audio, not only a WAV header or one click"),
     PerSecond>0 && Samples/PerSecond>=8 && RMS>1e-5 && Peak>1e-4 && AudibleSeconds>=6);
    Test->AddInfo(FString::Printf(TEXT("Native mixer PCM RMS=%.8f peak=%.8f audible_seconds=%d: %s"),RMS,Peak,AudibleSeconds,*Recording));
    return true;
   }
   if(Now>=Until){Test->AddError(TEXT("Native mixer did not export the requested recording"));return true;}
  }
  return false;
 }
private:
 FAutomationTestBase* Test;double Started=0;float Until=0;int32 Stage=0,AirSteps=0,NightBirds=0;
 bool bAudio=false;IConsoleVariable* AppVolume=nullptr;int32 PreviousAppVolume=0;
 FString Recording;FVector Origin=FVector::ZeroVector;
 TWeakObjectPtr<UNeonCityReferenceSoundscapeComponent> Audio;
 TWeakObjectPtr<ANeonCityDayNightCycle> Clock;
 FNeonCityVisualEvidence Evidence{TEXT("ForestSoundWalk")};
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceForestSoundWalkTest,"NeonCity.Audio.ForestSoundWalkPIE",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FReferenceForestSoundWalkTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/NeonCity/Maps/Marine/NeonCity_Tidewater_Island")));
 ADD_LATENT_AUTOMATION_COMMAND(FNeonCityStartVisualPIECommand());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FForestSoundWalk>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
