#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Phone/PhoneAppInterface.h"
#include "Economy/BankingData.h"
#include "PhoneComponent.generated.h"
class UPhoneWidget;
enum class ENeonCityPhonePageInput : uint8;
class UInputAction;
class UInputMappingContext;
class UInputComponent;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class APlayerController;

UCLASS(ClassGroup=(NeonCity), meta=(BlueprintSpawnableComponent))
class NEONCITY_API UPhoneComponent : public UActorComponent, public IPhoneAppInterface
{
	GENERATED_BODY()
public:
	UPhoneComponent();
	void SetupInput(UInputComponent* Input);
 bool ShowVehicleMarket();
 bool ShowRealEstate();
 bool ShowOpportunities();
 bool ShowBank();
 bool ShowPage(ENeonCityPhoneApp App);
 UFUNCTION(BlueprintPure,Category="Phone") ENeonCityPhoneApp GetActiveApp() const { return ActiveApp; }
 UFUNCTION(BlueprintPure,Category="Phone") FText GetDisplayedText() const { return DisplayedText; }
	UFUNCTION(BlueprintCallable, Category="NeonCity|Phone") void TogglePhone();
	UFUNCTION(BlueprintCallable, Category="NeonCity|Phone") bool AcceptLatestMessage();
	UFUNCTION(BlueprintPure, Category="NeonCity|Phone") bool IsPhoneOpen() const { return bOpen; }
	virtual ENeonCityPhoneApp GetPhoneApp_Implementation() const override { return ENeonCityPhoneApp::Messages; }
	virtual bool OpenPhoneApp_Implementation(APlayerController* Player) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
 UFUNCTION() void PageChanged();
 bool DispatchPage(ENeonCityPhonePageInput Input);
 void SwitchFishingApp();
 UPROPERTY() TObjectPtr<UInputAction> FishingAction;
	UFUNCTION() void InboxChanged();
 UFUNCTION() void AccountsChanged();
 void SetupBankInput(UEnhancedInputComponent* Input);
 void SwitchBankApp();
 void DepositInput();
 void WithdrawInput();
 void ToggleAllBankFunds();
 void IncreaseBankAmount();
 void DecreaseBankAmount();
 void QuoteLoanInput();
 void ConfirmLoanInput();
 void RepayInput();
 void LoanPurposeInput();
 void RefreshBank();
 bool IsBankActive() const { return bOpen && ActiveApp==ENeonCityPhoneApp::Bank; }
 UPROPERTY() TArray<TObjectPtr<UInputAction>> BankActions;
 bool bAllBankFunds=false;
 int64 BankAmount=100;
 ENeonCityLoanPurpose LoanPurpose=ENeonCityLoanPurpose::Vehicle;
 bool bConfirmLoan=false;
 FString BankNotice;
	void AcceptInput();
 void DeclineInput();
 void IgnoreInput();
 void SwitchOpportunityApp();
 void ScrollUp();
 void ScrollDown();
 FName SelectedAction() const;
 void BrowseMessagesOrOpportunities(int32 Direction);
 UFUNCTION() void OpportunitiesChanged();
 void ConsumeAppInput() {}
 void SwitchApp();
 void SwitchPropertyApp();
 UFUNCTION() void PropertyChanged(FName PropertyID);
 void NextVehicle();
 void PreviousVehicle();
	void Refresh();
	void RemoveInput();
	UPROPERTY() TObjectPtr<UPhoneWidget> Widget;
	UPROPERTY() TObjectPtr<UInputAction> ToggleAction;
	UPROPERTY() TObjectPtr<UInputAction> AcceptAction;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY() TObjectPtr<UInputMappingContext> AppMapping;
 UPROPERTY() TObjectPtr<UInputAction> SuppressCombatAction;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	TArray<uint32> Bindings;
	UPROPERTY() TObjectPtr<UInputAction> PropertyAction;
 UPROPERTY() TObjectPtr<UInputAction> MarketAction;
 UPROPERTY() TObjectPtr<UInputAction> NextAction;
 UPROPERTY() TObjectPtr<UInputAction> PreviousAction;
 UPROPERTY() TObjectPtr<UInputAction> OpportunityAction;
 UPROPERTY() TObjectPtr<UInputAction> DeclineAction;
 UPROPERTY() TObjectPtr<UInputAction> IgnoreAction;
 UPROPERTY() TObjectPtr<UInputAction> ScrollUpAction;
 UPROPERTY() TObjectPtr<UInputAction> ScrollDownAction;
 ENeonCityPhoneApp ActiveApp=ENeonCityPhoneApp::Messages;
 FText DisplayedText;
 int32 SelectedVehicle=0;
 int32 SelectedProperty=0;
 int32 SelectedMessage=INDEX_NONE;
 FName SelectedOpportunity;
 bool bOpen=false;
};
