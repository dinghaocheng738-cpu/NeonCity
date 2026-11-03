# Banking experience — incremental validation passed

This work implements the banking portion of Section 46. It began while
OpportunityFullRegression01 was running the previous binary, and is excluded
from that 123-case result. The whole vertical slice, varied income activities,
asset trading and business ownership remain incomplete.

## Account authority and player flow

NeonCityEconomySubsystem owns cash, bank, one active loan and the bounded ledger.
The starting design remains $1850 cash, $0 bank and no loan. Existing GetBalance,
CurrencyComponent, PlayerData.Money, activity rewards and cash spending retain
their meaning. Transfers move whole currency units atomically between accounts;
there is no second independent wallet or intermediate debit broadcast.

The existing phone app router opens Bank with B. Home deposits the selected
amount, End withdraws, Up/Down adjust $100 steps up to $2000, Insert toggles all
available funds (including small remainders), and T chooses the
loan purpose. L presents the quote and Enter confirms; reviewing terms never
disburses funds. K repays the selected amount or a smaller final remainder from
bank. Failed actions explain themselves. Closing the phone removes these input
bindings; moving/driving controls remain unchanged. Page Up/Down scrolls history.

BankPhoneApp only registers the app and validates its local controller.
PhoneBanking presents account data and requests actions. It never computes or
sets authoritative balances. No new 3D model, texture, audio or external asset is
used in this code/text UI increment, so no new art acquisition is claimed.

## Prototype loan policy

The current game defaults are a $100 minimum, $2000 absolute maximum, at most
50% of pre-loan liquid cash plus bank, and a disclosed 10% one-time fee rounded
up to a whole currency unit. The four policy values are Game config properties
on NeonCityEconomySubsystem. Runtime bounds prevent invalid config or large
balances overflowing quote arithmetic. Only one loan may be outstanding.
Vehicle, Property and Business are purpose values; a purpose does not itself
grant an asset, buy property or instantiate a business. Those player loops are
separate unfinished workstreams. Disbursement goes to bank, repayments debit
bank, and there is no recurring interest, wall-clock penalty or automatic debit.
The accepted loan retains its original fee when policy later changes. Quotes
are revalidated at execution, so stale quotes cannot create funds.

All cash, transfer, loan, repayment and restore operations reject nested account
mutations while notifying listeners. Both account balances, active debt and the
transaction are committed before notification. Original cash listeners receive
cash changes; OnAccountsChanged also covers bank/debt-only changes. Rejected
operations create neither history rows nor reward claims.

## Persistence

The value-only optional tagged Version 1 economy extension contains HasBanking,
BankBalance, ActiveLoan and up to 256 transactions. Loan IDs and transaction IDs
are GUIDs, with UTC receipt times. A loan stores purpose, original principal,
one-time fee and remaining debt. Every transaction stores cash/bank/debt deltas
and resulting balances. Fully paid active loans clear; ledger receipts retain
their loan ID. The separate bounded reward-ID set remains the duplicate-payment
authority even after history is trimmed.

Pre-banking Version 1 files lack HasBanking: restore their original cash and
reward receipts, default bank/debt to zero, and do not invent missing history.
New-game snapshots explicitly store empty banking state. No schema version is
silently downgraded. The existing SaveSubsystem is still the only disk boundary;
no autosave is added to every account change and no separate finance file exists.

Envelope validation rejects negative accounts, inconsistent/missing loan IDs,
invalid purposes, impossible amounts and oversized records before restoring any
world state. Normalization removes malformed/duplicate informational ledger rows
with warnings and discards stale history if its last account totals disagree.
History never derives balances or decides whether a reward has been paid.
ApplySnapshot also validates independently, emits state notifications and does
not replay a deposit, disbursal or reward.

## Required evidence

- Unit account conservation, overflow/overdraft, quote eligibility and staleness,
  partial/full repayment, same-ID restore, corrupt records, legacy compatibility,
  bounded history, reward protection and real multicast reentrancy.
- Actual phone keys for opening, denied withdrawal, deposit, withdrawal, reviewed
  borrowing, repayment, save/load, UI refresh and closed-screen input isolation.
- Separate A/B engine processes with matching SaveID, LoanID, cash, bank, debt,
  loan terms and every ledger field. B must also settle/save/reload the loan.
- Editor/Game Development Win64 builds, relevant historical economy/persistence/
  navigation/property checks and the complete registered regression queue.
- Individually inspect actual 1600 x 900 account/terms/payment/restored screens.

BankingEditorBuild01 succeeds with zero compile/link errors. BankingUnits01
runs four unique cases: BankingLedger, historical Invariants and Schema pass;
BankingCompatibility fails because its new diagnostic SaveGame fixture omitted
the required FormatID. The fixture is corrected to use NeonCity.SaveGame without
relaxing production validation. Subsequent evidence below completes those gates.
Failed evidence is retained.

BankingEditorBuild02 and BankingGameBuild01 both succeed with zero compile/link
errors. BankingFocused01 passes all 16 unique selected economy, opportunity,
navigation, persistence and property cases, including the corrected compatibility
fixture and the unchanged historical economy assertions. BankingPhone01 passes
actual keys plus the nested-load boundary check; all six actual 1600 x 900 views
(accounts, deposit, reviewed terms, repayment, restore and all-funds) were
individually inspected. Long history is deliberately scrollable with an always
visible close hint; these are functional screens, not final phone artwork.

BankingCrossSession01 passes independent processes A 30132 and B 19020.
SaveID 8223B8D745C20E3A1BF0868AA54CF134 and loan ID
E52415E642B24ADE4B4210A7B8309519 match. Both have cash 1750, bank 100,
debt 10 and five identical transaction records including IDs, deltas, totals,
reasons and UTC ticks. B repeats Load without disbursal, repays the final 10,
saves/reloads the cleared debt and deletes only its isolated slot. No production
save is modified. Conditional A/B cases are separate from the normal registry.

BankingFullRegression01 passes all 126 unique registered cases in one queue,
with zero errors, failures, omissions, unexpected names or duplicate names.
The report records 124 successes and two successes with warnings, taking
1395.9565 seconds. LivingCityPIE retains the Engine r.MotionVectorSimulation
render-thread warning; ThrottleLoopPIE reports a non-Nanite virtual-shadow-map
marking queue overflow. This is a performance follow-up, not a clean long-session
benchmark. Existing compile deprecations are also retained; no Engine installation
source is edited. Fishing source started during the queue is excluded from its
binaries and from this banking checkpoint.
