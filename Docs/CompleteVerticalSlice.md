# Phase 20.5c — complete vertical slice program

Starting HEAD: `d4f29a1b2ed446b083a70401d879ec9a270a8b00`.
Branch: `codex/phase20-5c-vertical-slice`.
Snapshot: `pre-complete-vertical-slice-20260925`.
User's `Edit-FileText.ps1` remains untracked and outside this program.

Overall state: **IN PROGRESS / PLAYER EXPERIENCE NOT COMPLETE / ART PASS NOT COMPLETE**.
Current acceptance source: `Requirements/Phase20_5c_PlayerFeedbackRegister.txt`.
All PF-001–PF-035 are tracked in `PlayerFeedbackRegister.md`; old technical passes
do not close player-reported failures. Connect the island to normal South Beach
before further independent-island polish. Current overall acceptance is FAIL.
Phase 21 is prohibited until explicit user acceptance. Do not describe a class,
test, debug fixture or imported asset as a completed player experience.

The authoritative attached program is retained in
`Requirements/Phase20_5c_Request.txt`. The user's later resource-discovery rules
override the original-first asset preference in that attachment: search lawful
existing resources before modeling or material work. See ProfessionalAssetPipeline.

## Workstreams and acceptance

Each delivery reads existing architecture, implements an incremental player loop,
builds Editor/Game, runs relevant automation and cross-system checks, verifies
normal South Beach PIE, updates these records and makes a scoped logical commit.
Do not bundle unrelated workstreams into a single terminal commit. Partial
blockers are recorded while independent work continues.

| Stream | Scope | Current status / next evidence |
|---|---|---|
| A | Licensed resources, coherent buildings/vehicles/coast/props | IN PROGRESS: licence receipts, 3 adapted CC0 environment assets and 9 PIE views; whole art pass incomplete |
| B | Character animation, damage feedback, police/weapon defect | IN PROGRESS: eight active clips, real skeletal/input captures, Editor/Game builds and 13 focused checks passed; first full run 114/115 plus successful updated living-city repeat; long-gun/finger/recovery polish open |
| C | Occupants, theft, collection, modifications, convertible, racing | IN PROGRESS: seven player transport classes have visible seated drivers and verified contacts; whole vehicle experience remains NOT COMPLETE |
| D | Navigation, minimap, opportunities, complete Marcus loop | IN PROGRESS: navigation checkpoint passes 122 tests; generic choices/history/restart and Marcus route pass, with all 123 registered cases successful; wider acceptance pending |
| E | Banking, varied jobs and economy feedback | IN PROGRESS: cash/bank/loan/history phone loop, focused/restart checks and all 126 banking regressions pass; varied jobs remain pending |
| F | Boat throttle, yacht, fishing, collection, aquarium, cooking, fish pen | IN PROGRESS: speedboat loop passes; coastal fishing/catch/market/grill/phone integration is under actual-route validation; yacht/aquarium/pen and remaining marine loops NOT COMPLETE |
| G | Diving, salvage, submarine, sonar, reused-combat torpedo | NOT COMPLETE |
| H | Aviation art/occupants/power/HUD/weapons/light and activities | IN PROGRESS: pilot contact, power/instruments and reviewed search-light input/visual checks verified; final uninterrupted flight, rotor audio and activities remain NOT COMPLETE |
| I | Autonomous events, NPC pursuit/memory/relationships | NOT COMPLETE |
| J | Property, business, camper, pet, photos, nightlife, transport | NOT COMPLETE |
| K | Gameplay weather and injury/hospital/recovery | NOT COMPLETE |
| L | Coherent phone/HUD/audio, save migration, 30–60 minute flow, stress/performance | NOT COMPLETE |

Dependencies: A/B supply real animation and art to occupants/combat; D/E give
navigation/rewards to jobs and activities; shared persistence snapshots remain
the integration boundary. Existing combat/vehicle/water/physical systems stay
authoritative. Houdini Apprentice output must not enter commercial production.

## Final gates

- Entry, controls, animation/UI/audio feedback, success, failure, recovery,
  cross-system consequence, exit and save/restart/load per major loop.
