#include "Phone/PhoneComponent.h"
#include "Phone/PhoneWidget.h"
#include "Phone/PhoneSubsystem.h"
#include "Economy/NeonCityEconomySubsystem.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
namespace
{
const TCHAR* PurposeName(ENeonCityLoanPurpose Purpose)
{
 switch(Purpose) { case ENeonCityLoanPurpose::Property:return TEXT("Property");case ENeonCityLoanPurpose::Business:return TEXT("Business");default:return TEXT("Vehicle"); }
}
const TCHAR* EntryName(ENeonCityTransactionType Type)
{
 switch(Type)
 {
 case ENeonCityTransactionType::Spending:return TEXT("Cash payment");
 case ENeonCityTransactionType::Deposit:return TEXT("Deposit");
 case ENeonCityTransactionType::Withdrawal:return TEXT("Withdrawal");
 case ENeonCityTransactionType::Loan:return TEXT("Loan received");
 case ENeonCityTransactionType::Repayment:return TEXT("Loan repayment");
 default:return TEXT("Cash received");
 }
}
}
void UPhoneComponent::SetupBankInput(UEnhancedInputComponent* Input)
{
 BankActions.Reset();
 const auto Bind=[&](FKey Key,void (UPhoneComponent::*Method)())
 {
  auto* Action=NewObject<UInputAction>(this);Action->ValueType=EInputActionValueType::Boolean;Action->bConsumeInput=true;
  BankActions.Add(Action);AppMapping->MapKey(Action,Key);
  Bindings.Add(Input->BindAction(Action,ETriggerEvent::Started,this,Method).GetHandle());
 };
 Bind(EKeys::Insert,&UPhoneComponent::ToggleAllBankFunds);
 Bind(EKeys::B,&UPhoneComponent::SwitchBankApp);Bind(EKeys::Home,&UPhoneComponent::DepositInput);Bind(EKeys::End,&UPhoneComponent::WithdrawInput);
 Bind(EKeys::Up,&UPhoneComponent::IncreaseBankAmount);Bind(EKeys::Down,&UPhoneComponent::DecreaseBankAmount);
 Bind(EKeys::L,&UPhoneComponent::QuoteLoanInput);Bind(EKeys::K,&UPhoneComponent::RepayInput);Bind(EKeys::T,&UPhoneComponent::LoanPurposeInput);
}
bool UPhoneComponent::ShowBank()
{
 auto* PC=Cast<APlayerController>(GetOwner());if(!PC || !PC->IsLocalController())return false;
 bConfirmLoan=false;BankNotice.Reset();ActiveApp=ENeonCityPhoneApp::Bank;if(!bOpen)TogglePhone();Refresh();return bOpen;
}
void UPhoneComponent::SwitchBankApp()
{
 if(!bOpen)return;bConfirmLoan=false;
 GetWorld()->GetGameInstance()->GetSubsystem<UPhoneSubsystem>()->OpenApp(IsBankActive()?ENeonCityPhoneApp::Messages:ENeonCityPhoneApp::Bank,Cast<APlayerController>(GetOwner()));
}
void UPhoneComponent::DepositInput()
{
 if(!IsBankActive())return;bConfirmLoan=false;
 auto* Economy=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 const int64 Amount=bAllBankFunds?Economy->GetBalance():BankAmount;const bool Success=Economy->Deposit(Amount);
 BankNotice=Success?FString::Printf(TEXT("Deposited $%lld."),Amount):TEXT("Deposit declined. Check your cash balance and bank capacity.");Refresh();
}
void UPhoneComponent::WithdrawInput()
{
 if(!IsBankActive())return;bConfirmLoan=false;
 auto* Economy=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 const int64 Amount=bAllBankFunds?Economy->GetBankBalance():BankAmount;const bool Success=Economy->Withdraw(Amount);
 BankNotice=Success?FString::Printf(TEXT("Withdrew $%lld."),Amount):TEXT("Withdrawal declined. Check your bank balance and cash capacity.");Refresh();
}
void UPhoneComponent::ToggleAllBankFunds()
{ if(IsBankActive()) { bAllBankFunds=!bAllBankFunds;bConfirmLoan=false;BankNotice.Reset();Refresh(); } }
void UPhoneComponent::IncreaseBankAmount()
{ if(IsBankActive()) { bAllBankFunds=false;BankAmount=FMath::Min<int64>(2000,BankAmount+100);bConfirmLoan=false;BankNotice.Reset();Refresh(); } }
void UPhoneComponent::DecreaseBankAmount()
{ if(IsBankActive()) { bAllBankFunds=false;BankAmount=FMath::Max<int64>(100,BankAmount-100);bConfirmLoan=false;BankNotice.Reset();Refresh(); } }
void UPhoneComponent::LoanPurposeInput()
{ if(IsBankActive()) { LoanPurpose=ENeonCityLoanPurpose((uint8(LoanPurpose)+1)%3);bConfirmLoan=false;BankNotice.Reset();Refresh(); } }
void UPhoneComponent::QuoteLoanInput()
{
 if(!IsBankActive())return;
 if(bAllBankFunds) { bConfirmLoan=false;BankNotice=TEXT("Choose a fixed loan amount with UP / DOWN first.");Refresh();return; }
 const auto Quote=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->GetLoanQuote(BankAmount,LoanPurpose);
 bConfirmLoan=Quote.Allowed;BankNotice=Quote.Allowed?TEXT("Review the terms below before confirming."):Quote.Reason.ToString();Refresh();
}
void UPhoneComponent::ConfirmLoanInput()
{
 if(!IsBankActive() || !bConfirmLoan)return;bConfirmLoan=false;
 const bool Success=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>()->RequestLoan(BankAmount,LoanPurpose);
 BankNotice=Success?FString::Printf(TEXT("Loan received: $%lld credited to bank."),BankAmount):TEXT("Loan declined. Review the updated terms.");Refresh();
}
void UPhoneComponent::RepayInput()
{
 if(!IsBankActive())return;bConfirmLoan=false;
 auto* Economy=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();const int64 Amount=FMath::Min(bAllBankFunds?Economy->GetBankBalance():BankAmount,Economy->GetDebt());
 const bool Success=Economy->RepayLoan(Amount);
 BankNotice=Success?FString::Printf(TEXT("Repaid $%lld from bank."),Amount):TEXT("No repayment made. Check your loan and bank balance.");Refresh();
}
void UPhoneComponent::RefreshBank()
{
 auto* Economy=GetWorld()->GetGameInstance()->GetSubsystem<UNeonCityEconomySubsystem>();
 const auto Quote=Economy->GetLoanQuote(BankAmount,LoanPurpose);const auto Loan=Economy->GetActiveLoan();
 FString Text=FString::Printf(TEXT("NEONCITY / BANK\nCash  $%lld     Bank  $%lld\nLoan outstanding  $%lld\n\nAmount  %s    UP / DOWN  Adjust\nINS  All / fixed amount\nHOME  Deposit    END  Withdraw\n\n"),Economy->GetBalance(),Economy->GetBankBalance(),Economy->GetDebt(),bAllBankFunds?TEXT("ALL"):*FString::Printf(TEXT("$%lld"),BankAmount));
 if(!BankNotice.IsEmpty())Text+=BankNotice+TEXT("\n\n");
 if(Loan.Remaining>0)
  Text+=FString::Printf(TEXT("%s loan\nBorrowed $%lld + fee $%lld\nRemaining $%lld\nK  Repay up to $%lld from bank\n\n"),PurposeName(Loan.Purpose),Loan.Principal,Loan.Fee,Loan.Remaining,FMath::Min(bAllBankFunds?Economy->GetBankBalance():BankAmount,Loan.Remaining));
 else
 {
  Text+=FString::Printf(TEXT("Loan purpose  %s    T  Change\nAvailable limit  $%lld\n"),PurposeName(LoanPurpose),Quote.Limit);
  if(bAllBankFunds)Text+=TEXT("Choose a fixed loan amount with UP / DOWN.\n");
  else if(Quote.Allowed)
  {
   Text+=FString::Printf(TEXT("Receive $%lld  |  One-time fee $%lld\nTotal to repay $%lld\n"),Quote.Principal,Quote.Fee,Quote.TotalRepayment);
   Text+=bConfirmLoan?TEXT("ENTER  Confirm loan    B  Cancel\n"):TEXT("L  Review loan\n");
  }
  else Text+=Quote.Reason.ToString()+TEXT("\n");
  Text+=TEXT("One active loan. No recurring interest.\n\n");
 }
 Text+=TEXT("B  Messages    P  Close\n\nRECENT TRANSACTIONS\n");
 const auto& History=Economy->GetTransactions();
 if(History.IsEmpty())Text+=TEXT("No recorded transactions.\n");
 for(int32 I=History.Num()-1;I>=FMath::Max(0,History.Num()-8);--I)
 {
  const auto& E=History[I];Text+=FString::Printf(TEXT("%s UTC  %s\n"),*E.Timestamp.ToString(TEXT("%m/%d %H:%M")),EntryName(E.Type));
  if(E.Amount)Text+=FString::Printf(TEXT("Cash %+lld  "),E.Amount);
  if(E.BankDelta)Text+=FString::Printf(TEXT("Bank %+lld  "),E.BankDelta);
  if(E.DebtDelta)Text+=FString::Printf(TEXT("Loan %+lld"),E.DebtDelta);
  Text+=TEXT("\n");
 }
 DisplayedText=FText::FromString(Text);Widget->SetPhoneText(DisplayedText);
}
