# City opportunities — verified incremental checkpoint

This source began while NavigationFullRegression01 ran the previous navigation
binaries. It is excluded from that completed 122-case result. The navigation
checkpoint was committed separately before building the opportunity work. The
registry now adds ChoicesPIE (123 normal cases); conditional A/B runs are separate.

The prior phone showed only its latest message and Marcus only supported
accepting. The phone now has a browsable opportunity catalogue and message
history, explicit Accept / Decline / Later, and persisted choices. It retains the existing delivery identity, unique receipt, completion guards,
navigation and starting economy. Declining must never take money, auto-accept or
reappear on every load. Later keeps the opportunity discoverable without forcing
a task or destination. Previously accepted/completed saves keep their meanings.

The opportunity router holds weak providers keyed by stable IDs and has no tick.
Gameplay providers retain progress and rewards, and supply/validate/apply their
own value-only snapshots. The source categories are NPC message, phone, random
event, location, time and relationship. The authored Marcus delivery is the first
live provider; adding source metadata does not claim all those city activities
are implemented. A separate phone adapter presents discovery and choices.

Required gates: ordinary phone choice/browse/accept/route, no forced activity when
declined/ignored, recovery, invalid snapshots, single reward, restart restoration,
Editor/Game compilation, existing regressions and inspected real UI.
OpportunityEditorBuild01 passes. OpportunityChoices01 passes the initial input,
snapshot, identity and message-history route; all four actual 1600 x 900 screens
were inspected. OpportunityEditorBuild02/Choices02 additionally pass actual
Page Down scrolling of a disclosed long-message fixture and closing while
scrolled; its top/scrolled images were inspected. The footer remains visible.
EditorBuild03 adds pre-accept availability gating and reason text; it compiles
successfully. OpportunityFocused01 runs 13 unique cases: 12 pass, including the
ordinary Marcus drive and all persistence/property checks. PhoneDeliveryPIE fails
its historical recreated-contract assertion. The new provider registration
returned early for duplicate IDs before observing the existing paid receipt.
EditorBuild04 fixes the order: every instance first observes completion, then
attempts registration, retaining duplicate-owner protection. The historical test
is unchanged. OpportunityFocusedFix01 passes both the unchanged historical case
and the expanded opportunity case. The later Game/full checks below pass. Failed
evidence is retained. OpportunityMarcus01 passes the ordinary physical delivery
with final runtime code and the new phone layout; its actual offer/completion
views were inspected. The existing $350 receipt, map route, driving and load
guards continue to work through the provider router.

Development Editor Win64 OpportunityEditorBuild04 and Development Game Win64
OpportunityGameBuild01 both succeed with no compile/link errors. The final
OpportunityFullRegression01 queue passes all 123 unique registered cases, with
zero errors, omissions, unexpected cases or duplicate names (runner exit 0).
One LivingCityPIE warning concerns Engine r.MotionVectorSimulation render-thread
flags; no Engine installation files were changed. Banking source written while
this queue ran is excluded from these binaries and this checkpoint.

Current implementation appends Declined=3 and Ignored=4 to the existing optional
Version 1 opportunity record while preserving Offered=0, Accepted=1, Completed=2.
The provider validates its own vehicle identity and supported state; unknown IDs,
invalid states and duplicates are skipped. Existing reward receipts remain the
completion authority. Source metadata comes from the authored provider, not a
serialized actor. Choices participate in the existing explicit/safe-event saves;
this change does not introduce per-frame or periodic autosaving.

The phone adapter adds O / Opportunities, Enter / Accept, Delete / Decline and
Backspace / Later only in the open-phone consuming input context. Left/right
browses messages or opportunities. The keyboard scroll view has a persistent
Page Up / Page Down hint and uses the available screen height, so long text does
not make the closing control inaccessible. The actual choice and scroll views were inspected as recorded above.

The new normal ChoicesPIE case covers input, provider identity ownership,
malformed snapshots, ignored-to-accepted recovery, declined-load behavior and
old-message replay. Conditional A/B cases write both ignored and declined saves
and restore them in a different process. OpportunityCrossSession01 passes:
process A 31344 writes Ignored SaveID C82D12C34162AFDDBF25FF8D3F04E0C0 and Declined
SaveID 8F4619A648F0BADF154EF99672256E55; process B 19696 loads matching identities,
retains each decision, leaves the balance at 1850 and creates no unsolicited route.
Only the ignored invitation is available to accept. The isolated test slots are
deleted after B verifies them.

An unavailable assigned car now disables Accept and supplies a readable reason,
while Decline/Later remain possible. The fault-injection assertion checks this
without destroying production actors. It supplements the previous navigation
recovery test for an already accepted damaged/missing car.
