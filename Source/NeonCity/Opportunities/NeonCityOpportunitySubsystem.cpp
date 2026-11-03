#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Opportunities/NeonCityOpportunityProvider.h"
#include "GameFramework/PlayerController.h"

INeonCityOpportunityProvider* UNeonCityOpportunitySubsystem::FindProvider(FName ID) const
{
 const auto* Entry=Providers.Find(ID);
 return Entry && Entry->IsValid() && Entry->Get()->GetWorld()==GetWorld()?Cast<INeonCityOpportunityProvider>(Entry->Get()):nullptr;
}
bool UNeonCityOpportunitySubsystem::RegisterProvider(UObject* Object)
{
 auto* Provider=Cast<INeonCityOpportunityProvider>(Object);
 if(!IsValid(Object) || !Provider || Object->GetWorld()!=GetWorld())return false;
 const auto Data=Provider->GetOpportunityData();
 if(Data.OpportunityID.IsNone() || Data.Title.IsEmpty() || uint8(Data.Source)>uint8(ENeonCityOpportunitySource::Relationship))return false;
 if(FindProvider(Data.OpportunityID))return Providers[Data.OpportunityID].Get()==Object;
 // Reclaim expired registrations without keeping old world actors alive.
 for(auto It=Providers.CreateIterator();It;++It)if(!It.Value().IsValid() || It.Value()->GetWorld()!=GetWorld())It.RemoveCurrent();
 if(Providers.Num()>=128)return false;
 Providers.Add(Data.OpportunityID,Object);OnOpportunitiesChanged.Broadcast();return true;
}
void UNeonCityOpportunitySubsystem::UnregisterProvider(UObject* Object)
{
 bool Removed=false;for(auto It=Providers.CreateIterator();It;++It)if(It.Value().Get()==Object) { It.RemoveCurrent();Removed=true; }
 if(Removed)OnOpportunitiesChanged.Broadcast();
}
void UNeonCityOpportunitySubsystem::NotifyChanged(UObject* Object)
{
 for(const auto& Entry:Providers)if(Entry.Value.Get()==Object) { OnOpportunitiesChanged.Broadcast();return; }
}
TArray<FNeonCityOpportunityData> UNeonCityOpportunitySubsystem::GetOpportunities() const
{
 TArray<FNeonCityOpportunityData> Result;
 for(const auto& Entry:Providers)if(const auto* Provider=FindProvider(Entry.Key))Result.Add(Provider->GetOpportunityData());
 Result.Sort([](const auto& A,const auto& B){return A.OpportunityID.LexicalLess(B.OpportunityID);});return Result;
}
bool UNeonCityOpportunitySubsystem::CanChoose(FName ID,ENeonCityOpportunityChoice Choice) const
{
 const auto* Provider=FindProvider(ID);if(!Provider)return false;
 const auto Data=Provider->GetOpportunityData();const auto State=Data.State;
 if(Choice==ENeonCityOpportunityChoice::Action)return State==ENeonCityOpportunityState::Accepted && Data.ActiveActionAvailable;
 if(Choice==ENeonCityOpportunityChoice::Cancel)return State==ENeonCityOpportunityState::Accepted && Data.Repeatable;
 if(Choice==ENeonCityOpportunityChoice::Accept && !Data.AcceptanceAvailable)return false;
 if(Choice==ENeonCityOpportunityChoice::Ignore)return State==ENeonCityOpportunityState::Offered;
 return (Choice==ENeonCityOpportunityChoice::Accept || Choice==ENeonCityOpportunityChoice::Decline)
  && (State==ENeonCityOpportunityState::Offered || State==ENeonCityOpportunityState::Ignored
   || (Choice==ENeonCityOpportunityChoice::Accept && Data.Repeatable && (State==ENeonCityOpportunityState::Completed || State==ENeonCityOpportunityState::Declined)));
}
bool UNeonCityOpportunitySubsystem::Choose(FName ID,ENeonCityOpportunityChoice Choice,APlayerController* Player)
{
 if(!IsValid(Player) || !Player->IsLocalController() || Player->GetWorld()!=GetWorld() || !CanChoose(ID,Choice))return false;
 auto* Provider=FindProvider(ID);if(!Provider || !Provider->ChooseOpportunity(Choice,Player))return false;
 OnOpportunitiesChanged.Broadcast();return true;
}
TArray<FNeonCityOpportunitySaveData> UNeonCityOpportunitySubsystem::BuildSnapshot(bool Defaults) const
{
 TArray<FNeonCityOpportunitySaveData> Result;
 for(const auto& Entry:Providers)if(const auto* Provider=FindProvider(Entry.Key))Result.Add(Provider->BuildOpportunitySnapshot(Defaults));
 Result.Sort([](const auto& A,const auto& B){return A.OpportunityID.LexicalLess(B.OpportunityID);});return Result;
}
TArray<FNeonCityMessageSaveData> UNeonCityOpportunitySubsystem::BuildDefaultMessages() const
{
 TArray<FNeonCityMessageSaveData> Result;
 for(const auto& Entry:GetOpportunities())if(const auto* Provider=FindProvider(Entry.OpportunityID))
 { auto Message=Provider->BuildDefaultOpportunityMessage();if(Message.MessageID.IsValid())Result.Add(MoveTemp(Message)); }
 return Result;
}
void UNeonCityOpportunitySubsystem::NormalizeSnapshot(TArray<FNeonCityOpportunitySaveData>& Snapshot,TArray<FString>& Warnings) const
{
 TSet<FName> Seen;
 Snapshot.RemoveAll([&](const auto& Data)
 {
  const auto* Provider=FindProvider(Data.OpportunityID);
  const bool Invalid=!Provider || Seen.Contains(Data.OpportunityID) || !Provider->IsValidOpportunitySnapshot(Data);
  Seen.Add(Data.OpportunityID);if(Invalid)Warnings.Add(TEXT("Unknown/invalid opportunity skipped; provider defaults retained."));return Invalid;
 });
}
void UNeonCityOpportunitySubsystem::ApplySnapshot(const TArray<FNeonCityOpportunitySaveData>& Snapshot)
{
 const auto Current=GetOpportunities();
 for(const auto& Entry:Current)if(auto* Provider=FindProvider(Entry.OpportunityID))
 {
  const auto* Data=Snapshot.FindByPredicate([&](const auto& Value){return Value.OpportunityID==Entry.OpportunityID;});
  Provider->ApplyOpportunitySnapshot(Data && Provider->IsValidOpportunitySnapshot(*Data)?*Data:Provider->BuildOpportunitySnapshot(true));
 }
 OnOpportunitiesChanged.Broadcast();
}
void UNeonCityOpportunitySubsystem::Deinitialize()
{ Providers.Reset();OnOpportunitiesChanged.Clear();Super::Deinitialize(); }