- Separate Technical / Player Experience / Visual / Performance status.
- All historic unique tests retained; repetitions reported separately.
- Genuine normal-game continuous PIE evidence, day/sunset/night/rain and a long
  session monitoring actors, memory, frame cost and save integrity.
- RTX 5060 Laptop, 1600×900, Epic, 100% baseline; no hidden quality reduction.
- User final playtest remains required even after internal acceptance.

## Commit record

First checkpoint: reviewed CC0 environment intake, adapted roads/sand/hydrants,
source/import ledgers and licence documentation. 114 unique tests executed; 113
passed in the full run, the one fixed-wing impact failure passed an unchanged
isolated repeat. Follow up the intermittent impact assertion in the next gate.
Animation imports in this checkpoint are evaluation assets, not active gameplay.
Final completion commit is prohibited before all completion gates are met.

Second checkpoint: shared skeletal weapon/hit/collapse presentation and licence
evaluation sources. 115 unique registered tests ran; 114 passed in the full run,
and the old single-node animation assertion was migrated to graph plus actual
foot motion and passed its focused repeat. Detailed evidence and remaining
visual defects are in CharacterPresentationValidation.md. No transport occupant
implementation is included in this character checkpoint.

Third checkpoint: visible transport occupants, non-stretch contact IK and owned
cockpit ergonomic derivatives. Editor/Game Development Win64 builds pass;
TransportOccupants04 passes with seven individually inspected real PIE views.
VerticalSliceOccupantRegression01 passes all 116 unique registered cases in one
queue with no omissions or duplicate names. Earlier failed runs remain recorded.
This is a bounded occupant checkpoint, not completion of Workstreams C or H.

Fourth checkpoint: a reviewed CC0 pistol base is adapted into a separately owned,
unbranded model with authored palm/support/muzzle sockets. The shared character
wrapper adds opt-in non-stretch hand contact, releasing it during reload/hit/downed
states. Editor/Game Development builds pass. ReviewedPistolPresentation03 passes
strict live contact checks and its actual aim/reload poses were inspected.
ReviewedPistolFullRegression01 passes 116/116 unique registered cases in one queue.
Full weapon art, precise individual fingers and the whole character experience
remain open. See ReviewedPistolValidation.md for retained failed iterations.

Fifth checkpoint: progressive speedboat thrust, local speed/throttle/recovery
HUD, reviewed CC0 spatial motor loop and bounded wake/spray. EditorBuild08 and
GameBuild03 pass. MarineThrottleLoop05 passes the continuous scripted driving,
swimming and reboarding route; its actual wake and HUD were inspected. Twelve
focused cases pass, and the separate MarineFeedbackCrossSession01 A/B processes
restore matching persistent boat state. MarineFeedbackFullRegression01 passes
all 117 unique registered cases in one queue. Repeated executions are reported
separately in MarineExperienceValidation.md. Audible quality, complete marine
art/lifestyle and whole-game long-session acceptance remain open. Aircraft power
source work started during that queue is separate and was not in its binaries.

Sixth checkpoint: progressive helicopter power, bounded deceleration, shared
aircraft telemetry and local pilot instruments. EditorBuild03 and GameBuild01
pass. PowerLoop01 and InstrumentsMissile01 pass with inspected actual control,
lock and ammunition feedback; 25 unique focused cases and separate Aircraft A/B
processes pass. AircraftPowerFullRegression01 passes all 118 unique registered
cases in one queue. HUD contrast was corrected after the first visual review.
HelicopterExperienceValidation retains input-route fixtures and remaining normal
night-light/audio/whole-game acceptance. Lighting authoring started during the
queue is separate and was not included in those binaries. Its concurrent short
Blender renders mean regression timing is diagnostic, not a clean benchmark.

Seventh checkpoint: a licensed CC0 searchlight optical head is adapted into a
socket-mounted aircraft fitting with an actual emitter and bounded night beam.
EditorBuild10/GameBuild02 pass. AircraftSearchlight08, nine environment views,
the marine input route and a separate no-readback lighting metrics run pass;
actual world/HUD images are 1600 x 900 and were inspected. The full
AircraftLightingFullRegression01 queue passes all 118 unique registered cases.
Earlier invalid-size, capture and exposure iterations remain documented in
AircraftLightingValidation.md. Navigation source started during this queue was
not included in those binaries. The wider aircraft art/audio and whole program
remain incomplete.

