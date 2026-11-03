# World lighting — Phase 20.5c correction in progress

Overall NIGHT PLAYABILITY remains unaccepted. This document distinguishes normal
game camera comparisons from automated state checks and asset diagnostics.

The shared WorldLightingSubsystem currently owns nine original city lights,
fifty canonical village fixtures, working-boat deck illumination and helicopter
searchlight photometry. One clock input supports late-loaded actors and materials;
one 4 Hz scheduler enforces 24 active lights / six dynamic shadow lights. The
source-village emission retains the original procedural window decisions.

`TidewaterLightingClock01`: two cases pass, including existing SouthBeachPIE and
new WorldBudgetPIE. The latter checks late night registrations, idempotence,
invalid-policy/clock rejection, global near shadow/dynamic budgets, middle/far
tiers, retained far emission, manual disabling, dawn and teardown.
`TidewaterLightingIntegration01`: four cases pass: WorldBudgetPIE, original
SearchLightPIE, ReferenceBoatLoopPIE and ReferenceVillageWalkPIE. This includes
real helicopter L input, powered flight, daytime reduction, far culling and
physical return/landing; the optical inspection views remain diagnostic.

Matched ordinary player-camera comparisons are retained in Saved/VisualOverhaul:

- TidewaterNightRoadBefore01: existing road lights illuminate parts of the street,
  but the sky is black and the broader building/transport lighting gates are open.
- TidewaterNightPierBefore01: actual island game view is effectively black. The
  village had looked up a clock only once before the fixture clock was created.
- TidewaterNightRoadManager01: local road/sidewalk illumination and shadows remain,
  a dim moonlit sky appears. This is a fixed view, not the continuous street route.
- TidewaterNightPierManager01: late-registration repair activates lamps/windows,
  but unrestricted adaptation reaches EV100 -5 and turns night into apparent day.
  It also exposes an engine cached-lighting exposure warning. **VISUAL FAIL**;
  do not hide the warning or count this as a night acceptance.

`TidewaterNightPierExposure02` adds the bounded fallback and was inspected: the
wooden route is illuminated, the forest remains dark against the blue night sky,
and village windows are visible. The cached-lighting exposure warning is gone;
one unrelated Engine motion-vector console-access warning remains in the report.
The original urban volume's minimum EV100 2 remains explicit and is not silently
overwritten. `TidewaterLightingEditor04` and `TidewaterForestLightingGame01` compile
and link successfully. `TidewaterForestLightingRegression01` completes all 139
registered cases successfully in 1620.17 s (136 plain / three with warnings; no
missing, unexpected or duplicate cases). The warnings concern Engine motion-vector
console access and VSM marking-queue capacity in fishing/forest views.

That run also exposes two pre-queue reflected-struct initialization errors, which
were not included in individual automation results. `FNeonCityReferencePlant`
now explicitly initializes both colour fields and its transform. All runners now
reject `LogClass: Error:` even before tests begin. `TidewaterForestDefaultsBuild01`
rebuilds Editor (11.84 s) and Game (17.75 s); `TidewaterForestDefaultsChecks01`
passes the source data, ordinary forest views, lighting budget and original world
tests, with no reflected-class errors. Remaining rendering warnings are retained.
The 139-case result is not a claim of a previously clean startup or performance.
No settings or screenshots may substitute for the six continuous night routes or
final human playtest.

## Forest integration follow-up — 2026-09-26

TidewaterForestBudgetNightRoad01 and TidewaterForestBudgetNightPier01 both pass
after the native grass, palm contact, mask repair and Epic rendering-budget
changes. Their ordinary game views were inspected: the road/sidewalk retain
light and shadows; the pier route and village windows remain lit against a dark
forest and sky. Each records the existing motion-vector warning and no test
errors. These are the same fixed viewpoints, not the six continuous night routes.
PalmContactBuild02 compiles Development Editor and Game with zero compile/link
errors. TidewaterGrassFoliageRegression01 now passes 147/147 registered tests
in 1731.05 s, retaining only the two documented engine rendering warnings.
