#include "Property/NeonCityPropertySign.h"
#include "Property/PropertySubsystem.h"
#include "Property/PropertyData.h"
#include "Leisure/NeonCityActivityHUD.h"
#include "Navigation/NeonCityNavigationComponent.h"
#include "Interaction/InteractionComponent.h"
#include "World/NeonCityDayNightCycle.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace NeonCityPropertySign
{
void Notice(UWorld* W,const FText& Text,float Seconds=6)
{if(auto* PC=W?W->GetFirstPlayerController():nullptr)if(auto* Nav=PC->FindComponentByClass<UNeonCityNavigationComponent>())Nav->ShowNotice(Text,Seconds);}
void Coin(UObject* Context)
{if(auto* S=LoadObject<USoundBase>(nullptr,TEXT("/Game/NeonCity/Art/WestLake/Audio/SW_WL_UI_Coin.SW_WL_UI_Coin")))UGameplayStatics::PlaySound2D(Context,S);}
}
using namespace NeonCityPropertySign;

ANeonCityPropertySign::ANeonCityPropertySign()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 Board=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaleBoard"));Board->SetupAttachment(RootComponent);
 Board->SetCollisionProfileName(TEXT("BlockAll"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> BoardMesh(TEXT("/Game/NeonCity/Art/WestLake/Hefang/SM_WL_HF_SaleBoard.SM_WL_HF_SaleBoard"));
 if(BoardMesh.Succeeded())Board->SetStaticMesh(BoardMesh.Object);
 Interaction=CreateDefaultSubobject<UBoxComponent>(TEXT("Interaction"));Interaction->SetupAttachment(RootComponent);
 Interaction->SetBoxExtent(FVector(40,60,90));Interaction->SetRelativeLocation(FVector(0,0,90));
 Interaction->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Interaction->SetCollisionResponseToAllChannels(ECR_Ignore);
 Interaction->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Caption=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Caption"));Caption->SetupAttachment(RootComponent);
 Caption->SetRelativeLocation(FVector(0,14,205));Caption->SetRelativeRotation(FRotator(0,90,0));Caption->SetWorldSize(9);
 Caption->SetHorizontalAlignment(EHTA_Center);Caption->SetTextRenderColor(FColor(255,226,170));
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Prompt"));Prompt->SetupAttachment(RootComponent);
 Prompt->SetRelativeLocation(FVector(0,0,240));Prompt->SetWorldSize(13);Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetVisibility(false);
}
void ANeonCityPropertySign::BeginPlay()
{
 Super::BeginPlay();
 if(auto* Props=GetGameInstance()->GetSubsystem<UPropertySubsystem>())
 {
  if(Data)Props->RegisterProperty(Data);
  Props->OnPropertyChanged.AddDynamic(this,&ANeonCityPropertySign::OnPropertyChanged);
 }
 Refresh();
}
void ANeonCityPropertySign::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* GI=GetGameInstance())if(auto* Props=GI->GetSubsystem<UPropertySubsystem>())Props->OnPropertyChanged.RemoveDynamic(this,&ANeonCityPropertySign::OnPropertyChanged);
 Super::EndPlay(Reason);
}
bool ANeonCityPropertySign::IsOwned() const
{
 const auto* Props=GetGameInstance()?GetGameInstance()->GetSubsystem<UPropertySubsystem>():nullptr;
 return Data && Props && Props->IsOwned(Data->PropertyID);
}
void ANeonCityPropertySign::OnPropertyChanged(FName ID){if(Data && ID==Data->PropertyID)Refresh();}
void ANeonCityPropertySign::Refresh()
{
 if(!Data){Caption->SetText(FText::GetEmpty());return;}
 Caption->SetText(IsOwned()
  ?FText::Format(NSLOCTEXT("NeonCity","SignOwned","{0}\nYOURS  ·  ${1} a day"),Data->DisplayName,FText::AsNumber(Data->DailyIncome))
  :FText::Format(NSLOCTEXT("NeonCity","SignSale","FOR SALE\n{0}\n${1}  ·  earns ${2} a day"),Data->DisplayName,FText::AsNumber(Data->Price),FText::AsNumber(Data->DailyIncome)));
}
bool ANeonCityPropertySign::CanInteract_Implementation(AActor* Visitor) const { return Data!=nullptr && Cast<APawn>(Visitor)!=nullptr; }
FText ANeonCityPropertySign::GetInteractionName_Implementation() const
{
 if(!Data)return FText::GetEmpty();
 return IsOwned()?FText::Format(NSLOCTEXT("NeonCity","SignInfo","{0}: yours, ${1} a day at midnight"),Data->DisplayName,FText::AsNumber(Data->DailyIncome))
  :FText::Format(NSLOCTEXT("NeonCity","SignBuy","Buy {0} for ${1}"),Data->DisplayName,FText::AsNumber(Data->Price));
}
void ANeonCityPropertySign::Interact_Implementation(AActor* Visitor)
{
 if(!CanInteract_Implementation(Visitor))return;
 if(IsOwned()){Notice(GetWorld(),FText::Format(NSLOCTEXT("NeonCity","SignEarns","{0} earns you ${1} every day, paid at midnight."),Data->DisplayName,FText::AsNumber(Data->DailyIncome)));return;}
 FText Why;
 if(!GetGameInstance()->GetSubsystem<UPropertySubsystem>()->Purchase(Data->PropertyID,Why)){Notice(GetWorld(),Why);return;}
 Coin(this);
 if(auto* HUD=GetWorld()->GetSubsystem<UNeonCityActivityHUDSubsystem>())
  HUD->Banner(NSLOCTEXT("NeonCity","SignBought","PROPERTY BOUGHT"),FText::Format(NSLOCTEXT("NeonCity","SignBoughtDetail","{0}  ·  earns ${1} a day"),Data->DisplayName,FText::AsNumber(Data->DailyIncome)),FLinearColor(1.f,.72f,.25f),4);
 Notice(GetWorld(),FText::Format(NSLOCTEXT("NeonCity","SignBoughtNotice","You bought {0}. Its takings are paid to you every midnight."),Data->DisplayName),7);
}
void ANeonCityPropertySign::Tick(float Delta)
{
 Super::Tick(Delta);
 auto* PC=GetWorld()->GetFirstPlayerController();APawn* Pawn=PC?PC->GetPawn():nullptr;
 const auto* Focus=Pawn?Pawn->FindComponentByClass<UInteractionComponent>():nullptr;
 const bool Visible=Focus && Focus->GetFocusedInteractable()==this;
 Prompt->SetVisibility(Visible);
 if(Visible)
 {
  Prompt->SetText(FText::Format(NSLOCTEXT("NeonCity","SignPrompt","F  {0}"),GetInteractionName_Implementation()));
  FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);Prompt->SetWorldRotation((Eye-Prompt->GetComponentLocation()).Rotation());
 }
}

void UNeonCityBusinessSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
 Super::OnWorldBeginPlay(InWorld);
 for(TActorIterator<ANeonCityDayNightCycle> It(&InWorld);It;++It){It->OnNewDay.AddUObject(this,&UNeonCityBusinessSubsystem::PayDay);break;}
}
void UNeonCityBusinessSubsystem::PayDay(int32 GameDay)
{
 auto* Props=GetWorld()->GetGameInstance()->GetSubsystem<UPropertySubsystem>();if(!Props)return;
 const int64 Paid=Props->SettleBusinessIncome(GameDay);
 if(Paid<=0)return;
 Coin(GetWorld());
 Notice(GetWorld(),FText::Format(NSLOCTEXT("NeonCity","PayDay","Day {0}: your businesses took ${1}. Paid into your wallet."),FText::AsNumber(GameDay),FText::AsNumber(Paid)),8);
}