Eighth checkpoint: local vector minimap and Marcus pickup/road/delivery guidance,
safe failure recovery and open-phone input arbitration. EditorBuild06/GameBuild01
pass; 11 focused cases pass. MarcusNavigation01 completes the ordinary physical
route without staged transforms and its four actual UI views were inspected.
MarcusNavigationCrossSession01 restores identical delivered-car/cash/contract
state in independent processes. NavigationFullRegression01 passes all 122 unique
registered cases. NavigationExperience retains initial compile/paint/input errors,
state-injection distinctions and short no-readback route timings. Opportunity
choice source begun during the queue is excluded from this checkpoint's binaries.

Ninth checkpoint: generic opportunity providers, explicit Accept/Decline/Later,
message history and keyboard scrolling, preserving the original Marcus route and
unique reward. EditorBuild04/GameBuild01, actual choice/scroll/drive views and
independent ignored/declined A/B saves pass. OpportunityFullRegression01 passes
all 123 unique registered cases with zero errors; one Engine rendering-variable
warning is retained. OpportunityExperience records the initial recreated-contract
failure and corrected receipt-before-registration order. Banking source started
during the queue is excluded. Wider activities and whole-program acceptance remain
open.

Tenth checkpoint: atomic cash/bank transfers, reviewed bounded loans, repayments
and persistent transaction history. EditorBuild02/GameBuild01 pass; 16 focused
cases, six inspected actual phone views and independent A/B restart checks pass.
BankingFullRegression01 passes all 126 unique registered cases with zero errors.
Two successful cases retain Engine motion-vector/VSM warnings. BankingExperience
records the corrected test fixture, exact persistence evidence and unfinished
financing purchases/business activities. Fishing source begun during this queue
is excluded from these binaries. The whole vertical slice remains incomplete.


## Tidewater direction amendment — 2026-09-26

The exact request is retained in Requirements/Phase20_5c_TidewaterUpdate.txt.
Preserve real HEAD and in-progress work; no reset, old checkout or duplicate
systems. A direction-adjustment snapshot preserves the current fishing loop and
licensed resource evaluations after Editor/Game and actual-route/restart checks.
Full 129-case FishingFullRegression01 is still running at that snapshot.

Priority now rises for Tidewater source/asset-license audit and pinned reference,
ocean/coast/island layout and rendering adaptation, boat first/third-person V
camera, drifting deck walk/fishing, richer charged-cast/bite/tension/fish-log
logic, marine fuel/refueling and the continuous mainland-island-return route.
Only the requested offshore vertical-slice island exception is allowed; no other
Phase 21 regions. Existing accepted work and remaining original workstreams stay
in scope. Current shore, boat-camera and deck-fishing gates remain NOT COMPLETE.


## Full fishing regression closeout — 2026-09-26

FishingFullRegression01: 129/129 unique registered tests succeeded, zero errors,
missing, unexpected or duplicate tests. Seven warnings: one Engine motion-vector,
one VSM queue overflow, three fishing spline attachment and two spline-material
usage/recompile warnings. The spline warnings are actionable and are being fixed;
this result is not a zero-warning or visual parity acceptance.

The highest-priority request is retained in Requirements/Phase20_5c_TidewaterParityUpdate.txt.
TIDEWATER PARITY TARGET covers the entire reasonable marine/island feature set.
See TidewaterParityChecklist.md and ThirdParty/TidewaterIntegration.md. Overall
Phase20.5c remains IN PROGRESS; no Phase21 work is authorized.

The subsequent `Requirements/Phase20_5c_DirectPortNightCorrection.txt` is the
highest-priority amendment. Current bare hills and unreadable island night are
explicit failures. Direct canonical vegetation and ocean/interaction ports,
central automatic lighting, and continuous normal-game before/after acceptance
take priority; all remaining original workstreams are retained. No completion
claim or Phase21 transition is permitted while any listed parity gate fails.

