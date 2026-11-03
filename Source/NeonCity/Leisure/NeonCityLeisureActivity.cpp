#include "Leisure/NeonCityLeisureActivity.h"
#include "Leisure/NeonCityActivityHUD.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Vehicles/Marine/NeonCityMarineVehicleBase.h"
#include "Vehicles/Aircraft/NeonCityAircraftVehicleBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"

ANeonCityLeisureActivity::ANeonCityLeisureActivity()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LeisureActivity")));
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=0;
}
void ANeonCityLeisureActivity::BeginPlay()
{
 Super::BeginPlay();Progress.OpportunityID=ActivityID;
 if(!ActivityID.IsNone() && !Title.IsEmpty())
  GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->RegisterProvider(this);
}
void ANeonCityLeisureActivity::EndPlay(EEndPlayReason::Type Reason)
{
 ClearRun();
 if(auto* GI=GetGameInstance())if(auto* O=GI->GetSubsystem<UNeonCityOpportunitySubsystem>())O->UnregisterProvider(this);
 Super::EndPlay(Reason);
}
ENeonCityLeisureVehicle ANeonCityLeisureActivity::Classify(const APawn* Pawn)
{
 if(Cast<ANeonCityMarineVehicleBase>(Pawn))return ENeonCityLeisureVehicle::Boat;
 if(Cast<ANeonCityAircraftVehicleBase>(Pawn))return ENeonCityLeisureVehicle::Aircraft;
 if(Cast<ANeonCityBaseVehicle>(Pawn))return ENeonCityLeisureVehicle::Car;
 return Cast<ACharacter>(Pawn)?ENeonCityLeisureVehicle::OnFoot:ENeonCityLeisureVehicle::Any;
}
FString ANeonCityLeisureActivity::FormatTime(float Seconds)
{
 Seconds=FMath::Max(0.f,Seconds);const int32 M=FMath::FloorToInt(Seconds/60);
 return FString::Printf(TEXT("%d:%04.1f"),M,Seconds-M*60);
}
float ANeonCityLeisureActivity::GetHour(const UWorld* World)
{
 if(World)for(TActorIterator<ANeonCityDayNightCycle> It(World);It;++It)return It->GetTimeOfDay();
 return 12;
}
bool ANeonCityLeisureActivity::IsOpenNow() const
{
 if(FMath::IsNearlyEqual(OpenFromHour,OpenToHour))return true;
 const float H=GetHour(GetWorld());
 return OpenFromHour<OpenToHour?(H>=OpenFromHour && H<OpenToHour):(H>=OpenFromHour || H<OpenToHour);
}
bool ANeonCityLeisureActivity::CanAcceptNow(FText& Reason) const
{
 if(IsOpenNow())return true;
 const int32 FromH=FMath::FloorToInt(OpenFromHour),FromM=FMath::RoundToInt((OpenFromHour-FromH)*60);
 Reason=FText::FromString(FString::Printf(TEXT("Opens at %02d:%02d."),FromH%24,FromM));return false;
}
int32 ANeonCityLeisureActivity::RankOf(float Seconds) const
{
 int32 Rank=1;for(const auto& R:Rivals)if(R.Seconds>0 && R.Seconds<Seconds)++Rank;return Rank;
}
FName ANeonCityLeisureActivity::GetReceiptID() const
{return FName(*FString::Printf(TEXT("Leisure.%s.%s"),*ActivityID.ToString(),*Progress.ActivityRunID.ToString(EGuidFormats::Digits)));}

