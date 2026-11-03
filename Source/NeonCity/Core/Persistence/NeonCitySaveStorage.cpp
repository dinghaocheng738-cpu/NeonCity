#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Core/Persistence/NeonCitySaveGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "TimerManager.h"

namespace NeonCitySaveStorage
{
 constexpr int32 MaximumBytes=4*1024*1024;
 const FString Backup=TEXT("_Backup"),Pending=TEXT("_Pending");
 constexpr ANSICHAR Footer[]="NCHECK01";
 constexpr int32 FooterBytes=sizeof(Footer)-1;
}
bool UNeonCitySaveSubsystem::Finish(ENeonCitySaveResult Result,const FString& Detail)
{
 LastResult=Result; LastDetail=Detail; OnSaveOperation.Broadcast(Result,Detail);
 UE_LOG(LogTemp,Display,TEXT("Persistence [%d] %s: %s"),int32(Result),*SlotName,*Detail);
 return Result==ENeonCitySaveResult::Success || Result==ENeonCitySaveResult::Deferred;
}
bool UNeonCitySaveSubsystem::ConfigureSlot(const FString& Name,bool EnableAutosave)
{
 if(bBusy) return Finish(ENeonCitySaveResult::Busy,TEXT("Save operation already active."));
 if(Name.IsEmpty() || Name.Len()>64 || Name.EndsWith(NeonCitySaveStorage::Backup) || Name.EndsWith(NeonCitySaveStorage::Pending)) return Finish(ENeonCitySaveResult::InvalidSlot,TEXT("Invalid or reserved slot name."));
 for(TCHAR C:Name) if(!FChar::IsAlnum(C) && C!='_' && C!='-') return Finish(ENeonCitySaveResult::InvalidSlot,TEXT("Slot names require letters, digits, underscore or hyphen."));
 if(GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReadyTimer); GetWorld()->GetTimerManager().ClearTimer(AutosaveTimer); }
 SlotName=Name; PendingOperation=0; bAutosaveEnabled=EnableAutosave; bWriteProtected=false; LastWriteTime=-1000; return true;
}
bool UNeonCitySaveSubsystem::DoesSaveExist() const
{
 return UGameplayStatics::DoesSaveGameExist(SlotName,0) || UGameplayStatics::DoesSaveGameExist(SlotName+NeonCitySaveStorage::Backup,0)
  || UGameplayStatics::DoesSaveGameExist(SlotName+NeonCitySaveStorage::Pending,0);
}
bool UNeonCitySaveSubsystem::Encode(UNeonCitySaveGame* Model,TArray<uint8>& Bytes) const
{
 if(!UGameplayStatics::SaveGameToMemory(Model,Bytes) || Bytes.Num()>NeonCitySaveStorage::MaximumBytes-64) return false;
 const FString Hash=FMD5::HashBytes(Bytes.GetData(),Bytes.Num()); FTCHARToUTF8 Digest(*Hash);
 Bytes.Append(reinterpret_cast<const uint8*>(Digest.Get()),32);
 Bytes.Append(reinterpret_cast<const uint8*>(NeonCitySaveStorage::Footer),NeonCitySaveStorage::FooterBytes); return true;
}
UNeonCitySaveGame* UNeonCitySaveSubsystem::ReadGeneration(const FString& Name,ENeonCitySaveResult& Result,FString& Detail) const
{
 Result=ENeonCitySaveResult::Missing; Detail=TEXT("Save generation does not exist.");
 if(!UGameplayStatics::DoesSaveGameExist(Name,0)) return nullptr;
 TArray<uint8> Bytes; Result=ENeonCitySaveResult::Corrupt; Detail=TEXT("Save integrity check failed; no state applied.");
 constexpr int32 Trailer=32+NeonCitySaveStorage::FooterBytes;
 if(!UGameplayStatics::LoadDataFromSlot(Bytes,Name,0) || Bytes.Num()<=Trailer || Bytes.Num()>NeonCitySaveStorage::MaximumBytes) return nullptr;
 const int32 Payload=Bytes.Num()-Trailer;
 if(FMemory::Memcmp(Bytes.GetData()+Bytes.Num()-NeonCitySaveStorage::FooterBytes,NeonCitySaveStorage::Footer,NeonCitySaveStorage::FooterBytes)!=0) return nullptr;
 const FString Hash=FMD5::HashBytes(Bytes.GetData(),Payload); FTCHARToUTF8 Digest(*Hash);
 if(FMemory::Memcmp(Bytes.GetData()+Payload,Digest.Get(),32)!=0) return nullptr;
 Bytes.SetNum(Payload);
 auto* Model=Cast<UNeonCitySaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)); if(!Model) return nullptr;
 Result=ENeonCitySaveResult::UnsupportedVersion; if(!Model->MigrateToCurrent(Detail)) return nullptr;
 Result=ENeonCitySaveResult::InvalidData; if(!Model->ValidateEnvelope(Detail)) return nullptr;
 Result=ENeonCitySaveResult::Success; Detail=TEXT("Validated SaveGame generation."); return Model;
}
UNeonCitySaveGame* UNeonCitySaveSubsystem::ReadBestGeneration()
{
 ENeonCitySaveResult Result; FString Detail;
 for(const FString& Name:{SlotName,SlotName+NeonCitySaveStorage::Backup,SlotName+NeonCitySaveStorage::Pending})
 {
  if(auto* Model=ReadGeneration(Name,Result,Detail))
  { if(Name!=SlotName) RestoreWarnings.Add(TEXT("Recovered a validated backup/staged generation.")); return Model; }
  if(Result==ENeonCitySaveResult::UnsupportedVersion) { bWriteProtected=true; Finish(Result,Detail); return nullptr; }
 }
 bWriteProtected=DoesSaveExist(); Finish(bWriteProtected?ENeonCitySaveResult::Corrupt:ENeonCitySaveResult::Missing,TEXT("No valid save generation; existing files preserved.")); return nullptr;
}
UNeonCitySaveGame* UNeonCitySaveSubsystem::ReadSaveForInspection()
{ RestoreWarnings.Reset(); return ReadBestGeneration(); }
bool UNeonCitySaveSubsystem::WriteGeneration(UNeonCitySaveGame* Model)
{
 if(bWriteProtected) return Finish(ENeonCitySaveResult::InvalidData,TEXT("Slot protected after failed load. Delete or choose another slot explicitly."));
 FString Error; if(!Model->MigrateToCurrent(Error) || !Model->ValidateEnvelope(Error)) return Finish(ENeonCitySaveResult::InvalidData,Error);
 ENeonCitySaveResult ReadResult; FString ReadDetail;
 auto* Previous=ReadGeneration(SlotName,ReadResult,ReadDetail);
 if(ReadResult==ENeonCitySaveResult::UnsupportedVersion) { bWriteProtected=true; return Finish(ReadResult,ReadDetail); }
 if(!Previous) Previous=ReadGeneration(SlotName+NeonCitySaveStorage::Backup,ReadResult,ReadDetail);
 if(ReadResult==ENeonCitySaveResult::UnsupportedVersion) { bWriteProtected=true; return Finish(ReadResult,ReadDetail); }
 if(!Previous && DoesSaveExist())
 {
  Previous=ReadGeneration(SlotName+NeonCitySaveStorage::Pending,ReadResult,ReadDetail);
  if(!Previous) { bWriteProtected=true; return Finish(ENeonCitySaveResult::Corrupt,TEXT("Refusing to overwrite unreadable generations.")); }
 }
 TArray<uint8> Bytes; if(!Encode(Model,Bytes)) return Finish(ENeonCitySaveResult::IOError,TEXT("Serialization failed or exceeded 4 MiB."));
 const FString Pending=SlotName+NeonCitySaveStorage::Pending,Backup=SlotName+NeonCitySaveStorage::Backup;
 if(!UGameplayStatics::SaveDataToSlot(Bytes,Pending,0) || !ReadGeneration(Pending,ReadResult,ReadDetail)) return Finish(ENeonCitySaveResult::IOError,TEXT("Staged write/verification failed; previous generation retained."));
 if(Previous)
 {
  TArray<uint8> PreviousBytes;
  if(!Encode(Previous,PreviousBytes) || !UGameplayStatics::SaveDataToSlot(PreviousBytes,Backup,0) || !ReadGeneration(Backup,ReadResult,ReadDetail)) return Finish(ENeonCitySaveResult::IOError,TEXT("Backup verification failed; main retained."));
 }
 if(!UGameplayStatics::SaveDataToSlot(Bytes,SlotName,0) || !ReadGeneration(SlotName,ReadResult,ReadDetail)) return Finish(ENeonCitySaveResult::IOError,TEXT("Main verification failed; recovery generation retained."));
 UGameplayStatics::DeleteGameInSlot(Pending,0); LastWriteTime=FPlatformTime::Seconds(); return true;
}
bool UNeonCitySaveSubsystem::DeleteSave()
{
 if(IsOperationInProgress()) return Finish(ENeonCitySaveResult::Busy,TEXT("Save/load operation active."));
 TGuardValue<bool> Guard(bBusy,true); bAutosaveEnabled=false;
 if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AutosaveTimer);
 for(const FString& Name:{SlotName,SlotName+NeonCitySaveStorage::Backup,SlotName+NeonCitySaveStorage::Pending})
  if(UGameplayStatics::DoesSaveGameExist(Name,0) && !UGameplayStatics::DeleteGameInSlot(Name,0)) return Finish(ENeonCitySaveResult::IOError,TEXT("Could not delete every generation."));
 bWriteProtected=false; return Finish(ENeonCitySaveResult::Success,TEXT("All generations deleted; autosave disabled until explicitly enabled."));
}
#if WITH_DEV_AUTOMATION_TESTS
bool UNeonCitySaveSubsystem::WriteTestFixture(UNeonCitySaveGame* Data,bool CorruptBytes)
{
 if(!GIsAutomationTesting || !SlotName.StartsWith(TEXT("NeonCity_Test_")) || !Data || bBusy) return false;
 TArray<uint8> Bytes; if(!Encode(Data,Bytes)) return false;
 if(CorruptBytes && Bytes.Num()>100) Bytes[100]^=0x5a;
 return UGameplayStatics::SaveDataToSlot(Bytes,SlotName,0);
}
#endif