## Canonical boat, village, forest and lighting checkpoint — 2026-09-26

Direct source conversion now includes the working boat and retained hydrodynamics,
real helm/chase/deck loop and separate-process persistence; the source village,
walkable pier/boardwalk and fifty lantern/window light positions; all ten plant
families and 65,383 source instances; and a shared bounded lighting scheduler.
The user's hillside colour correction is inspected in three ordinary game views.
The first actual island night view failed, while the later bounded-exposure pier
comparison is readable. These fixed comparisons are not continuous-route passes.

TidewaterForestLightingRegression01 completes all 139 registered tests, with three
rendering warnings. Two pre-queue colour-field initialization errors were found
separately, fixed and guarded in the runners. Editor/Game rebuild and four focused
forest/lighting checks pass in TidewaterForestDefaultsBuild01 / Checks01 with no
remaining reflected-class errors. Detailed evidence and limitations are in the
MarineReferenceBoat/Vegetation/VillageValidation and WorldLightingValidation docs.
Far canopy impostors, grass, ocean, the continuous routes and remaining original
workstreams remain in progress. This checkpoint does not close Phase20.5c.


## Far canopy checkpoint — 2026-09-26

Five canonical far variants / 180 source views now complement the same 18,343
near trees and shrubs, without double-counting plants. Source shadow policy,
conservative wind bounds, hill green grading and an 80 m normal-movement LOD
walk are verified. CanopyBuild05 Editor/Game pass; CanopyRegression02 passes
140/140 registered tests, zero errors. Engine motion-vector and CoastalLoop VSM
warnings remain; see MarineReferenceVegetationValidation.md for actual metrics.
The user now requires denser mountain jungle and website character models. These
are pending visual gates. Grass source conversion begun while tests ran is not
part of this checkpoint or those binaries. Phase20.5c remains IN PROGRESS.


## Source-character integration checkpoint — 2026-09-26

The actual reviewed Tidewater male/female people now replace player and pedestrian
mannequin appearances. Editable source skins are bound to the existing animation
rig; raw characters/clips and template assets remain intact. Normal-game movement,
jump, combat/recovery and transport occupant checks pass with inspected final
frames after shoulder/neck corrections. CharacterBuild02 Editor/Game both pass.
TidewaterCharacterRegression01 now passes all 141 registered tests, zero errors,
with the two previously recorded engine rendering warnings. See
ReferenceCharacterValidation.md for evidence.
Grass, mountain-jungle density and the remaining direct-port workstreams remain
IN PROGRESS; this checkpoint does not close Phase20.5c.

## Dense mountain forest and physical-contact checkpoint — 2026-09-26

The original 65,383 source plants remain, with a separate authored layer adding
20,510 trees, 6,254 shrubs, 1,852 ferns and 419 monsteras. Actual saved-map normal
movement passes the 133 m mountain-trail leg and original 80 m forest leg with
physical wood enabled. Nearby bark collision uses the original tree geometry;
soft understory bends and recovers through bounded springs. Actual Chaos contact
feeds existing vehicle damage. Source-character full regression is also closed
at 141/141. Current vegetation Editor/Game builds and focused contact checks pass.

The expanded PhysicalForestRegression01 finishes 144/145 Success. Its only
failure is the fixed-time aircraft overflight position check; the unchanged test
passes on independent rerun. Arrival diagnostics and a bounded physical-arrival
window now preserve the same distance/altitude assertions. This is not recorded
as a complete green full run; the next native integration requires one.
FoliageBuild05 Editor/Game compilation and FoliageCloseout01's three focused
aircraft/wood/contact tests pass after the test timing correction.
MountainJungleValidation.md and FoliageInteractionValidation.md retain measured
budgets, native captures, source/package receipts and remaining limitations.
Dense near views still show cache pressure, a VSM warning, foreground dithering
and excessive sun highlights; these are not visually accepted. Grass, ocean,
island integration and the other requested direct-port workstreams remain open.
Phase20.5c is IN PROGRESS; no Phase21 work or overall completion claim.