FNeonCityOpportunityData ANeonCityLeisureActivity::GetOpportunityData() const
{
 FNeonCityOpportunityData Data;Data.OpportunityID=ActivityID;Data.Source=ENeonCityOpportunitySource::Phone;Data.Contact=Contact;Data.Title=Title;
 Data.RewardPreview=RewardPreview;Data.State=ENeonCityOpportunityState(Progress.State);Data.Repeatable=true;
 FText Reason;Data.AcceptanceAvailable=Progress.State==1 || CanAcceptNow(Reason);Data.UnavailableReason=Reason;
 FString Text=DescribeRules();
 if(Progress.ActivityBestSeconds>0)
 {
  Text+=FString::Printf(TEXT("\n\nYour best %s · %d completed"),*FormatTime(Progress.ActivityBestSeconds),Progress.ActivityCompletions);
  if(Rivals.Num())Text+=FString::Printf(TEXT(" · rank %d of %d"),RankOf(Progress.ActivityBestSeconds),Rivals.Num()+1);
 }
 if(Rivals.Num())
 {
  TArray<FNeonCityLeisureRival> Board=Rivals;Board.Sort([](const auto& A,const auto& B){return A.Seconds<B.Seconds;});
  Text+=TEXT("\n\nLeaderboard");
  for(int32 I=0;I<FMath::Min(3,Board.Num());++I)Text+=FString::Printf(TEXT("\n%d. %s  %s"),I+1,*Board[I].Name,*FormatTime(Board[I].Seconds));
 }
 Data.Description=FText::FromString(Text);
 return Data;
}
FNeonCityMessageSaveData ANeonCityLeisureActivity::BuildDefaultOpportunityMessage() const
{
 FNeonCityMessageSaveData M;
 const uint32 H=FCrc::StrCrc32(*ActivityID.ToString());
 M.MessageID=FGuid(0x4C455301,H,H^0x5EA50FF1,0x21000000|(H&0xFFFF));
 M.Sender=FName(*Contact.ToString());M.ActionID=ActivityID;M.Timestamp=FDateTime::UtcNow();
 M.Content=FString::Printf(TEXT("%s\n\n%s"),*Title.ToString(),*Description.ToString());
 return M;
}
bool ANeonCityLeisureActivity::ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player)
{
 if(!Player || !Player->IsLocalController() || Player->GetWorld()!=GetWorld() || !Player->GetPawn())return false;
 if(Choice==ENeonCityOpportunityChoice::Cancel){if(Progress.State!=1)return false;Fail(FText::FromString(TEXT("Cancelled. No reward was paid.")));return true;}
 if(Choice==ENeonCityOpportunityChoice::Decline || Choice==ENeonCityOpportunityChoice::Ignore)
 {if(Progress.State==1)return false;Progress.State=Choice==ENeonCityOpportunityChoice::Decline?3:4;return true;}
 if(Choice!=ENeonCityOpportunityChoice::Accept || Progress.State==1)return false;
 FText Reason;if(!CanAcceptNow(Reason)){Notify(Reason);return false;}
 for(TActorIterator<ANeonCityLeisureActivity> It(GetWorld());It;++It)if(*It!=this && It->IsRunning())
 {Notify(FText::Format(FText::FromString(TEXT("Finish or cancel {0} first.")),It->Title));return false;}
 ClearRun();
 const float Best=Progress.ActivityBestSeconds;const int32 Count=Progress.ActivityCompletions;
 Progress={};Progress.OpportunityID=ActivityID;Progress.State=1;Progress.ActivityRunID=FGuid::NewGuid();
 Progress.ActivityBestSeconds=Best;Progress.ActivityCompletions=Count;
 Customer=Player;VehicleMissingSeconds=0;
 OnAccepted(Player);
 GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);
 return true;
}
void ANeonCityLeisureActivity::Tick(float Delta)
{
 Super::Tick(Delta);
 if(Progress.State!=1)return;
 APlayerController* PC=Customer.Get();if(!PC)PC=GetWorld()->GetFirstPlayerController();
 APawn* Pawn=PC?PC->GetPawn():nullptr;if(!Pawn)return;
 const FText Why=CheckFailure(PC,Pawn);
 if(!Why.IsEmpty()){Fail(Why);return;}
 TickActivity(Delta,PC,Pawn);
}
FText ANeonCityLeisureActivity::CheckFailure(APlayerController* Player,APawn* Pawn) const
{
 if(Progress.ActivityStage>=2 && MaximumSeconds>0 && Progress.ActivitySeconds>MaximumSeconds)return FText::FromString(TEXT("Out of time."));
 return FText::GetEmpty();
}
void ANeonCityLeisureActivity::SetStage(uint8 Stage)
{
 Progress.ActivityStage=Stage;
 GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);
}
void ANeonCityLeisureActivity::Complete(int64 Reward,float Seconds,const FText& Headline,const FText& Detail)
{
 if(Progress.State!=1)return;
 auto* Economy=GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 const FName Receipt=GetReceiptID();
 if(Reward>0 && Economy && !Economy->CreditOnce(Reward,Receipt) && !Economy->HasCredit(Receipt))return;
 if(Seconds>0)Progress.ActivityBestSeconds=Progress.ActivityBestSeconds>0?FMath::Min(Progress.ActivityBestSeconds,Seconds):Seconds;
 ++Progress.ActivityCompletions;Progress.State=2;Progress.ActivityStage=0;
 if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Banner(Headline,Detail,Accent,6);
 PlayCue(TEXT("Finish"));
 EndRun();
}
void ANeonCityLeisureActivity::Fail(const FText& Reason)
{
 if(Progress.State!=1)return;
 Progress.State=3;Progress.ActivityStage=0;
 if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Banner(FText::FromString(TEXT("RUN ENDED")),Reason,FLinearColor(.9f,.25f,.2f),4);
 Notify(Reason);
 EndRun();
}
void ANeonCityLeisureActivity::EndRun()
{
 ClearRun();
 if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Clear(ActivityID);
 if(auto* PC=GetWorld()->GetFirstPlayerController())if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ClearDestination(ActivityID);
 GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);
}
void ANeonCityLeisureActivity::Notify(const FText& Text,float Seconds) const
{if(auto* PC=GetWorld()->GetFirstPlayerController())if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ShowNotice(Text,Seconds);}
void ANeonCityLeisureActivity::Guide(const FVector& Location,const FText& Label,bool Road)
{
 auto* PC=Customer.Get();if(!PC)PC=GetWorld()->GetFirstPlayerController();
 auto* Nav=PC?PC->FindComponentByClass<UNeonCityNavigationComponent>():nullptr;if(!Nav)return;
 FNeonCityNavigationDestination Goal;Goal.OwnerID=ActivityID;Goal.Label=Label;Goal.Location=Location;Goal.UseRoadRoute=Road;
 Goal.DestinationID=FName(*FString::Printf(TEXT("%s.%d.%d"),*ActivityID.ToString(),Progress.ActivityStage,Progress.ActivityCheckpoint));
 Nav->SetDestination(Goal);
}
void ANeonCityLeisureActivity::ShowHUD(const FNeonCityActivityHUDState& State) const
{if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Show(ActivityID,State);}
void ANeonCityLeisureActivity::PlayCue(const TCHAR* Name,float Volume) const
{
 const FString Path=FString::Printf(TEXT("/Game/NeonCity/Art/WestLake/Audio/SW_WL_UI_%s.SW_WL_UI_%s"),Name,Name);
 if(auto* Sound=LoadObject<USoundBase>(nullptr,*Path))UGameplayStatics::PlaySound2D(this,Sound,Volume);
}

