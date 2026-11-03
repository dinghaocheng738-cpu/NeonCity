#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"

FNeonCityOpportunitySaveData ANeonCityTransportTest::BuildPersistenceSnapshot(bool Defaults) const
{
 FNeonCityOpportunitySaveData Result;Result.OpportunityID=ReceiptKey();Result.State=Defaults?0:uint8(State);
 if(IsValid(DeliveryVehicle))Result.VehicleID=DeliveryVehicle->VehicleID;return Result;
}
FNeonCityMessageSaveData ANeonCityTransportTest::BuildDefaultMessage() const
{
 FNeonCityMessageSaveData Message;Message.MessageID=FGuid(0xCE210001,0x57B847F5,0xA9BBA018,0x88232610);
 Message.Sender=TEXT("Marcus");Message.ActionID=ReceiptKey();Message.Timestamp=FDateTime::UtcNow();
 Message.Content=FString::Printf(TEXT("Can you deliver my Starter Sedan? It is parked near your apartment.\n\nReward: $%lld\nAccept to mark the car on your map. The road guide will appear when you enter it."),Reward);return Message;
}
bool ANeonCityTransportTest::IsValidOpportunitySnapshot(const FNeonCityOpportunitySaveData& Data) const
{
 return Data.OpportunityID==ReceiptKey() && IsValid(DeliveryVehicle) && Data.VehicleID==DeliveryVehicle->VehicleID
  && Data.State<=uint8(ENeonCityTransportState::Ignored);
}
void ANeonCityTransportTest::ApplyPersistenceSnapshot(const FNeonCityOpportunitySaveData& Snapshot)
{
 if(auto* PC=GetWorld()->GetFirstPlayerController())if(auto* Navigation=PC->FindComponentByClass<UNeonCityNavigationComponent>())
 { Navigation->ClearDestination(ReceiptKey());Navigation->ShowNotice(FText::GetEmpty(),0); }
 UnavailableReported=false;StoppedSeconds=0;Customer.Reset();Marker->SetVisibility(false,true);
 // Preserve Version 1 meanings 0/1/2. A receipt remains authoritative even when
 // the optional opportunity section is absent or malformed.
 const uint8 SavedState=IsValidOpportunitySnapshot(Snapshot)?Snapshot.State:0;
 State=GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->HasCredit(ReceiptKey())
  ?ENeonCityTransportState::Completed:static_cast<ENeonCityTransportState>(SavedState);
 if(State==ENeonCityTransportState::Accepted) { Customer=GetWorld()->GetFirstPlayerController();Marker->SetVisibility(true,true); }
 SetActorTickEnabled(State==ENeonCityTransportState::Accepted && IsValid(DeliveryVehicle));UpdateNavigation();
}