## Ground flora, palm contact and forest rendering checkpoint — 2026-09-26

The original 2,724 grass cells and three source patch levels now use native
Landscape triangle grounding. Curved mature palms gain bounded source-shaped
solid contact, while tree/shrub contact remains independent of visual density.
A confirmed signed-dither-mask composition error was repaired; bark is opaque
again. Epic shadow/cache budgets reduce measured forest GPU median from 37.676
to 19.557 ms on the same route/configuration, with unchanged source wind.
This is a single-run comparison of combined changes, not stable 60 FPS proof.

PalmContactBuild02 compiles Development Editor/Game with zero compile/link errors.
TidewaterGrassFoliageRegression01 passes **147/147** in 1731.05 s, including the
previously unstable aircraft crossing and all physical, combat, wanted and save
cases. Two existing engine warnings remain (motion-vector access, VSM marking).
The normal forest/grass walks and fixed night road/pier comparisons also pass.

Grass still appears too pale; dense sunlight/shadow balance, continuous island
and night routes, audio and the remaining direct-port workstreams remain open.
ForestRenderingValidation.md, ReferenceGrassValidation.md and
FoliageInteractionValidation.md record the evidence and limits. Phase20.5c
remains IN PROGRESS, and no Phase21 work has started.

## Physical forest feedback checkpoint — 2026-09-26

Real nearby wood/curved-palm collision and soft-plant springs now include bounded,
event-driven brush audio from an individually reviewed CC0 recording. Source
wind/insect/bird beds and grounded footsteps use the locked source audio library.
Separate native mixer recordings verify actual non-silent output; stationary
contact does not repeatedly rustle. The source foliage attribute adapter is now
compatible with Nanite's four UV channels, verified on 34,152 native mesh corners.

FoliageFeedbackFinalEditor/Game compile successfully. Full
TidewaterFoliageFeedbackRegression01 passes **151/151** in 1755.99 s, zero errors,
one existing motion-vector warning. The earlier 150-case reverse failure remains
documented; its rerun passes without changing behavior/assertions, but the prior
cause remains unconfirmed. ContactForestMetrics02 measures wall/GPU medians
19.001/17.987 ms and zero local collision-capacity misses on the 133 m trail;
this is not stable 60 FPS proof or whole-world performance certification.

Dense daylight balance and all outstanding direct-port/continuous-route/human
acceptance work remain open. Trees do not yet fracture/fall/uproot. See
FoliageContactAudioValidation.md and ForestAudioValidation.md. Phase20.5c remains
IN PROGRESS; there is no Phase21 work or overall completion claim.


## Ocean computation checkpoint (2026-09-27)

The locked Tidewater four-cascade FFT now executes as native UE global shaders
in a thin early-loading module. Real original WGSL and UE GPU fields independently
pass 243 NumPy comparisons, and all 243 fields pass source/native comparison.
A directional-energy type conversion bug was caught and corrected before world
integration. Editor/Game builds and seven relevant integration tests pass.
See OceanFFTValidation.md for retained failures, error budgets and runtime
ownership. Bounded asynchronous queries, water/coast appearance and actual
shared boat/swimmer coupling remain open; the ocean workstream is not complete.
The prior physical forest checkpoint retains its own complete 151-test evidence.


## Bounded water-query checkpoint (2026-09-27)

Native shared FFT queries preserve original displaced-height/normal solving with
a bounded three-buffer asynchronous CPU bridge. Full TidewaterOceanQueryFullRegression01
passes 152/152 in 1752.84 s, zero errors and one existing motion-vector warning.
The runner also passes 243 independent FFT checks and all 704 water-query points.
Both Development Editor and Game compile/link successfully; frozen implementation
and binary hashes match the archived report. WaterQueryValidation.md records the
separate original/native comparison, lifecycle checks and failure handling.

Source mesh/foam preparation remains outside this tested checkpoint. Actual ocean
rendering/coast, shared boat/swimmer coupling and the remaining visual/gameplay
workstreams are open. Phase20.5c is IN PROGRESS; no Phase21 work.
