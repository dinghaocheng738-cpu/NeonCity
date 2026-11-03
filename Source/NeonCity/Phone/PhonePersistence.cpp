#include "Phone/PhoneSubsystem.h"
#include "Phone/MessageSystem.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Engine/GameInstance.h"
TArray<FNeonCityMessageSaveData> UMessageSystem::BuildPersistenceSnapshot() const
{
 TArray<FNeonCityMessageSaveData> Result;
 for(const auto& Message:Messages)
 {
  FNeonCityMessageSaveData Data; Data.MessageID=Message.Id; Data.Sender=Message.Sender;
  Data.Content=Message.Content.ToString(); Data.Timestamp=Message.Timestamp; Data.ActionID=Message.ActionId; Result.Add(Data);
 }
 return Result;
}
void UMessageSystem::ApplyPersistenceSnapshot(const TArray<FNeonCityMessageSaveData>& Snapshot)
{
 Messages.Reset();
 for(const auto& Data:Snapshot)
 {
  FPhoneMessage Message; Message.Id=Data.MessageID; Message.Sender=Data.Sender;
  Message.Content=FText::FromString(Data.Content); Message.Timestamp=Data.Timestamp; Message.ActionId=Data.ActionID; Messages.Add(Message);
 }
 OnInboxChanged.Broadcast();
}
FNeonCityPhoneSaveData UPhoneSubsystem::BuildPersistenceSnapshot(bool Defaults) const
{
 FNeonCityPhoneSaveData Result;
 const auto* Opportunities=GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>();
 Result.Messages=Defaults?Opportunities->BuildDefaultMessages():GetGameInstance()->GetSubsystem<UMessageSystem>()->BuildPersistenceSnapshot();
 Result.Opportunities=Opportunities->BuildSnapshot(Defaults);
 return Result;
}
void UPhoneSubsystem::NormalizePersistenceSnapshot(FNeonCityPhoneSaveData& Snapshot,TArray<FString>& Warnings) const
{
 TSet<FGuid> MessageIDs;
 Snapshot.Messages.RemoveAll([&](const auto& Message)
 {
  const bool Invalid=!Message.MessageID.IsValid() || MessageIDs.Contains(Message.MessageID) || Message.Sender.IsNone() || Message.Content.IsEmpty() || Message.Content.Len()>8192;
  MessageIDs.Add(Message.MessageID); if(Invalid) Warnings.Add(TEXT("Invalid/duplicate phone message skipped.")); return Invalid;
 });
 GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NormalizeSnapshot(Snapshot.Opportunities,Warnings);
}
void UPhoneSubsystem::ApplyPersistenceSnapshot(const FNeonCityPhoneSaveData& Snapshot)
{
 GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->ApplySnapshot(Snapshot.Opportunities);
 GetGameInstance()->GetSubsystem<UMessageSystem>()->ApplyPersistenceSnapshot(Snapshot.Messages);
}