FNeonCityOpportunitySaveData ANeonCityLeisureActivity::BuildOpportunitySnapshot(bool Defaults) const
{
 if(Defaults){FNeonCityOpportunitySaveData D;D.OpportunityID=ActivityID;return D;}
 auto D=Progress;
 // a run in progress is not resumable mid-course: it reloads as accepted, back at the start
 D.ActivityStage=0;D.ActivityCheckpoint=0;D.ActivitySeconds=0;D.ActivityPhotoFile.Reset();
 return D;
}
bool ANeonCityLeisureActivity::IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& D) const
{
 return D.OpportunityID==ActivityID && D.State<=4 && FMath::IsFinite(D.ActivityBestSeconds) && D.ActivityBestSeconds>=0
  && D.ActivityCompletions>=0 && (D.State!=1 || D.ActivityRunID.IsValid());
}
void ANeonCityLeisureActivity::ApplyOpportunitySnapshot(const FNeonCityOpportunitySaveData& D)
{
 if(!IsValidOpportunitySnapshot(D))return;
 ClearRun();Progress=D;Progress.ActivityStage=0;Progress.ActivityCheckpoint=0;Progress.ActivitySeconds=0;
 if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())HUD->Clear(ActivityID);
 if(Progress.State==1)
 {
  Customer=GetWorld()->GetFirstPlayerController();VehicleMissingSeconds=0;
  if(Customer.IsValid())OnAccepted(Customer.Get());
 }
}
