#include "Missions/Prototype/NeonCityTransportTest.h"
#include "Core/Subsystems/NeonCitySaveSubsystem.h"
#include "Opportunities/NeonCityOpportunitySubsystem.h"
#include "Phone/MessageSystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "Vehicles/NeonCityBaseVehicle.h"
#include "Combat/HealthComponent.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ANeonCityTransportTest::ANeonCityTransportTest()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.TickInterval=0.1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DeliveryOrigin")));
	Marker=CreateDefaultSubobject<USceneComponent>(TEXT("DeliveryMarker"));
	Marker->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/NeonCity/Environment/Materials/M_SB_Yellow.M_SB_Yellow"));
	for (int32 I=0; I<4; ++I)
	{
		auto* Line=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("DeliveryLine%d"),I));
		Line->SetupAttachment(Marker);
		Line->SetStaticMesh(Cube.Object); Line->SetMaterial(0,Paint.Object);
		const bool bSide=I<2;
		Line->SetRelativeLocation(bSide ? FVector(0,I==0 ? -220:220,0) : FVector(I==2 ? -450:450,0,0));
		Line->SetRelativeScale3D(bSide ? FVector(9,0.08,0.02) : FVector(0.08,4.4,0.02));
		Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	auto* Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("DeliveryLabel"));
	Label->SetupAttachment(Marker); Label->SetRelativeLocation(FVector(-450,0,190));
	Label->SetRelativeRotation(FRotator(0,0,0));
	Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(30);
	Label->SetTextRenderColor(FColor::Yellow);
	Label->SetText(FText::FromString(TEXT("MARCUS DELIVERY\nStop inside the yellow zone")));
	Marker->SetVisibility(false,true);
}
void ANeonCityTransportTest::Send(const FText& Text,FName Action)
{
	GetGameInstance()->GetSubsystem<UMessageSystem>()->SendMessage(TEXT("Marcus"),Text,Action);
}
void ANeonCityTransportTest::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !IsValid(DeliveryVehicle) || Reward<=0 || Reward>10000) { SetActorTickEnabled(false); return; }
	// Receipt authority precedes registration: even a duplicate/recreated actor
	// must observe completion without taking over the existing provider's ID.
	if(GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->HasCredit(ReceiptKey()))State=ENeonCityTransportState::Completed;
	const bool Registered=GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->RegisterProvider(this);
	if(!Registered || State==ENeonCityTransportState::Completed) { SetActorTickEnabled(false);return; }
	Send(FText::FromString(FString::Printf(TEXT("Can you deliver my Starter Sedan? It is parked near your apartment.\n\nReward: $%lld\nAccept to mark the car on your map. The road guide will appear when you enter it."),Reward)),ReceiptKey());
}
bool ANeonCityTransportTest::Accept(APlayerController* Player)
{
	if (!HasAuthority() || (State!=ENeonCityTransportState::Offered && State!=ENeonCityTransportState::Ignored) || !IsValid(DeliveryVehicle) || DeliveryVehicle->GetHealthComponent()->IsDead()
		|| !IsValid(Player) || Player!=GetWorld()->GetFirstPlayerController() || !Player->GetPawn()) { return false; }
	Customer=Player; State=ENeonCityTransportState::Accepted;
	SetActorTickEnabled(true);
	Marker->SetVisibility(true,true);
	Send(FText::FromString(TEXT("Delivery accepted.\n\nFollow the map marker to my Starter Sedan. Use F to enter. Follow the road guide to the marked delivery zone, then stop inside for one second.\n\nP closes your phone to show the map.")));
	UpdateNavigation();
	return true;
}
FNeonCityOpportunityData ANeonCityTransportTest::GetOpportunityData() const
{
 FNeonCityOpportunityData Data;Data.OpportunityID=ReceiptKey();Data.Source=ENeonCityOpportunitySource::NPCMessage;
 Data.Contact=NSLOCTEXT("NeonCity","MarcusContact","Marcus");Data.Title=NSLOCTEXT("NeonCity","MarcusOpportunity","Coastal car delivery");
 Data.Description=NSLOCTEXT("NeonCity","MarcusOpportunityDescription","Find Marcus' sedan near your apartment, follow the road guide and stop in the delivery zone.");
 Data.RewardPreview=Reward;Data.State=static_cast<ENeonCityOpportunityState>(State);
 Data.AcceptanceAvailable=IsValid(DeliveryVehicle) && !DeliveryVehicle->GetHealthComponent()->IsDead();
 if(!Data.AcceptanceAvailable)Data.UnavailableReason=NSLOCTEXT("NeonCity","MarcusCannotAccept","Assigned sedan unavailable. This delivery cannot start right now.");
 return Data;
}
bool ANeonCityTransportTest::ChooseOpportunity(ENeonCityOpportunityChoice Choice,APlayerController* Player)
{
 if(!HasAuthority() || !IsValid(Player) || Player!=GetWorld()->GetFirstPlayerController())return false;
 if(Choice==ENeonCityOpportunityChoice::Accept)return Accept(Player);
 if(State!=ENeonCityTransportState::Offered && State!=ENeonCityTransportState::Ignored)return false;
 if(Choice==ENeonCityOpportunityChoice::Decline)
 {
  State=ENeonCityTransportState::Declined;
  Send(NSLOCTEXT("NeonCity","MarcusDeclined","No problem. I'll arrange another driver. You haven't been charged."));
 }
 else if(Choice==ENeonCityOpportunityChoice::Ignore && State==ENeonCityTransportState::Offered)State=ENeonCityTransportState::Ignored;
 else return false;
 SetActorTickEnabled(false);Marker->SetVisibility(false,true);Customer.Reset();
 if(auto* Nav=Player->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ClearDestination(ReceiptKey());
 return true;
}
void ANeonCityTransportTest::UpdateNavigation()
{
 if(!Customer.IsValid())return;
 auto* Navigation=Customer->FindComponentByClass<UNeonCityNavigationComponent>();if(!Navigation)return;
 if(State!=ENeonCityTransportState::Accepted || !IsValid(DeliveryVehicle) || DeliveryVehicle->GetHealthComponent()->IsDead())
 { Navigation->ClearDestination(ReceiptKey());return; }
 const bool DrivingAssigned=Customer->GetPawn()==DeliveryVehicle;
 FNeonCityNavigationDestination Destination;Destination.OwnerID=ReceiptKey();
 Destination.DestinationID=DrivingAssigned?FName(TEXT("SouthBeach.Marcus.Delivery")):FName(TEXT("SouthBeach.Marcus.AssignedSedan"));
 Destination.Label=DrivingAssigned?NSLOCTEXT("NeonCity","MarcusDeliver","Deliver Marcus' sedan"):NSLOCTEXT("NeonCity","MarcusFindCar","Find Marcus' sedan");
 Destination.Location=DrivingAssigned?GetActorLocation():DeliveryVehicle->GetActorLocation();
 if(!DrivingAssigned)Destination.MovingTarget=DeliveryVehicle;
 Destination.UseRoadRoute=DrivingAssigned;
 if(DrivingAssigned && FVector::Dist2D(GetActorLocation(),DeliveryVehicle->GetActorLocation())<500)
  Destination.Label=NSLOCTEXT("NeonCity","MarcusStop","Stop in the marked zone");
 Navigation->SetDestination(Destination);
}
void ANeonCityTransportTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EvaluateDelivery(DeltaSeconds);
	UpdateNavigation();
}
void ANeonCityTransportTest::EvaluateDelivery(float DeltaSeconds)
{
	if (!HasAuthority() || State!=ENeonCityTransportState::Accepted) { return; }
	if (!IsValid(DeliveryVehicle) || DeliveryVehicle->GetHealthComponent()->IsDead())
	{
		StoppedSeconds=0; Marker->SetVisibility(false,true);
		if(!UnavailableReported)
		{
		 UnavailableReported=true;Send(NSLOCTEXT("NeonCity","MarcusUnavailable","The assigned sedan is unavailable. Delivery is paused; no payment has been issued."));
		 if(Customer.IsValid())if(auto* Navigation=Customer->FindComponentByClass<UNeonCityNavigationComponent>())Navigation->ShowNotice(NSLOCTEXT("NeonCity","NavDeliveryPaused","Delivery paused\nAssigned car unavailable"));
		}
		return;
	}
	if(UnavailableReported) { UnavailableReported=false;Marker->SetVisibility(true,true); }
	const FVector Local=GetActorTransform().InverseTransformPosition(DeliveryVehicle->GetActorLocation());
	const bool bEligible=Customer.IsValid() && Customer->GetPawn()==DeliveryVehicle
		&& FMath::Abs(Local.X)<450 && FMath::Abs(Local.Y)<220 && FMath::Abs(Local.Z)<200
		&& DeliveryVehicle->GetVelocity().Size()<80;
	StoppedSeconds=bEligible ? StoppedSeconds+DeltaSeconds : 0;
	if (StoppedSeconds<1.f) { return; }
	auto* Economy=GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
	if (!Economy->CreditOnce(Reward,ReceiptKey()) && !Economy->HasCredit(ReceiptKey())) { return; }
	State=ENeonCityTransportState::Completed;
	GetGameInstance()->GetSubsystem<UNeonCityOpportunitySubsystem>()->NotifyChanged(this);
	Marker->SetVisibility(false,true); SetActorTickEnabled(false);
	Send(FText::FromString(FString::Printf(TEXT("Delivery complete. Thanks!\n\n$%lld has been added to your cash.\nCurrent balance: $%lld"),Reward,Economy->GetBalance())));
	if(Customer.IsValid())if(auto* Navigation=Customer->FindComponentByClass<UNeonCityNavigationComponent>())
	{
	 Navigation->ClearDestination(ReceiptKey());
	 Navigation->ShowNotice(FText::FromString(FString::Printf(TEXT("Delivery complete  +$%lld\nCash  $%lld"),Reward,Economy->GetBalance())));
	}
 GetGameInstance()->GetSubsystem<UNeonCitySaveSubsystem>()->RequestAutosave(ENeonCityAutosaveReason::TransportCompleted);
}
void ANeonCityTransportTest::EndPlay(EEndPlayReason::Type Reason)
{
	if(Customer.IsValid())if(auto* Navigation=Customer->FindComponentByClass<UNeonCityNavigationComponent>())Navigation->ClearDestination(ReceiptKey());
	if (UGameInstance* GI=GetGameInstance())
	{
		if (auto* Opportunities=GI->GetSubsystem<UNeonCityOpportunitySubsystem>()) { Opportunities->UnregisterProvider(this); }
	}
	Super::EndPlay(Reason);
}
